// src/services/backtesting/BacktestResultModel.cpp
#include "services/backtesting/BacktestResultModel.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace fincept::services::backtest {

namespace {

QString bt_rm_tr(const char* text) {
    return QCoreApplication::translate("fincept::services::backtest::BtRun", text);
}

double bt_rm_num(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        return std::isfinite(d) ? d : kBtNaN;
    }
    return kBtNaN;
}

int bt_rm_int(const QJsonValue& v, int fallback = -1) {
    const double d = bt_rm_num(v);
    return std::isfinite(d) ? static_cast<int>(std::llround(d)) : fallback;
}

QString bt_rm_str(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble());
    return {};
}

QStringList bt_rm_strings(const QJsonValue& v) {
    QStringList out;
    if (v.isArray()) {
        for (const QJsonValue& x : v.toArray())
            if (x.isString() && !x.toString().trimmed().isEmpty())
                out << x.toString().trimmed();
    } else if (v.isString() && !v.toString().trimmed().isEmpty()) {
        out << v.toString().trimmed();
    }
    return out;
}

QVector<double> bt_rm_doubles(const QJsonValue& v) {
    QVector<double> out;
    const QJsonArray a = v.toArray();
    out.reserve(a.size());
    for (const QJsonValue& x : a)
        out.append(bt_rm_num(x));
    return out;
}

void bt_rm_flatten(const QJsonObject& block, const QString& prefix, QHash<QString, double>& out) {
    for (auto it = block.constBegin(); it != block.constEnd(); ++it) {
        const QJsonValue v = it.value();
        if (v.isDouble()) {
            const double d = v.toDouble();
            if (std::isfinite(d))
                out.insert(prefix + QLatin1Char('.') + it.key(), d);
        } else if (v.isBool()) {
            out.insert(prefix + QLatin1Char('.') + it.key(), v.toBool() ? 1.0 : 0.0);
        }
    }
}

void bt_rm_perf_map(const QJsonObject& o, QHash<QString, double>& out) {
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        const double d = bt_rm_num(it.value());
        if (std::isfinite(d))
            out.insert(it.key(), d);
    }
}

/// days from 1970-01-01 of a proleptic Gregorian date (H. Hinnant's days_from_civil).
qint64 bt_rm_days_from_civil(int y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<qint64>(era) * 146097 + doe - 719468;
}

BtSeries bt_rm_dated(const QJsonValue& v, const char* value_key = "value") {
    BtSeries s;
    const QJsonArray a = v.toArray();
    s.t.reserve(a.size());
    s.v.reserve(a.size());
    const QString vk = QLatin1String(value_key);
    for (const QJsonValue& row : a) {
        const QJsonObject o = row.toObject();
        const double t = bt_time_ms(o.value(QStringLiteral("date")).toString());
        if (!std::isfinite(t))
            continue;
        s.t.append(t);
        s.v.append(bt_rm_num(o.value(vk)));
    }
    return s;
}

QVector<BtMonthRow> bt_rm_monthly(const QJsonValue& v) {
    QVector<BtMonthRow> out;
    for (const QJsonValue& row : v.toArray()) {
        const QJsonObject o = row.toObject();
        BtMonthRow m;
        m.year = bt_rm_int(o.value(QStringLiteral("year")), 0);
        if (m.year <= 0)
            continue;
        const QJsonArray months = o.value(QStringLiteral("months")).toArray();
        for (int i = 0; i < 12 && i < months.size(); ++i)
            m.months[i] = bt_rm_num(months.at(i));
        m.year_total = bt_rm_num(o.value(QStringLiteral("yearTotal")));
        out.append(m);
    }
    return out;
}

double bt_rm_compound(const BtMonthRow& r) {
    double g = 1.0;
    bool any = false;
    for (double m : r.months) {
        if (std::isfinite(m)) {
            g *= 1.0 + m;
            any = true;
        }
    }
    return any ? g - 1.0 : kBtNaN;
}

QVector<BtDrawdown> bt_rm_compute_drawdowns(const BtRun& r, int max_rows) {
    QVector<BtDrawdown> eps;
    const int n = static_cast<int>(r.equity.size());
    if (n < 2)
        return eps;
    int peak_i = 0;
    double peak = r.equity[0];
    int i = 1;
    while (i < n) {
        const double e = r.equity[i];
        if (!std::isfinite(e)) {
            ++i;
            continue;
        }
        if (e >= peak) {
            peak = e;
            peak_i = i;
            ++i;
            continue;
        }
        // In a drawdown from peak_i: find its trough and recovery.
        int trough_i = i;
        double trough = e;
        int j = i;
        int rec = -1;
        while (j < n) {
            const double x = r.equity[j];
            if (std::isfinite(x)) {
                if (x >= peak) {
                    rec = j;
                    break;
                }
                if (x < trough) {
                    trough = x;
                    trough_i = j;
                }
            }
            ++j;
        }
        BtDrawdown d;
        d.depth = peak > 0 ? (peak - trough) / peak : kBtNaN;
        d.peak_t = r.t.value(peak_i, kBtNaN);
        d.trough_t = r.t.value(trough_i, kBtNaN);
        d.peak_date = r.dates.value(peak_i);
        d.trough_date = r.dates.value(trough_i);
        d.to_trough_bars = trough_i - peak_i;
        d.peak_equity = peak;
        d.trough_equity = trough;
        if (rec >= 0) {
            d.recovery_t = r.t.value(rec, kBtNaN);
            d.recovery_date = r.dates.value(rec);
            d.recovery_bars = rec - trough_i;
            d.duration_bars = rec - peak_i;
        } else {
            d.duration_bars = (n - 1) - peak_i;
        }
        const double end_t = rec >= 0 ? d.recovery_t : r.t.value(n - 1, kBtNaN);
        if (std::isfinite(end_t) && std::isfinite(d.peak_t))
            d.duration_days = static_cast<int>(std::llround((end_t - d.peak_t) / 86400000.0));
        if (std::isfinite(d.depth) && d.depth > 1e-9)
            eps.append(d);
        if (rec < 0)
            break;
        peak = r.equity[rec];
        peak_i = rec;
        i = rec + 1;
    }
    std::sort(eps.begin(), eps.end(), [](const BtDrawdown& a, const BtDrawdown& b) { return a.depth > b.depth; });
    if (eps.size() > max_rows)
        eps.resize(max_rows);
    for (int k = 0; k < eps.size(); ++k)
        eps[k].rank = k + 1;
    return eps;
}

BtCostModel bt_rm_cost_model(const QJsonObject& m, const QJsonObject& itemised) {
    BtCostModel c;
    c.present = !m.isEmpty();
    c.preset = m.value(QStringLiteral("preset")).toString();
    c.source = m.value(QStringLiteral("source")).toString();
    c.commission_type = m.value(QStringLiteral("commissionType")).toString();
    c.commission_value = bt_rm_num(m.value(QStringLiteral("commissionValue")));
    c.min_commission = bt_rm_num(m.value(QStringLiteral("minCommission")));
    c.slippage_type = m.value(QStringLiteral("slippageType")).toString();
    c.slippage_value = bt_rm_num(m.value(QStringLiteral("slippageValue")));
    c.spread_bps = bt_rm_num(m.value(QStringLiteral("spreadBps")));
    c.borrow_rate = bt_rm_num(m.value(QStringLiteral("borrowRateAnnual")));
    c.financing_rate = bt_rm_num(m.value(QStringLiteral("financingRateAnnual")));
    c.day_count = m.value(QStringLiteral("dayCount")).toString();
    if (!itemised.isEmpty()) {
        c.itemised_commission = itemised.value(QStringLiteral("commission")).toBool(true);
        c.itemised_slippage = itemised.value(QStringLiteral("slippage")).toBool(true);
        c.itemised_borrow = itemised.value(QStringLiteral("borrow")).toBool(true);
    }
    return c;
}

} // namespace

