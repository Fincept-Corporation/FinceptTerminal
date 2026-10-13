// src/services/portfolio/PortfolioService_Metrics.cpp
//
// Analytics: one-shot market analytics (scripts/portfolio_metrics.py — holdings,
// benchmark and risk-free rate in ONE threaded download), the flow-adjusted
// time-weighted return series, compute_metrics, snapshot loading and history
// backfill.
//
// Part of the partial-class split of PortfolioService.cpp.

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
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

// Weights when all four signals exist (sum 100) and when only vol +
// concentration are known (sum 100).
constexpr double kWVolFull = 30.0;
constexpr double kWConcFull = 25.0;
constexpr double kWDrawdownFull = 25.0;
constexpr double kWBetaFull = 20.0;
constexpr double kWVolDegraded = 50.0;
constexpr double kWConcDegraded = 50.0;

constexpr double kVar95Tail = 0.05;
// Minimum TWR periods before a statistic is shown at all.
constexpr int kMinPerfObs = 20;
constexpr int kMinDrawdownObs = 5;
// Annualised return is only reported over at least half a year (GIPS-style:
// annualising a few weeks produces absurd numbers).
constexpr int kMinAnnualiseDays = 182;
// A flow-adjusted period return outside this band is an unrecorded deposit /
// withdrawal or a bad print, not performance — excluded and counted.
constexpr double kMaxPlausibleReturn = 1.0;
constexpr double kMinPlausibleReturn = -0.9;

constexpr const char* kRfValueKey = "portfolio.rf3m_value";
constexpr const char* kRfSourceKey = "portfolio.rf3m_source";
constexpr int kAnalyticsTimeoutMs = 120 * 1000;

std::optional<double> opt_double(const QJsonValue& v) {
    if (!v.isDouble())
        return std::nullopt;
    const double d = v.toDouble();
    return std::isfinite(d) ? std::optional<double>(d) : std::nullopt;
}

/// Close on or before @p date from parallel (ascending) date/close vectors.
std::optional<double> close_as_of(const QStringList& dates, const QVector<double>& closes, const QString& date) {
    if (dates.isEmpty() || dates.size() != closes.size())
        return std::nullopt;
    const auto it = std::upper_bound(dates.begin(), dates.end(), date);
    if (it == dates.begin())
        return std::nullopt;
    const qsizetype idx = std::distance(dates.begin(), it) - 1;
    const double c = closes[idx];
    return c > 0 ? std::optional<double>(c) : std::nullopt;
}

portfolio::MarketAnalytics parse_analytics(const QJsonObject& o, const QString& pid, const QString& bench) {
    portfolio::MarketAnalytics a;
    a.portfolio_id = pid;
    a.as_of = o.value("as_of").toString();
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
        a.risk_free_source = QStringLiteral("%1 %2 (%3)")
                                 .arg(rf.value("symbol").toString(), rf.value("description").toString(),
                                      rf.value("as_of").toString());
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
    a.observations = p.value("observations").toInt();
    a.coverage = p.value("coverage").toDouble();
    const auto hs = o.value("holdings").toObject();
    for (auto it = hs.begin(); it != hs.end(); ++it) {
        const auto h = it.value().toObject();
        portfolio::HoldingRisk hr;
        hr.ann_vol = opt_double(h.value("ann_vol"));
        hr.beta = opt_double(h.value("beta"));
        hr.risk_contribution = opt_double(h.value("risk_contribution"));
        hr.observations = h.value("observations").toInt();
        a.holdings.insert(it.key(), hr);
    }
    for (const auto& v : o.value("missing").toArray())
        a.missing.append(v.toString());
    for (const auto& v : o.value("warnings").toArray())
        a.warnings.append(v.toString());
    return a;
}

} // namespace portfolio_metrics_detail

namespace pmd = portfolio_metrics_detail;

// ── Benchmark selection ───────────────────────────────────────────────────────

