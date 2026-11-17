// src/services/portfolio/PortfolioService.h
#pragma once
#include "python/PythonRunner.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataService.h"
#include "services/portfolio/PortfolioAllocation.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "services/portfolio/PortfolioLots.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

#include <optional>

namespace fincept {
struct CustomIndex; // storage/repositories/CustomIndexRepository.h
}

namespace fincept::services {

/// Singleton service managing portfolio data, live quotes, and computed metrics.
/// Screens connect to signals — never call DB or market data directly.
///
/// Valuation is in each portfolio's BASE currency (Portfolio::currency): every
/// holding is converted at the spot rate (FxRateService) from the currency its
/// quote reports, minor units handled. See PortfolioValuation.h for the rules and
/// PortfolioSummary::complete for the incomplete state.
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

    /// Analytics settings (v055). `benchmark`: '' = default for the base currency,
    /// one Yahoo symbol, or a blend "SPY:60,AGG:40". `risk_free_override`: annual
    /// fraction, nullopt = automatic. `lot_method`: FIFO / LIFO / HIFO / AVERAGE /
    /// SPECIFIC. `long_term_days` > 0. Returns false (and emits operation_failed) on
    /// invalid input. Emits portfolio_settings_changed and invalidates every cache.
    bool update_portfolio_settings(const QString& id, const QString& benchmark,
                                   std::optional<double> risk_free_override, bool track_cash,
                                   const QString& lot_method, int long_term_days);

    // ── Summary (assets + live quotes) ───────────────────────────────────────
    void load_summary(const QString& portfolio_id);
    void refresh_summary(const QString& portfolio_id); // invalidates cache first

