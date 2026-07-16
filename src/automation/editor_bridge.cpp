#include "engine/automation/editor_bridge.h"

#include "engine/automation/automation_trace.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace engine {
namespace {

std::string quote_json(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += static_cast<unsigned char>(c) < 0x20 ? '?' : c; break;
        }
    }
    return out + '"';
}

std::filesystem::path normalize_project_root(const std::filesystem::path& project_root) {
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(project_root, ec);
    if (ec) return project_root.lexically_normal();
    const auto canonical = std::filesystem::weakly_canonical(absolute, ec);
    return ec ? absolute.lexically_normal() : canonical.lexically_normal();
}

std::uint64_t hash_project_path(const std::filesystem::path& project_root) {
    const auto normalized = normalize_project_root(project_root).generic_string();
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : normalized) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

#ifdef _WIN32
std::wstring to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

bool write_framed_message(HANDLE handle, const std::string& payload, std::uint32_t timeout_ms = 1000) {
    std::ostringstream header;
    header << "Content-Length: " << payload.size() << "\r\n\r\n";
    const auto header_text = header.str();

    auto write_with_timeout = [&](const void* data, DWORD size) -> bool {
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) return false;
        DWORD written = 0;
        const BOOL started =
            WriteFile(handle, data, size, &written, &overlapped) || GetLastError() == ERROR_IO_PENDING;
        if (!started) {
            CloseHandle(overlapped.hEvent);
            return false;
        }
        const DWORD wait = WaitForSingleObject(overlapped.hEvent, timeout_ms);
        if (wait != WAIT_OBJECT_0) {
            (void)CancelIoEx(handle, &overlapped);
            WaitForSingleObject(overlapped.hEvent, 50);
            CloseHandle(overlapped.hEvent);
            return false;
        }
        if (!GetOverlappedResult(handle, &overlapped, &written, FALSE)) {
            CloseHandle(overlapped.hEvent);
            return false;
        }
        CloseHandle(overlapped.hEvent);
        return written == size;
    };

    return write_with_timeout(header_text.data(), static_cast<DWORD>(header_text.size())) &&
        (payload.empty() || write_with_timeout(payload.data(), static_cast<DWORD>(payload.size())));
}

bool read_exact(HANDLE handle, void* buffer, DWORD size, std::uint32_t timeout_ms) {
    auto* out = static_cast<std::uint8_t*>(buffer);
    DWORD remaining = size;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (remaining > 0) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        DWORD available = 0;
        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) return false;
        if (available == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        DWORD read = 0;
        const DWORD chunk = std::min(remaining, available);
        if (!ReadFile(handle, out, chunk, &read, nullptr) || read == 0) return false;
        out += read;
        remaining -= read;
    }
    return true;
}

bool read_framed_message(HANDLE handle, std::string& payload, std::uint32_t timeout_ms) {
    payload.clear();
    std::string header;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (header.find("\r\n\r\n") == std::string::npos) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        DWORD available = 0;
        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) return false;
        if (available == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        char byte = 0;
        DWORD read = 0;
        if (!ReadFile(handle, &byte, 1, &read, nullptr) || read == 0) return false;
        header.push_back(byte);
        if (header.size() > 4096) return false;
    }
    const auto split = header.find("\r\n\r\n");
    const auto header_line = header.substr(0, split);
    const std::string prefix = "Content-Length: ";
    const auto pos = header_line.find(prefix);
    if (pos == std::string::npos) return false;
    const auto length = static_cast<std::size_t>(std::stoul(header_line.substr(pos + prefix.size())));
    payload.assign(length, '\0');
    if (length > 0 && !read_exact(handle, payload.data(), static_cast<DWORD>(length), timeout_ms)) return false;
    return true;
}

HANDLE create_listening_pipe(const std::wstring& pipe_name) {
    return CreateNamedPipeW(pipe_name.c_str(), PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, nullptr);
}