QString PortfolioService::default_benchmark_for_currency(const QString& currency) {
    const QString c = currency.trimmed().toUpper();
    if (c == "CAD")
        return QStringLiteral("^GSPTSE");
    if (c == "GBP")
        return QStringLiteral("^FTSE");
    if (c == "EUR")
        return QStringLiteral("^STOXX50E");
    if (c == "AUD")
        return QStringLiteral("^AXJO");
    if (c == "INR")
        return QStringLiteral("^NSEI");
    if (c == "JPY")
        return QStringLiteral("^N225");
    if (c == "HKD")
        return QStringLiteral("^HSI");
    return QStringLiteral("SPY"); // USD and unknown
}

// ── Risk-free rate ────────────────────────────────────────────────────────────

void PortfolioService::ensure_risk_free_loaded() {
    if (rf_loaded_)
        return;
    rf_loaded_ = true;
    auto& settings = SettingsRepository::instance();
    auto val_r = settings.get(pmd::kRfValueKey);
    auto src_r = settings.get(pmd::kRfSourceKey);
    bool ok = false;
    const double v = val_r.is_ok() ? val_r.value().toDouble(&ok) : 0.0;
    if (ok && std::isfinite(v) && v > -0.01 && v < 0.3) {
        rf_rate_ = v;
        rf_source_ = tr("%1 — last known value").arg(src_r.is_ok() ? src_r.value() : QStringLiteral("^IRX"));
    } else {
        rf_rate_ = kDefaultRiskFreeRate;
        rf_source_ = tr("default %1% — live T-bill rate not fetched yet").arg(kDefaultRiskFreeRate * 100.0, 0, 'f', 2);
    }
}

// ── Market analytics ──────────────────────────────────────────────────────────

portfolio::MarketAnalytics PortfolioService::last_market_analytics(const QString& portfolio_id) const {
    const auto it = analytics_cache_.constFind(portfolio_id);
    return it == analytics_cache_.constEnd() ? portfolio::MarketAnalytics{} : it->data;
}

void PortfolioService::publish_market_analytics(const portfolio::MarketAnalytics& a) {
    if (a.risk_free_rate.has_value()) {
        rf_rate_ = *a.risk_free_rate;
        rf_source_ = a.risk_free_source;
        rf_loaded_ = true;
        auto& settings = SettingsRepository::instance();
        settings.set(pmd::kRfValueKey, QString::number(rf_rate_, 'f', 6), "portfolio");
        settings.set(pmd::kRfSourceKey, a.risk_free_source, "portfolio");
    }
    emit market_analytics_loaded(a.portfolio_id, true, {});
}

void PortfolioService::fetch_market_analytics(const portfolio::PortfolioSummary& summary, bool force) {
    const QString pid = summary.portfolio.id;
    if (pid.isEmpty() || summary.holdings.isEmpty())
        return;
    const QString bench = default_benchmark_for_currency(summary.portfolio.currency);

    // Key on quantities, not weights: weights drift with every tick and would
    // re-trigger the download; quantities only change on a trade.
    QStringList parts;
    parts.reserve(summary.holdings.size());
    for (const auto& h : summary.holdings)
        parts.append(h.symbol + QLatin1Char(':') + QString::number(h.quantity, 'g', 10));
    parts.sort();
    const QString key = pid + QLatin1Char('|') + QDate::currentDate().toString(Qt::ISODate) + QLatin1Char('|') +
                        bench + QLatin1Char('|') + parts.join(QLatin1Char(','));

    const auto cached = analytics_cache_.constFind(pid);
    if (!force && cached != analytics_cache_.constEnd() && cached->key == key &&
        (QDateTime::currentSecsSinceEpoch() - cached->timestamp) < kAnalyticsTtlSec) {
        publish_market_analytics(cached->data);
        return;
    }
    if (analytics_inflight_.value(pid) == key)
        return; // identical request already running — it publishes when it lands
    analytics_inflight_.insert(pid, key);

    QJsonArray syms;
    QJsonObject weights;
    for (const auto& h : summary.holdings) {
        syms.append(h.symbol);
        weights.insert(h.symbol, h.weight / 100.0);
    }
    QJsonObject payload;
    payload["symbols"] = syms;
    payload["weights"] = weights;
    payload["benchmark"] = bench;
    payload["period"] = "1y";
    payload["corr_window"] = kCorrelationWindowDays;

    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = pmd::kAnalyticsTimeoutMs;
    opts.stdin_data = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_with_options(
        QStringLiteral("portfolio_metrics.py"), {QStringLiteral("analytics")}, opts,
        [self, pid, key, bench](python::PythonResult result) {
            if (!self)
                return;
            // A newer request for a different book/day may have superseded this
            // one; publishing the stale result would paint the wrong numbers.
            if (self->analytics_inflight_.value(pid) != key)
                return;
            self->analytics_inflight_.remove(pid);

            QJsonObject root;
            QString error;
            if (!result.success) {
                error = result.error.isEmpty() ? QStringLiteral("analytics script failed") : result.error;
            } else {
                QJsonParseError perr;
                const auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8(), &perr);
                if (perr.error != QJsonParseError::NoError || !doc.isObject())
                    error = QStringLiteral("analytics returned malformed JSON: ") + perr.errorString();
                else
                    root = doc.object();
            }
            if (!error.isEmpty()) {
                LOG_WARN("PortfolioSvc", "Market analytics failed for " + pid + ": " + error.left(300));
                emit self->market_analytics_loaded(pid, false, error.left(300));
                return;
            }
            auto a = pmd::parse_analytics(root, pid, bench);
            if (!a.warnings.isEmpty())
                LOG_INFO("PortfolioSvc", "Market analytics notes: " + a.warnings.join("; ").left(300));
            self->analytics_cache_.insert(pid, {key, QDateTime::currentSecsSinceEpoch(), a});
            self->publish_market_analytics(a);
        });
}

