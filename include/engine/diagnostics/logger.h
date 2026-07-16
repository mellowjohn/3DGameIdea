#pragma once

#include "engine/core/error.h"

#include <filesystem>
#include <fstream>
#include <deque>
#include <vector>
#include <mutex>
#include <string>

namespace engine {

class Logger final {
public:
    static Logger& instance();
    void initialize(const std::filesystem::path& jsonl_path);
    void write(Severity severity, std::string subsystem, std::string message,
               std::string correlation_id = {});
    void write(const EngineError& error);
    [[nodiscard]] std::filesystem::path log_path() const;
    [[nodiscard]] std::uint64_t error_count() const;
    [[nodiscard]] std::vector<EngineError> recent_errors() const;

private:
    Logger() = default;
    mutable std::mutex mutex_;
    std::ofstream stream_;
    std::filesystem::path path_;
    std::uint64_t error_count_ = 0;
    std::deque<EngineError> recent_errors_;
};

} // namespace engine
