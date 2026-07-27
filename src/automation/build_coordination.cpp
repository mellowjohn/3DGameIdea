#include "engine/automation/build_coordination.h"

#include "engine/automation/planning_backlog.h"
#include "engine/core/error.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace engine {
namespace {

constexpr int k_schema_version = 1;
constexpr std::int64_t k_default_lease_seconds = 600;
constexpr std::int64_t k_min_lease_seconds = 5;
constexpr std::int64_t k_max_lease_seconds = 3600;
constexpr std::int64_t k_default_wait_timeout_seconds = 60;
constexpr std::int64_t k_max_wait_timeout_seconds = 1800;
constexpr std::int64_t k_queue_entry_max_age_ms = 30 * 60 * 1000;
constexpr std::int64_t k_retry_after_ms = 2000;
constexpr std::size_t k_max_events = 20;

EngineError coord_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "build_coordination", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EditorBridgeResponse make_response(ExitCode exit_code, std::string summary,
    std::vector<EngineError> diagnostics = {}, std::map<std::string, std::string> metadata = {}) {
    EditorBridgeResponse response;
    response.exit_code = exit_code;
    response.summary = std::move(summary);
    response.diagnostics = std::move(diagnostics);
    response.metadata = std::move(metadata);
    return response;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string upper_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string trim_copy(std::string value) {
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
        value.pop_back();
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) ++start;
    return value.substr(start);
}

[[nodiscard]] std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();
}

[[nodiscard]] bool pid_alive(std::uint64_t pid) {
    if (pid == 0) return false;
#if defined(_WIN32)
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr) return GetLastError() == ERROR_ACCESS_DENIED;
    DWORD exit_code = 0;
    const bool alive = GetExitCodeProcess(handle, &exit_code) && exit_code == STILL_ACTIVE;
    CloseHandle(handle);
    return alive;
#else
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

struct CoordinationLease {
    std::string token;
    std::string agent_id;
    std::string ticket_id;
    std::string summary;
    std::string command;
    std::uint64_t pid = 0;
    std::int64_t acquired_at_ms = 0;
    std::int64_t heartbeat_at_ms = 0;
    std::int64_t expires_at_ms = 0;
    std::int64_t lease_seconds = k_default_lease_seconds;
};

struct CoordinationQueueEntry {
    std::string agent_id;
    std::string ticket_id;
    std::string summary;
    std::uint64_t pid = 0;
    std::int64_t requested_at_ms = 0;
};

struct CoordinationEvent {
    std::string kind;
    std::string detail;
    std::int64_t at_ms = 0;
};

struct CoordinationState {
    std::optional<CoordinationLease> lease;
    std::vector<CoordinationQueueEntry> queue;
    std::vector<CoordinationEvent> events;
    /// Set when a malformed state file was replaced during load (surfaced as an event on save).
    bool recovered_from_corrupt = false;
};

void push_event(CoordinationState& state, std::string kind, std::string detail, std::int64_t at) {
    state.events.push_back({std::move(kind), std::move(detail), at});
    if (state.events.size() > k_max_events)
        state.events.erase(state.events.begin(), state.events.end() - static_cast<std::ptrdiff_t>(k_max_events));
}

nlohmann::json lease_to_json(const CoordinationLease& lease) {
    return nlohmann::json{{"token", lease.token}, {"agentId", lease.agent_id}, {"ticketId", lease.ticket_id},
        {"summary", lease.summary}, {"command", lease.command}, {"pid", lease.pid},
        {"acquiredAtMs", lease.acquired_at_ms}, {"heartbeatAtMs", lease.heartbeat_at_ms},
        {"expiresAtMs", lease.expires_at_ms}, {"leaseSeconds", lease.lease_seconds}};
}

