#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeArchetypeKind : std::uint8_t { Starting, Advanced };

struct WorldForgeArchetypeUnlock {
    std::optional<double> morality_threshold;
    std::string faction_id;
    std::vector<std::string> tags;
};

struct WorldForgeArchetypeEntity {
    std::string id;
    WorldForgeArchetypeKind kind = WorldForgeArchetypeKind::Starting;
    std::string display_name;
    std::string role;
    std::string summary;
    std::string draft_advancement;
    std::string starter_kit_prefab_id;
    std::string story_ref;
    std::vector<std::string> tags;
    std::optional<WorldForgeArchetypeUnlock> unlock;
};

struct WorldForgeArchetypesAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeArchetypeEntity> entities;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] Result<void> validate_faction_refs(const std::unordered_set<std::string>& known_faction_ids) const;
    [[nodiscard]] const WorldForgeArchetypeEntity* find_entity(const std::string& entity_id) const;
    [[nodiscard]] WorldForgeArchetypeEntity* find_entity(const std::string& entity_id);
    [[nodiscard]] static Result<WorldForgeArchetypesAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeArchetypesAsset> parse(const std::string& text,
        const std::string& source_name = "archetypes.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_faction_ids);
};

[[nodiscard]] const char* to_string(WorldForgeArchetypeKind value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_archetypes_path(const std::filesystem::path& project_root);

} // namespace engine
