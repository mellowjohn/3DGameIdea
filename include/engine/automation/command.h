#pragma once

#include "engine/core/result.h"

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

enum class ExitCode : int {
    Success = 0, InvalidArguments = 2, ConfigurationError = 3,
    ValidationFailed = 4, Unavailable = 5, InternalError = 10
};

struct CommandRequest {
    std::string name;
    std::filesystem::path project;
    bool json = false;
    bool dry_run = false;
    std::vector<std::string> arguments;
    std::string correlation_id;
};

struct CommandResponse {
    ExitCode exit_code = ExitCode::Success;
    std::string summary;
    std::vector<std::string> changed_object_ids;
    std::vector<EngineError> diagnostics;
    std::map<std::string, double> metrics;
    std::map<std::string, std::string> metadata;
    std::vector<std::string> artifacts;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] Result<CommandRequest> parse_command_line(int argc, char** argv);
[[nodiscard]] CommandResponse execute_command(const CommandRequest& request);
[[nodiscard]] std::string command_help();

} // namespace engine
