// FxRateMath.cpp — see FxRateMath.h. Qt Core only.

#include "services/markets/FxRateMath.h"

#include "ui/formatting/CurrencyTable.h"

#include <QCoreApplication>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::services::fxmath {

namespace {
namespace uifmt = fincept::ui::formatting;
} // namespace

namespace fxm_detail {

const QString& fxm_usd() {
    static const QString k = QStringLiteral("USD");
    return k;
}

// Market convention: these are quoted as XXX/USD, the rest as USD/XXX.
bool fxm_usd_is_quote(const QString& ccy) {
    return ccy == QLatin1String("EUR") || ccy == QLatin1String("GBP") || ccy == QLatin1String("AUD") ||
           ccy == QLatin1String("NZD");
}

/// One currency's value in USD, from its leg (or the stablecoin peg).
struct FxmLeg {
    double usd_per_unit = 1.0;
    qint64 received_ms = 0; ///< 0 = no leg involved (USD itself, or the peg)
    qint64 as_of_ms = 0;    ///< market time when known, else receipt time; 0 = none
    bool approximate = false;
    QString reason;
};

bool fxm_sample_usable(const std::optional<FxLegSample>& s, qint64 now_ms, qint64 hard_max_s) {
    return s && std::isfinite(s->price) && s->price > 0 && s->received_ms > 0 &&
           now_ms - s->received_ms <= hard_max_s * 1000;
}

qint64 fxm_sample_as_of(const FxLegSample& s) {
    return s.market_time_s > 0 ? s.market_time_s * 1000 : s.received_ms;
}

std::optional<FxmLeg> fxm_usd_leg(const QString& code, const FxLegLookup& lookup, qint64 now_ms, qint64 hard_max_s,
                                  QStringList* missing) {
    if (code == fxm_usd())
        return FxmLeg{};
    const auto s = lookup ? lookup(fx_leg_symbol(code)) : std::nullopt;
    if (fxm_sample_usable(s, now_ms, hard_max_s))
        return FxmLeg{fx_usd_per_unit(code, s->price), s->received_ms, fxm_sample_as_of(*s), false, {}};
    missing->append(code);
    if (uifmt::is_stablecoin(code)) {
        FxmLeg peg;
        peg.approximate = true;
        peg.reason = QCoreApplication::translate("fincept::services::FxRateService",
                                                 "no %1 rate yet: 1 %1 taken as 1 USD")
                         .arg(code);
        return peg;
    }
    return std::nullopt;
}

qint64 fxm_min_nonzero(qint64 a, qint64 b) {
    if (a <= 0)
        return b;
    if (b <= 0)
        return a;
    return std::min(a, b);
}

FxQuote fxm_make(double rate, qint64 received_ms, qint64 as_of_ms, bool triangulated, qint64 now_ms,
                 qint64 stale_after_s) {
    FxQuote q;
    q.rate = rate;
    q.received_ms = received_ms;
    q.triangulated = triangulated;
    q.stale = received_ms > 0 && now_ms - received_ms > stale_after_s * 1000;
    if (as_of_ms > 0)
        q.as_of = QDateTime::fromMSecsSinceEpoch(as_of_ms, QTimeZone::utc());
    return q;
}

} // namespace fxm_detail

bool fx_is_crypto_leg(const QString& major_code) {
    return uifmt::is_crypto_currency(major_code);
}

bool fx_valid_code(const QString& major_code) {
    if (fx_is_crypto_leg(major_code))
        return true;
    if (major_code.size() != 3)
        return false;
    // Yahoo has no spot pair for the precious metals (USDXAU=X / XAUUSD=X do
    // not exist; GC=F is a future): not convertible, and never subscribed.
    uifmt::CurrencySpec spec;
    if (uifmt::find_currency(major_code, &spec) && spec.metal)
        return false;
    for (const QChar ch : major_code)
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z'))
            return false;
    return true;
}