nlohmann::json state_to_json(const CoordinationState& state) {
    nlohmann::json doc{{"schemaVersion", k_schema_version}};
    doc["lease"] = state.lease ? lease_to_json(*state.lease) : nlohmann::json(nullptr);
    doc["queue"] = nlohmann::json::array();
    for (const auto& entry : state.queue) {
        doc["queue"].push_back(nlohmann::json{{"agentId", entry.agent_id}, {"ticketId", entry.ticket_id},
            {"summary", entry.summary}, {"pid", entry.pid}, {"requestedAtMs", entry.requested_at_ms}});
    }
    doc["events"] = nlohmann::json::array();
    for (const auto& event : state.events)
        doc["events"].push_back(nlohmann::json{{"kind", event.kind}, {"detail", event.detail}, {"atMs", event.at_ms}});
    return doc;
}

CoordinationState state_from_json(const nlohmann::json& doc) {
    CoordinationState state;
    if (doc.contains("lease") && doc["lease"].is_object()) {
        const auto& lease = doc["lease"];
        CoordinationLease parsed;
        parsed.token = lease.value("token", std::string{});
        parsed.agent_id = lease.value("agentId", std::string{});
        parsed.ticket_id = lease.value("ticketId", std::string{});
        parsed.summary = lease.value("summary", std::string{});
        parsed.command = lease.value("command", std::string{});
        parsed.pid = lease.value("pid", std::uint64_t{0});
        parsed.acquired_at_ms = lease.value("acquiredAtMs", std::int64_t{0});
        parsed.heartbeat_at_ms = lease.value("heartbeatAtMs", std::int64_t{0});
        parsed.expires_at_ms = lease.value("expiresAtMs", std::int64_t{0});
        parsed.lease_seconds = lease.value("leaseSeconds", k_default_lease_seconds);
        if (!parsed.token.empty() && !parsed.agent_id.empty()) state.lease = std::move(parsed);
    }
    if (doc.contains("queue") && doc["queue"].is_array()) {
        for (const auto& entry : doc["queue"]) {
            if (!entry.is_object()) continue;
            CoordinationQueueEntry parsed;
            parsed.agent_id = entry.value("agentId", std::string{});
            parsed.ticket_id = entry.value("ticketId", std::string{});
            parsed.summary = entry.value("summary", std::string{});
            parsed.pid = entry.value("pid", std::uint64_t{0});
            parsed.requested_at_ms = entry.value("requestedAtMs", std::int64_t{0});
            if (!parsed.agent_id.empty()) state.queue.push_back(std::move(parsed));
        }
    }
    if (doc.contains("events") && doc["events"].is_array()) {
        for (const auto& event : doc["events"]) {
            if (!event.is_object()) continue;
            state.events.push_back({event.value("kind", std::string{}), event.value("detail", std::string{}),
                event.value("atMs", std::int64_t{0})});
        }
    }
    return state;
}

CoordinationState load_state(const std::filesystem::path& state_path) {
    CoordinationState state;
    std::ifstream input(state_path, std::ios::binary);
    if (!input) return state;
    std::string text((std::istreambuf_iterator<char>(input)), {});
    if (trim_copy(text).empty()) return state;
    const auto doc = nlohmann::json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        state.recovered_from_corrupt = true;
        return state;
    }
    state = state_from_json(doc);
    return state;
}

[[nodiscard]] bool save_state(const std::filesystem::path& state_path, const CoordinationState& state,
    std::string* error_out) {
    try {
        std::filesystem::create_directories(state_path.parent_path());
        const auto tmp = state_path.string() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                if (error_out) *error_out = "Could not open coordinator temp file for writing";
                return false;
            }
            out << state_to_json(state).dump(2) << '\n';
        }
        std::filesystem::rename(tmp, state_path);
        return true;
    } catch (const std::exception& ex) {
        if (error_out) *error_out = ex.what();
        return false;
    }
}

