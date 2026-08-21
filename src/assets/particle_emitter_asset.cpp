#include "engine/assets/particle_emitter_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace engine {
namespace {

EngineError particle_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "particles", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

std::array<float, 3> read_vec3(const nlohmann::json& value) {
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

nlohmann::json write_vec3(const std::array<float, 3>& value) { return nlohmann::json::array({value[0], value[1], value[2]}); }

std::array<float, 2> read_vec2(const nlohmann::json& value) {
    return {value.at(0).get<float>(), value.at(1).get<float>()};
}

Result<ParticleNumberSequence> parse_number_sequence(const nlohmann::json& value, const char* field) {
    ParticleNumberSequence sequence;
    if (value.is_number()) {
        sequence = ParticleNumberSequence::constant(value.get<float>());
        return Result<ParticleNumberSequence>::success(sequence);
    }
    if (value.is_object() && value.contains("keypoints") && value["keypoints"].is_array()) {
        for (const auto& entry : value["keypoints"]) {
            if (!entry.is_object() || !entry.contains("t") || !entry.contains("value")) {
                return Result<ParticleNumberSequence>::failure(particle_error("PARTICLE-SEQUENCE",
                    std::string(field) + " keypoints need t and value",
                    "Use {\"t\":0,\"value\":1} entries sorted by t."));
            }
            ParticleNumberKeypoint key;
            key.t = clampf(entry["t"].get<float>(), 0.0f, 1.0f);
            key.value = entry["value"].get<float>();
            if (!std::isfinite(key.value)) {
                return Result<ParticleNumberSequence>::failure(
                    particle_error("PARTICLE-SEQUENCE", std::string(field) + " value must be finite", "Use finite floats."));
            }
            sequence.keypoints.push_back(key);
        }
        if (sequence.keypoints.empty()) {
            return Result<ParticleNumberSequence>::failure(
                particle_error("PARTICLE-SEQUENCE", std::string(field) + " keypoints empty", "Provide at least one keypoint."));
        }
        std::sort(sequence.keypoints.begin(), sequence.keypoints.end(),
            [](const ParticleNumberKeypoint& a, const ParticleNumberKeypoint& b) { return a.t < b.t; });
        return Result<ParticleNumberSequence>::success(sequence);
    }
    return Result<ParticleNumberSequence>::failure(particle_error("PARTICLE-SEQUENCE",
        std::string(field) + " must be a number or {keypoints:[...]}", "Match Roblox NumberSequence style."));
}

Result<ParticleColorSequence> parse_color_sequence(const nlohmann::json& value) {
    ParticleColorSequence sequence;
    if (value.is_array() && value.size() == 3) {
        sequence = ParticleColorSequence::constant(read_vec3(value));
        return Result<ParticleColorSequence>::success(sequence);
    }
    if (value.is_object() && value.contains("keypoints") && value["keypoints"].is_array()) {
        for (const auto& entry : value["keypoints"]) {
            if (!entry.is_object() || !entry.contains("t") || !entry.contains("color")) {
                return Result<ParticleColorSequence>::failure(particle_error("PARTICLE-COLOR",
                    "color keypoints need t and color[3]", "Use {\"t\":0,\"color\":[1,0.8,0.3]}."));
            }
            ParticleColorKeypoint key;
            key.t = clampf(entry["t"].get<float>(), 0.0f, 1.0f);
            key.color = read_vec3(entry["color"]);
            for (float channel : key.color) {
                if (!std::isfinite(channel) || channel < 0.0f) {
                    return Result<ParticleColorSequence>::failure(
                        particle_error("PARTICLE-COLOR", "color channels must be finite and >= 0", "Fix color keypoints."));
                }
            }
            sequence.keypoints.push_back(key);
        }
        if (sequence.keypoints.empty()) {
            return Result<ParticleColorSequence>::failure(
                particle_error("PARTICLE-COLOR", "color keypoints empty", "Provide at least one keypoint."));
        }
        std::sort(sequence.keypoints.begin(), sequence.keypoints.end(),
            [](const ParticleColorKeypoint& a, const ParticleColorKeypoint& b) { return a.t < b.t; });
        return Result<ParticleColorSequence>::success(sequence);
    }
    return Result<ParticleColorSequence>::failure(
        particle_error("PARTICLE-COLOR", "color must be [r,g,b] or {keypoints:[...]}", "Match Roblox ColorSequence style."));
}

Result<ParticleRange> parse_range(const nlohmann::json& value, const char* field, float default_value) {
    ParticleRange range{default_value, default_value};
    if (value.is_number()) {
        range.min = range.max = value.get<float>();
    } else if (value.is_object()) {
        if (value.contains("min")) range.min = value["min"].get<float>();
        if (value.contains("max")) range.max = value["max"].get<float>();
        else if (value.contains("Min")) range.min = value["Min"].get<float>();
        if (value.contains("Max")) range.max = value["Max"].get<float>();
    } else if (value.is_array() && value.size() == 2) {
        range.min = value[0].get<float>();
        range.max = value[1].get<float>();
    } else {
        return Result<ParticleRange>::failure(particle_error("PARTICLE-RANGE",
            std::string(field) + " must be a number, [min,max], or {min,max}", "Fix the range field."));
    }
    if (!std::isfinite(range.min) || !std::isfinite(range.max) || range.min < 0.0f || range.max < 0.0f) {
        return Result<ParticleRange>::failure(
            particle_error("PARTICLE-RANGE", std::string(field) + " must be finite and >= 0", "Fix the range."));
    }
    if (range.max < range.min) std::swap(range.min, range.max);
    return Result<ParticleRange>::success(range);
}

Result<ParticleEmitterShape> parse_shape(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "box") return Result<ParticleEmitterShape>::success(ParticleEmitterShape::Box);
    if (key == "sphere") return Result<ParticleEmitterShape>::success(ParticleEmitterShape::Sphere);
    if (key == "cylinder") return Result<ParticleEmitterShape>::success(ParticleEmitterShape::Cylinder);
    if (key == "disc" || key == "disk") return Result<ParticleEmitterShape>::success(ParticleEmitterShape::Disc);
    return Result<ParticleEmitterShape>::failure(
        particle_error("PARTICLE-SHAPE", "Unsupported shape: " + raw, "Use box, sphere, cylinder, or disc."));
}

const char* shape_name(ParticleEmitterShape shape) {
    switch (shape) {
    case ParticleEmitterShape::Box: return "box";
    case ParticleEmitterShape::Sphere: return "sphere";
    case ParticleEmitterShape::Cylinder: return "cylinder";
    case ParticleEmitterShape::Disc: return "disc";
    }
    return "disc";
}

Result<ParticleEmitterShapeStyle> parse_shape_style(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "volume") return Result<ParticleEmitterShapeStyle>::success(ParticleEmitterShapeStyle::Volume);
    if (key == "surface") return Result<ParticleEmitterShapeStyle>::success(ParticleEmitterShapeStyle::Surface);
    return Result<ParticleEmitterShapeStyle>::failure(
        particle_error("PARTICLE-SHAPE-STYLE", "Unsupported shapeStyle: " + raw, "Use volume or surface."));
}

