#include "engine/diagnostics/crash_bundle.h"
#include "engine/diagnostics/gpu_diagnostics.h"
#include "engine/diagnostics/logger.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <dbghelp.h>

namespace engine {
namespace {
std::filesystem::path g_root;
std::atomic<bool> g_handling_crash{false};

const char* exception_name(DWORD code) noexcept {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case STATUS_HEAP_CORRUPTION: return "HEAP_CORRUPTION";
    case STATUS_STACK_BUFFER_OVERRUN: return "STACK_BUFFER_OVERRUN";
    case 0xE06D7363: return "CPP_EH_EXCEPTION";
    default: return "UNKNOWN";
    }
}

std::string hex(std::uint64_t value, int width = 16) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(width) << std::setfill('0') << value;
    return out.str();
}

/// Module file name + offset for an address, e.g. `engine.exe+0x175350`.
std::string module_offset(HANDLE process, DWORD64 address) {
    IMAGEHLP_MODULE64 module{};
    module.SizeOfStruct = sizeof(module);
    if (!SymGetModuleInfo64(process, address, &module))
        return hex(address);
    return std::string(module.ModuleName) + "+" + hex(address - module.BaseOfImage, 1);
}

/// Symbolized frames for the faulting thread. Empty when dbghelp is unavailable.
std::vector<std::string> symbolized_stack(EXCEPTION_POINTERS* exception) {
    std::vector<std::string> frames;
    if (!exception || !exception->ContextRecord)
        return frames;

    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES
                  | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    if (!SymInitialize(process, nullptr, TRUE))
        return frames;

    // StackWalk64 mutates the context it walks, so hand it a copy.
    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symbol_storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    for (int index = 0; index < 64; ++index) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        std::ostringstream line;
        line << "#" << std::setw(2) << std::setfill('0') << index << " "
             << module_offset(process, frame.AddrPC.Offset);
        DWORD64 displacement = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
            line << " " << symbol->Name << " + " << hex(displacement, 1);
        IMAGEHLP_LINE64 source{};
        source.SizeOfStruct = sizeof(source);
        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &source))
            line << " (" << source.FileName << ":" << source.LineNumber << ")";
        frames.push_back(line.str());
    }

    SymCleanup(process);
    return frames;
}

/// Human-readable exception summary lines (code, faulting address, access kind).
std::vector<std::string> exception_summary(EXCEPTION_POINTERS* exception) {
    std::vector<std::string> lines;
    if (!exception || !exception->ExceptionRecord)
        return lines;
    const EXCEPTION_RECORD& record = *exception->ExceptionRecord;
    lines.push_back(std::string("exceptionCode=") + hex(record.ExceptionCode, 8) + " "
                    + exception_name(record.ExceptionCode));
    lines.push_back("exceptionAddress=" + hex(reinterpret_cast<std::uint64_t>(record.ExceptionAddress)));
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
        const char* kind = record.ExceptionInformation[0] == 0   ? "read"
                           : record.ExceptionInformation[0] == 1 ? "write"
                                                                 : "execute";
        lines.push_back(std::string("accessViolation=") + kind + " at "
                        + hex(record.ExceptionInformation[1]));
    }
    return lines;
}

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
    // A second faulting thread would otherwise race the first and leave a
    // truncated (0-byte) minidump beside the good bundle.
    if (g_handling_crash.exchange(true))
        return EXCEPTION_EXECUTE_HANDLER;

    auto causes = exception_summary(exception);
    const auto frames = symbolized_stack(exception);
    causes.insert(causes.end(), frames.begin(), frames.end());

    EngineError error{"CRASH-UNHANDLED", Severity::Fatal, ErrorCategory::InternalInvariant,
                      "process", "Unhandled structured exception", std::nullopt, causes,
                      "Read stack.txt in the bundle; open process.dmp for full state.",
                      make_correlation_id()};
    auto bundle = CrashBundle::write_diagnostic_bundle(g_root, error);
    if (bundle) {
        std::ofstream stack(bundle.value() / "stack.txt", std::ios::trunc);
        for (const auto& cause : causes)
            stack << cause << '\n';
        stack.close();

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
                   << "\",\"loadedWorldCells\":[],\"recentCommands\":[],\"gpuDiagnostics\":"
                   << process_gpu_diagnostics().to_json() << '}';
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
