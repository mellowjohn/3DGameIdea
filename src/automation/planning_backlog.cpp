#include "engine/automation/planning_backlog.h"

#include "engine/core/error.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace engine {
namespace {

std::string trim_copy(std::string value) {
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
        value.pop_back();
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) ++start;
    return value.substr(start);
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<std::string> split_table_row(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    // Skip the leading pipe; keep escaped pipes out of scope (not used in epics.md).
    for (std::size_t i = 1; i < line.size(); ++i) {
        if (line[i] == '|') {
            cells.push_back(trim_copy(current));
            current.clear();
        } else {
            current.push_back(line[i]);
        }
    }
    return cells;
}

bool looks_like_ticket_id(const std::string& value) {
    if (value.rfind("TICKET-", 0) != 0 || value.size() <= 7) return false;
    return std::all_of(value.begin() + 7, value.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; });
}

} // namespace

const PlanningTicket* PlanningBacklog::find(const std::string& ticket_id) const {
    for (const auto& ticket : tickets) {
        if (ticket.id == ticket_id) return &ticket;
    }
    return nullptr;
}

std::map<std::string, int> PlanningBacklog::status_counts() const {
    std::map<std::string, int> counts;
    for (const auto& ticket : tickets) ++counts[ticket.status];
    return counts;
}

std::optional<std::filesystem::path> find_planning_root(const std::filesystem::path& start) {
    std::error_code ec;
    auto current = std::filesystem::weakly_canonical(start, ec);
    if (ec || current.empty()) current = std::filesystem::absolute(start);
    while (!current.empty()) {
        if (std::filesystem::exists(current / "context" / "planning" / "epics.md")) return current;
        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return std::nullopt;
}

Result<PlanningBacklog> load_planning_backlog(const std::filesystem::path& epics_md_path) {
    std::ifstream input(epics_md_path, std::ios::binary);
    if (!input) {
        return Result<PlanningBacklog>::failure(EngineError{"PLANNING-BACKLOG-MISSING", Severity::Error,
            ErrorCategory::Io, "planning_backlog", "Could not open " + epics_md_path.generic_string(),
            ENGINE_SOURCE_CONTEXT, {}, "Point at the authoritative context/planning/epics.md.",
            make_correlation_id()});
    }

    PlanningBacklog backlog;
    backlog.source_path = epics_md_path;
    std::string current_epic;
    std::string line;
    while (std::getline(input, line)) {
        const auto trimmed = trim_copy(line);
        if (trimmed.rfind("## EPIC-", 0) == 0) {
            const auto id_start = trimmed.find("EPIC-");
            auto id_end = trimmed.find(':', id_start);
            if (id_end == std::string::npos) id_end = trimmed.size();
            current_epic = trim_copy(trimmed.substr(id_start, id_end - id_start));
            continue;
        }
        // Non-epic headings (e.g. "## M6 engineering (under EPIC-0006 hold)") keep the row scope readable.
        if (trimmed.rfind("## ", 0) == 0 && trimmed.rfind("## EPIC-", 0) != 0) {
            const auto embedded = trimmed.find("EPIC-");
            current_epic = embedded != std::string::npos
                ? trim_copy(trimmed.substr(embedded, 9))
                : std::string{};
            continue;
        }
        if (trimmed.rfind("| TICKET-", 0) != 0) continue;
        const auto cells = split_table_row(trimmed);
        if (cells.size() < 4) continue;
        if (!looks_like_ticket_id(cells[0])) continue;
        PlanningTicket ticket;
        ticket.id = cells[0];
        ticket.title = cells[1];
        ticket.status = lower_copy(cells[2]);
        ticket.priority = cells[3];
        ticket.epic_id = current_epic;
        // A ticket may appear under two epics (e.g. TICKET-0114); keep the first row authoritative.
        if (!backlog.find(ticket.id)) backlog.tickets.push_back(std::move(ticket));
    }
    if (backlog.tickets.empty()) {
        return Result<PlanningBacklog>::failure(EngineError{"PLANNING-BACKLOG-EMPTY", Severity::Error,
            ErrorCategory::Validation, "planning_backlog",
            "No ticket table rows were found in " + epics_md_path.generic_string(), ENGINE_SOURCE_CONTEXT, {},
            "Verify the epics.md ticket tables use `| TICKET-#### | Title | Status | Priority | Notes |` rows.",
            make_correlation_id()});
    }
    return Result<PlanningBacklog>::success(std::move(backlog));
}

} // namespace engine
