#include "engine/automation/project_git_commands.h"

#include "engine/core/error.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace engine {
namespace {

EngineError git_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "project_git", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EditorBridgeResponse make_response(ExitCode exit_code, std::string summary,
    std::vector<EngineError> diagnostics = {}, std::map<std::string, std::string> metadata = {},
    std::vector<std::string> changed = {}) {
    EditorBridgeResponse response;
    response.exit_code = exit_code;
    response.summary = std::move(summary);
    response.diagnostics = std::move(diagnostics);
    response.metadata = std::move(metadata);
    response.changed_object_ids = std::move(changed);
    return response;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim_copy(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' '))
        value.pop_back();
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\n' || value[start] == '\r')) ++start;
    return value.substr(start);
}

std::string join_lines(const std::vector<std::string>& lines, std::size_t max_chars = 4000) {
    std::ostringstream out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out << '\n';
        out << lines[i];
        if (out.tellp() > static_cast<std::streamoff>(max_chars)) {
            out << "\n...";
            break;
        }
    }
    return out.str();
}

bool path_is_blocked(const std::string& relative_generic) {
    const auto key = lower_copy(relative_generic);
    static constexpr const char* blocked_prefixes[] = {
        "build/", ".vs/", "out/", "cmake-build-", "cmake-build/", ".idea/", "node_modules/",
    };
    for (const char* prefix : blocked_prefixes) {
        if (key.rfind(prefix, 0) == 0) return true;
        if (key.find(std::string("/") + prefix) != std::string::npos) return true;
    }
    static constexpr const char* blocked_suffixes[] = {
        ".exe", ".pdb", ".obj", ".lib", ".dll", ".ilk", ".exp", ".idb", ".suo", ".user",
    };
    for (const char* suffix : blocked_suffixes) {
        const auto len = std::char_traits<char>::length(suffix);
        if (key.size() >= len && key.compare(key.size() - len, len, suffix) == 0) return true;
    }
    if (key.find(".env") != std::string::npos) return true;
    if (key.find("secrets") != std::string::npos) return true;
    return false;
}

bool looks_like_world_forge(const std::string& relative_generic) {
    const auto key = lower_copy(relative_generic);
    return key.find(".worldforge.json") != std::string::npos || key.find("world-forge/") != std::string::npos;
}

bool looks_like_scene(const std::string& relative_generic) {
    const auto key = lower_copy(relative_generic);
    return key.size() >= 11 && key.compare(key.size() - 11, 11, ".world.json") == 0;
}

struct ProcessResult {
    int exit_code = 1;
    std::string stdout_text;
    std::string stderr_text;
};

#if defined(_WIN32)
std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), count);
    return out;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int count =
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), count, nullptr, nullptr);
    return out;
}

std::wstring quote_windows_arg(const std::string& arg) {
    std::wstring wide = utf8_to_wide(arg);
    if (wide.find_first_of(L" \t\"") == std::wstring::npos) return wide;
    std::wstring out = L"\"";
    for (wchar_t ch : wide) {
        if (ch == L'"') out += L"\\\"";
        else out += ch;
    }
    out += L'"';
    return out;
}

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
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = stdout_write;
    startup.hStdError = stderr_write;

    PROCESS_INFORMATION process{};
    std::wstring cwd_wide = utf8_to_wide(cwd.string());
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
        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0) _exit(127);
        }
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
        for (;;) {
            const ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n <= 0) break;
            text.append(buffer, buffer + n);
        }
        return text;
    };
    result.stdout_text = read_fd(stdout_pipe[0]);
    result.stderr_text = read_fd(stderr_pipe[0]);
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else result.exit_code = 1;
    return result;
}
#endif

ProcessResult run_git(const std::filesystem::path& repo_root, const std::vector<std::string>& git_args) {
    std::vector<std::string> args;
    args.reserve(git_args.size() + 1);
    args.push_back("git");
    args.insert(args.end(), git_args.begin(), git_args.end());
    return run_process(repo_root, args);
}

bool git_available() {
    const auto probe = run_process(std::filesystem::current_path(), {"git", "--version"});
    return probe.exit_code == 0 && probe.stdout_text.find("git version") != std::string::npos;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        if (ch == '\n') {
            if (!current.empty() && current.back() == '\r') current.pop_back();
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        if (current.back() == '\r') current.pop_back();
        lines.push_back(current);
    }
    return lines;
}

