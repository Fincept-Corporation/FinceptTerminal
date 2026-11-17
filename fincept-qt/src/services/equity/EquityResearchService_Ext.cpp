// src/services/equity/EquityResearchService_Ext.cpp
//
// One-shot, user-initiated EquityResearchService entry points (no caching, no
// broadcast). Kept out of EquityResearchService.cpp so the streaming/cached loaders
// there stay readable.
#include "services/equity/EquityResearchService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::equity {

namespace {

// `resolve_symbol` reply → SymbolResolution. Tolerates the older script, which sends no
// "candidates".
SymbolResolution eqrs_parse_resolution(const QString& query, const QJsonObject& o) {
    SymbolResolution r;
    r.query = query;
    r.resolved_symbol = o.value(QStringLiteral("resolved_symbol")).toString().trimmed();
    r.found = o.value(QStringLiteral("found")).toBool() && !r.resolved_symbol.isEmpty();
    if (r.resolved_symbol.isEmpty())
        r.resolved_symbol = query;
    r.exchange = o.value(QStringLiteral("exchange")).toString().trimmed();
    r.name = o.value(QStringLiteral("name")).toString().trimmed();
    r.message = o.value(QStringLiteral("message")).toString().trimmed();
    constexpr qsizetype kMaxCandidates = 8;
    for (const auto& v : o.value(QStringLiteral("candidates")).toArray()) {
        const QJsonObject c = v.toObject();
        SymbolCandidate sc;
        sc.symbol = c.value(QStringLiteral("symbol")).toString().trimmed();
        sc.name = c.value(QStringLiteral("name")).toString().trimmed();
        sc.exchange = c.value(QStringLiteral("exchange")).toString().trimmed();
        sc.type = c.value(QStringLiteral("type")).toString().trimmed();
        if (!sc.symbol.isEmpty() && r.candidates.size() < kMaxCandidates)
            r.candidates.append(sc);
    }
    return r;
}

double eqrs_num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

QDate eqrs_day(const QJsonValue& v) {
    return QDate::fromString(v.toString().left(10), Qt::ISODate);
}

QVector<PeriodEps> eqrs_period_eps(const QJsonArray& arr) {
    QVector<PeriodEps> out;
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        PeriodEps p;
        p.period = eqrs_day(o.value(QStringLiteral("period")));
        if (!p.period.isValid())
            continue;
        p.eps_diluted = eqrs_num(o.value(QStringLiteral("eps_diluted")));
        p.eps_basic = eqrs_num(o.value(QStringLiteral("eps_basic")));
        p.net_income = eqrs_num(o.value(QStringLiteral("net_income")));
        p.revenue = eqrs_num(o.value(QStringLiteral("revenue")));
        out.append(p);
    }
    std::sort(out.begin(), out.end(), [](const PeriodEps& a, const PeriodEps& b) { return a.period < b.period; });
    return out;
}

} // namespace

