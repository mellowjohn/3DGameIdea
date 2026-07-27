#pragma once

#include "engine/core/result.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

/// One row from an `epics.md` ticket table (DEC-0015 authoritative backlog).
struct PlanningTicket {
    std::string id;       // TICKET-0228
    std::string title;
    std::string status;   // lowercased token: proposed|ready|active|needs-approval|done|deferred
    std::string priority; // P0..P3
    std::string epic_id;  // EPIC-0001 (nearest heading above the row)
};

/// Read-only view of the planning backlog. Never writes Markdown or Notion.
struct PlanningBacklog {
    std::filesystem::path source_path;
    std::vector<PlanningTicket> tickets;

    [[nodiscard]] const PlanningTicket* find(const std::string& ticket_id) const;
    [[nodiscard]] std::map<std::string, int> status_counts() const;
};

/// Nearest ancestor of `start` (inclusive) that contains `context/planning/epics.md`.
[[nodiscard]] std::optional<std::filesystem::path> find_planning_root(const std::filesystem::path& start);

/// Parses the ticket tables out of `epics.md` (display/validation only; TICKET-0228).
[[nodiscard]] Result<PlanningBacklog> load_planning_backlog(const std::filesystem::path& epics_md_path);

} // namespace engine
