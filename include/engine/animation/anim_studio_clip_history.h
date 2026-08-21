#pragma once

#include "engine/assets/animation_clip_asset.h"

#include <cstddef>
#include <string>
#include <vector>

namespace engine {

/** One undo/redo snapshot of the Animation Studio edit-clip buffer. */
struct AnimStudioClipHistoryEntry {
    std::string label;
    AnimationClip clip;
};

/**
 * Snapshot stack for Animation Studio clip key edits (TICKET-style dual-edit buffer).
 * Discrete edits call push_before; continuous gestures (bone gizmo / timeline drag)
 * call begin_gesture once so a drag is a single undo step.
 */
class AnimStudioClipHistory final {
public:
    static constexpr std::size_t k_max_depth = 64;

    /** Snapshot `before` as the state prior to an upcoming discrete edit. Clears redo. */
    void push_before(AnimationClip before, std::string label);

    /**
     * First call in a gesture snapshots `before`; further calls while active are no-ops.
     * Pair with end_gesture when the mouse/gizmo is released.
     */
    void begin_gesture(AnimationClip before, std::string label);
    void end_gesture() noexcept;

    [[nodiscard]] bool undo(AnimationClip& current);
    [[nodiscard]] bool redo(AnimationClip& current);
    void clear() noexcept;

    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] bool gesture_active() const noexcept { return gesture_active_; }
    [[nodiscard]] std::size_t undo_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redo_size() const noexcept { return redo_.size(); }
    [[nodiscard]] const std::string& last_summary() const noexcept { return last_summary_; }

private:
    void trim_undo_() noexcept;

    std::vector<AnimStudioClipHistoryEntry> undo_;
    std::vector<AnimStudioClipHistoryEntry> redo_;
    std::string last_summary_;
    bool gesture_active_ = false;
};

} // namespace engine
