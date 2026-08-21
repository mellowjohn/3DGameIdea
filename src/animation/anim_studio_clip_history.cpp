#include "engine/animation/anim_studio_clip_history.h"

namespace engine {

void AnimStudioClipHistory::trim_undo_() noexcept {
    while (undo_.size() > k_max_depth)
        undo_.erase(undo_.begin());
}

void AnimStudioClipHistory::push_before(AnimationClip before, std::string label) {
    gesture_active_ = false;
    undo_.push_back(AnimStudioClipHistoryEntry{std::move(label), std::move(before)});
    trim_undo_();
    redo_.clear();
}

void AnimStudioClipHistory::begin_gesture(AnimationClip before, std::string label) {
    if (gesture_active_)
        return;
    push_before(std::move(before), std::move(label));
    gesture_active_ = true;
}

void AnimStudioClipHistory::end_gesture() noexcept {
    gesture_active_ = false;
}

bool AnimStudioClipHistory::undo(AnimationClip& current) {
    gesture_active_ = false;
    if (undo_.empty())
        return false;
    last_summary_ = undo_.back().label;
    AnimStudioClipHistoryEntry redone;
    redone.label = last_summary_;
    redone.clip = std::move(current);
    redo_.push_back(std::move(redone));
    current = std::move(undo_.back().clip);
    undo_.pop_back();
    return true;
}

bool AnimStudioClipHistory::redo(AnimationClip& current) {
    gesture_active_ = false;
    if (redo_.empty())
        return false;
    last_summary_ = redo_.back().label;
    AnimStudioClipHistoryEntry undone;
    undone.label = last_summary_;
    undone.clip = std::move(current);
    undo_.push_back(std::move(undone));
    trim_undo_();
    current = std::move(redo_.back().clip);
    redo_.pop_back();
    return true;
}

void AnimStudioClipHistory::clear() noexcept {
    undo_.clear();
    redo_.clear();
    last_summary_.clear();
    gesture_active_ = false;
}

} // namespace engine
