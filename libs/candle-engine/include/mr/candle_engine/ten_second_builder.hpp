#pragma once
#include "mr/candle_engine/candle_builder.hpp"
namespace mr { class TenSecondBuilder : public CandleBuilder { public: TenSecondBuilder() : CandleBuilder(Timeframe::Second10) {} }; }