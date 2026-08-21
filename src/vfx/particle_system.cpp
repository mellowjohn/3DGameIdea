#include "engine/vfx/particle_system.h"



#include <algorithm>

#include <cmath>

#include <cstdio>

#include <cstring>

#include <functional>

#include <string>



namespace engine {

namespace {



constexpr float k_pi = 3.14159265358979323846f;

constexpr const char* k_ambient_wind_key = "__ambient_wind__";



std::string normalize_particle_path(std::string path) {

    for (char& c : path) {

        if (c == '\\') c = '/';

    }

    while (!path.empty() && path.front() == '/') path.erase(path.begin());

    return path;

}



float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }



std::array<float, 3> add3(const std::array<float, 3>& a, const std::array<float, 3>& b) {

    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};

}



std::array<float, 3> scale3(const std::array<float, 3>& a, float s) { return {a[0] * s, a[1] * s, a[2] * s}; }



std::array<float, 3> normalize3(std::array<float, 3> v) {

    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (len < 1e-6f) return {0.0f, 1.0f, 0.0f};

    return {v[0] / len, v[1] / len, v[2] / len};

}



std::array<float, 3> cross3(const std::array<float, 3>& a, const std::array<float, 3>& b) {

    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};

}



/// Rotate vector by unit quaternion stored as xyzw (glTF / TransformComponent).

std::array<float, 3> rotate_by_quat(const std::array<float, 4>& q, const std::array<float, 3>& v) {

    const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];

    const float tx = 2.0f * (qy * v[2] - qz * v[1]);

    const float ty = 2.0f * (qz * v[0] - qx * v[2]);

    const float tz = 2.0f * (qx * v[1] - qy * v[0]);

    return {v[0] + qw * tx + (qy * tz - qz * ty), v[1] + qw * ty + (qz * tx - qx * tz),

        v[2] + qw * tz + (qx * ty - qy * tx)};

}



std::array<float, 3> transform_local_offset(const TransformComponent& transform,

    const std::array<float, 3>& local_offset) {

    const std::array<float, 3> scaled{local_offset[0] * transform.scale[0], local_offset[1] * transform.scale[1],

        local_offset[2] * transform.scale[2]};

    return add3(transform.position, rotate_by_quat(transform.rotation, scaled));

}



std::uint32_t hash_mix(std::uint32_t x) {

    x ^= x >> 16;

    x *= 0x7feb352du;

    x ^= x >> 15;

    x *= 0x846ca68bu;

    x ^= x >> 16;

    return x;

}



std::string placement_key(const std::string& prefab_path, const TransformComponent& transform) {

    char buffer[160];

    std::snprintf(buffer, sizeof(buffer), "%s@%.3f,%.3f,%.3f", normalize_particle_path(prefab_path).c_str(),

        transform.position[0], transform.position[1], transform.position[2]);

    return buffer;

}



std::array<float, 3> sample_shape(ParticleEmitterShape shape, ParticleEmitterShapeStyle style,

    const std::array<float, 3>& size, float u, float v, float w) {

    switch (shape) {

    case ParticleEmitterShape::Box: {

        const float sx = size[0];

        const float sy = size[1];

        const float sz = size[2];

        if (style == ParticleEmitterShapeStyle::Surface) {

            const int face = static_cast<int>(u * 6.0f) % 6;

            const float a = v * 2.0f - 1.0f;

            const float b = w * 2.0f - 1.0f;

            switch (face) {

            case 0: return {sx, a * sy, b * sz};

            case 1: return {-sx, a * sy, b * sz};

            case 2: return {a * sx, sy, b * sz};

            case 3: return {a * sx, -sy, b * sz};

            case 4: return {a * sx, b * sy, sz};

            default: return {a * sx, b * sy, -sz};

            }

        }

        return {(u * 2.0f - 1.0f) * sx, (v * 2.0f - 1.0f) * sy, (w * 2.0f - 1.0f) * sz};

    }

    case ParticleEmitterShape::Sphere: {

        const float radius = std::max(size[0], 0.01f);

        const float theta = u * k_pi * 2.0f;

        const float phi = std::acos(clampf(v * 2.0f - 1.0f, -1.0f, 1.0f));

        const float r = style == ParticleEmitterShapeStyle::Surface ? radius : radius * std::cbrt(w);

        return {r * std::sin(phi) * std::cos(theta), r * std::cos(phi), r * std::sin(phi) * std::sin(theta)};

    }

    case ParticleEmitterShape::Cylinder: {

        const float radius = std::max(size[0], 0.01f);

        const float half_h = std::max(size[1], 0.01f);

        const float theta = u * k_pi * 2.0f;

        const float r = style == ParticleEmitterShapeStyle::Surface ? radius : radius * std::sqrt(v);

        const float y = (w * 2.0f - 1.0f) * half_h;

        return {r * std::cos(theta), y, r * std::sin(theta)};

    }

    case ParticleEmitterShape::Disc:

    default: {

        const float radius = std::max(size[0], 0.01f);

        const float theta = u * k_pi * 2.0f;

        const float r = style == ParticleEmitterShapeStyle::Surface ? radius : radius * std::sqrt(v);

        return {r * std::cos(theta), (w * 2.0f - 1.0f) * size[1], r * std::sin(theta)};

    }

    }

}



