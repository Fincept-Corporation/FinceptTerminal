#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

#include <functional>

namespace fincept::services::polymarket {

/// Singleton service for all Polymarket API interactions.
/// Uses three public APIs:
///   - Gamma API  (gamma-api.polymarket.com) — markets, events, tags
///   - CLOB API   (clob.polymarket.com)      — order book, prices, price history
///   - Data API   (data-api.polymarket.com)   — public trades
///
/// All methods are async; results delivered via signals or callbacks.
class PolymarketService : public QObject {
    Q_OBJECT
  public:
    static PolymarketService& instance();

    // ── Gamma API ────────────────────────────────────────────────────────

    /// Fetch active markets sorted by a field.
    /// sort_by: "volume", "liquidity", "startDate"
    void fetch_markets(const QString& sort_by = "volume", int limit = 100,
                       int offset = 0, bool closed = false);

    /// Search markets by query string.
    void search_markets(const QString& query, int limit = 50);

    /// Fetch events (each may contain nested markets).
    void fetch_events(const QString& sort_by = "volume", int limit = 100,
                      int offset = 0, bool closed = false);

    /// Fetch all available tags.
    void fetch_tags();

    // ── CLOB API ─────────────────────────────────────────────────────────

    /// Fetch order book for a token.
    void fetch_order_book(const QString& token_id);

    /// Fetch price history for a token.
    /// interval: "1h", "6h", "1d", "1w", "1m", "max"
    void fetch_price_history(const QString& token_id, const QString& interval = "1d",
                             int fidelity = 5);

    /// Fetch price summary (midpoint, spread, last trade, best bid/ask).
    void fetch_price_summary(const QString& token_id);

    // ── Data API ─────────────────────────────────────────────────────────

    /// Fetch public trades for a market (by condition_id).
    void fetch_trades(const QString& condition_id, int limit = 100);

  signals:
    void markets_ready(const QVector<Market>& markets);
    void events_ready(const QVector<Event>& events);
    void tags_ready(const QVector<Tag>& tags);
    void order_book_ready(const OrderBook& book);
    void price_history_ready(const PriceHistory& history);
    void price_summary_ready(const PriceSummary& summary);
    void trades_ready(const QVector<Trade>& trades);
    void request_error(const QString& context, const QString& message);

  private:
    PolymarketService();

    using JsonCallback = std::function<void(const QJsonDocument&)>;

    void get_gamma(const QString& path, JsonCallback on_success,
                   const QString& error_ctx = "Gamma");
    void get_clob(const QString& path, JsonCallback on_success,
                  const QString& error_ctx = "CLOB");
    void get_data(const QString& path, JsonCallback on_success,
                  const QString& error_ctx = "Data");
    void get_json(QNetworkAccessManager* nam, const QString& url,
                  JsonCallback on_success, const QString& error_ctx);

    QNetworkAccessManager* gamma_nam_ = nullptr;
    QNetworkAccessManager* clob_nam_ = nullptr;
    QNetworkAccessManager* data_nam_ = nullptr;
};

} // namespace fincept::services::polymarket