void disconnect_client_pipe(HANDLE pipe) {
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

void finish_client_response(HANDLE pipe, bool wrote_response) {
    if (wrote_response) {
        (void)FlushFileBuffers(pipe);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    disconnect_client_pipe(pipe);
}

bool wait_for_client(HANDLE pipe, std::atomic<bool>& running) {
    while (running.load(std::memory_order_acquire)) {
        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) return true;
        if (GetLastError() != ERROR_PIPE_LISTENING) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
#endif

} // namespace

std::string editor_bridge_pipe_name(const std::filesystem::path& project_root) {
    std::ostringstream out;
    out << "\\\\.\\pipe\\airengine-" << std::hex << hash_project_path(project_root);
    return out.str();
}

std::string EditorBridgeResponse::to_json() const {
    std::ostringstream out;
    out << "{\"schemaVersion\":" << schema_version << ",\"requestId\":" << quote_json(request_id)
        << ",\"exitCode\":" << static_cast<int>(exit_code) << ",\"summary\":" << quote_json(summary)
        << ",\"changedObjectIds\":[";
    for (std::size_t i = 0; i < changed_object_ids.size(); ++i) {
        if (i) out << ',';
        out << quote_json(changed_object_ids[i]);
    }
    out << "],\"diagnostics\":[";
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i) out << ',';
        out << diagnostics[i].to_json();
    }
    out << "],\"metadata\":{";
    std::size_t index = 0;
    for (const auto& entry : metadata) {
        if (index++) out << ',';
        out << quote_json(entry.first) << ':' << quote_json(entry.second);
    }
    return out.str() + "}}";
}

std::optional<EditorBridgeRequest> EditorBridgeResponse::parse_request(const std::string& json) {
    try {
        const auto parsed = nlohmann::json::parse(json);
        if (!parsed.is_object()) return std::nullopt;
        EditorBridgeRequest request;
        request.schema_version = parsed.value("schemaVersion", 1);
        request.request_id = parsed.value("requestId", std::string{});
        request.operation = parsed.value("operation", std::string{});
        if (parsed.contains("params")) {
            if (parsed["params"].is_string()) request.params_json = parsed["params"].get<std::string>();
            else request.params_json = parsed["params"].dump();
        }
        if (request.operation.empty()) return std::nullopt;
        return request;
    } catch (...) {
        return std::nullopt;
    }
}

EditorBridgeServer::EditorBridgeServer(std::filesystem::path project_root) : project_root_(std::move(project_root)) {}

EditorBridgeServer::~EditorBridgeServer() { stop(); }

void EditorBridgeServer::close_listening_pipe() {
#ifdef _WIN32
    if (pipe_handle_ != nullptr) {
        auto* pipe = static_cast<HANDLE>(pipe_handle_);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        pipe_handle_ = nullptr;
    }
#endif
}

bool EditorBridgeServer::start() {
#ifdef _WIN32
    if (running_.load(std::memory_order_acquire)) return true;
    const auto pipe_name = editor_bridge_pipe_name(project_root_);
    const auto pipe = create_listening_pipe(to_wide(pipe_name));
    if (pipe == INVALID_HANDLE_VALUE) {
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "server_start_failed",
            {{"pipe", pipe_name}, {"detail", "CreateNamedPipe failed"}});
        return false;
    }
    pipe_handle_ = pipe;
    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&EditorBridgeServer::worker_main, this);
    AutomationTrace::log(AutomationTraceChannel::EditorBridge, "server_start",
        {{"pipe", pipe_name},
            {"project", project_root_.lexically_normal().generic_string()},
            {"logPath", AutomationTrace::log_path(AutomationTraceChannel::EditorBridge).generic_string()}});
    return true;
#else
    (void)project_root_;
    return false;
#endif
}