/// Serializes read-modify-write across engine processes on this machine.
class CoordinationGuard final {
public:
    explicit CoordinationGuard(const std::filesystem::path& root) {
#if defined(_WIN32)
        std::uint64_t hash = 1469598103934665603ull; // FNV-1a over the canonical root
        for (const char c : lower_copy(root.lexically_normal().generic_string()))
            hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(c))) * 1099511628211ull;
        std::wostringstream name;
        name << L"Local\\engine-build-coordinator-" << std::hex << hash;
        mutex_ = CreateMutexW(nullptr, FALSE, name.str().c_str());
        if (mutex_ != nullptr) {
            const DWORD wait = WaitForSingleObject(mutex_, 5000);
            locked_ = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        }
#else
        lock_path_ = root / ".engine" / "build-coordinator.lock";
        std::error_code ec;
        std::filesystem::create_directories(lock_path_.parent_path(), ec);
        for (int attempt = 0; attempt < 100 && !locked_; ++attempt) {
            std::ofstream probe(lock_path_, std::ios::binary | std::ios::app);
            // Best-effort advisory lock on non-Windows dev shells; Windows is the shipping target.
            locked_ = static_cast<bool>(probe);
            if (!locked_) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
#endif
    }

    ~CoordinationGuard() {
#if defined(_WIN32)
        if (mutex_ != nullptr) {
            if (locked_) ReleaseMutex(mutex_);
            CloseHandle(mutex_);
        }
#endif
    }

    CoordinationGuard(const CoordinationGuard&) = delete;
    CoordinationGuard& operator=(const CoordinationGuard&) = delete;

    [[nodiscard]] bool locked() const noexcept { return locked_; }

private:
#if defined(_WIN32)
    HANDLE mutex_ = nullptr;
#else
    std::filesystem::path lock_path_;
#endif
    bool locked_ = false;
};

/// Drops expired/dead leases and dead or ancient queue entries. Returns true when state changed.
bool reclaim_stale(CoordinationState& state, std::int64_t now) {
    bool changed = false;
    if (state.lease) {
        std::string reason;
        if (state.lease->expires_at_ms > 0 && now >= state.lease->expires_at_ms) reason = "lease expired";
        // pid binding is opt-in: CLI/MCP callers are transient (the rebuild loop even kills the
        // MCP engine process while its lease is held), so 0 means "expiry-only reclaim".
        else if (state.lease->pid != 0 && !pid_alive(state.lease->pid)) reason = "owner process exited";
        if (!reason.empty()) {
            push_event(state, "lease_reclaimed",
                state.lease->agent_id + " (" + state.lease->ticket_id + "): " + reason, now);
            state.lease.reset();
            changed = true;
        }
    }
    auto removed = std::remove_if(state.queue.begin(), state.queue.end(), [&](const CoordinationQueueEntry& entry) {
        const bool dead = entry.pid != 0 && !pid_alive(entry.pid);
        const bool ancient = entry.requested_at_ms > 0 && now - entry.requested_at_ms > k_queue_entry_max_age_ms;
        if (dead || ancient) {
            push_event(state, "queue_entry_dropped",
                entry.agent_id + " (" + entry.ticket_id + "): " + (dead ? "process exited" : "request aged out"),
                now);
            return true;
        }
        return false;
    });
    if (removed != state.queue.end()) {
        state.queue.erase(removed, state.queue.end());
        changed = true;
    }
    return changed;
}