EquityEvents EquityResearchService::parse_events(const QJsonObject& o) const {
    EquityEvents e;
    e.symbol = o.value(QStringLiteral("symbol")).toString();
    e.currency = o.value(QStringLiteral("currency")).toString();
    e.financial_currency = o.value(QStringLiteral("financial_currency")).toString();
    e.quote_type = o.value(QStringLiteral("quote_type")).toString();
    e.exchange = o.value(QStringLiteral("exchange")).toString();
    e.full_exchange_name = o.value(QStringLiteral("full_exchange_name")).toString();
    e.exchange_tz = o.value(QStringLiteral("exchange_tz")).toString();
    e.industry_key = o.value(QStringLiteral("industry_key")).toString();
    e.sector_key = o.value(QStringLiteral("sector_key")).toString();
    e.trailing_eps = eqrs_num(o.value(QStringLiteral("trailing_eps")));
    e.forward_eps = eqrs_num(o.value(QStringLiteral("forward_eps")));
    e.book_value = eqrs_num(o.value(QStringLiteral("book_value")));
    e.dividend_rate = eqrs_num(o.value(QStringLiteral("dividend_rate")));
    e.trailing_dividend_rate = eqrs_num(o.value(QStringLiteral("trailing_dividend_rate")));
    e.dividend_yield_pct = eqrs_num(o.value(QStringLiteral("dividend_yield_pct")));
    e.five_year_avg_yield_pct = eqrs_num(o.value(QStringLiteral("five_year_avg_yield_pct")));
    e.payout_ratio = eqrs_num(o.value(QStringLiteral("payout_ratio")));
    e.shares_outstanding = eqrs_num(o.value(QStringLiteral("shares_outstanding")));
    e.ex_dividend_date = eqrs_day(o.value(QStringLiteral("ex_dividend_date")));
    e.dividend_pay_date = eqrs_day(o.value(QStringLiteral("dividend_pay_date")));
    e.last_fiscal_year_end = eqrs_day(o.value(QStringLiteral("last_fiscal_year_end")));
    e.most_recent_quarter = eqrs_day(o.value(QStringLiteral("most_recent_quarter")));
    e.next_earnings = eqrs_day(o.value(QStringLiteral("next_earnings")));
    e.next_earnings_end = eqrs_day(o.value(QStringLiteral("next_earnings_end")));
    const QJsonObject est = o.value(QStringLiteral("earnings_estimate")).toObject();
    e.est_eps_avg = eqrs_num(est.value(QStringLiteral("avg")));
    e.est_eps_low = eqrs_num(est.value(QStringLiteral("low")));
    e.est_eps_high = eqrs_num(est.value(QStringLiteral("high")));
    e.est_rev_avg = eqrs_num(o.value(QStringLiteral("revenue_estimate")).toObject().value(QStringLiteral("avg")));
    for (const auto& v : o.value(QStringLiteral("dividends")).toArray()) {
        const QJsonObject d = v.toObject();
        DividendEvent ev{eqrs_day(d.value(QStringLiteral("date"))), eqrs_num(d.value(QStringLiteral("amount")))};
        if (ev.date.isValid() && std::isfinite(ev.amount) && ev.amount > 0.0)
            e.dividends.append(ev);
    }
    for (const auto& v : o.value(QStringLiteral("splits")).toArray()) {
        const QJsonObject d = v.toObject();
        SplitEvent ev{eqrs_day(d.value(QStringLiteral("date"))), eqrs_num(d.value(QStringLiteral("ratio")))};
        if (ev.date.isValid() && std::isfinite(ev.ratio) && ev.ratio > 0.0)
            e.splits.append(ev);
    }
    for (const auto& v : o.value(QStringLiteral("earnings")).toArray()) {
        const QJsonObject d = v.toObject();
        EarningsEvent ev;
        ev.quarter = eqrs_day(d.value(QStringLiteral("quarter")));
        ev.report_date = eqrs_day(d.value(QStringLiteral("report_date")));
        ev.date_estimated = d.value(QStringLiteral("date_source")).toString() == QLatin1String("estimated");
        ev.eps_estimate = eqrs_num(d.value(QStringLiteral("eps_estimate")));
        ev.eps_actual = eqrs_num(d.value(QStringLiteral("eps_actual")));
        ev.surprise_pct = eqrs_num(d.value(QStringLiteral("surprise_pct")));
        if (ev.quarter.isValid() || ev.report_date.isValid())
            e.earnings.append(ev);
    }
    std::sort(e.dividends.begin(), e.dividends.end(),
              [](const DividendEvent& a, const DividendEvent& b) { return a.date < b.date; });
    std::sort(e.splits.begin(), e.splits.end(), [](const SplitEvent& a, const SplitEvent& b) { return a.date < b.date; });
    std::sort(e.earnings.begin(), e.earnings.end(), [](const EarningsEvent& a, const EarningsEvent& b) {
        const QDate da = a.quarter.isValid() ? a.quarter : a.report_date.addDays(-30);
        const QDate db = b.quarter.isValid() ? b.quarter : b.report_date.addDays(-30);
        return da < db;
    });
    e.eps_quarterly = eqrs_period_eps(o.value(QStringLiteral("eps_quarterly")).toArray());
    e.eps_annual = eqrs_period_eps(o.value(QStringLiteral("eps_annual")).toArray());
    return e;
}

