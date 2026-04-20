#pragma once

// Meta-type registration entry point for the DataHub payload types.
// Included from main.cpp at startup.
//
// Q_DECLARE_METATYPE lives in the header that declares each type (so moc
// sees the specialization before it instantiates the primary template for
// signal signatures in unity builds). This header only aggregates the
// includes and declares register_metatypes() for runtime registration.
//
// Adding a new payload type to the hub:
//   1. In the header that declares the type, add `#include <QMetaType>`
//      and `Q_DECLARE_METATYPE(T)` at global scope after the namespace
//      closes.
//   2. Include that header here (if not already via another include).
//   3. Add the matching qRegisterMetaType<T>() call in
//      DataHubMetaTypes.cpp::register_metatypes().

#include "screens/relationship_map/RelationshipMapTypes.h" // RelationshipData
#include "services/dbnomics/DBnomicsModels.h"            // DbnDataPoint
#include "services/economics/EconomicsService.h"        // EconomicsResult
#include "services/geopolitics/GeopoliticsTypes.h"       // NewsEvent, HDXDataset, UniqueCountry, UniqueCategory
#include "services/gov_data/GovDataService.h"           // GovDataResult
#include "services/maritime/MaritimeTypes.h"             // VesselData
#include "services/markets/MarketDataService.h"         // QuoteData, HistoryPoint, InfoData
#include "services/news/NewsService.h"                  // NewsArticle
#include "services/polymarket/PolymarketTypes.h"        // OrderBook
#include "services/prediction/PredictionTypes.h"        // PredictionOrderBook, PredictionMarket, …
#include "trading/TradingTypes.h"                       // TickerData, OrderBookData, Candle, TradeData, Broker*

#include <QMetaType>

// Types that are not declared in a dedicated type header above — declare
// them here so consumers only need to include DataHubMetaTypes.h.
Q_DECLARE_METATYPE(fincept::services::QuoteData)
Q_DECLARE_METATYPE(fincept::services::HistoryPoint)
Q_DECLARE_METATYPE(fincept::services::InfoData)
Q_DECLARE_METATYPE(QVector<fincept::services::HistoryPoint>)
Q_DECLARE_METATYPE(fincept::trading::TickerData)
Q_DECLARE_METATYPE(fincept::trading::OrderBookData)
Q_DECLARE_METATYPE(fincept::trading::Candle)
Q_DECLARE_METATYPE(fincept::trading::TradeData)

namespace fincept::datahub {

/// Registers every hub payload type with Qt's meta-type system so they
/// can flow through QVariant-based topic values and cross-thread queued
/// signal emissions. Must be called once at app start, after the
/// QApplication is constructed but before any producer/subscriber runs.
void register_metatypes();

} // namespace fincept::datahub