// ── Time ────────────────────────────────────────────────────────────────────

double bt_time_ms(const QString& s) {
    const QStringView v = QStringView(s).trimmed();
    if (v.size() < 10 || v[4] != QLatin1Char('-') || v[7] != QLatin1Char('-'))
        return kBtNaN;
    auto num = [&v](int from, int len, int* out) {
        int x = 0;
        for (int i = from; i < from + len; ++i) {
            const QChar c = v[i];
            if (!c.isDigit())
                return false;
            x = x * 10 + c.digitValue();
        }
        *out = x;
        return true;
    };
    int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
    if (!num(0, 4, &y) || !num(5, 2, &m) || !num(8, 2, &d) || m < 1 || m > 12 || d < 1 || d > 31)
        return kBtNaN;
    if (v.size() >= 19 && (v[10] == QLatin1Char(' ') || v[10] == QLatin1Char('T')) && v[13] == QLatin1Char(':') &&
        v[16] == QLatin1Char(':')) {
        if (!num(11, 2, &hh) || !num(14, 2, &mm) || !num(17, 2, &ss) || hh > 23 || mm > 59 || ss > 60)
            return kBtNaN;
    }
    const qint64 days = bt_rm_days_from_civil(y, m, d);
    return static_cast<double>(days * 86400000LL + (hh * 3600LL + mm * 60LL + ss) * 1000LL);
}

bool bt_is_intraday(const QString& s) {
    const QString t = s.trimmed();
    return t.size() >= 19 && !t.mid(11, 8).startsWith(QLatin1String("00:00:00"));
}

int bt_warning_rank(const QString& severity) {
    if (severity == QLatin1String("error"))
        return 0;
    if (severity == QLatin1String("warn") || severity == QLatin1String("warning"))
        return 1;
    return 2;
}

// ── Shared readers ──────────────────────────────────────────────────────────

QVector<BtWarning> parse_warnings(const QJsonValue& v) {
    QVector<BtWarning> out;
    for (const QJsonValue& x : v.toArray()) {
        BtWarning w;
        if (x.isString()) {
            w.code = QStringLiteral("NOTE");
            w.severity = QStringLiteral("info");
            w.message = x.toString();
        } else {
            const QJsonObject o = x.toObject();
            w.code = o.value(QStringLiteral("code")).toString();
            w.severity = o.value(QStringLiteral("severity")).toString(QStringLiteral("info"));
            w.message = o.value(QStringLiteral("message")).toString();
            w.symbol = o.value(QStringLiteral("symbol")).toString();
        }
        if (!w.message.trimmed().isEmpty())
            out.append(w);
    }
    std::stable_sort(out.begin(), out.end(), [](const BtWarning& a, const BtWarning& b) {
        return bt_warning_rank(a.severity) < bt_warning_rank(b.severity);
    });
    return out;
}

BtProvenance parse_provenance(const QJsonObject& p) {
    BtProvenance v;
    v.present = !p.isEmpty();
    v.engine = p.value(QStringLiteral("engine")).toString();
    v.engine_implementation = p.value(QStringLiteral("engineImplementation")).toString();
    v.engine_version = bt_rm_str(p.value(QStringLiteral("engineVersion")));
    v.library = p.value(QStringLiteral("library")).toString();
    v.library_version = bt_rm_str(p.value(QStringLiteral("libraryVersion")));
    v.python_version = p.value(QStringLiteral("pythonVersion")).toString();
    v.run_id = p.value(QStringLiteral("runId")).toString();
    v.started_at = p.value(QStringLiteral("startedAt")).toString();
    const double dur = bt_rm_num(p.value(QStringLiteral("durationMs")));
    v.duration_ms = std::isfinite(dur) ? static_cast<qint64>(dur) : -1;
    v.data_source = p.value(QStringLiteral("dataSource")).toString();
    v.adjustment = p.value(QStringLiteral("adjustment")).toString();
    v.interval = p.value(QStringLiteral("interval")).toString();
    v.bars = bt_rm_int(p.value(QStringLiteral("bars")));
    v.first_bar = p.value(QStringLiteral("firstBar")).toString();
    v.last_bar = p.value(QStringLiteral("lastBar")).toString();
    v.requested_start = p.value(QStringLiteral("requestedStart")).toString();
    v.requested_end = p.value(QStringLiteral("requestedEnd")).toString();
    v.warmup_bars = bt_rm_int(p.value(QStringLiteral("warmupBars")));
    const QJsonObject st = p.value(QStringLiteral("strategy")).toObject();
    v.strategy_type = st.value(QStringLiteral("type")).toString();
    v.strategy_name = st.value(QStringLiteral("name")).toString();
    v.parameters = p.value(QStringLiteral("parameters")).toObject();
    v.currency = p.value(QStringLiteral("currency")).toString();
    v.seed = bt_rm_num(p.value(QStringLiteral("seed")));
    for (const QJsonValue& x : p.value(QStringLiteral("symbols")).toArray()) {
        const QJsonObject o = x.toObject();
        BtProvSymbol s;
        s.symbol = o.value(QStringLiteral("symbol")).toString();
        s.currency = o.value(QStringLiteral("currency")).toString();
        s.source = o.value(QStringLiteral("source")).toString();
        s.first_bar = o.value(QStringLiteral("firstBar")).toString();
        s.last_bar = o.value(QStringLiteral("lastBar")).toString();
        s.requested_start = o.value(QStringLiteral("requestedStart")).toString();
        s.cache_as_of = o.value(QStringLiteral("cacheAsOf")).toString();
        s.bars = bt_rm_int(o.value(QStringLiteral("bars")));
        s.missing_bars = bt_rm_int(o.value(QStringLiteral("missingBars")));
        if (!s.symbol.isEmpty())
            v.symbols.append(s);
    }
    if (v.currency.isEmpty() && !v.symbols.isEmpty())
        v.currency = v.symbols.first().currency;
    return v;
}

BtAssumptions parse_assumptions(const QJsonObject& a) {
    BtAssumptions v;
    v.engine = a.value(QStringLiteral("engine")).toString();
    v.execution = a.value(QStringLiteral("execution")).toString();
    v.data_source = a.value(QStringLiteral("dataSource")).toString();
    v.costs = bt_rm_str(a.value(QStringLiteral("costs")));
    v.position_sizing = bt_rm_str(a.value(QStringLiteral("positionSizing")));
    v.annualization = bt_rm_str(a.value(QStringLiteral("annualization")));
    v.survivorship = bt_rm_str(a.value(QStringLiteral("survivorship")));
    v.warm_up = bt_rm_str(a.value(QStringLiteral("warmUp")));
    v.risk_free_rate = bt_rm_num(a.value(QStringLiteral("riskFreeRate")));
    v.risk_free_source = bt_rm_str(a.value(QStringLiteral("riskFreeSource")));
    v.risk_free_description = bt_rm_str(a.value(QStringLiteral("riskFreeDescription")));
    const QJsonObject fm = a.value(QStringLiteral("fillModel")).toObject();
    v.fill_signal_at = fm.value(QStringLiteral("signalAt")).toString();
    v.fill_at = fm.value(QStringLiteral("fillAt")).toString();
    v.fill_description = fm.value(QStringLiteral("description")).toString();
    v.not_modelled = bt_rm_strings(a.value(QStringLiteral("notModelled")));
    v.ignored_settings = bt_rm_strings(a.value(QStringLiteral("ignoredSettings")));
    v.notes = bt_rm_strings(a.value(QStringLiteral("notes")));
    return v;
}