// ── Time-weighted return series ───────────────────────────────────────────────

QVector<portfolio::TwrPoint>
PortfolioService::build_twr_series(const QVector<portfolio::PortfolioSnapshot>& snapshots,
                                   const QVector<portfolio::Transaction>& transactions) {
    // Snapshots ascending, one per date (last write wins).
    QVector<portfolio::PortfolioSnapshot> snaps = snapshots;
    std::stable_sort(snaps.begin(), snaps.end(),
                     [](const auto& a, const auto& b) { return a.snapshot_date.left(10) < b.snapshot_date.left(10); });
    QVector<portfolio::PortfolioSnapshot> uniq;
    uniq.reserve(snaps.size());
    for (const auto& s : snaps) {
        if (!uniq.isEmpty() && uniq.last().snapshot_date.left(10) == s.snapshot_date.left(10))
            uniq.last() = s;
        else
            uniq.append(s);
    }

    // External flows by date: a BUY brings capital in, a SELL takes proceeds out.
    QMap<QString, double> flows; // ordered by date
    for (const auto& t : transactions) {
        const QString type = t.transaction_type.toUpper();
        if (!std::isfinite(t.quantity) || !std::isfinite(t.price))
            continue;
        const QString d = t.transaction_date.left(10);
        if (type == QLatin1String("BUY"))
            flows[d] += t.quantity * t.price;
        else if (type == QLatin1String("SELL"))
            flows[d] -= t.quantity * t.price;
    }

    QVector<portfolio::TwrPoint> out;
    out.reserve(uniq.size());
    double cum = 0.0;
    for (int i = 0; i < uniq.size(); ++i) {
        portfolio::TwrPoint p;
        p.date = uniq[i].snapshot_date.left(10);
        p.nav = uniq[i].total_value;
        if (i == 0) {
            p.valid = false; // the base point carries no return
            out.append(p);
            continue;
        }
        const QString prev = out.last().date;
        double flow = 0.0;
        for (auto it = flows.upperBound(prev); it != flows.end() && it.key() <= p.date; ++it)
            flow += it.value();
        p.net_flow = flow;
        const double base = out.last().nav;
        if (base > 1e-9 && std::isfinite(p.nav)) {
            const double r = (p.nav - flow) / base - 1.0;
            if (std::isfinite(r) && r <= pmd::kMaxPlausibleReturn && r >= pmd::kMinPlausibleReturn) {
                p.period_return = r;
                cum = (1.0 + cum) * (1.0 + r) - 1.0;
            } else {
                p.valid = false;
            }
        } else {
            p.valid = false;
        }
        p.cumulative = cum;
        out.append(p);
    }
    return out;
}

