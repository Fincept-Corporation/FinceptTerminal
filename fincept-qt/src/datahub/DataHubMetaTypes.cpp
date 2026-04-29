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

    // Prediction Markets (Polymarket, Kalshi, …)
    qRegisterMetaType<fincept::services::prediction::MarketKey>("fincept::services::prediction::MarketKey");
    qRegisterMetaType<fincept::services::prediction::PredictionMarket>("fincept::services::prediction::PredictionMarket");
    qRegisterMetaType<fincept::services::prediction::PredictionEvent>("fincept::services::prediction::PredictionEvent");
    qRegisterMetaType<fincept::services::prediction::PredictionOrderBook>("fincept::services::prediction::PredictionOrderBook");
    qRegisterMetaType<fincept::services::prediction::PriceHistory>("fincept::services::prediction::PriceHistory");
    qRegisterMetaType<fincept::services::prediction::PredictionTrade>("fincept::services::prediction::PredictionTrade");
    qRegisterMetaType<fincept::services::prediction::PredictionPosition>("fincept::services::prediction::PredictionPosition");
    qRegisterMetaType<fincept::services::prediction::OpenOrder>("fincept::services::prediction::OpenOrder");
    qRegisterMetaType<fincept::services::prediction::OrderResult>("fincept::services::prediction::OrderResult");
    qRegisterMetaType<fincept::services::prediction::AccountBalance>("fincept::services::prediction::AccountBalance");
    qRegisterMetaType<QVector<fincept::services::prediction::PredictionMarket>>("QVector<fincept::services::prediction::PredictionMarket>");
    qRegisterMetaType<QVector<fincept::services::prediction::PredictionEvent>>("QVector<fincept::services::prediction::PredictionEvent>");
    qRegisterMetaType<QVector<fincept::services::prediction::PredictionTrade>>("QVector<fincept::services::prediction::PredictionTrade>");
    qRegisterMetaType<QVector<fincept::services::prediction::PredictionPosition>>("QVector<fincept::services::prediction::PredictionPosition>");
    qRegisterMetaType<QVector<fincept::services::prediction::OpenOrder>>("QVector<fincept::services::prediction::OpenOrder>");
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

    // Crypto / Wallet (Phase 1 + Phase 2 Stage 2A.5 multi-token holdings)
    qRegisterMetaType<fincept::wallet::WalletBalance>("fincept::wallet::WalletBalance");
    qRegisterMetaType<fincept::wallet::TokenHolding>("fincept::wallet::TokenHolding");
    qRegisterMetaType<fincept::wallet::TokenPrice>("fincept::wallet::TokenPrice");
    qRegisterMetaType<fincept::wallet::TokenMetadata>("fincept::wallet::TokenMetadata");
    // FncptPrice is a typedef of TokenPrice — register its name as an alias so
    // Qt's metatype lookup serves either spelling.
    qRegisterMetaType<fincept::wallet::FncptPrice>("fincept::wallet::FncptPrice");
    // Phase 2 §2B activity feed — vector form is what publishes on the topic.
    qRegisterMetaType<fincept::wallet::ParsedActivity>("fincept::wallet::ParsedActivity");
    qRegisterMetaType<QVector<fincept::wallet::ParsedActivity>>(
        "QVector<fincept::wallet::ParsedActivity>");
    // Phase 2 §2C fee-discount eligibility.
    qRegisterMetaType<fincept::wallet::FncptDiscount>("fincept::wallet::FncptDiscount");

    LOG_INFO("DataHub", "Registered 42 payload meta-types");
}

} // namespace fincept::datahub