void bt_thin(const QVector<double>& x, const QVector<double>& y, int max_points, QVector<double>* out_x,
             QVector<double>* out_y) {
    const int n = static_cast<int>(std::min(x.size(), y.size()));
    if (out_x)
        out_x->clear();
    if (out_y)
        out_y->clear();
    if (n <= 0)
        return;
    if (max_points < 2 || n <= max_points) {
        if (out_x)
            *out_x = x.mid(0, n);
        if (out_y)
            *out_y = y.mid(0, n);
        return;
    }
    const int step = (n + max_points - 1) / max_points;
    for (int i = 0; i < n; i += step) {
        if (out_x)
            out_x->append(x[i]);
        if (out_y)
            out_y->append(y[i]);
    }
    if ((n - 1) % step != 0) {
        if (out_x)
            out_x->append(x[n - 1]);
        if (out_y)
            out_y->append(y[n - 1]);
    }
}

// ── BtTrade / BtRun ─────────────────────────────────────────────────────────

double BtTrade::costs() const {
    double sum = 0;
    bool any = false;
    for (double c : {commission, slippage, borrow}) {
        if (std::isfinite(c)) {
            sum += c;
            any = true;
        }
    }
    return any ? sum : kBtNaN;
}

double BtRun::metric(const QString& path) const {
    return metrics.value(path, kBtNaN);
}

bool BtRun::has_metric(const QString& path) const {
    return metrics.contains(path);
}

QString BtRun::why_blank(const QString& path) const {
    if (has_metric(path))
        return {};
    const QString leaf = path.section(QLatin1Char('.'), -1);
    const QString r = undefined.value(leaf);
    if (!r.isEmpty())
        return r;
    const QString eng = engine_implementation.isEmpty() ? engine : engine_implementation;
    return eng.isEmpty() ? bt_rm_tr("Not reported by this engine")
                         : bt_rm_tr("Not reported by this engine (%1)").arg(eng);
}

QStringList BtRun::symbols() const {
    QStringList out;
    for (const BtProvSymbol& s : provenance.symbols)
        if (!out.contains(s.symbol))
            out << s.symbol;
    if (out.isEmpty()) {
        for (const QJsonValue& v : payload.value(QStringLiteral("symbols")).toArray())
            if (!out.contains(v.toString()) && !v.toString().isEmpty())
                out << v.toString();
    }
    for (const BtTrade& t : trades)
        if (!t.symbol.isEmpty() && !out.contains(t.symbol))
            out << t.symbol;
    return out;
}

double BtRun::initial_capital() const {
    const double c = metric(QStringLiteral("statistics.initialCapital"));
    if (std::isfinite(c))
        return c;
    return equity.isEmpty() ? kBtNaN : equity.first();
}

double BtRun::final_equity() const {
    const double c = metric(QStringLiteral("statistics.finalCapital"));
    if (std::isfinite(c))
        return c;
    return equity.isEmpty() ? kBtNaN : equity.last();
}

double BtRun::years() const {
    if (t.size() < 2)
        return kBtNaN;
    return (t.last() - t.first()) / (365.25 * 86400000.0);
}

int BtRun::closed_trades() const {
    const double c = metric(QStringLiteral("tradeStats.closedTrades"));
    if (std::isfinite(c))
        return static_cast<int>(c);
    return static_cast<int>(trades.size()) - open_trades;
}

int BtRun::serious_warning_count() const {
    int n = 0;
    for (const BtWarning& w : warnings)
        n += bt_warning_rank(w.severity) <= 1 ? 1 : 0;
    return n;
}