const char* shape_style_name(ParticleEmitterShapeStyle style) {
    switch (style) {
    case ParticleEmitterShapeStyle::Volume: return "volume";
    case ParticleEmitterShapeStyle::Surface: return "surface";
    }
    return "volume";
}

Result<ParticleOrientation> parse_orientation(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "facingcamera" || key == "facing_camera")
        return Result<ParticleOrientation>::success(ParticleOrientation::FacingCamera);
    if (key == "facingcameraworldup" || key == "facing_camera_world_up")
        return Result<ParticleOrientation>::success(ParticleOrientation::FacingCameraWorldUp);
    if (key == "velocityparallel" || key == "velocity_parallel")
        return Result<ParticleOrientation>::success(ParticleOrientation::VelocityParallel);
    if (key == "velocityperpendicular" || key == "velocity_perpendicular")
        return Result<ParticleOrientation>::success(ParticleOrientation::VelocityPerpendicular);
    return Result<ParticleOrientation>::failure(particle_error("PARTICLE-ORIENTATION",
        "Unsupported orientation: " + raw,
        "Use FacingCamera, FacingCameraWorldUp, VelocityParallel, or VelocityPerpendicular."));
}

const char* orientation_name(ParticleOrientation orientation) {
    switch (orientation) {
    case ParticleOrientation::FacingCamera: return "FacingCamera";
    case ParticleOrientation::FacingCameraWorldUp: return "FacingCameraWorldUp";
    case ParticleOrientation::VelocityParallel: return "VelocityParallel";
    case ParticleOrientation::VelocityPerpendicular: return "VelocityPerpendicular";
    }
    return "FacingCamera";
}

