// src/algo_engine/fno/FnoExecution.cpp
#include "algo_engine/fno/FnoExecution.h"
#include "algo_engine/fno/FnoLegResolver.h"

#include <QJsonObject>

#include <algorithm>

namespace fincept::algo::fno {

// ---------------------------------------------------------------------------
// resolve_expiry
// ---------------------------------------------------------------------------

// Parse "DD-MMM-YY" (uppercase month, e.g. "26-JUN-25") to a QDate.
//
// Two issues to handle:
//   1. Qt6's QDate::fromString("dd-MMM-yy") uses the C locale which expects
//      title-case month abbreviations ("Jun" not "JUN").
//   2. Qt6 interprets "yy" as 1900+yy (so "25" → 1925, not 2025). We fix
//      this by parsing the 4-digit year explicitly after converting "YY" to
//      "20YY" for years 00-99.
//
// Approach: convert "DD-MMM-YY" → "DD-MMM-20YY" and parse with "dd-MMM-yyyy".
static QDate parse_expiry_date(const QString& s) {
    // Expected format: "DD-MMM-YY" (length 9, e.g. "26-JUN-25").
    if (s.length() >= 9 && s[2] == QChar('-') && s[6] == QChar('-')) {
        const QString day   = s.left(2);
        const QString month = s.mid(3, 3);  // e.g. "JUN"
        const QString yy    = s.mid(7, 2);  // e.g. "25"

        // Title-case the month (Qt C-locale fromString expects "Jun" not "JUN").
        const QString month_tc = month.left(1).toUpper() + month.mid(1).toLower();

        // Expand 2-digit year to 4-digit (20xx) — all NSE/BSE options expire well within 2099.
        const QString four_digit = QStringLiteral("20") + yy;

        const QString normalised = day + QChar('-') + month_tc + QChar('-') + four_digit;
        const QDate d = QDate::fromString(normalised, QStringLiteral("dd-MMM-yyyy"));
        if (d.isValid())
            return d;
    }
    // Fallback for other formats (yyyy-MM-dd, already title-cased, etc.).
    QDate d = QDate::fromString(s, QStringLiteral("dd-MMM-yy"));
    // Fix Qt "yy" → 1900+yy issue in fallback path.
    if (d.isValid() && d.year() < 2000)
        d = d.addYears(100);
    return d;
}

QString resolve_expiry(const QString& mode, const QString& value,
                       const QStringList& available_expiries,
                       const QDate& today) {
    if (available_expiries.isEmpty())
        return QString();

    // ABSOLUTE: return the value directly (it is an explicit "DD-MMM-YY" string).
    // We still validate it parses — if it doesn't we return it as-is (best-effort).
    if (mode == QLatin1String("ABSOLUTE")) {
        return value;
    }

    // Build a sorted list of (date, original-string) for future expiries only.
    using DateStr = std::pair<QDate, QString>;
    QVector<DateStr> future;
    future.reserve(available_expiries.size());
    for (const QString& e : available_expiries) {
        const QDate d = parse_expiry_date(e);
        if (d.isValid() && d >= today)
            future.append({d, e});
    }
    if (future.isEmpty())
        return QString();

    std::sort(future.begin(), future.end(),
              [](const DateStr& a, const DateStr& b) { return a.first < b.first; });

    if (mode == QLatin1String("WEEKLY")) {
        // Nearest future expiry.
        return future.first().second;
    }

    if (mode == QLatin1String("NEAREST_DTE")) {
        const int min_dte = value.toInt();
        for (const auto& [date, str] : future) {
            const int dte = static_cast<int>(today.daysTo(date));
            if (dte >= min_dte)
                return str;
        }
        // Nothing meets the DTE; return the farthest-out expiry as best-effort.
        return future.last().second;
    }

    if (mode == QLatin1String("MONTHLY")) {
        // Group future expiries by (year, month). Pick the earliest such group
        // and return its maximum date (last expiry of that month).
        // The list is already sorted ascending so the first group's last element
        // is the maximum date within that group.
        const int first_year  = future.first().first.year();
        const int first_month = future.first().first.month();
        QString best;
        QDate   best_date;
        for (const auto& [date, str] : future) {
            if (date.year() != first_year || date.month() != first_month)
                break; // moved past the earliest future month
            if (!best_date.isValid() || date > best_date) {
                best_date = date;
                best = str;
            }
        }
        return best;
    }

    // Unknown mode: fall back to nearest future expiry.
    return future.first().second;
}

// ---------------------------------------------------------------------------
// resolve_entry_legs
// ---------------------------------------------------------------------------

QVector<fincept::algo::AlgoOrderLeg>
resolve_entry_legs(const fincept::services::options::OptionChain& chain,
                   const QJsonArray& strategy_legs) {
    using fincept::algo::AlgoOrderLeg;
    using fincept::services::options::OptionChainRow;

    const QVector<AlgoFnoLeg> leg_rules = fno_legs_from_json(strategy_legs);
    const QVector<ResolvedLeg> resolved = FnoLegResolver::resolve(leg_rules, chain);

    QVector<AlgoOrderLeg> out;
    out.reserve(resolved.size());

    for (int i = 0; i < resolved.size(); ++i) {
        const ResolvedLeg& rl = resolved[i];
        if (!rl.valid)
            continue;

        const bool is_call = (rl.kind == QLatin1String("CE"));

        // Find the chain row whose CE or PE token matches the resolved token.
        const OptionChainRow* row = nullptr;
        for (const auto& r : chain.rows) {
            const qint64 tok = is_call ? r.ce_token : r.pe_token;
            if (tok == rl.token) {
                row = &r;
                break;
            }
        }
        if (!row)
            continue; // token not found in chain (shouldn't happen after valid resolve)

        AlgoOrderLeg leg;
        leg.symbol            = rl.symbol;
        leg.instrument_token  = rl.token;
        leg.side              = rl.side;
        leg.quantity          = static_cast<double>(rl.lots) * row->lot_size;
        leg.price             = is_call ? row->ce_quote.ltp : row->pe_quote.ltp;
        out.append(leg);
    }

    return out;
}

// ---------------------------------------------------------------------------
// build_exit_legs
// ---------------------------------------------------------------------------

QVector<fincept::algo::AlgoOrderLeg>
build_exit_legs(const QVector<fincept::algo::AlgoLegPosition>& open_legs) {
    using fincept::algo::AlgoOrderLeg;

    QVector<AlgoOrderLeg> out;
    out.reserve(open_legs.size());

    for (const auto& leg : open_legs) {
        AlgoOrderLeg el;
        el.symbol           = leg.symbol;
        el.instrument_token = leg.instrument_token;
        // Flip the side: long (side_sign > 0) exits as SELL; short exits as BUY.
        el.side             = (leg.side_sign > 0) ? QStringLiteral("SELL")
                                                   : QStringLiteral("BUY");
        el.quantity         = leg.quantity;
        el.price            = leg.current_price;
        out.append(el);
    }

    return out;
}

// ---------------------------------------------------------------------------
// build_basket_request
// ---------------------------------------------------------------------------

fincept::trading::BasketOrderRequest
build_basket_request(const QVector<fincept::algo::AlgoOrderLeg>& legs,
                     const QString& product_type) {
    using namespace fincept::trading;

    const ProductType product =
        (product_type == QLatin1String("CNC") || product_type == QLatin1String("delivery"))
            ? ProductType::Delivery
            : ProductType::Margin; // F&O baskets are margin/NRML

    BasketOrderRequest basket;
    basket.strategy_name = QStringLiteral("algo_fno_basket");
    basket.orders.reserve(legs.size());
    for (const auto& leg : legs) {
        UnifiedOrder o;
        o.symbol       = leg.symbol;
        o.exchange     = QStringLiteral("NFO");
        o.side         = (leg.side == QLatin1String("BUY")) ? OrderSide::Buy : OrderSide::Sell;
        o.order_type   = OrderType::Market;
        o.quantity     = leg.quantity;
        o.price        = 0; // market
        o.product_type = product;
        basket.orders.append(o);
    }
    return basket;
}

// ---------------------------------------------------------------------------
// leg_positions_to_json / leg_positions_from_json
// ---------------------------------------------------------------------------

QJsonArray
leg_positions_to_json(const QVector<fincept::algo::AlgoLegPosition>& legs) {
    QJsonArray arr;
    for (const auto& l : legs) {
        QJsonObject o;
        o["symbol"]      = l.symbol;
        o["token"]       = static_cast<double>(l.instrument_token);
        o["is_call"]     = l.is_call;
        o["strike"]      = l.strike;
        o["side_sign"]   = l.side_sign;
        o["quantity"]    = l.quantity;
        o["entry_price"] = l.entry_price;
        arr.append(o);
    }
    return arr;
}

QVector<fincept::algo::AlgoLegPosition>
leg_positions_from_json(const QJsonArray& arr) {
    using fincept::algo::AlgoLegPosition;
    QVector<AlgoLegPosition> out;
    out.reserve(arr.size());
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        AlgoLegPosition l;
        l.symbol           = o.value("symbol").toString();
        l.instrument_token = static_cast<qint64>(o.value("token").toDouble());
        l.is_call          = o.value("is_call").toBool();
        l.strike           = o.value("strike").toDouble();
        l.side_sign        = o.value("side_sign").toInt();
        l.quantity         = o.value("quantity").toDouble();
        l.entry_price      = o.value("entry_price").toDouble();
        l.current_price    = l.entry_price; // re-marked on the next tick
        l.unrealized_pnl   = 0;
        out.append(l);
    }
    return out;
}

// ---------------------------------------------------------------------------
// leg_marks_from_chain
// ---------------------------------------------------------------------------

QHash<QString, double>
leg_marks_from_chain(const QVector<fincept::algo::AlgoLegPosition>& legs,
                     const fincept::services::options::OptionChain& chain) {
    QHash<QString, double> marks;
    for (const auto& leg : legs) {
        double ltp = 0;
        for (const auto& r : chain.rows) {
            const qint64 tok = leg.is_call ? r.ce_token : r.pe_token;
            if (tok == leg.instrument_token) {
                ltp = leg.is_call ? r.ce_quote.ltp : r.pe_quote.ltp;
                break;
            }
        }
        if (ltp > 0)
            marks.insert(leg.symbol, ltp);
    }
    return marks;
}

} // namespace fincept::algo::fno