BtRun parse_backtest(const QJsonObject& data) {
    BtRun r;
    r.payload = data;
    r.schema_version = bt_rm_int(data.value(QStringLiteral("schemaVersion")), 1);
    r.id = bt_rm_str(data.value(QStringLiteral("id")));
    r.status = data.value(QStringLiteral("status")).toString();
    r.synthetic = data.value(QStringLiteral("usingSyntheticData")).toBool(false);
    r.periods_per_year = bt_rm_num(data.value(QStringLiteral("periodsPerYear")));
    r.provenance = parse_provenance(data.value(QStringLiteral("provenance")).toObject());
    r.assumptions = parse_assumptions(data.value(QStringLiteral("assumptions")).toObject());
    r.engine = data.value(QStringLiteral("engine")).toString(r.provenance.engine);
    r.engine_implementation = data.value(QStringLiteral("engineImplementation")).toString(r.provenance.engine_implementation);
    r.engine_version = bt_rm_str(data.value(QStringLiteral("engineVersion")));
    if (r.engine_version.isEmpty())
        r.engine_version = r.provenance.engine_version;
    r.currency = r.provenance.currency;

    // ── metrics ──
    static const char* const kBlocks[] = {"performance",   "statistics",      "tradeStats",   "advancedMetrics",
                                          "periodStats",   "exposure",        "costs",        "benchmark",
                                          "drawdownAnalysis", "returnsAnalysis", "riskMetrics", "extendedStats"};
    for (const char* b : kBlocks)
        bt_rm_flatten(data.value(QLatin1String(b)).toObject(), QLatin1String(b), r.metrics);
    auto has = [&r](const char* k) { return r.metrics.contains(QLatin1String(k)); };
    auto fill = [&r, &has](const char* to, std::initializer_list<const char*> from) {
        if (has(to))
            return;
        for (const char* f : from) {
            if (has(f)) {
                r.metrics.insert(QLatin1String(to), r.metrics.value(QLatin1String(f)));
                return;
            }
        }
    };
    fill("performance.cagr", {"performance.annualizedReturn"});
    fill("performance.annualizedReturn", {"performance.cagr"});
    fill("performance.alpha", {"advancedMetrics.benchmarkAlpha"});
    fill("performance.beta", {"advancedMetrics.benchmarkBeta"});
    fill("performance.informationRatio", {"advancedMetrics.informationRatio"});
    fill("performance.downsideDeviation", {"riskMetrics.downsideDeviation"});
    fill("exposure.timeInMarket", {"performance.exposureTime", "extendedStats.exposureTime"});
    fill("periodStats.bestDay", {"statistics.bestDay"});
    fill("periodStats.worstDay", {"statistics.worstDay"});
    for (const char* k : {"bestWeek", "worstWeek", "bestMonth", "worstMonth", "longestWinStreak", "longestLossStreak"}) {
        const QByteArray to = QByteArray("periodStats.") + k;
        const QByteArray from = QByteArray("returnsAnalysis.") + k;
        fill(to.constData(), {from.constData()});
    }
    fill("tradeStats.closedTrades", {"performance.totalTrades"});
    fill("tradeStats.winningTrades", {"performance.winningTrades"});
    fill("tradeStats.losingTrades", {"performance.losingTrades"});
    fill("tradeStats.winRate", {"performance.winRate"});
    fill("tradeStats.profitFactor", {"performance.profitFactor"});
    fill("costs.totalCommission", {"statistics.totalFees", "tradeStats.totalCommission"});
    fill("costs.totalSlippage", {"statistics.totalSlippage"});

    // ── equity ──
    const QJsonArray eq = data.value(QStringLiteral("equity")).toArray();
    const int n = static_cast<int>(eq.size());
    r.t.reserve(n);
    r.equity.reserve(n);
    r.dates.reserve(n);
    QVector<double> dd, rets, bench, expo, net, cash;
    dd.reserve(n);
    rets.reserve(n);
    bench.reserve(n);
    bool any_bench = false, any_dd = false, any_ret = false, any_expo = false, any_net = false, any_cash = false;
    for (const QJsonValue& row : eq) {
        const QJsonObject o = row.toObject();
        const QString date = o.value(QStringLiteral("date")).toString();
        const double t = bt_time_ms(date);
        if (!std::isfinite(t))
            continue;
        r.t.append(t);
        r.dates.append(date);
        r.equity.append(bt_rm_num(o.value(QStringLiteral("equity"))));
        const QJsonValue dv = o.value(QStringLiteral("drawdown"));
        any_dd = any_dd || dv.isDouble();
        dd.append(bt_rm_num(dv));
        const QJsonValue rv = o.value(QStringLiteral("returns"));
        any_ret = any_ret || rv.isDouble();
        rets.append(bt_rm_num(rv));
        const double b = bt_rm_num(o.value(QStringLiteral("benchmark")));
        any_bench = any_bench || std::isfinite(b);
        bench.append(b);
        const QJsonValue ev = o.value(QStringLiteral("exposure"));
        if (ev.isDouble() || o.contains(QStringLiteral("exposure"))) {
            any_expo = any_expo || ev.isDouble();
        }
        expo.append(bt_rm_num(ev));
        const QJsonValue nv = o.value(QStringLiteral("netExposure"));
        any_net = any_net || nv.isDouble();
        net.append(bt_rm_num(nv));
        const QJsonValue cv = o.value(QStringLiteral("cash"));
        any_cash = any_cash || cv.isDouble();
        cash.append(bt_rm_num(cv));
    }
    const int m = static_cast<int>(r.t.size());
    if (m > 0)
        r.intraday = bt_is_intraday(r.dates.first()) || bt_is_intraday(r.dates.last());
    if (any_dd) {
        r.drawdown = dd;
    } else if (m > 0) {
        r.drawdown.resize(m);
        double peak = r.equity.value(0, kBtNaN);
        for (int i = 0; i < m; ++i) {
            const double e = r.equity[i];
            if (std::isfinite(e) && (!std::isfinite(peak) || e > peak))
                peak = e;
            r.drawdown[i] = (std::isfinite(e) && peak > 0) ? e / peak - 1.0 : kBtNaN;
        }
    }
    if (any_ret) {
        r.returns = rets;
    } else if (m > 0) {
        r.returns.resize(m);
        r.returns[0] = 0.0;
        for (int i = 1; i < m; ++i) {
            const double a = r.equity[i - 1], b = r.equity[i];
            r.returns[i] = (std::isfinite(a) && std::isfinite(b) && a != 0) ? b / a - 1.0 : kBtNaN;
        }
    }
    if (any_bench)
        r.benchmark = bench;
    if (any_expo)
        r.exposure = expo;
    if (any_net)
        r.net_exposure = net;
    if (any_cash)
        r.cash = cash;

    const QJsonObject bench_obj = data.value(QStringLiteral("benchmark")).toObject();
    r.benchmark_symbol = bench_obj.value(QStringLiteral("symbol")).toString();

    // ── trades ──
    const QJsonArray tr = data.value(QStringLiteral("trades")).toArray();
    r.trades.reserve(tr.size());
    int auto_id = 0;
    for (const QJsonValue& row : tr) {
        const QJsonObject o = row.toObject();
        BtTrade t;
        ++auto_id;
        t.id = bt_rm_str(o.value(QStringLiteral("id")));
        if (t.id.isEmpty())
            t.id = QStringLiteral("t%1").arg(auto_id);
        t.symbol = o.value(QStringLiteral("symbol")).toString();
        t.side = o.value(QStringLiteral("side")).toString(QStringLiteral("long")).toLower();
        t.entry_date = o.value(QStringLiteral("entryDate")).toString();
        t.exit_date = o.value(QStringLiteral("exitDate")).toString();
        t.entry_t = bt_time_ms(t.entry_date);
        t.exit_t = bt_time_ms(t.exit_date);
        t.entry_price = bt_rm_num(o.value(QStringLiteral("entryPrice")));
        t.exit_price = bt_rm_num(o.value(QStringLiteral("exitPrice")));
        t.mark_price = bt_rm_num(o.value(QStringLiteral("markPrice")));
        t.quantity = bt_rm_num(o.value(QStringLiteral("quantity")));
        t.pnl = bt_rm_num(o.value(QStringLiteral("pnl")));
        t.pnl_pct = bt_rm_num(o.value(QStringLiteral("pnlPercent")));
        t.gross_pnl = bt_rm_num(o.value(QStringLiteral("grossPnl")));
        t.commission = bt_rm_num(o.value(QStringLiteral("commission")));
        t.slippage = bt_rm_num(o.value(QStringLiteral("slippageCost")));
        t.borrow = bt_rm_num(o.value(QStringLiteral("borrowCost")));
        t.holding_bars = bt_rm_int(o.value(QStringLiteral("holdingPeriod")));
        t.holding_days = bt_rm_int(o.value(QStringLiteral("holdingDays")));
        t.mae = bt_rm_num(o.value(QStringLiteral("mae")));
        t.mfe = bt_rm_num(o.value(QStringLiteral("mfe")));
        t.exit_reason = o.value(QStringLiteral("exitReason")).toString();
        t.entry_bar = bt_rm_int(o.value(QStringLiteral("entryBar")));
        t.exit_bar = bt_rm_int(o.value(QStringLiteral("exitBar")));
        t.open = t.exit_date.isEmpty() || t.exit_reason == QLatin1String("open") ||
                 o.value(QStringLiteral("unrealized")).toBool(false);
        if (t.open) {
            ++r.open_trades;
            if (t.exit_reason.isEmpty())
                t.exit_reason = QStringLiteral("open");
        }
        r.trades.append(t);
    }

    // time in market, when no block reported it
    if (!has("exposure.timeInMarket")) {
        if (!r.exposure.isEmpty()) {
            int inside = 0, counted = 0;
            for (double x : r.exposure) {
                if (!std::isfinite(x))
                    continue;
                ++counted;
                inside += std::fabs(x) > 1e-9 ? 1 : 0;
            }
            if (counted > 0)
                r.metrics.insert(QStringLiteral("exposure.timeInMarket"), static_cast<double>(inside) / counted);
        } else if (m > 1 && !r.trades.isEmpty()) {
            qint64 bars = 0;
            bool all_known = true;
            for (const BtTrade& t : r.trades) {
                if (t.holding_bars < 0) {
                    all_known = false;
                    break;
                }
                bars += t.holding_bars;
            }
            if (all_known)
                r.metrics.insert(QStringLiteral("exposure.timeInMarket"),
                                 std::min(1.0, static_cast<double>(bars) / static_cast<double>(m)));
        }
    }

    // ── drawdowns ──
    for (const QJsonValue& row : data.value(QStringLiteral("drawdowns")).toArray()) {
        const QJsonObject o = row.toObject();
        BtDrawdown d;
        d.rank = bt_rm_int(o.value(QStringLiteral("rank")), static_cast<int>(r.drawdowns.size()) + 1);
        d.depth = bt_rm_num(o.value(QStringLiteral("depth")));
        d.peak_date = o.value(QStringLiteral("peakDate")).toString();
        d.trough_date = o.value(QStringLiteral("troughDate")).toString();
        d.recovery_date = o.value(QStringLiteral("recoveryDate")).toString();
        d.peak_t = bt_time_ms(d.peak_date);
        d.trough_t = bt_time_ms(d.trough_date);
        d.recovery_t = bt_time_ms(d.recovery_date);
        d.to_trough_bars = bt_rm_int(o.value(QStringLiteral("toTroughBars")));
        d.recovery_bars = bt_rm_int(o.value(QStringLiteral("recoveryBars")));
        d.duration_bars = bt_rm_int(o.value(QStringLiteral("durationBars")));
        d.duration_days = bt_rm_int(o.value(QStringLiteral("durationDays")));
        d.peak_equity = bt_rm_num(o.value(QStringLiteral("peakEquity")));
        d.trough_equity = bt_rm_num(o.value(QStringLiteral("troughEquity")));
        if (std::isfinite(d.depth))
            r.drawdowns.append(d);
    }
    if (r.drawdowns.isEmpty() && !data.contains(QStringLiteral("drawdowns"))) {
        r.drawdowns = bt_rm_compute_drawdowns(r, 10);
        r.drawdowns_computed = !r.drawdowns.isEmpty();
    }

    // ── monthly / yearly ──
    r.monthly = bt_rm_monthly(data.value(QStringLiteral("monthlyReturns")));
    r.benchmark_monthly = bt_rm_monthly(data.value(QStringLiteral("benchmarkMonthlyReturns")));
    for (const QJsonValue& row : data.value(QStringLiteral("yearlyReturns")).toArray()) {
        const QJsonObject o = row.toObject();
        BtYear y;
        y.year = bt_rm_int(o.value(QStringLiteral("year")), 0);
        y.strategy = bt_rm_num(o.value(QStringLiteral("strategy")));
        y.benchmark = bt_rm_num(o.value(QStringLiteral("benchmark")));
        y.excess = bt_rm_num(o.value(QStringLiteral("excess")));
        if (y.year > 0)
            r.yearly.append(y);
    }
    if (r.yearly.isEmpty() && !r.monthly.isEmpty()) {
        QHash<int, double> bench_years;
        for (const BtMonthRow& b : r.benchmark_monthly)
            bench_years.insert(b.year, std::isfinite(b.year_total) ? b.year_total : bt_rm_compound(b));
        for (const BtMonthRow& row : r.monthly) {
            BtYear y;
            y.year = row.year;
            y.strategy = std::isfinite(row.year_total) ? row.year_total : bt_rm_compound(row);
            y.benchmark = bench_years.value(row.year, kBtNaN);
            y.excess = (std::isfinite(y.strategy) && std::isfinite(y.benchmark)) ? y.strategy - y.benchmark : kBtNaN;
            r.yearly.append(y);
        }
        r.yearly_computed = true;
    }

    // ── rolling ──
    const QJsonObject rm = data.value(QStringLiteral("rollingMetrics")).toObject();
    r.rolling_window = bt_rm_int(rm.value(QStringLiteral("window")), 0);
    r.rolling_sharpe = bt_rm_dated(rm.value(QStringLiteral("rollingSharpe")));
    r.rolling_volatility = bt_rm_dated(rm.value(QStringLiteral("rollingVolatility")));
    r.rolling_beta = bt_rm_dated(rm.value(QStringLiteral("rollingBeta")));
    r.rolling_drawdown = bt_rm_dated(rm.value(QStringLiteral("rollingDrawdown")));

    // ── attribution ──
    for (const QJsonValue& row : data.value(QStringLiteral("attribution")).toArray()) {
        const QJsonObject o = row.toObject();
        BtAttribution a;
        a.symbol = o.value(QStringLiteral("symbol")).toString();
        a.trades = bt_rm_int(o.value(QStringLiteral("trades")), 0);
        a.win_rate = bt_rm_num(o.value(QStringLiteral("winRate")));
        a.net_pnl = bt_rm_num(o.value(QStringLiteral("netPnl")));
        a.gross_pnl = bt_rm_num(o.value(QStringLiteral("grossPnl")));
        a.costs = bt_rm_num(o.value(QStringLiteral("costs")));
        a.contribution = bt_rm_num(o.value(QStringLiteral("contribution")));
        a.avg_hold_bars = bt_rm_num(o.value(QStringLiteral("avgHoldingBars")));
        a.time_in_market = bt_rm_num(o.value(QStringLiteral("timeInMarket")));
        if (!a.symbol.isEmpty())
            r.attribution.append(a);
    }
    if (r.attribution.isEmpty() && !r.trades.isEmpty()) {
        QHash<QString, int> idx;
        QHash<QString, int> wins, closed, hold_n;
        QHash<QString, double> hold_sum;
        const double cap = r.initial_capital();
        for (const BtTrade& t : r.trades) {
            const QString sym = t.symbol.isEmpty() ? QStringLiteral("?") : t.symbol;
            if (!idx.contains(sym)) {
                idx.insert(sym, static_cast<int>(r.attribution.size()));
                BtAttribution a;
                a.symbol = sym;
                a.net_pnl = 0;
                a.costs = 0;
                a.computed = true;
                r.attribution.append(a);
            }
            BtAttribution& a = r.attribution[idx.value(sym)];
            ++a.trades;
            if (std::isfinite(t.pnl))
                a.net_pnl += t.pnl;
            const double c = t.costs();
            if (std::isfinite(c))
                a.costs += c;
            if (!t.open) {
                closed[sym] += 1;
                if (std::isfinite(t.pnl) && t.pnl > 0)
                    wins[sym] += 1;
            }
            if (t.holding_bars >= 0) {
                hold_sum[sym] += t.holding_bars;
                hold_n[sym] += 1;
            }
        }
        for (BtAttribution& a : r.attribution) {
            const int c = closed.value(a.symbol);
            a.win_rate = c > 0 ? static_cast<double>(wins.value(a.symbol)) / c : kBtNaN;
            a.contribution = (std::isfinite(cap) && cap > 0) ? a.net_pnl / cap : kBtNaN;
            const int hn = hold_n.value(a.symbol);
            a.avg_hold_bars = hn > 0 ? hold_sum.value(a.symbol) / hn : kBtNaN;
        }
    }

    // ── positions ──
    for (const QJsonValue& row : data.value(QStringLiteral("positions")).toArray()) {
        const QJsonObject o = row.toObject();
        BtPosition p;
        p.symbol = o.value(QStringLiteral("symbol")).toString();
        p.quantity = bt_rm_num(o.value(QStringLiteral("quantity")));
        p.avg_price = bt_rm_num(o.value(QStringLiteral("avgPrice")));
        p.last_price = bt_rm_num(o.value(QStringLiteral("lastPrice")));
        p.market_value = bt_rm_num(o.value(QStringLiteral("marketValue")));
        p.weight = bt_rm_num(o.value(QStringLiteral("weight")));
        p.unrealized = bt_rm_num(o.value(QStringLiteral("unrealizedPnl")));
        if (!p.symbol.isEmpty())
            r.positions.append(p);
    }

    // ── costs ──
    const QJsonObject co = data.value(QStringLiteral("costs")).toObject();
    r.costs.present = !co.isEmpty();
    r.costs.total_commission = r.metric(QStringLiteral("costs.totalCommission"));
    r.costs.total_slippage = r.metric(QStringLiteral("costs.totalSlippage"));
    r.costs.total_borrow = r.metric(QStringLiteral("costs.totalBorrow"));
    r.costs.total_costs = r.metric(QStringLiteral("costs.totalCosts"));
    if (!std::isfinite(r.costs.total_costs)) {
        double sum = 0;
        bool any = false;
        for (double c : {r.costs.total_commission, r.costs.total_slippage, r.costs.total_borrow}) {
            if (std::isfinite(c)) {
                sum += c;
                any = true;
            }
        }
        if (any)
            r.costs.total_costs = sum;
    }
    r.costs.gross_pnl = r.metric(QStringLiteral("costs.grossPnl"));
    r.costs.net_pnl = r.metric(QStringLiteral("costs.netPnl"));
    if (!std::isfinite(r.costs.net_pnl)) {
        const double ic = r.initial_capital(), fe = r.final_equity();
        if (std::isfinite(ic) && std::isfinite(fe))
            r.costs.net_pnl = fe - ic;
    }
    if (!std::isfinite(r.costs.gross_pnl) && std::isfinite(r.costs.net_pnl) && std::isfinite(r.costs.total_costs))
        r.costs.gross_pnl = r.costs.net_pnl + r.costs.total_costs;
    r.costs.cost_drag = r.metric(QStringLiteral("costs.costDrag"));
    r.costs.traded_notional = r.metric(QStringLiteral("costs.tradedNotional"));
    r.costs.annual_turnover = r.metric(QStringLiteral("costs.annualTurnover"));
    r.costs.pct_of_gross_profit = r.metric(QStringLiteral("costs.costsPctOfGrossProfit"));
    r.costs.fills = bt_rm_int(co.value(QStringLiteral("fills")));
    for (const QJsonValue& row : co.value(QStringLiteral("byYear")).toArray()) {
        const QJsonObject o = row.toObject();
        BtCostYear y;
        y.year = bt_rm_int(o.value(QStringLiteral("year")), 0);
        y.commission = bt_rm_num(o.value(QStringLiteral("commission")));
        y.slippage = bt_rm_num(o.value(QStringLiteral("slippage")));
        y.notional = bt_rm_num(o.value(QStringLiteral("tradedNotional")));
        y.turnover = bt_rm_num(o.value(QStringLiteral("turnover")));
        if (y.year > 0)
            r.costs.by_year.append(y);
    }
    r.costs.model = bt_rm_cost_model(co.value(QStringLiteral("model")).toObject(),
                                     co.value(QStringLiteral("itemised")).toObject());

    // ── warnings, logs, disclosures ──
    r.warnings = parse_warnings(data.value(QStringLiteral("warnings")));
    r.logs = bt_rm_strings(data.value(QStringLiteral("logs")));
    auto has_code = [&r](const QString& code) {
        for (const BtWarning& w : r.warnings)
            if (w.code == code)
                return true;
        return false;
    };
    if (r.synthetic && !has_code(QStringLiteral("SYNTHETIC_DATA")))
        r.warnings.prepend({QStringLiteral("SYNTHETIC_DATA"), QStringLiteral("error"),
                            bt_rm_tr("SYNTHETIC DATA — prices were randomly generated because real market data could "
                                     "not be loaded. Every number has NO market meaning."),
                            QString()});
    if (r.schema_version < 2) {
        for (const QString& s : r.assumptions.ignored_settings)
            r.warnings.append({QStringLiteral("IGNORED_SETTING"), QStringLiteral("info"), s, QString()});
    }
    if (data.value(QStringLiteral("usesModelPricedDerivatives")).toBool(false))
        r.data_disclosures << bt_rm_tr("Option prices are MODEL-DERIVED (Black-Scholes / Black-76 on the real "
                                       "underlying bars), not historical option quotes.");
    if (data.value(QStringLiteral("usesDerivedFuturesContracts")).toBool(false))
        r.data_disclosures << bt_rm_tr("Individual futures contracts are DERIVED from the continuous front-month "
                                       "series (cost of carry), not historical contract prices.");
    if (data.value(QStringLiteral("usesUniverseSelection")).toBool(false))
        r.data_disclosures << (data.value(QStringLiteral("usesFundamentalSnapshot")).toBool(false)
                                   ? bt_rm_tr("Universe selection from a fixed pool of today's large US stocks / ETFs, "
                                              "with current fundamentals: survivorship and look-ahead bias.")
                                   : bt_rm_tr("Universe selection from a fixed pool of today's large US stocks / ETFs: "
                                              "survivorship bias."));
    if (data.value(QStringLiteral("usesCustomData")).toBool(false))
        r.data_disclosures << bt_rm_tr("The strategy read custom data files with its own reader.");
    for (const QString& s : bt_rm_strings(data.value(QStringLiteral("modelPricedDisclosures"))))
        if (!r.data_disclosures.contains(s))
            r.data_disclosures << s;

    // ── why a metric is blank ──
    const QJsonObject um = data.value(QStringLiteral("undefinedMetrics")).toObject();
    for (auto it = um.constBegin(); it != um.constEnd(); ++it) {
        const QString reason = it.value().isString() ? it.value().toString() : bt_rm_str(it.value());
        if (!reason.isEmpty())
            r.undefined.insert(it.key(), reason);
    }
    auto why = [&r](const char* key, const QString& reason) {
        if (!r.undefined.contains(QLatin1String(key)))
            r.undefined.insert(QLatin1String(key), reason);
    };
    if (!r.has_benchmark() && r.benchmark_symbol.isEmpty()) {
        const QString nb = bt_rm_tr("No benchmark in this run");
        for (const char* k : {"alpha", "beta", "informationRatio", "trackingError", "treynorRatio", "benchmarkAlpha",
                              "benchmarkBeta", "benchmarkCorrelation", "rSquared", "upCapture", "downCapture"})
            why(k, nb);
    }
    if (r.metric(QStringLiteral("tradeStats.profitFactorInfinite")) > 0.5)
        why("profitFactor", bt_rm_tr("No losing trade: the profit factor is unbounded (∞)"));
    if (r.closed_trades() == 0) {
        const QString nt = bt_rm_tr("No closed trades in this run");
        for (const char* k : {"winRate", "profitFactor", "avgWin", "avgLoss", "payoffRatio", "expectancy",
                              "expectancyPct", "avgHoldingBars", "largestWin", "largestLoss", "sqn", "kellyFraction"})
            why(k, nt);
    }
    if (r.metric(QStringLiteral("tradeStats.losingTrades")) == 0.0) {
        const QString nl = bt_rm_tr("No losing trade");
        for (const char* k : {"avgLoss", "avgLossPct", "largestLoss", "payoffRatio", "avgLossHoldingBars"})
            why(k, nl);
    }
    if (!r.costs.model.itemised_slippage)
        why("totalSlippage", bt_rm_tr("Slippage is inside the fill prices and not itemised by this engine"));
    return r;
}

