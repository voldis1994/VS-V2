#pragma once
#include <string>
#include <variant>
#include <utility>

namespace mr {

template <typename T>
struct Result {
    bool ok{false};
    T value{};
    std::string error;

    static Result success(T v) { return Result{true, std::move(v), {}}; }
    static Result failure(std::string msg) { return Result{false, {}, std::move(msg)}; }
    [[nodiscard]] explicit operator bool() const { return ok; }
};

template <>
struct Result<void> {
    bool ok{false};
    std::string error;
    static Result success() { return Result{true, {}}; }
    static Result failure(std::string msg) { return Result{false, std::move(msg)}; }
    [[nodiscard]] explicit operator bool() const { return ok; }
};

}  // namespace mr
