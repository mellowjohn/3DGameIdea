#pragma once

#include "engine/core/error.h"

#include <stdexcept>
#include <utility>
#include <variant>

namespace engine {

template <typename T>
class Result {
public:
    static Result success(const T& value) { return Result(value); }
    static Result success(T&& value) { return Result(std::move(value)); }
    static Result failure(EngineError error) { return Result(std::move(error)); }

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] T& value() { return std::get<T>(storage_); }
    [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
    [[nodiscard]] EngineError& error() { return std::get<EngineError>(storage_); }
    [[nodiscard]] const EngineError& error() const { return std::get<EngineError>(storage_); }

private:
    explicit Result(const T& value) : storage_(std::in_place_type<T>, value) {}
    explicit Result(T&& value) : storage_(std::in_place_type<T>, std::move(value)) {}
    explicit Result(EngineError error) : storage_(std::in_place_type<EngineError>, std::move(error)) {}
    std::variant<T, EngineError> storage_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(); }
    static Result failure(EngineError error) { return Result(std::move(error)); }
    [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] EngineError& error() { return error_.value(); }
    [[nodiscard]] const EngineError& error() const { return error_.value(); }

private:
    Result() = default;
    explicit Result(EngineError error) : error_(std::move(error)) {}
    std::optional<EngineError> error_;
};

} // namespace engine