// ── Optimise ────────────────────────────────────────────────────────────────

BtOptimizeResult parse_optimize(const QJsonObject& data) {
    BtOptimizeResult r;
    r.payload = data;
    r.schema_version = bt_rm_int(data.value(QStringLiteral("schemaVersion")), 1);
    r.objective = data.value(QStringLiteral("objective")).toString();
    r.method = data.value(QStringLiteral("method")).toString();
    r.iterations = bt_rm_int(data.value(QStringLiteral("iterations")), 0);
    r.total_combinations = bt_rm_int(data.value(QStringLiteral("totalCombinations")), r.iterations);
    r.truncated = data.value(QStringLiteral("truncated")).toBool(false);
    r.seed = bt_rm_num(data.value(QStringLiteral("seed")));
    r.sampling = data.value(QStringLiteral("sampling")).toString();
    r.best_params = data.value(QStringLiteral("bestParameters")).toObject();
    r.best_objective = bt_rm_num(data.value(QStringLiteral("bestObjectiveValue")));
    bt_rm_perf_map(data.value(QStringLiteral("bestPerformance")).toObject(), r.best_perf);
    for (const QJsonValue& row : data.value(QStringLiteral("allResults")).toArray()) {
        const QJsonObject o = row.toObject();
        BtOptRow x;
        x.params = o.value(QStringLiteral("parameters")).toObject();
        x.objective = bt_rm_num(o.value(QStringLiteral("objectiveValue")));
        bt_rm_perf_map(o.value(QStringLiteral("performance")).toObject(), x.perf);
        r.rows.append(x);
    }
    for (const QString& p : bt_rm_strings(data.value(QStringLiteral("paramNames"))))
        r.param_names << p;
    const QJsonObject grid = data.value(QStringLiteral("paramGrid")).toObject();
    for (auto it = grid.constBegin(); it != grid.constEnd(); ++it) {
        QVector<double> vals;
        for (const QJsonValue& v : it.value().toArray()) {
            if (v.isDouble())
                vals.append(v.toDouble());
            else if (v.isBool())
                vals.append(v.toBool() ? 1.0 : 0.0);
        }
        if (!vals.isEmpty())
            r.param_grid.insert(it.key(), vals);
    }
    // v1: varying parameters / their values from the evaluated rows.
    if (r.param_names.isEmpty() || r.param_grid.isEmpty()) {
        QHash<QString, QSet<double>> seen;
        QStringList order;
        for (const BtOptRow& row : r.rows) {
            for (auto it = row.params.constBegin(); it != row.params.constEnd(); ++it) {
                if (!it.value().isDouble())
                    continue;
                if (!seen.contains(it.key()))
                    order << it.key();
                seen[it.key()].insert(it.value().toDouble());
            }
        }
        for (const QString& k : order) {
            if (seen.value(k).size() < 2)
                continue;
            if (!r.param_names.contains(k))
                r.param_names << k;
            if (!r.param_grid.contains(k)) {
                QVector<double> vals(seen.value(k).cbegin(), seen.value(k).cend());
                std::sort(vals.begin(), vals.end());
                r.param_grid.insert(k, vals);
            }
        }
    }
    for (auto& v : r.param_grid)
        std::sort(v.begin(), v.end());
    bt_rm_perf_map(data.value(QStringLiteral("parameterSensitivity")).toObject(), r.sensitivity);
    bt_rm_perf_map(data.value(QStringLiteral("parameterSpread")).toObject(), r.spread);
    const QJsonObject stab = data.value(QStringLiteral("stability")).toObject();
    r.plateau_score = bt_rm_num(stab.value(QStringLiteral("plateauScore")));
    r.neighbours = bt_rm_int(stab.value(QStringLiteral("neighbours")));

    const QJsonObject of = data.value(QStringLiteral("overfitting")).toObject();
    r.has_overfitting = !of.isEmpty();
    r.n_trials = bt_rm_int(of.value(QStringLiteral("nTrials")));
    r.sharpe_std = bt_rm_num(of.value(QStringLiteral("sharpeStd")));
    r.expected_max_sharpe = bt_rm_num(of.value(QStringLiteral("expectedMaxSharpe")));
    r.best_sharpe = bt_rm_num(of.value(QStringLiteral("bestSharpe")));
    r.deflated_sharpe = bt_rm_num(of.value(QStringLiteral("deflatedSharpe")));
    r.probabilistic_sharpe = bt_rm_num(of.value(QStringLiteral("probabilisticSharpe")));
    r.pbo = bt_rm_num(of.value(QStringLiteral("pbo")));
    r.prob_oos_loss = bt_rm_num(of.value(QStringLiteral("probOosLoss")));
    r.effective_trials = bt_rm_num(of.value(QStringLiteral("effectiveTrials")));
    r.pbo_reason = bt_rm_str(of.value(QStringLiteral("pboReason")));
    r.overfitting_method = of.value(QStringLiteral("method")).toString();

    const QJsonObject ho = data.value(QStringLiteral("holdout")).toObject();
    r.has_holdout = !ho.isEmpty();
    r.holdout_split_date = ho.value(QStringLiteral("splitDate")).toString();
    r.holdout_fraction = bt_rm_num(ho.value(QStringLiteral("fraction")));
    r.holdout_best_params = ho.value(QStringLiteral("bestParameters")).toObject();
    bt_rm_perf_map(ho.value(QStringLiteral("inSample")).toObject(), r.holdout_in_sample);
    bt_rm_perf_map(ho.value(QStringLiteral("outOfSample")).toObject(), r.holdout_out_sample);

    const QJsonObject sig = data.value(QStringLiteral("statisticalSignificance")).toObject();
    r.has_significance = !sig.isEmpty();
    r.significance_method = sig.value(QStringLiteral("method")).toString();
    r.p_value = bt_rm_num(sig.value(QStringLiteral("pValue")));
    r.significant = sig.value(QStringLiteral("significant")).toBool(false);
    r.significance_note = sig.value(QStringLiteral("note")).toString();
    r.workers = bt_rm_int(data.value(QStringLiteral("workers")));
    r.elapsed_sec = bt_rm_num(data.value(QStringLiteral("elapsedSec")));
    r.warnings = parse_warnings(data.value(QStringLiteral("warnings")));
    r.logs = bt_rm_strings(data.value(QStringLiteral("logs")));
    r.provenance = parse_provenance(data.value(QStringLiteral("provenance")).toObject());
    r.assumptions = parse_assumptions(data.value(QStringLiteral("assumptions")).toObject());
    if (!std::isfinite(r.best_objective) && !r.rows.isEmpty())
        r.best_objective = r.rows.first().objective;
    if (r.best_params.isEmpty() && !r.rows.isEmpty())
        r.best_params = r.rows.first().params;
    if (r.best_perf.isEmpty() && !r.rows.isEmpty())
        r.best_perf = r.rows.first().perf;
    return r;
}

