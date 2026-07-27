#pragma once

#include "engine/core/result.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace engine {

/// Session story / outcome flags (TICKET-0225 / DEC-0046). Freeform string ids for Act 0 gates and forks.
class FlagRuntime {
public:
    [[nodiscard]] Result<void> set(const std::string& flag_id);
    [[nodiscard]] Result<void> clear(const std::string& flag_id);
    [[nodiscard]] Result<void> apply_many(const std::vector<std::string>& flag_ids);
    [[nodiscard]] bool has(const std::string& flag_id) const noexcept;
    /// Sorted for deterministic save / MCP list output.
    [[nodiscard]] std::vector<std::string> list() const;
    void reset() noexcept;
    /// Replace session flags from save hydrate (empty ids skipped).
    void restore(std::vector<std::string> flag_ids);

private:
    std::unordered_set<std::string> flags_;
};

} // namespace engine
