// src/services/portfolio/PortfolioService_Metrics.cpp
//
// Analytics: one-shot market analytics (scripts/portfolio_metrics.py — holdings,
// FX, benchmark and risk-free rate in ONE threaded download, all in the base
// currency), what-if and historical-window stress, the realised metrics
// (PortfolioPerformance.h, computed on a worker thread), snapshot loading and the
// NAV history replay.
//
// Part of the partial-class split of PortfolioService.cpp.

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "python/PythonRunner.h"
#include "services/markets/FxRateService.h"
#include "services/portfolio/PortfolioPerformance.h"
#include "services/portfolio/PortfolioService.h"
#include "services/portfolio/PortfolioValuation.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>

namespace fincept::services {

namespace portfolio_metrics_detail {

// ── Risk-model tuning constants ───────────────────────────────────────────────
// The composite risk score is a 0–100 number built from normalised sub-scores.
// Each raw input is divided by a "cap" (the level at which that input is
// considered maximally risky and saturates its sub-score at 1.0), then weighted.
constexpr double kVolCapPct = 40.0;           // 40% annualised vol → max vol risk
constexpr double kConcentrationCapPct = 80.0; // top-3 weight 80% → max concentration risk
constexpr double kDrawdownCapPct = 50.0;      // 50% max drawdown → max drawdown risk
constexpr double kBetaCap = 2.0;              // |beta| 2.0 → max market-sensitivity risk
constexpr double kWVolFull = 30.0;
constexpr double kWConcFull = 25.0;
constexpr double kWDrawdownFull = 25.0;
constexpr double kWBetaFull = 20.0;
constexpr double kWVolDegraded = 50.0;
constexpr double kWConcDegraded = 50.0;

constexpr const char* kRfKeyPrefix = "portfolio.rf.";        // + CCY + ".value" / ".source"
constexpr const char* kLegacyRfValueKey = "portfolio.rf3m_value"; // USD, before v055
constexpr const char* kLegacyRfSourceKey = "portfolio.rf3m_source";
constexpr int kAnalyticsTimeoutMs = 150 * 1000;
constexpr int kReplayTimeoutMs = 240 * 1000;

std::optional<double> opt_double(const QJsonValue& v) {
    if (!v.isDouble())
        return std::nullopt;
    const double d = v.toDouble();
    return std::isfinite(d) ? std::optional<double>(d) : std::nullopt;
}

/// yfinance period covering the book's own history (earliest trade or stored NAV
/// snapshot), so the benchmark overlay spans the 5Y / ALL windows. The script keeps
/// its ex-ante risk statistics on a trailing one-year window whatever the period.
QString benchmark_period_for(const QString& portfolio_id) {
    auto& repo = PortfolioRepository::instance();
    const QDate today = QDate::currentDate();
    QDate earliest = today;
    if (auto r = repo.get_transactions(portfolio_id, /*limit=*/1000000); r.is_ok()) {
        for (const auto& t : r.value()) {
            const QDate d = QDate::fromString(t.transaction_date.left(10), Qt::ISODate);
            if (d.isValid() && d < earliest)
                earliest = d;
        }
    }
    if (auto r = repo.get_snapshots(portfolio_id, /*days=*/0); r.is_ok() && !r.value().isEmpty()) {
        const QDate d = QDate::fromString(r.value().first().snapshot_date.left(10), Qt::ISODate);
        if (d.isValid() && d < earliest)
            earliest = d;
    }
    const qint64 span_days = earliest.daysTo(today);
    if (span_days <= 365)
        return QStringLiteral("1y");
    if (span_days <= 730)
        return QStringLiteral("2y");
    if (span_days <= 1825)
        return QStringLiteral("5y");
    if (span_days <= 3650)
        return QStringLiteral("10y");
    return QStringLiteral("max");
}

QJsonObject risk_options_json(const portfolio::RiskOptions& o) {
    QJsonObject v;
    QJsonArray conf;
    for (double c : o.confidences)
        if (std::isfinite(c) && c >= 0.5 && c < 1.0)
            conf.append(c);
    if (conf.isEmpty())
        conf.append(0.95);
    v["confidences"] = conf;
    v["horizon_days"] = std::clamp(o.horizon_days, 1, 60);
    v["mc_paths"] = std::clamp(o.mc_paths, 0, 200000);
    if (o.seed)
        v["seed"] = static_cast<double>(*o.seed);
    return v;
}

QString risk_options_key(const portfolio::RiskOptions& o) {
    QStringList c;
    for (double x : o.confidences)
        c << QString::number(x, 'g', 6);
    return c.join(QLatin1Char('/')) + QLatin1Char('h') + QString::number(o.horizon_days) + QLatin1Char('m') +
           QString::number(o.mc_paths) + QLatin1Char('s') + (o.seed ? QString::number(*o.seed) : QStringLiteral("-"));
}

/// Calendar venue of a benchmark spec: its first leg's venue, else NYSE.
const core::market::Venue* calendar_venue_for(const QString& bench_spec) {
    const auto legs = PortfolioService::parse_benchmark(bench_spec);
    for (const auto& l : legs)
        if (const auto* v = core::market::venue_for_symbol(l.first))
            return v;
    return core::market::venue_by_code(QStringLiteral("NYSE"));
}

/// Holdings block of the script payloads: symbols, weights (fractions of the valued
/// total), quoted currencies.
void holdings_payload(const portfolio::PortfolioSummary& s, QJsonObject& payload) {
    QJsonArray syms;
    QJsonObject weights, ccys;
    for (const auto& h : s.holdings) {
        syms.append(h.symbol);
        if (h.valued() && std::isfinite(h.weight))
            weights.insert(h.symbol, h.weight / 100.0);
        if (!h.price_currency.isEmpty())
            ccys.insert(h.symbol, h.price_currency);
    }
    payload["symbols"] = syms;
    payload["weights"] = weights;
    payload["currencies"] = ccys;
    payload["base_currency"] = s.base_currency.isEmpty() ? s.portfolio.currency : s.base_currency;
}

portfolio::MarketAnalytics parse_analytics(const QJsonObject& o, const QString& pid, const QString& bench) {
    portfolio::MarketAnalytics a;
    a.portfolio_id = pid;
    a.as_of = o.value("as_of").toString();
    a.base_currency = o.value("base_currency").toString();
    const auto b = o.value("benchmark").toObject();
    a.benchmark = b.value("symbol").toString(bench);
    const auto bd = b.value("dates").toArray();
    const auto bc = b.value("closes").toArray();
    if (bd.size() == bc.size()) {
        for (int i = 0; i < bd.size(); ++i) {
            a.benchmark_dates.append(bd[i].toString());
            a.benchmark_closes.append(bc[i].toDouble());
        }
    }
    const auto rf = o.value("risk_free").toObject();
    if (auto r = opt_double(rf.value("rate"))) {
        a.risk_free_rate = r;
        a.risk_free_source = QStringLiteral("%1 (%2)").arg(rf.value("description").toString(), rf.value("as_of").toString());
    }
    const auto corr = o.value("correlation").toObject();
    a.correlation_window = corr.value("window").toInt();
    const auto mat = corr.value("matrix").toObject();
    for (auto it = mat.begin(); it != mat.end(); ++it)
        if (it.value().isDouble())
            a.correlation.insert(it.key(), it.value().toDouble());
    const auto p = o.value("portfolio").toObject();
    a.ann_vol = opt_double(p.value("ann_vol"));
    a.var_95_1d = opt_double(p.value("var_95_1d"));
    a.cvar_95_1d = opt_double(p.value("cvar_95_1d"));
    a.beta = opt_double(p.value("beta"));
    a.param_var_95_1d = opt_double(p.value("param_var_95_1d"));
    a.diversification_ratio = opt_double(p.value("diversification_ratio"));
    a.tracking_error = opt_double(p.value("tracking_error"));
    a.benchmark_corr = opt_double(p.value("benchmark_corr"));
    a.fx_vol_contribution = opt_double(p.value("fx_vol_contribution"));
    a.observations = p.value("observations").toInt();
    a.coverage = p.value("coverage").toDouble();
    if (auto ppy = opt_double(p.value("periods_per_year")); ppy && *ppy > 0)
        a.periods_per_year = *ppy;
    for (const auto& v : o.value("var").toArray()) {
        const auto vo = v.toObject();
        portfolio::VarEstimate e;
        e.confidence = vo.value("confidence").toDouble(0.95);
        e.horizon_days = vo.value("horizon_days").toInt(1);
        e.historical = opt_double(vo.value("historical"));
        e.cvar = opt_double(vo.value("cvar"));
        e.parametric = opt_double(vo.value("parametric"));
        e.monte_carlo = opt_double(vo.value("monte_carlo"));
        e.monte_carlo_cvar = opt_double(vo.value("monte_carlo_cvar"));
        e.horizon_scaled = vo.value("horizon_scaled").toBool();
        a.var.append(e);
    }
    a.var_confidence = o.value("var_confidence").toDouble(0.95);
    a.var_horizon_days = o.value("var_horizon_days").toInt(1);
    a.mc_paths = o.value("mc_paths").toInt();
    const auto hs = o.value("holdings").toObject();
    for (auto it = hs.begin(); it != hs.end(); ++it) {
        const auto h = it.value().toObject();
        portfolio::HoldingRisk hr;
        hr.ann_vol = opt_double(h.value("ann_vol"));
        hr.beta = opt_double(h.value("beta"));
        hr.risk_contribution = opt_double(h.value("risk_contribution"));
        hr.marginal_var = opt_double(h.value("marginal_var"));
        hr.component_var = opt_double(h.value("component_var"));
        hr.fx_vol = opt_double(h.value("fx_vol"));
        hr.observations = h.value("observations").toInt();
        a.holdings.insert(it.key(), hr);
    }
    for (const auto& v : o.value("missing").toArray())
        a.missing.append(v.toString());
    for (const auto& v : o.value("warnings").toArray())
        a.warnings.append(v.toString());
    return a;
}

/// One stdin-JSON call of portfolio_metrics.py, decoded to an AnalyticsResult. The
/// returned request's token cancels it (the callback then reports `cancelled`).
AnalyticsRequest run_metrics_command(QObject* ctx, const QString& command, const QJsonObject& payload,
                                     int timeout_ms, AnalyticsCallback cb) {
    const AnalyticsRequest req = PortfolioAnalyticsService::make_request();
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = timeout_ms;
    opts.stdin_data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    opts.cancel_token = req.token;
    QPointer<QObject> guard(ctx);
    python::PythonRunner::instance().run_with_options(
        QStringLiteral("portfolio_metrics.py"), {command}, opts,
        [guard, cb = std::move(cb), command, id = req.id, token = req.token](python::PythonResult result) {
            if (!guard)
                return;
            AnalyticsResult out;
            out.request_id = id;
            if (token && token->load()) {
                out.cancelled = true;
                out.error = QStringLiteral("Cancelled");
                if (cb)
                    cb(out);
                return;
            }
            QJsonParseError perr;
            const auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8(), &perr);
            if (doc.isObject() && doc.object().contains("error")) {
                out.error = doc.object().value("error").toString();
            } else if (!result.success) {
                out.error = result.error.isEmpty() ? command + QStringLiteral(" failed") : result.error;
            } else if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                out.error = command + QStringLiteral(" returned malformed JSON: ") + perr.errorString();
            } else {
                out.success = true;
                out.data = doc.object();
            }
            if (cb)
                cb(out);
        });
    return req;
}

} // namespace portfolio_metrics_detail