BtSurface optimize_surface(const BtOptimizeResult& r, const QString& x_param, const QString& y_param,
                           const QString& metric) {
    BtSurface s;
    s.x_param = x_param;
    s.y_param = y_param;
    s.higher_is_better = !(metric == QLatin1String("maxDrawdown") || metric == QLatin1String("volatility"));
    s.x_values = r.param_grid.value(x_param);
    const bool one_d = y_param.isEmpty() || y_param == x_param;
    s.y_values = one_d ? QVector<double>{kBtNaN} : r.param_grid.value(y_param);
    if (s.x_values.isEmpty() || s.y_values.isEmpty())
        return s;
    s.values = QVector<QVector<double>>(s.y_values.size(), QVector<double>(s.x_values.size(), kBtNaN));
    auto index_of = [](const QVector<double>& vals, double v) {
        for (int i = 0; i < vals.size(); ++i)
            if (std::fabs(vals[i] - v) <= 1e-9 * std::max(1.0, std::fabs(v)))
                return i;
        return -1;
    };
    auto value_of = [](const QJsonValue& v) {
        if (v.isDouble())
            return v.toDouble();
        if (v.isBool())
            return v.toBool() ? 1.0 : 0.0;
        return kBtNaN;
    };
    for (int k = 0; k < r.rows.size(); ++k) {
        const BtOptRow& row = r.rows[k];
        const int c = index_of(s.x_values, value_of(row.params.value(x_param)));
        const int rr = one_d ? 0 : index_of(s.y_values, value_of(row.params.value(y_param)));
        if (c < 0 || rr < 0)
            continue;
        const double v = metric == QLatin1String("objective") ? row.objective : row.perf_value(metric);
        if (!std::isfinite(v))
            continue;
        double& cell = s.values[rr][c];
        if (!std::isfinite(cell) || (s.higher_is_better ? v > cell : v < cell))
            cell = v;
        if (k == 0) {
            s.best_row = rr;
            s.best_col = c;
        }
    }
    return s;
}

