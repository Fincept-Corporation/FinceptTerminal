#pragma once

#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionTypes.h"

#include <QVector>

namespace fincept::services::prediction::polymarket_map {

/// Stable adapter id used for MarketKey::exchange_id and hub topics.
inline constexpr const char* kExchangeId = "polymarket";

prediction::PredictionMarket to_prediction(const services::polymarket::Market& src);
prediction::PredictionEvent to_prediction(const services::polymarket::Event& src);
prediction::PredictionOrderBook to_prediction(const services::polymarket::OrderBook& src);
prediction::PriceHistory to_prediction(const services::polymarket::PriceHistory& src,
                                       const QString& asset_id);
prediction::PredictionTrade to_prediction(const services::polymarket::Trade& src);

QVector<prediction::PredictionMarket> to_prediction(const QVector<services::polymarket::Market>& src);
QVector<prediction::PredictionEvent> to_prediction(const QVector<services::polymarket::Event>& src);
QVector<prediction::PredictionTrade> to_prediction(const QVector<services::polymarket::Trade>& src);

} // namespace fincept::services::prediction::polymarket_map
