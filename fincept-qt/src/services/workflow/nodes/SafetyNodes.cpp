#include "services/workflow/nodes/SafetyNodes.h"

#include "services/workflow/NodeRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

namespace fincept::workflow {

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
                {"max_volatility",   "Max Volatility",  "number", 0.5, {}, "Max annualized volatility (0-1)"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject trade = data.isObject() ? data.toObject() : QJsonObject{};

                double max_pos_pct  = params.value("max_position_pct").toDouble(5.0);
                double max_vol      = params.value("max_volatility").toDouble(0.5);

                double quantity        = trade.value("quantity").toDouble(0);
                double price           = trade.value("price").toDouble(0);
                double portfolio_value = trade.value("portfolio_value").toDouble(0);
                double volatility      = trade.value("volatility").toDouble(0);

                QStringList failures;

                if (portfolio_value > 0 && price > 0) {
                    double position_value = quantity * price;
                    double position_pct   = (position_value / portfolio_value) * 100.0;
                    if (position_pct > max_pos_pct)
                        failures << QString("Position size %1% exceeds max %2%")
                                        .arg(position_pct, 0, 'f', 2)
                                        .arg(max_pos_pct, 0, 'f', 2);
                }

                if (volatility > 0 && volatility > max_vol)
                    failures << QString("Volatility %1 exceeds max %2")
                                    .arg(volatility, 0, 'f', 3)
                                    .arg(max_vol, 0, 'f', 3);

                QJsonObject out = trade;
                out["risk_check_passed"] = failures.isEmpty();
                if (!failures.isEmpty()) {
                    QJsonArray fa;
                    for (const QString& f : failures) fa.append(f);
                    out["risk_failures"] = fa;
                    cb(false, out, failures.join("; "));
                } else {
                    cb(true, out, {});
                }
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
                {"output_fail", "Over Limit",  PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"daily_limit",  "Daily Loss Limit",  "number", 1000, {}, "$ amount"},
                {"weekly_limit", "Weekly Loss Limit", "number", 5000, {}, "$ amount"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double daily_limit  = params.value("daily_limit").toDouble(1000);
                double weekly_limit = params.value("weekly_limit").toDouble(5000);
                double daily_pnl    = obj.value("daily_pnl").toDouble(0);
                double weekly_pnl   = obj.value("weekly_pnl").toDouble(0);

                QStringList failures;
                if (-daily_pnl > daily_limit)
                    failures << QString("Daily loss $%1 exceeds limit $%2")
                                    .arg(-daily_pnl, 0, 'f', 2).arg(daily_limit, 0, 'f', 2);
                if (-weekly_pnl > weekly_limit)
                    failures << QString("Weekly loss $%1 exceeds limit $%2")
                                    .arg(-weekly_pnl, 0, 'f', 2).arg(weekly_limit, 0, 'f', 2);

                obj["loss_limit_passed"] = failures.isEmpty();
                if (!failures.isEmpty()) {
                    QJsonArray fa; for (const QString& f : failures) fa.append(f);
                    obj["loss_failures"] = fa;
                    cb(false, obj, failures.join("; "));
                } else {
                    cb(true, obj, {});
                }
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
                {"max_shares", "Max Shares",     "number", 1000,  {}, ""},
                {"max_value",  "Max Value ($)",  "number", 50000, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double max_shares = params.value("max_shares").toDouble(1000);
                double max_value  = params.value("max_value").toDouble(50000);
                double quantity   = obj.value("quantity").toDouble(0);
                double price      = obj.value("price").toDouble(0);
                double trade_val  = quantity * price;

                QStringList failures;
                if (quantity > max_shares)
                    failures << QString("Quantity %1 exceeds max %2").arg(quantity).arg(max_shares);
                if (price > 0 && trade_val > max_value)
                    failures << QString("Trade value $%1 exceeds max $%2")
                                    .arg(trade_val, 0, 'f', 2).arg(max_value, 0, 'f', 2);

                obj["size_limit_passed"] = failures.isEmpty();
                if (!failures.isEmpty()) {
                    QJsonArray fa; for (const QString& f : failures) fa.append(f);
                    obj["size_failures"] = fa;
                    cb(false, obj, failures.join("; "));
                } else {
                    cb(true, obj, {});
                }
            },
    });