struct RepoContext {
    std::filesystem::path project_root;
    std::filesystem::path repo_root;
    std::string project_prefix; // relative to repo root, empty means whole repo
};

Result<RepoContext> resolve_repo(const std::filesystem::path& project_root) {
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(project_root, ec);
    const auto root = ec ? std::filesystem::absolute(project_root) : absolute;
    if (!std::filesystem::exists(root)) {
        return Result<RepoContext>::failure(git_error("PROJECT-GIT-PROJECT-MISSING", ErrorCategory::Validation,
            "Project root does not exist: " + root.generic_string(), "Pass a valid --project directory."));
    }
    if (!git_available()) {
        return Result<RepoContext>::failure(git_error("PROJECT-GIT-GIT-MISSING", ErrorCategory::Configuration,
            "git was not found on PATH.", "Install Git and ensure `git` is available in the process PATH."));
    }
    const auto inside = run_process(root, {"git", "rev-parse", "--is-inside-work-tree"});
    if (inside.exit_code != 0 || trim_copy(inside.stdout_text) != "true") {
        return Result<RepoContext>::failure(git_error("PROJECT-GIT-NOT-A-REPO", ErrorCategory::Validation,
            "Project is not inside a git working tree.",
            "Open a project under a git clone, or initialize git for the content repo."));
    }
    const auto toplevel = run_process(root, {"git", "rev-parse", "--show-toplevel"});
    if (toplevel.exit_code != 0) {
        return Result<RepoContext>::failure(git_error("PROJECT-GIT-TOPLEVEL-FAILED", ErrorCategory::InternalInvariant,
            "Failed to resolve git toplevel.", trim_copy(toplevel.stderr_text)));
    }
    RepoContext context;
    context.project_root = root;
    context.repo_root = std::filesystem::path(trim_copy(toplevel.stdout_text));
    const auto project_generic = context.project_root.lexically_normal().generic_string();
    const auto repo_generic = context.repo_root.lexically_normal().generic_string();
    if (project_generic == repo_generic) {
        context.project_prefix.clear();
    } else if (project_generic.rfind(repo_generic, 0) == 0) {
        auto relative = project_generic.substr(repo_generic.size());
        while (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
        context.project_prefix = relative;
    } else {
        context.project_prefix.clear();
    }
    return Result<RepoContext>::success(std::move(context));
}

std::string current_branch(const RepoContext& context) {
    const auto symbolic = run_git(context.repo_root, {"symbolic-ref", "--short", "HEAD"});
    if (symbolic.exit_code == 0) return trim_copy(symbolic.stdout_text);
    const auto detached = run_git(context.repo_root, {"rev-parse", "--short", "HEAD"});
    if (detached.exit_code == 0) return "detached@" + trim_copy(detached.stdout_text);
    return {};
}

std::vector<std::string> scope_pathspecs(const RepoContext& context) {
    if (context.project_prefix.empty()) return {"."};
    return {context.project_prefix};
}

struct StatusEntry {
    std::string xy;
    std::string path;
};

std::vector<StatusEntry> parse_porcelain(const std::string& text) {
    std::vector<StatusEntry> entries;
    for (const auto& line : split_lines(text)) {
        if (line.size() < 4) continue;
        StatusEntry entry;
        entry.xy = line.substr(0, 2);
        std::string path = line.substr(3);
        const auto arrow = path.find(" -> ");
        if (arrow != std::string::npos) path = path.substr(arrow + 4);
        entry.path = path;
        entries.push_back(std::move(entry));
    }
    return entries;
}

bool entry_in_scope(const RepoContext& context, const std::string& path) {
    if (context.project_prefix.empty()) return true;
    const auto key = path;
    const auto prefix = context.project_prefix + "/";
    return key == context.project_prefix || key.rfind(prefix, 0) == 0;
}

void fill_common_metadata(std::map<std::string, std::string>& metadata, const RepoContext& context,
    const std::string& action) {
    metadata["action"] = action;
    metadata["repoRoot"] = context.repo_root.generic_string();
    metadata["projectRoot"] = context.project_root.generic_string();
    metadata["projectPrefix"] = context.project_prefix;
    metadata["branch"] = current_branch(context);
}

EditorBridgeResponse status_action(const RepoContext& context) {
    std::vector<std::string> args = {"status", "--porcelain=v1", "-b", "--"};
    const auto specs = scope_pathspecs(context);
    args.insert(args.end(), specs.begin(), specs.end());
    const auto result = run_git(context.repo_root, args);
    if (result.exit_code != 0) {
        return make_response(ExitCode::InternalError, "git status failed",
            {git_error("PROJECT-GIT-STATUS-FAILED", ErrorCategory::InternalInvariant, trim_copy(result.stderr_text),
                "Check repository state and retry.")});
    }

    std::map<std::string, std::string> metadata;
    fill_common_metadata(metadata, context, "status");

    std::vector<std::string> dirty;
    std::vector<std::string> conflicted;
    std::string branch_header;
    for (const auto& line : split_lines(result.stdout_text)) {
        if (line.rfind("## ", 0) == 0) {
            branch_header = line.substr(3);
            metadata["branchHeader"] = branch_header;
            continue;
        }
        if (line.size() < 4) continue;
        const auto xy = line.substr(0, 2);
        std::string path = line.substr(3);
        const auto arrow = path.find(" -> ");
        if (arrow != std::string::npos) path = path.substr(arrow + 4);
        if (!entry_in_scope(context, path)) continue;
        if (path_is_blocked(path)) continue;
        dirty.push_back(path);
        if (xy.find('U') != std::string::npos || xy == "AA" || xy == "DD") conflicted.push_back(path);
    }
    metadata["dirtyCount"] = std::to_string(dirty.size());
    metadata["dirtyPaths"] = join_lines(dirty);
    metadata["conflictedCount"] = std::to_string(conflicted.size());
    metadata["conflictedPaths"] = join_lines(conflicted);
    metadata["stdout"] = trim_copy(result.stdout_text);

    std::string summary = "On " + metadata["branch"];
    if (!dirty.empty()) summary += " — " + std::to_string(dirty.size()) + " dirty path(s)";
    else summary += " — clean";
    if (!conflicted.empty()) summary += " — " + std::to_string(conflicted.size()) + " conflict(s)";
    return make_response(ExitCode::Success, std::move(summary), {}, std::move(metadata), std::move(dirty));
}

EditorBridgeResponse fetch_action(const RepoContext& context) {
    const auto result = run_git(context.repo_root, {"fetch", "--prune"});
    std::map<std::string, std::string> metadata;
    fill_common_metadata(metadata, context, "fetch");
    metadata["stdout"] = trim_copy(result.stdout_text);
    metadata["stderr"] = trim_copy(result.stderr_text);
    if (result.exit_code != 0) {
        return make_response(ExitCode::Unavailable, "git fetch failed",
            {git_error("PROJECT-GIT-FETCH-FAILED", ErrorCategory::Io, trim_copy(result.stderr_text),
                "Check remote URL, network, and OS git credentials / SSH agent.")},
            std::move(metadata));
    }
    return make_response(ExitCode::Success, "Fetched remotes", {}, std::move(metadata));
}

EditorBridgeResponse pull_action(const RepoContext& context) {
    const auto before = run_git(context.repo_root, {"rev-parse", "HEAD"});
    const std::string before_sha = before.exit_code == 0 ? trim_copy(before.stdout_text) : std::string{};
    const auto result = run_git(context.repo_root, {"pull", "--ff-only"});
    std::map<std::string, std::string> metadata;
    fill_common_metadata(metadata, context, "pull");
    metadata["stdout"] = trim_copy(result.stdout_text);
    metadata["stderr"] = trim_copy(result.stderr_text);
    metadata["beforeHead"] = before_sha;

    if (result.exit_code != 0) {
        const auto status = status_action(context);
        if (status.metadata.count("conflictedPaths"))
            metadata["conflictedPaths"] = status.metadata.at("conflictedPaths");
        if (status.metadata.count("conflictedCount"))
            metadata["conflictedCount"] = status.metadata.at("conflictedCount");
        return make_response(ExitCode::ValidationFailed, "git pull failed",
            {git_error("PROJECT-GIT-PULL-FAILED", ErrorCategory::Validation,
                trim_copy(result.stderr_text.empty() ? result.stdout_text : result.stderr_text),
                "Resolve conflicts with git, or rebase/merge outside the engine. Fast-forward only is used in v1.")},
            std::move(metadata));
    }

    const auto after = run_git(context.repo_root, {"rev-parse", "HEAD"});
    const std::string after_sha = after.exit_code == 0 ? trim_copy(after.stdout_text) : std::string{};
    metadata["afterHead"] = after_sha;

    std::vector<std::string> changed;
    bool world_forge = false;
    bool scene = false;
    if (!before_sha.empty() && !after_sha.empty() && before_sha != after_sha) {
        const auto diff = run_git(context.repo_root, {"diff", "--name-only", before_sha, after_sha});
        for (const auto& path : split_lines(diff.stdout_text)) {
            if (path.empty() || !entry_in_scope(context, path)) continue;
            changed.push_back(path);
            if (looks_like_world_forge(path)) world_forge = true;
            if (looks_like_scene(path)) scene = true;
        }
    }
    metadata["changedCount"] = std::to_string(changed.size());
    metadata["changedPaths"] = join_lines(changed);
    metadata["changedWorldForge"] = world_forge ? "true" : "false";
    metadata["changedScene"] = scene ? "true" : "false";
    metadata["requiresWorldForgeReload"] = world_forge ? "true" : "false";

    std::string summary = before_sha == after_sha ? "Already up to date" : "Pulled updates";
    if (world_forge) summary += " — World Forge files changed (reload recommended)";
    return make_response(ExitCode::Success, std::move(summary), {}, std::move(metadata), std::move(changed));
}

EditorBridgeResponse commit_action(const RepoContext& context, const nlohmann::json& params) {
    const auto message = trim_copy(params.value("message", std::string{}));
    if (message.empty()) {
        return make_response(ExitCode::InvalidArguments, "commit requires message",
            {git_error("PROJECT-GIT-MESSAGE-REQUIRED", ErrorCategory::Validation,
                "Commit message was empty.", "Pass message in params or --message on the CLI.")});
    }

    std::vector<std::string> to_stage;
    if (params.contains("paths") && params["paths"].is_array()) {
        for (const auto& entry : params["paths"]) {
            if (!entry.is_string()) continue;
            const auto path = entry.get<std::string>();
            if (path.empty() || path_is_blocked(path)) continue;
            if (!entry_in_scope(context, path)) continue;
            to_stage.push_back(path);
        }
    } else {
        std::vector<std::string> status_args = {"status", "--porcelain=v1", "--"};
        const auto specs = scope_pathspecs(context);
        status_args.insert(status_args.end(), specs.begin(), specs.end());
        const auto porcelain = run_git(context.repo_root, status_args);
        for (const auto& entry : parse_porcelain(porcelain.stdout_text)) {
            if (!entry_in_scope(context, entry.path)) continue;
            if (path_is_blocked(entry.path)) continue;
            to_stage.push_back(entry.path);
        }
    }

    std::sort(to_stage.begin(), to_stage.end());
    to_stage.erase(std::unique(to_stage.begin(), to_stage.end()), to_stage.end());
    if (to_stage.empty()) {
        return make_response(ExitCode::ValidationFailed, "Nothing to commit in project scope",
            {git_error("PROJECT-GIT-NOTHING-TO-COMMIT", ErrorCategory::Validation,
                "No stageable project content paths were found.",
                "Save World Forge / content files first, or pass explicit paths[].")});
    }

    std::vector<std::string> add_args = {"add", "--"};
    add_args.insert(add_args.end(), to_stage.begin(), to_stage.end());
    const auto add = run_git(context.repo_root, add_args);
    if (add.exit_code != 0) {
        return make_response(ExitCode::InternalError, "git add failed",
            {git_error("PROJECT-GIT-ADD-FAILED", ErrorCategory::InternalInvariant, trim_copy(add.stderr_text),
                "Check path permissions and retry.")});
    }

    // Resolve directory pathspecs to concrete cached files; drop anything blocked that slipped in.
    const auto cached = run_git(context.repo_root, {"diff", "--cached", "--name-only", "-z"});
    std::vector<std::string> staged_files;
    {
        std::string current;
        for (char ch : cached.stdout_text) {
            if (ch == '\0') {
                if (!current.empty()) staged_files.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) staged_files.push_back(current);
    }
    std::vector<std::string> keep;
    std::vector<std::string> drop;
    for (const auto& path : staged_files) {
        if (!entry_in_scope(context, path) || path_is_blocked(path)) drop.push_back(path);
        else keep.push_back(path);
    }
    if (!drop.empty()) {
        std::vector<std::string> reset_args = {"reset", "-q", "HEAD", "--"};
        reset_args.insert(reset_args.end(), drop.begin(), drop.end());
        (void)run_git(context.repo_root, reset_args);
    }
    if (keep.empty()) {
        return make_response(ExitCode::ValidationFailed, "Nothing to commit in project scope",
            {git_error("PROJECT-GIT-NOTHING-TO-COMMIT", ErrorCategory::Validation,
                "No stageable project content paths remained after filtering build/secret artifacts.",
                "Save World Forge / content files first, or pass explicit paths[].")});
    }

    const auto commit = run_git(context.repo_root, {"commit", "-m", message});
    std::map<std::string, std::string> metadata;
    fill_common_metadata(metadata, context, "commit");
    metadata["message"] = message;
    metadata["stagedPaths"] = join_lines(keep);
    metadata["stdout"] = trim_copy(commit.stdout_text);
    metadata["stderr"] = trim_copy(commit.stderr_text);
    if (commit.exit_code != 0) {
        return make_response(ExitCode::ValidationFailed, "git commit failed",
            {git_error("PROJECT-GIT-COMMIT-FAILED", ErrorCategory::Validation,
                trim_copy(commit.stderr_text.empty() ? commit.stdout_text : commit.stderr_text),
                "Configure user.name/user.email if needed, resolve conflicts, then retry.")},
            std::move(metadata));
    }
    const auto staged_count = keep.size();
    auto changed = keep;
    return make_response(ExitCode::Success, "Committed " + std::to_string(staged_count) + " path(s)", {},
        std::move(metadata), std::move(changed));
}

EditorBridgeResponse push_action(const RepoContext& context) {
    const auto result = run_git(context.repo_root, {"push"});
    std::map<std::string, std::string> metadata;
    fill_common_metadata(metadata, context, "push");
    metadata["stdout"] = trim_copy(result.stdout_text);
    metadata["stderr"] = trim_copy(result.stderr_text);
    if (result.exit_code != 0) {
        return make_response(ExitCode::Unavailable, "git push failed",
            {git_error("PROJECT-GIT-PUSH-FAILED", ErrorCategory::Io,
                trim_copy(result.stderr_text.empty() ? result.stdout_text : result.stderr_text),
                "Check remote permissions and OS git credentials / SSH agent.")},
            std::move(metadata));
    }
    return make_response(ExitCode::Success, "Pushed to remote", {}, std::move(metadata));
}

} // namespace

EditorBridgeResponse apply_project_git_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params) {
    const auto action = lower_copy(params.value("action", std::string{}));
    if (action.empty()) {
        return make_response(ExitCode::InvalidArguments, "action is required",
            {git_error("PROJECT-GIT-ACTION-REQUIRED", ErrorCategory::Validation, "Missing action.",
                "Use action=status|fetch|pull|commit|push.")});
    }

    auto resolved = resolve_repo(project_root);
    if (!resolved) {
        return make_response(ExitCode::Unavailable, resolved.error().message, {resolved.error()});
    }
    const RepoContext& context = resolved.value();

    if (action == "status") return status_action(context);
    if (action == "fetch") return fetch_action(context);
    if (action == "pull") return pull_action(context);
    if (action == "commit") return commit_action(context, params);
    if (action == "push") return push_action(context);

    return make_response(ExitCode::InvalidArguments, "Unsupported project git action: " + action,
        {git_error("PROJECT-GIT-ACTION-UNKNOWN", ErrorCategory::Validation,
            "Unsupported action: " + action, "Use status|fetch|pull|commit|push.")});
}

} // namespace engine
