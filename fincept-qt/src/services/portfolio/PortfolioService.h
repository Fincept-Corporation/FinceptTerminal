// src/services/portfolio/PortfolioService.h
#pragma once
#include "python/PythonRunner.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataService.h"

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace fincept {
struct CustomIndex; // storage/repositories/CustomIndexRepository.h
}

namespace fincept::services {

/// Singleton service managing portfolio data, live quotes, and computed metrics.
/// Screens connect to signals — never call DB or market data directly.
class PortfolioService : public QObject {
    Q_OBJECT
  public:
    static PortfolioService& instance();

    // ── Portfolio CRUD ───────────────────────────────────────────────────────
    void load_portfolios();
    /// `broker_account_id` links the new portfolio to a broker account so
    /// PortfolioService routes live quotes via that broker instead of yfinance.
    /// Empty for manual / JSON imports — yfinance fallback applies.
    void create_portfolio(const QString& name, const QString& owner, const QString& currency,
                          const QString& description = {}, const QString& broker_account_id = {});
    void delete_portfolio(const QString& id);

    // ── Summary (assets + live quotes) ───────────────────────────────────────
    void load_summary(const QString& portfolio_id);
    void refresh_summary(const QString& portfolio_id); // invalidates cache first

    // ── Asset operations ─────────────────────────────────────────────────────
    /// `broker_symbol`+`exchange` are stored alongside the canonical
    /// yfinance-format `symbol` so broker quote calls can use the native
    /// pair. Both empty for manual / JSON imports. `date` is the trade date
    /// (YYYY-MM-DD or ISO date-time; empty = now). Returns false (and records
    /// nothing) on a non-positive quantity/price or a database error.
    bool add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                   const QString& date = {}, const QString& broker_symbol = {}, const QString& exchange = {});
    /// Records a SELL and reduces (or closes) the position. Refuses — returning
    /// false and recording nothing — when the symbol is not held or @p qty
    /// exceeds the quantity held: an over-sell used to close the position and
    /// still write a SELL for the larger quantity, leaving the ledger short.
    bool sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                    const QString& date = {});

    // ── Transactions ─────────────────────────────────────────────────────────
    void load_transactions(const QString& portfolio_id, int limit = 50);
    /// Full ledger, newest first (local SQLite read — for exports/edits).
    QVector<portfolio::Transaction> ledger(const QString& portfolio_id) const;
    /// Edits a ledger row and re-derives that symbol's position from the
    /// ledger, so holdings and history can no longer disagree.
    bool update_transaction(const QString& portfolio_id, const QString& id, double qty, double price,
                            const QString& date, const QString& notes = {});
    bool delete_transaction(const QString& id, const QString& portfolio_id);

    // ── Dividend ──────────────────────────────────────────────────────────────
    bool record_dividend(const QString& portfolio_id, const QString& symbol, double qty, double amount_per_share,
                         double total, const QString& date = {}, const QString& notes = {});

    // ── Market analytics (one Python call) ────────────────────────────────────
    /// Downloads every holding + the currency benchmark + the 13-week T-bill in
    /// ONE threaded yfinance call (scripts/portfolio_metrics.py) and derives the
    /// correlation matrix, benchmark history, risk-free rate and ex-ante risk at
    /// current weights. Results are cached per (portfolio, day, holdings,
    /// benchmark); repeats inside the TTL replay from memory and identical
    /// in-flight requests are collapsed. Emits market_analytics_loaded().
    void fetch_market_analytics(const portfolio::PortfolioSummary& summary, bool force = false);
    /// Last analytics delivered for @p portfolio_id (empty portfolio_id if none).
    portfolio::MarketAnalytics last_market_analytics(const QString& portfolio_id) const;

    /// Pick a default benchmark from a portfolio currency (CAD → ^GSPTSE,
    /// USD → SPY, GBP → ^FTSE, EUR → ^STOXX50E, AUD → ^AXJO, INR → ^NSEI,
    /// JPY → ^N225, HKD → ^HSI). Falls back to SPY for unknown currencies.
    static QString default_benchmark_for_currency(const QString& currency);

    /// Trailing trading days behind the holdings correlation matrix.
    static constexpr int kCorrelationWindowDays = 63;

    /// Annual risk-free rate currently used for Sharpe/Sortino/alpha, and a
    /// human-readable description of where it came from.
    double risk_free_rate() const { return rf_rate_; }
    QString risk_free_source() const { return rf_source_; }

    // ── Metrics ──────────────────────────────────────────────────────────────
    void compute_metrics(const portfolio::PortfolioSummary& summary);

    /// Build the flow-adjusted (time-weighted) return series from NAV
    /// snapshots and the transaction ledger. Exposed for tests/exports.
    static QVector<portfolio::TwrPoint> build_twr_series(const QVector<portfolio::PortfolioSnapshot>& snapshots,
                                                         const QVector<portfolio::Transaction>& transactions);

    // ── Import / Export ──────────────────────────────────────────────────────
    void export_csv(const QString& portfolio_id, const QString& file_path);
    void export_json(const QString& portfolio_id, const QString& file_path);
    /// Accepts the terminal's export format ({portfolio_name, transactions[]})
    /// AND the holdings-snapshot shape ({name, holdings[]}). The whole file is
    /// validated before anything is written; a rejected file reports every
    /// problem found (row-by-row) instead of a generic "unsupported format".
    void import_json(const QString& file_path, portfolio::ImportMode mode, const QString& merge_target_id = {});

    // ── Snapshots (performance history) ─────────────────────────────────────
    void load_snapshots(const QString& portfolio_id, int days = 365);

    /// Reconstruct daily NAV from yfinance OHLC for the current holdings and
    /// upsert one row per trading day into portfolio_snapshots. This is what
    /// gives Beta and MDD a real time series on a freshly imported portfolio
    /// — without it both metrics show "—" until the user keeps the app open
    /// across enough days for daily snapshots to accumulate.
    /// Period: e.g. "1mo" / "6mo" / "1y" / "5y" (yfinance period strings).
    /// Emits history_backfilled(portfolio_id, point_count) on success.
    void backfill_history(const QString& portfolio_id, const QString& period = "1y");

    // ── Cache control ────────────────────────────────────────────────────────
    void invalidate_cache(const QString& portfolio_id);

    // ── Custom indices ───────────────────────────────────────────────────────
    /// Current level of @p idx given the live prices in @p summary. A
    /// constituent with no live price falls back to its creation price, so an
    /// index built on a portfolio the summary does not describe reads as its
    /// base value rather than garbage. Shared by the Indices view (creation)
    /// and the per-refresh recorder so the two can never disagree.
    static double compute_custom_index_value(const fincept::CustomIndex& idx,
                                             const portfolio::PortfolioSummary& summary);

  signals:
    void portfolios_loaded(QVector<portfolio::Portfolio> portfolios);
    void portfolio_created(portfolio::Portfolio portfolio);
    void portfolio_deleted(QString id);

    void summary_loaded(portfolio::PortfolioSummary summary);
    void summary_error(QString portfolio_id, QString error);

    void transactions_loaded(QVector<portfolio::Transaction> transactions);

    void metrics_computed(portfolio::ComputedMetrics metrics);
    void snapshots_loaded(QString portfolio_id, QVector<portfolio::PortfolioSnapshot> snapshots);

    void asset_added(QString portfolio_id);
    void asset_sold(QString portfolio_id);
    /// A ledger row was edited or deleted and the affected position re-derived.
    void ledger_changed(QString portfolio_id);
    /// A user-initiated write was refused or failed (over-sell, bad input, DB
    /// error). Previously these were log-only and the action looked ignored.
    void operation_failed(QString portfolio_id, QString message);

    void export_complete(QString file_path);
    /// An export could not be written (unreadable portfolio or unwritable
    /// path). Previously this was log-only, so the user saw nothing happen.
    void export_failed(QString file_path, QString error);
    void import_complete(portfolio::ImportResult result);

    /// fetch_market_analytics() finished for @p portfolio_id. On success read
    /// the result (correlation matrix "SYM1|SYM2", benchmark closes, risk-free
    /// rate, ex-ante risk) with last_market_analytics(). On failure `ok` is
    /// false, `error` says why, and the previous result (if any) is still
    /// served by last_market_analytics().
    void market_analytics_loaded(QString portfolio_id, bool ok, QString error);

    /// Fired when backfill_history finishes. point_count is the number of
    /// trading days written (0 on failure).
    void history_backfilled(QString portfolio_id, int point_count);

  private:
    PortfolioService();

    void build_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                       const portfolio::Portfolio& portfolio);

    /// Common "I have quotes, build the summary and emit" path used by both
    /// the broker and yfinance routes. quote_map is keyed by the asset's
    /// canonical (yfinance) `symbol` field.
    void finalize_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                          const portfolio::Portfolio& portfolio, const QHash<QString, QuoteData>& quote_map);

    /// Persist today's level for every custom index built on this portfolio, so
    /// the Indices view has a real value history instead of only the level at
    /// creation time.
    void record_custom_index_values(const portfolio::PortfolioSummary& summary);

    /// Single exit path for an analytics result (live or cache hit).
    void publish_market_analytics(const portfolio::MarketAnalytics& a);

    /// Load the last known-good risk-free rate from settings (once per process).
    void ensure_risk_free_loaded();

    /// Re-derive @p symbol's position (quantity + weighted-average cost) from
    /// the ledger after a ledger row changed. Only applied when the ledger
    /// explained the position before the change; otherwise the quantity delta
    /// is applied so positions that pre-date their transactions survive.
    void reconcile_position(const QString& portfolio_id, const QString& symbol,
                            const QVector<portfolio::Transaction>& before);

    /// Try to fetch live quotes via the broker linked to `portfolio.broker_account_id`.
    /// On success, calls finalize_summary with broker-sourced QuoteData.
    /// On any failure (no account, disconnected, broker null, API error),
    /// silently falls back to the yfinance path. Returns true if the broker
    /// path was attempted (even if it ultimately failed); false if we should
    /// go straight to yfinance.
    bool try_broker_quotes(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                           const portfolio::Portfolio& portfolio);

    // ── Summary cache (P11) ──────────────────────────────────────────────────
    // In-memory (not CacheManager) intentionally: PortfolioSummary holds nested
    // Portfolio + QVector<HoldingWithQuote>, which would require a large JSON
    // (de)serializer to persist. The summary is cheap to recompute from quotes
    // on restart, so durability gains nothing. Kept tight with a 5-min TTL.
    struct CachedSummary {
        portfolio::PortfolioSummary summary;
        qint64 timestamp = 0;
    };
    QHash<QString, CachedSummary> summary_cache_;
    QMutex cache_mutex_;
    static constexpr int kCacheTtlSec = 300; // 5 minutes

    // ── Risk-free rate (annual fraction, e.g. 0.043) ─────────────────────────
    // Live value comes from the analytics run (^IRX); the last good value is
    // persisted in SettingsRepository so a restart without network still has
    // one. The 4% default is used only when nothing was ever fetched, and the
    // source label says so.
    static constexpr double kDefaultRiskFreeRate = 0.04;
    double rf_rate_ = kDefaultRiskFreeRate;
    QString rf_source_;
    bool rf_loaded_ = false;

    // ── Market analytics cache (P11) ─────────────────────────────────────────
    // The screen asks on every summary refresh (60 s by default). The inputs
    // are daily closes, so one run per (portfolio, market day, holdings,
    // benchmark) is enough; repeats replay from memory.
    struct AnalyticsCacheEntry {
        QString key; // portfolio|date|benchmark|sorted holdings
        qint64 timestamp = 0;
        portfolio::MarketAnalytics data;
    };
    QHash<QString, AnalyticsCacheEntry> analytics_cache_; // portfolio_id → last result
    QHash<QString, QString> analytics_inflight_;          // portfolio_id → key being fetched
    static constexpr int kAnalyticsTtlSec = 30 * 60;

    // ── Sector-resolved refresh debounce ─────────────────────────────────────
    // A fresh import resolves one sector per holding; each resolution used to
    // trigger its own quote fetch + summary rebuild. Batch them.
    QSet<QString> sector_refresh_pending_;

    // ── Backfill state ───────────────────────────────────────────────────────
    // Per-portfolio guard so compute_metrics doesn't kick off backfill on
    // every refresh tick. Cleared on app restart — that's the explicit retry.
    QSet<QString> backfill_attempted_;
};

} // namespace fincept::services
