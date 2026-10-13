#include "trading/OrderValidator.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QHash>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

namespace {

constexpr const char* kOvLog = "OrderValidator";

// Thread-safe snapshot of what the risk gate needs per account. The source caches
// (AccountDataStream) are GUI-thread-only, while orders are placed from executor
// threads, so DataStreamManager mirrors funds / positions / last prices in here.
struct OvAccountSnapshot {
    double available = 0.0;
    bool funds_known = false;
    QVector<BrokerPosition> positions;
    bool positions_known = false;
    QHash<QString, double> ltp; // symbol (upper) -> last price
};

QMutex& ov_snapshot_mutex() {
    static QMutex m;
    return m;
}

QHash<QString, OvAccountSnapshot>& ov_snapshots() {
    static QHash<QString, OvAccountSnapshot> s;
    return s;
}

QString ov_money(double v) {
    return QLocale(QLocale::English).toString(v, 'f', 2);
}

} // namespace

const QSet<QString>& OrderValidator::valid_exchanges() {
    static const QSet<QString> kExchanges = {
        // India
        "NSE", "BSE", "NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX", "NSE_INDEX", "BSE_INDEX", "MCX_INDEX",
        // US / global
        "NYSE", "NASDAQ", "AMEX", "ARCA", "BATS", "CBOE", "LSE", "TSX", "XETRA", "EURONEXT", "XNYS", "XNAS",
        // crypto / forex
        "CRYPTO", "FOREX"};
    return kExchanges;
}

bool OrderValidator::is_valid_exchange(const QString& exchange) {
    return valid_exchanges().contains(exchange.toUpper());
}

bool OrderValidator::requires_price(OrderType type) {
    return type == OrderType::Limit || type == OrderType::StopLossLimit;
}

bool OrderValidator::requires_trigger_price(OrderType type) {
    return type == OrderType::StopLoss || type == OrderType::StopLossLimit;
}

OrderValidator::ValidationResult OrderValidator::validate(const UnifiedOrder& order) {
    ValidationResult r;

    if (order.symbol.trimmed().isEmpty())
        r.errors.append("Symbol is required");
    if (order.exchange.trimmed().isEmpty())
        r.errors.append("Exchange is required");
    else if (!is_valid_exchange(order.exchange))
        r.errors.append("Invalid exchange: " + order.exchange);

    // NaN compares false against everything, so a NaN quantity used to slip past
    // "quantity <= 0" and reach the broker serializer. Reject non-finite numbers.
    if (!std::isfinite(order.quantity) || order.quantity <= 0)
        r.errors.append("Quantity must be a positive number");

    if (!std::isfinite(order.price) || order.price < 0 || !std::isfinite(order.stop_price) || order.stop_price < 0)
        r.errors.append("Price / trigger must be finite and not negative");

    if (requires_price(order.order_type) && order.price <= 0)
        r.errors.append("Price is required for LIMIT / SL-Limit orders");

    if (requires_trigger_price(order.order_type) && order.stop_price <= 0)
        r.errors.append("Trigger price is required for SL / SL-M orders");

    r.valid = r.errors.isEmpty();
    return r;
}

// ── Pre-trade risk gate ───────────────────────────────────────────────────────

bool OrderValidator::is_derivative_exchange(const QString& exchange) {
    static const QSet<QString> kDerivative = {"NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX", "MCX_INDEX"};
    return kDerivative.contains(exchange.trimmed().toUpper());
}