std::array<float, 3> emit_direction(const std::array<float, 3>& base, float spread_x_deg, float spread_y_deg, float u,

    float v) {

    const float yaw = (u * 2.0f - 1.0f) * spread_x_deg * (k_pi / 180.0f);

    const float pitch = (v * 2.0f - 1.0f) * spread_y_deg * (k_pi / 180.0f);

    std::array<float, 3> up = {0.0f, 1.0f, 0.0f};

    if (std::abs(base[1]) > 0.95f) up = {1.0f, 0.0f, 0.0f};

    const auto right = normalize3(cross3(up, base));

    up = normalize3(cross3(base, right));

    auto dir = add3(add3(scale3(base, std::cos(pitch) * std::cos(yaw)), scale3(right, std::sin(yaw))),

        scale3(up, std::sin(pitch)));

    return normalize3(dir);

}



} // namespace



void ParticleSystem::clear() {

    assets_.clear();

    texture_paths_.clear();

    emitters_.clear();

    ambient_emitter_.reset();

    draw_.clear();

    active_particles_ = 0;

    burst_seq_ = 0;

}



void ParticleSystem::register_asset(std::string asset_path, ParticleEmitterAsset asset) {

    register_texture_path(asset.texture);

    assets_[normalize_particle_path(std::move(asset_path))] = std::move(asset);

}



const ParticleEmitterAsset* ParticleSystem::find_asset(const std::string& asset_path) const {

    const auto found = assets_.find(normalize_particle_path(asset_path));

    return found == assets_.end() ? nullptr : &found->second;

}



float ParticleSystem::next_u01(Emitter& emitter) {

    emitter.rng = hash_mix(emitter.rng + 0x9e3779b9u);

    return static_cast<float>(emitter.rng & 0x00FFFFFFu) / static_cast<float>(0x01000000u);

}



void ParticleSystem::register_texture_path(const std::string& texture_path) {

    if (texture_path.empty()) return;

    const auto path = normalize_particle_path(texture_path);

    for (const auto& existing : texture_paths_) {

        if (existing == path) return;

    }

    texture_paths_.push_back(path);

}



std::uint32_t ParticleSystem::texture_index_for_asset(const ParticleEmitterAsset& asset) {

    if (asset.texture.empty()) return 0u;

    const auto path = normalize_particle_path(asset.texture);

    for (std::size_t i = 0; i < texture_paths_.size(); ++i) {

        if (texture_paths_[i] == path) return static_cast<std::uint32_t>(i + 1);

    }

    texture_paths_.push_back(path);

    return static_cast<std::uint32_t>(texture_paths_.size());

}



float ParticleSystem::flipbook_frame_f(const ParticleEmitterAsset& asset, float age,
    std::uint32_t start_frame) {

    const std::uint32_t frames = asset.flipbook_frame_count();

    if (frames <= 1 || asset.flipbook_layout == ParticleFlipbookLayout::None) return 0.0f;

    const float frame_count = static_cast<float>(frames);

    const float start = static_cast<float>(start_frame % frames);

    if (asset.flipbook_mode == ParticleFlipbookMode::Random) return start;

    const float fps = std::max(asset.flipbook_framerate, 0.0f);

    const float advanced = age * fps;

    if (asset.flipbook_mode == ParticleFlipbookMode::OneShot) {

        return std::min(start + advanced, frame_count - 1.0001f);

    }

    if (asset.flipbook_mode == ParticleFlipbookMode::PingPong) {

        if (frames == 1) return 0.0f;

        const float span = frame_count - 1.0f;

        const float cycle = span * 2.0f;

        float pos = std::fmod(start + advanced, cycle);

        if (pos < 0.0f) pos += cycle;

        if (pos <= span) return pos;

        return cycle - pos;

    }

    // Loop (continuous; fractional part drives GPU crossfade)

    float pos = std::fmod(start + advanced, frame_count);

    if (pos < 0.0f) pos += frame_count;

    return pos;

}



