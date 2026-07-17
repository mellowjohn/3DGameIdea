#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

enum class WorldForgePantheonKind : std::uint8_t { Deity, Aspect, Force };
enum class WorldForgePantheonCanonStatus : std::uint8_t { Established, Draft, Proposal, Open };

struct WorldForgePantheonEntity {
    std::string id;
    WorldForgePantheonKind kind = WorldForgePantheonKind::Deity;
    std::string display_name;
    WorldForgePantheonCanonStatus canon_status = WorldForgePantheonCanonStatus::Draft;
    std::string summary;
    std::string story_ref;
    std::vector<std::string> tags;
    /// Empty means no parent. Non-empty must reference another entity id in the same asset.
    std::string parent_id;
    std::vector<std::string> open_questions;
};

struct WorldForgePantheonAsset {
    int schema_version = 1;
    std::string id;
    std::vector<WorldForgePantheonEntity> entities;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] const WorldForgePantheonEntity* find_entity(const std::string& entity_id) const;
    [[nodiscard]] WorldForgePantheonEntity* find_entity(const std::string& entity_id);
    [[nodiscard]] static Result<WorldForgePantheonAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<WorldForgePantheonAsset> parse(const std::string& text,
        const std::string& source_name = "pantheon.worldforge.json");
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<void> validate_file(const std::filesystem::path& path);
};

[[nodiscard]] const char* to_string(WorldForgePantheonKind value) noexcept;
[[nodiscard]] const char* to_string(WorldForgePantheonCanonStatus value) noexcept;

[[nodiscard]] std::filesystem::path default_world_forge_pantheon_path(const std::filesystem::path& project_root);

} // namespace engine