// ── Walk-forward ────────────────────────────────────────────────────────────

BtWalkForwardResult parse_walk_forward(const QJsonObject& data) {
    BtWalkForwardResult r;
    r.payload = data;
    r.schema_version = bt_rm_int(data.value(QStringLiteral("schemaVersion")), 1);
    r.mode = data.value(QStringLiteral("mode")).toString();
    r.objective = data.value(QStringLiteral("objective")).toString();
    r.purge_bars = bt_rm_int(data.value(QStringLiteral("purgeBars")));
    r.embargo_bars = bt_rm_int(data.value(QStringLiteral("embargoBars")));
    r.combinations_per_window = bt_rm_int(data.value(QStringLiteral("combinationsPerWindow")));
    for (const QJsonValue& row : data.value(QStringLiteral("windows")).toArray()) {
        const QJsonObject o = row.toObject();
        BtWfWindow w;
        w.window = bt_rm_int(o.value(QStringLiteral("window")), static_cast<int>(r.windows.size()) + 1);
        w.train_start = o.value(QStringLiteral("trainStart")).toString();
        w.train_end = o.value(QStringLiteral("trainEnd")).toString();
        w.test_start = o.value(QStringLiteral("testStart")).toString();
        w.test_end = o.value(QStringLiteral("testEnd")).toString();
        w.train_bars = bt_rm_int(o.value(QStringLiteral("trainBars")));
        w.test_bars = bt_rm_int(o.value(QStringLiteral("testBars")));
        w.best_params = o.value(QStringLiteral("bestParams")).toObject();
        w.is_return = bt_rm_num(o.value(QStringLiteral("inSampleReturn")));
        w.is_sharpe = bt_rm_num(o.value(QStringLiteral("inSampleSharpe")));
        w.oos_return = bt_rm_num(o.value(QStringLiteral("outOfSampleReturn")));
        w.oos_sharpe = bt_rm_num(o.value(QStringLiteral("outOfSampleSharpe")));
        w.oos_max_dd = bt_rm_num(o.value(QStringLiteral("outOfSampleMaxDrawdown")));
        w.oos_trades = bt_rm_num(o.value(QStringLiteral("outOfSampleTrades")));
        w.oos_cagr = bt_rm_num(o.value(QStringLiteral("outOfSampleCagr")));
        r.windows.append(w);
    }
    r.n_windows = bt_rm_int(data.value(QStringLiteral("nWindows")), static_cast<int>(r.windows.size()));
    r.oos_equity = bt_rm_dated(data.value(QStringLiteral("oosEquity")), "equity");
    r.oos_coverage = bt_rm_num(data.value(QStringLiteral("oosCoverage")));
    r.avg_oos_return = bt_rm_num(data.value(QStringLiteral("avgOosReturn")));
    r.avg_oos_sharpe = bt_rm_num(data.value(QStringLiteral("avgOosSharpe")));
    r.avg_is_sharpe = bt_rm_num(data.value(QStringLiteral("avgIsSharpe")));
    r.oos_return_std = bt_rm_num(data.value(QStringLiteral("oosReturnStd")));
    r.compounded_oos_return = bt_rm_num(data.value(QStringLiteral("compoundedOosReturn")));
    r.oos_sharpe = bt_rm_num(data.value(QStringLiteral("oosSharpe")));
    r.robustness = bt_rm_num(data.value(QStringLiteral("robustnessScore")));
    r.degradation = bt_rm_num(data.value(QStringLiteral("degradationRatio")));
    r.wf_efficiency = bt_rm_num(data.value(QStringLiteral("walkForwardEfficiency")));
    const QJsonObject ps = data.value(QStringLiteral("paramStability")).toObject();
    for (auto it = ps.constBegin(); it != ps.constEnd(); ++it) {
        const QJsonObject o = it.value().toObject();
        r.param_stability.insert(it.key(), {bt_rm_doubles(o.value(QStringLiteral("values"))),
                                            bt_rm_num(o.value(QStringLiteral("std")))});
    }
    // v1: per-window best parameters -> stability.
    if (r.param_stability.isEmpty() && !r.windows.isEmpty()) {
        QStringList keys;
        for (const BtWfWindow& w : r.windows)
            for (auto it = w.best_params.constBegin(); it != w.best_params.constEnd(); ++it)
                if (it.value().isDouble() && !keys.contains(it.key()))
                    keys << it.key();
        for (const QString& k : keys) {
            QVector<double> vals;
            double sum = 0;
            int cnt = 0;
            for (const BtWfWindow& w : r.windows) {
                const double v = bt_rm_num(w.best_params.value(k));
                vals.append(v);
                if (std::isfinite(v)) {
                    sum += v;
                    ++cnt;
                }
            }
            double sd = kBtNaN;
            if (cnt > 1) {
                const double mean = sum / cnt;
                double ss = 0;
                for (double v : vals)
                    if (std::isfinite(v))
                        ss += (v - mean) * (v - mean);
                sd = std::sqrt(ss / (cnt - 1));
            }
            r.param_stability.insert(k, {vals, sd});
        }
    }
    r.workers = bt_rm_int(data.value(QStringLiteral("workers")));
    r.warnings = parse_warnings(data.value(QStringLiteral("warnings")));
    r.logs = bt_rm_strings(data.value(QStringLiteral("logs")));
    r.provenance = parse_provenance(data.value(QStringLiteral("provenance")).toObject());
    r.assumptions = parse_assumptions(data.value(QStringLiteral("assumptions")).toObject());
    return r;
}

