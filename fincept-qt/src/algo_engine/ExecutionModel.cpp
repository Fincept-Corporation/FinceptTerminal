// src/algo_engine/ExecutionModel.cpp
#include "algo_engine/ExecutionModel.h"

#include <QCoreApplication>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace fincept::algo {

namespace {

constexpr double kExecMinPrice = 1e-9;
// Tolerance when flooring a quantity to its lot: 2.9999999999 lots is 3 lots.
constexpr double kExecLotEps = 1e-9;

double exec_clean(double v) {
    return std::isfinite(v) ? v : 0.0;
}

QString exec_tr(const char* text) {
    return QCoreApplication::translate("ExecutionModel", text);
}

} // namespace

// ── CostModel ────────────────────────────────────────────────────────────────

bool CostModel::is_zero() const {
    return commission_pct == 0 && commission_per_unit == 0 && commission_per_order == 0 && commission_min == 0 &&
           buy_tax_pct == 0 && sell_tax_pct == 0 && slippage_bps == 0 && slippage_atr_frac == 0 &&
           borrow_pct_annual == 0;
}

QJsonObject CostModel::to_json() const {
    QJsonObject o;
    o["preset"] = preset;
    o["commission_pct"] = commission_pct;
    o["commission_per_unit"] = commission_per_unit;
    o["commission_per_order"] = commission_per_order;
    o["commission_min"] = commission_min;
    o["commission_max"] = commission_max;
    o["buy_tax_pct"] = buy_tax_pct;
    o["sell_tax_pct"] = sell_tax_pct;
    o["slippage_bps"] = slippage_bps;
    o["slippage_atr_frac"] = slippage_atr_frac;
    o["lot_size"] = lot_size;
    o["borrow_pct_annual"] = borrow_pct_annual;
    return o;
}

CostModel CostModel::from_json(const QJsonObject& o) {
    CostModel m;
    if (o.isEmpty())
        return m;
    auto num = [&o](const char* k, double def) {
        const QJsonValue v = o.value(QLatin1String(k));
        const double d = v.isDouble() ? v.toDouble() : def;
        return std::isfinite(d) ? std::max(0.0, d) : def;
    };
    m.preset = o.value("preset").toString(QStringLiteral("custom"));
    m.commission_pct = num("commission_pct", 0);
    m.commission_per_unit = num("commission_per_unit", 0);
    m.commission_per_order = num("commission_per_order", 0);
    m.commission_min = num("commission_min", 0);
    m.commission_max = num("commission_max", 0);
    m.buy_tax_pct = num("buy_tax_pct", 0);
    m.sell_tax_pct = num("sell_tax_pct", 0);
    m.slippage_bps = num("slippage_bps", 0);
    m.slippage_atr_frac = num("slippage_atr_frac", 0);
    m.lot_size = num("lot_size", 1);
    if (m.lot_size <= 0)
        m.lot_size = 1;
    m.borrow_pct_annual = num("borrow_pct_annual", 0);
    return m;
}

bool CostModel::operator==(const CostModel& o) const {
    return commission_pct == o.commission_pct && commission_per_unit == o.commission_per_unit &&
           commission_per_order == o.commission_per_order && commission_min == o.commission_min &&
           commission_max == o.commission_max && buy_tax_pct == o.buy_tax_pct && sell_tax_pct == o.sell_tax_pct &&
           slippage_bps == o.slippage_bps && slippage_atr_frac == o.slippage_atr_frac && lot_size == o.lot_size &&
           borrow_pct_annual == o.borrow_pct_annual;
}

