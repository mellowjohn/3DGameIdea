#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <optional>
#include <string>

namespace engine {

class QuestRuntime;
class StandingRuntime;
class FlagRuntime;

enum class SessionMode : std::uint8_t { Solo, Coop };

enum class GameSessionState : std::uint8_t {
    Menu,
    SoloLoading,
    CoopLobby,
    CoopLoading,
    Playing,
    PausedWaitingGuest,
    Ended
};

/// Shared camera leash for local/online co-op (TICKET-0212). Midpoint keeps both humans framed.
enum class CameraLeashMode : std::uint8_t { Midpoint };

struct PlayerSlotState {
    bool connected = false;
    bool ready = false;
    /// Local device index: 0 = keyboard WASD (host), 1 = arrows / second device (guest).
    int device_index = -1;
};

[[nodiscard]] const char* to_string(SessionMode value) noexcept;
[[nodiscard]] const char* to_string(GameSessionState value) noexcept;
[[nodiscard]] const char* to_string(CameraLeashMode value) noexcept;

/// Campaign session owner for solo vs co-op mode locks ([DEC-0042] / TICKET-0212).
/// Headless-first; editor play-test binds QuestRuntime / StandingRuntime and optional dual local slots.
class GameSession {
public:
    static constexpr int k_host_slot = 0;
    static constexpr int k_guest_slot = 1;
    static constexpr int k_slot_count = 2;

    [[nodiscard]] SessionMode session_mode() const noexcept { return mode_; }
    [[nodiscard]] GameSessionState state() const noexcept { return state_; }
    [[nodiscard]] CameraLeashMode camera_leash() const noexcept { return CameraLeashMode::Midpoint; }
    [[nodiscard]] const PlayerSlotState& slot(int index) const;
    [[nodiscard]] bool simulation_allowed() const noexcept;
    [[nodiscard]] bool is_coop() const noexcept { return mode_ == SessionMode::Coop; }

    void bind_quest_runtime(QuestRuntime* quests) noexcept { quests_ = quests; }
    void bind_standing_runtime(StandingRuntime* standing) noexcept { standing_ = standing; }
    void bind_flag_runtime(FlagRuntime* flags) noexcept { flags_ = flags; }
    [[nodiscard]] QuestRuntime* quest_runtime() const noexcept { return quests_; }
    [[nodiscard]] StandingRuntime* standing_runtime() const noexcept { return standing_; }
    [[nodiscard]] FlagRuntime* flag_runtime() const noexcept { return flags_; }

    /// Reset to menu; clears slot connection flags. Does not destroy bound runtimes.
    void reset_to_menu() noexcept;

    /// Solo: Menu → SoloLoading with slot 0 connected.
    [[nodiscard]] Result<void> begin_solo();
    /// Co-op: Menu → CoopLobby (neither slot required yet).
    [[nodiscard]] Result<void> begin_coop_lobby();

    /// Mark a human slot connected/disconnected. Guest disconnect while Playing → PausedWaitingGuest.
    /// Disconnect clears ready for that slot.
    [[nodiscard]] Result<void> set_slot_connected(int slot, bool connected, int device_index = -1);

    /// Lobby ready toggle (TICKET-0214). Requires slot connected.
    [[nodiscard]] Result<void> set_ready(int slot, bool ready);
    [[nodiscard]] bool is_ready(int slot) const noexcept;

    /// Host Start gate: both connected, both ready, state CoopLobby/CoopLoading.
    [[nodiscard]] bool can_host_start() const noexcept;

    /// Mock online join for local lobby prove-out (default invite `COOP-LOCAL`).
    [[nodiscard]] const std::string& invite_code() const noexcept { return invite_code_; }
    void set_invite_code(std::string code);
    [[nodiscard]] Result<void> mock_guest_join(const std::string& code);

    /// SoloLoading → Playing, or CoopLobby/CoopLoading → Playing when both slots connected.
    [[nodiscard]] Result<void> start_playing();

    /// CoopLobby → CoopLoading (requires both slots connected).
    [[nodiscard]] Result<void> enter_coop_loading();

    /// PausedWaitingGuest → Playing when guest slot is connected again.
    [[nodiscard]] Result<void> resume_after_guest_reconnect();

    /// Any active state → Ended. Does not change sessionMode to Solo.
    [[nodiscard]] Result<void> end_session();

private:
    [[nodiscard]] Result<void> fail(std::string code, std::string message, std::string remedy) const;
    [[nodiscard]] bool both_humans_connected() const noexcept;

    SessionMode mode_ = SessionMode::Solo;
    GameSessionState state_ = GameSessionState::Menu;
    PlayerSlotState slots_[k_slot_count]{};
    std::string invite_code_ = "COOP-LOCAL";
    QuestRuntime* quests_ = nullptr;
    StandingRuntime* standing_ = nullptr;
    FlagRuntime* flags_ = nullptr;
};

} // namespace engine
