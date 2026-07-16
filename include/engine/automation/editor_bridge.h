#pragma once

#include "engine/automation/command.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace engine {

struct EditorBridgeRequest {
    int schema_version = 1;
    std::string request_id;
    std::string operation;
    std::string params_json;
};

struct EditorBridgeResponse {
    int schema_version = 1;
    std::string request_id;
    ExitCode exit_code = ExitCode::Success;
    std::string summary;
    std::vector<std::string> changed_object_ids;
    std::vector<EngineError> diagnostics;
    std::map<std::string, std::string> metadata;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static std::optional<EditorBridgeRequest> parse_request(const std::string& json);
};

[[nodiscard]] std::string editor_bridge_pipe_name(const std::filesystem::path& project_root);

class EditorBridgeServer final {
public:
    explicit EditorBridgeServer(std::filesystem::path project_root);
    ~EditorBridgeServer();
    EditorBridgeServer(const EditorBridgeServer&) = delete;
    EditorBridgeServer& operator=(const EditorBridgeServer&) = delete;
    [[nodiscard]] bool start();
    void stop();
    void poll_pending(std::function<EditorBridgeResponse(const EditorBridgeRequest&)> handler);
    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_acquire); }
private:
    void worker_main();
    void close_listening_pipe();

    std::filesystem::path project_root_;
    void* pipe_handle_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable request_ready_;
    std::condition_variable response_ready_;
    bool request_pending_ = false;
    bool response_pending_ = false;
    EditorBridgeRequest active_request_;
    EditorBridgeResponse active_response_;
};

class EditorBridgeClient final {
public:
    explicit EditorBridgeClient(std::filesystem::path project_root);
    [[nodiscard]] bool is_editor_running() const;
    [[nodiscard]] EditorBridgeResponse send(const EditorBridgeRequest& request, std::uint32_t timeout_ms = 5000) const;
private:
    std::filesystem::path project_root_;
};

} // namespace engine
