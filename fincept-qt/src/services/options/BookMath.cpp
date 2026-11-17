#include "services/options/BookMath.h"

#include "services/options/OptionModels.h"
#include "services/options/VolSurface.h"

#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::services::options::book {

namespace {

[[maybe_unused]] constexpr double kBmMinYears = 0.25 / 365.0; ///< an option expiring today, without a chain: a quarter day

bool bm_finite_pos(double v) {
    return std::isfinite(v) && v > 0;
}

/// The chain expiry on `d`, or null.
const market::FreeExpiry* bm_expiry(const market::FreeChain& c, const QDate& d, int* index) {
    for (int e = 0; e < c.expiries.size(); ++e) {
        if (c.expiries[e].date == d) {
            if (index)
                *index = e;
            return &c.expiries[e];
        }
    }
    return nullptr;
}

/// The chain's volatility for strike K at one expiry: the raw-SVI fit inside its
/// fitted range (with a small margin), else the expiry's ATM IV; 0 when neither.
double bm_chain_iv(const market::FreeExpiry& x, double K) {
    if (bm_finite_pos(x.forward) && K > 0 && x.svi.ok) {
        const double k = std::log(K / x.forward);
        if (k >= x.k_lo - 0.05 && k <= x.k_hi + 0.05) {
            const double v = vol::svi_vol(x.svi.params, k, x.years);
            if (bm_finite_pos(v))
                return v;
        }
    }
    return bm_finite_pos(x.smile.atm_iv) ? x.smile.atm_iv : 0.0;
}

void bm_add(LegResult& to, const LegResult& l) {
    to.value += l.value;
    to.pnl += l.pnl;
    to.delta += l.delta;
    to.dollar_delta += l.dollar_delta;
    to.gamma += l.gamma;
    to.dollar_gamma += l.dollar_gamma;
    to.vega += l.vega;
    to.theta += l.theta;
    to.rho += l.rho;
}

void bm_add(Exposure& to, const LegResult& l) {
    to.value += l.value;
    to.pnl += l.pnl;
    to.delta += l.delta;
    to.dollar_delta += l.dollar_delta;
    to.gamma += l.gamma;
    to.dollar_gamma += l.dollar_gamma;
    to.vega += l.vega;
    to.theta += l.theta;
    to.rho += l.rho;
}

/// Carried at the entry price: no market data for this leg.
LegResult bm_at_entry(const BookLeg& leg, double units) {
    LegResult r;
    r.source = MarkSource::Entry;
    r.mark = leg.entry_price;
    switch (leg.type) {
        case LegType::Future:
            r.value = 0;
            r.delta = units;
            r.dollar_delta = units * leg.entry_price;
            break;
        case LegType::Swap:
            r.mark = leg.entry_price;
            r.value = leg.entry_price;
            break;
        case LegType::Stock:
            r.value = leg.quantity * leg.entry_price;
            r.delta = leg.quantity;
            r.dollar_delta = leg.quantity * leg.entry_price;
            break;
        default:
            r.value = units * leg.entry_price;
            break;
    }
    return r;
}

LegResult bm_option(const BookLeg& leg, const BookPosition& p, const MarketView& m) {
    const double units = leg.quantity * p.multiplier;
    const UnderlyingQuote quote = m.underlyings.value(underlying_key(p.underlying));
    const auto& chain = quote.chain;
    const double S = std::isfinite(quote.spot) ? quote.spot : (chain ? chain->spot : market::kNoValue);
    const bool call = leg.type == LegType::Call;
    if (!leg.expiry.isValid())
        return bm_at_entry(leg, units);
    const qint64 days = m.today.daysTo(leg.expiry);
    if (days < 0) {
        // Expired: worth its intrinsic value at today's spot (the exercise / assignment outcome).
        if (!bm_finite_pos(S))
            return bm_at_entry(leg, units);
        LegResult r;
        r.expired = true;
        r.source = MarkSource::Intrinsic;
        r.spot = S;
        r.mark = std::max(call ? S - leg.strike : leg.strike - S, 0.0);
        r.value = units * r.mark;
        r.pnl = units * (r.mark - leg.entry_price);
        return r;
    }
    if (!bm_finite_pos(S))
        return bm_at_entry(leg, units);
    int ei = -1;
    const market::FreeExpiry* x = chain ? bm_expiry(*chain, leg.expiry, &ei) : nullptr;
    const double T = x ? std::max(x->years, 1e-6) : std::max(static_cast<double>(days), 0.25) / 365.0;
    const double r_ = x ? x->rate : quote.r;
    const double q_ = x ? x->carry_q : quote.q;

    LegResult r;
    r.spot = S;
    r.years = T;
    double mid = 0, feed_iv = 0, iv = 0;
    bool have_mid = false;
    if (x && market::find_quote(*chain, ei, leg.strike, call, &mid, &feed_iv) && bm_finite_pos(mid)) {
        have_mid = true;
        iv = bm_finite_pos(feed_iv) ? feed_iv : bm_chain_iv(*x, leg.strike);
    } else if (x) {
        iv = bm_chain_iv(*x, leg.strike);
    }
    if (iv > 0) {
        r.source = have_mid ? MarkSource::ChainMid : MarkSource::ModelChainIv;
    } else if (bm_finite_pos(leg.entry_iv)) {
        iv = leg.entry_iv;
        r.source = have_mid ? MarkSource::ChainMid : MarkSource::ModelEntryIv;
    } else if (!have_mid) {
        return bm_at_entry(leg, units);
    } else {
        r.source = MarkSource::ChainMid;
    }
    models::Greeks g;
    if (iv > 0)
        g = models::bsm_full(call, S, leg.strike, T, r_, q_, iv);
    r.iv = iv > 0 ? iv : market::kNoValue;
    r.mark = have_mid ? mid : g.price;
    if (!std::isfinite(r.mark))
        return bm_at_entry(leg, units);
    r.value = units * r.mark;
    r.pnl = units * (r.mark - leg.entry_price);
    if (g.valid) {
        r.delta = units * g.delta;
        r.gamma = units * g.gamma;
        r.vega = units * g.vega / 100.0;
        r.theta = units * g.theta / 365.0;
        r.rho = units * g.rho / 100.0;
        r.dollar_delta = r.delta * S;
        r.dollar_gamma = r.gamma * S * S / 100.0;
    }
    return r;
}

LegResult bm_stock(const BookLeg& leg, const BookPosition& p, const MarketView& m) {
    const UnderlyingQuote quote = m.underlyings.value(underlying_key(p.underlying));
    const double S = std::isfinite(quote.spot) ? quote.spot : (quote.chain ? quote.chain->spot : market::kNoValue);
    if (!bm_finite_pos(S))
        return bm_at_entry(leg, leg.quantity);
    LegResult r;
    r.source = MarkSource::Spot;
    r.spot = S;
    r.mark = S;
    r.value = leg.quantity * S;
    r.pnl = leg.quantity * (S - leg.entry_price);
    r.delta = leg.quantity;
    r.dollar_delta = leg.quantity * S;
    return r;
}

LegResult bm_future(const BookLeg& leg, const BookPosition& p, const MarketView& m) {
    const double units = leg.quantity * p.multiplier;
    if (leg.expiry.isValid() && m.today.daysTo(leg.expiry) < 0) {
        LegResult r = bm_at_entry(leg, units);
        r.expired = true;
        r.delta = 0;
        r.dollar_delta = 0;
        return r;
    }
    const QString c = leg.contract.trimmed().toUpper();
    const auto it = m.futures.constFind(c);
    if (c.isEmpty() || it == m.futures.constEnd() || !bm_finite_pos(it.value()))
        return bm_at_entry(leg, units);
    LegResult r;
    r.source = MarkSource::FuturesLast;
    r.mark = it.value();
    r.spot = it.value();
    r.pnl = units * (r.mark - leg.entry_price);
    r.value = r.pnl; // marked to market daily: the position's value is its open P&L
    r.delta = units;
    r.dollar_delta = units * r.mark;
    return r;
}

LegResult bm_swap(const BookLeg& leg, const BookPosition& p, const MarketView& m) {
    LegResult r;
    const double elapsed = p.entry_date.isValid() ? std::max<qint64>(0, p.entry_date.daysTo(m.today)) / 365.0 : 0.0;
    double start = leg.swap_start - elapsed;
    double tenor = leg.swap_tenor;
    if (start < 0) {
        tenor += start; // the part already run off
        start = 0;
    }
    if (!(tenor > 1e-6)) {
        r.expired = true;
        r.source = MarkSource::Curve;
        r.mark = 0;
        r.value = 0;
        r.pnl = -leg.entry_price;
        return r;
    }
    if (m.curve.empty())
        return bm_at_entry(leg, 1.0);
    const rates::SwapResult s =
        rates::swap_on_curve(m.curve, start, tenor, leg.swap_rate, leg.swap_freq, leg.notional, leg.payer);
    if (!std::isfinite(s.pv))
        return bm_at_entry(leg, 1.0);
    r.source = MarkSource::Curve;
    r.mark = s.pv;
    r.value = s.pv;
    r.pnl = s.pv - leg.entry_price;
    r.rho = s.dv01 * 100.0;
    r.years = start + tenor;
    return r;
}

} // namespace

