#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QNetworkAccessManager>
#include <QObject>

#include <functional>

namespace fincept::services::polymarket {

/// Singleton service for all Polymarket API interactions.
/// Uses three public APIs:
///   - Gamma API  (gamma-api.polymarket.com) — markets, events, tags, comments, teams, search
///   - CLOB API   (clob.polymarket.com)      — order book, prices, price history
///   - Data API   (data-api.polymarket.com)   — trades, top holders, leaderboard, activity
class PolymarketService : public QObject {
    Q_OBJECT
  public:
    static PolymarketService& instance();

    // ── Gamma API ────────────────────────────────────────────────────────

    void fetch_markets(const QString& sort_by = "volume", int limit = 100, int offset = 0, bool closed = false);
    void fetch_markets_by_tag(const QString& tag, const QString& sort_by = "volume", int limit = 100, int offset = 0);
    void fetch_market_by_id(int id);
    void search_markets(const QString& query, int limit = 50);
    void unified_search(const QString& query);
    void fetch_events(const QString& sort_by = "volume", int limit = 100, int offset = 0, bool closed = false);
    void fetch_event_by_id(int id);
    void fetch_related_markets(int event_id);
    void fetch_tags();
    void fetch_comments(const QString& condition_id, int limit = 50);
    void fetch_teams(const QString& league = "");

    // ── CLOB API ─────────────────────────────────────────────────────────

    void fetch_order_book(const QString& token_id);
    void fetch_price_history(const QString& token_id, const QString& interval = "1d", int fidelity = 5);
    void fetch_price_summary(const QString& token_id);

    // ── Data API ─────────────────────────────────────────────────────────

    void fetch_trades(const QString& condition_id, int limit = 100);
    void fetch_top_holders(const QString& condition_id, int limit = 20);
    void fetch_leaderboard(int limit = 50);
    void fetch_activity(const QString& condition_id, int limit = 50);
    void fetch_live_volume(const QString& event_id);
    void fetch_open_interest(const QStringList& condition_ids);

  signals:
    // Gamma
    void markets_ready(const QVector<Market>& markets);
    void events_ready(const QVector<Event>& events);
    void tags_ready(const QVector<Tag>& tags);
    void market_detail_ready(const Market& market);
    void event_detail_ready(const Event& event);
    void related_markets_ready(const QVector<Market>& markets);
    void comments_ready(const QVector<Comment>& comments);
    void teams_ready_list(const QVector<Team>& teams);
    void search_results_ready(const QVector<Market>& markets, const QVector<Event>& events);

    // CLOB
    void order_book_ready(const OrderBook& book);
    void price_history_ready(const PriceHistory& history);
    void price_summary_ready(const PriceSummary& summary);

    // Data
    void trades_ready(const QVector<Trade>& trades);
    void top_holders_ready(const QVector<TopHolder>& holders);
    void leaderboard_ready(const QVector<LeaderboardEntry>& entries);
    void activity_ready(const QVector<Activity>& activities);
    void live_volume_ready(const LiveVolume& vol);
    void open_interest_ready(const QVector<OpenInterest>& oi);

    // Errors
    void request_error(const QString& context, const QString& message);

  private:
    PolymarketService();

    using JsonCallback = std::function<void(const QJsonDocument&)>;

    void get_gamma(const QString& path, JsonCallback on_success, const QString& error_ctx = "Gamma");
    void get_clob(const QString& path, JsonCallback on_success, const QString& error_ctx = "CLOB");
    void get_data(const QString& path, JsonCallback on_success, const QString& error_ctx = "Data");
    void get_json(QNetworkAccessManager* nam, const QString& url, JsonCallback on_success, const QString& error_ctx);

    QNetworkAccessManager* gamma_nam_ = nullptr;
    QNetworkAccessManager* clob_nam_ = nullptr;
    QNetworkAccessManager* data_nam_ = nullptr;

    static constexpr int kTagsTtlSec = 5 * 60;
    static constexpr int kLeaderboardTtlSec = 5 * 60;
    static constexpr int kMarketsTtlSec = 2 * 60;
    static constexpr int kEventsTtlSec = 2 * 60;
};

} // namespace fincept::services::polymarket