// ── Metrics computation ───────────────────────────────────────────────────────

void PortfolioService::compute_metrics(const portfolio::PortfolioSummary& summary) {
    if (summary.holdings.isEmpty()) {
        emit metrics_computed({});
        return;
    }
    ensure_risk_free_loaded();

    const QString pid = summary.portfolio.id;
    portfolio::ComputedMetrics m;
    m.benchmark_symbol = default_benchmark_for_currency(summary.portfolio.currency);
    m.risk_free_rate = rf_rate_;
    m.risk_free_source = rf_source_;

    // ── Concentration top-3 ───────────────────────────────────────────────────
    QVector<double> weights;
    weights.reserve(summary.holdings.size());
    for (const auto& h : summary.holdings)
        weights.append(h.weight);
    std::sort(weights.begin(), weights.end(), std::greater<>());
    double conc = 0;
    for (qsizetype i = 0; i < std::min(qsizetype{3}, weights.size()); ++i)
        conc += weights[i];
    m.concentration_top3 = conc;

    // ── Realised, flow-adjusted (time-weighted) returns ──────────────────────
    auto& repo = PortfolioRepository::instance();
    QVector<portfolio::PortfolioSnapshot> snaps;
    if (auto r = repo.get_snapshots(pid, 365); r.is_ok())
        snaps = r.value();
    if (snaps.size() < 3 && !backfill_attempted_.contains(pid)) {
        // One-shot per process: reconstruct NAV history from the ledger so the
        // realised figures have something to work with.
        backfill_attempted_.insert(pid);
        QPointer<PortfolioService> self = this;
        QMetaObject::invokeMethod(
            this,
            [self, pid]() {
                if (self)
                    self->backfill_history(pid);
            },
            Qt::QueuedConnection);
    }
    QVector<portfolio::Transaction> txns;
    if (auto r = repo.get_transactions(pid, 1000000); r.is_ok())
        txns = r.value();
    m.twr_series = build_twr_series(snaps, txns);

    QVector<double> rets;
    QVector<QString> starts, ends;
    for (int i = 1; i < m.twr_series.size(); ++i) {
        const auto& p = m.twr_series[i];
        if (!p.valid) {
            ++m.perf_excluded;
            continue;
        }
        rets.append(p.period_return);
        starts.append(m.twr_series[i - 1].date);
        ends.append(p.date);
    }
    if (!m.twr_series.isEmpty()) {
        m.perf_start = m.twr_series.first().date;
        m.perf_end = m.twr_series.last().date;
    }
    const int n = rets.size();
    m.perf_observations = n;

    if (n >= pmd::kMinDrawdownObs) {
        double idx = 1.0, peak = 1.0, mdd = 0.0;
        for (double r : rets) {
            idx *= (1.0 + r);
            peak = std::max(peak, idx);
            mdd = std::min(mdd, idx / peak - 1.0);
        }
        m.max_drawdown = mdd * 100.0;
        m.total_return = (idx - 1.0) * 100.0;
    }

    double ppy = 252.0;
    std::optional<double> realised_vol_pct;
    std::optional<double> realised_beta;
    if (n >= pmd::kMinPerfObs) {
        const QDate d0 = QDate::fromString(m.perf_start, Qt::ISODate);
        const QDate d1 = QDate::fromString(m.perf_end, Qt::ISODate);
        const int span_days = (d0.isValid() && d1.isValid()) ? static_cast<int>(d0.daysTo(d1)) : 0;
        const double years = span_days / 365.25;
        // Observations per year implied by the sample itself: ≈252 for
        // trading-day snapshots, ≈365 when weekends were snapshotted too (their
        // zero returns then scale back out), lower when the app was closed.
        if (years > 0)
            ppy = std::clamp(n / years, 1.0, 366.0);

        const double mean = std::accumulate(rets.begin(), rets.end(), 0.0) / n;
        double ss = 0;
        for (double r : rets)
            ss += (r - mean) * (r - mean);
        const double sd = std::sqrt(ss / (n - 1));
        const double rf_p = std::pow(1.0 + rf_rate_, 1.0 / ppy) - 1.0;

        realised_vol_pct = sd * std::sqrt(ppy) * 100.0;
        if (sd > 1e-12)
            m.sharpe = (mean - rf_p) / sd * std::sqrt(ppy);

        // Sortino: downside deviation vs the risk-free target over ALL periods.
        double dss = 0;
        for (double r : rets) {
            const double shortfall = std::min(r - rf_p, 0.0);
            dss += shortfall * shortfall;
        }
        const double dd = std::sqrt(dss / n);
        if (dd > 1e-12)
            m.sortino = (mean - rf_p) / dd * std::sqrt(ppy);

        if (span_days >= pmd::kMinAnnualiseDays && m.total_return.has_value())
            m.annualized_return = (std::pow(1.0 + *m.total_return / 100.0, 1.0 / years) - 1.0) * 100.0;

        // Historical VaR/CVaR from the realised series (used only when the
        // ex-ante figures below are unavailable).
        if (summary.total_market_value > 0) {
            QVector<double> sorted = rets;
            std::sort(sorted.begin(), sorted.end());
            const int k = std::max(1, static_cast<int>(std::floor(n * pmd::kVar95Tail)));
            double tail = 0;
            for (int i = 0; i < k; ++i)
                tail += sorted[i];
            m.var_95 = summary.total_market_value * std::max(-sorted[k - 1], 0.0);
            m.cvar_95 = summary.total_market_value * std::max(-(tail / k), 0.0);
        }

        // Realised beta / Jensen's alpha vs the currency benchmark, each
        // period matched to the benchmark's move over the same dates.
        const auto a = last_market_analytics(pid);
        if (a.benchmark == m.benchmark_symbol && a.benchmark_dates.size() >= 2) {
            QVector<double> pr, br;
            for (int i = 0; i < n; ++i) {
                const auto b0 = pmd::close_as_of(a.benchmark_dates, a.benchmark_closes, starts[i]);
                const auto b1 = pmd::close_as_of(a.benchmark_dates, a.benchmark_closes, ends[i]);
                if (!b0 || !b1)
                    continue;
                pr.append(rets[i]);
                br.append(*b1 / *b0 - 1.0);
            }
            const int mc = pr.size();
            if (mc >= pmd::kMinPerfObs) {
                const double pm = std::accumulate(pr.begin(), pr.end(), 0.0) / mc;
                const double bm = std::accumulate(br.begin(), br.end(), 0.0) / mc;
                double cov = 0, var_b = 0;
                for (int i = 0; i < mc; ++i) {
                    cov += (pr[i] - pm) * (br[i] - bm);
                    var_b += (br[i] - bm) * (br[i] - bm);
                }
                if (var_b > 1e-14) {
                    const double beta = cov / var_b;
                    realised_beta = beta;
                    m.alpha = ((pm - rf_p) - beta * (bm - rf_p)) * ppy * 100.0;
                }
            }
        }
    }

    // ── Ex-ante risk at current weights (preferred for vol / beta / VaR) ──────
    const auto a = last_market_analytics(pid);
    if (a.benchmark == m.benchmark_symbol && a.ann_vol.has_value()) {
        m.risk_ex_ante = true;
        m.risk_observations = a.observations;
        m.volatility = *a.ann_vol * 100.0;
        m.beta = a.beta.has_value() ? a.beta : realised_beta;
        if (a.var_95_1d && summary.total_market_value > 0)
            m.var_95 = *a.var_95_1d * summary.total_market_value;
        if (a.cvar_95_1d && summary.total_market_value > 0)
            m.cvar_95 = *a.cvar_95_1d * summary.total_market_value;
    } else {
        m.volatility = realised_vol_pct;
        m.beta = realised_beta;
    }

    // ── Composite risk score (0-100) — only with a real volatility ───────────
    if (m.volatility.has_value()) {
        const double vol = *m.volatility;
        if (m.max_drawdown.has_value() && m.beta.has_value()) {
            m.risk_score = std::min(vol / pmd::kVolCapPct, 1.0) * pmd::kWVolFull +
                           std::min(conc / pmd::kConcentrationCapPct, 1.0) * pmd::kWConcFull +
                           std::min(std::abs(*m.max_drawdown) / pmd::kDrawdownCapPct, 1.0) * pmd::kWDrawdownFull +
                           std::min(std::abs(*m.beta) / pmd::kBetaCap, 1.0) * pmd::kWBetaFull;
        } else {
            m.risk_score = std::min(vol / pmd::kVolCapPct, 1.0) * pmd::kWVolDegraded +
                           std::min(conc / pmd::kConcentrationCapPct, 1.0) * pmd::kWConcDegraded;
        }
    }

    emit metrics_computed(m);
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

    // Full ledger: the replay walks every BUY/SELL since the portfolio began.
    auto txns_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    if (txns_r.is_err() || txns_r.value().isEmpty()) {
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    const auto txns = txns_r.value();

    // A temp file, not argv: 500 transactions would exceed Windows' 8191-char
    // argument limit. (yfinance_data.py's replay action reads a path.)
    QJsonArray tx_arr;
    for (const auto& t : txns) {
        QJsonObject obj;
        obj["date"] = t.transaction_date.left(10);
        obj["symbol"] = t.symbol;
        obj["type"] = t.transaction_type;
        obj["quantity"] = t.quantity;
        obj["price"] = t.price;
        tx_arr.append(obj);
    }

    const QString tmp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmp_dir);
    const QString tmp_path = QDir(tmp_dir).filePath(
        QString("fincept_replay_%1_%2.json").arg(portfolio_id.left(16)).arg(QDateTime::currentMSecsSinceEpoch()));

    QFile f(tmp_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_WARN("PortfolioSvc", "backfill_history: cannot open temp file " + tmp_path);
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    f.write(QJsonDocument(tx_arr).toJson(QJsonDocument::Compact));
    f.close();

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run(
        "yfinance_data.py", {"portfolio_nav_history_replay", tmp_path},
        [self, portfolio_id, tmp_path](python::PythonResult result) {
            QFile::remove(tmp_path); // always — leaks add up on frequent re-imports
            if (!self)
                return;
            if (!result.success || result.output.trimmed().isEmpty()) {
                LOG_WARN("PortfolioSvc",
                         QString("backfill_history failed for %1: %2").arg(portfolio_id, result.error.left(200)));
                emit self->history_backfilled(portfolio_id, 0);
                return;
            }
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8(), &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                LOG_WARN("PortfolioSvc", "backfill_history: bad JSON: " + err.errorString());
                emit self->history_backfilled(portfolio_id, 0);
                return;
            }
            const auto obj = doc.object();
            if (obj.contains("error")) {
                LOG_WARN("PortfolioSvc", "backfill_history: " + obj["error"].toString());
                emit self->history_backfilled(portfolio_id, 0);
                return;
            }
            const auto dates = obj["dates"].toArray();
            const auto navs = obj["navs"].toArray();
            const auto costs = obj["costs"].toArray();
            if (dates.isEmpty() || dates.size() != navs.size()) {
                emit self->history_backfilled(portfolio_id, 0);
                return;
            }
            const bool have_costs = (costs.size() == dates.size());

            // INSERT OR REPLACE keyed by (portfolio, date): a re-run corrects
            // existing rows in place.
            auto& repo = PortfolioRepository::instance();
            int written = 0;
            for (int i = 0; i < dates.size(); ++i) {
                const QString d = dates[i].toString();
                const double nav = navs[i].toDouble();
                const double cost_basis = have_costs ? costs[i].toDouble() : 0.0;
                const double pnl = nav - cost_basis;
                const double pnl_pct = cost_basis > 0 ? (pnl / cost_basis) * 100.0 : 0.0;
                if (repo.save_snapshot(portfolio_id, nav, cost_basis, pnl, pnl_pct, d).is_ok())
                    ++written;
            }

            LOG_INFO("PortfolioSvc", QString("Replayed %1 historical snapshots for %2").arg(written).arg(portfolio_id));
            self->invalidate_cache(portfolio_id);
            emit self->history_backfilled(portfolio_id, written);
        });
}

} // namespace fincept::services
