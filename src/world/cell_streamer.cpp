#include "engine/world/cell_streamer.h"

#include <chrono>

namespace engine {
namespace {
EngineError stream_error(std::string code, std::string message) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Io, "streaming", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, "Validate the cell source and retry the streaming request.", make_correlation_id()};
}
}

CellStreamer::CellStreamer(CellLoader loader) : loader_(std::move(loader)) {}
CellStreamer::~CellStreamer() { cancel_all(); for (auto& item : pending_) item.second.future.wait(); }

void CellStreamer::cancel_all() { for (auto& item : pending_) item.second.cancelled->store(true, std::memory_order_release); }

Result<void> CellStreamer::request(CellCoord center, std::uint32_t radius) {
    if (!loader_) return Result<void>::failure(stream_error("STREAM-LOADER-MISSING", "No cell loader is configured"));
    if (radius > 32) return Result<void>::failure(stream_error("STREAM-RADIUS-INVALID", "Streaming radius exceeds safety limit 32"));
    ++generation_;
    desired_.clear();
    for (std::int32_t z = -static_cast<std::int32_t>(radius); z <= static_cast<std::int32_t>(radius); ++z)
        for (std::int32_t x = -static_cast<std::int32_t>(radius); x <= static_cast<std::int32_t>(radius); ++x)
            desired_.insert({center.x + x, center.z + z});
    for (auto& item : pending_) {
        if (desired_.count(item.first)) item.second.generation = generation_;
        else item.second.cancelled->store(true, std::memory_order_release);
    }
    for (auto it = loaded_.begin(); it != loaded_.end();) it = desired_.count(it->first) ? std::next(it) : loaded_.erase(it);
    for (const auto coordinate : desired_) {
        if (loaded_.count(coordinate) || pending_.count(coordinate)) continue;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        const auto current_generation = generation_;
        pending_[coordinate] = Pending{current_generation, cancelled,
            std::async(std::launch::async, [loader = loader_, coordinate, cancelled]() {
                return std::make_unique<Result<CellData>>(loader(coordinate, *cancelled));
            })};
    }
    return Result<void>::success();
}

std::vector<EngineError> CellStreamer::update() {
    std::vector<EngineError> errors;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++it; continue; }
        auto result = it->second.future.get();
        const bool current = it->second.generation == generation_ && desired_.count(it->first) && !it->second.cancelled->load();
        if (current && result && *result) {
            if (result->value().coordinate != it->first || result->value().schema_version != 1)
                errors.push_back(stream_error("STREAM-CELL-INVALID", "Loaded cell identity or schema does not match request " + it->first.str()));
            else loaded_[it->first] = std::move(result->value());
        } else if (current && result && !*result) errors.push_back(result->error());
        else if (current && !result) errors.push_back(stream_error("STREAM-RESULT-MISSING", "Cell loader returned no result"));
        it = pending_.erase(it);
    }
    return errors;
}

bool CellStreamer::loaded(CellCoord coordinate) const { return loaded_.find(coordinate) != loaded_.end(); }

} // namespace engine