Result<ParticleBlendMode> parse_blend(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "alpha") return Result<ParticleBlendMode>::success(ParticleBlendMode::Alpha);
    if (key == "additive") return Result<ParticleBlendMode>::success(ParticleBlendMode::Additive);
    if (key == "softlight" || key == "soft_light")
        return Result<ParticleBlendMode>::success(ParticleBlendMode::SoftLight);
    return Result<ParticleBlendMode>::failure(
        particle_error("PARTICLE-BLEND", "Unsupported blend: " + raw, "Use alpha, additive, or softLight."));
}

const char* blend_name(ParticleBlendMode blend) {
    switch (blend) {
    case ParticleBlendMode::Alpha: return "alpha";
    case ParticleBlendMode::Additive: return "additive";
    case ParticleBlendMode::SoftLight: return "softLight";
    }
    return "softLight";
}

Result<ParticleFlipbookLayout> parse_flipbook_layout(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "none" || key.empty()) return Result<ParticleFlipbookLayout>::success(ParticleFlipbookLayout::None);
    if (key == "grid2x2" || key == "2x2") return Result<ParticleFlipbookLayout>::success(ParticleFlipbookLayout::Grid2x2);
    if (key == "grid4x4" || key == "4x4") return Result<ParticleFlipbookLayout>::success(ParticleFlipbookLayout::Grid4x4);
    if (key == "grid8x8" || key == "8x8") return Result<ParticleFlipbookLayout>::success(ParticleFlipbookLayout::Grid8x8);
    return Result<ParticleFlipbookLayout>::failure(particle_error("PARTICLE-FLIPBOOK-LAYOUT",
        "Unsupported flipbookLayout: " + raw, "Use None, Grid2x2, Grid4x4, or Grid8x8."));
}

const char* flipbook_layout_name(ParticleFlipbookLayout layout) {
    switch (layout) {
    case ParticleFlipbookLayout::None: return "None";
    case ParticleFlipbookLayout::Grid2x2: return "Grid2x2";
    case ParticleFlipbookLayout::Grid4x4: return "Grid4x4";
    case ParticleFlipbookLayout::Grid8x8: return "Grid8x8";
    }
    return "None";
}

Result<ParticleFlipbookMode> parse_flipbook_mode(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "oneshot" || key == "one_shot") return Result<ParticleFlipbookMode>::success(ParticleFlipbookMode::OneShot);
    if (key == "loop") return Result<ParticleFlipbookMode>::success(ParticleFlipbookMode::Loop);
    if (key == "pingpong" || key == "ping_pong")
        return Result<ParticleFlipbookMode>::success(ParticleFlipbookMode::PingPong);
    if (key == "random") return Result<ParticleFlipbookMode>::success(ParticleFlipbookMode::Random);
    return Result<ParticleFlipbookMode>::failure(particle_error("PARTICLE-FLIPBOOK-MODE",
        "Unsupported flipbookMode: " + raw, "Use OneShot, Loop, PingPong, or Random."));
}

const char* flipbook_mode_name(ParticleFlipbookMode mode) {
    switch (mode) {
    case ParticleFlipbookMode::OneShot: return "OneShot";
    case ParticleFlipbookMode::Loop: return "Loop";
    case ParticleFlipbookMode::PingPong: return "PingPong";
    case ParticleFlipbookMode::Random: return "Random";
    }
    return "Loop";
}

nlohmann::json write_number_sequence(const ParticleNumberSequence& sequence) {
    if (sequence.keypoints.size() == 1) return sequence.keypoints.front().value;
    nlohmann::json keypoints = nlohmann::json::array();
    for (const auto& key : sequence.keypoints)
        keypoints.push_back({{"t", key.t}, {"value", key.value}});
    return {{"keypoints", keypoints}};
}

