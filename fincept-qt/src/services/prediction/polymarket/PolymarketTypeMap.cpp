#include "services/prediction/polymarket/PolymarketTypeMap.h"

namespace fincept::services::prediction::polymarket_map {

namespace pm = fincept::services::polymarket;
namespace pr = fincept::services::prediction;

static pr::MarketKey make_market_key(const pm::Market& m) {
    pr::MarketKey k;
    k.exchange_id = QString::fromLatin1(kExchangeId);
    k.market_id = m.condition_id;
    k.event_id = m.event_id > 0 ? QString::number(m.event_id) : QString();
    k.asset_ids = m.clob_token_ids;
    return k;
}

pr::PredictionMarket to_prediction(const pm::Market& src) {
    pr::PredictionMarket out;
    out.key = make_market_key(src);
    out.question = src.question;
    out.description = src.description;
    out.category = src.category;
    out.image_url = src.image;
    out.end_date_iso = src.end_date;
    out.volume = src.volume;
    out.liquidity = src.liquidity;
    out.active = src.active;
    out.closed = src.closed;
    out.tags = src.tags;

    out.outcomes.reserve(src.outcomes.size());
    for (int i = 0; i < src.outcomes.size(); ++i) {
        pr::Outcome o;
        o.name = src.outcomes[i].name;
        o.price = src.outcomes[i].price;
        o.asset_id = (i < src.clob_token_ids.size()) ? src.clob_token_ids[i] : QString();
        out.outcomes.push_back(o);
    }
    return out;
}

pr::PredictionEvent to_prediction(const pm::Event& src) {
    pr::PredictionEvent out;
    out.key.exchange_id = QString::fromLatin1(kExchangeId);
    out.key.event_id = src.id > 0 ? QString::number(src.id) : src.slug;
    out.title = src.title;
    out.description = src.description;
    out.category = src.category;
    out.volume = src.volume;
    out.liquidity = src.liquidity;
    out.active = src.active;
    out.closed = src.closed;
    out.tags = src.tags;
    out.markets = to_prediction(src.markets);
    return out;
}

pr::PredictionOrderBook to_prediction(const pm::OrderBook& src) {
    pr::PredictionOrderBook out;
    out.asset_id = src.asset_id;
    out.tick_size = src.tick_size;
    out.min_order_size = src.min_order_size;
    out.bids.reserve(src.bids.size());
    for (const auto& l : src.bids) out.bids.push_back({l.price, l.size});
    out.asks.reserve(src.asks.size());
    for (const auto& l : src.asks) out.asks.push_back({l.price, l.size});
    return out;
}

pr::PriceHistory to_prediction(const pm::PriceHistory& src, const QString& asset_id) {
    pr::PriceHistory out;
    out.asset_id = asset_id;
    out.points.reserve(src.points.size());
    for (const auto& p : src.points) out.points.push_back({p.timestamp * 1000, p.price});
    return out;
}

pr::PredictionTrade to_prediction(const pm::Trade& src) {
    pr::PredictionTrade out;
    out.asset_id = src.condition_id;
    out.side = src.side;
    out.price = src.price;
    out.size = src.size;
    out.ts_ms = src.timestamp * 1000;
    return out;
}

QVector<pr::PredictionMarket> to_prediction(const QVector<pm::Market>& src) {
    QVector<pr::PredictionMarket> out;
    out.reserve(src.size());
    for (const auto& m : src) out.push_back(to_prediction(m));
    return out;
}

QVector<pr::PredictionEvent> to_prediction(const QVector<pm::Event>& src) {
    QVector<pr::PredictionEvent> out;
    out.reserve(src.size());
    for (const auto& e : src) out.push_back(to_prediction(e));
    return out;
}

QVector<pr::PredictionTrade> to_prediction(const QVector<pm::Trade>& src) {
    QVector<pr::PredictionTrade> out;
    out.reserve(src.size());
    for (const auto& t : src) out.push_back(to_prediction(t));
    return out;
}

} // namespace fincept::services::prediction::polymarket_map