void EditorBridgeServer::stop() {
#ifdef _WIN32
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    AutomationTrace::log(AutomationTraceChannel::EditorBridge, "server_stop", "shutdown requested");
    close_listening_pipe();
    {
        std::lock_guard lock(mutex_);
        if (request_pending_ && !response_pending_) {
            active_response_.schema_version = 1;
            active_response_.request_id = active_request_.request_id;
            active_response_.exit_code = ExitCode::Unavailable;
            active_response_.summary = "Editor live automation is shutting down";
            response_pending_ = true;
            response_ready_.notify_one();
        }
    }
    request_ready_.notify_one();
    if (worker_.joinable()) worker_.join();
    request_pending_ = false;
    response_pending_ = false;
#endif
}

void EditorBridgeServer::worker_main() {
#ifdef _WIN32
    const auto pipe_name = to_wide(editor_bridge_pipe_name(project_root_));
    while (running_.load(std::memory_order_acquire)) {
        if (pipe_handle_ == nullptr) {
            const auto pipe = create_listening_pipe(pipe_name);
            if (pipe == INVALID_HANDLE_VALUE) break;
            pipe_handle_ = pipe;
        }

        auto* listening = static_cast<HANDLE>(pipe_handle_);
        if (!wait_for_client(listening, running_)) break;
        pipe_handle_ = nullptr;
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_connected");

        std::string request_text;
        EditorBridgeRequest request;
        EditorBridgeResponse response;
        response.schema_version = 1;
        response.exit_code = ExitCode::InvalidArguments;
        response.summary = "Invalid bridge request";

        if (read_framed_message(listening, request_text, 2000)) {
            const auto parsed = EditorBridgeResponse::parse_request(request_text);
            if (parsed) {
                request = *parsed;
                response.request_id = request.request_id;
                AutomationTrace::log(AutomationTraceChannel::EditorBridge, "request_received",
                    {{"operation", request.operation}, {"requestId", request.request_id}});
                {
                    std::unique_lock lock(mutex_);
                    active_request_ = request;
                    request_pending_ = true;
                    response_pending_ = false;
                    request_ready_.notify_one();
                    response_ready_.wait(lock, [&] {
                        return response_pending_ || !running_.load(std::memory_order_acquire);
                    });
                    if (response_pending_) {
                        response = active_response_;
                        response.request_id = request.request_id;
                    } else {
                        response.exit_code = ExitCode::Unavailable;
                        response.summary = "Editor live automation is shutting down";
                    }
                    request_pending_ = false;
                    response_pending_ = false;
                }
                AutomationTrace::log(AutomationTraceChannel::EditorBridge, "response_ready",
                    {{"operation", request.operation},
                        {"requestId", response.request_id},
                        {"exitCode", std::to_string(static_cast<int>(response.exit_code))},
                        {"summary", response.summary}});
            } else {
                AutomationTrace::log(AutomationTraceChannel::EditorBridge, "request_invalid", "failed to parse JSON");
            }
            const bool wrote = write_framed_message(listening, response.to_json());
            AutomationTrace::log(AutomationTraceChannel::EditorBridge, "response_sent",
                {{"wrote", wrote ? "true" : "false"}});
            finish_client_response(listening, wrote);
        } else {
            AutomationTrace::log(AutomationTraceChannel::EditorBridge, "read_timeout", "no framed request within 2s");
            disconnect_client_pipe(listening);
        }
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_disconnected");
    }
    close_listening_pipe();
#endif
}

void EditorBridgeServer::poll_pending(std::function<EditorBridgeResponse(const EditorBridgeRequest&)> handler) {
#ifdef _WIN32
    if (!running_.load(std::memory_order_acquire)) return;
    std::unique_lock lock(mutex_);
    if (!request_pending_) return;
    const auto request = active_request_;
    lock.unlock();
    AutomationTrace::log(AutomationTraceChannel::EditorBridge, "poll_dispatch",
        {{"operation", request.operation}, {"requestId", request.request_id}});
    auto response = handler(request);
    response.request_id = request.request_id;
    lock.lock();
    if (request_pending_ && active_request_.request_id == request.request_id) {
        active_response_ = std::move(response);
        response_pending_ = true;
        response_ready_.notify_one();
    }
#endif
}

