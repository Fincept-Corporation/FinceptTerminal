#include "services/options/DerivMarketData.h"

#include "services/options/OptionModels.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::services::options::market {

namespace {

constexpr double kSecondsPerYear = 365.0 * 86400.0;
constexpr double kMinSmileMid = 0.05;

double dmd_num(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        return std::isfinite(d) ? d : kNoValue;
    }
    return kNoValue;
}

qint64 dmd_int(const QJsonValue& v) {
    const double d = dmd_num(v);
    return std::isfinite(d) && d > 0 ? static_cast<qint64>(std::llround(d)) : 0;
}

/// "2026-11-16T16:00:00" in `tz` ("America/New_York", "UTC") or "…Z" → UTC.
QDateTime dmd_time(const QString& text, const QString& tz) {
    if (text.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid())
        return {};
    if (text.endsWith(QLatin1Char('Z')) || tz.isEmpty() || tz == QLatin1String("UTC"))
        return QDateTime(dt.date(), dt.time(), QTimeZone::UTC);
    const QTimeZone zone(tz.toUtf8());
    if (!zone.isValid())
        return QDateTime(dt.date(), dt.time(), QTimeZone::UTC);
    return QDateTime(dt.date(), dt.time(), zone).toUTC();
}

/// 16:00 New York on `d`, in UTC (the PM settlement of US listed options).
QDateTime dmd_expiry_close(const QDate& d) {
    static const QTimeZone ny("America/New_York");
    if (ny.isValid())
        return QDateTime(d, QTime(16, 0), ny).toUTC();
    return QDateTime(d, QTime(21, 0), QTimeZone::UTC); // EST without tz data
}

double dmd_median(QVector<double> v) {
    if (v.isEmpty())
        return kNoValue;
    std::sort(v.begin(), v.end());
    const int n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

bool dmd_smile_ok(const FreeOptionQuote& q) {
    return q.quoted() && std::isfinite(q.mid) && q.mid >= kMinSmileMid && (q.ask - q.bid) <= q.mid &&
           std::isfinite(q.iv) && q.iv > 0;
}

} // namespace

// ── Chain ────────────────────────────────────────────────────────────────────

