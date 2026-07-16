#include "engine/automation/command.h"
#include "engine/automation/automation_trace.h"
#include "engine/automation/mcp_server.h"
#include "engine/diagnostics/crash_bundle.h"
#include "engine/diagnostics/logger.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    using namespace engine;
    std::filesystem::path log_path="out/logs/engine.jsonl";
    for(int i=1;i+1<argc;++i)if(std::string(argv[i])=="--log-file")log_path=argv[i+1];
    auto parsed = parse_command_line(argc, argv);
    if (!parsed) {
        Logger::instance().initialize(log_path);
        Logger::instance().write(parsed.error());
        std::cerr << parsed.error().to_text() << '\n';
        return static_cast<int>(ExitCode::InvalidArguments);
    }
    if (parsed.value().name == "mcp") {
        if (parsed.value().project.empty()) {
            std::cerr << "mcp requires --project\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }
        if (const char* trace_env = std::getenv("ENGINE_AUTOMATION_TRACE")) {
            if (trace_env[0] == '0' && (trace_env[1] == '\0' || trace_env[1] == '\n')) AutomationTrace::set_enabled(false);
        }
        AutomationTrace::set_log_root(parsed.value().project / "out/logs");
        const auto result = run_mcp_server(parsed.value().project);
        return result ? static_cast<int>(result.value()) : static_cast<int>(ExitCode::InternalError);
    }
    Logger::instance().initialize(log_path);
    Logger::instance().write(Severity::Info,"process","Engine session started; log="+log_path.generic_string());
    CrashBundle::install("crash-bundles");
    const auto response = execute_command(parsed.value());
    for (const auto& error : response.diagnostics) Logger::instance().write(error);
    if (parsed.value().json) std::cout << response.to_json() << '\n';
    else {
        std::cout << response.summary << '\n';
        for (const auto& error : response.diagnostics) std::cout << error.to_text() << '\n';
    }
    Logger::instance().write(Severity::Info,"process","Engine session finished; exitCode="+std::to_string(static_cast<int>(response.exit_code))+"; errors="+std::to_string(Logger::instance().error_count()));
    return static_cast<int>(response.exit_code);
}