void ParticleSystem::emit_one(Emitter& emitter, const ParticleEmitterAsset& asset) {

    Particle* slot = nullptr;

    for (auto& particle : emitter.pool) {

        if (!particle.alive) {

            slot = &particle;

            break;

        }

    }

    if (!slot) return;



    const float u = next_u01(emitter);

    const float v = next_u01(emitter);

    const float w = next_u01(emitter);

    const auto local = sample_shape(asset.shape, asset.shape_style, asset.shape_size, u, v, w);

    const auto local_spawn = add3(emitter.offset, local);

    slot->position = transform_local_offset(emitter.transform, local_spawn);



    const auto base_dir = emitter.emission_direction_override

        ? *emitter.emission_direction_override

        : rotate_by_quat(emitter.transform.rotation, asset.emission_direction);

    const auto dir =

        emit_direction(base_dir, asset.spread_angle_deg[0], asset.spread_angle_deg[1], next_u01(emitter),

            next_u01(emitter));

    const float speed = asset.speed.sample(next_u01(emitter));

    slot->velocity = scale3(dir, speed);

    slot->lifetime = std::max(asset.lifetime.sample(next_u01(emitter)), 0.05f);

    slot->age = 0.0f;

    slot->rotation_offset_deg = asset.rotation_start_random ? next_u01(emitter) * 360.0f : 0.0f;

    slot->flipbook_start = 0;

    if (asset.flipbook_layout != ParticleFlipbookLayout::None) {

        const std::uint32_t frames = std::max(asset.flipbook_frame_count(), 1u);

        if (asset.flipbook_mode == ParticleFlipbookMode::Random || asset.flipbook_start_random) {

            slot->flipbook_start = static_cast<std::uint32_t>(next_u01(emitter) * static_cast<float>(frames)) % frames;

        }

    }

    slot->alive = true;

}



void ParticleSystem::sync_placements(const std::map<std::string, PrefabAsset>& prefab_catalog,

    const std::vector<std::pair<std::string, TransformComponent>>& placements) {

    std::vector<Emitter> next;

    next.reserve(placements.size() + emitters_.size());

    // Preserve one-shot burst emitters across placement rebuilds.

    for (auto& emitter : emitters_) {

        if (emitter.transient_burst) next.push_back(std::move(emitter));

    }

    for (const auto& placement : placements) {

        const auto* prefab = find_prefab_in_catalog(prefab_catalog, placement.first);

        if (!prefab || (prefab->particles.empty() && !prefab->particle)) continue;

        std::vector<PrefabParticleEmitter> emitters_spec = prefab->particles;

        if (emitters_spec.empty() && prefab->particle) emitters_spec.push_back(*prefab->particle);

        for (std::size_t emitter_index = 0; emitter_index < emitters_spec.size(); ++emitter_index) {

            const auto& spec = emitters_spec[emitter_index];

            if (spec.asset.empty()) continue;

            const auto key = placement_key(placement.first, placement.second) + "|" +
                normalize_particle_path(spec.asset) + "|" + std::to_string(emitter_index);

            Emitter* existing = nullptr;

            for (auto& emitter : emitters_) {

                if (!emitter.transient_burst && emitter.key == key) {

                    existing = &emitter;

                    break;

                }

            }

            Emitter emitter = existing ? std::move(*existing) : Emitter{};

            emitter.key = key;

            emitter.asset_path = normalize_particle_path(spec.asset);

            emitter.transform = placement.second;

            emitter.offset = spec.offset;

            emitter.enabled = spec.enabled;

            emitter.emission_direction_override.reset();

            emitter.rate_scale = 1.0f;

            emitter.transient_burst = false;

            if (emitter.pool.empty()) {

                const auto* asset = find_asset(emitter.asset_path);

                const std::uint32_t pool = asset ? asset->max_particles : 128;

                emitter.pool.assign(pool, Particle{});

                emitter.rng = hash_mix(seed_ ^ static_cast<std::uint32_t>(std::hash<std::string>{}(key)));

            }

            next.push_back(std::move(emitter));

        }

    }

    emitters_ = std::move(next);

}