nlohmann::json write_color_sequence(const ParticleColorSequence& sequence) {
    if (sequence.keypoints.size() == 1) return write_vec3(sequence.keypoints.front().color);
    nlohmann::json keypoints = nlohmann::json::array();
    for (const auto& key : sequence.keypoints)
        keypoints.push_back({{"t", key.t}, {"color", write_vec3(key.color)}});
    return {{"keypoints", keypoints}};
}

nlohmann::json write_range(const ParticleRange& range) {
    if (std::abs(range.min - range.max) < 1e-6f) return range.min;
    return {{"min", range.min}, {"max", range.max}};
}

std::array<float, 3> normalize3(std::array<float, 3> v) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v[0] / len, v[1] / len, v[2] / len};
}

} // namespace

float ParticleNumberSequence::sample(float t) const {
    if (keypoints.empty()) return 0.0f;
    t = clampf(t, 0.0f, 1.0f);
    if (t <= keypoints.front().t) return keypoints.front().value;
    if (t >= keypoints.back().t) return keypoints.back().value;
    for (std::size_t i = 1; i < keypoints.size(); ++i) {
        if (t <= keypoints[i].t) {
            const auto& a = keypoints[i - 1];
            const auto& b = keypoints[i];
            const float span = std::max(b.t - a.t, 1e-6f);
            const float u = (t - a.t) / span;
            return a.value + (b.value - a.value) * u;
        }
    }
    return keypoints.back().value;
}

ParticleNumberSequence ParticleNumberSequence::constant(float value) {
    ParticleNumberSequence sequence;
    sequence.keypoints.push_back({0.0f, value});
    return sequence;
}

std::array<float, 3> ParticleColorSequence::sample(float t) const {
    if (keypoints.empty()) return {1.0f, 1.0f, 1.0f};
    t = clampf(t, 0.0f, 1.0f);
    if (t <= keypoints.front().t) return keypoints.front().color;
    if (t >= keypoints.back().t) return keypoints.back().color;
    for (std::size_t i = 1; i < keypoints.size(); ++i) {
        if (t <= keypoints[i].t) {
            const auto& a = keypoints[i - 1];
            const auto& b = keypoints[i];
            const float span = std::max(b.t - a.t, 1e-6f);
            const float u = (t - a.t) / span;
            return {a.color[0] + (b.color[0] - a.color[0]) * u, a.color[1] + (b.color[1] - a.color[1]) * u,
                a.color[2] + (b.color[2] - a.color[2]) * u};
        }
    }
    return keypoints.back().color;
}

ParticleColorSequence ParticleColorSequence::constant(std::array<float, 3> color) {
    ParticleColorSequence sequence;
    sequence.keypoints.push_back({0.0f, color});
    return sequence;
}

float ParticleRange::sample(float u01) const { return min + (max - min) * clampf(u01, 0.0f, 1.0f); }

std::uint32_t ParticleEmitterAsset::flipbook_columns() const noexcept {
    switch (flipbook_layout) {
    case ParticleFlipbookLayout::Grid2x2: return 2;
    case ParticleFlipbookLayout::Grid4x4: return 4;
    case ParticleFlipbookLayout::Grid8x8: return 8;
    case ParticleFlipbookLayout::None: return 1;
    }
    return 1;
}

std::uint32_t ParticleEmitterAsset::flipbook_frame_count() const noexcept {
    const auto cols = flipbook_columns();
    return cols * cols;
}