void EquityResearchService::fetch_events(const QString& symbol, bool force) {
    const QString sym = symbol.trimmed();
    if (sym.isEmpty())
        return;
    const QString cache_key = QStringLiteral("equity:events:") + sym;
    auto& cache = fincept::CacheManager::instance();
    if (force)
        cache.remove(cache_key);
    if (const auto hit = cache.try_get(cache_key)) {
        const QJsonObject obj = QJsonDocument::fromJson(hit->toUtf8()).object();
        if (!obj.isEmpty() && !obj.contains(QStringLiteral("error"))) {
            emit events_loaded(parse_events(obj));
            return;
        }
    }
    const auto it = events_inflight_.constFind(sym);
    if (it != events_inflight_.constEnd() && QDateTime::currentMSecsSinceEpoch() - it.value() < kInflightMaxAgeMs * 2)
        return; // the answer is broadcast to every listener
    events_inflight_.insert(sym, QDateTime::currentMSecsSinceEpoch());
    run_python_coded(QStringLiteral("equity_research_data.py"), {QStringLiteral("events"), sym},
                     [this, sym, cache_key](bool ok, const QString& out, const QString& code) {
                         events_inflight_.remove(sym);
                         const QString fallback = tr("Failed to load dividends and earnings for %1").arg(sym);
                         if (!ok) {
                             report_failure(sym, QStringLiteral("Events"), {}, out, code, fallback);
                             return;
                         }
                         const QJsonObject obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                         if (obj.isEmpty() || obj.contains(QStringLiteral("error"))) {
                             report_failure(sym, QStringLiteral("Events"), {},
                                            obj.value(QStringLiteral("error")).toString(),
                                            obj.value(QStringLiteral("code")).toString(), fallback);
                             return;
                         }
                         fincept::CacheManager::instance().put(
                             cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                             kEventsTtlSec, "equity");
                         emit events_loaded(parse_events(obj));
                     });
}

void EquityResearchService::fetch_risk_free(const QString& currency, RiskFreeCallback done) {
    if (!done)
        return;
    const QString ccy = currency.trimmed();
    if (ccy.isEmpty()) {
        done(false, {}, tr("No currency"));
        return;
    }
    auto parse = [](const QJsonObject& o) {
        RiskFreeRate r;
        r.currency = o.value(QStringLiteral("currency")).toString();
        r.rate_pct = o.value(QStringLiteral("rate_pct")).toDouble();
        r.source = o.value(QStringLiteral("source")).toString();
        r.label = o.value(QStringLiteral("label")).toString();
        r.as_of = o.value(QStringLiteral("as_of")).toString();
        return r;
    };
    const QString cache_key = QStringLiteral("equity:riskfree:") + ccy.toUpper();
    if (const auto hit = fincept::CacheManager::instance().try_get(cache_key)) {
        const QJsonObject obj = QJsonDocument::fromJson(hit->toUtf8()).object();
        if (obj.contains(QStringLiteral("rate_pct"))) {
            done(true, parse(obj), {});
            return;
        }
    }
    run_python(QStringLiteral("equity_research_data.py"), {QStringLiteral("risk_free"), ccy},
               [cache_key, done, parse](bool ok, const QString& out) {
                   const QJsonObject obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                   if (!ok || !obj.contains(QStringLiteral("rate_pct"))) {
                       const QString err = obj.value(QStringLiteral("error")).toString();
                       done(false, {}, err.isEmpty() ? out.left(300) : err);
                       return;
                   }
                   // A reference value is not market data: keep it for an hour only, so a
                   // later session that can reach the live source picks that up.
                   const bool reference = obj.value(QStringLiteral("source")).toString() == QLatin1String("reference");
                   fincept::CacheManager::instance().put(
                       cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                       reference ? 3600 : kRiskFreeTtlSec, "equity");
                   done(true, parse(obj), {});
               });
}