// ── Monte Carlo ─────────────────────────────────────────────────────────────

BtMonteCarloResult parse_monte_carlo(const QJsonObject& data) {
    BtMonteCarloResult r;
    r.payload = data;
    r.schema_version = bt_rm_int(data.value(QStringLiteral("schemaVersion")), 1);
    r.method = data.value(QStringLiteral("method")).toString();
    r.method_id = data.value(QStringLiteral("methodId")).toString(r.method);
    r.n_simulations = bt_rm_int(data.value(QStringLiteral("nSimulations")), 0);
    r.seed = bt_rm_num(data.value(QStringLiteral("seed")));
    r.block_size = bt_rm_num(data.value(QStringLiteral("blockSize")));
    r.horizon_bars = bt_rm_int(data.value(QStringLiteral("horizonBars")));
    r.n_trades = bt_rm_int(data.value(QStringLiteral("nTrades")));
    r.initial_capital = bt_rm_num(data.value(QStringLiteral("initialCapital")));
    const QJsonObject pb = data.value(QStringLiteral("percentileBands")).toObject();
    if (!pb.isEmpty()) {
        r.x = bt_rm_doubles(pb.value(QStringLiteral("x")));
        r.p5 = bt_rm_doubles(pb.value(QStringLiteral("p5")));
        r.p25 = bt_rm_doubles(pb.value(QStringLiteral("p25")));
        r.p50 = bt_rm_doubles(pb.value(QStringLiteral("p50")));
        r.p75 = bt_rm_doubles(pb.value(QStringLiteral("p75")));
        r.p95 = bt_rm_doubles(pb.value(QStringLiteral("p95")));
    } else {
        // v1: bands[] {lower, upper, lowerValues, upperValues} + median.
        r.x = bt_rm_doubles(data.value(QStringLiteral("x")));
        for (const QJsonValue& b : data.value(QStringLiteral("bands")).toArray()) {
            const QJsonObject o = b.toObject();
            const int lo = bt_rm_int(o.value(QStringLiteral("lower")), -1);
            const QVector<double> lv = bt_rm_doubles(o.value(QStringLiteral("lowerValues")));
            const QVector<double> uv = bt_rm_doubles(o.value(QStringLiteral("upperValues")));
            if (lo == 5) {
                r.p5 = lv;
                r.p95 = uv;
            } else if (lo == 25) {
                r.p25 = lv;
                r.p75 = uv;
            }
        }
        r.p50 = bt_rm_doubles(data.value(QStringLiteral("median")));
    }
    if (r.x.isEmpty()) {
        const int n = static_cast<int>(std::max(r.p50.size(), r.p5.size()));
        r.x.resize(n);
        for (int i = 0; i < n; ++i)
            r.x[i] = i;
    }
    r.x_dates = bt_rm_strings(data.value(QStringLiteral("xDates")));
    r.original = bt_rm_doubles(data.value(QStringLiteral("originalPath")));
    if (r.original.isEmpty())
        r.original = bt_rm_doubles(data.value(QStringLiteral("original")));
    for (const QJsonValue& p : data.value(QStringLiteral("samplePaths")).toArray())
        r.sample_paths.append(bt_rm_doubles(p));
    r.final_values = bt_rm_doubles(data.value(QStringLiteral("finalValues")));
    if (r.final_values.isEmpty())
        r.final_values = bt_rm_doubles(data.value(QStringLiteral("finalEquity")));
    r.drawdown_values = bt_rm_doubles(data.value(QStringLiteral("drawdownValues")));
    if (r.drawdown_values.isEmpty() && data.value(QStringLiteral("maxDrawdown")).isArray())
        r.drawdown_values = bt_rm_doubles(data.value(QStringLiteral("maxDrawdown")));
    bt_rm_perf_map(data.value(QStringLiteral("stats")).toObject(), r.stats);
    bt_rm_perf_map(data.value(QStringLiteral("backtest")).toObject(), r.backtest);
    if (!std::isfinite(r.initial_capital) && !r.p50.isEmpty())
        r.initial_capital = r.p50.first();
    r.warnings = parse_warnings(data.value(QStringLiteral("warnings")));
    r.logs = bt_rm_strings(data.value(QStringLiteral("logs")));
    r.provenance = parse_provenance(data.value(QStringLiteral("provenance")).toObject());
    r.assumptions = parse_assumptions(data.value(QStringLiteral("assumptions")).toObject());
    return r;
}

} // namespace fincept::services::backtest
