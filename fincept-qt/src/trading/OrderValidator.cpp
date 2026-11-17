// OrderValidator.cpp — the PURE half of OrderValidator: order validation and the
// pre-trade risk rules. Includes only its own header (Qt Core + TradingTypes), so
// tests/tst_order_validator compiles it with Qt Core alone. Settings persistence,
// logging, the per-account snapshot and pre_trade_risk() live in OrderRiskGate.cpp.
#include "trading/OrderValidator.h"

#include <QLocale>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::trading {

namespace {

// File-local names carry an ovr_ prefix: the release build is a unity build, and
// OrderRiskGate.cpp (the shell half of this class) must never collide with them.

// Fallback amount text when the shell passes no formatter (this TU is Qt Core
// only, so it cannot use ui/formatting): "1,234.50", followed by the ISO code
// when the currency is known.
QString ovr_money(double v) {
    return QLocale(QLocale::English).toString(v, 'f', 2);
}

QString ovr_amount(const OrderValidator::RiskContext& ctx, double v, const QString& ccy) {
    if (ccy.isEmpty())
        return ovr_money(v);
    if (ctx.money_text)
        return ctx.money_text(v, ccy);
    return ovr_money(v) + QLatin1Char(' ') + ccy;
}

// An amount brought into the currency a rule compares in.
struct OvrConverted {
    bool ok = true;         // false: a conversion was needed and no rate is known
    bool converted = false; // a rate was applied
    double value = 0.0;     // in `to` when converted, else the input unchanged
    QString from;
    QString to;
    OrderValidator::FxRate rate;
};

// `from` or `to` unknown (empty) or the same code: nothing to convert (an unknown
// currency is taken to be the other one — the documented fallback). Otherwise
// the lookup's rate, or ok = false: a missing rate is never 1:1.
OvrConverted ovr_convert(double v, const QString& from, const QString& to, const OrderValidator::FxLookup& fx) {
    OvrConverted c;
    c.value = v;
    c.from = from;
    c.to = to;
    if (from.isEmpty() || to.isEmpty() || from == to)
        return c;
    std::optional<OrderValidator::FxRate> r;
    if (fx)
        r = fx(from, to);
    if (!r || !std::isfinite(r->rate) || r->rate <= 0.0) {
        c.ok = false;
        c.value = std::numeric_limits<double>::quiet_NaN();
        return c;
    }
    c.converted = true;
    c.rate = *r;
    c.value = v * r->rate;
    return c;
}

// The currency `c.value` is in.
QString ovr_value_ccy(const OvrConverted& c) {
    return c.converted ? c.to : (c.from.isEmpty() ? c.to : c.from);
}

QString ovr_rate_text(const OvrConverted& c) {
    QString t = c.rate.note;
    if (t.isEmpty()) {
        t = QStringLiteral("1 %1 = %2 %3").arg(c.from, QString::number(c.rate.rate, 'g', 6), c.to);
        if (c.rate.stale)
            t += QStringLiteral(", stale rate");
        if (c.rate.approximate)
            t += QStringLiteral(", approximate (1:1)");
    }
    return t;
}

// " (117,700.00 GBp at 1 GBP = 1.3183 USD · …)" after a converted amount, "" otherwise.
QString ovr_source_text(const OrderValidator::RiskContext& ctx, double source, const OvrConverted& c) {
    if (!c.converted)
        return {};
    return QStringLiteral(" (%1 at %2)").arg(ovr_amount(ctx, source, c.from), ovr_rate_text(c));
}

QString ovr_missing_rate(const OvrConverted& c) {
    return QStringLiteral("no %1 to %2 exchange rate is available yet (it is being fetched) — order refused; "
                          "try again in a few seconds")
        .arg(c.from, c.to);
}

// A limits currency as stored: 3-5 letters (ISO code, crypto / stablecoin code),
// upper-cased ("gbp" -> "GBP"; a minor unit such as "GBp" becomes its major code).
QString ovr_limits_currency(const QString& text) {
    const QString t = text.trimmed().toUpper();
    if (t.size() < 3 || t.size() > 5)
        return {};
    for (const QChar ch : t)
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z'))
            return {};
    return t;
}

constexpr const char* ovr_kKeyMaxValue = "risk.max_order_value";
constexpr const char* ovr_kKeyMaxQty = "risk.max_order_quantity";
constexpr const char* ovr_kKeyDailyLoss = "risk.daily_loss_limit";
constexpr const char* ovr_kKeySanity = "risk.sanity_checks";
constexpr const char* ovr_kKeyBlocked = "risk.blocked_symbols";
constexpr const char* ovr_kKeyLimitsCcy = "risk.limits_currency";

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

OrderValidator::ValidationResult OrderValidator::validate_smart(const SmartOrder& order) {
    ValidationResult r;

    if (order.symbol.trimmed().isEmpty())
        r.errors.append("Symbol is required");
    if (order.exchange.trimmed().isEmpty())
        r.errors.append("Exchange is required");
    else if (!is_valid_exchange(order.exchange))
        r.errors.append("Invalid exchange: " + order.exchange);

    // Non-finite numbers are rejected exactly as validate() rejects them. They used
    // to pass here: a NaN position_size made place_smart_order's risk quantity NaN,
    // its `quantity > 0` guard false, and so skipped the whole pre-trade risk gate
    // (blocked symbols, max quantity / value, daily loss) on the way to the broker.
    if (!std::isfinite(order.position_size))
        r.errors.append("Target position size must be a finite number");

    // position_size is the TARGET net position, and 0 is a legitimate target —
    // "flatten": the engine reads the live book and derives the quantity itself.
    // An explicit 0 is indistinguishable from an unset one, so requiring a
    // fallback quantity here would reject every flatten order. The engine already
    // reports the genuinely-empty case (no position AND no quantity) as a benign
    // "no action needed", so only a nonsensical negative fallback is an error.
    if (!std::isfinite(order.quantity))
        r.errors.append("Quantity must be a finite number");
    else if (order.quantity < 0)
        r.errors.append("Quantity cannot be negative");

    if (!std::isfinite(order.price) || order.price < 0 || !std::isfinite(order.trigger_price) ||
        order.trigger_price < 0)
        r.errors.append("Price / trigger must be finite and not negative");

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

// ── Pre-trade risk rules (pure) ───────────────────────────────────────────────

bool OrderValidator::is_derivative_exchange(const QString& exchange) {
    static const QSet<QString> kDerivative = {"NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX", "MCX_INDEX"};
    return kDerivative.contains(exchange.trimmed().toUpper());
}

OrderValidator::ValidationResult OrderValidator::check_risk(const UnifiedOrder& order, const RiskSettings& s,
                                                            const RiskContext& ctx) {
    ValidationResult r;
    const QString sym = order.symbol.trimmed().toUpper();
    const QString exch = order.exchange.trimmed().toUpper();

    // 0. Input integrity — independent of any setting. Every limit below is a
    //    `quantity > cap` comparison, which a NaN quantity passes; refuse it here
    //    instead of letting it through every configured cap (fail closed).
    if (!std::isfinite(order.quantity)) {
        r.errors.append(QStringLiteral("Quantity is not a finite number — order refused"));
        r.valid = false;
        return r;
    }

    // Order value in the instrument's currency as quoted (pence for an LSE share).
    const double notional = ctx.ref_price > 0.0 ? ctx.ref_price * order.quantity : 0.0;
    // Currencies (see "Currencies" in the header): an unknown instrument currency
    // is the account's; the money limits default to the account's currency.
    const QString acct_ccy = ctx.account_currency.trimmed();
    const QString px_ccy = ctx.price_currency.trimmed().isEmpty() ? acct_ccy : ctx.price_currency.trimmed();
    const QString limit_ccy = s.limits_currency.trimmed().isEmpty() ? acct_ccy : s.limits_currency.trimmed();
    // Buying power is in the currency the broker reported its funds in, when it said.
    const QString bp_ccy = ctx.funds_currency.trimmed().isEmpty() ? acct_ccy : ctx.funds_currency.trimmed();

    // 1. Absurd-order guard — on unless the user switched it off.
    if (s.sanity_checks) {
        if (order.quantity > kSanityMaxQuantity)
            r.errors.append(QString("Quantity %1 is beyond any sane single order (> %2) — rejected as a probable typo")
                                .arg(QLocale(QLocale::English).toString(order.quantity, 'f', 0))
                                .arg(QLocale(QLocale::English).toString(kSanityMaxQuantity, 'f', 0)));
        if (!ctx.reduces_position && !is_derivative_exchange(exch) && notional > 0.0 && ctx.buying_power > 0.0) {
            // Compare like with like: the order value in the buying power's currency.
            const OvrConverted v = ovr_convert(notional, px_ccy, bp_ccy, ctx.fx);
            if (!v.ok)
                r.errors.append(QString("Order value %1 cannot be compared with the available buying power %2: %3")
                                    .arg(ovr_amount(ctx, notional, px_ccy), ovr_amount(ctx, ctx.buying_power, bp_ccy),
                                         ovr_missing_rate(v)));
            else if (v.value > kSanityBuyingPowerMultiple * ctx.buying_power)
                r.errors.append(QString("Order value %1%2 is more than %3x the available buying power %4 — rejected "
                                        "as a probable typo (extra zero?)")
                                    .arg(ovr_amount(ctx, v.value, ovr_value_ccy(v)), ovr_source_text(ctx, notional, v))
                                    .arg(kSanityBuyingPowerMultiple, 0, 'f', 0)
                                    .arg(ovr_amount(ctx, ctx.buying_power, bp_ccy)));
        }
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
            if (notional <= 0.0) {
                // Fail closed: a configured cap that cannot be evaluated is not a pass.
                r.errors.append(QString("Max order value %1 is set but no reference price is known for this order — "
                                        "use a LIMIT order or wait for quotes to load")
                                    .arg(ovr_amount(ctx, s.max_order_value, limit_ccy)));
            } else {
                const OvrConverted v = ovr_convert(notional, px_ccy, limit_ccy, ctx.fx);
                if (!v.ok)
                    // Same rule: a cap that cannot be evaluated is not a pass.
                    r.errors.append(QString("Max order value %1 is set but the order value %2 cannot be converted: %3")
                                        .arg(ovr_amount(ctx, s.max_order_value, limit_ccy),
                                             ovr_amount(ctx, notional, px_ccy), ovr_missing_rate(v)));
                else if (v.value > s.max_order_value)
                    r.errors.append(QString("Order value %1%2 exceeds your max order value %3 (Risk limits)")
                                        .arg(ovr_amount(ctx, v.value, ovr_value_ccy(v)), ovr_source_text(ctx, notional, v),
                                             ovr_amount(ctx, s.max_order_value, limit_ccy)));
            }
        }

        if (s.daily_loss_limit > 0.0) {
            if (!ctx.day_pnl_known) {
                r.errors.append(QString("Daily loss limit %1 is set but today's P&L for this account is not loaded "
                                        "yet — open the account in Equity Trading, then retry")
                                    .arg(ovr_amount(ctx, s.daily_loss_limit, limit_ccy)));
            } else if (!std::isfinite(ctx.day_pnl)) {
                // A NaN P&L (one bad position row) compares false against the limit
                // and used to let every order through. Treat it as unknown.
                r.errors.append(QString("Daily loss limit %1 is set but today's P&L for this account could not be "
                                        "computed (a position reports no valid P&L) — order refused")
                                    .arg(ovr_amount(ctx, s.daily_loss_limit, limit_ccy)));
            } else {
                // Day P&L by the currency each part is in (the account's unless the
                // positions said otherwise); the limit may be in yet another. Every
                // part is converted on its own — P&L in two currencies is never added raw.
                QVector<QPair<QString, double>> parts;
                if (ctx.day_pnl_by_currency.isEmpty()) {
                    parts.append({acct_ccy, ctx.day_pnl});
                } else {
                    for (auto it = ctx.day_pnl_by_currency.cbegin(); it != ctx.day_pnl_by_currency.cend(); ++it)
                        parts.append({it.key().trimmed().isEmpty() ? acct_ccy : it.key().trimmed(), it.value()});
                    std::sort(parts.begin(), parts.end()); // stable message order
                }
                if (parts.size() == 1) {
                    const OvrConverted p = ovr_convert(parts.first().second, parts.first().first, limit_ccy, ctx.fx);
                    if (!p.ok)
                        r.errors.append(QString("Daily loss limit %1 is set but today's P&L %2 cannot be converted: %3")
                                            .arg(ovr_amount(ctx, s.daily_loss_limit, limit_ccy),
                                                 ovr_amount(ctx, parts.first().second, parts.first().first),
                                                 ovr_missing_rate(p)));
                    else if (p.value <= -s.daily_loss_limit)
                        r.errors.append(QString("Daily loss limit reached: today's P&L %1%2 <= -%3. Only orders that "
                                                "reduce an open position are allowed until tomorrow (Risk limits)")
                                            .arg(ovr_amount(ctx, p.value, ovr_value_ccy(p)),
                                                 ovr_source_text(ctx, parts.first().second, p),
                                                 ovr_amount(ctx, s.daily_loss_limit, limit_ccy)));
                } else {
                    double total = 0.0;
                    QStringList shown;
                    QString missing;
                    for (const auto& part : parts) {
                        const OvrConverted p = ovr_convert(part.second, part.first, limit_ccy, ctx.fx);
                        if (!p.ok) {
                            missing = ovr_missing_rate(p);
                            break;
                        }
                        total += p.value;
                        shown << ovr_amount(ctx, part.second, part.first) + (p.converted ? QStringLiteral(" at ") +
                                                                                               ovr_rate_text(p)
                                                                                         : QString());
                    }
                    const QString limit_ccy_text = limit_ccy.isEmpty() ? acct_ccy : limit_ccy;
                    if (!missing.isEmpty())
                        r.errors.append(QString("Daily loss limit %1 is set but today's P&L (in %2 currencies) cannot be "
                                                "converted: %3")
                                            .arg(ovr_amount(ctx, s.daily_loss_limit, limit_ccy))
                                            .arg(parts.size())
                                            .arg(missing));
                    else if (total <= -s.daily_loss_limit)
                        r.errors.append(QString("Daily loss limit reached: today's P&L %1 (%2) <= -%3. Only orders that "
                                                "reduce an open position are allowed until tomorrow (Risk limits)")
                                            .arg(ovr_amount(ctx, total, limit_ccy_text), shown.join(QStringLiteral(" + ")),
                                                 ovr_amount(ctx, s.daily_loss_limit, limit_ccy)));
                }
            }
        }
    }

    r.valid = r.errors.isEmpty();
    return r;
}

OrderValidator::RiskContext OrderValidator::build_risk_context(const UnifiedOrder& order, bool broker_routed,
                                                               const RiskSnapshot* snapshot) {
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

    if (!snapshot)
        return ctx;

    if (snapshot->funds_known) {
        ctx.buying_power = snapshot->available;
        ctx.funds_currency = snapshot->funds_currency.trimmed();
    }
    if (ctx.ref_price <= 0.0)
        ctx.ref_price = snapshot->ltp.value(sym, 0.0);
    if (snapshot->positions_known) {
        ctx.day_pnl_known = true;
        bool any_row_ccy = false;
        for (int i = 0; i < snapshot->positions.size(); ++i) {
            const auto& p = snapshot->positions.at(i);
            ctx.day_pnl += p.pnl;
            // A row without its own currency is in the funds' (else the account's) currency.
            QString row_ccy = snapshot->position_currencies.value(i).trimmed();
            any_row_ccy = any_row_ccy || !row_ccy.isEmpty();
            if (row_ccy.isEmpty())
                row_ccy = snapshot->funds_currency.trimmed();
            ctx.day_pnl_by_currency[row_ccy] += p.pnl;
            const bool same_instrument =
                p.symbol.trimmed().toUpper() == sym &&
                (exch.isEmpty() || p.exchange.isEmpty() || p.exchange.trimmed().toUpper() == exch);
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
        // Nothing said which currency: all of day_pnl is the account's (the documented fallback).
        if (!any_row_ccy && snapshot->funds_currency.trimmed().isEmpty())
            ctx.day_pnl_by_currency.clear();
    }
    return ctx;
}

OrderValidator::RiskSettings OrderValidator::parse_risk_settings(const QVector<QPair<QString, QString>>& rows) {
    RiskSettings s;
    for (const auto& kv : rows) {
        const QString v = kv.second.trimmed();
        if (kv.first == QLatin1String(ovr_kKeyMaxValue))
            s.max_order_value = std::max(0.0, v.toDouble());
        else if (kv.first == QLatin1String(ovr_kKeyMaxQty))
            s.max_order_quantity = std::max(0.0, v.toDouble());
        else if (kv.first == QLatin1String(ovr_kKeyDailyLoss))
            s.daily_loss_limit = std::fabs(v.toDouble());
        else if (kv.first == QLatin1String(ovr_kKeySanity))
            s.sanity_checks = (v != QLatin1String("0"));
        else if (kv.first == QLatin1String(ovr_kKeyLimitsCcy))
            s.limits_currency = ovr_limits_currency(v);
        else if (kv.first == QLatin1String(ovr_kKeyBlocked)) {
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
    return s;
}

QVector<QPair<QString, QString>> OrderValidator::serialize_risk_settings(const RiskSettings& s) {
    return {
        {QString::fromLatin1(ovr_kKeyMaxValue), QString::number(s.max_order_value, 'f', 2)},
        {QString::fromLatin1(ovr_kKeyMaxQty), QString::number(s.max_order_quantity, 'f', 0)},
        {QString::fromLatin1(ovr_kKeyDailyLoss), QString::number(s.daily_loss_limit, 'f', 2)},
        {QString::fromLatin1(ovr_kKeySanity), s.sanity_checks ? QStringLiteral("1") : QStringLiteral("0")},
        {QString::fromLatin1(ovr_kKeyBlocked), s.blocked_symbols.join(QLatin1Char(','))},
        // Always written, so choosing "each account's currency" again clears the row.
        {QString::fromLatin1(ovr_kKeyLimitsCcy), ovr_limits_currency(s.limits_currency)},
    };
}

} // namespace fincept::trading