void EquityResearchService::resolve_symbol(const QString& text, ResolveCallback done) {
    if (!done)
        return;
    const QString query = text.trimmed();
    if (query.isEmpty()) {
        done(false, {}, tr("Nothing to look up."));
        return;
    }
    const QString cache_key = QStringLiteral("equity:resolve:") + query.toUpper();
    if (const auto hit = fincept::CacheManager::instance().try_get(cache_key)) {
        const QJsonObject obj = QJsonDocument::fromJson(hit->toUtf8()).object();
        if (!obj.isEmpty()) {
            done(true, eqrs_parse_resolution(query, obj), {});
            return;
        }
    }

    // Straight to the runner (not run_python): on failure the reply JSON in `output` can
    // still answer the question ({"error", "found": false, "candidates": [...]}).
    QPointer<EquityResearchService> self = this;
    python::PythonRunner::instance().run(
        QStringLiteral("yfinance_data.py"), {QStringLiteral("resolve_symbol"), query},
        [self, query, cache_key, done](python::PythonResult result) {
            if (!self)
                return;
            const QJsonDocument doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8());
            const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
            const bool answered = obj.contains(QStringLiteral("found")) || obj.contains(QStringLiteral("resolved_symbol"));
            bool lookup_failed = !answered;
            if (answered && !result.success && !obj.value(QStringLiteral("found")).toBool()) {
                // "not found" is an answer; "could not ask" (network, rate limit) is not.
                const QString code = obj.value(QStringLiteral("code")).toString().trimmed().toLower();
                lookup_failed = code == QLatin1String("network") || code == QLatin1String("rate_limited") ||
                                code == QLatin1String("unknown");
            }
            if (lookup_failed) {
                QString err = result.error.trimmed();
                if (err.isEmpty())
                    err = obj.value(QStringLiteral("error")).toString().trimmed();
                LOG_WARN("EquityResearch", QString("Symbol lookup for \"%1\" failed: %2").arg(query, err.left(200)));
                done(false, {}, err.isEmpty() ? tr("The symbol lookup failed.") : err.left(400));
                return;
            }
            const SymbolResolution res = eqrs_parse_resolution(query, obj);
            if (res.found)
                fincept::CacheManager::instance().put(
                    cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                    kResolveTtlSec, "equity");
            done(true, res, {});
        });
}

void EquityResearchService::fetch_price_history(const QString& symbol, const QString& period,
                                                const QString& interval, HistoryCallback done) {
    if (!done)
        return;
    const QString sym = symbol.trimmed();
    if (sym.isEmpty()) {
        done(false, {}, tr("No symbol to download."));
        return;
    }
    // Screens never call PythonRunner themselves (rule D1/P6): the CSV export dialog asks here.
    run_python("yfinance_data.py", {"historical_period", sym, period, interval},
               [this, sym, period, interval, done](bool ok, const QString& out) {
                   if (!ok) {
                       LOG_WARN("EquityResearch", QString("Price history export failed for %1 (%2/%3)")
                                                      .arg(sym, period, interval));
                       done(false, {}, out.trimmed().isEmpty() ? tr("The price history request failed.")
                                                               : out.trimmed().left(300));
                       return;
                   }
                   const QJsonDocument doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   if (doc.isObject() && doc.object().contains(QStringLiteral("error"))) {
                       done(false, {}, doc.object().value(QStringLiteral("error")).toString());
                       return;
                   }
                   QVector<Candle> candles = parse_candles(doc.array());
                   std::sort(candles.begin(), candles.end(),
                             [](const Candle& a, const Candle& b) { return a.timestamp < b.timestamp; });
                   done(true, candles, {});
               });
}

} // namespace fincept::services::equity