void fill_state_metadata(std::map<std::string, std::string>& metadata, const CoordinationState& state,
    std::int64_t now) {
    if (state.lease) {
        metadata["leaseAgentId"] = state.lease->agent_id;
        metadata["leaseTicketId"] = state.lease->ticket_id;
        metadata["leaseSummary"] = state.lease->summary;
        metadata["leaseCommand"] = state.lease->command;
        metadata["leasePid"] = std::to_string(state.lease->pid);
        metadata["leasePidAlive"] =
            state.lease->pid == 0 ? "unbound" : (pid_alive(state.lease->pid) ? "true" : "false");
        metadata["leaseAcquiredAtMs"] = std::to_string(state.lease->acquired_at_ms);
        metadata["leaseExpiresAtMs"] = std::to_string(state.lease->expires_at_ms);
        metadata["leaseExpiresInMs"] =
            std::to_string(std::max<std::int64_t>(0, state.lease->expires_at_ms - now));
    }
    metadata["queueLength"] = std::to_string(state.queue.size());
    nlohmann::json queue = nlohmann::json::array();
    for (const auto& entry : state.queue) {
        queue.push_back(nlohmann::json{{"agentId", entry.agent_id}, {"ticketId", entry.ticket_id},
            {"summary", entry.summary}, {"pid", entry.pid}, {"requestedAtMs", entry.requested_at_ms}});
    }
    metadata["queueJson"] = queue.dump();
    nlohmann::json events = nlohmann::json::array();
    for (const auto& event : state.events)
        events.push_back(nlohmann::json{{"kind", event.kind}, {"detail", event.detail}, {"atMs", event.at_ms}});
    metadata["eventsJson"] = events.dump();
}

struct AcquireParams {
    std::string agent_id;
    std::string ticket_id;
    std::string summary;
    std::string command;
    std::uint64_t pid = 0;
    std::int64_t lease_seconds = k_default_lease_seconds;
};

Result<AcquireParams> parse_acquire_params(const nlohmann::json& params) {
    AcquireParams parsed;
    parsed.agent_id = trim_copy(params.value("agentId", std::string{}));
    parsed.ticket_id = upper_copy(trim_copy(params.value("ticketId", std::string{})));
    parsed.summary = trim_copy(params.value("summary", std::string{}));
    parsed.command = trim_copy(params.value("command", std::string{}));
    // 0 (default) keeps the lease valid across transient CLI/MCP processes; pass a pid to
    // bind the lease to a long-lived agent process for dead-owner reclaim.
    parsed.pid = params.value("pid", std::uint64_t{0});
    if (parsed.agent_id.empty() || parsed.ticket_id.empty() || parsed.summary.empty()) {
        return Result<AcquireParams>::failure(coord_error("BUILD-COORD-PARAMS-REQUIRED",
            ErrorCategory::Validation, "acquire/wait require agentId, ticketId, and summary.",
            "Pass agentId (who you are), ticketId (TICKET-#### from epics.md), and a short work summary."));
    }
    std::int64_t lease_seconds = params.value("leaseSeconds", k_default_lease_seconds);
    parsed.lease_seconds = std::clamp(lease_seconds, k_min_lease_seconds, k_max_lease_seconds);
    return Result<AcquireParams>::success(std::move(parsed));
}

Result<void> validate_ticket_id(const std::filesystem::path& coordination_root, const std::string& ticket_id) {
    const auto epics_path = coordination_root / "context" / "planning" / "epics.md";
    auto backlog = load_planning_backlog(epics_path);
    if (!backlog) {
        return Result<void>::failure(coord_error("BUILD-COORD-BACKLOG-MISSING", ErrorCategory::Configuration,
            "Cannot validate ticketId — planning backlog unavailable: " + backlog.error().message,
            "Run from a checkout that contains context/planning/epics.md."));
    }
    if (!backlog.value().find(ticket_id)) {
        return Result<void>::failure(coord_error("BUILD-COORD-TICKET-UNKNOWN", ErrorCategory::Validation,
            "Unknown ticketId: " + ticket_id + " (not present in " + epics_path.generic_string() + ").",
            "Claim an existing TICKET-#### from context/planning/epics.md; create the ticket row first if new."));
    }
    return Result<void>::success();
}