void ParticleSystem::set_ambient_wind(bool enabled, const std::string& asset_path,

    const std::array<float, 3>& camera_position, const std::array<float, 3>& wind_direction, float strength,
    float gust, float wind_speed) {

    if (!enabled || asset_path.empty() || strength <= 0.01f) {

        if (ambient_emitter_) ambient_emitter_->enabled = false;

        return;

    }

    const auto path = normalize_particle_path(asset_path);

    if (!find_asset(path)) {

        if (ambient_emitter_) ambient_emitter_->enabled = false;

        return;

    }

    if (!ambient_emitter_ || ambient_emitter_->asset_path != path) {

        Emitter emitter;

        emitter.key = k_ambient_wind_key;

        emitter.asset_path = path;

        const auto* asset = find_asset(path);

        emitter.pool.assign(asset ? asset->max_particles : 64, Particle{});

        emitter.rng = hash_mix(seed_ ^ 0xA71Bu);

        ambient_emitter_ = std::move(emitter);

    }

    const auto wind_dir = normalize3(wind_direction);

    ambient_emitter_->enabled = true;

    ambient_emitter_->transform.position = camera_position;

    ambient_emitter_->transform.scale = {1.0f, 1.0f, 1.0f};

    // Spawn upwind and slightly above the camera so elongated streaks drift through the view.
    const float upwind = 6.0f + clampf(wind_speed, 1.0f, 12.0f) * 0.35f;
    ambient_emitter_->offset = {-wind_dir[0] * upwind, 2.1f, -wind_dir[2] * upwind};

    ambient_emitter_->emission_direction_override = wind_dir;

    // Calm meadows keep a faint trickle; traveling gust bands thicken the trails.
    const float gust01 = clampf(gust, 0.0f, 1.0f);
    ambient_emitter_->rate_scale = clampf(strength * (0.4f + 0.95f * gust01), 0.12f, 2.6f);

}



void ParticleSystem::update_emitter(Emitter& emitter, float dt) {

    const auto* asset = find_asset(emitter.asset_path);

    if (!asset || !asset->enabled || !emitter.enabled) return;

    if (emitter.pool.size() != asset->max_particles) emitter.pool.resize(asset->max_particles);



    emitter.spawn_accum += asset->rate * emitter.rate_scale * dt;

    while (emitter.spawn_accum >= 1.0f) {

        emit_one(emitter, *asset);

        emitter.spawn_accum -= 1.0f;

    }



    for (auto& particle : emitter.pool) {

        if (!particle.alive) continue;

        particle.age += dt;

        if (particle.age >= particle.lifetime) {

            particle.alive = false;

            continue;

        }

        particle.velocity = add3(particle.velocity, scale3(asset->acceleration, dt));

        const float drag_factor = std::exp(-asset->drag * dt);

        particle.velocity = scale3(particle.velocity, drag_factor);

        particle.position = add3(particle.position, scale3(particle.velocity, dt));

    }

}



void ParticleSystem::update(float dt_seconds, const std::array<float, 3>& camera_position) {

    const float dt = std::max(0.0f, std::min(dt_seconds, 0.1f));

    for (auto& emitter : emitters_) update_emitter(emitter, dt);

    if (ambient_emitter_) update_emitter(*ambient_emitter_, dt);

    // Drop finished one-shot bursts so sync_placements does not accumulate forever.

    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(),
                        [](const Emitter& emitter) {
                            if (!emitter.transient_burst) return false;
                            for (const auto& particle : emitter.pool) {
                                if (particle.alive) return false;
                            }
                            return true;
                        }),
        emitters_.end());

    rebuild_draw_list(camera_position);

}