namespace pmd = portfolio_metrics_detail;

// ── Benchmark selection ───────────────────────────────────────────────────────

namespace {
struct DefaultBenchmark {
    const char* ccy;
    const char* symbol;
    bool total_return;
};
// Verified on Yahoo (Nov 2026) to return daily history. ETFs are fetched with
// dividend-adjusted closes, i.e. total return; "^" indices are price-only except
// ^NZ50 (a gross index). Currencies whose home index has no usable Yahoo history
// use ACWI (global, total return), converted into the base currency.
const DefaultBenchmark kDefaultBenchmarks[] = {
    {"USD", "SPY", true},           {"EUR", "EXSA.DE", true},      {"GBP", "ISF.L", true},
    {"JPY", "1306.T", true},        {"CHF", "CHSPI.SW", true},     {"CAD", "XIC.TO", true},
    {"AUD", "IOZ.AX", true},        {"NZD", "^NZ50", true},        {"HKD", "2800.HK", true},
    {"SGD", "ES3.SI", true},        {"SEK", "XACT-OMXS30.ST", true}, {"NOK", "ACWI", true},
    {"DKK", "^OMXC25", false},      {"CNY", "510300.SS", true},    {"INR", "NIFTYBEES.NS", true},
    {"BRL", "BOVA11.SA", true},     {"MXN", "NAFTRACISHRS.MX", true}, {"KRW", "069500.KS", true},
    {"TWD", "0050.TW", true},       {"THB", "ACWI", true},         {"ZAR", "STX40.JO", true},
    {"TRY", "XU100.IS", false},     {"RUB", "ACWI", true},         {"PLN", "ACWI", true},
    {"CZK", "ACWI", true},          {"HUF", "ACWI", true},         {"ILS", "TA35.TA", false},
    {"AED", "ACWI", true},          {"SAR", "ACWI", true},         {"IDR", "^JKSE", false},
    {"MYR", "^KLSE", false},        {"PHP", "ACWI", true},         {"VND", "E1VFVN30.VN", true},
    {"NGN", "ACWI", true},          {"EGP", "ACWI", true},         {"BDT", "ACWI", true},
};
} // namespace

