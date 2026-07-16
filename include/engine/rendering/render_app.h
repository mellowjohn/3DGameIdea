#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace engine {

struct RenderOptions {
    std::filesystem::path project_root;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frame_limit = 0;
    bool hidden = false;
    bool enable_debug_layer = true;
    bool debug_world = false;
    bool editor = false;
    bool attach_console = false;
    std::string initial_viewport;
    std::filesystem::path capture_path;
    std::filesystem::path world_path;
};

struct RenderStats {
    std::uint64_t frames = 0;
    double elapsed_seconds = 0.0;
    double average_cpu_ms = 0.0;
    double average_gpu_ms = 0.0;
    double frames_per_second = 0.0;
    std::string adapter;
};

[[nodiscard]] Result<RenderStats> run_render_app(const RenderOptions& options);

} // namespace engine