namespace execution {

double slipped_price(const CostModel& m, FillSide side, double ref_price, double atr) {
    ref_price = exec_clean(ref_price);
    if (ref_price <= 0)
        return 0.0;
    const double atr_part = (std::isfinite(atr) && atr > 0) ? atr * m.slippage_atr_frac : 0.0;
    const double bps_part = ref_price * m.slippage_bps / 10000.0;
    const double move = std::max(0.0, bps_part + atr_part);
    const double px = side == FillSide::Buy ? ref_price + move : ref_price - move;
    return std::max(kExecMinPrice, px);
}

FeeBreakdown fees(const CostModel& m, FillSide side, double qty, double price) {
    FeeBreakdown f;
    qty = std::abs(exec_clean(qty));
    price = std::abs(exec_clean(price));
    if (qty <= 0 || price <= 0)
        return f;
    const double value = qty * price;
    double c = value * m.commission_pct / 100.0 + qty * m.commission_per_unit + m.commission_per_order;
    if (c > 0 || m.commission_min > 0)
        c = std::max(c, m.commission_min);
    if (m.commission_max > 0)
        c = std::min(c, m.commission_max);
    f.commission = std::max(0.0, c);
    f.taxes = value * (side == FillSide::Buy ? m.buy_tax_pct : m.sell_tax_pct) / 100.0;
    return f;
}

double round_quantity(const CostModel& m, double qty) {
    qty = exec_clean(qty);
    if (qty <= 0)
        return 0.0;
    const double lot = m.lot_size > 0 ? m.lot_size : 1.0;
    const double lots = std::floor(qty / lot + kExecLotEps);
    if (lots <= 0)
        return 0.0;
    // Re-derive from the lot count so 0.1 + 0.2 style errors do not leak into
    // the quantity (0.30000000000000004 units of a 0.1 step).
    const double out = lots * lot;
    if (lot < 1.0) {
        const double scale = std::pow(10.0, std::ceil(-std::log10(lot)) + 2);
        return std::round(out * scale) / scale;
    }
    return out;
}

double max_affordable_qty(const CostModel& m, FillSide side, double cash, double price) {
    cash = exec_clean(cash);
    price = exec_clean(price);
    if (cash <= 0 || price <= 0)
        return 0.0;
    const double lot = m.lot_size > 0 ? m.lot_size : 1.0;
    // Closed-form estimate ignoring the commission floor / cap, then step down a
    // lot at a time until the order (with its fees) fits.
    const double pct = (m.commission_pct + (side == FillSide::Buy ? m.buy_tax_pct : m.sell_tax_pct)) / 100.0;
    const double per_unit_cost = price * (1.0 + pct) + m.commission_per_unit;
    const double fixed = std::max(m.commission_per_order, m.commission_min);
    double qty = round_quantity(m, (cash - fixed) / per_unit_cost);
    auto cost = [&](double q) { return q * price + fees(m, side, q, price).total(); };
    for (int guard = 0; qty > 0 && cost(qty) > cash + 1e-9 && guard < 64; ++guard)
        qty = round_quantity(m, qty - lot);
    if (qty > 0 && cost(qty) > cash + 1e-9) {
        // Pathological fee shape (huge per-order fee vs tiny lot): bisect the lot count.
        double lo = 0, hi = std::floor(qty / lot);
        while (hi - lo > 1) {
            const double mid = std::floor((lo + hi) / 2);
            if (cost(mid * lot) <= cash)
                lo = mid;
            else
                hi = mid;
        }
        qty = round_quantity(m, lo * lot);
    }
    return std::max(0.0, qty);
}

Fill fill(const CostModel& m, FillSide side, double ref_price, double qty, double atr, bool apply_slippage) {
    Fill f;
    f.side = side;
    f.ref_price = exec_clean(ref_price);
    f.qty = std::abs(exec_clean(qty));
    f.price = apply_slippage ? slipped_price(m, side, f.ref_price, atr) : f.ref_price;
    if (!f.valid())
        return f;
    f.notional = f.price * f.qty;
    f.fees = fees(m, side, f.qty, f.price);
    f.slippage_cost = std::abs(f.price - f.ref_price) * f.qty;
    return f;
}

double borrow_cost(const CostModel& m, double notional, double days) {
    if (m.borrow_pct_annual <= 0 || !(days > 0))
        return 0.0;
    return std::abs(exec_clean(notional)) * m.borrow_pct_annual / 100.0 * days / 365.0;
}

QVector<CostPreset> presets() {
    QVector<CostPreset> out;

    {
        CostPreset p;
        p.id = QStringLiteral("none");
        p.label = exec_tr("No costs (gross)");
        p.note = exec_tr("No commission, taxes or slippage. Results are GROSS and will overstate live performance.");
        p.model.preset = p.id;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("us_equity_zero");
        p.label = exec_tr("US shares - commission-free broker (approx.)");
        p.note = exec_tr("Approximation: no commission, SEC fee ~0.00278 % on sells, 2 bps slippage. Check your "
                         "broker's schedule.");
        p.region = QStringLiteral("US");
        p.model.sell_tax_pct = 0.00278;
        p.model.slippage_bps = 2;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("us_equity_per_share");
        p.label = exec_tr("US shares - per-share commission (approx.)");
        p.note = exec_tr("Approximation: 0.005 per share, minimum 1.00 per order, SEC fee ~0.00278 % on sells, "
                         "2 bps slippage.");
        p.region = QStringLiteral("US");
        p.model.commission_per_unit = 0.005;
        p.model.commission_min = 1.0;
        p.model.sell_tax_pct = 0.00278;
        p.model.slippage_bps = 2;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("in_equity_delivery");
        p.label = exec_tr("India shares - delivery (approx.)");
        p.note = exec_tr("Approximation for NSE/BSE delivery: zero brokerage, STT 0.1 % both sides, stamp duty "
                         "0.015 % on buys, exchange + SEBI fees and GST. Excludes depository charges. 3 bps "
                         "slippage.");
        p.region = QStringLiteral("IN");
        p.model.buy_tax_pct = 0.1186;
        p.model.sell_tax_pct = 0.1036;
        p.model.slippage_bps = 3;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("in_equity_intraday");
        p.label = exec_tr("India shares - intraday (approx.)");
        p.note = exec_tr("Approximation for NSE/BSE intraday: brokerage 0.03 % capped at 20 per order plus GST, "
                         "STT 0.025 % on sells, stamp duty 0.003 % on buys, exchange + SEBI fees. 3 bps slippage.");
        p.region = QStringLiteral("IN");
        p.model.commission_pct = 0.0354;
        p.model.commission_max = 23.6;
        p.model.buy_tax_pct = 0.0066;
        p.model.sell_tax_pct = 0.0286;
        p.model.slippage_bps = 3;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("uk_equity");
        p.label = exec_tr("UK shares (approx.)");
        p.note = exec_tr("Approximation: commission 0.1 % of value, 0.5 % stamp duty (SDRT) on buys, 5 bps "
                         "slippage. AIM shares and ETFs are exempt from stamp duty.");
        p.region = QStringLiteral("GB");
        p.model.commission_pct = 0.1;
        p.model.buy_tax_pct = 0.5;
        p.model.slippage_bps = 5;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("eu_equity");
        p.label = exec_tr("Euro-area shares (approx.)");
        p.note = exec_tr("Approximation: commission 0.1 % of value, 5 bps slippage. Add the financial transaction "
                         "tax where it applies (France 0.4 %, Italy 0.1 % on buys of large caps).");
        p.region = QStringLiteral("EU");
        p.model.commission_pct = 0.1;
        p.model.slippage_bps = 5;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("hk_equity");
        p.label = exec_tr("Hong Kong shares (approx.)");
        p.note = exec_tr("Approximation: commission 0.03 %, stamp duty 0.1 % plus trading fee and levies "
                         "(~0.11 % each side), 5 bps slippage. Set the lot size to the stock's board lot.");
        p.region = QStringLiteral("HK");
        p.model.commission_pct = 0.03;
        p.model.buy_tax_pct = 0.1086;
        p.model.sell_tax_pct = 0.1086;
        p.model.slippage_bps = 5;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("jp_equity");
        p.label = exec_tr("Japan shares (approx.)");
        p.note = exec_tr("Approximation: no commission (common at online brokers), trading unit 100 shares, 3 bps "
                         "slippage.");
        p.region = QStringLiteral("JP");
        p.model.lot_size = 100;
        p.model.slippage_bps = 3;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("crypto_spot");
        p.label = exec_tr("Crypto spot - taker (approx.)");
        p.note = exec_tr("Approximation: 0.1 % taker fee per side, fractional quantities (0.0001), 5 bps "
                         "slippage.");
        p.region = QStringLiteral("CRYPTO");
        p.model.commission_pct = 0.1;
        p.model.lot_size = 0.0001;
        p.model.slippage_bps = 5;
        out.append(p);
    }
    {
        CostPreset p;
        p.id = QStringLiteral("fx_spot");
        p.label = exec_tr("FX spot - spread only (approx.)");
        p.note = exec_tr("Approximation: no commission, 1 bp slippage standing in for half the spread.");
        p.region = QStringLiteral("FX");
        p.model.slippage_bps = 1;
        out.append(p);
    }

    for (auto& p : out)
        p.model.preset = p.id;
    return out;
}

CostPreset preset(const QString& id) {
    const auto all = presets();
    for (const auto& p : all)
        if (p.id == id)
            return p;
    return all.first();
}

QString default_preset_for(const QString& venue_or_country, bool intraday) {
    const QString k = venue_or_country.trimmed().toUpper();
    if (k.isEmpty())
        return QStringLiteral("none");
    if (k == QLatin1String("NYSE") || k == QLatin1String("NASDAQ") || k == QLatin1String("US"))
        return QStringLiteral("us_equity_zero");
    if (k == QLatin1String("NSE") || k == QLatin1String("BSE") || k == QLatin1String("IN"))
        return intraday ? QStringLiteral("in_equity_intraday") : QStringLiteral("in_equity_delivery");
    if (k == QLatin1String("LSE") || k == QLatin1String("GB") || k == QLatin1String("UK"))
        return QStringLiteral("uk_equity");
    if (k == QLatin1String("HKEX") || k == QLatin1String("HK"))
        return QStringLiteral("hk_equity");
    if (k == QLatin1String("TSE") || k == QLatin1String("JP"))
        return QStringLiteral("jp_equity");
    if (k == QLatin1String("CRYPTO"))
        return QStringLiteral("crypto_spot");
    if (k == QLatin1String("FX"))
        return QStringLiteral("fx_spot");
    static const char* const kEuro[] = {"EU", "DE", "FR", "NL", "BE", "IT", "ES", "PT", "IE", "AT", "FI",
                                        "XETRA", "EURONEXT", "BIT", "BME"};
    for (const char* e : kEuro)
        if (k == QLatin1String(e))
            return QStringLiteral("eu_equity");
    return QStringLiteral("none");
}

} // namespace execution
} // namespace fincept::algo
