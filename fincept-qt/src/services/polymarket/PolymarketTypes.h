#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
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
    QStringList tags;
    int event_id = 0;

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
    QString category;

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
    int64_t timestamp = 0;
    double price = 0.0;
};

struct PriceHistory {
    QVector<PricePoint> points;

    static PriceHistory from_json(const QJsonObject& obj);
};

// ── Trade (from Data API) ───────────────────────────────────────────────────

struct Trade {
    QString side;
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

// ── Top Holder (from Data API) ──────────────────────────────────────────────

struct TopHolder {
    QString address;
    QString display_name;
    double position_size = 0.0;
    double entry_price = 0.0;
    int rank = 0;

    static TopHolder from_json(const QJsonObject& obj);
};

// ── Leaderboard Entry (from Data API) ───────────────────────────────────────

struct LeaderboardEntry {
    QString address;
    QString display_name;
    QString profile_image;
    double pnl = 0.0;
    double volume = 0.0;
    int num_trades = 0;
    int rank = 0;

    static LeaderboardEntry from_json(const QJsonObject& obj);
};

// ── Activity (from Data API) ────────────────────────────────────────────────

struct Activity {
    QString type; // TRADE, SPLIT, MERGE, REDEEM, REWARD
    QString address;
    double amount = 0.0;
    double usdc_size = 0.0;
    double price = 0.0;
    int64_t timestamp = 0;
    QString condition_id;
    QString title;
    QString outcome;

    static Activity from_json(const QJsonObject& obj);
};

// ── Comment (from Gamma API) ────────────────────────────────────────────────

struct Comment {
    int id = 0;
    QString body;
    QString author;
    QString author_address;
    int64_t created_at = 0;
    int parent_id = 0;
    int likes = 0;

    static Comment from_json(const QJsonObject& obj);
};

// ── Series (from Gamma API) ─────────────────────────────────────────────────

struct Series {
    int id = 0;
    QString title;
    QString slug;
    QVector<Event> events;

    static Series from_json(const QJsonObject& obj);
};

// ── Team (from Gamma API) ───────────────────────────────────────────────────

struct Team {
    QString name;
    QString league;
    QString abbreviation;
    QString image;

    static Team from_json(const QJsonObject& obj);
};

// ── Open Interest (from Data API) ───────────────────────────────────────────

struct OpenInterest {
    QString condition_id;
    double open_interest = 0.0;

    static OpenInterest from_json(const QJsonObject& obj);
};

// ── Live Volume (from Data API) ─────────────────────────────────────────────

struct LiveVolume {
    QString event_id;
    double volume_24h = 0.0;

    static LiveVolume from_json(const QJsonObject& obj);
};

// ── WebSocket Market Update ─────────────────────────────────────────────────

struct WsMarketUpdate {
    QString asset_id;
    double price = 0.0;
    QVector<OrderLevel> bids;
    QVector<OrderLevel> asks;
    int64_t timestamp = 0;

    static WsMarketUpdate from_json(const QJsonObject& obj);
};

} // namespace fincept::services::polymarket
