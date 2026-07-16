#include "engine/diagnostics/logger.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <windows.h>

namespace engine {

Logger& Logger::instance() { static Logger value; return value; }

void Logger::initialize(const std::filesystem::path& jsonl_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = jsonl_path;
    if (path_.has_parent_path()) std::filesystem::create_directories(path_.parent_path());
    stream_.open(path_, std::ios::app);
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
    if (stream_) { stream_ << error.to_json() << '\n'; stream_.flush(); }
    if(error.severity==Severity::Error||error.severity==Severity::Fatal){++error_count_;recent_errors_.push_back(error);while(recent_errors_.size()>128)recent_errors_.pop_front();}
}
std::uint64_t Logger::error_count() const { std::lock_guard<std::mutex> lock(mutex_); return error_count_; }
std::vector<EngineError> Logger::recent_errors() const { std::lock_guard<std::mutex> lock(mutex_); return {recent_errors_.begin(),recent_errors_.end()}; }

std::filesystem::path Logger::log_path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

} // namespace engine
