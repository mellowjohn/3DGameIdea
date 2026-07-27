#include "engine/party/party_runtime.h"

#include "engine/core/error.h"

#include <algorithm>

namespace engine {
namespace {

EngineError party_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "party_runtime",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

} // namespace

Result<void> PartyRuntime::fail(std::string code, std::string message, std::string remedy) const {
    return Result<void>::failure(party_error(std::move(code), std::move(message), std::move(remedy)));
}

std::size_t PartyRuntime::max_humans() const noexcept {
    if (session_ && session_->session_mode() == SessionMode::Coop) return 2;
    return 1;
}

std::size_t PartyRuntime::max_companions() const noexcept {
    if (session_ && session_->session_mode() == SessionMode::Coop) return 2;
    return 3;
}

std::size_t PartyRuntime::max_party_size() const noexcept { return 4; }

void PartyRuntime::set_human_member_ids(std::string host_id, std::string guest_id) {
    if (!host_id.empty()) host_member_id_ = std::move(host_id);
    guest_member_id_ = std::move(guest_id);
}

Result<void> PartyRuntime::add_companion(const std::string& companion_id) {
    if (companion_id.empty()) {
        return fail("PARTY-INVALID-ID", "Companion id must be non-empty", "Pass a World Forge person id.");
    }
    if (std::find(companions_.begin(), companions_.end(), companion_id) != companions_.end()) {
        return fail("PARTY-DUPLICATE", "Companion already in party: " + companion_id,
            "Remove the companion before adding again.");
    }
    if (companions_.size() >= max_companions()) {
        return fail("PARTY-OVER-CAP",
            "Companion roster is full for this session mode (max " + std::to_string(max_companions()) + ")",
            "Remove a companion or switch session mode caps.");
    }
    companions_.push_back(companion_id);
    return Result<void>::success();
}

Result<void> PartyRuntime::remove_companion(const std::string& companion_id) {
    const auto it = std::find(companions_.begin(), companions_.end(), companion_id);
    if (it == companions_.end()) {
        return fail("PARTY-NOT-FOUND", "Companion not in party: " + companion_id,
            "Use list_active for current companion ids.");
    }
    companions_.erase(it);
    return Result<void>::success();
}

Result<void> PartyRuntime::set_active_companions(std::vector<std::string> companion_ids) {
    if (companion_ids.size() > max_companions()) {
        return fail("PARTY-OVER-CAP",
            "Cannot restore " + std::to_string(companion_ids.size()) + " companions (max " +
                std::to_string(max_companions()) + " for mode)",
            "Trim sharedCampaign.party.activeCompanionIds to the mode cap.");
    }
    for (const auto& id : companion_ids) {
        if (id.empty()) {
            return fail("PARTY-INVALID-ID", "Companion id must be non-empty", "Fix save party ids.");
        }
    }
    for (std::size_t i = 0; i < companion_ids.size(); ++i) {
        for (std::size_t j = i + 1; j < companion_ids.size(); ++j) {
            if (companion_ids[i] == companion_ids[j]) {
                return fail("PARTY-DUPLICATE", "Duplicate companion id in roster: " + companion_ids[i],
                    "Deduplicate activeCompanionIds.");
            }
        }
    }
    companions_ = std::move(companion_ids);
    return Result<void>::success();
}

std::vector<std::string> PartyRuntime::party_members_for_camp() const {
    std::vector<std::string> out;
    out.reserve(max_party_size());
    if (!host_member_id_.empty()) out.push_back(host_member_id_);
    if (max_humans() > 1 && !guest_member_id_.empty()) out.push_back(guest_member_id_);
    for (const auto& companion : companions_) {
        if (out.size() >= max_party_size()) break;
        out.push_back(companion);
    }
    return out;
}

void PartyRuntime::reset() noexcept { companions_.clear(); }

} // namespace engine