FreeChain parse_free_chain(const QByteArray& json) {
    FreeChain c;
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        c.status = QStringLiteral("parse_error");
        c.message = QStringLiteral("The option chain reply could not be read.");
        return c;
    }
    const QJsonObject o = doc.object();
    c.symbol = o.value(QStringLiteral("symbol")).toString();
    if (!o.value(QStringLiteral("ok")).toBool()) {
        c.status = o.value(QStringLiteral("status")).toString(QStringLiteral("feed_unavailable"));
        c.message = o.value(QStringLiteral("message")).toString();
        return c;
    }
    c.source = o.value(QStringLiteral("source")).toString();
    c.source_label = o.value(QStringLiteral("source_label")).toString();
    c.delayed = o.value(QStringLiteral("delayed")).toBool(true);
    c.fetched_at = dmd_time(o.value(QStringLiteral("fetched_at")).toString(), QStringLiteral("UTC"));
    const QJsonObject u = o.value(QStringLiteral("underlying")).toObject();
    c.spot = dmd_num(u.value(QStringLiteral("price")));
    c.bid = dmd_num(u.value(QStringLiteral("bid")));
    c.ask = dmd_num(u.value(QStringLiteral("ask")));
    c.prev_close = dmd_num(u.value(QStringLiteral("prev_close")));
    c.change = dmd_num(u.value(QStringLiteral("change")));
    c.change_pct = dmd_num(u.value(QStringLiteral("change_pct")));
    c.iv30_pct = dmd_num(u.value(QStringLiteral("iv30")));
    c.currency = u.value(QStringLiteral("currency")).toString(QStringLiteral("USD"));
    c.market_state = u.value(QStringLiteral("market_state")).toString();
    c.quote_time = dmd_time(u.value(QStringLiteral("quote_time")).toString(), u.value(QStringLiteral("quote_tz")).toString());

    const QJsonArray exps = o.value(QStringLiteral("expirations")).toArray();
    for (const QJsonValue& e : exps) {
        FreeExpiry fe;
        fe.date = QDate::fromString(e.toString(), Qt::ISODate);
        c.expiries.append(fe);
    }
    const QJsonArray cols = o.value(QStringLiteral("columns")).toArray();
    auto col = [&cols](const char* name) {
        for (int i = 0; i < cols.size(); ++i)
            if (cols[i].toString() == QLatin1String(name))
                return i;
        return -1;
    };
    const int ce = col("e"), ck = col("k"), cc = col("c"), cbid = col("bid"), cask = col("ask"), clast = col("last"),
              cvol = col("vol"), coi = col("oi"), civ = col("iv");
    const QJsonArray rows = o.value(QStringLiteral("rows")).toArray();
    c.quotes.reserve(rows.size());
    if (ce < 0 || ck < 0 || cc < 0) {
        c.status = QStringLiteral("parse_error");
        c.message = QStringLiteral("The option chain reply has no contract columns.");
        return c;
    }
    for (const QJsonValue& rv : rows) {
        const QJsonArray r = rv.toArray();
        auto at = [&r](int i) { return i >= 0 && i < r.size() ? r.at(i) : QJsonValue(); };
        FreeOptionQuote q;
        q.expiry = at(ce).toInt(-1);
        q.strike = dmd_num(at(ck));
        if (q.expiry < 0 || q.expiry >= c.expiries.size() || !(q.strike > 0) || !c.expiries[q.expiry].date.isValid())
            continue;
        q.is_call = at(cc).toInt() == 1;
        q.bid = dmd_num(at(cbid));
        q.ask = dmd_num(at(cask));
        q.last = dmd_num(at(clast));
        q.volume = dmd_int(at(cvol));
        q.open_interest = dmd_int(at(coi));
        q.feed_iv = dmd_num(at(civ));
        c.quotes.append(q);
    }
    std::sort(c.quotes.begin(), c.quotes.end(), [](const FreeOptionQuote& a, const FreeOptionQuote& b) {
        if (a.expiry != b.expiry)
            return a.expiry < b.expiry;
        if (a.strike != b.strike)
            return a.strike < b.strike;
        return !a.is_call && b.is_call; // put first
    });
    int i = 0;
    for (int e = 0; e < c.expiries.size(); ++e) {
        c.expiries[e].begin = i;
        while (i < c.quotes.size() && c.quotes[i].expiry == e)
            ++i;
        c.expiries[e].end = i;
    }
    // Expiries without a single contract are dropped (indexes re-mapped).
    QVector<FreeExpiry> kept;
    QVector<int> remap(c.expiries.size(), -1);
    for (int e = 0; e < c.expiries.size(); ++e) {
        if (c.expiries[e].end > c.expiries[e].begin) {
            remap[e] = kept.size();
            kept.append(c.expiries[e]);
        }
    }
    if (kept.size() != c.expiries.size()) {
        for (FreeOptionQuote& q : c.quotes)
            q.expiry = remap[q.expiry];
        c.expiries = kept;
    }
    for (const QJsonValue& n : o.value(QStringLiteral("notes")).toArray())
        c.notes.append(n.toString());
    c.fallback_reason = o.value(QStringLiteral("fallback_reason")).toString();
    const QJsonObject partial = o.value(QStringLiteral("partial")).toObject();
    c.partial_loaded = partial.value(QStringLiteral("loaded")).toInt();
    c.partial_total = partial.value(QStringLiteral("total")).toInt();
    if (c.quotes.isEmpty()) {
        c.status = QStringLiteral("no_options");
        c.message = QStringLiteral("No current option contracts were found for %1.").arg(c.symbol);
    }
    return c;
}

