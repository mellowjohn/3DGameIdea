#pragma once

#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeEventCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };

enum class EventTimelineStepKind : std::uint8_t {
    Wait,
    LockControl,
    UnlockControl,
    StartDialogue,
    Emit,
    LookAt,
};

struct EventTimelineStep {
    EventTimelineStepKind kind = EventTimelineStepKind::Wait;
    float seconds = 0.0f;
    std::string dialogue_id;
    std::string emit_name;
    /// Raw JSON object text for emit payload (default "{}").
    std::string payload_json = "{}";
    /// look_at world target (x,y,z).
    std::array<float, 3> look_at_target{0.0f, 0.0f, 0.0f};
    /// Optional orbit distance (<=0 keeps current).
    float look_at_distance = 0.0f;
    /// Optional pitch radians; use look_at_has_pitch.
    float look_at_pitch = 0.0f;
    bool look_at_has_pitch = false;
};

struct EventTimelineSequence {
    std::string id;
    std::string display_name;
    WorldForgeEventCanonStatus canon_status = WorldForgeEventCanonStatus::Draft;
    std::string summary;
    std::vector<EventTimelineStep> steps;
    /// Campaign act membership (`act0`..`act4`). Empty = campaign-wide. See DEC-0036.
    std::vector<std::string> acts;
    std::vector<std::string> tags;
};

struct WorldForgeEventsAsset {
    int schema_version = 1;
    std::string id;
    std::vector<EventTimelineSequence> sequences;

    [[nodiscard]] Result<void> validate() const;
    /// When non-empty, every start_dialogue dialogueId must be listed.
    [[nodiscard]] Result<void> validate_dialogue_refs(const std::unordered_set<std::string>& known_dialogue_ids) const;
    [[nodiscard]] const EventTimelineSequence* find_sequence(const std::string& sequence_id) const;
    [[nodiscard]] EventTimelineSequence* find_sequence(const std::string& sequence_id);
    [[nodiscard]] static Result<WorldForgeEventsAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeEventsAsset> parse(const std::string& text,
        const std::string& source_name = "events.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_dialogue_ids);
};

[[nodiscard]] const char* to_string(WorldForgeEventCanonStatus value) noexcept;
[[nodiscard]] const char* to_string(EventTimelineStepKind value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_events_path(const std::filesystem::path& project_root);

} // namespace engine