QString PortfolioService::default_benchmark_for_currency(const QString& currency) {
    const QString c = core::market::major_currency(currency);
    for (const auto& d : kDefaultBenchmarks)
        if (c == QLatin1String(d.ccy))
            return QString::fromLatin1(d.symbol);
    return QStringLiteral("ACWI");
}

bool PortfolioService::default_benchmark_is_total_return(const QString& currency) {
    const QString c = core::market::major_currency(currency);
    for (const auto& d : kDefaultBenchmarks)
        if (c == QLatin1String(d.ccy))
            return d.total_return;
    return true; // ACWI
}

QString PortfolioService::benchmark_for(const portfolio::Portfolio& p) {
    const QString spec = p.benchmark.trimmed();
    if (!spec.isEmpty() && !parse_benchmark(spec).isEmpty())
        return spec;
    return default_benchmark_for_currency(p.currency);
}

QVector<QPair<QString, double>> PortfolioService::parse_benchmark(const QString& spec) {
    QVector<QPair<QString, double>> out;
    const QString s = spec.trimmed();
    if (s.isEmpty())
        return out;
    double total = 0;
    for (const QString& part : s.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString p = part.trimmed();
        // "SYM:60" — but a bare "SYM" (no weight) is weight 1. Yahoo symbols never contain ':'.
        const qsizetype colon = p.lastIndexOf(QLatin1Char(':'));
        QString sym = p;
        double w = 1.0;
        if (colon > 0) {
            sym = p.left(colon).trimmed();
            bool ok = false;
            w = p.mid(colon + 1).trimmed().toDouble(&ok);
            if (!ok || !std::isfinite(w) || w <= 0)
                return {};
        }
        sym = sym.toUpper();
        if (sym.isEmpty() || sym.contains(QLatin1Char(' ')))
            return {};
        out.append({sym, w});
        total += w;
    }
    if (out.isEmpty() || total <= 0)
        return {};
    for (auto& p : out)
        p.second /= total;
    return out;
}

