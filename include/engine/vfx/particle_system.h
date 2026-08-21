#pragma once



#include "engine/assets/particle_emitter_asset.h"

#include "engine/assets/prefab_asset.h"

#include "engine/world/components.h"



#include <array>

#include <cstdint>

#include <map>

#include <optional>

#include <string>

#include <vector>



namespace engine {



struct ParticleDrawInstance {

    std::array<float, 3> position{{0.0f, 0.0f, 0.0f}};

    float size = 0.2f; // billboard height (world units)

    float aspect = 1.0f; // width = size * aspect

    float rotation_rad = 0.0f; // roll/yaw applied in the renderer

    float min_screen_size = 0.0f; // pixels; 0 = no screen-space floor

    /// 0 = single billboard; 1/2 = world-locked crossed arms (90° apart around Y).
    std::uint32_t cross_arm = 0;

    std::array<float, 4> color{{1.0f, 1.0f, 1.0f, 1.0f}}; // rgb + alpha (1 - transparency)

    std::array<float, 3> axis{{0.0f, 1.0f, 0.0f}}; // velocity / stretch axis

    float light_emission = 0.0f;

    std::uint32_t orientation = 0; // ParticleOrientation

    std::uint32_t texture_index = 0; // 0 = built-in soft disc; 1+ = texture_paths()[index-1]

    float uv_offset_u = 0.0f;

    float uv_offset_v = 0.0f;

    float uv_scale_u = 1.0f;

    float uv_scale_v = 1.0f;

    float uv_offset_u2 = 0.0f;

    float uv_offset_v2 = 0.0f;

    float flipbook_blend = 0.0f; // 0 = uv, 1 = uv2 (soft frame crossfade)

    /// 1 = apply scene-depth soft occlusion; 0 = always draw (weapon impacts).
    float soft_occlusion = 1.0f;

    std::uint32_t blend_mode = 0; // ParticleBlendMode

};



/// CPU particle emitter pool (TICKET-0122 MVP). Billboard GPU draw lives in the renderer.

class ParticleSystem final {

public:

    void clear();

    void set_seed(std::uint32_t seed) noexcept { seed_ = seed; }



    /// Register or replace an emitter definition (asset id → definition).

    void register_asset(std::string asset_path, ParticleEmitterAsset asset);



    [[nodiscard]] const ParticleEmitterAsset* find_asset(const std::string& asset_path) const;



    /// Sync live emitters from scene placements that declare a prefab `particle` attachment.

    /// Does not clear the ambient wind emitter.

    void sync_placements(const std::map<std::string, PrefabAsset>& prefab_catalog,

        const std::vector<std::pair<std::string, TransformComponent>>& placements);



    /// Camera-local ambient wind streak emitter (TICKET-0230). Empty asset_path disables.
    /// `gust` is a 0..1 traveling-band pulse (same envelope as foliage); `wind_speed` scales drift.

    void set_ambient_wind(bool enabled, const std::string& asset_path, const std::array<float, 3>& camera_position,

        const std::array<float, 3>& wind_direction, float strength = 1.0f, float gust = 0.0f,
        float wind_speed = 4.5f);



    /// Spawn/update particles. `camera_position` used for optional distance cull later.

    void update(float dt_seconds, const std::array<float, 3>& camera_position);



    [[nodiscard]] const std::vector<ParticleDrawInstance>& draw_instances() const noexcept { return draw_; }

    /// Project-relative PNG paths for texture_index 1..N (empty when only soft-disc particles).
    [[nodiscard]] const std::vector<std::string>& texture_paths() const noexcept { return texture_paths_; }

    [[nodiscard]] std::size_t active_particle_count() const noexcept { return active_particles_; }

    [[nodiscard]] std::size_t emitter_count() const noexcept {

        return emitters_.size() + (ambient_emitter_ ? 1u : 0u);

    }

    [[nodiscard]] bool ambient_wind_enabled() const noexcept {

        return ambient_emitter_.has_value() && ambient_emitter_->enabled;

    }



    /// Gameplay / test one-shot: spawn `count` particles at a world position from a registered asset.
    /// Transient burst emitters survive `sync_placements` and are removed once their particles die.
    /// Returns false if the asset is unregistered or `count` is zero.
    [[nodiscard]] bool spawn_burst(const std::string& asset_path, const std::array<float, 3>& world_position,
        std::size_t count, const std::optional<std::array<float, 3>>& emission_direction = std::nullopt);

    /// Headless helper: force-spawn `count` particles on emitter 0 (tests).

    void debug_burst(std::size_t count);



private:

    struct Particle {

        std::array<float, 3> position{};

        std::array<float, 3> velocity{};

        float age = 0.0f;

        float lifetime = 1.0f;

        float rotation_offset_deg = 0.0f;

        std::uint32_t flipbook_start = 0;

        bool alive = false;

    };



    struct Emitter {

        std::string key;

        std::string asset_path;

        TransformComponent transform{};

        std::array<float, 3> offset{{0.0f, 0.0f, 0.0f}};

        bool enabled = true;

        float spawn_accum = 0.0f;

        std::vector<Particle> pool;

        std::uint32_t rng = 1;

        /// When set, overrides asset emission_direction for this emitter.

        std::optional<std::array<float, 3>> emission_direction_override;

        float rate_scale = 1.0f;

        /// One-shot burst emitters are preserved across sync_placements and culled when empty.

        bool transient_burst = false;

    };



    [[nodiscard]] float next_u01(Emitter& emitter);

    void emit_one(Emitter& emitter, const ParticleEmitterAsset& asset);

    void update_emitter(Emitter& emitter, float dt);

    void rebuild_draw_list(const std::array<float, 3>& camera_position);

    [[nodiscard]] std::uint32_t texture_index_for_asset(const ParticleEmitterAsset& asset);

    void register_texture_path(const std::string& texture_path);

    [[nodiscard]] static float flipbook_frame_f(const ParticleEmitterAsset& asset, float age,
        std::uint32_t start_frame);



    std::map<std::string, ParticleEmitterAsset> assets_;

    std::vector<std::string> texture_paths_;

    std::vector<Emitter> emitters_;

    std::optional<Emitter> ambient_emitter_;

    std::vector<ParticleDrawInstance> draw_;

    std::size_t active_particles_ = 0;

    std::uint32_t seed_ = 0xC0FFEEu;

    std::uint32_t burst_seq_ = 0;

};



} // namespace engine


