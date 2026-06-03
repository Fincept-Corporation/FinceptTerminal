#include "OptionsStrategyBuilder.h"

#include "core/logging/Logger.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentService.h"

#include <QDate>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

namespace {

// Default exchange for option legs. Index/equity F&O on NSE trade on NFO.
// BSE F&O (SENSEX/BANKEX) would use "BFO" — symbol resolution will refine this
// once InstrumentService is wired in.
constexpr const char* kDefaultFnoExchange = "NFO";

// Compact an expiry into the form used in tradingsymbols: "2025-03-28" -> "28MAR25".
// Falls back to the digits/letters of the input string when it is not an ISO date.
QString compact_expiry(const QString& expiry)
{
    QDate d = QDate::fromString(expiry, "yyyy-MM-dd");
    if (d.isValid()) {
        // Fixed English month abbreviations — QDate::toString("MMM") uses the
        // system locale, which would produce non-English names; tradingsymbols
        // require the uppercase English abbreviation regardless of locale.
        static const char* const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                              "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        const QString month = QString::fromLatin1(kMonths[d.month() - 1]);
        return QString("%1%2%3")
            .arg(d.day(), 2, 10, QChar('0'))
            .arg(month)
            .arg(d.year() % 100, 2, 10, QChar('0'));
    }
    // Not an ISO date — strip separators so the symbol stays readable.
    QString compact = expiry;
    compact.remove('-');
    compact.remove('/');
    compact.remove(' ');
    return compact.toUpper();
}

// Format a strike without a trailing ".0" for whole-number strikes.
QString strike_str(double strike)
{
    if (std::floor(strike) == strike)
        return QString::number(static_cast<long long>(strike));
    return QString::number(strike, 'g', 10);
}

// Build a single leg with its placeholder symbol filled in.
OptionsLeg make_leg(const QString& underlying, const QString& expiry, double strike,
                    const QString& opt_type, OrderSide side, double quantity)
{
    OptionsLeg leg;
    leg.symbol = OptionsStrategyBuilder::build_option_symbol(underlying, expiry, strike, opt_type);
    leg.exchange = QString::fromLatin1(kDefaultFnoExchange);
    leg.side = side;
    leg.quantity = quantity;
    leg.strike = strike;
    leg.option_type = opt_type;
    leg.expiry = expiry;
    leg.underlying = underlying;
    return leg;
}

// Resolve a leg's real broker tradingsymbol from the instrument master.
// Best-effort: returns the master's normalized symbol when it's loaded and the
// exact (underlying, expiry, strike, CE/PE) contract is found; otherwise falls
// back to the human-readable placeholder (leg.symbol) so behaviour never
// regresses (a warning is logged). Runs at order-build time, so it also covers
// strategies reloaded from storage.
QString resolve_leg_symbol(const OptionsLeg& leg, const QString& broker_id)
{
    if (broker_id.isEmpty() || leg.underlying.isEmpty())
        return leg.symbol;

    auto& svc = InstrumentService::instance();
    if (!svc.is_loaded(broker_id))
        return leg.symbol;

    const InstrumentType want =
        (leg.option_type.compare(QLatin1String("PE"), Qt::CaseInsensitive) == 0) ? InstrumentType::PE
                                                                                 : InstrumentType::CE;
    const QString expiry_disp = norm::expiry_display(leg.expiry);

    const auto candidates =
        svc.find_options_for_underlying(broker_id, leg.underlying, expiry_disp, leg.exchange);
    for (const auto& inst : candidates) {
        if (inst.instrument_type == want && qFuzzyCompare(inst.strike + 1.0, leg.strike + 1.0))
            return inst.symbol;
    }

    LOG_WARN("OptionsStrategyBuilder",
             QString("Could not resolve tradingsymbol for %1 %2 %3 %4 (broker=%5) — using placeholder '%6'")
                 .arg(leg.underlying, leg.expiry)
                 .arg(leg.strike)
                 .arg(leg.option_type, broker_id, leg.symbol));
    return leg.symbol;
}

} // namespace

QString OptionsStrategyBuilder::build_option_symbol(const QString& underlying,
                                                    const QString& expiry, double strike,
                                                    const QString& opt_type)
{
    // TODO: Replace with InstrumentService lookup. Real broker tradingsymbols
    // (e.g. Zerodha "NIFTY25MAR24500CE", Dhan numeric security IDs) must be
    // resolved from the instrument master — this placeholder is a human-readable
    // approximation only and is not guaranteed to be tradable as-is.
    return underlying.toUpper() + compact_expiry(expiry) + strike_str(strike) + opt_type.toUpper();
}

OptionsStrategy OptionsStrategyBuilder::straddle(const QString& underlying, const QString& expiry,
                                                 double atm_strike, double lot_size,
                                                 OrderSide direction)
{
    OptionsStrategy s;
    s.name = (direction == OrderSide::Buy) ? "Long Straddle" : "Short Straddle";
    s.legs.append(make_leg(underlying, expiry, atm_strike, "CE", direction, lot_size));
    s.legs.append(make_leg(underlying, expiry, atm_strike, "PE", direction, lot_size));
    // Payoff requires the net premium (CE + PE) which needs live quotes:
    //   long straddle:  max_loss = premium paid; breakevens = strike ± premium.
    //   short straddle: max_profit = premium received; breakevens = strike ± premium.
    // Left at 0 until premium data is supplied.
    return s;
}