double PortfolioService::periods_per_year_for(const portfolio::Portfolio& p, QString* calendar_id) {
    const auto* venue = pmd::calendar_venue_for(benchmark_for(p));
    if (calendar_id)
        *calendar_id = venue ? venue->id : QStringLiteral("NYSE");
    return venue ? core::market::trading_days_per_year(*venue) : 252.0;
}

QJsonObject PortfolioService::analytics_context(const portfolio::PortfolioSummary& summary) const {
    QJsonObject ctx;
    ctx["base_currency"] = summary.base_currency.isEmpty() ? summary.portfolio.currency : summary.base_currency;
    QJsonObject ccys;
    for (const auto& h : summary.holdings)
        if (!h.price_currency.isEmpty())
            ccys.insert(h.symbol, h.price_currency);
    ctx["currencies"] = ccys;
    ctx["benchmark"] = benchmark_for(summary.portfolio);
    ctx["periods_per_year"] = periods_per_year_for(summary.portfolio);
    const RiskFree rf = risk_free_for(summary.portfolio);
    if (rf.known)
        ctx["risk_free_rate"] = rf.rate;
    ctx["risk_free_known"] = rf.known;
    return ctx;
}

// ── Risk-free rate ────────────────────────────────────────────────────────────

void PortfolioService::ensure_risk_free_loaded() const {
    if (rf_loaded_)
        return;
    rf_loaded_ = true;
    auto& settings = SettingsRepository::instance();
    auto cat = settings.get_by_category(QStringLiteral("portfolio"));
    if (cat.is_ok()) {
        for (const auto& s : cat.value()) {
            if (!s.key.startsWith(QLatin1String(pmd::kRfKeyPrefix)) || !s.key.endsWith(QLatin1String(".value")))
                continue;
            const QString ccy = s.key.mid(int(qstrlen(pmd::kRfKeyPrefix))).section(QLatin1Char('.'), 0, 0);
            bool ok = false;
            const double v = s.value.toDouble(&ok);
            if (!ok || !std::isfinite(v) || v <= -0.05 || v >= 1.0)
                continue;
            const auto src = settings.get(QString::fromLatin1(pmd::kRfKeyPrefix) + ccy + QStringLiteral(".source"));
            rf_by_ccy_.insert(ccy, {v, src.is_ok() ? src.value() : ccy});
        }
    }
    // Deliberate USD: the single pre-v055 rate was the US 3-month T-bill (^IRX).
    if (!rf_by_ccy_.contains(QStringLiteral("USD"))) {
        auto val_r = settings.get(pmd::kLegacyRfValueKey);
        auto src_r = settings.get(pmd::kLegacyRfSourceKey);
        bool ok = false;
        const double v = val_r.is_ok() ? val_r.value().toDouble(&ok) : 0.0;
        if (ok && std::isfinite(v) && v > -0.01 && v < 0.3)
            rf_by_ccy_.insert(QStringLiteral("USD"), {v, src_r.is_ok() ? src_r.value() : QStringLiteral("^IRX")});
    }
}

PortfolioService::RiskFree PortfolioService::risk_free_for(const portfolio::Portfolio& p) const {
    RiskFree out;
    if (p.risk_free_override && std::isfinite(*p.risk_free_override)) {
        out.rate = *p.risk_free_override;
        out.source = tr("user setting");
        out.known = true;
        return out;
    }
    ensure_risk_free_loaded();
    const QString ccy = portfolio::base_currency_of(p);
    const auto it = rf_by_ccy_.constFind(ccy);
    if (it != rf_by_ccy_.constEnd()) {
        out.rate = it->first;
        out.source = it->second;
        out.known = true;
        return out;
    }
    out.rate = 0.0;
    out.source = tr("no %1 risk-free rate available — 0% used; set one in the portfolio settings").arg(ccy);
    out.known = false;
    return out;
}

// ── Market analytics ──────────────────────────────────────────────────────────

portfolio::MarketAnalytics PortfolioService::last_market_analytics(const QString& portfolio_id) const {
    const auto it = analytics_cache_.constFind(portfolio_id);
    return it == analytics_cache_.constEnd() ? portfolio::MarketAnalytics{} : it->data;
}

void PortfolioService::publish_market_analytics(const portfolio::MarketAnalytics& a) {
    if (a.risk_free_rate.has_value() && !a.base_currency.isEmpty()) {
        const QString ccy = core::market::major_currency(a.base_currency);
        rf_by_ccy_.insert(ccy, {*a.risk_free_rate, a.risk_free_source});
        auto& settings = SettingsRepository::instance();
        settings.set(QString::fromLatin1(pmd::kRfKeyPrefix) + ccy + QStringLiteral(".value"),
                     QString::number(*a.risk_free_rate, 'f', 6), "portfolio");
        settings.set(QString::fromLatin1(pmd::kRfKeyPrefix) + ccy + QStringLiteral(".source"), a.risk_free_source,
                     "portfolio");
    }
    emit market_analytics_loaded(a.portfolio_id, true, {});
}

