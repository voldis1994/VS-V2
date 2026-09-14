#pragma once
#include "mr/candle_engine/candle_builder.hpp"
namespace mr { class SecondBuilder : public CandleBuilder { public: SecondBuilder() : CandleBuilder(Timeframe::Second1) {} }; }