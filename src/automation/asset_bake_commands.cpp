#include "engine/automation/asset_bake_commands.h"

#include "engine/core/error.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace engine {
namespace {

#if defined(_WIN32)
std::wstring quote_windows_arg(const std::string& arg) {
    std::wstring wide(arg.begin(), arg.end());
    if (wide.find_first_of(L" \t\"") == std::wstring::npos) return wide;
    std::wstring out = L"\"";
    for (wchar_t ch : wide) {
        if (ch == L'"') out += L"\\\"";
        else out.push_back(ch);
    }
    out += L'"';
    return out;
}

struct ProcessResult {
    int exit_code = 1;
    std::string stdout_text;
    std::string stderr_text;
};

ProcessResult run_process(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
    ProcessResult result;
    if (args.empty()) {
        result.stderr_text = "empty command";
        return result;
    }
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;
    if (!CreatePipe(&stdout_read, &stdout_write, &security, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &security, 0)) {
        result.stderr_text = "failed to create pipes";
        return result;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    std::wstring command_line;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i) command_line.push_back(L' ');
        command_line += quote_windows_arg(args[i]);
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_write;
    startup.hStdError = stderr_write;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring cwd_wide = cwd.empty() ? std::wstring{} : cwd.wstring();
    std::vector<wchar_t> mutable_cmd(command_line.begin(), command_line.end());
    mutable_cmd.push_back(L'\0');
    const BOOL created = CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, cwd_wide.empty() ? nullptr : cwd_wide.c_str(), &startup, &process);
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);
    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        result.stderr_text = "CreateProcess failed (" + std::to_string(GetLastError()) + ")";
        return result;
    }
    auto read_handle = [](HANDLE handle) {
        std::string text;
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(handle, buffer, sizeof(buffer), &read, nullptr) && read > 0)
            text.append(buffer, buffer + read);
        return text;
    };
    result.stdout_text = read_handle(stdout_read);
    result.stderr_text = read_handle(stderr_read);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    result.exit_code = static_cast<int>(code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    return result;
}
#else
struct ProcessResult {
    int exit_code = 1;
    std::string stdout_text;
    std::string stderr_text;
};

ProcessResult run_process(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
    ProcessResult result;
    if (args.empty()) {
        result.stderr_text = "empty command";
        return result;
    }
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.stderr_text = "pipe failed";
        return result;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        result.stderr_text = "fork failed";
        return result;
    }
    if (pid == 0) {
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    auto read_fd = [](int fd) {
        std::string text;
        char buffer[4096];
        ssize_t n = 0;
        while ((n = read(fd, buffer, sizeof(buffer))) > 0) text.append(buffer, buffer + n);
        return text;
    };
    result.stdout_text = read_fd(stdout_pipe[0]);
    result.stderr_text = read_fd(stderr_pipe[0]);
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return result;
}
#endif

std::filesystem::path repo_root() {
    return std::filesystem::path(ENGINE_REPOSITORY_ROOT);
}

std::string find_python() {
#if defined(_WIN32)
    const auto py = run_process({}, {"py", "-3", "--version"});
    if (py.exit_code == 0) return "py";
#endif
    const auto python = run_process({}, {"python", "--version"});
    if (python.exit_code == 0) return "python";
    const auto python3 = run_process({}, {"python3", "--version"});
    if (python3.exit_code == 0) return "python3";
    return {};
}

EngineError bake_error(const std::string& code, const std::string& message, const std::string& remediation = {}) {
    EngineError error;
    error.code = code;
    error.message = message;
    error.remediation = remediation;
    error.category = ErrorCategory::AssetImport;
    error.severity = Severity::Error;
    return error;
}

nlohmann::json extract_json_object(const std::string& text) {
    const auto start = text.find('{');
    const auto end = text.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end <= start) return nlohmann::json{};
    try {
        return nlohmann::json::parse(text.substr(start, end - start + 1));
    } catch (...) {
        return nlohmann::json{};
    }
}