/// One serialized acquire attempt. Grants only when no lease is active and the caller is first in line.
EditorBridgeResponse try_acquire(const std::filesystem::path& state_path, const AcquireParams& request) {
    CoordinationState state = load_state(state_path);
    const auto now = now_ms();
    if (state.recovered_from_corrupt)
        push_event(state, "state_recovered", "Malformed coordinator state was reset", now);
    reclaim_stale(state, now);

    std::map<std::string, std::string> metadata;
    std::string save_error;

    // Holder re-acquire is idempotent and refreshes the expiry window.
    if (state.lease && state.lease->agent_id == request.agent_id) {
        state.lease->heartbeat_at_ms = now;
        state.lease->lease_seconds = request.lease_seconds;
        state.lease->expires_at_ms = now + request.lease_seconds * 1000;
        if (!state.lease->summary.empty() && request.summary != state.lease->summary)
            state.lease->summary = request.summary;
        if (!save_state(state_path, state, &save_error)) {
            return make_response(ExitCode::InternalError, "Failed to persist coordinator state",
                {coord_error("BUILD-COORD-SAVE-FAILED", ErrorCategory::Io, save_error,
                    "Check .engine directory permissions.")});
        }
        fill_state_metadata(metadata, state, now);
        metadata["state"] = "granted";
        metadata["leaseToken"] = state.lease->token;
        return make_response(ExitCode::Success,
            "Build lease refreshed for " + request.agent_id + " (" + request.ticket_id + ")", {},
            std::move(metadata));
    }

    const auto queue_position_of = [&](const std::string& agent_id) -> std::size_t {
        for (std::size_t i = 0; i < state.queue.size(); ++i) {
            if (state.queue[i].agent_id == agent_id) return i;
        }
        return state.queue.size();
    };

    const bool queued_already = queue_position_of(request.agent_id) < state.queue.size();
    const bool first_in_line =
        state.queue.empty() || state.queue.front().agent_id == request.agent_id;

    if (!state.lease && first_in_line) {
        if (!state.queue.empty()) state.queue.erase(state.queue.begin());
        CoordinationLease lease;
        lease.token = make_correlation_id();
        lease.agent_id = request.agent_id;
        lease.ticket_id = request.ticket_id;
        lease.summary = request.summary;
        lease.command = request.command;
        lease.pid = request.pid;
        lease.acquired_at_ms = now;
        lease.heartbeat_at_ms = now;
        lease.lease_seconds = request.lease_seconds;
        lease.expires_at_ms = now + request.lease_seconds * 1000;
        push_event(state, "lease_granted", request.agent_id + " (" + request.ticket_id + ")", now);
        state.lease = std::move(lease);
        if (!save_state(state_path, state, &save_error)) {
            return make_response(ExitCode::InternalError, "Failed to persist coordinator state",
                {coord_error("BUILD-COORD-SAVE-FAILED", ErrorCategory::Io, save_error,
                    "Check .engine directory permissions.")});
        }
        fill_state_metadata(metadata, state, now);
        metadata["state"] = "granted";
        metadata["leaseToken"] = state.lease->token;
        return make_response(ExitCode::Success,
            "Build lease granted to " + request.agent_id + " (" + request.ticket_id + ")", {},
            std::move(metadata));
    }

    if (!queued_already) {
        CoordinationQueueEntry entry;
        entry.agent_id = request.agent_id;
        entry.ticket_id = request.ticket_id;
        entry.summary = request.summary;
        entry.pid = request.pid;
        entry.requested_at_ms = now;
        state.queue.push_back(std::move(entry));
    }
    if (!save_state(state_path, state, &save_error)) {
        return make_response(ExitCode::InternalError, "Failed to persist coordinator state",
            {coord_error("BUILD-COORD-SAVE-FAILED", ErrorCategory::Io, save_error,
                "Check .engine directory permissions.")});
    }
    fill_state_metadata(metadata, state, now);
    const auto position = queue_position_of(request.agent_id) + 1;
    metadata["state"] = "queued";
    metadata["queuePosition"] = std::to_string(position);
    metadata["retryAfterMs"] = std::to_string(k_retry_after_ms);
    std::string holder = state.lease ? state.lease->agent_id + " (" + state.lease->ticket_id + ")"
                                     : std::string("queue front ") + state.queue.front().agent_id;
    return make_response(ExitCode::Unavailable,
        "Build lease busy — " + request.agent_id + " queued at position " + std::to_string(position) +
            " behind " + holder + ". Wait your turn; do not start MSBuild.",
        {}, std::move(metadata));
}

