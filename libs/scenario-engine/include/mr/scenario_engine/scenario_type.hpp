#pragma once
namespace mr {
enum class ScenarioType : std::uint8_t {
    Continuation=0, Pullback=1, Range=2, Breakout=3, Failure=4, Reversal=5
};
}