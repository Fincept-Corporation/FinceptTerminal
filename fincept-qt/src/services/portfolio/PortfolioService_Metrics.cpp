// src/services/portfolio/PortfolioService_Metrics.cpp
//
// Analytics: fetch_correlation, benchmark + SPY history, risk-free rate,
// compute_metrics, snapshot loading, history backfill.
//
// Part of the partial-class split of PortfolioService.cpp.

#include "services/portfolio/PortfolioService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/sectors/SectorResolver.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::services {

void PortfolioService::fetch_correlation(const QStringList& symbols) {
    if (symbols.size() < 2) {
        emit correlation_computed({});
        return;
    }

    // Build inline Python that embeds the symbol list, fetches 30-day closes,
    // and prints a JSON correlation matrix to stdout.
    QJsonArray sym_arr;
    for (const auto& s : symbols)
        sym_arr.append(s);
    const QString sym_json = QString::fromUtf8(QJsonDocument(sym_arr).toJson(QJsonDocument::Compact));

    const QString code = QString(R"python(
import json, sys
import yfinance as yf
import numpy as np

symbols = %1
data = {}
for sym in symbols:
    try:
        hist = yf.download(sym, period="30d", interval="1d", progress=False)
        if hist is not None and not hist.empty:
            closes = hist["Close"].dropna().tolist()
            if hasattr(closes[0], 'item'):
                closes = [v.item() for v in closes]
            data[sym] = closes
    except Exception:
        pass

# Compute daily returns
returns = {}
for sym, prices in data.items():
    if len(prices) >= 5:
        r = [(prices[i] - prices[i-1]) / prices[i-1] for i in range(1, len(prices))]
        returns[sym] = r

syms = list(returns.keys())
matrix = {}
for i in range(len(syms)):
    for j in range(len(syms)):
        a = returns[syms[i]]
        b = returns[syms[j]]
        n = min(len(a), len(b))
        if n < 2:
            val = 1.0 if i == j else 0.0
        else:
            a, b = a[-n:], b[-n:]
            ma, mb = sum(a)/n, sum(b)/n
            num = sum((a[k]-ma)*(b[k]-mb) for k in range(n))
            da  = sum((a[k]-ma)**2 for k in range(n))
            db  = sum((b[k]-mb)**2 for k in range(n))
            denom = (da*db)**0.5
            val = num/denom if denom > 1e-10 else (1.0 if i==j else 0.0)
            val = max(-1.0, min(1.0, val))
        matrix[syms[i] + "|" + syms[j]] = round(val, 4)

print(json.dumps(matrix))
)python")
                             .arg(sym_json);

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_code(code, [self](python::PythonResult result) {
        if (!self)
            return;
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("PortfolioSvc", "Correlation fetch failed: " + result.error.left(200));
            emit self->correlation_computed({});
            return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit self->correlation_computed({});
            return;
        }
        QHash<QString, double> matrix;
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            matrix[it.key()] = it.value().toDouble();
        emit self->correlation_computed(matrix);
    });
}

// ── SPY benchmark data ────────────────────────────────────────────────────────

QString PortfolioService::default_benchmark_for_currency(const QString& currency) {
    const QString c = currency.trimmed().toUpper();
    if (c == "CAD") return QStringLiteral("^GSPTSE");
    if (c == "GBP") return QStringLiteral("^FTSE");
    if (c == "EUR") return QStringLiteral("^STOXX50E");
    if (c == "AUD") return QStringLiteral("^AXJO");
    if (c == "INR") return QStringLiteral("^NSEI");
    if (c == "JPY") return QStringLiteral("^N225");
    if (c == "HKD") return QStringLiteral("^HSI");
    return QStringLiteral("SPY"); // USD and unknown
}

void PortfolioService::fetch_spy_history(const QString& period) {
    fetch_benchmark_history(QStringLiteral("SPY"), period);
}

