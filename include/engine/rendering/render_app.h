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
    /// Local co-op prove-out: dual player slots without networking (TICKET-0212 / --coop-local).
    bool coop_local = false;
    bool attach_console = false;
    /// When true (benchmark), missing GPU timestamp queries fail the run (TICKET-0139).
    bool require_gpu_timestamps = false;
    std::string initial_viewport;
    std::filesystem::path capture_path;
    std::filesystem::path world_path;
    /// Optional JSON metrics dump path for benchmark runs.
    std::filesystem::path benchmark_report_path;
    /// When true, print CommandResponse JSON before hard-exit on hidden benchmark success.
    bool cli_json = false;
    /// Prefer Game play-test RT for capture (chrome-free; TICKET-0145). Also used when initial_viewport is game.
    bool capture_game_viewport = false;
    /// Apply once to orbit look after `capture_look_at_frame` presented frames (pixel-space orbit deltas).
    float capture_look_dx = 0.0f;
    float capture_look_dy = 0.0f;
    std::uint32_t capture_look_at_frame = 20;
    /// Skip std::_Exit after hidden frame-limited runs (needed when one process captures multiple shots).
    bool skip_hidden_hard_exit = false;
};

struct RenderStats {
    std::uint64_t frames = 0;
    double elapsed_seconds = 0.0;
    double average_cpu_ms = 0.0;
    double average_gpu_ms = 0.0;
    double frames_per_second = 0.0;
    std::string adapter;
    /// Last-frame streamed terrain cell count (after warm-up streaming).
    std::uint64_t terrain_cells = 0;
    /// Last-frame opaque/foliage DrawInstanced count (world pass).
    std::uint64_t draw_calls = 0;
    /// Last-frame instance count summed across instanced draws (foliage-heavy).
    std::uint64_t instances_drawn = 0;
    bool gpu_timestamps_ok = false;
};

[[nodiscard]] Result<RenderStats> run_render_app(const RenderOptions& options);

} // namespace engine
