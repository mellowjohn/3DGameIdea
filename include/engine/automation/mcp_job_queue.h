#pragma once

#include "engine/automation/editor_bridge.h"
#include "engine/core/result.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace engine {

struct EditorSessionContext;

enum class McpJobStatus : std::uint8_t {
  Queued = 0,
  Running,
  Succeeded,
  Failed,
  Cancelled,
};

[[nodiscard]] const char *mcp_job_status_string(McpJobStatus status) noexcept;

struct McpJobOp {
  std::string target; // terrain | scene | water
  nlohmann::json params = nlohmann::json::object();
};

struct McpJob {
  std::string id;
  std::string label;
  McpJobStatus status = McpJobStatus::Queued;
  std::vector<McpJobOp> ops;
  std::size_t next_index = 0;
  int ops_per_frame = 1;
  bool cancel_requested = false;
  std::string summary;
  std::vector<std::string> changed_object_ids;
  std::vector<EngineError> diagnostics;
  std::map<std::string, std::string> metadata;
};

/// Async MCP recipe runner: submit heavy terrain/scene/water op lists, tick one
/// (or few) ops per editor frame so the bridge stays responsive.
class McpJobQueue final {
public:
  static constexpr std::size_t k_max_ops_per_job = 512;
  static constexpr std::size_t k_max_retained_jobs = 24;

  [[nodiscard]] bool has_work() const noexcept;
  [[nodiscard]] std::size_t queued_or_running_count() const noexcept;

  /// Parse submit/status/cancel/list and return a bridge response.
  [[nodiscard]] EditorBridgeResponse handle_call(const nlohmann::json &params);

  /// Advance the active job by up to `ops_budget` ops (default from job).
  /// Returns true when any op ran this tick.
  bool tick(EditorSessionContext &context, int ops_budget = 0);

  [[nodiscard]] const McpJob *find(const std::string &job_id) const;
  [[nodiscard]] McpJob *find_mutable(const std::string &job_id);

private:
  [[nodiscard]] EditorBridgeResponse submit(const nlohmann::json &params);
  [[nodiscard]] EditorBridgeResponse status_response(const std::string &job_id,
                                                     bool include_ops) const;
  [[nodiscard]] EditorBridgeResponse cancel(const std::string &job_id);
  [[nodiscard]] EditorBridgeResponse list() const;

  void prune_history();
  [[nodiscard]] static Result<McpJobOp> parse_op(const nlohmann::json &op);
  [[nodiscard]] static std::string operation_for_target(const std::string &target);
  [[nodiscard]] EditorBridgeResponse job_to_response(const McpJob &job,
                                                    bool include_ops) const;

  std::deque<McpJob> jobs_;
  std::string active_job_id_;
};

} // namespace engine