void PortfolioService::fetch_benchmark_history(const QString& symbol, const QString& period) {
    // Allow callers to omit the symbol → defaults to SPY (legacy behaviour).
    const QString sym = symbol.isEmpty() ? QStringLiteral("SPY") : symbol;

    const QString code = QString(R"python(
import json, sys
import yfinance as yf

symbol = "%1"
period = "%2"
try:
    hist = yf.download(symbol, period=period, interval="1d", progress=False, auto_adjust=True)
    dates  = []
    closes = []
    if hist is not None and not hist.empty:
        for dt, row in hist.iterrows():
            v = row["Close"]
            if hasattr(v, "item"): v = v.item()
            dates.append(dt.strftime("%Y-%m-%d"))
            closes.append(float(v))
    print(json.dumps({"symbol": symbol, "dates": dates, "closes": closes}))
except Exception as e:
    print(json.dumps({"symbol": symbol, "dates": [], "closes": [], "error": str(e)}))
)python")
                             .arg(sym, period);

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_code(code, [self, sym](python::PythonResult result) {
        if (!self)
            return;
        QStringList dates;
        QVector<double> closes;
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("PortfolioSvc",
                     QString("Benchmark %1 fetch failed: %2").arg(sym, result.error.left(200)));
        } else {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                const auto obj = doc.object();
                const auto dates_arr = obj["dates"].toArray();
                const auto closes_arr = obj["closes"].toArray();
                dates.reserve(dates_arr.size());
                closes.reserve(closes_arr.size());
                for (const auto& v : dates_arr)
                    dates.append(v.toString());
                for (const auto& v : closes_arr)
                    closes.append(v.toDouble());
            }
        }

        // Beta computation in compute_metrics() always regresses against SPY,
        // so only update that cache when SPY is what the caller asked for —
        // otherwise we would corrupt Beta with e.g. TSX returns.
        if (sym == QStringLiteral("SPY")) {
            self->spy_dates_cache_ = dates;
            self->spy_closes_cache_ = closes;
            emit self->spy_history_loaded(dates, closes);
        }
        emit self->benchmark_history_loaded(sym, dates, closes);
    });
}

// ── Risk-free rate (FRED DGS10) ───────────────────────────────────────────────

void PortfolioService::fetch_risk_free_rate() {
    // Check 24h cache in SettingsRepository
    auto& settings = SettingsRepository::instance();
    const qint64 now_secs = QDateTime::currentSecsSinceEpoch();

    auto ts_r = settings.get("portfolio.rf_rate_timestamp");
    auto val_r = settings.get("portfolio.rf_rate_value");
    if (ts_r.is_ok() && val_r.is_ok()) {
        bool ts_ok = false, val_ok = false;
        const qint64 cached_ts = ts_r.value().toLongLong(&ts_ok);
        const double cached_val = val_r.value().toDouble(&val_ok);
        if (ts_ok && val_ok && (now_secs - cached_ts) < 86400) {
            // Cache still valid — use stored value
            rf_rate_ = cached_val;
            emit risk_free_rate_loaded(rf_rate_);
            return;
        }
    }

    // Fetch from FRED via inline Python (requests library)
    const QString code = QString(R"python(
import json, urllib.request
api_key = "9da0d86e0cf58f4a4023e5e686226d69"
url = (
    "https://api.stlouisfed.org/fred/series/observations"
    "?series_id=DGS10&api_key=" + api_key +
    "&file_type=json&sort_order=desc&limit=5"
)
try:
    with urllib.request.urlopen(url, timeout=10) as r:
        data = json.loads(r.read().decode())
    rate = None
    for obs in data.get("observations", []):
        v = obs.get("value", ".")
        if v != ".":
            rate = float(v) / 100.0  # convert percent to decimal
            break
    print(json.dumps({"rate": rate if rate is not None else 0.04}))
except Exception as e:
    print(json.dumps({"rate": 0.04, "error": str(e)}))
)python");

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_code(code, [self, now_secs](python::PythonResult result) {
        if (!self)
            return;
        double rate = 0.04; // fallback
        if (result.success && !result.output.trimmed().isEmpty()) {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.object().contains("rate"))
                rate = doc.object()["rate"].toDouble(0.04);
        }
        // Persist to 24h cache
        auto& settings = SettingsRepository::instance();
        settings.set("portfolio.rf_rate_timestamp", QString::number(now_secs));
        settings.set("portfolio.rf_rate_value", QString::number(rate, 'f', 6));
        self->rf_rate_ = rate;
        emit self->risk_free_rate_loaded(rate);
    });
}

// ── Metrics computation (async, P8-safe) ─────────────────────────────────────

