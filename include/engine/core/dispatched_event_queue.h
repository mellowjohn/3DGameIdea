#pragma once

#include <cstddef>
#include <vector>

namespace engine {

/// Keep the newest `max_keep` entries and repair a monotonic Lua/dispatch cursor
/// after erasing from the front. Call **after** dispatching newly appended events.
///
/// Without cursor repair, trimming `recent_*_events` leaves `dispatched` past
/// `size()` so later appends are never consumed (combat hits appear to "stop").
template <typename T>
void trim_dispatched_event_queue(std::vector<T>& events, std::size_t& dispatched,
                                 std::size_t max_keep) {
    if (dispatched > events.size())
        dispatched = events.size();
    if (max_keep == 0) {
        events.clear();
        dispatched = 0;
        return;
    }
    if (events.size() <= max_keep)
        return;
    const std::size_t remove = events.size() - max_keep;
    events.erase(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(remove));
    dispatched = dispatched > remove ? dispatched - remove : 0;
}

} // namespace engine