void ParticleSystem::rebuild_draw_list(const std::array<float, 3>& /*camera_position*/) {

    draw_.clear();

    active_particles_ = 0;

    const auto append_emitter = [&](const Emitter& emitter) {

        const auto* asset = find_asset(emitter.asset_path);

        if (!asset) return;

        for (const auto& particle : emitter.pool) {

            if (!particle.alive) continue;

            const float t = clampf(particle.age / std::max(particle.lifetime, 1e-4f), 0.0f, 1.0f);

            const auto rgb = asset->color.sample(t);

            const float size = std::max(asset->size.sample(t), 0.01f);

            const float transparency = clampf(asset->transparency.sample(t), 0.0f, 1.0f);

            const float rotation_deg = asset->rotation.sample(t) + particle.rotation_offset_deg;

            ParticleDrawInstance instance;

            instance.position = particle.position;

            instance.size = size;

            instance.aspect = std::max(asset->aspect_ratio, 0.05f);

            instance.rotation_rad = rotation_deg * 0.01745329252f;

            instance.min_screen_size = asset->min_screen_size;

            instance.cross_arm = asset->crossed_billboards ? 1u : 0u;

            instance.color = {rgb[0], rgb[1], rgb[2], 1.0f - transparency};

            instance.axis = normalize3(particle.velocity);

            instance.light_emission = asset->light_emission;

            if (asset->blend == ParticleBlendMode::Additive) instance.light_emission = 1.0f;

            else if (asset->blend == ParticleBlendMode::Alpha && asset->light_emission < 0.25f)
                instance.light_emission = 0.15f;

            instance.blend_mode = static_cast<std::uint32_t>(asset->blend);

            instance.orientation = static_cast<std::uint32_t>(asset->orientation);

            instance.texture_index = texture_index_for_asset(*asset);

            instance.uv_offset_u = 0.0f;

            instance.uv_offset_v = 0.0f;

            instance.uv_scale_u = 1.0f;

            instance.uv_scale_v = 1.0f;

            instance.uv_offset_u2 = 0.0f;

            instance.uv_offset_v2 = 0.0f;

            instance.flipbook_blend = 0.0f;

            instance.soft_occlusion = asset->soft_occlusion ? 1.0f : 0.0f;

            if (asset->flipbook_layout != ParticleFlipbookLayout::None && instance.texture_index != 0) {

                const auto cols = std::max(asset->flipbook_columns(), 1u);

                const auto frames = std::max(asset->flipbook_frame_count(), 1u);

                const float frame_f = flipbook_frame_f(*asset, particle.age, particle.flipbook_start);

                const float frame_floor = std::floor(frame_f);

                const auto frame_a = static_cast<std::uint32_t>(frame_floor) % frames;

                const auto frame_b = (frame_a + 1u) % frames;

                const float inv = 1.0f / static_cast<float>(cols);

                instance.uv_offset_u = static_cast<float>(frame_a % cols) * inv;

                instance.uv_offset_v = static_cast<float>(frame_a / cols) * inv;

                instance.uv_offset_u2 = static_cast<float>(frame_b % cols) * inv;

                instance.uv_offset_v2 = static_cast<float>(frame_b / cols) * inv;

                instance.uv_scale_u = inv;

                instance.uv_scale_v = inv;

                instance.flipbook_blend = (asset->flipbook_mode == ParticleFlipbookMode::Random)

                    ? 0.0f

                    : (frame_f - frame_floor);

            }

            draw_.push_back(instance);

            ++active_particles_;

            if (asset->crossed_billboards) {

                instance.cross_arm = 2;

                draw_.push_back(instance);

            }

        }

    };

    for (const auto& emitter : emitters_) append_emitter(emitter);

    if (ambient_emitter_) append_emitter(*ambient_emitter_);

}



bool ParticleSystem::spawn_burst(const std::string& asset_path, const std::array<float, 3>& world_position,
    std::size_t count, const std::optional<std::array<float, 3>>& emission_direction) {

    if (count == 0) return false;

    const auto path = normalize_particle_path(asset_path);

    const auto* asset = find_asset(path);

    if (!asset || !asset->enabled) return false;

    Emitter emitter;

    emitter.key = "burst|" + path + "|" + std::to_string(++burst_seq_);

    emitter.asset_path = path;

    emitter.transform.position = world_position;

    emitter.transform.scale = {1.0f, 1.0f, 1.0f};

    emitter.offset = {0.0f, 0.0f, 0.0f};

    emitter.enabled = true;

    emitter.rate_scale = 0.0f; // no continuous spawn; particles come from the forced emit below

    emitter.transient_burst = true;

    if (emission_direction) emitter.emission_direction_override = normalize3(*emission_direction);

    emitter.pool.assign(asset->max_particles, Particle{});

    emitter.rng = hash_mix(seed_ ^ burst_seq_ ^ static_cast<std::uint32_t>(std::hash<std::string>{}(emitter.key)));

    const std::size_t to_emit = std::min(count, static_cast<std::size_t>(asset->max_particles));

    for (std::size_t i = 0; i < to_emit; ++i) emit_one(emitter, *asset);

    emitters_.push_back(std::move(emitter));

    rebuild_draw_list(world_position);

    return true;

}



void ParticleSystem::debug_burst(std::size_t count) {

    if (emitters_.empty()) return;

    auto& emitter = emitters_.front();

    const auto* asset = find_asset(emitter.asset_path);

    if (!asset) return;

    if (emitter.pool.empty()) emitter.pool.assign(asset->max_particles, Particle{});

    for (std::size_t i = 0; i < count; ++i) emit_one(emitter, *asset);

    rebuild_draw_list({0.0f, 0.0f, 0.0f});

}



} // namespace engine