void PortfolioService::compute_metrics(const portfolio::PortfolioSummary& summary) {
    if (summary.holdings.isEmpty()) {
        emit metrics_computed({});
        return;
    }

    portfolio::ComputedMetrics metrics;

    // ── Concentration top-3 ───────────────────────────────────────────────────
    QVector<double> weights;
    weights.reserve(summary.holdings.size());
    for (const auto& h : summary.holdings)
        weights.append(h.weight);
    std::sort(weights.begin(), weights.end(), std::greater<>());
    double conc = 0;
    for (qsizetype i = 0; i < std::min(qsizetype{3}, weights.size()); ++i)
        conc += weights[i];
    metrics.concentration_top3 = conc;
    metrics.risk_score = std::min(conc / 80.0, 1.0) * 50.0; // concentration-only baseline

    // ── Load snapshots synchronously for time-series metrics ─────────────────
    // (this runs on the calling thread — compute_metrics is always called from
    //  the UI thread after summary_loaded, so we keep computation fast by
    //  loading snapshots from SQLite which is sub-millisecond for <365 rows)
    auto snap_r = PortfolioRepository::instance().get_snapshots(summary.portfolio.id, 365);
    if (snap_r.is_err() || snap_r.value().size() < 3) {
        // Trigger an async backfill so the next compute_metrics call has data.
        // This is one-shot per process to avoid hammering yfinance — once we've
        // attempted, the user can manually re-trigger via re-import.
        if (!backfill_attempted_.contains(summary.portfolio.id)) {
            backfill_attempted_.insert(summary.portfolio.id);
            QPointer<PortfolioService> self = this;
            const QString pid = summary.portfolio.id;
            QMetaObject::invokeMethod(this, [self, pid]() {
                if (self) self->backfill_history(pid, "1y");
            }, Qt::QueuedConnection);
        }
        // Fallback: derive volatility from cross-sectional day changes only
        double sum = 0, sum_sq = 0;
        int n = 0;
        for (const auto& h : summary.holdings) {
            if (std::abs(h.day_change_percent) > 0.0001) {
                sum += h.day_change_percent;
                sum_sq += h.day_change_percent * h.day_change_percent;
                ++n;
            }
        }
        if (n >= 2) {
            const double mean = sum / n;
            const double var = (sum_sq / n) - (mean * mean);
            const double daily_vol = std::sqrt(std::max(var, 0.0));
            const double ann_vol = daily_vol * std::sqrt(252.0);
            metrics.volatility = ann_vol * 100.0; // store as %
            const double rf_daily = rf_rate_ / 252.0;
            if (daily_vol > 1e-6)
                metrics.sharpe = ((mean / 100.0 - rf_daily) / (daily_vol / 100.0)) * std::sqrt(252.0);
            if (summary.total_market_value > 0)
                metrics.var_95 = summary.total_market_value * std::abs(mean / 100.0 - 1.645 * daily_vol / 100.0);
            const double vol_score = std::min(ann_vol / 40.0, 1.0) * 50.0;
            const double conc_score = std::min(conc / 80.0, 1.0) * 50.0;
            metrics.risk_score = vol_score + conc_score;
        }
        emit metrics_computed(metrics);
        return;
    }

    // ── Build daily return series from snapshots ──────────────────────────────
    auto snaps = snap_r.value();
    // Sort ascending by date
    std::sort(snaps.begin(), snaps.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });

    QVector<double> port_returns; // daily log returns (%)
    port_returns.reserve(snaps.size() - 1);
    for (int i = 1; i < snaps.size(); ++i) {
        const double prev = snaps[i - 1].total_value;
        const double curr = snaps[i].total_value;
        if (prev > 1e-6)
            port_returns.append((curr - prev) / prev * 100.0);
    }

    if (port_returns.size() < 2) {
        emit metrics_computed(metrics);
        return;
    }

    const int n = port_returns.size();

    // ── Mean and std-dev ──────────────────────────────────────────────────────
    const double mean = std::accumulate(port_returns.begin(), port_returns.end(), 0.0) / n;
    double sum_sq = 0;
    for (const double r : port_returns)
        sum_sq += (r - mean) * (r - mean);
    const double daily_vol = std::sqrt(sum_sq / (n - 1)); // sample std-dev
    const double ann_vol = daily_vol * std::sqrt(252.0);
    metrics.volatility = ann_vol; // already in %

    // ── Sharpe ratio (annualised) ─────────────────────────────────────────────
    // rf_rate_ = live DGS10 annual decimal (e.g. 0.043); convert to daily %
    const double rf_daily = rf_rate_ / 252.0 * 100.0;
    if (daily_vol > 1e-6)
        metrics.sharpe = ((mean - rf_daily) / daily_vol) * std::sqrt(252.0);

    // ── Max drawdown ──────────────────────────────────────────────────────────
    double peak = snaps.first().total_value;
    double max_dd = 0.0;
    for (const auto& s : snaps) {
        if (s.total_value > peak)
            peak = s.total_value;
        if (peak > 1e-6) {
            const double dd = (s.total_value - peak) / peak * 100.0;
            if (dd < max_dd)
                max_dd = dd;
        }
    }
    metrics.max_drawdown = max_dd; // negative %

    // ── Beta vs SPY (OLS regression on aligned date windows) ─────────────────
    // Build SPY daily return series from cached closes, aligned to snapshot dates.
    if (spy_closes_cache_.size() >= 2 && spy_dates_cache_.size() == spy_closes_cache_.size()) {
        // Build a date→close map for O(1) lookup
        QHash<QString, double> spy_map;
        spy_map.reserve(spy_dates_cache_.size());
        for (int i = 0; i < spy_dates_cache_.size(); ++i)
            spy_map[spy_dates_cache_[i]] = spy_closes_cache_[i];

        // For each consecutive snapshot pair, find SPY return for the same day
        QVector<double> spy_aligned;
        QVector<double> port_aligned;
        spy_aligned.reserve(snaps.size() - 1);
        port_aligned.reserve(snaps.size() - 1);

        for (int i = 1; i < snaps.size(); ++i) {
            const QString date = snaps[i].snapshot_date;
            if (!spy_map.contains(date))
                continue;
            // Find previous available SPY close
            const QString prev_date = snaps[i - 1].snapshot_date;
            if (!spy_map.contains(prev_date))
                continue;

            const double spy_prev = spy_map[prev_date];
            const double spy_curr = spy_map[date];
            if (spy_prev < 1e-6)
                continue;

            const double spy_ret = (spy_curr - spy_prev) / spy_prev * 100.0;
            const double pv = snaps[i - 1].total_value;
            const double cv = snaps[i].total_value;
            if (pv < 1e-6)
                continue;
            const double port_ret = (cv - pv) / pv * 100.0;
            spy_aligned.append(spy_ret);
            port_aligned.append(port_ret);
        }

        const int m = spy_aligned.size();
        if (m >= 5) {
            // OLS: beta = cov(port, spy) / var(spy)
            const double spy_mean = std::accumulate(spy_aligned.begin(), spy_aligned.end(), 0.0) / m;
            const double port_mean = std::accumulate(port_aligned.begin(), port_aligned.end(), 0.0) / m;
            double cov = 0.0, var_spy = 0.0;
            for (int i = 0; i < m; ++i) {
                cov += (port_aligned[i] - port_mean) * (spy_aligned[i] - spy_mean);
                var_spy += (spy_aligned[i] - spy_mean) * (spy_aligned[i] - spy_mean);
            }
            if (var_spy > 1e-10)
                metrics.beta = cov / var_spy;
        }
    }

    // ── VaR 95% and CVaR 95% (historical simulation) ─────────────────────────
    // Sort returns ascending; VaR = worst 5th percentile; CVaR = mean of tail.
    if (summary.total_market_value > 0 && !port_returns.isEmpty()) {
        QVector<double> sorted_rets = port_returns;
        std::sort(sorted_rets.begin(), sorted_rets.end());
        const int tail_count = std::max(1, static_cast<int>(std::floor(sorted_rets.size() * 0.05)));
        // VaR: loss at 95th percentile (positive value = amount at risk)
        const double var_pct = -sorted_rets[tail_count - 1]; // worst 5th pct return (%)
        metrics.var_95 = summary.total_market_value * std::max(var_pct, 0.0) / 100.0;
        // CVaR: expected loss beyond VaR (average of worst tail_count returns)
        double tail_sum = 0.0;
        for (int i = 0; i < tail_count; ++i)
            tail_sum += sorted_rets[i];
        const double cvar_pct = -(tail_sum / tail_count);
        metrics.cvar_95 = summary.total_market_value * std::max(cvar_pct, 0.0) / 100.0;
    }

    // ── Composite risk score (0-100) ─────────────────────────────────────────
    {
        const double vol_score = std::min(ann_vol / 40.0, 1.0) * 30.0;
        const double conc_score = std::min(conc / 80.0, 1.0) * 25.0;
        const double dd_score = std::min(std::abs(max_dd) / 50.0, 1.0) * 25.0;
        const double beta_val = metrics.beta.value_or(1.0);
        const double beta_score = std::min(std::abs(beta_val) / 2.0, 1.0) * 20.0;
        metrics.risk_score = vol_score + conc_score + dd_score + beta_score;
    }

    emit metrics_computed(metrics);
}

