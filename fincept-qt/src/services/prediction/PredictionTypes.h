#pragma once

// Unified data model for the Prediction Markets tab (Polymarket, Kalshi, …).
//
// Fields shared across exchanges are first-class members. Exchange-scoped
// identifiers live in MarketKey. Exchange-specific flags (neg-risk, tick
// size, series ticker, etc.) live in `extras`. Each adapter translates its
// native structs into these types via its TypeMap translation unit.

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace fincept::services::prediction {

// ── Market keys ─────────────────────────────────────────────────────────────

/// Exchange-scoped identifier for a market. Opaque to the UI; interpreted
/// only by the owning adapter.
///
///   exchange_id  — "polymarket" | "kalshi"
///   market_id    — Polymarket: condition_id (0x…). Kalshi: market_ticker.
///   event_id     — Polymarket: event slug/id. Kalshi: event_ticker. May be empty.
///   asset_ids    — Polymarket: clob_token_ids (one per outcome).
///                  Kalshi: ["<ticker>:yes", "<ticker>:no"] synthetic IDs.
struct MarketKey {
    QString exchange_id;
    QString market_id;
    QString event_id;
    QStringList asset_ids;

    bool is_empty() const noexcept {
        return exchange_id.isEmpty() && market_id.isEmpty() && event_id.isEmpty();
    }
};

struct Outcome {
    QString name;      // "Yes" / "No" / free-form for multi-outcome markets.
    QString asset_id;  // Exchange-opaque ID for subscribing / ordering.
    double price = 0.0;  // 0.0–1.0 probability.
};

struct PredictionMarket {
    MarketKey key;
    QString question;
    QString description;
    QString category;
    QString image_url;
    QString end_date_iso;
    double volume = 0.0;
    double liquidity = 0.0;
    double open_interest = 0.0;
    bool active = false;
    bool closed = false;
    QVector<Outcome> outcomes;  // size ≥ 2; binary markets have 2.
    QStringList tags;
    QVariantMap extras;  // exchange-specific: "neg_risk", "tick_size", "series_ticker", etc.
};

struct PredictionEvent {
    MarketKey key;  // key.market_id is empty for pure events.
    QString title;
    QString description;
    QString category;
    double volume = 0.0;
    double liquidity = 0.0;
    bool active = false;
    bool closed = false;
    QVector<PredictionMarket> markets;
    QStringList tags;
    QVariantMap extras;
};

// ── Order book / price history / trades ─────────────────────────────────────

struct OrderLevel {
    double price = 0.0;
    double size = 0.0;
};

struct PredictionOrderBook {
    QString asset_id;
    QVector<OrderLevel> bids;  // highest first
    QVector<OrderLevel> asks;  // lowest first
    double tick_size = 0.01;
    double min_order_size = 0.0;
    qint64 last_update_ms = 0;
};

struct PricePoint {
    qint64 ts_ms = 0;
    double price = 0.0;
};

struct PriceHistory {
    QString asset_id;
    QVector<PricePoint> points;
};

struct PredictionTrade {
    QString asset_id;
    QString side;  // "BUY" | "SELL"
    double price = 0.0;
    double size = 0.0;
    qint64 ts_ms = 0;
    QString maker;  // Polymarket address or empty (Kalshi).
};

// ── Account / trading ───────────────────────────────────────────────────────

struct PredictionPosition {
    QString asset_id;
    QString market_id;
    QString outcome;  // resolved outcome name, e.g. "YES" / "NO" / custom.
    double size = 0.0;
    double avg_price = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double current_value = 0.0;
};

struct OpenOrder {
    QString order_id;
    QString asset_id;
    QString market_id;
    QString outcome;
    QString side;        // "BUY" | "SELL"
    QString order_type;  // "GTC" | "GTD" | "FOK" | "FAK" | "LIMIT" | "MARKET"
    double price = 0.0;
    double size = 0.0;
    double filled = 0.0;
    QString status;      // normalized: "LIVE" | "PARTIAL" | "FILLED" | "CANCELED"
    qint64 created_ms = 0;
    qint64 expires_ms = 0;
};

struct OrderRequest {
    MarketKey key;
    QString asset_id;
    QString side;        // "BUY" | "SELL"
    QString order_type;  // see OpenOrder::order_type
    double price = 0.0;
    double size = 0.0;
    qint64 expires_ms = 0;   // 0 = GTC
    QString client_order_id; // adapter fills if empty
    QVariantMap extras;      // per-exchange hints, e.g. {"tick_size": 0.01, "neg_risk": false}
};

struct OrderResult {
    bool ok = false;
    QString order_id;
    QString status;
    QString error_code;
    QString error_message;
    double filled = 0.0;
};

struct AccountBalance {
    double available = 0.0;    // USDC (Polymarket) or USD (Kalshi)
    double total_value = 0.0;  // includes open positions + collateral
    QString currency;          // "USDC" / "USD"
};

// ── Capability flags ────────────────────────────────────────────────────────

struct ExchangeCapabilities {
    bool has_events = true;
    bool has_multi_outcome = false;
    bool has_orderbook_ws = true;
    bool has_trade_ws = false;
    bool has_rewards = false;        // Polymarket liquidity rewards
    bool has_maker_rebates = false;  // Polymarket
    bool has_leaderboard = false;

    bool supports_limit_orders = true;
    bool supports_market_orders = true;
    bool supports_gtd = false;
    bool supports_fok = true;
    bool supports_fak = true;
    bool supports_decrease_order = false;  // Kalshi only
    bool supports_batch_orders = false;

    QString quote_currency = "USD";
    double min_order_size = 1.0;
    double default_tick_size = 0.01;
    int max_requests_per_sec = 10;
};

} // namespace fincept::services::prediction

Q_DECLARE_METATYPE(fincept::services::prediction::MarketKey)
Q_DECLARE_METATYPE(fincept::services::prediction::PredictionMarket)
Q_DECLARE_METATYPE(fincept::services::prediction::PredictionEvent)
Q_DECLARE_METATYPE(fincept::services::prediction::PredictionOrderBook)
Q_DECLARE_METATYPE(fincept::services::prediction::PriceHistory)
Q_DECLARE_METATYPE(fincept::services::prediction::PredictionTrade)
Q_DECLARE_METATYPE(fincept::services::prediction::PredictionPosition)
Q_DECLARE_METATYPE(fincept::services::prediction::OpenOrder)
Q_DECLARE_METATYPE(fincept::services::prediction::OrderResult)
Q_DECLARE_METATYPE(fincept::services::prediction::AccountBalance)
Q_DECLARE_METATYPE(QVector<fincept::services::prediction::PredictionMarket>)
Q_DECLARE_METATYPE(QVector<fincept::services::prediction::PredictionEvent>)
Q_DECLARE_METATYPE(QVector<fincept::services::prediction::PredictionTrade>)
Q_DECLARE_METATYPE(QVector<fincept::services::prediction::PredictionPosition>)
Q_DECLARE_METATYPE(QVector<fincept::services::prediction::OpenOrder>)
