#include "engine/diagnostics/crash_bundle.h"
#include "engine/diagnostics/logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <windows.h>
#include <dbghelp.h>

namespace engine {
namespace {
std::filesystem::path g_root;

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &value);
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d-%H%M%S");
    return out.str();
}

LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* exception) {
    EngineError error{"CRASH-UNHANDLED", Severity::Fatal, ErrorCategory::InternalInvariant,
                      "process", "Unhandled structured exception", std::nullopt, {},
                      "Open the generated minidump and diagnostic.json.", make_correlation_id()};
    auto bundle = CrashBundle::write_diagnostic_bundle(g_root, error);
    if (bundle) {
        const auto dump_path = bundle.value() / "process.dmp";
        HANDLE file = CreateFileW(dump_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION details{GetCurrentThreadId(), exception, FALSE};
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &details, nullptr, nullptr);
            CloseHandle(file);
        }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
}

void CrashBundle::install(const std::filesystem::path& root) {
    g_root = root;
    SetUnhandledExceptionFilter(unhandled_filter);
}

Result<std::filesystem::path> CrashBundle::write_diagnostic_bundle(
    const std::filesystem::path& root, const EngineError& reason) {
    try {
        const auto folder = root / (timestamp() + "-" + reason.correlation_id);
        std::filesystem::create_directories(folder);
        std::ofstream diagnostic(folder / "diagnostic.json", std::ios::trunc);
        diagnostic << "{\"formatVersion\":1,\"build\":\"0.2.0-dev\",\"reason\":" << reason.to_json()
                   << ",\"logPath\":\"" << Logger::instance().log_path().generic_string()
                   << "\",\"loadedWorldCells\":[],\"recentCommands\":[],\"gpuDiagnostics\":null}";
        diagnostic.close();
        return Result<std::filesystem::path>::success(folder);
    } catch (const std::exception& exception) {
        EngineError error{"DIAG-BUNDLE-WRITE", Severity::Error, ErrorCategory::Io, "diagnostics",
                          "Could not write diagnostic bundle", ENGINE_SOURCE_CONTEXT,
                          {exception.what()}, "Check free space and directory permissions.", make_correlation_id()};
        return Result<std::filesystem::path>::failure(std::move(error));
    }
}

} // namespace engine