// ── Import / Export ──────────────────────────────────────────────────────────


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

    // Pull the full transaction history (not just the latest 50 — we need
    // every BUY/SELL since the portfolio was created so the replay walks the
    // whole timeline).
    auto txns_r = repo.get_transactions(portfolio_id, /*limit=*/100000);
    if (txns_r.is_err() || txns_r.value().isEmpty()) {
        // No transactions → nothing to replay. Emit zero so callers don't wait.
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    const auto txns = txns_r.value();

    // Serialize transactions to a temp JSON file. The Python side (action
    // `portfolio_nav_history_replay`) reads it and walks the transactions in
    // order to reconstruct daily position vectors and per-day cost basis.
    // A file is used rather than CLI args because a portfolio with 500
    // transactions × ~40 chars each would exceed Windows' 8191-char arg limit.
    QJsonArray tx_arr;
    for (const auto& t : txns) {
        QJsonObject obj;
        obj["date"] = t.transaction_date.left(10); // YYYY-MM-DD
        obj["symbol"] = t.symbol;
        obj["type"] = t.transaction_type;
        obj["quantity"] = t.quantity;
        obj["price"] = t.price;
        tx_arr.append(obj);
    }

    const QString tmp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmp_dir);
    const QString tmp_path = QDir(tmp_dir).filePath(
        QString("fincept_replay_%1_%2.json")
            .arg(portfolio_id.left(16))
            .arg(QDateTime::currentMSecsSinceEpoch()));

    QFile f(tmp_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_WARN("PortfolioSvc", "backfill_history: cannot open temp file " + tmp_path);
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    f.write(QJsonDocument(tx_arr).toJson(QJsonDocument::Compact));
    f.close();

    QStringList args;
    args << "portfolio_nav_history_replay" << tmp_path;

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run("yfinance_data.py", args,
                                         [self, portfolio_id, tmp_path](python::PythonResult result) {
        // Always remove the temp file when we're done with it, regardless of
        // success — leaks add up if a user re-imports often.
        QFile::remove(tmp_path);

        if (!self)
            return;
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("PortfolioSvc",
                     QString("backfill_history failed for %1: %2").arg(portfolio_id, result.error.left(200)));
            emit self->history_backfilled(portfolio_id, 0);
            return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
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

        // Upsert each row. INSERT OR REPLACE keyed by (portfolio_id, snapshot_date)
        // so re-running backfill corrects existing rows in place. Per-day cost
        // basis comes from the replay (BUY adds, SELL reduces by WAC); falls
        // back to NAV if the Python side didn't return a costs array.
        auto& repo = PortfolioRepository::instance();
        int written = 0;
        for (int i = 0; i < dates.size(); ++i) {
            const QString d = dates[i].toString();
            const double nav = navs[i].toDouble();
            const double cost_basis = have_costs ? costs[i].toDouble() : 0.0;
            const double pnl = nav - cost_basis;
            const double pnl_pct = cost_basis > 0 ? (pnl / cost_basis) * 100.0 : 0.0;
            auto wr = repo.save_snapshot(portfolio_id, nav, cost_basis, pnl, pnl_pct, d);
            if (wr.is_ok())
                ++written;
        }

        LOG_INFO("PortfolioSvc",
                 QString("Replayed %1 historical snapshots for %2").arg(written).arg(portfolio_id));
        self->invalidate_cache(portfolio_id);
        emit self->history_backfilled(portfolio_id, written);
    });
}

// ── Cache control ────────────────────────────────────────────────────────────

} // namespace fincept::services