Result<ParticleEmitterAsset> ParticleEmitterAsset::parse(const std::string& text, const std::string& source_name) {
    nlohmann::json document;
    try {
        document = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        return Result<ParticleEmitterAsset>::failure(
            particle_error("PARTICLE-JSON", std::string("Invalid JSON in ") + source_name + ": " + ex.what(),
                "Fix JSON syntax."));
    }
    if (!document.is_object()) {
        return Result<ParticleEmitterAsset>::failure(
            particle_error("PARTICLE-ROOT", "Particle asset root must be an object", "Wrap fields in {}."));
    }

    ParticleEmitterAsset asset;
    asset.schema_version = document.value("schemaVersion", 1);
    if (asset.schema_version != 1) {
        return Result<ParticleEmitterAsset>::failure(particle_error("PARTICLE-SCHEMA",
            "Unsupported schemaVersion " + std::to_string(asset.schema_version), "Use schemaVersion 1."));
    }
    asset.id = document.value("id", std::string{});
    asset.texture = document.value("texture", std::string{});
    if (!asset.texture.empty() && !is_valid_particle_texture_path(asset.texture)) {
        return Result<ParticleEmitterAsset>::failure(particle_error("PARTICLE-TEXTURE-PATH-INVALID",
            "texture must be a project-relative .png path without '..'",
            "Use paths like assets/vfx/fire_flipbook_4x4.png."));
    }
    if (document.contains("color")) {
        auto color = parse_color_sequence(document["color"]);
        if (!color) return Result<ParticleEmitterAsset>::failure(color.error());
        asset.color = color.value();
    }
    if (document.contains("size")) {
        auto size = parse_number_sequence(document["size"], "size");
        if (!size) return Result<ParticleEmitterAsset>::failure(size.error());
        asset.size = size.value();
    }
    if (document.contains("transparency")) {
        auto transparency = parse_number_sequence(document["transparency"], "transparency");
        if (!transparency) return Result<ParticleEmitterAsset>::failure(transparency.error());
        asset.transparency = transparency.value();
    }
    if (document.contains("lifetime")) {
        auto lifetime = parse_range(document["lifetime"], "lifetime", 1.0f);
        if (!lifetime) return Result<ParticleEmitterAsset>::failure(lifetime.error());
        asset.lifetime = lifetime.value();
        if (asset.lifetime.max > 20.0f) asset.lifetime.max = 20.0f;
        if (asset.lifetime.min > asset.lifetime.max) asset.lifetime.min = asset.lifetime.max;
    }
    if (document.contains("speed")) {
        auto speed = parse_range(document["speed"], "speed", 1.0f);
        if (!speed) return Result<ParticleEmitterAsset>::failure(speed.error());
        asset.speed = speed.value();
    }
    if (document.contains("rate")) {
        asset.rate = document["rate"].get<float>();
        if (!std::isfinite(asset.rate) || asset.rate < 0.0f) {
            return Result<ParticleEmitterAsset>::failure(
                particle_error("PARTICLE-RATE", "rate must be finite and >= 0", "Lower rate for performance."));
        }
        if (asset.rate > 400.0f) asset.rate = 400.0f;
    }
    if (document.contains("spreadAngle")) {
        if (!document["spreadAngle"].is_array() || document["spreadAngle"].size() != 2) {
            return Result<ParticleEmitterAsset>::failure(
                particle_error("PARTICLE-SPREAD", "spreadAngle must be [x,y] degrees", "Use [18,18]."));
        }
        asset.spread_angle_deg = read_vec2(document["spreadAngle"]);
    }
    if (document.contains("shape")) {
        auto shape = parse_shape(document["shape"].get<std::string>());
        if (!shape) return Result<ParticleEmitterAsset>::failure(shape.error());
        asset.shape = shape.value();
    }
    if (document.contains("shapeStyle")) {
        auto style = parse_shape_style(document["shapeStyle"].get<std::string>());
        if (!style) return Result<ParticleEmitterAsset>::failure(style.error());
        asset.shape_style = style.value();
    }
    if (document.contains("shapeSize")) asset.shape_size = read_vec3(document["shapeSize"]);
    if (document.contains("orientation")) {
        auto orientation = parse_orientation(document["orientation"].get<std::string>());
        if (!orientation) return Result<ParticleEmitterAsset>::failure(orientation.error());
        asset.orientation = orientation.value();
    }
    if (document.contains("lightEmission")) {
        asset.light_emission = clampf(document["lightEmission"].get<float>(), 0.0f, 1.0f);
    }
    if (document.contains("blend")) {
        auto blend = parse_blend(document["blend"].get<std::string>());
        if (!blend) return Result<ParticleEmitterAsset>::failure(blend.error());
        asset.blend = blend.value();
    }
    if (document.contains("acceleration")) asset.acceleration = read_vec3(document["acceleration"]);
    if (document.contains("drag")) {
        asset.drag = document["drag"].get<float>();
        if (!std::isfinite(asset.drag) || asset.drag < 0.0f) {
            return Result<ParticleEmitterAsset>::failure(
                particle_error("PARTICLE-DRAG", "drag must be finite and >= 0", "Use 0..2."));
        }
    }
    if (document.contains("maxParticles")) {
        asset.max_particles = document["maxParticles"].get<std::uint32_t>();
        if (asset.max_particles == 0 || asset.max_particles > 4096) {
            return Result<ParticleEmitterAsset>::failure(
                particle_error("PARTICLE-POOL", "maxParticles must be 1..4096", "Lower the pool size."));
        }
    }
    if (document.contains("enabled")) asset.enabled = document["enabled"].get<bool>();
    if (document.contains("emissionDirection")) {
        asset.emission_direction = normalize3(read_vec3(document["emissionDirection"]));
    } else {
        asset.emission_direction = normalize3(asset.emission_direction);
    }
    if (document.contains("aspectRatio")) {
        asset.aspect_ratio = document["aspectRatio"].get<float>();
        if (!std::isfinite(asset.aspect_ratio) || asset.aspect_ratio <= 0.0f) {
            return Result<ParticleEmitterAsset>::failure(particle_error("PARTICLE-ASPECT",
                "aspectRatio must be finite and > 0", "Use width/height, typically 0.4..2.0."));
        }
        asset.aspect_ratio = clampf(asset.aspect_ratio, 0.05f, 8.0f);
    }
    if (document.contains("rotation")) {
        auto rotation = parse_number_sequence(document["rotation"], "rotation");
        if (!rotation) return Result<ParticleEmitterAsset>::failure(rotation.error());
        asset.rotation = rotation.value();
    }
    if (document.contains("rotationStartRandom"))
        asset.rotation_start_random = document["rotationStartRandom"].get<bool>();
    if (document.contains("crossedBillboards"))
        asset.crossed_billboards = document["crossedBillboards"].get<bool>();
    if (document.contains("minScreenSize")) {
        asset.min_screen_size = document["minScreenSize"].get<float>();
        if (!std::isfinite(asset.min_screen_size) || asset.min_screen_size < 0.0f) {
            return Result<ParticleEmitterAsset>::failure(particle_error("PARTICLE-MIN-SCREEN",
                "minScreenSize must be finite and >= 0", "Use 0 to disable, or ~12..48 pixels for landmarks."));
        }
        if (asset.min_screen_size > 256.0f) asset.min_screen_size = 256.0f;
    }
    if (document.contains("softOcclusion"))
        asset.soft_occlusion = document["softOcclusion"].get<bool>();
    if (document.contains("flipbookLayout")) {
        auto layout = parse_flipbook_layout(document["flipbookLayout"].get<std::string>());
        if (!layout) return Result<ParticleEmitterAsset>::failure(layout.error());
        asset.flipbook_layout = layout.value();
    }
    if (document.contains("flipbookMode")) {
        auto mode = parse_flipbook_mode(document["flipbookMode"].get<std::string>());
        if (!mode) return Result<ParticleEmitterAsset>::failure(mode.error());
        asset.flipbook_mode = mode.value();
    }
    if (document.contains("flipbookFramerate")) {
        asset.flipbook_framerate = document["flipbookFramerate"].get<float>();
        if (!std::isfinite(asset.flipbook_framerate) || asset.flipbook_framerate < 0.0f) {
            return Result<ParticleEmitterAsset>::failure(particle_error("PARTICLE-FLIPBOOK-FPS",
                "flipbookFramerate must be finite and >= 0", "Use 0..60 typical."));
        }
        if (asset.flipbook_framerate > 120.0f) asset.flipbook_framerate = 120.0f;
    }
    if (document.contains("flipbookStartRandom")) asset.flipbook_start_random = document["flipbookStartRandom"].get<bool>();
    if (asset.flipbook_layout != ParticleFlipbookLayout::None && asset.texture.empty()) {
        return Result<ParticleEmitterAsset>::failure(particle_error("PARTICLE-FLIPBOOK-TEXTURE",
            "flipbookLayout requires a non-empty texture path", "Point texture at a grid atlas PNG."));
    }
    if (asset.id.empty()) asset.id = "particle";
    return Result<ParticleEmitterAsset>::success(asset);
}