OrderValidator::ValidationResult OrderValidator::check_risk(const UnifiedOrder& order, const RiskSettings& s,
                                                            const RiskContext& ctx) {
    ValidationResult r;
    const QString sym = order.symbol.trimmed().toUpper();
    const QString exch = order.exchange.trimmed().toUpper();
    const double notional = ctx.ref_price > 0.0 ? ctx.ref_price * order.quantity : 0.0;

    // 1. Absurd-order guard — on unless the user switched it off.
    if (s.sanity_checks) {
        if (order.quantity > kSanityMaxQuantity)
            r.errors.append(QString("Quantity %1 is beyond any sane single order (> %2) — rejected as a probable typo")
                                .arg(QLocale(QLocale::English).toString(order.quantity, 'f', 0))
                                .arg(QLocale(QLocale::English).toString(kSanityMaxQuantity, 'f', 0)));
        if (!ctx.reduces_position && !is_derivative_exchange(exch) && notional > 0.0 && ctx.buying_power > 0.0 &&
            notional > kSanityBuyingPowerMultiple * ctx.buying_power)
            r.errors.append(QString("Order value %1 is more than %2x the available buying power %3 — rejected as a "
                                    "probable typo (extra zero?)")
                                .arg(ov_money(notional))
                                .arg(kSanityBuyingPowerMultiple, 0, 'f', 0)
                                .arg(ov_money(ctx.buying_power)));
    }

    // 2. Configured limits apply to broker-routed orders that ADD risk. Closing /
    //    reducing an existing position is always allowed — a limit must never trap
    //    the user in the position it exists to cap.
    if (ctx.broker_routed && !ctx.reduces_position) {
        if (!s.blocked_symbols.isEmpty() &&
            (s.blocked_symbols.contains(sym) || (!exch.isEmpty() && s.blocked_symbols.contains(exch + ":" + sym))))
            r.errors.append(QString("%1 is on your blocked-symbols list (Risk limits)").arg(order.symbol));

        if (s.max_order_quantity > 0.0 && order.quantity > s.max_order_quantity)
            r.errors.append(QString("Quantity %1 exceeds your max order quantity %2 (Risk limits)")
                                .arg(order.quantity)
                                .arg(s.max_order_quantity));

        if (s.max_order_value > 0.0) {
            if (notional <= 0.0)
                // Fail closed: a configured cap that cannot be evaluated is not a pass.
                r.errors.append(QString("Max order value %1 is set but no reference price is known for this order — "
                                        "use a LIMIT order or wait for quotes to load")
                                    .arg(ov_money(s.max_order_value)));
            else if (notional > s.max_order_value)
                r.errors.append(QString("Order value %1 exceeds your max order value %2 (Risk limits)")
                                    .arg(ov_money(notional), ov_money(s.max_order_value)));
        }

        if (s.daily_loss_limit > 0.0) {
            if (!ctx.day_pnl_known)
                r.errors.append(QString("Daily loss limit %1 is set but today's P&L for this account is not loaded "
                                        "yet — open the account in Equity Trading, then retry")
                                    .arg(ov_money(s.daily_loss_limit)));
            else if (ctx.day_pnl <= -s.daily_loss_limit)
                r.errors.append(QString("Daily loss limit reached: today's P&L %1 <= -%2. Only orders that reduce an "
                                        "open position are allowed until tomorrow (Risk limits)")
                                    .arg(ov_money(ctx.day_pnl), ov_money(s.daily_loss_limit)));
        }
    }

    r.valid = r.errors.isEmpty();
    return r;
}

OrderValidator::RiskSettings OrderValidator::load_risk_settings(bool* ok_out) {
    RiskSettings s;
    auto res = SettingsRepository::instance().get_by_category(QStringLiteral("risk"));
    if (res.is_err()) {
        LOG_ERROR(kOvLog, QString("Risk settings unreadable: %1").arg(QString::fromStdString(res.error())));
        if (ok_out)
            *ok_out = false;
        return s;
    }
    for (const auto& kv : res.value()) {
        const QString v = kv.value.trimmed();
        if (kv.key == QLatin1String("risk.max_order_value"))
            s.max_order_value = std::max(0.0, v.toDouble());
        else if (kv.key == QLatin1String("risk.max_order_quantity"))
            s.max_order_quantity = std::max(0.0, v.toDouble());
        else if (kv.key == QLatin1String("risk.daily_loss_limit"))
            s.daily_loss_limit = std::fabs(v.toDouble());
        else if (kv.key == QLatin1String("risk.sanity_checks"))
            s.sanity_checks = (v != QLatin1String("0"));
        else if (kv.key == QLatin1String("risk.blocked_symbols")) {
            for (const QString& part : v.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                const QString t = part.trimmed().toUpper();
                if (!t.isEmpty())
                    s.blocked_symbols << t;
            }
        }
    }
    if (!std::isfinite(s.max_order_value))
        s.max_order_value = 0.0;
    if (!std::isfinite(s.max_order_quantity))
        s.max_order_quantity = 0.0;
    if (!std::isfinite(s.daily_loss_limit))
        s.daily_loss_limit = 0.0;
    if (ok_out)
        *ok_out = true;
    return s;
}