void PortfolioService::fetch_market_analytics(const portfolio::PortfolioSummary& summary, bool force,
                                              const portfolio::RiskOptions& opts) {
    const QString pid = summary.portfolio.id;
    if (pid.isEmpty() || summary.holdings.isEmpty())
        return;
    const QString bench = benchmark_for(summary.portfolio);
    const QString period = pmd::benchmark_period_for(pid);
    const auto* venue = pmd::calendar_venue_for(bench);
    const double ppy = venue ? core::market::trading_days_per_year(*venue) : 252.0;

    // Key on quantities, not weights: weights drift with every tick and would
    // re-trigger the download; quantities only change on a trade. Which holdings are
    // VALUED is part of the key: the payload weights only those, and the first summary
    // of a multi-currency book is often built before its FX legs arrive — its run
    // (e.g. SAP.DE alone = 100 %) was cached for the whole day, so the RISK view and
    // the ribbon showed one stock's volatility (40.7 % instead of 16.6 %) and a
    // diversification ratio of 1.00.
    QStringList parts;
    parts.reserve(summary.holdings.size());
    for (const auto& h : summary.holdings)
        parts.append(h.symbol + QLatin1Char(':') + QString::number(h.quantity, 'g', 10) +
                     (h.valued() && std::isfinite(h.weight) ? QString() : QStringLiteral("!unvalued")));
    parts.sort();
    const QString key = pid + QLatin1Char('|') + QDate::currentDate().toString(Qt::ISODate) + QLatin1Char('|') +
                        bench + QLatin1Char('|') + summary.base_currency + QLatin1Char('|') + period +
                        QLatin1Char('|') + pmd::risk_options_key(opts) + QLatin1Char('|') + parts.join(QLatin1Char(','));

    const auto cached = analytics_cache_.constFind(pid);
    if (!force && cached != analytics_cache_.constEnd() && cached->key == key &&
        (QDateTime::currentSecsSinceEpoch() - cached->timestamp) < kAnalyticsTtlSec) {
        publish_market_analytics(cached->data);
        return;
    }
    if (analytics_inflight_.value(pid) == key)
        return; // identical request already running — it publishes when it lands
    analytics_inflight_.insert(pid, key);

    QJsonObject payload;
    pmd::holdings_payload(summary, payload);
    payload["benchmark"] = bench;
    payload["period"] = period;
    payload["corr_window"] = kCorrelationWindowDays;
    payload["periods_per_year"] = ppy;
    payload["var"] = pmd::risk_options_json(opts);
    payload["risk_free_currency"] = summary.base_currency;

    QPointer<PortfolioService> self = this;
    pmd::run_metrics_command(this, QStringLiteral("analytics"), payload, pmd::kAnalyticsTimeoutMs,
                             [self, pid, key, bench](const AnalyticsResult& r) {
                                 if (!self)
                                     return;
                                 // A newer request for a different book/day may have superseded this one.
                                 if (self->analytics_inflight_.value(pid) != key)
                                     return;
                                 self->analytics_inflight_.remove(pid);
                                 if (!r.success) {
                                     LOG_WARN("PortfolioSvc", "Market analytics failed for " + pid + ": " + r.error.left(300));
                                     emit self->market_analytics_loaded(pid, false, r.error.left(300));
                                     return;
                                 }
                                 auto a = pmd::parse_analytics(r.data, pid, bench);
                                 if (!a.warnings.isEmpty())
                                     LOG_INFO("PortfolioSvc", "Market analytics notes: " + a.warnings.join("; ").left(300));
                                 self->analytics_cache_.insert(pid, {key, QDateTime::currentSecsSinceEpoch(), a});
                                 self->publish_market_analytics(a);
                             });
}

AnalyticsRequest PortfolioService::what_if(const portfolio::PortfolioSummary& summary,
                                           const QHash<QString, double>& target_weights_pct, AnalyticsCallback cb,
                                           const portfolio::RiskOptions& opts) {
    QJsonObject payload;
    pmd::holdings_payload(summary, payload);
    QJsonObject after;
    QJsonArray syms = payload["symbols"].toArray();
    for (auto it = target_weights_pct.constBegin(); it != target_weights_pct.constEnd(); ++it) {
        const QString s = it.key().trimmed().toUpper();
        if (s.isEmpty() || !std::isfinite(it.value()) || it.value() < 0)
            continue;
        after.insert(s, it.value() / 100.0);
        if (!syms.contains(s))
            syms.append(s);
    }
    payload["symbols"] = syms;
    payload["weights_after"] = after;
    const QString bench = benchmark_for(summary.portfolio);
    const auto* venue = pmd::calendar_venue_for(bench);
    payload["benchmark"] = bench;
    payload["periods_per_year"] = venue ? core::market::trading_days_per_year(*venue) : 252.0;
    payload["var"] = pmd::risk_options_json(opts);
    payload["nav"] = summary.total_market_value;
    return pmd::run_metrics_command(this, QStringLiteral("what_if"), payload, pmd::kAnalyticsTimeoutMs,
                                    std::move(cb));
}

