#include "services/workflow/RiskManager.h"

#include "core/logging/Logger.h"
#include "services/markets/FxRateService.h"
#include "ui/formatting/NumberFormat.h"

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

namespace {

// File-local names carry an rm_ prefix (unity build).

QString rm_money(double v, const QString& ccy) {
    return fincept::ui::formatting::money(v, ccy);
}

// The order value brought into the limits' currency.
struct RmValue {
    bool ok = true;  // false: a conversion was needed and no rate is known
    double value = 0;
    QString ccy{};   // currency of `value` ("" = unknown)
    QString note{};  // rate and time when converted
};

RmValue rm_in_limits_ccy(double value, const QString& price_ccy, const QString& limits_ccy) {
    RmValue r;
    r.value = value;
    r.ccy = price_ccy.isEmpty() ? limits_ccy : price_ccy; // unknown price currency = the limits'
    if (limits_ccy.isEmpty() || price_ccy.isEmpty() || price_ccy == limits_ccy)
        return r; // nothing to convert (no limits currency: compared in its own currency)
    const auto c = services::FxRateService::instance().convert_for_display(value, price_ccy, limits_ccy);
    if (!c.ok()) {
        r.ok = false; // never 1:1; the leg has been requested
        return r;
    }
    r.value = c.amount;
    r.ccy = limits_ccy;
    r.note = c.converted ? fincept::ui::formatting::fx_note(c) : QString();
    return r;
}

QString rm_missing_rate(const QString& from, const QString& to) {
    return QStringLiteral("no %1 to %2 exchange rate is available yet (it is being fetched) — order refused; try "
                          "again in a few seconds")
        .arg(from, to);
}

} // namespace

QVector<RiskCheckResult> RiskManager::validate_order(const QString& symbol, const QString& side, double quantity,
                                                     double price, bool paper_trading) const {
    return validate_order(symbol, side, quantity, price, paper_trading, QString());
}

QVector<RiskCheckResult> RiskManager::validate_order(const QString& symbol, const QString& side, double quantity,
                                                     double price, bool paper_trading,
                                                     const QString& price_currency) const {
    Q_UNUSED(paper_trading);
    QVector<RiskCheckResult> results;
    // Order value in the instrument's currency as quoted (pence for an LSE share),
    // then in the limits' currency for every money limit.
    const QString price_ccy = price_currency.trimmed();
    const QString limits_ccy = limits_.limits_currency.trimmed().toUpper();
    const double raw_value = quantity * price;
    const bool money_limit_set =
        limits_.max_position_value > 0 || limits_.max_single_order_value > 0 || limits_.max_daily_volume > 0;
    const RmValue v = money_limit_set ? rm_in_limits_ccy(raw_value, price_ccy, limits_ccy) : RmValue{true, raw_value};
    const double order_value = v.value;
    const QString value_ccy = v.ccy;
    const QString limit_unit = limits_ccy.isEmpty() ? value_ccy : limits_ccy;
    auto value_text = [&]() {
        QString t = rm_money(order_value, value_ccy);
        if (!v.note.isEmpty())
            t += QStringLiteral(" (%1 at %2)").arg(rm_money(raw_value, price_ccy), v.note);
        return t;
    };
    auto unconvertible = [&](const QString& limit_name, double limit) {
        return QString("%1 %2 is set but the order value %3 cannot be converted: %4")
            .arg(limit_name, rm_money(limit, limits_ccy), rm_money(raw_value, price_ccy),
                 rm_missing_rate(price_ccy, limits_ccy));
    };

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

    // Position value check (a set limit that cannot be evaluated is not a pass)
    if (limits_.max_position_value > 0 && !v.ok) {
        results.append({false, RiskSeverity::Error, "position_value",
                        unconvertible(QStringLiteral("Max position value"), limits_.max_position_value)});
    } else if (limits_.max_position_value > 0 && order_value > limits_.max_position_value) {
        results.append({false, RiskSeverity::Error, "position_value",
                        QString("Value %1 exceeds max %2")
                            .arg(value_text(), rm_money(limits_.max_position_value, limit_unit))});
    } else {
        results.append({true, RiskSeverity::Info, "position_value", "OK"});
    }

    // Single order value check
    if (limits_.max_single_order_value > 0 && !v.ok) {
        results.append({false, RiskSeverity::Error, "order_value",
                        unconvertible(QStringLiteral("Single order limit"), limits_.max_single_order_value)});
    } else if (limits_.max_single_order_value > 0 && order_value > limits_.max_single_order_value) {
        results.append({false, RiskSeverity::Error, "order_value",
                        QString("Order value %1 exceeds single order limit %2")
                            .arg(value_text(), rm_money(limits_.max_single_order_value, limit_unit))});
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

    // Daily volume check (the day's volume is recorded in the limits' currency)
    if (limits_.max_daily_volume > 0 && !v.ok) {
        results.append({false, RiskSeverity::Warning, "daily_volume",
                        unconvertible(QStringLiteral("Daily volume limit"), limits_.max_daily_volume)});
    } else if (limits_.max_daily_volume > 0 && eff_volume + order_value > limits_.max_daily_volume) {
        results.append({false, RiskSeverity::Warning, "daily_volume",
                        QString("Daily volume would exceed the %1 limit")
                            .arg(rm_money(limits_.max_daily_volume, limit_unit))});
    } else {
        results.append({true, RiskSeverity::Info, "daily_volume", "OK"});
    }

    // Daily loss limit check (realized P&L is recorded in the limits' currency)
    if (limits_.daily_loss_limit > 0 && eff_realized_pnl < -limits_.daily_loss_limit) {
        results.append({false, RiskSeverity::Critical, "daily_loss",
                        QString("Daily loss %1 exceeds limit %2")
                            .arg(rm_money(-eff_realized_pnl, limits_ccy), rm_money(limits_.daily_loss_limit, limits_ccy))});
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
    return is_order_allowed(symbol, side, quantity, price, paper_trading, reason_out, QString());
}

bool RiskManager::is_order_allowed(const QString& symbol, const QString& side, double quantity, double price,
                                   bool paper_trading, QString* reason_out, const QString& price_currency) const {
    auto results = validate_order(symbol, side, quantity, price, paper_trading, price_currency);
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
        const QString ccy = limits_.limits_currency.trimmed().toUpper();
        breach.message = QString("PnL: %1, Trades: %2, Volume: %3")
                             .arg(rm_money(daily_stats_.realized_pnl, ccy))
                             .arg(daily_stats_.trade_count)
                             .arg(rm_money(daily_stats_.volume, ccy));
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