void analyse_chain(FreeChain& chain, const rates::ZeroCurve& rates, double fallback_rate) {
    if (!chain.ok())
        return;
    const QDateTime base = chain.quote_time.isValid()   ? chain.quote_time
                           : chain.fetched_at.isValid() ? chain.fetched_at
                                                        : QDateTime::currentDateTimeUtc();
    static const QTimeZone ny("America/New_York");
    const QDate base_date = ny.isValid() ? base.toTimeZone(ny).date() : base.date();
    const double S = chain.spot;
    const vol::SviFit* prev_fit = nullptr;
    const FreeExpiry* prev_exp = nullptr;

    for (int e = 0; e < chain.expiries.size(); ++e) {
        FreeExpiry& x = chain.expiries[e];
        const qint64 secs = base.secsTo(dmd_expiry_close(x.date));
        x.years = std::max(static_cast<double>(secs) / kSecondsPerYear, 1.0 / kSecondsPerYear * 60.0);
        x.days = static_cast<int>(base_date.daysTo(x.date));
        x.rate = rates.empty() ? fallback_rate : rates.rate_at(x.years);
        const double T = x.years, r = x.rate;

        // Mids, and the implied forward from put-call parity near the money.
        QVector<QPair<double, double>> parity; // (|K − S|, F_K)
        for (int i = x.begin; i < x.end; ++i) {
            FreeOptionQuote& q = chain.quotes[i];
            q.mid = q.quoted() ? 0.5 * (q.bid + q.ask) : kNoValue;
        }
        for (int i = x.begin; i + 1 < x.end; ++i) {
            const FreeOptionQuote& p = chain.quotes[i];
            const FreeOptionQuote& cq = chain.quotes[i + 1];
            if (!p.is_call && cq.is_call && p.strike == cq.strike && std::isfinite(p.mid) && std::isfinite(cq.mid))
                parity.append({std::fabs(p.strike - (std::isfinite(S) ? S : p.strike)),
                               p.strike + std::exp(r * T) * (cq.mid - p.mid)});
        }
        std::sort(parity.begin(), parity.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        QVector<double> fks;
        for (int i = 0; i < parity.size() && i < 5; ++i)
            fks.append(parity[i].second);
        const double f_par = dmd_median(fks);
        const double f_rate = std::isfinite(S) && S > 0 ? S * std::exp(r * T) : kNoValue;
        if (std::isfinite(f_par) && f_par > 0 && (!std::isfinite(S) || std::fabs(f_par / S - 1.0) < 0.5)) {
            x.forward = f_par;
            x.forward_from_parity = true;
        } else {
            x.forward = f_rate;
        }
        const double spot_ref = (std::isfinite(S) && S > 0) ? S : x.forward;
        x.carry_q = (std::isfinite(x.forward) && x.forward > 0 && spot_ref > 0 && T > 0)
                        ? r - std::log(x.forward / spot_ref) / T
                        : 0.0;
        const double F = x.forward;
        if (!std::isfinite(F) || !(F > 0))
            continue;

        // Local IV and Greeks for every contract with a price.
        double best_gap = -1;
        for (int i = x.begin; i < x.end; ++i) {
            FreeOptionQuote& q = chain.quotes[i];
            double sigma = kNoValue;
            if (std::isfinite(q.mid) && q.mid > 0) {
                const models::VolSolve s = models::bsm_implied_vol(q.mid, q.is_call, spot_ref, q.strike, T, r, x.carry_q);
                if (s.status == models::SolveStatus::Ok && s.sigma > 1e-3 && s.sigma < 10.0)
                    sigma = s.sigma;
            }
            q.iv = sigma;
            const double greek_vol = std::isfinite(sigma) ? sigma : q.feed_iv;
            if (std::isfinite(greek_vol) && greek_vol > 0) {
                const models::Greeks g = models::bsm_full(q.is_call, spot_ref, q.strike, T, r, x.carry_q, greek_vol);
                if (g.valid) {
                    q.delta = g.delta;
                    q.gamma = g.gamma;
                    q.theta = g.theta / 365.0;
                    q.vega = g.vega / 100.0;
                }
            }
            const double gap = std::fabs(q.strike - F);
            if (best_gap < 0 || gap < best_gap) {
                best_gap = gap;
                x.atm_strike = q.strike;
            }
            (q.is_call ? x.call_oi : x.put_oi) += static_cast<double>(q.open_interest);
            (q.is_call ? x.call_volume : x.put_volume) += static_cast<double>(q.volume);
        }

        // ATM straddle.
        double c_mid = kNoValue, p_mid = kNoValue;
        for (int i = x.begin; i < x.end; ++i) {
            const FreeOptionQuote& q = chain.quotes[i];
            if (q.strike == x.atm_strike && std::isfinite(q.mid))
                (q.is_call ? c_mid : p_mid) = q.mid;
        }
        if (std::isfinite(c_mid) && std::isfinite(p_mid))
            x.straddle = c_mid + p_mid;

        // Smile, statistics, SVI.
        const QVector<vol::SmileQuote> smile = smile_quotes(chain, e);
        x.smile_points = smile.size();
        std::vector<vol::SmileQuote> sv(smile.begin(), smile.end());
        x.smile = vol::smile_stats(sv, spot_ref, T);
        if (!smile.isEmpty()) {
            x.k_lo = smile.first().k;
            x.k_hi = smile.last().k;
        }
        std::vector<double> ks, ivs, wts;
        for (int i = x.begin; i < x.end; ++i) {
            const FreeOptionQuote& q = chain.quotes[i];
            const bool otm = q.is_call ? q.strike >= F : q.strike < F;
            if (!otm || !dmd_smile_ok(q))
                continue;
            ks.push_back(std::log(q.strike / F));
            ivs.push_back(q.iv);
            wts.push_back(1.0 / std::max((q.ask - q.bid) / q.mid, 0.02));
        }
        x.svi = vol::fit_svi(ks, ivs, wts, T);
        if (x.svi.ok) {
            x.butterfly = vol::butterfly_check(x.svi.params, x.k_lo, x.k_hi, 201);
            if (prev_fit && prev_exp) {
                const double lo = std::max(x.k_lo, prev_exp->k_lo), hi = std::min(x.k_hi, prev_exp->k_hi);
                if (hi > lo) {
                    x.calendar = vol::calendar_check(prev_fit->params, x.svi.params, lo, hi, 101);
                    x.calendar_checked = true;
                }
            }
            prev_fit = &x.svi;
            prev_exp = &x;
        }
        if (e > 0) {
            const FreeExpiry& p = chain.expiries[e - 1];
            if (p.smile.atm_iv > 0 && x.smile.atm_iv > 0)
                x.forward_vol = vol::forward_vol(p.smile.atm_iv, p.years, x.smile.atm_iv, x.years);
        }

        // Max pain: the strike where option writers pay out the least at expiry.
        const QVector<double> ks_list = strikes(chain, e);
        double best_pain = -1;
        for (double K : ks_list) {
            double pain = 0;
            for (int i = x.begin; i < x.end; ++i) {
                const FreeOptionQuote& q = chain.quotes[i];
                const double intrinsic = q.is_call ? std::max(K - q.strike, 0.0) : std::max(q.strike - K, 0.0);
                pain += intrinsic * static_cast<double>(q.open_interest);
            }
            if (best_pain < 0 || pain < best_pain) {
                best_pain = pain;
                x.max_pain = K;
            }
        }
    }

    // Surface grid: SVI where it fits well and inside the quoted range, else the market smile.
    chain.surface_moneyness = surface_rows();
    chain.surface_iv.clear();
    for (double m : chain.surface_moneyness) {
        QVector<double> row;
        const double k = std::log(m);
        for (int e = 0; e < chain.expiries.size(); ++e) {
            const FreeExpiry& x = chain.expiries[e];
            double v = kNoValue;
            if (x.smile_points >= 2 && k >= x.k_lo - 0.02 && k <= x.k_hi + 0.02) {
                if (x.svi.ok && x.svi.rmse_vol < 0.03) {
                    v = vol::svi_vol(x.svi.params, k, x.years);
                } else {
                    const QVector<vol::SmileQuote> sm = smile_quotes(chain, e);
                    std::vector<double> kx, iy;
                    for (const vol::SmileQuote& s : sm) {
                        if (!kx.empty() && s.k == kx.back())
                            continue;
                        kx.push_back(s.k);
                        iy.push_back(s.iv);
                    }
                    v = vol::interp_linear(kx, iy, k, false);
                }
            }
            row.append(std::isfinite(v) && v > 0 ? v : kNoValue);
        }
        chain.surface_iv.append(row);
    }
    chain.analysed = true;
}

QVector<double> surface_rows() {
    QVector<double> v;
    for (int i = 0; i <= 16; ++i)
        v.append(0.80 + 0.025 * i);
    return v;
}

QVector<vol::SmileQuote> smile_quotes(const FreeChain& chain, int e) {
    QVector<vol::SmileQuote> out;
    if (e < 0 || e >= chain.expiries.size())
        return out;
    const FreeExpiry& x = chain.expiries[e];
    const double F = x.forward;
    if (!std::isfinite(F) || !(F > 0) || !(x.years > 0))
        return out;
    for (int i = x.begin; i < x.end; ++i) {
        const FreeOptionQuote& q = chain.quotes[i];
        const bool otm = q.is_call ? q.strike >= F : q.strike < F;
        if (!otm || !dmd_smile_ok(q))
            continue;
        vol::SmileQuote s;
        s.strike = q.strike;
        s.k = std::log(q.strike / F);
        s.iv = q.iv;
        s.is_call = q.is_call;
        const double st = q.iv * std::sqrt(x.years);
        const double d1 = (-s.k + 0.5 * st * st) / st;
        const double nd1 = 0.5 * std::erfc(-d1 / std::sqrt(2.0));
        s.fwd_delta = q.is_call ? nd1 : nd1 - 1.0;
        out.append(s);
    }
    std::sort(out.begin(), out.end(), [](const vol::SmileQuote& a, const vol::SmileQuote& b) { return a.k < b.k; });
    return out;
}

bool find_quote(const FreeChain& chain, int e, double strike, bool is_call, double* mid, double* iv) {
    if (e < 0 || e >= chain.expiries.size())
        return false;
    const FreeExpiry& x = chain.expiries[e];
    for (int i = x.begin; i < x.end; ++i) {
        const FreeOptionQuote& q = chain.quotes[i];
        if (q.is_call == is_call && std::fabs(q.strike - strike) < 1e-9) {
            if (mid)
                *mid = std::isfinite(q.mid) ? q.mid : 0.0;
            if (iv)
                *iv = std::isfinite(q.iv) ? q.iv : (std::isfinite(q.feed_iv) ? q.feed_iv : 0.0);
            return std::isfinite(q.mid) || std::isfinite(q.iv) || std::isfinite(q.feed_iv);
        }
    }
    return false;
}

QVector<double> strikes(const FreeChain& chain, int e) {
    QVector<double> out;
    if (e < 0 || e >= chain.expiries.size())
        return out;
    const FreeExpiry& x = chain.expiries[e];
    for (int i = x.begin; i < x.end; ++i) {
        const double k = chain.quotes[i].strike;
        if (out.isEmpty() || k != out.last())
            out.append(k);
    }
    return out;
}

// ── Curves ───────────────────────────────────────────────────────────────────

TreasuryCurve parse_treasury_curve(const QByteArray& json) {
    TreasuryCurve c;
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        c.status = QStringLiteral("parse_error");
        c.message = QStringLiteral("The Treasury curve reply could not be read.");
        return c;
    }
    const QJsonObject o = doc.object();
    if (!o.value(QStringLiteral("ok")).toBool()) {
        c.status = o.value(QStringLiteral("status")).toString(QStringLiteral("feed_unavailable"));
        c.message = o.value(QStringLiteral("message")).toString();
        return c;
    }
    c.date = QDate::fromString(o.value(QStringLiteral("date")).toString(), Qt::ISODate);
    c.prev_date = QDate::fromString(o.value(QStringLiteral("prev_date")).toString(), Qt::ISODate);
    c.source_label = o.value(QStringLiteral("source_label")).toString();
    c.fetched_at = dmd_time(o.value(QStringLiteral("fetched_at")).toString(), QStringLiteral("UTC"));
    const QJsonArray t = o.value(QStringLiteral("tenors")).toArray();
    const QJsonArray y = o.value(QStringLiteral("yields")).toArray();
    const QJsonArray l = o.value(QStringLiteral("labels")).toArray();
    const QJsonArray p = o.value(QStringLiteral("prev_yields")).toArray();
    for (int i = 0; i < t.size() && i < y.size(); ++i) {
        const double ti = dmd_num(t[i]), yi = dmd_num(y[i]);
        if (!(ti > 0) || !std::isfinite(yi))
            continue;
        c.tenors.append(ti);
        c.yields.append(yi);
        c.labels.append(i < l.size() ? l[i].toString() : QString());
        c.prev_yields.append(i < p.size() ? dmd_num(p[i]) : kNoValue);
    }
    if (c.tenors.size() < 2) {
        c.status = QStringLiteral("parse_error");
        c.message = QStringLiteral("The Treasury curve reply has too few points.");
    }
    return c;
}

