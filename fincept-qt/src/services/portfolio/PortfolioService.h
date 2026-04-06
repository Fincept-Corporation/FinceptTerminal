// src/services/portfolio/PortfolioService.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/markets/MarketDataService.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QTimer>

namespace fincept::services {

/// Singleton service managing portfolio data, live quotes, and computed metrics.
/// Screens connect to signals — never call DB or market data directly.
class PortfolioService : public QObject {
    Q_OBJECT
  public:
    static PortfolioService& instance();

    // ── Portfolio CRUD ───────────────────────────────────────────────────────
    void load_portfolios();
    void create_portfolio(const QString& name, const QString& owner, const QString& currency,
                          const QString& description = {});
    void delete_portfolio(const QString& id);

    // ── Summary (assets + live quotes) ───────────────────────────────────────
    void load_summary(const QString& portfolio_id);
    void refresh_summary(const QString& portfolio_id); // invalidates cache first

    // ── Asset operations ─────────────────────────────────────────────────────
    void add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                   const QString& date = {});
    void sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                    const QString& date = {});

    // ── Transactions ─────────────────────────────────────────────────────────
    void load_transactions(const QString& portfolio_id, int limit = 50);
    void update_transaction(const QString& id, double qty, double price, const QString& date,
                            const QString& notes = {});
    void delete_transaction(const QString& id, const QString& portfolio_id);

    // ── Dividend ──────────────────────────────────────────────────────────────
    void record_dividend(const QString& portfolio_id, const QString& symbol,
                         double qty, double amount_per_share, double total,
                         const QString& date = {}, const QString& notes = {});

    // ── Historical correlation ─────────────────────────────────────────────────
    /// Fetch 30-day daily closes for @p symbols and compute pairwise Pearson
    /// correlation matrix. Result emitted via correlation_computed().
    void fetch_correlation(const QStringList& symbols);

    // ── SPY benchmark data ────────────────────────────────────────────────────
    /// Fetch SPY daily closes for the given period string (e.g. "1y", "6mo").
    /// Emits spy_history_loaded(dates, closes).
    void fetch_spy_history(const QString& period = "1y");

    // ── Risk-free rate ────────────────────────────────────────────────────────
    /// Fetch the current 10-year Treasury yield (DGS10) from FRED.
    /// Result is cached 24h in SettingsRepository. Emits risk_free_rate_loaded(rate).
    void fetch_risk_free_rate();

    // ── Metrics (async computation) ──────────────────────────────────────────
    void compute_metrics(const portfolio::PortfolioSummary& summary);

    // ── Import / Export ──────────────────────────────────────────────────────
    void export_csv(const QString& portfolio_id, const QString& file_path);
    void export_json(const QString& portfolio_id, const QString& file_path);
    void import_json(const QString& file_path, portfolio::ImportMode mode, const QString& merge_target_id = {});

    // ── Snapshots (performance history) ─────────────────────────────────────
    void load_snapshots(const QString& portfolio_id, int days = 365);

    // ── Cache control ────────────────────────────────────────────────────────
    void invalidate_cache(const QString& portfolio_id);

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

    void export_complete(QString file_path);
    void import_complete(portfolio::ImportResult result);

    /// Pairwise Pearson correlation matrix keyed by "SYM1|SYM2".
    /// Values in [-1, 1]. Diagonal (self-correlation) = 1.0.
    void correlation_computed(QHash<QString, double> matrix);

    /// SPY daily close history: parallel vectors of ISO date strings and prices.
    void spy_history_loaded(QStringList dates, QVector<double> closes);

    /// Current 10-year risk-free rate as annual decimal (e.g. 0.043 = 4.3%).
    void risk_free_rate_loaded(double rate);

  private:
    PortfolioService();

    void build_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                       const portfolio::Portfolio& portfolio);

    // ── Summary cache (P11) ──────────────────────────────────────────────────
    struct CachedSummary {
        portfolio::PortfolioSummary summary;
        qint64 timestamp = 0;
    };
    QHash<QString, CachedSummary> summary_cache_;
    QMutex cache_mutex_;
    static constexpr int kCacheTtlSec = 300; // 5 minutes

    // ── SPY cache (for OLS beta in compute_metrics) ──────────────────────────
    QStringList     spy_dates_cache_;
    QVector<double> spy_closes_cache_;

    // ── Risk-free rate cache (annual decimal, e.g. 0.043) ────────────────────
    double rf_rate_ = 0.04; // default 4% until FRED responds
};

} // namespace fincept::services
