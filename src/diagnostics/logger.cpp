#include "engine/diagnostics/logger.h"
#include "engine/diagnostics/gpu_diagnostics.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <windows.h>

namespace engine {

Logger& Logger::instance() { static Logger value; return value; }

namespace {
std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << (static_cast<unsigned char>(c) < 0x20 ? '?' : c); break;
        }
    }
    return out.str();
}

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &seconds);
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string process_context_json(const ProcessLogContext& context) {
    std::ostringstream out;
    out << "{\"sessionId\":\"" << json_escape(context.session_id)
        << "\",\"pid\":" << context.pid
        << ",\"startedAtUtc\":\"" << json_escape(context.started_at_utc)
        << "\",\"mode\":\"" << json_escape(context.mode)
        << "\",\"project\":\"" << json_escape(context.project)
        << "\",\"world\":\"" << json_escape(context.world)
        << "\",\"buildConfiguration\":\""
        << json_escape(context.build_configuration) << "\"}";
    return out.str();
}
} // namespace

void Logger::initialize(const std::filesystem::path& jsonl_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = jsonl_path;
    process_context_ = {};
    process_context_.pid = GetCurrentProcessId();
    process_context_.session_id = make_correlation_id();
    process_context_.started_at_utc = utc_timestamp();
    if (path_.has_parent_path()) std::filesystem::create_directories(path_.parent_path());
    stream_.open(path_, std::ios::app);
    set_process_gpu_diagnostics(GpuDiagnostics::capture());
}

void Logger::set_process_context(ProcessLogContext context) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (context.pid == 0) context.pid = GetCurrentProcessId();
    if (context.session_id.empty()) context.session_id = process_context_.session_id;
    if (context.started_at_utc.empty()) context.started_at_utc = process_context_.started_at_utc;
    process_context_ = std::move(context);
}

void Logger::write(Severity severity, std::string subsystem, std::string message, std::string correlation_id) {
    if (correlation_id.empty()) correlation_id = make_correlation_id();
    EngineError event{"LOG", severity, ErrorCategory::InternalInvariant, std::move(subsystem),
                      std::move(message), std::nullopt, {}, {}, std::move(correlation_id)};
    write(event);
}

void Logger::write(const EngineError& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    HANDLE console=GetStdHandle(STD_ERROR_HANDLE); CONSOLE_SCREEN_BUFFER_INFO original{}; const bool colored=console!=INVALID_HANDLE_VALUE&&GetConsoleScreenBufferInfo(console,&original);
    if(colored){WORD color=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE;if(error.severity==Severity::Warning)color=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY;else if(error.severity==Severity::Error||error.severity==Severity::Fatal)color=FOREGROUND_RED|FOREGROUND_INTENSITY;SetConsoleTextAttribute(console,color);}
    std::cerr << error.to_text() << '\n';
    if(colored)SetConsoleTextAttribute(console,original.wAttributes);
    if (stream_) {
        std::string event = error.to_json();
        event.pop_back();
        stream_ << event << ",\"gpuDiagnostics\":" << process_gpu_diagnostics().to_json()
                << ",\"process\":" << process_context_json(process_context_) << "}\n";
        stream_.flush();
    }
    if(error.severity==Severity::Error||error.severity==Severity::Fatal){++error_count_;recent_errors_.push_back(error);while(recent_errors_.size()>128)recent_errors_.pop_front();}
}

void Logger::write_event(std::string event_name, std::string json_object_payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_) return;
    if (json_object_payload.empty() || json_object_payload.front() != '{' ||
        json_object_payload.back() != '}') {
        json_object_payload = "{\"unstructuredPayload\":\"" +
                              json_escape(json_object_payload) + "\"}";
    }
    stream_ << "{\"schemaVersion\":1,\"event\":\"" << json_escape(event_name)
            << "\",\"data\":" << json_object_payload
            << ",\"gpuDiagnostics\":" << process_gpu_diagnostics().to_json()
            << ",\"process\":" << process_context_json(process_context_) << "}\n";
    stream_.flush();
}
std::uint64_t Logger::error_count() const { std::lock_guard<std::mutex> lock(mutex_); return error_count_; }
std::vector<EngineError> Logger::recent_errors() const { std::lock_guard<std::mutex> lock(mutex_); return {recent_errors_.begin(),recent_errors_.end()}; }

std::filesystem::path Logger::log_path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

} // namespace engine
