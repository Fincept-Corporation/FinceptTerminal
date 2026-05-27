// src/algo_engine/AlgoEngineTypes.cpp
#include "algo_engine/AlgoEngineTypes.h"

namespace {
const int reg_algo_metrics = qRegisterMetaType<fincept::algo::AlgoMetrics>("fincept::algo::AlgoMetrics");
const int reg_algo_trade = qRegisterMetaType<fincept::algo::AlgoTradeRecord>("fincept::algo::AlgoTradeRecord");
const int reg_ohlcv = qRegisterMetaType<fincept::algo::OhlcvCandle>("fincept::algo::OhlcvCandle");
} // namespace