EditorBridgeResponse release_action(const std::filesystem::path& state_path, const nlohmann::json& params) {
    const auto token = trim_copy(params.value("token", std::string{}));
    if (token.empty()) {
        return make_response(ExitCode::InvalidArguments, "release requires token",
            {coord_error("BUILD-COORD-TOKEN-REQUIRED", ErrorCategory::Validation,
                "Missing lease token.", "Pass the token returned by acquire/wait.")});
    }
    CoordinationState state = load_state(state_path);
    const auto now = now_ms();
    reclaim_stale(state, now);
    if (!state.lease) {
        return make_response(ExitCode::ValidationFailed, "No active build lease to release",
            {coord_error("BUILD-COORD-NOT-HELD", ErrorCategory::Validation,
                "There is no active lease (it may have expired or been reclaimed).",
                "Acquire a fresh lease before the next rebuild.")});
    }
    if (state.lease->token != token) {
        return make_response(ExitCode::ValidationFailed, "Lease token mismatch",
            {coord_error("BUILD-COORD-TOKEN-MISMATCH", ErrorCategory::Validation,
                "The provided token does not match the active lease held by " + state.lease->agent_id + ".",
                "Only the current holder may release; use clear-stale for stuck leases.")});
    }
    push_event(state, "lease_released", state.lease->agent_id + " (" + state.lease->ticket_id + ")", now);
    const auto agent = state.lease->agent_id;
    state.lease.reset();
    std::string save_error;
    if (!save_state(state_path, state, &save_error)) {
        return make_response(ExitCode::InternalError, "Failed to persist coordinator state",
            {coord_error("BUILD-COORD-SAVE-FAILED", ErrorCategory::Io, save_error,
                "Check .engine directory permissions.")});
    }
    std::map<std::string, std::string> metadata;
    fill_state_metadata(metadata, state, now);
    metadata["state"] = "released";
    return make_response(ExitCode::Success, "Build lease released by " + agent, {}, std::move(metadata));
}

EditorBridgeResponse heartbeat_action(const std::filesystem::path& state_path, const nlohmann::json& params) {
    const auto token = trim_copy(params.value("token", std::string{}));
    if (token.empty()) {
        return make_response(ExitCode::InvalidArguments, "heartbeat requires token",
            {coord_error("BUILD-COORD-TOKEN-REQUIRED", ErrorCategory::Validation,
                "Missing lease token.", "Pass the token returned by acquire/wait.")});
    }
    CoordinationState state = load_state(state_path);
    const auto now = now_ms();
    reclaim_stale(state, now);
    if (!state.lease || state.lease->token != token) {
        return make_response(ExitCode::ValidationFailed, "Heartbeat rejected — not the active lease",
            {coord_error("BUILD-COORD-TOKEN-MISMATCH", ErrorCategory::Validation,
                "The provided token does not match an active lease.",
                "Re-acquire the lease; it may have expired.")});
    }
    state.lease->heartbeat_at_ms = now;
    state.lease->expires_at_ms = now + state.lease->lease_seconds * 1000;
    std::string save_error;
    if (!save_state(state_path, state, &save_error)) {
        return make_response(ExitCode::InternalError, "Failed to persist coordinator state",
            {coord_error("BUILD-COORD-SAVE-FAILED", ErrorCategory::Io, save_error,
                "Check .engine directory permissions.")});
    }
    std::map<std::string, std::string> metadata;
    fill_state_metadata(metadata, state, now);
    metadata["state"] = "granted";
    metadata["leaseToken"] = state.lease->token;
    return make_response(ExitCode::Success, "Build lease extended for " + state.lease->agent_id, {},
        std::move(metadata));
}

