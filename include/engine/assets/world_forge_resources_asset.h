#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

enum class WorldForgeResourceKind : std::uint8_t {
    Mineral,
    Herb,
    Food,
    Craft,
    Quest,
    Other
};

enum class WorldForgeResourceRarity : std::uint8_t {
    Common,
    Uncommon,
    Rare,
    Legendary,
    Unique
};

struct WorldForgeResourceEntity {
    std::string id;
    WorldForgeResourceKind kind = WorldForgeResourceKind::Other;
    std::string display_name;
    std::string summary;
    std::string obtain_notes;
    std::string story_ref;
    std::optional<WorldForgeResourceRarity> rarity;
    std::vector<std::string> region_ids;
    std::vector<std::string> tags;
};

struct WorldForgeResourcesAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgeResourceEntity> entities;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] Result<void> validate_region_refs(const std::unordered_set<std::string>& known_region_ids) const;
    [[nodiscard]] const WorldForgeResourceEntity* find_entity(const std::string& entity_id) const;
    [[nodiscard]] WorldForgeResourceEntity* find_entity(const std::string& entity_id);
    [[nodiscard]] static Result<WorldForgeResourcesAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgeResourcesAsset> parse(const std::string& text,
        const std::string& source_name = "resources.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path,
        const std::unordered_set<std::string>& known_region_ids);
};

[[nodiscard]] const char* to_string(WorldForgeResourceKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgeResourceRarity value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_resources_path(const std::filesystem::path& project_root);

} // namespace engine