AnalyticsRequest PortfolioService::historical_stress(const portfolio::PortfolioSummary& summary,
                                                     const QString& start, const QString& end,
                                                     AnalyticsCallback cb) {
    QJsonObject payload;
    pmd::holdings_payload(summary, payload);
    payload["benchmark"] = benchmark_for(summary.portfolio);
    payload["start"] = start;
    payload["end"] = end;
    payload["nav"] = summary.total_market_value;
    return pmd::run_metrics_command(this, QStringLiteral("historical_stress"), payload, pmd::kAnalyticsTimeoutMs,
                                    std::move(cb));
}

// ── Time-weighted return series (legacy helper) ───────────────────────────────

QVector<portfolio::TwrPoint>
PortfolioService::build_twr_series(const QVector<portfolio::PortfolioSnapshot>& snapshots,
                                   const QVector<portfolio::Transaction>& transactions) {
    return build_twr_series(snapshots, transactions, nullptr);
}

QVector<portfolio::TwrPoint>
PortfolioService::build_twr_series(const QVector<portfolio::PortfolioSnapshot>& snapshots,
                                   const QVector<portfolio::Transaction>& transactions,
                                   QStringList* approx_currencies) {
    if (approx_currencies)
        approx_currencies->clear();
    if (snapshots.isEmpty())
        return {};
    // The NAV is in the portfolio's base currency; so must every flow be. (This
    // helper used to take every row at 1:1 and every instrument as USD.)
    const QString pid = snapshots.first().portfolio_id;
    auto port = PortfolioRepository::instance().get_portfolio(pid);
    if (port.is_err()) {
        LOG_WARN("PortfolioService", QString("TWR series: portfolio %1 unreadable — no series").arg(pid));
        return {};
    }
    portfolio::LedgerOptions opt = instance().ledger_options(port.value());
    const QString base = opt.base_currency;
    const auto dated = opt.historical_fx;
    auto approx = std::make_shared<QSet<QString>>();
    opt.historical_fx = [dated, base, approx](const QString& ccy, const QDate& date) -> std::optional<double> {
        if (dated) {
            if (const auto r = dated(ccy, date))
                return r;
        }
        // The flow date's rate is not cached yet: today's spot rate, flagged.
        if (const auto r = FxRateService::instance().rate(ccy, base); r && std::isfinite(*r) && *r > 0.0) {
            approx->insert(ccy);
            return r;
        }
        return std::nullopt; // the flow's period is left unmeasured — never 1:1
    };
    const auto led = portfolio::replay_ledger(transactions, opt);
    if (!approx->isEmpty()) {
        QStringList list(approx->cbegin(), approx->cend());
        list.sort();
        LOG_INFO("PortfolioService", QString("TWR series of %1: flows in %2 converted at today's rate (dated rate not "
                                             "loaded) — series approximate")
                                         .arg(pid, list.join(QLatin1Char(','))));
        if (approx_currencies)
            *approx_currencies = list;
    }
    return portfolio::build_twr(snapshots, led.flows);
}

// ── Metrics computation ───────────────────────────────────────────────────────

