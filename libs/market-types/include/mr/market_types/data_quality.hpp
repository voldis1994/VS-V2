#pragma once
#include <cstdint>
namespace mr {
enum class DataQualityFlag : std::uint32_t {
    None=0, Stale=1<<0, OutOfOrder=1<<1, Duplicate=1<<2, SequenceGap=1<<3,
    Crossed=1<<4, WideSpread=1<<5, MissingField=1<<6, Divergent=1<<7
};
using DataQualityFlags = std::uint32_t;
inline DataQualityFlags operator|(DataQualityFlag a, DataQualityFlag b) {
    return static_cast<DataQualityFlags>(a)|static_cast<DataQualityFlags>(b);
}
inline bool has_flag(DataQualityFlags f, DataQualityFlag flag) {
    return (f & static_cast<DataQualityFlags>(flag)) != 0;
}
}