QString fx_leg_symbol(const QString& ccy) {
    const QString c = uifmt::major_currency(ccy);
    if (c.isEmpty() || c == fxm_detail::fxm_usd() || !fx_valid_code(c))
        return {};
    if (fx_is_crypto_leg(c))
        return c + QStringLiteral("-USD");
    return fxm_detail::fxm_usd_is_quote(c) ? c + QStringLiteral("USD=X") : QStringLiteral("USD") + c + QStringLiteral("=X");
}

double fx_usd_per_unit(const QString& major_code, double leg_price) {
    // Crypto legs ("BTC-USD") and XXXUSD pairs are priced in USD per unit; USDXXX pairs in units per USD.
    if (fx_is_crypto_leg(major_code) || fxm_detail::fxm_usd_is_quote(major_code))
        return leg_price;
    return 1.0 / leg_price;
}

FxCombine fx_combine(const QString& from_ccy, const QString& to_ccy, const FxLegLookup& lookup, qint64 now_ms,
                     qint64 stale_after_s, qint64 hard_max_s) {
    using namespace fxm_detail;
    FxCombine out;
    const QString from = uifmt::major_currency(from_ccy);
    const QString to = uifmt::major_currency(to_ccy);
    if (!fx_valid_code(from) || !fx_valid_code(to))
        return out;
    // 1 GBp = 0.01 GBP; 1 USD = 100 USX.
    const double scale = double(uifmt::minor_unit_divisor(to_ccy)) / double(uifmt::minor_unit_divisor(from_ccy));
    if (from == to) {
        FxQuote q;
        q.rate = scale;
        q.as_of = QDateTime::fromMSecsSinceEpoch(now_ms, QTimeZone::utc());
        out.quote = q;
        return out;
    }

    // A direct cross already in the hub (e.g. EURGBP=X on a forex widget).
    std::optional<FxQuote> direct;
    if (from != fxm_usd() && to != fxm_usd() && !fx_is_crypto_leg(from) && !fx_is_crypto_leg(to) && lookup) {
        const auto d = lookup(from + to + QStringLiteral("=X"));
        if (fxm_sample_usable(d, now_ms, hard_max_s)) {
            direct = fxm_make(d->price * scale, d->received_ms, fxm_sample_as_of(*d), false, now_ms, stale_after_s);
        } else {
            const auto inv = lookup(to + from + QStringLiteral("=X"));
            if (fxm_sample_usable(inv, now_ms, hard_max_s))
                direct = fxm_make(scale / inv->price, inv->received_ms, fxm_sample_as_of(*inv), false, now_ms,
                                  stale_after_s);
        }
        if (direct && !direct->stale) {
            out.quote = direct;
            return out;
        }
    }

    QStringList missing;
    const auto lf = fxm_usd_leg(from, lookup, now_ms, hard_max_s, &missing);
    const auto lt = fxm_usd_leg(to, lookup, now_ms, hard_max_s, &missing);
    std::optional<FxQuote> tri;
    if (lf && lt && lt->usd_per_unit > 0) {
        const double r = lf->usd_per_unit / lt->usd_per_unit * scale;
        if (std::isfinite(r) && r > 0) {
            tri = fxm_make(r, fxm_min_nonzero(lf->received_ms, lt->received_ms),
                           fxm_min_nonzero(lf->as_of_ms, lt->as_of_ms), from != fxm_usd() && to != fxm_usd(), now_ms,
                           stale_after_s);
            tri->approximate = lf->approximate || lt->approximate;
            tri->approx_reason = lf->approximate ? lf->reason : lt->reason;
        }
    }
    out.missing = missing;
    // A stale direct cross still beats legs that are stale or only approximate.
    if (direct && (!tri || tri->stale || tri->approximate))
        out.quote = direct;
    else
        out.quote = tri;
    return out;
}

} // namespace fincept::services::fxmath