    // ── Trading Hours Check ───────────────────────────────────────
    // Uses QDateTime in UTC to check exchange trading hours.
    registry.register_type({
        .type_id = "safety.trading_hours",
        .display_name = "Trading Hours Check",
        .category = "Safety",
        .description = "Validate trade is within market hours",
        .icon_text = "!",
        .accent_color = "#dc2626",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_open",   "Market Open",   PortDirection::Output, ConnectionType::Main},
                {"output_closed", "Market Closed", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"exchange",       "Exchange",         "select", "NYSE", {"NYSE", "NASDAQ", "LSE", "TSE", "NSE", "BSE"}, ""},
                {"allow_premarket","Allow Pre-Market",  "boolean", false, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                QString exchange      = params.value("exchange").toString("NYSE");
                bool allow_premarket  = params.value("allow_premarket").toBool(false);

                QDateTime utc_now = QDateTime::currentDateTimeUtc();
                int day_of_week   = utc_now.date().dayOfWeek(); // 1=Mon, 7=Sun
                int hour_utc      = utc_now.time().hour();
                int min_utc       = utc_now.time().minute();
                int time_utc      = hour_utc * 100 + min_utc;

                // Weekend check (universal)
                bool is_weekend = (day_of_week == 6 || day_of_week == 7);

                // Session hours in UTC
                // NYSE/NASDAQ: 14:30-21:00 UTC (pre-market 09:00-14:30)
                // LSE: 08:00-16:30 UTC
                // TSE (Tokyo): 00:00-06:00 UTC (09:00-15:00 JST)
                // NSE/BSE (India): 03:45-10:00 UTC (09:15-15:30 IST)
                struct Session { int open; int close; int pre_open; };
                Session session{1430, 2100, 900};
                if      (exchange == "LSE")                  session = {800, 1630, 700};
                else if (exchange == "TSE")                  session = {0,   600,  2300};
                else if (exchange == "NSE" || exchange == "BSE") session = {345, 1000, 300};

                bool in_regular   = !is_weekend && time_utc >= session.open  && time_utc < session.close;
                bool in_premarket = !is_weekend && time_utc >= session.pre_open && time_utc < session.open;
                bool is_open      = in_regular || (allow_premarket && in_premarket);

                obj["market_open"]  = is_open;
                obj["exchange"]     = exchange;
                obj["utc_time"]     = utc_now.toString("HH:mm");
                obj["session_type"] = in_regular ? "regular" : (in_premarket ? "pre_market" : "closed");

                if (is_open)
                    cb(true, obj, {});
                else
                    cb(false, obj, QString("Market closed: %1 UTC on %2")
                                       .arg(utc_now.toString("HH:mm"), exchange));
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
                {"output_fail", "Exceeded",    PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_drawdown_pct", "Max Drawdown %", "number", 10.0, {}, ""},
                {"window",           "Window",         "select", "daily", {"daily", "weekly", "monthly", "ytd"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double max_dd = params.value("max_drawdown_pct").toDouble(10.0);

                // Accept either drawdown_pct directly or peak/current values
                double drawdown_pct = obj.value("drawdown_pct").toDouble(-1);
                if (drawdown_pct < 0) {
                    double peak    = obj.value("peak_value").toDouble(0);
                    double current = obj.value("current_value").toDouble(0);
                    drawdown_pct = (peak > 0) ? ((peak - current) / peak * 100.0) : 0;
                }

                obj["drawdown_pct"]         = drawdown_pct;
                obj["max_drawdown_pct"]     = max_dd;
                obj["drawdown_check_passed"]= drawdown_pct <= max_dd;

                if (drawdown_pct > max_dd)
                    cb(false, obj, QString("Drawdown %1% exceeds max %2%")
                                       .arg(drawdown_pct, 0, 'f', 2).arg(max_dd, 0, 'f', 2));
                else
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
                {"output_pass", "Low Correlation",  PortDirection::Output, ConnectionType::Main},
                {"output_fail", "High Correlation", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_correlation", "Max Correlation", "number", 0.8, {}, "0-1 threshold"},
                {"lookback_days",   "Lookback Days",   "number", 90,  {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                double max_corr = params.value("max_correlation").toDouble(0.8);
                double corr     = std::abs(obj.value("correlation").toDouble(0));

                obj["correlation_check_passed"] = corr <= max_corr;
                obj["correlation_abs"]          = corr;

                if (corr > max_corr)
                    cb(false, obj, QString("Correlation %1 exceeds max %2")
                                       .arg(corr, 0, 'f', 3).arg(max_corr, 0, 'f', 3));
                else
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
                {"output_fail", "High Vol",   PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_annualized_vol", "Max Ann. Vol %", "number", 50.0, {}, ""},
                {"method",             "Method",         "select", "realized", {"realized", "implied", "vix_level"}, ""},
                {"vix_threshold",      "VIX Threshold",  "number", 30.0, {}, "For VIX method"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject obj = data.isObject() ? data.toObject() : QJsonObject{};

                QString method      = params.value("method").toString("realized");
                double max_vol      = params.value("max_annualized_vol").toDouble(50.0);
                double vix_threshold= params.value("vix_threshold").toDouble(30.0);

                bool exceeded = false;
                QString reason;

                if (method == "vix_level") {
                    double vix = obj.value("vix").toDouble(0);
                    exceeded = vix > vix_threshold;
                    reason = QString("VIX %1 > threshold %2").arg(vix).arg(vix_threshold);
                } else {
                    // realized or implied: expect annualized_vol as 0-100 percentage
                    double vol_pct = obj.value("annualized_vol").toDouble(0);
                    exceeded = vol_pct > max_vol;
                    reason = QString("Ann. vol %1% > max %2%").arg(vol_pct, 0, 'f', 1).arg(max_vol, 0, 'f', 1);
                }

                obj["volatility_check_passed"] = !exceeded;
                if (exceeded)
                    cb(false, obj, reason);
                else
                    cb(true, obj, {});
            },
    });
}

} // namespace fincept::workflow