OptionsStrategy OptionsStrategyBuilder::strangle(const QString& underlying, const QString& expiry,
                                                 double atm_strike, double width, double lot_size,
                                                 OrderSide direction)
{
    OptionsStrategy s;
    s.name = (direction == OrderSide::Buy) ? "Long Strangle" : "Short Strangle";
    const double call_strike = atm_strike + width; // OTM call
    const double put_strike = atm_strike - width;  // OTM put
    s.legs.append(make_leg(underlying, expiry, call_strike, "CE", direction, lot_size));
    s.legs.append(make_leg(underlying, expiry, put_strike, "PE", direction, lot_size));
    // Payoff requires the net premium (needs live quotes):
    //   long:  max_loss = premium paid; breakevens = call_strike + prem / put_strike - prem.
    //   short: max_profit = premium received; same breakeven structure.
    // Left at 0 until premium data is supplied.
    return s;
}

OptionsStrategy OptionsStrategyBuilder::iron_condor(const QString& underlying,
                                                    const QString& expiry, double atm_strike,
                                                    double near_width, double far_width,
                                                    double lot_size)
{
    OptionsStrategy s;
    s.name = "Iron Condor";
    const double sell_ce = atm_strike + near_width; // short near call
    const double buy_ce = atm_strike + far_width;   // long far call (protection)
    const double sell_pe = atm_strike - near_width; // short near put
    const double buy_pe = atm_strike - far_width;   // long far put (protection)

    // Order: sell near CE, buy far CE, sell near PE, buy far PE (per spec §13).
    s.legs.append(make_leg(underlying, expiry, sell_ce, "CE", OrderSide::Sell, lot_size));
    s.legs.append(make_leg(underlying, expiry, buy_ce, "CE", OrderSide::Buy, lot_size));
    s.legs.append(make_leg(underlying, expiry, sell_pe, "PE", OrderSide::Sell, lot_size));
    s.legs.append(make_leg(underlying, expiry, buy_pe, "PE", OrderSide::Buy, lot_size));

    // Max loss is bounded by the wing width minus the net credit:
    //   max_loss = (far_width - near_width) * lot_size - net_credit.
    // max_profit = net_credit. Both depend on the net premium collected, which
    // needs live quotes — left at 0. Breakevens are short_strike ± net_credit
    // (per contract), also premium-dependent.
    return s;
}

OptionsStrategy OptionsStrategyBuilder::bull_call_spread(const QString& underlying,
                                                         const QString& expiry, double lower_strike,
                                                         double upper_strike, double lot_size)
{
    OptionsStrategy s;
    s.name = "Bull Call Spread";
    s.legs.append(make_leg(underlying, expiry, lower_strike, "CE", OrderSide::Buy, lot_size));
    s.legs.append(make_leg(underlying, expiry, upper_strike, "CE", OrderSide::Sell, lot_size));

    // The maximum payoff (gross of premium) is the strike spread × lot size.
    // The net debit (premium paid) is unknown without quotes, so:
    //   true max_profit = (spread × lot) - net_debit,
    //   true max_loss   = net_debit,
    //   breakeven_lower = lower_strike + net_debit (per contract).
    // We expose the strike-geometry portion as the gross spread payoff and leave
    // the premium-dependent net figures at 0.
    const double spread = (upper_strike - lower_strike) * lot_size;
    s.max_profit = spread; // gross of premium — see note above
    return s;
}

OptionsStrategy OptionsStrategyBuilder::bear_put_spread(const QString& underlying,
                                                        const QString& expiry, double upper_strike,
                                                        double lower_strike, double lot_size)
{
    OptionsStrategy s;
    s.name = "Bear Put Spread";
    s.legs.append(make_leg(underlying, expiry, upper_strike, "PE", OrderSide::Buy, lot_size));
    s.legs.append(make_leg(underlying, expiry, lower_strike, "PE", OrderSide::Sell, lot_size));

    // Gross payoff (before premium) is the strike spread × lot size. The net
    // debit is premium-dependent (needs quotes), so:
    //   true max_profit = (spread × lot) - net_debit,
    //   true max_loss   = net_debit,
    //   breakeven_upper = upper_strike - net_debit (per contract).
    // Expose the gross spread payoff; leave net/premium figures at 0.
    const double spread = (upper_strike - lower_strike) * lot_size;
    s.max_profit = spread; // gross of premium — see note above
    return s;
}

BasketOrderRequest OptionsStrategyBuilder::to_basket_order(const OptionsStrategy& strategy,
                                                           ProductType product,
                                                           const QString& broker_id)
{
    BasketOrderRequest request;
    request.strategy_name = strategy.name;

    // SELL legs first (collect premium → reduce margin), BUY legs second.
    // stable_partition preserves the relative order within each group.
    QVector<OptionsLeg> sorted = strategy.legs;
    std::stable_partition(sorted.begin(), sorted.end(),
                          [](const OptionsLeg& leg) { return leg.side == OrderSide::Sell; });

    request.orders.reserve(sorted.size());
    for (const OptionsLeg& leg : sorted) {
        UnifiedOrder order;
        order.symbol = resolve_leg_symbol(leg, broker_id);
        order.exchange = leg.exchange;
        order.side = leg.side;
        order.quantity = leg.quantity;
        order.order_type = OrderType::Market;
        order.product_type = product;
        request.orders.append(order);
    }

    return request;
}

} // namespace fincept::trading
