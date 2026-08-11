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
///
/// IMPORTANT — every limit is OPT-IN and defaults to "no limit" (0). These are
/// enforced on the real order path (UnifiedTrading::place_order and the basket /
/// split / smart paths), so a non-zero default would silently start rejecting
/// trades that work today on an install nobody ever configured. A limit only
/// applies once an operator has set it to a positive value; `blocked_symbols` is
/// likewise empty and short selling is allowed (selling is normal trading — this
/// flag exists to opt *out*).
struct RiskLimits {
    // Position limits (0 = unlimited)
    double max_position_size = 0;      // max units per order
    double max_position_value = 0;     // max value per position
    double max_portfolio_exposure = 0; // max fraction of portfolio
    int max_total_positions = 0;

    // Order limits (0 = unlimited)
    double max_single_order_value = 0;
    int max_daily_trades = 0;
    double max_daily_volume = 0;

    // Loss limits (0 = unlimited)
    double daily_loss_limit = 0;
    double weekly_loss_limit = 0;
    double per_position_stop_loss = 0;

    // Asset restrictions
    QStringList allowed_asset_classes = {"equity", "etf", "crypto"};
    QStringList blocked_symbols;
    bool allow_short_selling = true;
    bool allow_margin = true;

    // Time restrictions
    bool trading_hours_only = false;
    bool allow_premarket = true;
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

    /// Check if all validations pass. On failure `reason_out` (optional) receives
    /// the joined messages of the failing Error/Critical checks, so the caller can
    /// tell the user exactly which limit blocked the order instead of a bare
    /// "rejected".
    bool is_order_allowed(const QString& symbol, const QString& side, double quantity, double price,
                          bool paper_trading = true, QString* reason_out = nullptr) const;

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