EditorBridgeClient::EditorBridgeClient(std::filesystem::path project_root) : project_root_(std::move(project_root)) {}

bool EditorBridgeClient::is_editor_running() const {
#ifdef _WIN32
    return WaitNamedPipeW(to_wide(editor_bridge_pipe_name(project_root_)).c_str(), 50) != FALSE;
#else
    return false;
#endif
}

EditorBridgeResponse EditorBridgeClient::send(const EditorBridgeRequest& request, std::uint32_t timeout_ms) const {
    EditorBridgeResponse response;
    response.schema_version = 1;
    response.request_id = request.request_id;
#ifdef _WIN32
    const auto pipe_name = to_wide(editor_bridge_pipe_name(project_root_));
    constexpr std::uint32_t k_probe_timeout_ms = 50;
    if (!WaitNamedPipeW(pipe_name.c_str(), std::min(timeout_ms, k_probe_timeout_ms))) {
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_send_failed",
            {{"operation", request.operation}, {"requestId", request.request_id}, {"reason", "pipe unavailable"}});
        response.exit_code = ExitCode::Unavailable;
        response.summary = "Editor live automation is not connected";
        return response;
    }
    HANDLE handle = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_send_failed",
            {{"operation", request.operation}, {"requestId", request.request_id}, {"reason", "CreateFile failed"}});
        response.exit_code = ExitCode::Unavailable;
        response.summary = "Editor live automation is not connected";
        return response;
    }
    DWORD mode = PIPE_READMODE_BYTE;
    (void)SetNamedPipeHandleState(handle, &mode, nullptr, nullptr);

    nlohmann::json payload;
    payload["schemaVersion"] = request.schema_version;
    payload["requestId"] = request.request_id;
    payload["operation"] = request.operation;
    if (!request.params_json.empty()) {
        try {
            payload["params"] = nlohmann::json::parse(request.params_json);
        } catch (...) {
            payload["params"] = request.params_json;
        }
    }
    if (!write_framed_message(handle, payload.dump())) {
        CloseHandle(handle);
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_send_failed",
            {{"operation", request.operation}, {"requestId", request.request_id}, {"reason", "write timeout"}});
        response.exit_code = ExitCode::InternalError;
        response.summary = "Failed to write bridge request";
        return response;
    }
    std::string response_text;
    if (!read_framed_message(handle, response_text, timeout_ms)) {
        CloseHandle(handle);
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_send_failed",
            {{"operation", request.operation}, {"requestId", request.request_id}, {"reason", "read timeout"}});
        response.exit_code = ExitCode::InternalError;
        response.summary = "Timed out waiting for editor response";
        return response;
    }
    CloseHandle(handle);
    try {
        const auto parsed = nlohmann::json::parse(response_text);
        response.exit_code = static_cast<ExitCode>(parsed.value("exitCode", 0));
        response.summary = parsed.value("summary", std::string{});
        if (parsed.contains("changedObjectIds") && parsed["changedObjectIds"].is_array())
            for (const auto& id : parsed["changedObjectIds"]) response.changed_object_ids.push_back(id.get<std::string>());
        if (parsed.contains("metadata") && parsed["metadata"].is_object())
            for (auto it = parsed["metadata"].begin(); it != parsed["metadata"].end(); ++it)
                response.metadata[it.key()] = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
    } catch (...) {
        response.exit_code = ExitCode::InternalError;
        response.summary = "Invalid bridge response";
    }
    AutomationTrace::log(AutomationTraceChannel::EditorBridge, "client_response",
        {{"operation", request.operation},
            {"requestId", response.request_id},
            {"exitCode", std::to_string(static_cast<int>(response.exit_code))},
            {"summary", response.summary}});
    return response;
#else
    (void)timeout_ms;
    response.exit_code = ExitCode::Unavailable;
    response.summary = "Editor bridge is only supported on Windows";
    return response;
#endif
}

} // namespace engine
