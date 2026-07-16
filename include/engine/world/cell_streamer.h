#pragma once

#include "engine/world/world_partition.h"

#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace engine {

struct CellData {
    CellCoord coordinate;
    std::uint32_t schema_version = 1;
    std::string payload;
};

using CellLoader = std::function<Result<CellData>(CellCoord, const std::atomic_bool& cancelled)>;

class CellStreamer final {
public:
    explicit CellStreamer(CellLoader loader);
    ~CellStreamer();
    CellStreamer(const CellStreamer&) = delete;
    CellStreamer& operator=(const CellStreamer&) = delete;

    [[nodiscard]] Result<void> request(CellCoord center, std::uint32_t radius);
    [[nodiscard]] std::vector<EngineError> update();
    [[nodiscard]] bool loaded(CellCoord coordinate) const;
    [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_.size(); }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

private:
    struct Pending {
        std::uint64_t generation = 0;
        std::shared_ptr<std::atomic_bool> cancelled;
        std::future<std::unique_ptr<Result<CellData>>> future;
    };
    void cancel_all();
    CellLoader loader_;
    std::uint64_t generation_ = 0;
    std::set<CellCoord> desired_;
    std::map<CellCoord, CellData> loaded_;
    std::map<CellCoord, Pending> pending_;
};

} // namespace engine
