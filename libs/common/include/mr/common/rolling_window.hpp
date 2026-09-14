#pragma once
#include "mr/common/ring_buffer.hpp"
#include <cstddef>
#include <vector>

namespace mr {

template <typename T, std::size_t Capacity>
class RollingWindow {
public:
    void push(const T& v) { buf_.push(v); }
    void push(T&& v) { buf_.push(std::move(v)); }
    [[nodiscard]] std::size_t size() const { return buf_.size(); }
    [[nodiscard]] bool empty() const { return buf_.empty(); }
    [[nodiscard]] const T& newest() const { return buf_.newest(); }
    [[nodiscard]] const T& at(std::size_t age) const { return buf_.at(age); }
    void clear() { buf_.clear(); }

    template <typename Fn>
    [[nodiscard]] std::vector<double> map_values(Fn fn) const {
        std::vector<double> out;
        for (std::size_t i = 0; i < size(); ++i) out.push_back(fn(at(i)));
        return out;
    }

private:
    RingBuffer<T, Capacity> buf_;
};

}  // namespace mr