bool OrderValidator::save_risk_settings(const RiskSettings& s) {
    auto& repo = SettingsRepository::instance();
    const QString cat = QStringLiteral("risk");
    bool ok = repo.set("risk.max_order_value", QString::number(s.max_order_value, 'f', 2), cat).is_ok();
    ok = repo.set("risk.max_order_quantity", QString::number(s.max_order_quantity, 'f', 0), cat).is_ok() && ok;
    ok = repo.set("risk.daily_loss_limit", QString::number(s.daily_loss_limit, 'f', 2), cat).is_ok() && ok;
    ok = repo.set("risk.sanity_checks", s.sanity_checks ? "1" : "0", cat).is_ok() && ok;
    ok = repo.set("risk.blocked_symbols", s.blocked_symbols.join(QLatin1Char(',')), cat).is_ok() && ok;
    LOG_INFO(kOvLog, QString("Risk limits saved: max value %1, max qty %2, daily loss %3, %4 blocked, sanity %5")
                         .arg(s.max_order_value)
                         .arg(s.max_order_quantity)
                         .arg(s.daily_loss_limit)
                         .arg(s.blocked_symbols.size())
                         .arg(s.sanity_checks ? "on" : "off"));
    return ok;
}

OrderValidator::ValidationResult OrderValidator::pre_trade_risk(const QString& account_id, const UnifiedOrder& order,
                                                                bool broker_routed) {
    bool settings_ok = true;
    const RiskSettings s = load_risk_settings(&settings_ok);
    if (!settings_ok) {
        ValidationResult r;
        r.valid = false;
        r.errors << QStringLiteral("Risk settings could not be read — order refused (fail closed)");
        return r;
    }

    RiskContext ctx;
    ctx.broker_routed = broker_routed;
    const QString sym = order.symbol.trimmed().toUpper();
    const QString exch = order.exchange.trimmed().toUpper();
    if ((order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) && order.price > 0.0)
        ctx.ref_price = order.price;
    else if (order.order_type == OrderType::StopLoss && order.stop_price > 0.0)
        ctx.ref_price = order.stop_price;
    else if (order.price > 0.0)
        ctx.ref_price = order.price; // paper market orders carry the LTP here

    {
        QMutexLocker lock(&ov_snapshot_mutex());
        const auto it = ov_snapshots().constFind(account_id);
        if (it != ov_snapshots().constEnd()) {
            const OvAccountSnapshot& snap = it.value();
            if (snap.funds_known)
                ctx.buying_power = snap.available;
            if (ctx.ref_price <= 0.0)
                ctx.ref_price = snap.ltp.value(sym, 0.0);
            if (snap.positions_known) {
                ctx.day_pnl_known = true;
                for (const auto& p : snap.positions) {
                    ctx.day_pnl += p.pnl;
                    const bool same_instrument = p.symbol.trimmed().toUpper() == sym &&
                                                 (exch.isEmpty() || p.exchange.isEmpty() ||
                                                  p.exchange.trimmed().toUpper() == exch);
                    if (!same_instrument || p.quantity == 0.0)
                        continue;
                    if (ctx.ref_price <= 0.0 && p.ltp > 0.0)
                        ctx.ref_price = p.ltp;
                    // Reduces only when it is the closing side, in the SAME product
                    // (Indian brokers net per product) and no larger than the position.
                    if (closing_side_for_position(p.side, p.quantity) == order.side &&
                        product_from_broker_str(p.product_type) == order.product_type &&
                        order.quantity <= std::abs(p.quantity) + 1e-9)
                        ctx.reduces_position = true;
                }
            }
        }
    }

    ValidationResult r = check_risk(order, s, ctx);
    if (!r.valid)
        LOG_WARN(kOvLog, QString("Pre-trade risk gate refused %1 %2 x%3 on %4: %5")
                             .arg(QString::fromLatin1(order_side_str(order.side)), order.symbol)
                             .arg(order.quantity)
                             .arg(account_id, r.errors.join("; ")));
    return r;
}

