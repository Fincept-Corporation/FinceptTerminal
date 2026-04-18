#include "datahub/DataHubMetaTypes.h"

#include "core/logging/Logger.h"

namespace fincept::datahub {

void register_metatypes() {
    qRegisterMetaType<fincept::services::QuoteData>("fincept::services::QuoteData");
    qRegisterMetaType<fincept::services::HistoryPoint>("fincept::services::HistoryPoint");
    qRegisterMetaType<fincept::services::InfoData>("fincept::services::InfoData");
    qRegisterMetaType<fincept::services::NewsArticle>("fincept::services::NewsArticle");
    qRegisterMetaType<fincept::services::EconomicsResult>("fincept::services::EconomicsResult");
    qRegisterMetaType<QVector<fincept::services::HistoryPoint>>("QVector<fincept::services::HistoryPoint>");
    qRegisterMetaType<QVector<fincept::services::NewsArticle>>("QVector<fincept::services::NewsArticle>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<fincept::trading::TickerData>("fincept::trading::TickerData");
    qRegisterMetaType<fincept::trading::OrderBookData>("fincept::trading::OrderBookData");
    qRegisterMetaType<fincept::trading::Candle>("fincept::trading::Candle");
    qRegisterMetaType<fincept::trading::TradeData>("fincept::trading::TradeData");
    qRegisterMetaType<fincept::services::polymarket::OrderBook>("fincept::services::polymarket::OrderBook");
    qRegisterMetaType<fincept::services::DbnDataPoint>("fincept::services::DbnDataPoint");
    qRegisterMetaType<fincept::services::GovDataResult>("fincept::services::GovDataResult");
    qRegisterMetaType<fincept::trading::BrokerPosition>("fincept::trading::BrokerPosition");
    qRegisterMetaType<fincept::trading::BrokerHolding>("fincept::trading::BrokerHolding");
    qRegisterMetaType<fincept::trading::BrokerOrderInfo>("fincept::trading::BrokerOrderInfo");
    qRegisterMetaType<fincept::trading::BrokerQuote>("fincept::trading::BrokerQuote");
    qRegisterMetaType<fincept::trading::BrokerFunds>("fincept::trading::BrokerFunds");
    qRegisterMetaType<QVector<fincept::trading::BrokerPosition>>("QVector<fincept::trading::BrokerPosition>");
    qRegisterMetaType<QVector<fincept::trading::BrokerHolding>>("QVector<fincept::trading::BrokerHolding>");
    qRegisterMetaType<QVector<fincept::trading::BrokerOrderInfo>>("QVector<fincept::trading::BrokerOrderInfo>");
    qRegisterMetaType<QVector<fincept::trading::BrokerQuote>>("QVector<fincept::trading::BrokerQuote>");

    // Phase 8 — Geopolitics / Maritime / RelationshipMap
    qRegisterMetaType<fincept::services::geo::NewsEvent>("fincept::services::geo::NewsEvent");
    qRegisterMetaType<fincept::services::geo::HDXDataset>("fincept::services::geo::HDXDataset");
    qRegisterMetaType<fincept::services::geo::UniqueCountry>("fincept::services::geo::UniqueCountry");
    qRegisterMetaType<fincept::services::geo::UniqueCategory>("fincept::services::geo::UniqueCategory");
    qRegisterMetaType<QVector<fincept::services::geo::NewsEvent>>("QVector<fincept::services::geo::NewsEvent>");
    qRegisterMetaType<QVector<fincept::services::geo::HDXDataset>>("QVector<fincept::services::geo::HDXDataset>");
    qRegisterMetaType<QVector<fincept::services::geo::UniqueCountry>>("QVector<fincept::services::geo::UniqueCountry>");
    qRegisterMetaType<QVector<fincept::services::geo::UniqueCategory>>("QVector<fincept::services::geo::UniqueCategory>");
    qRegisterMetaType<fincept::services::maritime::VesselData>("fincept::services::maritime::VesselData");
    qRegisterMetaType<QVector<fincept::services::maritime::VesselData>>("QVector<fincept::services::maritime::VesselData>");
    qRegisterMetaType<fincept::relmap::RelationshipData>("fincept::relmap::RelationshipData");

    LOG_INFO("DataHub", "Registered 35 payload meta-types");
}

} // namespace fincept::datahub
