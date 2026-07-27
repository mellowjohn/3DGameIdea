#include "engine/assets/world_forge_events_asset.h"
#include "engine/assets/world_forge_acts.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError evt_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_events", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeEventCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key.empty() || key == "draft")
        return Result<WorldForgeEventCanonStatus>::success(WorldForgeEventCanonStatus::Draft);
    if (key == "established")
        return Result<WorldForgeEventCanonStatus>::success(WorldForgeEventCanonStatus::Established);
    if (key == "proposal")
        return Result<WorldForgeEventCanonStatus>::success(WorldForgeEventCanonStatus::Proposal);
    if (key == "open") return Result<WorldForgeEventCanonStatus>::success(WorldForgeEventCanonStatus::Open);
    return Result<WorldForgeEventCanonStatus>::failure(evt_error("EVENT-CANON", ErrorCategory::Validation,
        "Unsupported canonStatus: " + raw, "Use established, draft, proposal, or open."));
}

Result<EventTimelineStepKind> parse_step_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "wait") return Result<EventTimelineStepKind>::success(EventTimelineStepKind::Wait);
    if (key == "lock_control" || key == "lockcontrol")
        return Result<EventTimelineStepKind>::success(EventTimelineStepKind::LockControl);
    if (key == "unlock_control" || key == "unlockcontrol")
        return Result<EventTimelineStepKind>::success(EventTimelineStepKind::UnlockControl);
    if (key == "start_dialogue" || key == "startdialogue")
        return Result<EventTimelineStepKind>::success(EventTimelineStepKind::StartDialogue);
    if (key == "emit") return Result<EventTimelineStepKind>::success(EventTimelineStepKind::Emit);
    if (key == "look_at" || key == "lookat")
        return Result<EventTimelineStepKind>::success(EventTimelineStepKind::LookAt);
    return Result<EventTimelineStepKind>::failure(evt_error("EVENT-STEP-KIND", ErrorCategory::Validation,
        "Unknown event timeline step kind: " + raw,
        "Use wait, lock_control, unlock_control, start_dialogue, emit, or look_at."));
}

std::vector<std::string> read_string_array(const nlohmann::json& node) {
    std::vector<std::string> out;
    if (!node.is_array()) return out;
    for (const auto& entry : node) {
        if (entry.is_string()) out.push_back(entry.get<std::string>());
    }
    return out;
}

