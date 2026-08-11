#include "services/workflow/RiskManager.h"

#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDate>

namespace fincept::workflow {

RiskManager& RiskManager::instance() {
    static RiskManager s;
    return s;
}

RiskManager::RiskManager() : QObject(nullptr) {
    daily_stats_.date = QDate::currentDate().toString(Qt::ISODate);
    // The gate is consulted from QtConcurrent worker threads (basket / split /
    // algo order paths), so this singleton can end up first constructed on a pool
    // thread that later dies — leaving its signals and its end-of-process
    // destruction homed to a dead thread. Re-home it on the application thread.
    // Moving is only legal from the object's current thread, which is exactly
    // where we are during construction.
    if (auto* app = QCoreApplication::instance(); app && thread() != app->thread())
        moveToThread(app->thread());
}

QVector<RiskCheckResult> RiskManager::validate_order(const QString& symbol, const QString& side, double quantity,
                                                     double price, bool paper_trading) const {
    Q_UNUSED(paper_trading);
    QVector<RiskCheckResult> results;
    double order_value = quantity * price;

    // Daily counters roll over at midnight. This method is const (the real reset
    // lives in record_trade), so use effective values: once the calendar day has
    // changed, yesterday's count / volume / loss no longer apply — otherwise the
    // first order after midnight is wrongly blocked by yesterday's daily loss.
    const bool same_day = (daily_stats_.date == QDate::currentDate().toString(Qt::ISODate));
    const int eff_trade_count = same_day ? daily_stats_.trade_count : 0;
    const double eff_volume = same_day ? daily_stats_.volume : 0.0;
    const double eff_realized_pnl = same_day ? daily_stats_.realized_pnl : 0.0;

    // Every numeric limit below is OPT-IN: a value of 0 (the default) means "no
    // limit". This gate runs on the real order path, so an unset limit must pass
    // the order through rather than reject everything.

    // Position size check
    if (limits_.max_position_size > 0 && quantity > limits_.max_position_size) {
        results.append({false, RiskSeverity::Error, "position_size",
                        QString("Quantity %1 exceeds max %2").arg(quantity).arg(limits_.max_position_size)});
    } else {
        results.append({true, RiskSeverity::Info, "position_size", "OK"});
    }

    // Position value check
    if (limits_.max_position_value > 0 && order_value > limits_.max_position_value) {
        results.append({false, RiskSeverity::Error, "position_value",
                        QString("Value $%1 exceeds max $%2")
                            .arg(order_value, 0, 'f', 2)
                            .arg(limits_.max_position_value, 0, 'f', 2)});
    } else {
        results.append({true, RiskSeverity::Info, "position_value", "OK"});
    }

    // Single order value check
    if (limits_.max_single_order_value > 0 && order_value > limits_.max_single_order_value) {
        results.append({false, RiskSeverity::Error, "order_value",
                        QString("Order value $%1 exceeds single order limit $%2")
                            .arg(order_value, 0, 'f', 2)
                            .arg(limits_.max_single_order_value, 0, 'f', 2)});
    } else {
        results.append({true, RiskSeverity::Info, "order_value", "OK"});
    }

    // Daily trade count check
    if (limits_.max_daily_trades > 0 && eff_trade_count >= limits_.max_daily_trades) {
        results.append(
            {false, RiskSeverity::Warning, "daily_trades",
             QString("Daily trade count %1 at limit %2").arg(eff_trade_count).arg(limits_.max_daily_trades)});
    } else {
        results.append({true, RiskSeverity::Info, "daily_trades", "OK"});
    }

    // Daily volume check
    if (limits_.max_daily_volume > 0 && eff_volume + order_value > limits_.max_daily_volume) {
        results.append({false, RiskSeverity::Warning, "daily_volume",
                        QString("Daily volume would exceed $%1 limit").arg(limits_.max_daily_volume, 0, 'f', 2)});
    } else {
        results.append({true, RiskSeverity::Info, "daily_volume", "OK"});
    }

    // Daily loss limit check
    if (limits_.daily_loss_limit > 0 && eff_realized_pnl < -limits_.daily_loss_limit) {
        results.append({false, RiskSeverity::Critical, "daily_loss",
                        QString("Daily loss $%1 exceeds limit $%2")
                            .arg(-eff_realized_pnl, 0, 'f', 2)
                            .arg(limits_.daily_loss_limit, 0, 'f', 2)});
    } else {
        results.append({true, RiskSeverity::Info, "daily_loss", "OK"});
    }

    // Blocked symbol check
    if (limits_.blocked_symbols.contains(symbol, Qt::CaseInsensitive)) {
        results.append({false, RiskSeverity::Critical, "blocked_symbol", QString("Symbol %1 is blocked").arg(symbol)});
    } else {
        results.append({true, RiskSeverity::Info, "blocked_symbol", "OK"});
    }

    // Short selling check
    if (side == "sell" && !limits_.allow_short_selling) {
        results.append({false, RiskSeverity::Error, "short_selling", "Short selling is not allowed"});
    } else {
        results.append({true, RiskSeverity::Info, "short_selling", "OK"});
    }

    return results;
}

bool RiskManager::is_order_allowed(const QString& symbol, const QString& side, double quantity, double price,
                                   bool paper_trading, QString* reason_out) const {
    auto results = validate_order(symbol, side, quantity, price, paper_trading);
    QStringList reasons;
    for (const auto& r : results) {
        if (!r.passed && (r.severity == RiskSeverity::Error || r.severity == RiskSeverity::Critical))
            reasons.append(r.message);
    }
    if (reasons.isEmpty())
        return true;
    // Report WHICH limit bit — a bare "rejected" gives the user nothing to act on.
    LOG_INFO("RiskManager", QString("Order rejected by risk limits: %1 %2 x%3 @ %4 — %5")
                                .arg(side, symbol)
                                .arg(quantity)
                                .arg(price)
                                .arg(reasons.join("; ")));
    if (reason_out)
        *reason_out = reasons.join("; ");
    return false;
}

void RiskManager::record_trade(double pnl, double volume) {
    // Reset if new day
    QString today = QDate::currentDate().toString(Qt::ISODate);
    if (daily_stats_.date != today)
        reset_daily_stats();

    daily_stats_.realized_pnl += pnl;
    daily_stats_.trade_count++;
    daily_stats_.volume += volume;

    auto level = current_risk_level();
    emit risk_level_changed(level);

    if (level == RiskSeverity::Critical || level == RiskSeverity::Error) {
        RiskCheckResult breach;
        breach.passed = false;
        breach.severity = level;
        breach.check_name = "daily_stats";
        breach.message = QString("PnL: $%1, Trades: %2, Volume: $%3")
                             .arg(daily_stats_.realized_pnl, 0, 'f', 2)
                             .arg(daily_stats_.trade_count)
                             .arg(daily_stats_.volume, 0, 'f', 2);
        emit limit_breached(breach);
    }
}

void RiskManager::reset_daily_stats() {
    daily_stats_ = {};
    daily_stats_.date = QDate::currentDate().toString(Qt::ISODate);
}

RiskSeverity RiskManager::current_risk_level() const {
    // Unset limits (0) mean "no limit" — without these guards a zero daily_loss_limit
    // would report Critical the instant realized P&L went a cent negative.
    if (limits_.daily_loss_limit > 0) {
        if (daily_stats_.realized_pnl < -limits_.daily_loss_limit)
            return RiskSeverity::Critical;
        if (daily_stats_.realized_pnl < -limits_.daily_loss_limit * 0.8)
            return RiskSeverity::Error;
    }
    if (limits_.max_daily_trades > 0 && daily_stats_.trade_count > limits_.max_daily_trades * 0.8)
        return RiskSeverity::Warning;
    return RiskSeverity::Info;
}

} // namespace fincept::workflow