EditorBridgeResponse status_action(const std::filesystem::path& state_path) {
    CoordinationState state = load_state(state_path);
    const auto now = now_ms();
    bool changed = state.recovered_from_corrupt;
    if (state.recovered_from_corrupt)
        push_event(state, "state_recovered", "Malformed coordinator state was reset", now);
    changed = reclaim_stale(state, now) || changed;
    if (changed) {
        std::string save_error;
        (void)save_state(state_path, state, &save_error);
    }
    std::map<std::string, std::string> metadata;
    fill_state_metadata(metadata, state, now);
    metadata["state"] = state.lease ? "held" : "idle";
    std::string summary;
    if (state.lease) {
        summary = "Build lease held by " + state.lease->agent_id + " (" + state.lease->ticket_id + ")";
        if (!state.queue.empty()) summary += " — " + std::to_string(state.queue.size()) + " waiting";
    } else if (!state.queue.empty()) {
        summary = "Build lease idle — " + std::to_string(state.queue.size()) + " queued (front: " +
            state.queue.front().agent_id + ")";
    } else {
        summary = "Build lease idle — no agents waiting";
    }
    return make_response(ExitCode::Success, std::move(summary), {}, std::move(metadata));
}

EditorBridgeResponse clear_stale_action(const std::filesystem::path& state_path, const nlohmann::json& params) {
    CoordinationState state = load_state(state_path);
    const auto now = now_ms();
    if (state.recovered_from_corrupt)
        push_event(state, "state_recovered", "Malformed coordinator state was reset", now);
    bool cleared = reclaim_stale(state, now);
    const bool force = params.value("force", false);
    if (force && state.lease) {
        const auto by = trim_copy(params.value("agentId", std::string{"owner"}));
        push_event(state, "lease_force_cleared",
            state.lease->agent_id + " (" + state.lease->ticket_id + ") cleared by " + by, now);
        state.lease.reset();
        cleared = true;
    }
    std::string save_error;
    if (!save_state(state_path, state, &save_error)) {
        return make_response(ExitCode::InternalError, "Failed to persist coordinator state",
            {coord_error("BUILD-COORD-SAVE-FAILED", ErrorCategory::Io, save_error,
                "Check .engine directory permissions.")});
    }
    std::map<std::string, std::string> metadata;
    fill_state_metadata(metadata, state, now);
    metadata["state"] = state.lease ? "held" : "idle";
    metadata["cleared"] = cleared ? "true" : "false";
    std::string summary = cleared ? "Stale coordination state cleared" : "Nothing stale to clear";
    if (state.lease && !force) summary += " — active lease kept (use force to clear a live lease)";
    return make_response(ExitCode::Success, std::move(summary), {}, std::move(metadata));
}

} // namespace

std::filesystem::path build_coordination_root(const std::filesystem::path& project_root) {
    if (const auto planning_root = find_planning_root(project_root)) return *planning_root;
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(project_root, ec);
    return ec ? std::filesystem::absolute(project_root) : canonical;
}

std::filesystem::path build_coordination_state_path(const std::filesystem::path& project_root) {
    return build_coordination_root(project_root) / ".engine" / "build-coordinator.json";
}

