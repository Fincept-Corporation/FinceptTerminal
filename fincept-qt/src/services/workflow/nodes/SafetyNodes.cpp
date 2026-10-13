#include "services/workflow/nodes/SafetyNodes.h"

#include "services/workflow/NodeRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimeZone>

#include <cmath>

namespace fincept::workflow {

namespace safety_nodes_detail {

// A gate that cannot see the number it is meant to check must not wave a trade
// through: true when `key` is present in the payload as a finite number.
bool has_number(const QJsonObject& obj, const char* key) {
    const QJsonValue v = obj.value(QLatin1String(key));
    return v.isDouble() && std::isfinite(v.toDouble());
}

} // namespace safety_nodes_detail

void register_safety_nodes(NodeRegistry& registry) {

    // ── Risk Check ────────────────────────────────────────────────
    // Input: { symbol, quantity, price, portfolio_value, volatility }
    // Checks position size % and volatility against thresholds.
    registry.register_type({
        .type_id = "safety.risk_check",
        .display_name = "Risk Check",
        .category = "Safety",
        .description = "Validate trade against position size and volatility limits",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Trade In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_pass", "Pass", PortDirection::Output, ConnectionType::Main},
                {"output_fail", "Fail", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_position_pct", "Max Position %", "number", 5.0, {}, "Max % of portfolio per position"},
                {"max_volatility", "Max Volatility", "number", 0.5, {}, "Max annualized volatility (0-1)"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject trade = data.isObject() ? data.toObject() : QJsonObject{};

                double max_pos_pct = params.value("max_position_pct").toDouble(5.0);
                double max_vol = params.value("max_volatility").toDouble(0.5);

                double quantity = trade.value("quantity").toDouble(0);
                double price = trade.value("price").toDouble(0);
                double portfolio_value = trade.value("portfolio_value").toDouble(0);
                double volatility = trade.value("volatility").toDouble(0);

                QStringList failures;

                // Fail CLOSED when the data needed to size the position is
                // missing: a risk gate that can't evaluate its rule because the
                // upstream portfolio_value/price is absent must not wave the
                // trade through. The old code skipped the check (fail-open).
                // The quantity must be in the payload too: a missing quantity read as 0
                // shares = 0% of the portfolio, so the gate passed whatever the order
                // node downstream was about to send.
                if (portfolio_value <= 0 || price <= 0 || quantity <= 0) {
                    failures << QString("Cannot verify position size — missing %1")
                                    .arg(portfolio_value <= 0 ? QStringLiteral("portfolio_value")
                                         : price <= 0         ? QStringLiteral("price")
                                                              : QStringLiteral("quantity"));
                } else {
                    double position_value = quantity * price;
                    double position_pct = (position_value / portfolio_value) * 100.0;
                    if (position_pct > max_pos_pct)
                        failures << QString("Position size %1% exceeds max %2%")
                                        .arg(position_pct, 0, 'f', 2)
                                        .arg(max_pos_pct, 0, 'f', 2);
                }

                if (volatility > 0 && volatility > max_vol)
                    failures
                        << QString("Volatility %1 exceeds max %2").arg(volatility, 0, 'f', 3).arg(max_vol, 0, 'f', 3);

                QJsonObject out = trade;
                bool passed = failures.isEmpty();
                out["risk_check_passed"] = passed;
                out["_branch"] = passed ? "true" : "false";
                if (!passed) {
                    QJsonArray fa;
                    for (const QString& f : failures)
                        fa.append(f);
                    out["risk_failures"] = fa;
                }
                cb(true, out, {});
            },
    });

    // ── Loss Limit ────────────────────────────────────────────────
    // Input: { daily_pnl, weekly_pnl } — checks against configured limits.
    registry.register_type({
        .type_id = "safety.loss_limit",
        .display_name = "Loss Limit",
        .category = "Safety",
        .description = "Enforce daily/weekly loss limits",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_pass", "Under Limit", PortDirection::Output, ConnectionType::Main},
                {"output_fail", "Over Limit", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"daily_limit", "Daily Loss Limit", "number", 1000, {}, "$ amount"},
                {"weekly_limit", "Weekly Loss Limit", "number", 5000, {}, "$ amount"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double daily_limit = params.value("daily_limit").toDouble(1000);
                double weekly_limit = params.value("weekly_limit").toDouble(5000);
                double daily_pnl = obj.value("daily_pnl").toDouble(0);
                double weekly_pnl = obj.value("weekly_pnl").toDouble(0);

                QStringList failures;
                // Fail closed: with neither P&L figure in the payload the old gate compared
                // 0 against the limits and always passed.
                if (!safety_nodes_detail::has_number(obj, "daily_pnl") &&
                    !safety_nodes_detail::has_number(obj, "weekly_pnl"))
                    failures << QStringLiteral("Cannot verify loss limits — upstream data has no daily_pnl or "
                                               "weekly_pnl");
                if (-daily_pnl > daily_limit)
                    failures << QString("Daily loss $%1 exceeds limit $%2")
                                    .arg(-daily_pnl, 0, 'f', 2)
                                    .arg(daily_limit, 0, 'f', 2);
                if (-weekly_pnl > weekly_limit)
                    failures << QString("Weekly loss $%1 exceeds limit $%2")
                                    .arg(-weekly_pnl, 0, 'f', 2)
                                    .arg(weekly_limit, 0, 'f', 2);

                bool passed = failures.isEmpty();
                obj["loss_limit_passed"] = passed;
                obj["_branch"] = passed ? "true" : "false";
                if (!passed) {
                    QJsonArray fa;
                    for (const QString& f : failures)
                        fa.append(f);
                    obj["loss_failures"] = fa;
                }
                cb(true, obj, {});
            },
    });

    // ── Position Size Limit ───────────────────────────────────────
    // Input: { quantity, price }
    registry.register_type({
        .type_id = "safety.position_size_limit",
        .display_name = "Position Size Limit",
        .category = "Safety",
        .description = "Enforce maximum position sizing",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_pass", "Pass", PortDirection::Output, ConnectionType::Main},
                {"output_fail", "Fail", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_shares", "Max Shares", "number", 1000, {}, ""},
                {"max_value", "Max Value ($)", "number", 50000, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double max_shares = params.value("max_shares").toDouble(1000);
                double max_value = params.value("max_value").toDouble(50000);
                double quantity = obj.value("quantity").toDouble(0);
                double price = obj.value("price").toDouble(0);
                double trade_val = quantity * price;

                QStringList failures;
                if (quantity <= 0)
                    failures << QStringLiteral("Cannot verify position size — upstream data has no quantity");
                if (quantity > max_shares)
                    failures << QString("Quantity %1 exceeds max %2").arg(quantity).arg(max_shares);
                if (price > 0 && trade_val > max_value)
                    failures << QString("Trade value $%1 exceeds max $%2")
                                    .arg(trade_val, 0, 'f', 2)
                                    .arg(max_value, 0, 'f', 2);

                bool passed = failures.isEmpty();
                obj["size_limit_passed"] = passed;
                obj["_branch"] = passed ? "true" : "false";
                if (!passed) {
                    QJsonArray fa;
                    for (const QString& f : failures)
                        fa.append(f);
                    obj["size_failures"] = fa;
                }
                cb(true, obj, {});
            },
    });

    // ── Trading Hours Check ───────────────────────────────────────
    // Uses QDateTime in UTC to check exchange trading hours.
    registry.register_type({
        .type_id = "safety.trading_hours",
        .display_name = "Trading Hours Check",
        .category = "Safety",
        .description = "Validate trade is within regular exchange hours (local time, DST-aware; holidays not checked)",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_open", "Market Open", PortDirection::Output, ConnectionType::Main},
                {"output_closed", "Market Closed", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"exchange", "Exchange", "select", "NYSE", {"NYSE", "NASDAQ", "LSE", "TSE", "NSE", "BSE"}, ""},
                {"allow_premarket", "Allow Pre-Market", "boolean", false, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                QString exchange = params.value("exchange").toString("NYSE");
                bool allow_premarket = params.value("allow_premarket").toBool(false);

                // Evaluate in the exchange's own time zone. The old fixed UTC windows ignored
                // daylight saving: for half the year NYSE was "closed" for its first hour and
                // "open" for an hour after the 16:00 ET close.
                struct Session {
                    const char* tz;
                    int pre_open; // minutes after local midnight
                    int open;
                    int close;
                    int lunch_start; // -1 = no lunch break
                    int lunch_end;
                };
                Session ses{"America/New_York", 4 * 60, 9 * 60 + 30, 16 * 60, -1, -1}; // NYSE / NASDAQ
                if (exchange == "LSE")
                    ses = {"Europe/London", 7 * 60 + 50, 8 * 60, 16 * 60 + 30, -1, -1};
                else if (exchange == "TSE")
                    ses = {"Asia/Tokyo", 8 * 60, 9 * 60, 15 * 60 + 30, 11 * 60 + 30, 12 * 60 + 30};
                else if (exchange == "NSE" || exchange == "BSE")
                    ses = {"Asia/Kolkata", 9 * 60, 9 * 60 + 15, 15 * 60 + 30, -1, -1};

                const QTimeZone tz(QByteArray(ses.tz));
                const QDateTime utc_now = QDateTime::currentDateTimeUtc();
                const QDateTime local = tz.isValid() ? utc_now.toTimeZone(tz) : utc_now;
                const int minute = local.time().hour() * 60 + local.time().minute();
                const int dow = local.date().dayOfWeek(); // 1=Mon, 7=Sun
                const bool is_weekend = (dow == 6 || dow == 7);

                const bool in_lunch = ses.lunch_start >= 0 && minute >= ses.lunch_start && minute < ses.lunch_end;
                const bool in_regular = !is_weekend && minute >= ses.open && minute < ses.close && !in_lunch;
                const bool in_premarket = !is_weekend && minute >= ses.pre_open && minute < ses.open;
                const bool is_open = tz.isValid() && (in_regular || (allow_premarket && in_premarket));

                obj["local_time"] = local.toString("yyyy-MM-dd HH:mm");
                obj["time_zone"] = QString::fromLatin1(ses.tz);
                obj["utc_time"] = utc_now.toString("HH:mm");
                obj["session_type"] = in_regular     ? "regular"
                                      : in_premarket ? "pre_market"
                                      : in_lunch     ? "lunch_break"
                                                     : "closed";
                obj["holidays_checked"] = false; // exchange holidays are not known to this gate
                if (!tz.isValid())
                    obj["error"] = QStringLiteral("Time zone %1 unavailable — treated as closed")
                                       .arg(QString::fromLatin1(ses.tz));
                obj["market_open"] = is_open;
                obj["exchange"] = exchange;
                // Route: output_open = true branch, output_closed = false branch
                obj["_branch"] = is_open ? "true" : "false";
                cb(true, obj, {});
            },
    });

    // ── Max Drawdown Check ────────────────────────────────────────
    // Input: { peak_value, current_value } or { drawdown_pct }
    registry.register_type({
        .type_id = "safety.max_drawdown_check",
        .display_name = "Max Drawdown Check",
        .category = "Safety",
        .description = "Halt trading if drawdown exceeds threshold",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_pass", "Under Limit", PortDirection::Output, ConnectionType::Main},
                {"output_fail", "Exceeded", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_drawdown_pct", "Max Drawdown %", "number", 10.0, {}, ""},
                {"window", "Window", "select", "daily", {"daily", "weekly", "monthly", "ytd"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double max_dd = params.value("max_drawdown_pct").toDouble(10.0);

                // Accept either drawdown_pct directly or peak/current values
                double drawdown_pct = obj.value("drawdown_pct").toDouble(-1);
                bool known = drawdown_pct >= 0;
                if (!known) {
                    double peak = obj.value("peak_value").toDouble(0);
                    double current = obj.value("current_value").toDouble(0);
                    known = peak > 0 && safety_nodes_detail::has_number(obj, "current_value");
                    drawdown_pct = known ? ((peak - current) / peak * 100.0) : 0;
                }

                // Unknown drawdown used to count as 0% and pass.
                bool passed = known && drawdown_pct <= max_dd;
                if (!known)
                    obj["drawdown_failure"] = "Cannot verify drawdown — need drawdown_pct or peak_value + current_value";
                obj["drawdown_pct"] = drawdown_pct;
                obj["max_drawdown_pct"] = max_dd;
                obj["drawdown_check_passed"] = passed;
                obj["_branch"] = passed ? "true" : "false";
                cb(true, obj, {});
            },
    });

    // ── Correlation Check ─────────────────────────────────────────
    // Input: { correlation } — checks against threshold.
    registry.register_type({
        .type_id = "safety.correlation_check",
        .display_name = "Correlation Check",
        .category = "Safety",
        .description = "Block trade if too correlated with existing positions",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_pass", "Low Correlation", PortDirection::Output, ConnectionType::Main},
                {"output_fail", "High Correlation", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_correlation", "Max Correlation", "number", 0.8, {}, "0-1 threshold"},
                {"lookback_days", "Lookback Days", "number", 90, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double max_corr = params.value("max_correlation").toDouble(0.8);
                double corr = std::abs(obj.value("correlation").toDouble(0));
                const bool known = safety_nodes_detail::has_number(obj, "correlation");

                bool passed = known && corr <= max_corr; // missing correlation used to pass as 0
                if (!known)
                    obj["correlation_failure"] = "Cannot verify correlation — upstream data has no correlation";
                obj["correlation_check_passed"] = passed;
                obj["correlation_abs"] = corr;
                obj["_branch"] = passed ? "true" : "false";
                cb(true, obj, {});
            },
    });

    // ── Volatility Filter ─────────────────────────────────────────
    // Input: { annualized_vol } or { vix }
    registry.register_type({
        .type_id = "safety.volatility_filter",
        .display_name = "Volatility Filter",
        .category = "Safety",
        .description = "Skip trade if volatility is too high",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_pass", "Normal Vol", PortDirection::Output, ConnectionType::Main},
                {"output_fail", "High Vol", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_annualized_vol", "Max Ann. Vol %", "number", 50.0, {}, ""},
                {"method", "Method", "select", "realized", {"realized", "implied", "vix_level"}, ""},
                {"vix_threshold", "VIX Threshold", "number", 30.0, {}, "For VIX method"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                QString method = params.value("method").toString("realized");
                double max_vol = params.value("max_annualized_vol").toDouble(50.0);
                double vix_threshold = params.value("vix_threshold").toDouble(30.0);

                bool exceeded = false;
                QString reason;

                // A missing reading used to be 0 and pass; now it blocks, and says why.
                if (method == "vix_level") {
                    double vix = obj.value("vix").toDouble(0);
                    if (!safety_nodes_detail::has_number(obj, "vix")) {
                        exceeded = true;
                        reason = QStringLiteral("Cannot verify volatility — upstream data has no vix");
                    } else {
                        exceeded = vix > vix_threshold;
                        reason = QString("VIX %1 vs threshold %2").arg(vix).arg(vix_threshold);
                    }
                } else {
                    // realized or implied: expect annualized_vol as 0-100 percentage
                    double vol_pct = obj.value("annualized_vol").toDouble(0);
                    if (!safety_nodes_detail::has_number(obj, "annualized_vol")) {
                        exceeded = true;
                        reason = QStringLiteral("Cannot verify volatility — upstream data has no annualized_vol");
                    } else {
                        exceeded = vol_pct > max_vol;
                        reason =
                            QString("Ann. vol %1% vs max %2%").arg(vol_pct, 0, 'f', 1).arg(max_vol, 0, 'f', 1);
                    }
                }

                obj["volatility_check_passed"] = !exceeded;
                obj["volatility_reason"] = reason;
                obj["_branch"] = exceeded ? "false" : "true";
                cb(true, obj, {});
            },
    });
}

} // namespace fincept::workflow