nlohmann::json write_string_array(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

Result<EventTimelineStep> read_step(const nlohmann::json& node) {
    if (!node.is_object()) {
        return Result<EventTimelineStep>::failure(evt_error("EVENT-STEP", ErrorCategory::Validation,
            "Each timeline step must be an object", "Fix step entries."));
    }
    const auto kind = parse_step_kind(node.value("kind", std::string{}));
    if (!kind) return Result<EventTimelineStep>::failure(kind.error());
    EventTimelineStep step;
    step.kind = kind.value();
    switch (step.kind) {
    case EventTimelineStepKind::Wait:
        step.seconds = node.value("seconds", 0.0f);
        if (!(step.seconds >= 0.0f)) {
            return Result<EventTimelineStep>::failure(evt_error("EVENT-STEP-WAIT", ErrorCategory::Validation,
                "wait.seconds must be >= 0", "Set seconds to a non-negative number."));
        }
        break;
    case EventTimelineStepKind::LockControl:
    case EventTimelineStepKind::UnlockControl:
        break;
    case EventTimelineStepKind::StartDialogue:
        step.dialogue_id = node.value("dialogueId", node.value("dialogue_id", std::string{}));
        if (step.dialogue_id.empty()) {
            return Result<EventTimelineStep>::failure(evt_error("EVENT-STEP-DIALOGUE", ErrorCategory::Validation,
                "start_dialogue requires dialogueId", "Set dialogueId to a dialogues tree id."));
        }
        break;
    case EventTimelineStepKind::Emit:
        step.emit_name = node.value("name", std::string{});
        if (step.emit_name.empty()) {
            return Result<EventTimelineStep>::failure(evt_error("EVENT-STEP-EMIT", ErrorCategory::Validation,
                "emit requires name", "Set a non-empty emit name."));
        }
        if (node.contains("payload")) {
            const auto& payload = node.at("payload");
            if (!payload.is_object()) {
                return Result<EventTimelineStep>::failure(evt_error("EVENT-STEP-PAYLOAD", ErrorCategory::Validation,
                    "emit.payload must be a JSON object", "Use {} or an object."));
            }
            step.payload_json = payload.dump();
        } else {
            step.payload_json = "{}";
        }
        break;
    case EventTimelineStepKind::LookAt: {
        step.seconds = node.value("seconds", 0.0f);
        if (!(step.seconds >= 0.0f)) {
            return Result<EventTimelineStep>::failure(evt_error("EVENT-CAM-DURATION", ErrorCategory::Validation,
                "look_at.seconds must be >= 0", "Set seconds to a non-negative number."));
        }
        const auto target = node.contains("target") ? node.at("target") : node.value("lookAt", nlohmann::json{});
        if (!target.is_array() || target.size() != 3 || !target[0].is_number() || !target[1].is_number()
            || !target[2].is_number()) {
            return Result<EventTimelineStep>::failure(evt_error("EVENT-CAM-TARGET", ErrorCategory::Validation,
                "look_at requires target [x,y,z]", "Provide a numeric target array."));
        }
        step.look_at_target = {target[0].get<float>(), target[1].get<float>(), target[2].get<float>()};
        step.look_at_distance = node.value("distance", 0.0f);
        if (node.contains("pitch") && node.at("pitch").is_number()) {
            step.look_at_pitch = node.at("pitch").get<float>();
            step.look_at_has_pitch = true;
        }
        break;
    }
    }
    return Result<EventTimelineStep>::success(std::move(step));
}

nlohmann::ordered_json write_step(const EventTimelineStep& step) {
    nlohmann::ordered_json json;
    json["kind"] = to_string(step.kind);
    switch (step.kind) {
    case EventTimelineStepKind::Wait:
        json["seconds"] = step.seconds;
        break;
    case EventTimelineStepKind::LockControl:
    case EventTimelineStepKind::UnlockControl:
        break;
    case EventTimelineStepKind::StartDialogue:
        json["dialogueId"] = step.dialogue_id;
        break;
    case EventTimelineStepKind::Emit: {
        json["name"] = step.emit_name;
        try {
            json["payload"] = nlohmann::json::parse(step.payload_json.empty() ? "{}" : step.payload_json);
        } catch (...) {
            json["payload"] = nlohmann::json::object();
        }
        break;
    }
    case EventTimelineStepKind::LookAt:
        json["seconds"] = step.seconds;
        json["target"] = nlohmann::ordered_json::array(
            {step.look_at_target[0], step.look_at_target[1], step.look_at_target[2]});
        if (step.look_at_distance > 0.0f) json["distance"] = step.look_at_distance;
        if (step.look_at_has_pitch) json["pitch"] = step.look_at_pitch;
        break;
    }
    return json;
}

Result<void> validate_sequence(const EventTimelineSequence& sequence) {
    if (sequence.id.empty()) {
        return Result<void>::failure(evt_error("EVENT-ID", ErrorCategory::Validation,
            "Event sequence id is required", "Set a unique non-empty id for each sequence."));
    }
    for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
        const auto& step = sequence.steps[i];
        switch (step.kind) {
        case EventTimelineStepKind::Wait:
            if (!(step.seconds >= 0.0f)) {
                return Result<void>::failure(evt_error("EVENT-STEP-WAIT", ErrorCategory::Validation,
                    "wait.seconds must be >= 0 on sequence '" + sequence.id + "' step " + std::to_string(i),
                    "Set seconds to a non-negative number."));
            }
            break;
        case EventTimelineStepKind::StartDialogue:
            if (step.dialogue_id.empty()) {
                return Result<void>::failure(evt_error("EVENT-STEP-DIALOGUE", ErrorCategory::Validation,
                    "start_dialogue requires dialogueId on sequence '" + sequence.id + "'",
                    "Set dialogueId to a dialogues tree id."));
            }
            break;
        case EventTimelineStepKind::Emit:
            if (step.emit_name.empty()) {
                return Result<void>::failure(evt_error("EVENT-STEP-EMIT", ErrorCategory::Validation,
                    "emit requires name on sequence '" + sequence.id + "'", "Set a non-empty emit name."));
            }
            break;
        case EventTimelineStepKind::LookAt:
            if (!(step.seconds >= 0.0f)) {
                return Result<void>::failure(evt_error("EVENT-CAM-DURATION", ErrorCategory::Validation,
                    "look_at.seconds must be >= 0 on sequence '" + sequence.id + "'",
                    "Set seconds to a non-negative number."));
            }
            break;
        case EventTimelineStepKind::LockControl:
        case EventTimelineStepKind::UnlockControl:
            break;
        }
    }
    return Result<void>::success();
}

} // namespace