EditorBridgeResponse apply_build_coordination_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params) {
    const auto action = lower_copy(trim_copy(params.value("action", std::string{})));
    if (action.empty()) {
        return make_response(ExitCode::InvalidArguments, "action is required",
            {coord_error("BUILD-COORD-ACTION-REQUIRED", ErrorCategory::Validation, "Missing action.",
                "Use action=status|acquire|wait|release|heartbeat|clear-stale.")});
    }
    if (!std::filesystem::exists(project_root)) {
        return make_response(ExitCode::InvalidArguments, "project root does not exist",
            {coord_error("BUILD-COORD-PROJECT-MISSING", ErrorCategory::Validation,
                "Project root does not exist: " + project_root.generic_string(),
                "Pass a valid --project directory.")});
    }

    const auto root = build_coordination_root(project_root);
    const auto state_path = root / ".engine" / "build-coordinator.json";

    const auto respond_with_root = [&](EditorBridgeResponse response) {
        response.metadata["coordinationRoot"] = root.generic_string();
        response.metadata["statePath"] = state_path.generic_string();
        return response;
    };

    if (action == "status" || action == "clear-stale" || action == "clear_stale") {
        CoordinationGuard guard(root);
        if (!guard.locked()) {
            return make_response(ExitCode::Unavailable, "Coordination mutex busy",
                {coord_error("BUILD-COORD-LOCK-TIMEOUT", ErrorCategory::Io,
                    "Timed out waiting for the coordination mutex.", "Retry shortly.")});
        }
        return respond_with_root(action == "status" ? status_action(state_path)
                                                    : clear_stale_action(state_path, params));
    }

    if (action == "release" || action == "heartbeat") {
        CoordinationGuard guard(root);
        if (!guard.locked()) {
            return make_response(ExitCode::Unavailable, "Coordination mutex busy",
                {coord_error("BUILD-COORD-LOCK-TIMEOUT", ErrorCategory::Io,
                    "Timed out waiting for the coordination mutex.", "Retry shortly.")});
        }
        return respond_with_root(action == "release" ? release_action(state_path, params)
                                                     : heartbeat_action(state_path, params));
    }

    if (action == "acquire" || action == "wait") {
        auto parsed = parse_acquire_params(params);
        if (!parsed) return make_response(ExitCode::InvalidArguments, parsed.error().message, {parsed.error()});
        const auto ticket_valid = validate_ticket_id(root, parsed.value().ticket_id);
        if (!ticket_valid) {
            const auto code =
                ticket_valid.error().code == "BUILD-COORD-BACKLOG-MISSING" ? ExitCode::ConfigurationError
                                                                           : ExitCode::ValidationFailed;
            return make_response(code, ticket_valid.error().message, {ticket_valid.error()});
        }

        const auto attempt = [&]() {
            CoordinationGuard guard(root);
            if (!guard.locked()) {
                return make_response(ExitCode::Unavailable, "Coordination mutex busy",
                    {coord_error("BUILD-COORD-LOCK-TIMEOUT", ErrorCategory::Io,
                        "Timed out waiting for the coordination mutex.", "Retry shortly.")});
            }
            return try_acquire(state_path, parsed.value());
        };

        if (action == "acquire") return respond_with_root(attempt());

        std::int64_t timeout_seconds = params.value("timeoutSeconds", k_default_wait_timeout_seconds);
        timeout_seconds = std::clamp<std::int64_t>(timeout_seconds, 1, k_max_wait_timeout_seconds);
        const auto deadline = now_ms() + timeout_seconds * 1000;
        EditorBridgeResponse last = attempt();
        while (last.exit_code == ExitCode::Unavailable && now_ms() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            last = attempt();
        }
        if (last.exit_code == ExitCode::Unavailable) {
            last.summary = "Timed out after " + std::to_string(timeout_seconds) +
                "s waiting for the build lease — still queued. Retry wait; do not start MSBuild.";
            last.diagnostics.push_back(coord_error("BUILD-COORD-WAIT-TIMEOUT", ErrorCategory::Validation,
                "The build lease was not granted within the wait timeout.",
                "Retry `wait` (your queue position is preserved) or check status for a stuck holder."));
        }
        return respond_with_root(std::move(last));
    }

    return make_response(ExitCode::InvalidArguments, "Unsupported build coordination action: " + action,
        {coord_error("BUILD-COORD-ACTION-UNKNOWN", ErrorCategory::Validation, "Unsupported action: " + action,
            "Use status|acquire|wait|release|heartbeat|clear-stale.")});
}

} // namespace engine
