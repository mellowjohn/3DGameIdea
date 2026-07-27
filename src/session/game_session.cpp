#include "engine/session/game_session.h"

#include "engine/core/error.h"

namespace engine {
namespace {

EngineError session_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "game_session",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

} // namespace

const char* to_string(SessionMode value) noexcept {
    switch (value) {
    case SessionMode::Solo: return "solo";
    case SessionMode::Coop: return "coop";
    }
    return "solo";
}

const char* to_string(GameSessionState value) noexcept {
    switch (value) {
    case GameSessionState::Menu: return "menu";
    case GameSessionState::SoloLoading: return "solo_loading";
    case GameSessionState::CoopLobby: return "coop_lobby";
    case GameSessionState::CoopLoading: return "coop_loading";
    case GameSessionState::Playing: return "playing";
    case GameSessionState::PausedWaitingGuest: return "paused_waiting_guest";
    case GameSessionState::Ended: return "ended";
    }
    return "menu";
}

const char* to_string(CameraLeashMode value) noexcept {
    switch (value) {
    case CameraLeashMode::Midpoint: return "midpoint";
    }
    return "midpoint";
}

Result<void> GameSession::fail(std::string code, std::string message, std::string remedy) const {
    return Result<void>::failure(session_error(std::move(code), std::move(message), std::move(remedy)));
}

const PlayerSlotState& GameSession::slot(int index) const {
    static const PlayerSlotState k_invalid{};
    if (index < 0 || index >= k_slot_count) return k_invalid;
    return slots_[index];
}

bool GameSession::simulation_allowed() const noexcept {
    return state_ == GameSessionState::Playing;
}

bool GameSession::both_humans_connected() const noexcept {
    return slots_[k_host_slot].connected && slots_[k_guest_slot].connected;
}

void GameSession::reset_to_menu() noexcept {
    mode_ = SessionMode::Solo;
    state_ = GameSessionState::Menu;
    slots_[0] = {};
    slots_[1] = {};
    invite_code_ = "COOP-LOCAL";
}

Result<void> GameSession::begin_solo() {
    if (state_ != GameSessionState::Menu && state_ != GameSessionState::Ended) {
        return fail("GAME-SESSION-INVALID-STATE",
            std::string("Cannot begin solo from state ") + to_string(state_),
            "Call end_session or reset_to_menu first.");
    }
    mode_ = SessionMode::Solo;
    state_ = GameSessionState::SoloLoading;
    slots_[0] = PlayerSlotState{true, false, 0};
    slots_[1] = {};
    return Result<void>::success();
}

Result<void> GameSession::begin_coop_lobby() {
    if (state_ != GameSessionState::Menu && state_ != GameSessionState::Ended) {
        return fail("GAME-SESSION-INVALID-STATE",
            std::string("Cannot begin co-op lobby from state ") + to_string(state_),
            "Call end_session or reset_to_menu first.");
    }
    mode_ = SessionMode::Coop;
    state_ = GameSessionState::CoopLobby;
    slots_[0] = {};
    slots_[1] = {};
    return Result<void>::success();
}

Result<void> GameSession::set_slot_connected(int slot, bool connected, int device_index) {
    if (slot < 0 || slot >= k_slot_count) {
        return fail("GAME-SESSION-INVALID-SLOT", "Player slot must be 0 (host) or 1 (guest)",
            "Pass playerSlot 0 or 1.");
    }
    if (mode_ == SessionMode::Solo && slot == k_guest_slot && connected) {
        return fail("GAME-SESSION-SOLO-NO-GUEST", "Solo sessions reject guest slot connection",
            "Use begin_coop_lobby for two-player sessions.");
    }

    slots_[slot].connected = connected;
    slots_[slot].device_index = connected ? device_index : -1;
    if (!connected) slots_[slot].ready = false;

    if (state_ == GameSessionState::Playing && mode_ == SessionMode::Coop && slot == k_guest_slot &&
        !connected) {
        state_ = GameSessionState::PausedWaitingGuest;
    }
    return Result<void>::success();
}

Result<void> GameSession::set_ready(int slot, bool ready) {
    if (slot < 0 || slot >= k_slot_count) {
        return fail("GAME-SESSION-INVALID-SLOT", "Player slot must be 0 (host) or 1 (guest)",
            "Pass playerSlot 0 or 1.");
    }
    if (!slots_[slot].connected) {
        return fail("GAME-SESSION-NOT-CONNECTED",
            "Cannot ready a disconnected slot", "Connect the slot before setting ready.");
    }
    if (mode_ != SessionMode::Coop ||
        (state_ != GameSessionState::CoopLobby && state_ != GameSessionState::CoopLoading)) {
        return fail("GAME-SESSION-INVALID-STATE", "Ready toggles are only valid in co-op lobby",
            "Call begin_coop_lobby before set_ready.");
    }
    slots_[slot].ready = ready;
    return Result<void>::success();
}

bool GameSession::is_ready(int slot) const noexcept {
    if (slot < 0 || slot >= k_slot_count) return false;
    return slots_[slot].ready;
}

bool GameSession::can_host_start() const noexcept {
    if (mode_ != SessionMode::Coop) return false;
    if (state_ != GameSessionState::CoopLobby && state_ != GameSessionState::CoopLoading) return false;
    return both_humans_connected() && slots_[k_host_slot].ready && slots_[k_guest_slot].ready;
}

void GameSession::set_invite_code(std::string code) {
    if (!code.empty()) invite_code_ = std::move(code);
}

Result<void> GameSession::mock_guest_join(const std::string& code) {
    if (mode_ != SessionMode::Coop || state_ != GameSessionState::CoopLobby) {
        return fail("GAME-SESSION-INVALID-STATE", "mock_guest_join requires CoopLobby",
            "Call begin_coop_lobby first.");
    }
    if (!slots_[k_host_slot].connected) {
        return fail("GAME-SESSION-HOST-REQUIRED", "Host must be connected before guest joins",
            "Connect playerSlot 0 first.");
    }
    if (code != invite_code_) {
        return fail("GAME-SESSION-BAD-INVITE", "Invite code does not match lobby",
            "Use the host invite code (default COOP-LOCAL).");
    }
    return set_slot_connected(k_guest_slot, true, 1);
}

Result<void> GameSession::enter_coop_loading() {
    if (mode_ != SessionMode::Coop || state_ != GameSessionState::CoopLobby) {
        return fail("GAME-SESSION-INVALID-STATE", "enter_coop_loading requires CoopLobby",
            "Call begin_coop_lobby and connect both slots first.");
    }
    if (!both_humans_connected()) {
        return fail("GAME-SESSION-COOP-NEEDS-GUEST",
            "Co-op loading requires host and guest slots connected",
            "Connect playerSlot 1 before starting the co-op session.");
    }
    state_ = GameSessionState::CoopLoading;
    return Result<void>::success();
}

Result<void> GameSession::start_playing() {
    if (mode_ == SessionMode::Solo) {
        if (state_ != GameSessionState::SoloLoading) {
            return fail("GAME-SESSION-INVALID-STATE",
                std::string("Solo start_playing requires SoloLoading, got ") + to_string(state_),
                "Call begin_solo first.");
        }
        if (!slots_[k_host_slot].connected) {
            return fail("GAME-SESSION-HOST-REQUIRED", "Solo playing requires host slot connected",
                "Connect playerSlot 0.");
        }
        state_ = GameSessionState::Playing;
        return Result<void>::success();
    }

    // Co-op
    if (state_ == GameSessionState::CoopLobby) {
        const auto loading = enter_coop_loading();
        if (!loading) return loading;
    }
    if (state_ != GameSessionState::CoopLoading) {
        return fail("GAME-SESSION-INVALID-STATE",
            std::string("Co-op start_playing requires CoopLobby or CoopLoading, got ") + to_string(state_),
            "Call begin_coop_lobby and connect both humans first.");
    }
    if (!both_humans_connected()) {
        return fail("GAME-SESSION-COOP-NEEDS-GUEST",
            "Co-op playing requires host and guest slots connected",
            "Connect playerSlot 1 before entering playing.");
    }
    state_ = GameSessionState::Playing;
    return Result<void>::success();
}

Result<void> GameSession::resume_after_guest_reconnect() {
    if (state_ != GameSessionState::PausedWaitingGuest) {
        return fail("GAME-SESSION-INVALID-STATE",
            "resume_after_guest_reconnect requires paused_waiting_guest",
            "Wait for guest disconnect pause before resuming.");
    }
    if (!slots_[k_guest_slot].connected) {
        return fail("GAME-SESSION-COOP-NEEDS-GUEST",
            "Cannot resume until guest slot is connected again",
            "Call set_slot_connected(1, true) then resume.");
    }
    if (!slots_[k_host_slot].connected) {
        return fail("GAME-SESSION-HOST-REQUIRED", "Host slot must remain connected during co-op resume",
            "Reconnect host slot 0.");
    }
    state_ = GameSessionState::Playing;
    return Result<void>::success();
}

Result<void> GameSession::end_session() {
    if (state_ == GameSessionState::Menu) {
        return fail("GAME-SESSION-INVALID-STATE", "Already at menu; nothing to end",
            "Begin a session before ending.");
    }
    // Preserve mode_ — co-op saves must not downgrade to solo ([DEC-0042]).
    state_ = GameSessionState::Ended;
    return Result<void>::success();
}

} // namespace engine
