#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

struct AnimationPreviewRequest {
    std::filesystem::path project_root;
    std::string controller_path; // project-relative; empty = first *.animator.json found
    std::string entity_id = "animation-preview-entity";
    std::uint32_t frames = 60;
    float dt_seconds = 1.0f / 60.0f;
    float speed = 0.5f;
    bool fire_attack_trigger = true;
};

struct AnimationPreviewFrame {
    std::uint32_t index = 0;
    std::string state;
    float root_motion_z = 0.0f;
    std::uint32_t events_fired = 0;
};

struct AnimationPreviewReport {
    bool ok = false;
    std::string controller_path;
    std::string initial_state;
    std::string final_state;
    std::uint32_t total_events = 0;
    float total_root_motion_z = 0.0f;
    std::vector<AnimationPreviewFrame> key_frames;
    [[nodiscard]] std::string to_json() const;
};

/// Headless animator smoke for CLI / M5 exit (TICKET-0110).
[[nodiscard]] Result<AnimationPreviewReport> run_animation_preview(const AnimationPreviewRequest& request);

/// First `*.animator.json` under project assets (deterministic sort).
[[nodiscard]] Result<std::string> find_default_animator_controller(const std::filesystem::path& project_root);

} // namespace engine