rates::ZeroCurve zero_curve(const TreasuryCurve& c) {
    if (!c.ok())
        return {};
    return rates::bootstrap_par_curve(std::vector<double>(c.tenors.begin(), c.tenors.end()),
                                      std::vector<double>(c.yields.begin(), c.yields.end()));
}

FuturesCurve parse_futures_curve(const QByteArray& json) {
    FuturesCurve c;
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        c.status = QStringLiteral("parse_error");
        c.message = QStringLiteral("The futures curve reply could not be read.");
        return c;
    }
    const QJsonObject o = doc.object();
    c.root = o.value(QStringLiteral("root")).toString(o.value(QStringLiteral("symbol")).toString());
    if (!o.value(QStringLiteral("ok")).toBool()) {
        c.status = o.value(QStringLiteral("status")).toString(QStringLiteral("feed_unavailable"));
        c.message = o.value(QStringLiteral("message")).toString();
        return c;
    }
    c.name = o.value(QStringLiteral("name")).toString();
    c.unit = o.value(QStringLiteral("unit")).toString();
    c.source_label = o.value(QStringLiteral("source_label")).toString();
    c.fetched_at = dmd_time(o.value(QStringLiteral("fetched_at")).toString(), QStringLiteral("UTC"));
    c.continuous = dmd_num(o.value(QStringLiteral("continuous")).toObject().value(QStringLiteral("last")));
    for (const QJsonValue& v : o.value(QStringLiteral("contracts")).toArray()) {
        const QJsonObject r = v.toObject();
        FuturesContract fc;
        fc.symbol = r.value(QStringLiteral("symbol")).toString();
        fc.month = QDate::fromString(r.value(QStringLiteral("month")).toString() + QStringLiteral("-01"), Qt::ISODate);
        fc.last = dmd_num(r.value(QStringLiteral("last")));
        fc.prev = dmd_num(r.value(QStringLiteral("prev")));
        fc.volume = dmd_int(r.value(QStringLiteral("volume")));
        fc.date = QDate::fromString(r.value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (fc.month.isValid() && std::isfinite(fc.last))
            c.contracts.append(fc);
    }
    for (const QJsonValue& v : o.value(QStringLiteral("missing")).toArray())
        c.missing.append(v.toString());
    std::sort(c.contracts.begin(), c.contracts.end(),
              [](const FuturesContract& a, const FuturesContract& b) { return a.month < b.month; });
    if (c.contracts.isEmpty()) {
        c.status = QStringLiteral("feed_unavailable");
        c.message = QStringLiteral("No contract months could be priced for %1.").arg(c.root);
    }
    return c;
}

} // namespace fincept::services::options::market
