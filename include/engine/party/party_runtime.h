#pragma once

#include "engine/core/result.h"
#include "engine/session/game_session.h"

#include <cstddef>
#include <string>
#include <vector>

namespace engine {

/// Active companion roster with mode-dependent caps ([DEC-0042] / TICKET-0213).
class PartyRuntime {
public:
    void bind_session(const GameSession* session) noexcept { session_ = session; }
    [[nodiscard]] const GameSession* session() const noexcept { return session_; }

    [[nodiscard]] std::size_t max_humans() const noexcept;
    [[nodiscard]] std::size_t max_companions() const noexcept;
    [[nodiscard]] std::size_t max_party_size() const noexcept;

    /// Optional display ids used by camp staging (defaults: player_host / player_guest).
    void set_human_member_ids(std::string host_id, std::string guest_id = {});

    [[nodiscard]] Result<void> add_companion(const std::string& companion_id);
    [[nodiscard]] Result<void> remove_companion(const std::string& companion_id);
    [[nodiscard]] const std::vector<std::string>& list_active() const noexcept { return companions_; }
    [[nodiscard]] Result<void> set_active_companions(std::vector<std::string> companion_ids);

    /// Humans (per mode) + active companions, capped at max_party_size (DEC-0033 camp staging).
    [[nodiscard]] std::vector<std::string> party_members_for_camp() const;

    void reset() noexcept;

private:
    [[nodiscard]] Result<void> fail(std::string code, std::string message, std::string remedy) const;

    const GameSession* session_ = nullptr;
    std::vector<std::string> companions_;
    std::string host_member_id_ = "player_host";
    std::string guest_member_id_ = "player_guest";
};

} // namespace engine