const char* to_string(WorldForgeEventCanonStatus value) noexcept {
    switch (value) {
    case WorldForgeEventCanonStatus::Established: return "established";
    case WorldForgeEventCanonStatus::Draft: return "draft";
    case WorldForgeEventCanonStatus::Proposal: return "proposal";
    case WorldForgeEventCanonStatus::Open: return "open";
    }
    return "draft";
}

const char* to_string(EventTimelineStepKind value) noexcept {
    switch (value) {
    case EventTimelineStepKind::Wait: return "wait";
    case EventTimelineStepKind::LockControl: return "lock_control";
    case EventTimelineStepKind::UnlockControl: return "unlock_control";
    case EventTimelineStepKind::StartDialogue: return "start_dialogue";
    case EventTimelineStepKind::Emit: return "emit";
    case EventTimelineStepKind::LookAt: return "look_at";
    }
    return "wait";
}

std::filesystem::path default_world_forge_events_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "events.worldforge.json";
}

Result<void> WorldForgeEventsAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(evt_error("EVENT-SCHEMA", ErrorCategory::Validation,
            "Only World Forge events schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> sequence_ids;
    sequence_ids.reserve(sequences.size());
    for (const auto& sequence : sequences) {
        if (sequence.id.empty()) {
            return Result<void>::failure(evt_error("EVENT-ID", ErrorCategory::Validation,
                "Event sequence id is required", "Set a unique non-empty id for each sequence."));
        }
        if (!sequence_ids.insert(sequence.id).second) {
            return Result<void>::failure(evt_error("EVENT-ID-DUP", ErrorCategory::Validation,
                "Duplicate event sequence id: " + sequence.id, "Ensure every sequence id is unique."));
        }
        if (const auto valid = validate_sequence(sequence); !valid) return Result<void>::failure(valid.error());
        if (const auto acts_ok = validate_world_forge_acts(sequence.acts, "event sequence", sequence.id); !acts_ok) {
            return Result<void>::failure(acts_ok.error());
        }
    }
    return Result<void>::success();
}

Result<void> WorldForgeEventsAsset::validate_dialogue_refs(
    const std::unordered_set<std::string>& known_dialogue_ids) const {
    if (known_dialogue_ids.empty()) return Result<void>::success();
    for (const auto& sequence : sequences) {
        for (const auto& step : sequence.steps) {
            if (step.kind != EventTimelineStepKind::StartDialogue) continue;
            if (known_dialogue_ids.find(step.dialogue_id) == known_dialogue_ids.end()) {
                return Result<void>::failure(evt_error("EVENT-DIALOGUE-MISSING", ErrorCategory::Validation,
                    "Unknown dialogueId '" + step.dialogue_id + "' on sequence '" + sequence.id + "'",
                    "Point dialogueId at a dialogues tree id."));
            }
        }
    }
    return Result<void>::success();
}

const EventTimelineSequence* WorldForgeEventsAsset::find_sequence(const std::string& sequence_id) const {
    for (const auto& sequence : sequences) {
        if (sequence.id == sequence_id) return &sequence;
    }
    return nullptr;
}

EventTimelineSequence* WorldForgeEventsAsset::find_sequence(const std::string& sequence_id) {
    for (auto& sequence : sequences) {
        if (sequence.id == sequence_id) return &sequence;
    }
    return nullptr;
}

Result<WorldForgeEventsAsset> WorldForgeEventsAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-ROOT", ErrorCategory::Serialization,
                source_name + " must be a JSON object", "Wrap events in an object with schemaVersion and id."));
        }
        WorldForgeEventsAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-SCHEMA", ErrorCategory::Validation,
                "Unsupported World Forge events schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto sequences_json = json.value("sequences", nlohmann::json::array());
        if (!sequences_json.is_array()) {
            return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-SEQUENCES", ErrorCategory::Validation,
                "sequences must be an array", "Provide a sequences array."));
        }
        for (const auto& sequence_node : sequences_json) {
            if (!sequence_node.is_object()) {
                return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-SEQUENCE", ErrorCategory::Validation,
                    "Each event sequence must be an object", "Fix sequence entries."));
            }
            EventTimelineSequence sequence;
            sequence.id = sequence_node.value("id", std::string{});
            if (sequence.id.empty()) {
                return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-ID", ErrorCategory::Validation,
                    "Event sequence id is required", "Set a unique non-empty id for each sequence."));
            }
            sequence.display_name = sequence_node.value("displayName", std::string{});
            const auto canon = parse_canon_status(sequence_node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeEventsAsset>::failure(canon.error());
            sequence.canon_status = canon.value();
            sequence.summary = sequence_node.value("summary", std::string{});
            sequence.acts = read_string_array(sequence_node.value("acts", nlohmann::json::array()));
            sequence.tags = read_string_array(sequence_node.value("tags", nlohmann::json::array()));

            const auto steps_json = sequence_node.value("steps", nlohmann::json::array());
            if (!steps_json.is_array()) {
                return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-STEPS", ErrorCategory::Validation,
                    "steps must be an array on sequence '" + sequence.id + "'",
                    "Provide a steps array (may be empty)."));
            }
            for (const auto& step_json : steps_json) {
                auto step = read_step(step_json);
                if (!step) return Result<WorldForgeEventsAsset>::failure(step.error());
                sequence.steps.push_back(std::move(step.value()));
            }
            asset.sequences.push_back(std::move(sequence));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeEventsAsset>::failure(valid.error());
        }
        return Result<WorldForgeEventsAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = evt_error("EVENT-PARSE", ErrorCategory::Serialization, "Failed to parse " + source_name,
            "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeEventsAsset>::failure(std::move(error));
    }
}

