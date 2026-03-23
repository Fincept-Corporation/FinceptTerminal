#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::polymarket {

// ── Market (from Gamma API) ─────────────────────────────────────────────────

struct Outcome {
    QString name;
    double price = 0.0; // 0.0–1.0
};

struct Market {
    int id = 0;
    QString question;
    QString slug;
    QString description;
    QString condition_id;
    QString image;
    double volume = 0.0;
    double liquidity = 0.0;
    bool active = false;
    bool closed = false;
    QString end_date;
    QString category;
    QVector<Outcome> outcomes;
    QStringList clob_token_ids;

    static Market from_json(const QJsonObject& obj);
};

// ── Event (from Gamma API) ──────────────────────────────────────────────────

struct Event {
    int id = 0;
    QString title;
    QString slug;
    QString description;
    QString image;
    double volume = 0.0;
    double liquidity = 0.0;
    bool active = false;
    bool closed = false;
    QString end_date;
    QVector<Market> markets;
    QStringList tags;

    static Event from_json(const QJsonObject& obj);
};

// ── Order Book (from CLOB API) ──────────────────────────────────────────────

struct OrderLevel {
    double price = 0.0;
    double size = 0.0;
};

struct OrderBook {
    QString market;
    QString asset_id;
    QVector<OrderLevel> bids;
    QVector<OrderLevel> asks;
    double tick_size = 0.01;
    double min_order_size = 0.01;
    bool neg_risk = false;

    static OrderBook from_json(const QJsonObject& obj);
};

// ── Price History (from CLOB API) ───────────────────────────────────────────

struct PricePoint {
    int64_t timestamp = 0; // unix seconds
    double price = 0.0;
};

struct PriceHistory {
    QVector<PricePoint> points;

    static PriceHistory from_json(const QJsonObject& obj);
};

// ── Trade (from Data API) ───────────────────────────────────────────────────

struct Trade {
    QString side;       // BUY or SELL
    double price = 0.0;
    double size = 0.0;
    int64_t timestamp = 0;
    QString condition_id;

    static Trade from_json(const QJsonObject& obj);
};

// ── Price Summary (from CLOB API) ───────────────────────────────────────────

struct PriceSummary {
    double midpoint = 0.0;
    double spread = 0.0;
    double best_bid = 0.0;
    double best_ask = 0.0;
    double last_trade_price = 0.0;
};

// ── Tag (from Gamma API) ────────────────────────────────────────────────────

struct Tag {
    QString label;
    QString slug;
};

} // namespace fincept::services::polymarket