void OrderValidator::note_funds(const QString& account_id, double available_balance) {
    if (account_id.isEmpty() || !std::isfinite(available_balance))
        return;
    QMutexLocker lock(&ov_snapshot_mutex());
    auto& snap = ov_snapshots()[account_id];
    snap.available = available_balance;
    snap.funds_known = true;
}

void OrderValidator::note_positions(const QString& account_id, const QVector<BrokerPosition>& positions) {
    if (account_id.isEmpty())
        return;
    QMutexLocker lock(&ov_snapshot_mutex());
    auto& snap = ov_snapshots()[account_id];
    snap.positions = positions;
    snap.positions_known = true;
}

void OrderValidator::note_quote(const QString& account_id, const QString& symbol, double ltp) {
    if (account_id.isEmpty() || symbol.isEmpty() || !(ltp > 0.0) || !std::isfinite(ltp))
        return;
    QMutexLocker lock(&ov_snapshot_mutex());
    ov_snapshots()[account_id].ltp.insert(symbol.trimmed().toUpper(), ltp);
}

OrderValidator::ValidationResult OrderValidator::validate_smart(const SmartOrder& order) {
    ValidationResult r;

    if (order.symbol.trimmed().isEmpty())
        r.errors.append("Symbol is required");
    if (order.exchange.trimmed().isEmpty())
        r.errors.append("Exchange is required");
    else if (!is_valid_exchange(order.exchange))
        r.errors.append("Invalid exchange: " + order.exchange);

    // position_size is the TARGET net position, and 0 is a legitimate target —
    // "flatten": the engine reads the live book and derives the quantity itself.
    // An explicit 0 is indistinguishable from an unset one, so requiring a
    // fallback quantity here would reject every flatten order. The engine already
    // reports the genuinely-empty case (no position AND no quantity) as a benign
    // "no action needed", so only a nonsensical negative fallback is an error.
    if (order.quantity < 0)
        r.errors.append("Quantity cannot be negative");

    if (requires_price(order.order_type) && order.price <= 0)
        r.errors.append("Price is required for LIMIT / SL-Limit orders");

    if (requires_trigger_price(order.order_type) && order.trigger_price <= 0)
        r.errors.append("Trigger price is required for SL / SL-M orders");

    r.valid = r.errors.isEmpty();
    return r;
}

OrderValidator::ValidationResult OrderValidator::validate_basket(const BasketOrderRequest& basket) {
    ValidationResult r;

    if (basket.orders.isEmpty()) {
        r.errors.append("Basket has no orders");
        r.valid = false;
        return r;
    }

    for (int i = 0; i < basket.orders.size(); ++i) {
        auto leg = validate(basket.orders[i]);
        if (!leg.valid) {
            for (const auto& e : leg.errors)
                r.errors.append(QString("Order %1 (%2): %3").arg(i + 1).arg(basket.orders[i].symbol, e));
        }
    }

    r.valid = r.errors.isEmpty();
    return r;
}

} // namespace fincept::trading