bool is_valid_particle_texture_path(const std::string& relative) noexcept {
    if (relative.empty()) return false;
    if (relative.find("..") != std::string::npos) return false;
    if (relative.size() >= 2 && std::isalpha(static_cast<unsigned char>(relative[0])) && relative[1] == ':')
        return false;
    if (relative[0] == '/' || relative[0] == '\\') return false;
    const auto slash = relative.find_last_of("/\\");
    const std::string file = slash == std::string::npos ? relative : relative.substr(slash + 1);
    if (file.size() < 5) return false;
    std::string ext = file.substr(file.size() - 4);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".png";
}

Result<void> ParticleEmitterAsset::validate_texture(const std::filesystem::path& project_root) const {
    if (texture.empty()) return Result<void>::success();
    if (!is_valid_particle_texture_path(texture))
        return Result<void>::failure(particle_error("PARTICLE-TEXTURE-PATH-INVALID",
            "texture must be a project-relative .png path without '..'",
            "Use paths like assets/vfx/fire_flipbook_4x4.png."));
    if (!std::filesystem::exists(project_root / texture))
        return Result<void>::failure(particle_error("PARTICLE-TEXTURE-MISSING",
            "texture file not found: " + texture, "Add the PNG under the project or clear texture."));
    return Result<void>::success();
}

