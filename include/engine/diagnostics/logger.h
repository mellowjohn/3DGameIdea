#pragma once

#include "engine/core/error.h"

#include <filesystem>
#include <fstream>
#include <deque>
#include <cstdint>
#include <vector>
#include <mutex>
#include <string>

namespace engine {

struct ProcessLogContext {
    std::string session_id;
    std::string started_at_utc;
    std::string mode;
    std::string project;
    std::string world;
    std::string build_configuration;
    std::uint32_t pid = 0;
};

class Logger final {
public:
    static Logger& instance();
    void initialize(const std::filesystem::path& jsonl_path);
    void set_process_context(ProcessLogContext context);
    void write(Severity severity, std::string subsystem, std::string message,
               std::string correlation_id = {});
    void write(const EngineError& error);
    /// Writes a JSON object payload as a first-class event. This is intended
    /// for rare diagnostic events (such as an FPS dip), never per-frame data.
    void write_event(std::string event_name, std::string json_object_payload);
    [[nodiscard]] std::filesystem::path log_path() const;
    [[nodiscard]] std::uint64_t error_count() const;
    [[nodiscard]] std::vector<EngineError> recent_errors() const;

private:
    Logger() = default;
    mutable std::mutex mutex_;
    std::ofstream stream_;
    std::filesystem::path path_;
    ProcessLogContext process_context_;
    std::uint64_t error_count_ = 0;
    std::deque<EngineError> recent_errors_;
};

} // namespace engine
