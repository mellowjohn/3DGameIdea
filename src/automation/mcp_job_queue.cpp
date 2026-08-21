#include "engine/automation/mcp_job_queue.h"

#include "engine/automation/editor_session.h"
#include "engine/core/error.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace engine {
namespace {

EngineError job_error(const char *code, std::string message, std::string remedy) {
  return EngineError{code,
                     Severity::Error,
                     ErrorCategory::Validation,
                     "automation",
                     std::move(message),
                     std::nullopt,
                     {},
                     std::move(remedy),
                     make_correlation_id()};
}

EditorBridgeResponse make_ok(std::string summary,
                             std::map<std::string, std::string> metadata = {}) {
  EditorBridgeResponse response;
  response.schema_version = 1;
  response.exit_code = ExitCode::Success;
  response.summary = std::move(summary);
  response.metadata = std::move(metadata);
  return response;
}

EditorBridgeResponse make_err(ExitCode code, std::string summary,
                              EngineError err) {
  EditorBridgeResponse response;
  response.schema_version = 1;
  response.exit_code = code;
  response.summary = std::move(summary);
  response.diagnostics.push_back(std::move(err));
  return response;
}

std::string normalize_target(std::string target) {
  std::transform(target.begin(), target.end(), target.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  if (target == "terrain_apply")
    return "terrain";
  if (target == "scene_apply")
    return "scene";
  if (target == "water_apply")
    return "water";
  return target;
}

} // namespace

const char *mcp_job_status_string(McpJobStatus status) noexcept {
  switch (status) {
  case McpJobStatus::Queued:
    return "queued";
  case McpJobStatus::Running:
    return "running";
  case McpJobStatus::Succeeded:
    return "succeeded";
  case McpJobStatus::Failed:
    return "failed";
  case McpJobStatus::Cancelled:
    return "cancelled";
  }
  return "unknown";
}

bool McpJobQueue::has_work() const noexcept {
  for (const auto &job : jobs_) {
    if (job.status == McpJobStatus::Queued ||
        job.status == McpJobStatus::Running)
      return true;
  }
  return false;
}

std::size_t McpJobQueue::queued_or_running_count() const noexcept {
  std::size_t n = 0;
  for (const auto &job : jobs_) {
    if (job.status == McpJobStatus::Queued ||
        job.status == McpJobStatus::Running)
      ++n;
  }
  return n;
}

const McpJob *McpJobQueue::find(const std::string &job_id) const {
  for (const auto &job : jobs_) {
    if (job.id == job_id)
      return &job;
  }
  return nullptr;
}

McpJob *McpJobQueue::find_mutable(const std::string &job_id) {
  for (auto &job : jobs_) {
    if (job.id == job_id)
      return &job;
  }
  return nullptr;
}

std::string McpJobQueue::operation_for_target(const std::string &target) {
  if (target == "terrain")
    return "terrain_apply";
  if (target == "scene")
    return "scene_apply";
  if (target == "water")
    return "water_apply";
  return {};
}

Result<McpJobOp> McpJobQueue::parse_op(const nlohmann::json &op) {
  if (!op.is_object()) {
    return Result<McpJobOp>::failure(
        job_error("JOB-OP-OBJECT", "Each job op must be a JSON object.",
                  "Example: {\"target\":\"terrain\",\"action\":\"smooth\",...}"));
  }
  McpJobOp parsed;
  if (op.contains("target") && op["target"].is_string())
    parsed.target = normalize_target(op["target"].get<std::string>());
  else if (op.contains("operation") && op["operation"].is_string())
    parsed.target = normalize_target(op["operation"].get<std::string>());
  else
    parsed.target = "terrain";

  if (operation_for_target(parsed.target).empty()) {
    return Result<McpJobOp>::failure(job_error(
        "JOB-OP-TARGET",
        "Unsupported job op target: " + parsed.target,
        "Use target terrain, scene, or water."));
  }

  parsed.params = op;
  parsed.params.erase("target");
  parsed.params.erase("operation");
  if (!parsed.params.contains("action") || !parsed.params["action"].is_string() ||
      parsed.params["action"].get<std::string>().empty()) {
    return Result<McpJobOp>::failure(job_error(
        "JOB-OP-ACTION", "Job op requires action string.",
        "Provide action (e.g. set_height_along, paint_along, save)."));
  }
  return Result<McpJobOp>::success(std::move(parsed));
}

EditorBridgeResponse McpJobQueue::job_to_response(const McpJob &job,
                                                 bool include_ops) const {
  auto response = make_ok(job.summary.empty() ? std::string("MCP job status")
                                              : job.summary);
  response.changed_object_ids = job.changed_object_ids;
  response.diagnostics = job.diagnostics;
  response.metadata = job.metadata;
  response.metadata["jobId"] = job.id;
  response.metadata["label"] = job.label;
  response.metadata["status"] = mcp_job_status_string(job.status);
  response.metadata["opIndex"] = std::to_string(job.next_index);
  response.metadata["opCount"] = std::to_string(job.ops.size());
  response.metadata["opsPerFrame"] = std::to_string(job.ops_per_frame);
  response.metadata["cancelRequested"] =
      job.cancel_requested ? "true" : "false";
  const float progress =
      job.ops.empty()
          ? 1.0f
          : static_cast<float>(std::min(job.next_index, job.ops.size())) /
                static_cast<float>(job.ops.size());
  response.metadata["progress"] = std::to_string(progress);
  if (include_ops) {
    nlohmann::json ops = nlohmann::json::array();
    for (const auto &op : job.ops) {
      nlohmann::json row = op.params;
      row["target"] = op.target;
      ops.push_back(std::move(row));
    }
    response.metadata["opsJson"] = ops.dump();
  }
  if (job.status == McpJobStatus::Failed)
    response.exit_code = ExitCode::ValidationFailed;
  return response;
}

void McpJobQueue::prune_history() {
  while (jobs_.size() > k_max_retained_jobs) {
    const auto &front = jobs_.front();
    if (front.status == McpJobStatus::Queued ||
        front.status == McpJobStatus::Running)
      break;
    if (front.id == active_job_id_)
      active_job_id_.clear();
    jobs_.pop_front();
  }
}

EditorBridgeResponse McpJobQueue::submit(const nlohmann::json &params) {
  if (!params.contains("ops") || !params["ops"].is_array()) {
    return make_err(ExitCode::InvalidArguments, "job submit requires ops[]",
                    job_error("JOB-SUBMIT-ARGS",
                              "submit requires a non-empty ops array.",
                              "Pass ops:[{target,action,...},...]"));
  }
  const auto &ops_json = params["ops"];
  if (ops_json.empty()) {
    return make_err(ExitCode::InvalidArguments, "job submit ops[] is empty",
                    job_error("JOB-SUBMIT-EMPTY", "ops[] must not be empty.",
                              "Include at least one terrain/scene/water op."));
  }
  if (ops_json.size() > k_max_ops_per_job) {
    return make_err(
        ExitCode::InvalidArguments, "job submit exceeds op cap",
        job_error("JOB-SUBMIT-CAP",
                  "ops[] exceeds max " + std::to_string(k_max_ops_per_job) +
                      ".",
                  "Split into multiple jobs."));
  }

  McpJob job;
  job.id = make_correlation_id();
  job.label = params.value("label", std::string{});
  if (job.label.empty())
    job.label = "mcp_job";
  job.ops_per_frame = std::clamp(params.value("opsPerFrame", 1), 1, 8);
  job.summary = "MCP job queued";
  job.metadata["label"] = job.label;

  for (const auto &op : ops_json) {
    auto parsed = parse_op(op);
    if (!parsed)
      return make_err(ExitCode::InvalidArguments, parsed.error().message,
                      parsed.error());
    job.ops.push_back(std::move(parsed.value()));
  }

  jobs_.push_back(std::move(job));
  prune_history();
  const auto &stored = jobs_.back();
  auto response = job_to_response(stored, false);
  response.summary = "MCP job submitted";
  response.metadata["status"] = "queued";
  return response;
}

EditorBridgeResponse McpJobQueue::status_response(const std::string &job_id,
                                                 bool include_ops) const {
  if (job_id.empty()) {
    if (!active_job_id_.empty()) {
      if (const auto *active = find(active_job_id_))
        return job_to_response(*active, include_ops);
    }
    for (auto it = jobs_.rbegin(); it != jobs_.rend(); ++it) {
      if (it->status == McpJobStatus::Running ||
          it->status == McpJobStatus::Queued)
        return job_to_response(*it, include_ops);
    }
    if (!jobs_.empty())
      return job_to_response(jobs_.back(), include_ops);
    return make_err(ExitCode::InvalidArguments, "no MCP jobs",
                    job_error("JOB-NONE", "No jobs have been submitted.",
                              "Call kind=submit first."));
  }
  const auto *job = find(job_id);
  if (!job) {
    return make_err(ExitCode::InvalidArguments, "job not found",
                    job_error("JOB-NOT-FOUND", "Unknown jobId: " + job_id,
                              "Use kind=list or the jobId from submit."));
  }
  return job_to_response(*job, include_ops);
}

EditorBridgeResponse McpJobQueue::cancel(const std::string &job_id) {
  McpJob *job = nullptr;
  if (!job_id.empty())
    job = find_mutable(job_id);
  else if (!active_job_id_.empty())
    job = find_mutable(active_job_id_);
  else {
    for (auto it = jobs_.rbegin(); it != jobs_.rend(); ++it) {
      if (it->status == McpJobStatus::Running ||
          it->status == McpJobStatus::Queued) {
        job = &(*it);
        break;
      }
    }
  }
  if (!job) {
    return make_err(ExitCode::InvalidArguments, "job not found",
                    job_error("JOB-NOT-FOUND", "No cancellable job found.",
                              "Pass jobId from submit."));
  }
  if (job->status == McpJobStatus::Succeeded ||
      job->status == McpJobStatus::Failed ||
      job->status == McpJobStatus::Cancelled) {
    return job_to_response(*job, false);
  }
  job->cancel_requested = true;
  if (job->status == McpJobStatus::Queued) {
    job->status = McpJobStatus::Cancelled;
    job->summary = "MCP job cancelled before start";
  } else {
    job->summary = "MCP job cancel requested";
  }
  return job_to_response(*job, false);
}

EditorBridgeResponse McpJobQueue::list() const {
  nlohmann::json rows = nlohmann::json::array();
  for (const auto &job : jobs_) {
    rows.push_back({{"jobId", job.id},
                    {"label", job.label},
                    {"status", mcp_job_status_string(job.status)},
                    {"opIndex", job.next_index},
                    {"opCount", job.ops.size()},
                    {"progress",
                     job.ops.empty()
                         ? 1.0
                         : static_cast<double>(job.next_index) /
                               static_cast<double>(job.ops.size())}});
  }
  auto response = make_ok("MCP job list",
                          {{"count", std::to_string(jobs_.size())},
                           {"activeJobId", active_job_id_},
                           {"jobsJson", rows.dump()}});
  return response;
}

EditorBridgeResponse McpJobQueue::handle_call(const nlohmann::json &params) {
  const auto kind = params.value("kind", params.value("action", std::string{}));
  if (kind == "submit")
    return submit(params);
  if (kind == "status") {
    return status_response(params.value("jobId", std::string{}),
                           params.value("includeOps", false));
  }
  if (kind == "cancel")
    return cancel(params.value("jobId", std::string{}));
  if (kind == "list")
    return list();
  return make_err(
      ExitCode::InvalidArguments, "unsupported job kind",
      job_error("JOB-KIND", "kind must be submit|status|cancel|list.",
                "Example: {\"kind\":\"submit\",\"ops\":[...]}"));
}

bool McpJobQueue::tick(EditorSessionContext &context, int ops_budget) {
  if (active_job_id_.empty()) {
    for (auto &job : jobs_) {
      if (job.status == McpJobStatus::Queued) {
        active_job_id_ = job.id;
        job.status = McpJobStatus::Running;
        job.summary = "MCP job running";
        break;
      }
    }
  }
  if (active_job_id_.empty())
    return false;

  auto *job = find_mutable(active_job_id_);
  if (!job) {
    active_job_id_.clear();
    return false;
  }

  if (job->cancel_requested) {
    job->status = McpJobStatus::Cancelled;
    job->summary = "MCP job cancelled";
    active_job_id_.clear();
    return false;
  }

  const int budget =
      ops_budget > 0 ? ops_budget : std::max(1, job->ops_per_frame);
  bool ran = false;
  for (int i = 0; i < budget && job->next_index < job->ops.size(); ++i) {
    if (job->cancel_requested) {
      job->status = McpJobStatus::Cancelled;
      job->summary = "MCP job cancelled";
      active_job_id_.clear();
      return ran;
    }

    const auto &op = job->ops[job->next_index];
    const auto operation = operation_for_target(op.target);
    job->metadata["currentTarget"] = op.target;
    job->metadata["currentAction"] = op.params.value("action", std::string{});
    job->metadata["opIndex"] = std::to_string(job->next_index);

    auto step =
        execute_editor_operation(context, operation, op.params.dump());
    ran = true;
    for (const auto &id : step.changed_object_ids)
      job->changed_object_ids.push_back(id);
    for (const auto &diag : step.diagnostics)
      job->diagnostics.push_back(diag);
    for (const auto &[k, v] : step.metadata)
      job->metadata["last." + k] = v;

    if (step.exit_code != ExitCode::Success) {
      job->status = McpJobStatus::Failed;
      job->summary = "MCP job failed at op " + std::to_string(job->next_index) +
                     ": " + step.summary;
      job->metadata["failedOpIndex"] = std::to_string(job->next_index);
      job->metadata["failedSummary"] = step.summary;
      active_job_id_.clear();
      return true;
    }

    ++job->next_index;
    job->summary = "MCP job running (" + std::to_string(job->next_index) +
                   "/" + std::to_string(job->ops.size()) + ")";
  }

  if (job->next_index >= job->ops.size()) {
    job->status = McpJobStatus::Succeeded;
    job->summary = "MCP job succeeded";
    active_job_id_.clear();
  }
  return ran;
}

} // namespace engine