Result<WorldForgeEventsAsset> WorldForgeEventsAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeEventsAsset>::failure(evt_error("EVENT-READ", ErrorCategory::Io,
            "Could not read World Forge events: " + path.generic_string(), "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeEventsAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto sequences_json = nlohmann::ordered_json::array();
    for (const auto& sequence : sequences) {
        nlohmann::ordered_json sequence_json;
        sequence_json["id"] = sequence.id;
        sequence_json["displayName"] = sequence.display_name;
        sequence_json["canonStatus"] = to_string(sequence.canon_status);
        sequence_json["summary"] = sequence.summary;
        auto steps_json = nlohmann::ordered_json::array();
        for (const auto& step : sequence.steps) steps_json.push_back(write_step(step));
        sequence_json["steps"] = std::move(steps_json);
        sequence_json["acts"] = write_string_array(sequence.acts);
        sequence_json["tags"] = write_string_array(sequence.tags);
        sequences_json.push_back(std::move(sequence_json));
    }
    json["sequences"] = std::move(sequences_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeEventsAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(evt_error("EVENT-IO", ErrorCategory::Io,
                "Could not write World Forge events: " + path.generic_string(),
                "Check file permissions and disk space."));
        }
        output << to_json();
        if (!output) {
            return Result<void>::failure(evt_error("EVENT-IO", ErrorCategory::Io,
                "Failed while writing World Forge events: " + path.generic_string(),
                "Check disk space and retry."));
        }
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::rename(path, backup, ec);
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        return Result<void>::failure(evt_error("EVENT-IO", ErrorCategory::Io,
            "Could not finalize World Forge events write: " + path.generic_string(),
            "Check file permissions; restore from .bak if needed."));
    }
    return Result<void>::success();
}

Result<void> WorldForgeEventsAsset::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return loaded.value().validate();
}

Result<void> WorldForgeEventsAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_dialogue_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    if (const auto valid = loaded.value().validate(); !valid) return Result<void>::failure(valid.error());
    return loaded.value().validate_dialogue_refs(known_dialogue_ids);
}

} // namespace engine