Result<ParticleEmitterAsset> ParticleEmitterAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<ParticleEmitterAsset>::failure(
            particle_error("PARTICLE-IO", "Could not open " + path.string(), "Check the project-relative path."));
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse(buffer.str(), path.string());
}

std::string ParticleEmitterAsset::to_json() const {
    nlohmann::json document{{"schemaVersion", schema_version}, {"id", id}, {"texture", texture},
        {"color", write_color_sequence(color)}, {"size", write_number_sequence(size)},
        {"transparency", write_number_sequence(transparency)}, {"lifetime", write_range(lifetime)},
        {"speed", write_range(speed)}, {"rate", rate},
        {"spreadAngle", nlohmann::json::array({spread_angle_deg[0], spread_angle_deg[1]})},
        {"shape", shape_name(shape)}, {"shapeStyle", shape_style_name(shape_style)},
        {"shapeSize", write_vec3(shape_size)}, {"orientation", orientation_name(orientation)},
        {"lightEmission", light_emission}, {"blend", blend_name(blend)}, {"acceleration", write_vec3(acceleration)},
        {"drag", drag}, {"maxParticles", max_particles}, {"enabled", enabled},
        {"emissionDirection", write_vec3(emission_direction)}, {"aspectRatio", aspect_ratio},
        {"rotation", write_number_sequence(rotation)}, {"rotationStartRandom", rotation_start_random},
        {"crossedBillboards", crossed_billboards}, {"minScreenSize", min_screen_size},
        {"softOcclusion", soft_occlusion},
        {"flipbookLayout", flipbook_layout_name(flipbook_layout)},
        {"flipbookMode", flipbook_mode_name(flipbook_mode)}, {"flipbookFramerate", flipbook_framerate},
        {"flipbookStartRandom", flipbook_start_random}};
    return document.dump(2);
}

Result<void> ParticleEmitterAsset::save_atomic(const std::filesystem::path& path) const {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output)
            return Result<void>::failure(
                particle_error("PARTICLE-IO", "Could not write " + temp, "Check filesystem permissions."));
        output << to_json();
        if (!output)
            return Result<void>::failure(
                particle_error("PARTICLE-IO", "Failed while writing " + temp, "Retry the save."));
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(temp, path, ec);
        if (ec)
            return Result<void>::failure(
                particle_error("PARTICLE-IO", "Could not atomically replace " + path.string(), "Retry the save."));
    }
    return Result<void>::success();
}

} // namespace engine
