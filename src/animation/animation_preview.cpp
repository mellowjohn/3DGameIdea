#include "engine/animation/animation_preview.h"

#include "engine/animation/animator_runtime.h"
#include "engine/assets/animation_clip_asset.h"
#include "engine/core/correlation.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace engine {
namespace {

EngineError preview_error(std::string code, std::string message, std::string remedy = {}) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "animation_preview",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string quote_json(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        default: out += c; break;
        }
    }
    return out + '"';
}

} // namespace

std::string AnimationPreviewReport::to_json() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << "{\"schemaVersion\":1,\"ok\":" << (ok ? "true" : "false")
        << ",\"controllerPath\":" << quote_json(controller_path)
        << ",\"initialState\":" << quote_json(initial_state)
        << ",\"finalState\":" << quote_json(final_state)
        << ",\"totalEvents\":" << total_events
        << ",\"totalRootMotionZ\":" << total_root_motion_z
        << ",\"keyFrames\":[";
    for (std::size_t i = 0; i < key_frames.size(); ++i) {
        if (i) out << ',';
        const auto& frame = key_frames[i];
        out << "{\"index\":" << frame.index
            << ",\"state\":" << quote_json(frame.state)
            << ",\"rootMotionZ\":" << frame.root_motion_z
            << ",\"eventsFired\":" << frame.events_fired << '}';
    }
    return out.str() + "]}";
}

Result<std::string> find_default_animator_controller(const std::filesystem::path& project_root) {
    std::vector<std::filesystem::path> matches;
    const auto assets = project_root / "assets";
    if (!std::filesystem::exists(assets)) {
        return Result<std::string>::failure(
            preview_error("ANIM-PREVIEW-NO-ASSETS", "Project assets folder missing: " + assets.generic_string(),
                "Add assets/animators/*.animator.json or pass --controller."));
    }
    for (auto it = std::filesystem::recursive_directory_iterator(assets);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        if (it->path().extension() == ".json" && it->path().filename().string().find(".animator.") != std::string::npos)
            matches.push_back(it->path());
    }
    if (matches.empty()) {
        return Result<std::string>::failure(
            preview_error("ANIM-PREVIEW-NO-CONTROLLER", "No *.animator.json under assets/",
                "Author a controller or pass --controller <path>."));
    }
    std::sort(matches.begin(), matches.end());
    return Result<std::string>::success(
        std::filesystem::relative(matches.front(), project_root).generic_string());
}

Result<AnimationPreviewReport> run_animation_preview(const AnimationPreviewRequest& request) {
    if (request.project_root.empty()) {
        return Result<AnimationPreviewReport>::failure(
            preview_error("ANIM-PREVIEW-PROJECT", "project_root is required", "Pass --project <dir>."));
    }
    if (request.frames == 0) {
        return Result<AnimationPreviewReport>::failure(
            preview_error("ANIM-PREVIEW-FRAMES", "frames must be > 0", "Pass --frames <n>."));
    }

    std::string controller = request.controller_path;
    if (controller.empty()) {
        auto found = find_default_animator_controller(request.project_root);
        if (!found) return Result<AnimationPreviewReport>::failure(found.error());
        controller = found.value();
    }

    AnimationClipLibrary clips;
    AnimatorRuntime animator;
    animator.set_project_root(request.project_root);
    animator.set_clip_library(&clips);

    auto attached = animator.attach(request.entity_id, controller);
    if (!attached) return Result<AnimationPreviewReport>::failure(attached.error());

    AnimationPreviewReport report;
    report.controller_path = controller;
    report.ok = true;

    auto initial = animator.current_state(request.entity_id);
    report.initial_state = initial ? initial.value() : std::string{};

    std::string last_state = report.initial_state;
    const std::uint32_t key_stride = std::max<std::uint32_t>(1, request.frames / 10);

    (void)animator.set_float(request.entity_id, "speed", request.speed);
    if (request.fire_attack_trigger) {
        const std::uint32_t trigger_frame = request.frames / 2;
        for (std::uint32_t frame = 0; frame < request.frames; ++frame) {
            if (frame == trigger_frame) (void)animator.set_trigger(request.entity_id, "attack");
            animator.tick(request.dt_seconds);
            std::uint32_t batch = static_cast<std::uint32_t>(animator.take_fired_events().size());
            report.total_events += batch;
            auto root = animator.root_motion_delta(request.entity_id);
            if (root) report.total_root_motion_z += root.value().translation[2];
            auto state = animator.current_state(request.entity_id);
            const std::string current = state ? state.value() : std::string{};
            if (frame == 0 || frame + 1 == request.frames || frame % key_stride == 0 || current != last_state) {
                AnimationPreviewFrame key{frame, current, root ? root.value().translation[2] : 0.0f, batch};
                report.key_frames.push_back(std::move(key));
                last_state = current;
            }
        }
    } else {
        for (std::uint32_t frame = 0; frame < request.frames; ++frame) {
            animator.tick(request.dt_seconds);
            std::uint32_t batch = static_cast<std::uint32_t>(animator.take_fired_events().size());
            report.total_events += batch;
            auto root = animator.root_motion_delta(request.entity_id);
            if (root) report.total_root_motion_z += root.value().translation[2];
            auto state = animator.current_state(request.entity_id);
            const std::string current = state ? state.value() : std::string{};
            if (frame == 0 || frame + 1 == request.frames || frame % key_stride == 0 || current != last_state) {
                AnimationPreviewFrame key{frame, current, root ? root.value().translation[2] : 0.0f, batch};
                report.key_frames.push_back(std::move(key));
                last_state = current;
            }
        }
    }

    auto final_state = animator.current_state(request.entity_id);
    report.final_state = final_state ? final_state.value() : std::string{};
    return Result<AnimationPreviewReport>::success(std::move(report));
}

} // namespace engine
