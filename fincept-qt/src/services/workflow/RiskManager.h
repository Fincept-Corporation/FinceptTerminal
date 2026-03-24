#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::workflow {

/// Risk check severity level.
enum class RiskSeverity { Info, Warning, Error, Critical };

/// Result of a single risk check.
struct RiskCheckResult {
    bool passed = true;
    RiskSeverity severity = RiskSeverity::Info;
    QString check_name;
    QString message;
};

/// Comprehensive risk limits configuration.
struct RiskLimits {
    // Position limits
    double max_position_size = 1000;      // max shares per position
    double max_position_value = 50000;    // max $ value per position
    double max_portfolio_exposure = 0.25; // max % of portfolio
    int max_total_positions = 20;

    // Order limits
    double max_single_order_value = 25000;
    int max_daily_trades = 100;
    double max_daily_volume = 500000;

    // Loss limits
    double daily_loss_limit = 5000;
    double weekly_loss_limit = 15000;
    double per_position_stop_loss = 0.05; // 5%

    // Asset restrictions
    QStringList allowed_asset_classes = {"equity", "etf", "crypto"};
    QStringList blocked_symbols;
    bool allow_short_selling = false;
    bool allow_margin = false;

    // Time restrictions
    bool trading_hours_only = true;
    bool allow_premarket = false;
};

/// Tracks daily P&L and trade counts.
struct DailyTradingStats {
    double realized_pnl = 0;
    double unrealized_pnl = 0;
    int trade_count = 0;
    double volume = 0;
    QString date;
};

/// Comprehensive risk manager for trading workflows.
class RiskManager : public QObject {
    Q_OBJECT
  public:
    static RiskManager& instance();

    /// Validate an order against all risk limits.
    QVector<RiskCheckResult> validate_order(const QString& symbol, const QString& side, double quantity, double price,
                                            bool paper_trading = true) const;

    /// Check if all validations pass.
    bool is_order_allowed(const QString& symbol, const QString& side, double quantity, double price,
                          bool paper_trading = true) const;

    /// Get/set risk limits.
    const RiskLimits& limits() const { return limits_; }
    void set_limits(const RiskLimits& limits) { limits_ = limits; }

    /// Get/set daily stats.
    const DailyTradingStats& daily_stats() const { return daily_stats_; }
    void record_trade(double pnl, double volume);
    void reset_daily_stats();

    /// Get overall risk level.
    RiskSeverity current_risk_level() const;

  signals:
    void risk_level_changed(RiskSeverity level);
    void limit_breached(const RiskCheckResult& result);

  private:
    RiskManager();

    RiskLimits limits_;
    DailyTradingStats daily_stats_;
};

} // namespace fincept::workflow