    // ── Asset operations ─────────────────────────────────────────────────────
    /// `broker_symbol`+`exchange` are stored alongside the canonical
    /// yfinance-format `symbol` so broker quote calls can use the native
    /// pair. Both empty for manual / JSON imports. `date` is the trade date
    /// (YYYY-MM-DD or ISO date-time; empty = now). Returns false (and records
    /// nothing) on a non-positive quantity/price or a database error.
    /// The price is in the instrument's quoted units (pence for "VOD.L").
    bool add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                   const QString& date = {}, const QString& broker_symbol = {}, const QString& exchange = {});
    /// Records a SELL and reduces (or closes) the position. Refuses — returning
    /// false and recording nothing — when the symbol is not held or @p qty
    /// exceeds the quantity held.
    bool sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                    const QString& date = {});

    /// Any ledger row (see namespace portfolio::txn for each type's conventions),
    /// with fees, taxes, currency and trade-date FX. BUY / SELL / SPLIT also update
    /// the stored position (SELL refuses an over-sell); a DIVIDEND with quantity <= 0
    /// gets the quantity held at the start of its date (the ex-date) from the ledger.
    /// `t.portfolio_id` is required; `t.id` is ignored. Returns false (and emits
    /// operation_failed) when rejected. `broker_symbol` / `exchange` as for add_asset().
    bool record_transaction(const portfolio::Transaction& t, const QString& broker_symbol = {},
                            const QString& exchange = {});
    /// SPLIT convenience: `factor` = new units per old unit (2 = 2-for-1, 0.1 = 1-for-10).
    bool record_split(const QString& portfolio_id, const QString& symbol, double factor, const QString& date,
                      const QString& notes = {});

    /// Every symbol held in any portfolio (canonical yfinance form), de-duplicated.
    /// Reads the repository only — no service state — so it is safe on a worker
    /// thread (the News screen uses it to flag holdings in the feed).
    static QStringList held_symbols();

    // ── Transactions ─────────────────────────────────────────────────────────
    void load_transactions(const QString& portfolio_id, int limit = 50);
    /// Full ledger, newest first (local SQLite read — for exports/edits).
    QVector<portfolio::Transaction> ledger(const QString& portfolio_id) const;
    /// Edits a ledger row and re-derives that symbol's position from the
    /// ledger, so holdings and history can no longer disagree.
    bool update_transaction(const QString& portfolio_id, const QString& id, double qty, double price,
                            const QString& date, const QString& notes = {});
    /// Edits the v055 cost fields of a row (fees, taxes, currency, trade FX, lot id).
    bool update_transaction_costs(const QString& portfolio_id, const QString& id, double fees, double taxes,
                                  const QString& currency, double fx_rate, const QString& lot_id = {});
    bool delete_transaction(const QString& id, const QString& portfolio_id);

    /// Replays the ledger: open tax lots, realised gains (short / long term), cash
    /// balances, income, fees and external flows, in the base currency. `method` /
    /// `long_term_days` default to the portfolio's settings. Synchronous SQLite read +
    /// pure computation (milliseconds for thousands of rows).
    portfolio::LedgerResult replay_ledger(const QString& portfolio_id,
                                          std::optional<portfolio::LotMethod> method = std::nullopt,
                                          int long_term_days = -1) const;

    // ── Dividend ──────────────────────────────────────────────────────────────
    /// `qty` is used only when the ledger cannot tell the quantity held at the start
    /// of `date` (the ex-date): when it can, the ledger quantity is recorded instead
    /// (the dialog used to pass today's holding). `total` is ignored (qty × amount).
    bool record_dividend(const QString& portfolio_id, const QString& symbol, double qty, double amount_per_share,
                         double total, const QString& date = {}, const QString& notes = {});

    // ── Market analytics (one Python call) ────────────────────────────────────
    /// Downloads every holding + its currency's FX pair + the benchmark + the
    /// risk-free series in ONE threaded call (scripts/portfolio_metrics.py) and
    /// derives, in the BASE currency: correlation matrix, benchmark history,
    /// risk-free rate, ex-ante risk at current weights, VaR / CVaR (historical,
    /// parametric, Monte Carlo) at `opts`, and per-holding marginal / component VaR.
    /// Cached per (portfolio, day, holdings, benchmark, options). Emits
    /// market_analytics_loaded().
    void fetch_market_analytics(const portfolio::PortfolioSummary& summary, bool force = false,
                                const portfolio::RiskOptions& opts = {});
    /// Last analytics delivered for @p portfolio_id (empty portfolio_id if none).
    portfolio::MarketAnalytics last_market_analytics(const QString& portfolio_id) const;

    /// Before/after risk of moving the book to `target_weights_pct` (symbol -> % of
    /// the valued total; symbols not in the book may be added; omitted = 0).
    /// Callback (main thread): data = {"before": {...}, "after": {...}, "delta": {...},
    /// "weights": {...}, "warnings": [...]} — see scripts/portfolio_metrics.py what_if.
    /// The returned request carries the id echoed in AnalyticsResult::request_id and
    /// the token that cancels the run (the callback then reports `cancelled`).
    AnalyticsRequest what_if(const portfolio::PortfolioSummary& summary,
                             const QHash<QString, double>& target_weights_pct, AnalyticsCallback cb,
                             const portfolio::RiskOptions& opts = {});

    /// Historical-window replay: the CURRENT book (weights, base currency) through
    /// the market moves of [start, end] (YYYY-MM-DD). Callback data =
    /// {"return", "pnl_fraction", "max_drawdown", "coverage", "holdings": {SYM: {"return",
    /// "contribution"}}, "missing": [...], "benchmark_return", ...}. Cancellable as what_if().
    AnalyticsRequest historical_stress(const portfolio::PortfolioSummary& summary, const QString& start,
                                       const QString& end, AnalyticsCallback cb);

    /// True while a market-analytics run (fetch_market_analytics) is in flight for
    /// @p portfolio_id — views show "loading" only then, never for a book that has
    /// nothing to download.
    bool market_analytics_pending(const QString& portfolio_id) const {
        return analytics_inflight_.contains(portfolio_id);
    }

    /// Default benchmark for a base currency: a total-return (dividend-adjusted)
    /// ETF of the home market where Yahoo has one with history, else a price index,
    /// else ACWI (global, converted into the base currency). Covers every currency
    /// the create dialog offers.
    static QString default_benchmark_for_currency(const QString& currency);
    /// True when the default benchmark of `currency` is total-return.
    static bool default_benchmark_is_total_return(const QString& currency);
    /// Benchmark spec effectively used for `p` (its own setting or the default).
    static QString benchmark_for(const portfolio::Portfolio& p);
    /// Parses "SPY" / "SPY:60,AGG:40" into normalised fractional weights; empty on error.
    static QVector<QPair<QString, double>> parse_benchmark(const QString& spec);

    /// Trailing trading days behind the holdings correlation matrix.
    static constexpr int kCorrelationWindowDays = 63;

    /// Context for PortfolioAnalyticsService calls so every script uses the same
    /// base currency, quoted currencies, benchmark, calendar and risk-free rate:
    /// {"base_currency", "currencies": {SYM: "GBp"}, "benchmark", "periods_per_year",
    ///  "risk_free_rate" (absent when unknown), "risk_free_known"}.
    QJsonObject analytics_context(const portfolio::PortfolioSummary& summary) const;
    /// Trading days per year of the calendar venue of `p`'s benchmark (≈252 NYSE,
    /// 245 TSE, 365 crypto ...), and that venue's id.
    static double periods_per_year_for(const portfolio::Portfolio& p, QString* calendar_id = nullptr);

    struct RiskFree {
        double rate = 0;  ///< annual fraction
        QString source;   ///< human-readable
        bool known = false; ///< false: no rate for this currency and no override (0 % used)
    };
    /// Risk-free rate for `p`: its override, else the last value fetched for its base
    /// currency (USD ^IRX; EUR €STR — ECB; GBP SONIA — BoE; CAD CORRA — BoC;
    /// CHF SARON — SNB), else unknown.
    RiskFree risk_free_for(const portfolio::Portfolio& p) const;

    /// Rate / source of the portfolio whose summary was built last (legacy accessors).
    double risk_free_rate() const { return rf_rate_; }
    QString risk_free_source() const { return rf_source_; }

    // ── Metrics ──────────────────────────────────────────────────────────────
    /// Realised (TWR / MWR / benchmark-relative) metrics over `window`, merged with
    /// the ex-ante figures of the last analytics run. The ledger / NAV history are
    /// read and the statistics computed on a worker thread; metrics_computed fires
    /// on the main thread. A newer request for the same portfolio supersedes an
    /// older one still running.
    void compute_metrics(const portfolio::PortfolioSummary& summary,
                         portfolio::PerfWindow window = portfolio::PerfWindow::All);

    /// TWR of `snapshots` (NAV in the portfolio's base currency; the portfolio is
    /// the snapshots' portfolio_id). Every ledger flow is converted into the base at
    /// its own recorded rate, else the daily rate of its date (the service's FX
    /// history cache); when that date's rate is not known yet, at today's spot rate
    /// — and its currency is listed in `approx_currencies`: the series is then
    /// approximate and the view should say so, like PortfolioSummary::fx_stale. A
    /// flow with no rate at all leaves its period unmeasured (TwrPoint::valid
    /// false) — never 1:1. Empty series when the portfolio cannot be read. GUI
    /// thread (reads the FX caches). Prefer compute_metrics().
    static QVector<portfolio::TwrPoint> build_twr_series(const QVector<portfolio::PortfolioSnapshot>& snapshots,
                                                         const QVector<portfolio::Transaction>& transactions);
    static QVector<portfolio::TwrPoint> build_twr_series(const QVector<portfolio::PortfolioSnapshot>& snapshots,
                                                         const QVector<portfolio::Transaction>& transactions,
                                                         QStringList* approx_currencies);

    // ── Allocation ───────────────────────────────────────────────────────────
    /// Allocation by sector, industry, asset class, country, region, currency,
    /// exchange and market-cap bucket, plus concentration (HHI, effective N, top-N),
    /// from the summary and the `market:info:<SYM>` cache. Symbols whose info is not
    /// cached yet are requested; allocation_updated(portfolio_id) fires as they land.
    portfolio::AllocationReport allocation(const portfolio::PortfolioSummary& summary);
    /// The `market:info:<SYM>` fundamentals cached by allocation() (52-week range,
    /// dividend rate / yield, ex-dividend date, ...). nullopt until they have landed —
    /// call allocation() first; allocation_updated fires as they arrive.
    std::optional<InfoData> cached_info(const QString& symbol) const {
        const auto it = info_cache_.constFind(symbol);
        if (it == info_cache_.constEnd())
            return std::nullopt;
        return *it;
    }

    // ── Import / Export ──────────────────────────────────────────────────────
    void export_csv(const QString& portfolio_id, const QString& file_path);
    void export_json(const QString& portfolio_id, const QString& file_path);
    /// Accepts the terminal's export format ({portfolio_name, transactions[]})
    /// AND the holdings-snapshot shape ({name, holdings[]}). The whole file is
    /// validated before anything is written; a rejected file reports every
    /// problem found (row-by-row) instead of a generic "unsupported format".
    void import_json(const QString& file_path, portfolio::ImportMode mode, const QString& merge_target_id = {});

    // ── Snapshots (performance history) ─────────────────────────────────────
    /// `days` <= 0 loads the whole history.
    void load_snapshots(const QString& portfolio_id, int days = 365);

    /// Reconstruct daily NAV in the base currency by replaying the ledger over
    /// split-adjusted-back (raw) closes, daily FX closes and recorded splits /
    /// dividends / cash (scripts/portfolio_metrics.py nav_replay), and upsert one
    /// row per trading day. Days on which a held position has no price are not
    /// written. Emits history_backfilled(portfolio_id, point_count).
    void backfill_history(const QString& portfolio_id, const QString& period = "1y");

    // ── Cache control ────────────────────────────────────────────────────────
    void invalidate_cache(const QString& portfolio_id);

    // ── Custom indices ───────────────────────────────────────────────────────
    /// Current level of @p idx given the live prices in @p summary. NaN when a
    /// constituent has no live price (the level is unknown — it is not recorded).
    /// "Price Weighted", "Equal Weighted", "Geometric Mean" and "Portfolio Weighted"
    /// (= creation weights × price relatives) are computed; any other stored method
    /// name (legacy Market Cap / Float / Fundamental / Modified / Factor / Risk
    /// Parity / Capped) is computed as Portfolio Weighted.
    static double compute_custom_index_value(const fincept::CustomIndex& idx,
                                             const portfolio::PortfolioSummary& summary);

    // ── Storage failures ─────────────────────────────────────────────────────
    /// Translated, user-facing text for a failed database call: `action` (what the
    /// user tried, already translated) followed by what it means. A missing table /
    /// column or an unreadable file is reported as "this profile's database cannot
    /// be read by this build"; the raw driver message is never part of the text.
    static QString storage_failure_text(const QString& action, const QString& raw_error);
    /// True when `raw_error` means the database does not have this build's schema
    /// (missing table / column, not a database, malformed file).
    static bool is_schema_failure(const QString& raw_error);
    /// The raw reason behind the operation_failed signal being delivered now (empty
    /// for a validation refusal). Read it from a slot connected to operation_failed.
    QString last_failure_details() const { return last_failure_details_; }
    /// The portfolio list could not be read (load_portfolios); empty when it could.
    QString load_failure_message() const { return load_failure_message_; }
    QString load_failure_details() const { return load_failure_details_; }

    /// Base units per ONE major unit of `ccy` on `date` from the cached daily FX history
    /// the ledger replay uses (forward-filled at most 5 days); nullopt when not loaded.
    /// GUI thread only. Lets views convert rows exactly as the summary does.
    std::optional<double> fx_on(const QString& ccy, const QString& base, const QDate& date) const {
        return historical_fx(ccy, base, date);
    }

  signals:
    void portfolios_loaded(QVector<portfolio::Portfolio> portfolios);
    /// The portfolio list could not be read: `message` is translated and user-facing
    /// (storage_failure_text), `details` the raw database reason.
    void portfolios_load_failed(QString message, QString details);
    void portfolio_created(portfolio::Portfolio portfolio);
    void portfolio_deleted(QString id);
    void portfolio_settings_changed(QString id);

    void summary_loaded(portfolio::PortfolioSummary summary);
    void summary_error(QString portfolio_id, QString error);

    void transactions_loaded(QVector<portfolio::Transaction> transactions);

    /// metrics.portfolio_id / metrics.window identify the request.
    void metrics_computed(portfolio::ComputedMetrics metrics);
    void snapshots_loaded(QString portfolio_id, QVector<portfolio::PortfolioSnapshot> snapshots);

    void asset_added(QString portfolio_id);
    void asset_sold(QString portfolio_id);
    /// A ledger row was added, edited or deleted and the affected position re-derived.
    void ledger_changed(QString portfolio_id);
    /// A user-initiated write was refused or failed (over-sell, bad input, DB
    /// error). Previously these were log-only and the action looked ignored.
    void operation_failed(QString portfolio_id, QString message);

    void export_complete(QString file_path);
    void export_failed(QString file_path, QString error);
    void import_complete(portfolio::ImportResult result);

    void market_analytics_loaded(QString portfolio_id, bool ok, QString error);

    /// Fired when backfill_history finishes. point_count is the number of
    /// trading days written (0 on failure).
    void history_backfilled(QString portfolio_id, int point_count);
    /// The NAV replay found corporate actions / data issues worth showing:
    /// splits in Yahoo's data that the ledger does not record ("VOD.L 1:10 split on
    /// 2024-05-02 not recorded"), symbols without price history, skipped days.
    void history_notes(QString portfolio_id, QStringList notes);

    /// market:info for some holdings arrived — call allocation() again.
    void allocation_updated(QString portfolio_id);

  private:
    PortfolioService();

    /// emit operation_failed with `details` (raw reason, may be empty) readable
    /// through last_failure_details() while the signal is delivered.
    void fail(const QString& portfolio_id, const QString& message, const QString& details = {});
    /// fail() for a failed repository call: storage_failure_text(action, raw) + raw as details.
    void fail_storage(const QString& portfolio_id, const QString& action, const std::string& raw);

    void build_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                       const portfolio::Portfolio& portfolio);

    /// Common "I have quotes, build the summary and emit" path used by both
    /// the broker and yfinance routes. quote_map is keyed by the asset's
    /// canonical (yfinance) `symbol` field.
    void finalize_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                          const portfolio::Portfolio& portfolio, const QHash<QString, QuoteData>& quote_map);
    /// Second half of finalize_summary, on the GUI thread once the worker has read and
    /// replayed the ledger: value the book, cache / snapshot it, emit summary_loaded.
    void finish_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                        const portfolio::Portfolio& portfolio, const QHash<QString, QuoteData>& quote_map,
                        const portfolio::LedgerOptions& opt, const QVector<portfolio::Transaction>& txns,
                        const portfolio::LedgerResult& ledger);
    QHash<QString, quint64> summary_seq_; ///< latest finalize_summary request per portfolio

    /// Persist today's level for every custom index built on this portfolio.
    void record_custom_index_values(const portfolio::PortfolioSummary& summary);

    void publish_market_analytics(const portfolio::MarketAnalytics& a);

    /// Load the last known-good risk-free rates from settings (once per process).
    void ensure_risk_free_loaded() const;

    /// Re-derive @p symbol's position (quantity + average cost incl. fees) from the
    /// ledger after a ledger row changed. Only applied when the ledger explained the
    /// position before the change; otherwise the quantity delta is applied so
    /// positions that pre-date their transactions survive.
    void reconcile_position(const QString& portfolio_id, const QString& symbol,
                            const QVector<portfolio::Transaction>& before);

    bool try_broker_quotes(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                           const portfolio::Portfolio& portfolio);

    /// Base per 1 major unit of `ccy` on `date` from the cached daily history
    /// (forward-filled at most 5 days). Requests the history when missing.
    std::optional<double> historical_fx(const QString& ccy, const QString& base, const QDate& date) const;
    /// Ask FxRateService for daily history of each currency the ledger needs.
    void request_fx_history(const QString& portfolio_id, const QStringList& ccys, const QString& base,
                            const QDate& from);
    /// Ledger options for `p` (method, base, historical FX lookup).
    portfolio::LedgerOptions ledger_options(const portfolio::Portfolio& p,
                                            const QHash<QString, QString>& quote_ccy = {}) const;

    // ── Summary cache (P11) ──────────────────────────────────────────────────
    struct CachedSummary {
        portfolio::PortfolioSummary summary;
        qint64 timestamp = 0;
    };
    QHash<QString, CachedSummary> summary_cache_;
    QMutex cache_mutex_;
    static constexpr int kCacheTtlSec = 300; // 5 minutes

    /// Inputs of a summary that was incomplete for want of FX (spot or history):
    /// re-valued without a new quote fetch when the rates land.
    struct PendingValuation {
        QVector<portfolio::PortfolioAsset> assets;
        portfolio::Portfolio portfolio;
        QHash<QString, QuoteData> quotes;
        QStringList waiting_spot; ///< major currencies without a spot rate to base
        bool waiting_history = false; ///< a daily FX history request is in flight
    };
    /// Re-value the pending summaries whose missing rates have arrived.
    void revalue_pending(bool history_arrived);
    QHash<QString, PendingValuation> fx_pending_;
    bool fx_refresh_scheduled_ = false;

    QString last_failure_details_;  ///< see last_failure_details()
    QString load_failure_message_;  ///< see load_failure_message()
    QString load_failure_details_;

    /// "CCY>BASE" -> date -> base per 1 CCY (daily closes, forward-filled by FxRateService).
    QHash<QString, QMap<QDate, double>> fx_history_;
    QSet<QString> fx_history_requested_;

    // ── Risk-free rates (annual fraction) by base currency ──────────────────
    mutable QHash<QString, QPair<double, QString>> rf_by_ccy_; // ccy -> (rate, source)
    mutable bool rf_loaded_ = false;
    double rf_rate_ = 0;
    QString rf_source_;

    // ── Market analytics cache (P11) ─────────────────────────────────────────
    struct AnalyticsCacheEntry {
        QString key; // portfolio|date|benchmark|options|sorted holdings
        qint64 timestamp = 0;
        portfolio::MarketAnalytics data;
    };
    QHash<QString, AnalyticsCacheEntry> analytics_cache_; // portfolio_id → last result
    QHash<QString, QString> analytics_inflight_;          // portfolio_id → key being fetched
    static constexpr int kAnalyticsTtlSec = 30 * 60;

    // ── Metrics jobs ─────────────────────────────────────────────────────────
    QHash<QString, quint64> metrics_seq_; // portfolio_id|window -> latest request id

    // ── Allocation info ──────────────────────────────────────────────────────
    QHash<QString, InfoData> info_cache_;     // symbol -> fundamentals (from market:info)
    QSet<QString> info_subscribed_;
    QHash<QString, QSet<QString>> info_waiters_; // symbol -> portfolio ids waiting for it

    // ── Sector-resolved refresh debounce ─────────────────────────────────────
    QSet<QString> sector_refresh_pending_;

    // ── Backfill state ───────────────────────────────────────────────────────
    QSet<QString> backfill_attempted_;

    /// Replay the NAV history after a back-dated / edited ledger row. Debounced and
    /// collapsed per portfolio.
    void schedule_history_replay(const QString& portfolio_id);
    QSet<QString> replay_pending_;
};

} // namespace fincept::services