AssetBakeResult parse_bake_stdout(const ProcessResult& proc) {
    AssetBakeResult result;
    result.raw_json = proc.stdout_text;
    auto payload = extract_json_object(proc.stdout_text);
    if (payload.is_null() || payload.empty()) {
        result.ok = false;
        result.summary = "asset bake produced no JSON";
        result.diagnostics.push_back(bake_error("ASSET-BAKE-TOOLING",
            proc.stderr_text.empty() ? proc.stdout_text : proc.stderr_text,
            "Ensure Python + Pillow are on PATH and tools/asset_bake.py runs."));
        return result;
    }
    result.ok = payload.value("ok", false) && proc.exit_code == 0;
    result.summary = payload.value("summary", result.ok ? "bake ok" : "bake failed");
    if (payload.contains("meshReloads") && payload["meshReloads"].is_array()) {
        for (const auto& item : payload["meshReloads"]) {
            if (item.is_string()) result.mesh_reloads.push_back(item.get<std::string>());
        }
    }
    if (payload.contains("verify") && payload["verify"].is_array()) {
        for (const auto& gate : payload["verify"]) {
            if (!gate.is_object()) continue;
            if (gate.value("ok", true)) continue;
            result.diagnostics.push_back(bake_error(gate.value("code", "ASSET-BAKE-TOOLING"),
                gate.value("detail", "verify failed"), gate.value("remediation", "")));
        }
    }
    if (!result.ok && result.diagnostics.empty()) {
        result.diagnostics.push_back(bake_error("ASSET-BAKE-TOOLING",
            proc.stderr_text.empty() ? result.summary : proc.stderr_text));
    }
    return result;
}

ProcessResult invoke_asset_bake(const std::filesystem::path& project_root, const std::vector<std::string>& extra) {
    const auto python = find_python();
    ProcessResult failed;
    if (python.empty()) {
        failed.stderr_text = "python not found on PATH";
        failed.exit_code = 127;
        return failed;
    }
    const auto script = repo_root() / "tools" / "asset_bake.py";
    std::vector<std::string> args;
    args.push_back(python);
#if defined(_WIN32)
    if (python == "py") args.push_back("-3");
#endif
    args.push_back(script.string());
    args.push_back("--project");
    args.push_back(project_root.string());
    args.push_back("--json");
    args.insert(args.end(), extra.begin(), extra.end());
    return run_process(repo_root(), args);
}

} // namespace

std::vector<AssetBakeTargetInfo> list_asset_bake_targets() {
    std::vector<AssetBakeTargetInfo> out;
    const auto catalog_path = repo_root() / "tools" / "asset_bake_catalog.json";
    std::ifstream in(catalog_path);
    if (!in) return out;
    nlohmann::json catalog;
    try {
        in >> catalog;
    } catch (...) {
        return out;
    }
    for (const auto& t : catalog.value("targets", nlohmann::json::array())) {
        AssetBakeTargetInfo info;
        info.id = t.value("id", "");
        info.kind = t.value("kind", "");
        info.default_source = t.value("defaultSource", "");
        if (!info.id.empty()) out.push_back(std::move(info));
    }
    return out;
}

AssetBakeResult run_asset_bake(const std::filesystem::path& project_root, const std::string& target,
    const std::string& source_override) {
    std::vector<std::string> extra{"--target", target};
    if (!source_override.empty()) {
        extra.push_back("--source");
        extra.push_back(source_override);
    }
    return parse_bake_stdout(invoke_asset_bake(project_root, extra));
}

EditorBridgeResponse apply_asset_bake_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params) {
    EditorBridgeResponse response;
    response.request_id = params.value("requestId", "");
    const std::string action = params.value("action", params.value("list", false) ? "list" : "bake");
    if (action == "list") {
        const auto targets = list_asset_bake_targets();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& t : targets) {
            arr.push_back({{"id", t.id}, {"kind", t.kind}, {"defaultSource", t.default_source}});
        }
        response.exit_code = ExitCode::Success;
        response.summary = "asset bake catalog";
        response.metadata["targetsJson"] = arr.dump();
        response.metadata["targetCount"] = std::to_string(targets.size());
        return response;
    }

    const std::string target = params.value("target", "");
    if (target.empty()) {
        response.exit_code = ExitCode::InvalidArguments;
        response.summary = "target required";
        response.diagnostics.push_back(
            bake_error("ASSET-BAKE-SOURCE-MISSING", "target is required", "Pass target id from --list / catalog."));
        return response;
    }
    const std::string source = params.value("source", "");
    const auto result = run_asset_bake(project_root, target, source);
    response.exit_code = result.ok ? ExitCode::Success : ExitCode::ValidationFailed;
    response.summary = result.summary;
    response.diagnostics = result.diagnostics;
    response.metadata["reportJson"] = result.raw_json;
    if (!result.mesh_reloads.empty()) {
        response.metadata["meshReloads"] = nlohmann::json(result.mesh_reloads).dump();
        for (const auto& mesh : result.mesh_reloads) response.changed_object_ids.push_back(mesh);
    }
    return response;
}

} // namespace engine