QString underlying_key(const QString& symbol) {
    QString k = symbol.trimmed().toUpper();
    if (k.startsWith(QLatin1Char('^')))
        k = k.mid(1);
    if (k == QLatin1String("GSPC"))
        k = QStringLiteral("SPX");
    return k;
}

QString exposure_key(const BookPosition& p) {
    if (p.kind == QLatin1String("swap"))
        return QStringLiteral("RATES:") + p.currency.toUpper();
    return underlying_key(p.underlying);
}

double years_to(const QDate& today, const QDate& expiry) {
    if (!today.isValid() || !expiry.isValid())
        return 0.0;
    return std::max<qint64>(0, today.daysTo(expiry)) / 365.0;
}

LegResult value_leg(const BookLeg& leg, const BookPosition& p, const MarketView& m) {
    switch (leg.type) {
        case LegType::Call:
        case LegType::Put:
            return bm_option(leg, p, m);
        case LegType::Stock:
            return bm_stock(leg, p, m);
        case LegType::Future:
            return bm_future(leg, p, m);
        case LegType::Swap:
            return bm_swap(leg, p, m);
    }
    return {};
}

PositionResult value_position(const BookPosition& p, const MarketView& m) {
    PositionResult out;
    out.id = p.id;
    out.key = exposure_key(p);
    const UnderlyingQuote quote = m.underlyings.value(underlying_key(p.underlying));
    bool any_live = false;
    for (const BookLeg& leg : p.legs) {
        const LegResult l = value_leg(leg, p, m);
        out.legs.append(l);
        bm_add(out.total, l);
        if (l.source == MarkSource::Entry)
            out.stale = true;
        if (!l.expired)
            any_live = true;
        if (!std::isfinite(out.total.spot) && std::isfinite(l.spot))
            out.total.spot = l.spot;
        if ((leg.is_option() || leg.type == LegType::Future) && leg.expiry.isValid() && !l.expired &&
            (!out.nearest_expiry.isValid() || leg.expiry < out.nearest_expiry))
            out.nearest_expiry = leg.expiry;
        QDateTime when;
        QString src;
        switch (l.source) {
            case MarkSource::ChainMid:
            case MarkSource::ModelChainIv:
            case MarkSource::Spot:
            case MarkSource::Intrinsic:
                when = quote.as_of;
                src = quote.source;
                break;
            case MarkSource::FuturesLast:
                when = m.futures_as_of.value(leg.contract.trimmed().toUpper());
                break;
            case MarkSource::Curve:
                if (m.curve_date.isValid())
                    when = QDateTime(m.curve_date, QTime(0, 0), QTimeZone::UTC);
                break;
            default:
                break;
        }
        if (when.isValid() && (!out.as_of.isValid() || when < out.as_of))
            out.as_of = when;
        if (out.source.isEmpty())
            out.source = src;
    }
    out.all_expired = !p.legs.isEmpty() && !any_live;
    if (out.legs.size() == 1) {
        out.total.mark = out.legs.first().mark;
        out.total.iv = out.legs.first().iv;
        out.total.years = out.legs.first().years;
        out.total.source = out.legs.first().source;
        out.total.expired = out.legs.first().expired;
    } else {
        out.total.mark = market::kNoValue;
        out.total.source = out.stale ? MarkSource::Entry : MarkSource::ChainMid;
    }
    return out;
}