void PortfolioService::compute_metrics(const portfolio::PortfolioSummary& summary, portfolio::PerfWindow window) {
    const QString pid = summary.portfolio.id;
    if (pid.isEmpty()) {
        emit metrics_computed({});
        return;
    }

    // Everything the worker needs, captured by value on the main thread.
    const portfolio::MarketAnalytics analytics = last_market_analytics(pid);
    const RiskFree rf = risk_free_for(summary.portfolio);
    const QString bench = benchmark_for(summary.portfolio);
    QHash<QString, QString> quote_ccy;
    for (const auto& h : summary.holdings)
        if (!h.price_currency.isEmpty())
            quote_ccy.insert(h.symbol.toUpper(), h.price_currency);
    const portfolio::LedgerOptions opt = ledger_options(summary.portfolio, quote_ccy);
    const core::market::Venue* venue = pmd::calendar_venue_for(bench);
    const double ppy = venue ? core::market::trading_days_per_year(*venue) : 252.0;
    const QString calendar = venue ? venue->id : QStringLiteral("NYSE");
    const QString job_key = pid + QLatin1Char('|') + portfolio::perf_window_key(window);
    const quint64 seq = ++metrics_seq_[job_key];
    const bool bench_matches = analytics.benchmark == bench && !analytics.benchmark_dates.isEmpty();

    QPointer<PortfolioService> self = this;
    (void)QtConcurrent::run([self, pid, window, analytics, rf, bench, opt, venue, ppy, calendar, job_key, seq,
                             bench_matches, summary]() {
        // Worker thread: the repository hands this thread its own SQLite connection.
        auto& repo = PortfolioRepository::instance();
        QVector<portfolio::PortfolioSnapshot> snaps;
        if (auto r = repo.get_snapshots(pid, /*days=*/0); r.is_ok())
            snaps = r.value();
        QVector<portfolio::Transaction> txns;
        if (auto r = repo.get_transactions(pid, /*limit=*/1000000); r.is_ok())
            txns = r.value();
        const portfolio::LedgerResult led = portfolio::replay_ledger(txns, opt);

        portfolio::PerfInputs in;
        in.snapshots = snaps;
        in.flows = led.flows;
        in.window = window;
        if (bench_matches) {
            in.benchmark_dates = analytics.benchmark_dates;
            in.benchmark_levels = analytics.benchmark_closes;
        }
        in.risk_free = rf.rate;
        in.periods_per_year = ppy;
        in.calendar_id = calendar;
        if (venue)
            in.is_trading_day = [venue](const QDate& d) { return core::market::is_trading_day(*venue, d); };

        portfolio::ComputedMetrics m;
        portfolio::compute_realised_metrics(in, m);
        const int snap_count = static_cast<int>(snaps.size());

        QMetaObject::invokeMethod(
            self.data(),
            [self, m, pid, analytics, rf, bench, job_key, seq, snap_count, summary]() mutable {
                if (!self || self->metrics_seq_.value(job_key) != seq)
                    return; // superseded
                m.portfolio_id = pid;
                m.base_currency = summary.base_currency;
                m.benchmark_symbol = bench;
                m.risk_free_rate = rf.rate;
                m.risk_free_source = rf.source;
                m.risk_free_known = rf.known;

                if (snap_count < 3 && !self->backfill_attempted_.contains(pid)) {
                    // One-shot per process: reconstruct NAV history from the ledger.
                    self->backfill_attempted_.insert(pid);
                    self->backfill_history(pid);
                }

                // ── Holdings-based figures ─────────────────────────────────
                QVector<QPair<QString, double>> w;
                double valued_mv = 0;
                for (const auto& h : summary.holdings) {
                    if (h.valued() && std::isfinite(h.weight)) {
                        w.append({h.symbol, h.weight});
                        valued_mv += h.market_value;
                    }
                }
                const auto conc = portfolio::concentration_of(w);
                if (std::isfinite(conc.top3))
                    m.concentration_top3 = conc.top3;
                if (std::isfinite(conc.effective_n))
                    m.effective_n = conc.effective_n;

                // ── Ex-ante risk at current weights ────────────────────────
                const bool ex_ante = analytics.benchmark == bench && analytics.ann_vol.has_value();
                if (ex_ante) {
                    m.ex_ante_volatility = *analytics.ann_vol * 100.0;
                    m.ex_ante_beta = analytics.beta;
                    m.risk_ex_ante = true;
                    m.risk_observations = analytics.observations;
                    // VaR in currency on the part of the book the run had history for.
                    const double covered = valued_mv * std::clamp(analytics.coverage, 0.0, 1.0);
                    if (analytics.var_95_1d && covered > 0)
                        m.var_95 = *analytics.var_95_1d * covered;
                    if (analytics.cvar_95_1d && covered > 0)
                        m.cvar_95 = *analytics.cvar_95_1d * covered;
                    if (analytics.coverage < 0.999)
                        m.notes << tr("VaR covers %1% of the book (the rest has no price history)")
                                       .arg(analytics.coverage * 100.0, 0, 'f', 0);
                    m.diversification_ratio = analytics.diversification_ratio;
                    if (!m.tracking_error && analytics.tracking_error) {
                        m.tracking_error = *analytics.tracking_error * 100.0;
                        m.tracking_error_ex_ante = true;
                    }
                }
                // `volatility` / `beta`: realised when the window has enough history.
                if (m.realised_volatility) {
                    m.volatility = m.realised_volatility;
                } else if (m.ex_ante_volatility) {
                    m.volatility = m.ex_ante_volatility;
                    m.volatility_is_ex_ante = true;
                }
                if (m.realised_beta) {
                    m.beta = m.realised_beta;
                } else if (m.ex_ante_beta) {
                    m.beta = m.ex_ante_beta;
                    m.volatility_is_ex_ante = true;
                }
                if (m.var_95)
                    m.var_95_10d = *m.var_95 * std::sqrt(10.0);
                if (!rf.known)
                    m.notes << rf.source;

                // ── Composite risk score (0-100) — only with a real volatility ─
                if (m.volatility.has_value() && m.concentration_top3) {
                    const double vol = *m.volatility;
                    const double conc3 = *m.concentration_top3;
                    if (m.max_drawdown.has_value() && m.beta.has_value()) {
                        m.risk_score =
                            std::min(vol / pmd::kVolCapPct, 1.0) * pmd::kWVolFull +
                            std::min(conc3 / pmd::kConcentrationCapPct, 1.0) * pmd::kWConcFull +
                            std::min(std::abs(*m.max_drawdown) / pmd::kDrawdownCapPct, 1.0) * pmd::kWDrawdownFull +
                            std::min(std::abs(*m.beta) / pmd::kBetaCap, 1.0) * pmd::kWBetaFull;
                    } else {
                        m.risk_score = std::min(vol / pmd::kVolCapPct, 1.0) * pmd::kWVolDegraded +
                                       std::min(conc3 / pmd::kConcentrationCapPct, 1.0) * pmd::kWConcDegraded;
                    }
                }
                emit self->metrics_computed(m);
            },
            Qt::QueuedConnection);
    });
}

// ── Snapshots ─────────────────────────────────────────────────────────────────

void PortfolioService::load_snapshots(const QString& portfolio_id, int days) {
    auto r = PortfolioRepository::instance().get_snapshots(portfolio_id, days);
    if (r.is_ok()) {
        emit snapshots_loaded(portfolio_id, r.value());
    } else {
        LOG_WARN("PortfolioSvc", "Failed to load snapshots: " + QString::fromStdString(r.error()));
    }
}

void PortfolioService::backfill_history(const QString& portfolio_id, const QString& /*period*/) {
    if (portfolio_id.isEmpty())
        return;

    auto& repo = PortfolioRepository::instance();
    auto port_r = repo.get_portfolio(portfolio_id);
    auto txns_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    if (port_r.is_err() || txns_r.is_err() || txns_r.value().isEmpty()) {
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    const portfolio::Portfolio port = port_r.value();
    const QString base = portfolio::base_currency_of(port);

    // The quote currency of each instrument tells the replay what unit its
    // currency-less (legacy) rows were in; the cached summary knows it best.
    QHash<QString, QString> quote_ccy;
    {
        QMutexLocker lock(&cache_mutex_);
        const auto it = summary_cache_.constFind(portfolio_id);
        if (it != summary_cache_.constEnd())
            for (const auto& h : it->summary.holdings)
                if (!h.price_currency.isEmpty())
                    quote_ccy.insert(h.symbol.toUpper(), h.price_currency);
    }

    QJsonArray tx_arr;
    QJsonObject instr;
    for (const auto& t : txns_r.value()) {
        QJsonObject obj;
        obj["id"] = t.id;
        obj["date"] = t.transaction_date.left(10);
        obj["symbol"] = t.symbol;
        obj["type"] = t.transaction_type.toUpper();
        obj["quantity"] = t.quantity;
        obj["price"] = t.price;
        obj["fees"] = t.fees;
        obj["taxes"] = t.taxes;
        obj["currency"] = t.currency;
        obj["fx_rate"] = t.fx_rate;
        obj["created_at"] = t.created_at;
        tx_arr.append(obj);
        const QString sym = t.symbol.toUpper();
        if (!sym.isEmpty() && !instr.contains(sym)) {
            QString c = quote_ccy.value(sym);
            if (c.isEmpty())
                c = core::market::currency_for_symbol(sym);
            if (!c.isEmpty())
                instr.insert(sym, c);
        }
    }
    QJsonObject payload;
    payload["transactions"] = tx_arr;
    payload["base_currency"] = base;
    payload["track_cash"] = port.track_cash;
    payload["instrument_currencies"] = instr;

    QPointer<PortfolioService> self = this;
    pmd::run_metrics_command(
        this, QStringLiteral("nav_replay"), payload, pmd::kReplayTimeoutMs,
        [self, portfolio_id](const AnalyticsResult& r) {
            if (!self)
                return;
            if (!r.success) {
                LOG_WARN("PortfolioSvc", QString("backfill_history failed for %1: %2").arg(portfolio_id, r.error.left(200)));
                emit self->history_backfilled(portfolio_id, 0);
                return;
            }
            const auto& obj = r.data;
            const auto dates = obj["dates"].toArray();
            const auto navs = obj["navs"].toArray();
            const auto costs = obj["costs"].toArray();
            QStringList notes;
            for (const auto& v : obj["warnings"].toArray())
                notes << v.toString();
            if (!notes.isEmpty())
                emit self->history_notes(portfolio_id, notes);
            if (dates.isEmpty() || dates.size() != navs.size()) {
                emit self->history_backfilled(portfolio_id, 0);
                return;
            }
            const bool have_costs = (costs.size() == dates.size());

            // INSERT OR REPLACE keyed by (portfolio, date): a re-run corrects
            // existing rows in place. Non-finite values are never written.
            auto& repo = PortfolioRepository::instance();
            int written = 0;
            for (int i = 0; i < dates.size(); ++i) {
                const QString d = dates[i].toString();
                // A day whose NAV or cost the replay could not establish (missing FX) is
                // skipped — the snapshot row has no "unknown" value to store.
                if (!navs[i].isDouble() || (have_costs && !costs[i].isDouble()))
                    continue;
                const double nav = navs[i].toDouble();
                const double cost_basis = have_costs ? costs[i].toDouble() : 0.0;
                const double pnl = nav - cost_basis;
                const double pnl_pct = cost_basis > 0 ? (pnl / cost_basis) * 100.0 : 0.0;
                if (repo.save_snapshot(portfolio_id, nav, cost_basis, pnl, pnl_pct, d).is_ok())
                    ++written;
            }

            LOG_DEBUG("PortfolioSvc", QString("Replayed %1 historical snapshots for %2").arg(written).arg(portfolio_id));
            self->invalidate_cache(portfolio_id);
            emit self->history_backfilled(portfolio_id, written);
        });
}

} // namespace fincept::services