QVector<PositionResult> value_book(const QVector<BookPosition>& positions, const MarketView& m) {
    QVector<PositionResult> out;
    out.reserve(positions.size());
    for (const BookPosition& p : positions)
        out.append(value_position(p, m));
    return out;
}

int expiry_bucket(int days) {
    if (days < 0)
        return 0;
    if (days <= 7)
        return 1;
    if (days <= 30)
        return 2;
    if (days <= 90)
        return 3;
    if (days <= 180)
        return 4;
    if (days <= 365)
        return 5;
    return 6;
}

BookSummary summarise(const QVector<BookPosition>& positions, const QVector<PositionResult>& results,
                      const QDate& today) {
    BookSummary s;
    QHash<QString, int> index;
    QHash<qint64, int> ladder_index; // by Julian day
    const int n = std::min(positions.size(), results.size());
    for (int i = 0; i < n; ++i) {
        const BookPosition& p = positions[i];
        const PositionResult& r = results[i];
        if (!s.currencies.contains(p.currency))
            s.currencies.append(p.currency);
        if (r.stale)
            ++s.stale;
        const QString key = r.key.isEmpty() ? exposure_key(p) : r.key;
        int at = index.value(key, -1);
        if (at < 0) {
            Exposure e;
            e.key = key;
            e.label = key.startsWith(QLatin1String("RATES:")) ? p.currency.toUpper() + QStringLiteral(" rates")
                                                              : p.underlying.trimmed();
            e.currency = p.currency;
            at = s.by_underlying.size();
            s.by_underlying.append(e);
            index.insert(key, at);
        }
        Exposure& e = s.by_underlying[at];
        ++e.positions;
        bm_add(e, r.total);
        ++s.total.positions;
        bm_add(s.total, r.total);
        for (int l = 0; l < p.legs.size() && l < r.legs.size(); ++l) {
            const BookLeg& leg = p.legs[l];
            if (!(leg.is_option() || leg.type == LegType::Future) || !leg.expiry.isValid())
                continue;
            int li = ladder_index.value(leg.expiry.toJulianDay(), -1);
            if (li < 0) {
                LadderRow row;
                row.expiry = leg.expiry;
                row.days = static_cast<int>(today.daysTo(leg.expiry));
                row.bucket = expiry_bucket(row.days);
                li = s.ladder.size();
                s.ladder.append(row);
                ladder_index.insert(leg.expiry.toJulianDay(), li);
            }
            LadderRow& row = s.ladder[li];
            const LegResult& lr = r.legs[l];
            ++row.legs;
            row.value += lr.value;
            row.dollar_delta += lr.dollar_delta;
            row.gamma += lr.gamma;
            row.dollar_gamma += lr.dollar_gamma;
            row.vega += lr.vega;
            row.theta += lr.theta;
        }
    }
    s.total.key = QStringLiteral("TOTAL");
    s.total.delta = 0; // shares of different underlyings do not add up
    s.total.currency = s.currencies.size() == 1 ? s.currencies.first() : QString();
    std::sort(s.by_underlying.begin(), s.by_underlying.end(), [](const Exposure& a, const Exposure& b) {
        if (std::fabs(a.dollar_delta) != std::fabs(b.dollar_delta))
            return std::fabs(a.dollar_delta) > std::fabs(b.dollar_delta);
        return std::fabs(a.value) > std::fabs(b.value);
    });
    std::sort(s.ladder.begin(), s.ladder.end(), [](const LadderRow& a, const LadderRow& b) { return a.expiry < b.expiry; });
    return s;
}

std::vector<strategy::Leg> strategy_legs(const BookPosition& p, const QDate& today, int* skipped) {
    std::vector<strategy::Leg> out;
    int skip = 0;
    for (const BookLeg& b : p.legs) {
        strategy::Leg l;
        if (b.is_option()) {
            l.kind = b.type == LegType::Call ? strategy::LegKind::Call : strategy::LegKind::Put;
            l.quantity = b.quantity;
            l.strike = b.strike;
            l.expiry_years = years_to(today, b.expiry);
            l.entry_price = b.entry_price;
            l.iv = b.entry_iv > 0 ? b.entry_iv : 0.2;
        } else if (b.type == LegType::Stock) {
            l.kind = strategy::LegKind::Underlying;
            l.quantity = b.quantity;
            l.entry_price = b.entry_price;
        } else {
            ++skip;
            continue;
        }
        out.push_back(l);
    }
    if (skipped)
        *skipped = skip;
    return out;
}

} // namespace fincept::services::options::book
