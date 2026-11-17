// src/services/backtesting/BacktestExportText.cpp
#include "services/backtesting/BacktestExportText.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace fincept::services::backtest {

namespace {

QString bt_ex_join(const QStringList& cells) {
    QStringList out;
    out.reserve(cells.size());
    for (const QString& c : cells)
        out << bt_csv_field(c);
    return out.join(QLatin1Char(','));
}

QString bt_ex_params(const QJsonObject& p) {
    QStringList parts;
    for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
        const QJsonValue v = it.value();
        QString s;
        if (v.isDouble())
            s = bt_csv_number(v.toDouble());
        else if (v.isBool())
            s = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        else if (v.isString())
            s = v.toString();
        else
            s = QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        parts << it.key() + QLatin1Char('=') + s;
    }
    return parts.join(QLatin1Char(' '));
}

QString bt_ex_start(const BtExportMeta& meta) {
    return bt_export_header(meta).join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace

QString bt_csv_field(const QString& s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')) ||
        s.contains(QLatin1Char('\r'))) {
        QString q = s;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return s;
}

QString bt_csv_number(double v) {
    if (!std::isfinite(v))
        return {};
    if (v == std::floor(v) && std::fabs(v) < 1e15)
        return QString::number(static_cast<qint64>(v));
    return QString::number(v, 'g', 15);
}

BtExportMeta bt_export_meta(const BtProvenance& prov, const BtAssumptions& a, const QString& command) {
    BtExportMeta m;
    m.command = command;
    m.engine = prov.engine;
    m.engine_implementation = prov.engine_implementation;
    m.engine_version = prov.engine_version;
    m.strategy = prov.strategy_name.isEmpty() ? prov.strategy_type : prov.strategy_name;
    QStringList syms;
    for (const BtProvSymbol& s : prov.symbols)
        syms << s.symbol;
    m.symbols = syms.join(QLatin1Char(' '));
    m.start = prov.first_bar;
    m.end = prov.last_bar;
    m.interval = prov.interval;
    m.currency = prov.currency;
    m.data_source = prov.data_source.isEmpty() ? a.data_source : prov.data_source;
    m.adjustment = prov.adjustment;
    m.fill_model = a.fill_description.isEmpty() ? a.fill_at : a.fill_description;
    m.cost_model = a.costs;
    if (std::isfinite(a.risk_free_rate))
        m.risk_free = (a.risk_free_source.isEmpty() ? QString() : a.risk_free_source + QLatin1Char(' ')) +
                      bt_csv_number(a.risk_free_rate);
    else
        m.risk_free = a.risk_free_source;
    return m;
}

BtExportMeta bt_export_meta(const BtRun& run) {
    BtExportMeta m = bt_export_meta(run.provenance, run.assumptions, QStringLiteral("backtest"));
    if (m.engine.isEmpty())
        m.engine = run.engine;
    if (m.engine_implementation.isEmpty())
        m.engine_implementation = run.engine_implementation;
    if (m.engine_version.isEmpty())
        m.engine_version = run.engine_version;
    if (m.symbols.isEmpty())
        m.symbols = run.symbols().join(QLatin1Char(' '));
    if (m.start.isEmpty() && !run.dates.isEmpty())
        m.start = run.dates.first();
    if (m.end.isEmpty() && !run.dates.isEmpty())
        m.end = run.dates.last();
    if (m.currency.isEmpty())
        m.currency = run.currency;
    if (run.costs.model.present && !run.costs.model.preset.isEmpty() && m.cost_model.isEmpty())
        m.cost_model = run.costs.model.preset;
    m.synthetic = run.synthetic;
    return m;
}

QStringList bt_export_header(const BtExportMeta& meta) {
    QStringList h;
    if (meta.synthetic)
        h << QStringLiteral("# WARNING: SYNTHETIC DATA - prices were randomly generated; these results have NO market "
                            "meaning");
    const QString when =
        meta.generated_at.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODate) : meta.generated_at;
    h << QStringLiteral("# Fincept Terminal backtesting export (%1), generated %2")
             .arg(meta.command.isEmpty() ? QStringLiteral("backtest") : meta.command, when);
    QString engine = meta.engine;
    if (!meta.engine_implementation.isEmpty())
        engine += QStringLiteral(" (") + meta.engine_implementation + QLatin1Char(')');
    if (!meta.engine_version.isEmpty())
        engine += QStringLiteral(" version ") + meta.engine_version;
    h << QStringLiteral("# engine: %1").arg(engine);
    if (!meta.strategy.isEmpty())
        h << QStringLiteral("# strategy: %1").arg(meta.strategy);
    h << QStringLiteral("# symbols: %1 | period: %2 to %3 | interval: %4")
             .arg(meta.symbols, meta.start, meta.end, meta.interval);
    h << QStringLiteral("# data: %1 | adjustment: %2").arg(meta.data_source, meta.adjustment);
    if (!meta.fill_model.isEmpty())
        h << QStringLiteral("# fill model: %1").arg(meta.fill_model);
    if (!meta.cost_model.isEmpty())
        h << QStringLiteral("# cost model: %1").arg(meta.cost_model);
    if (!meta.risk_free.isEmpty())
        h << QStringLiteral("# risk-free: %1").arg(meta.risk_free);
    h << QStringLiteral("# currency: %1 (money columns; returns, rates and drawdowns are fractions; holding periods "
                        "in bars)")
             .arg(meta.currency.isEmpty() ? QStringLiteral("unknown") : meta.currency);
    // A comment line never breaks out of its line.
    for (QString& line : h) {
        line.replace(QLatin1Char('\n'), QLatin1Char(' '));
        line.replace(QLatin1Char('\r'), QLatin1Char(' '));
    }
    return h;
}

QString bt_trades_csv(const BtRun& run, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    out += bt_ex_join({QStringLiteral("id"), QStringLiteral("symbol"), QStringLiteral("side"), QStringLiteral("status"),
                       QStringLiteral("entry_date"), QStringLiteral("entry_price"), QStringLiteral("exit_date"),
                       QStringLiteral("exit_price"), QStringLiteral("mark_price"), QStringLiteral("quantity"),
                       QStringLiteral("pnl"), QStringLiteral("pnl_fraction"), QStringLiteral("gross_pnl"),
                       QStringLiteral("commission"), QStringLiteral("slippage"), QStringLiteral("borrow"),
                       QStringLiteral("holding_bars"), QStringLiteral("holding_days"), QStringLiteral("mae_fraction"),
                       QStringLiteral("mfe_fraction"), QStringLiteral("exit_reason"), QStringLiteral("currency")}) +
           QLatin1Char('\n');
    for (const BtTrade& t : run.trades) {
        out += bt_ex_join({t.id, t.symbol, t.side, t.open ? QStringLiteral("open") : QStringLiteral("closed"),
                           t.entry_date, bt_csv_number(t.entry_price), t.exit_date, bt_csv_number(t.exit_price),
                           bt_csv_number(t.mark_price), bt_csv_number(t.quantity), bt_csv_number(t.pnl),
                           bt_csv_number(t.pnl_pct), bt_csv_number(t.gross_pnl), bt_csv_number(t.commission),
                           bt_csv_number(t.slippage), bt_csv_number(t.borrow),
                           t.holding_bars >= 0 ? QString::number(t.holding_bars) : QString(),
                           t.holding_days >= 0 ? QString::number(t.holding_days) : QString(), bt_csv_number(t.mae),
                           bt_csv_number(t.mfe), t.exit_reason, meta.currency}) +
               QLatin1Char('\n');
    }
    return out;
}

QString bt_equity_csv(const BtRun& run, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    out += QStringLiteral("date,equity,drawdown_fraction,return_fraction,benchmark,exposure_fraction,"
                          "net_exposure_fraction,cash,currency\n");
    const int n = static_cast<int>(run.t.size());
    auto at = [](const QVector<double>& v, int i) { return i < v.size() ? v[i] : kBtNaN; };
    for (int i = 0; i < n; ++i) {
        out += bt_ex_join({run.dates.value(i), bt_csv_number(at(run.equity, i)), bt_csv_number(at(run.drawdown, i)),
                           bt_csv_number(at(run.returns, i)), bt_csv_number(at(run.benchmark, i)),
                           bt_csv_number(at(run.exposure, i)), bt_csv_number(at(run.net_exposure, i)),
                           bt_csv_number(at(run.cash, i)), meta.currency}) +
               QLatin1Char('\n');
    }
    return out;
}

QString bt_metrics_csv(const BtRun& run, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    out += QStringLiteral("metric,value,why_blank\n");
    QStringList keys = run.metrics.keys();
    // Blank metrics with a reason are listed too, so a reader sees what is missing and why.
    for (auto it = run.undefined.constBegin(); it != run.undefined.constEnd(); ++it) {
        bool found = false;
        for (const QString& k : keys)
            if (k.section(QLatin1Char('.'), -1) == it.key())
                found = true;
        if (!found)
            keys << QStringLiteral("undefined.") + it.key();
    }
    std::sort(keys.begin(), keys.end());
    for (const QString& k : keys) {
        if (k.startsWith(QLatin1String("undefined."))) {
            const QString leaf = k.mid(10);
            out += bt_ex_join({leaf, QString(), run.undefined.value(leaf)}) + QLatin1Char('\n');
            continue;
        }
        out += bt_ex_join({k, bt_csv_number(run.metrics.value(k)), QString()}) + QLatin1Char('\n');
    }
    return out;
}

QString bt_drawdowns_csv(const BtRun& run, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    out += QStringLiteral("rank,depth_fraction,peak_date,trough_date,recovery_date,to_trough_bars,recovery_bars,"
                          "duration_bars,duration_days,peak_equity,trough_equity,currency\n");
    for (const BtDrawdown& d : run.drawdowns) {
        out += bt_ex_join({QString::number(d.rank), bt_csv_number(d.depth), d.peak_date, d.trough_date, d.recovery_date,
                           d.to_trough_bars >= 0 ? QString::number(d.to_trough_bars) : QString(),
                           d.recovery_bars >= 0 ? QString::number(d.recovery_bars) : QString(),
                           d.duration_bars >= 0 ? QString::number(d.duration_bars) : QString(),
                           d.duration_days >= 0 ? QString::number(d.duration_days) : QString(),
                           bt_csv_number(d.peak_equity), bt_csv_number(d.trough_equity), meta.currency}) +
               QLatin1Char('\n');
    }
    return out;
}

QString bt_optimize_csv(const BtOptimizeResult& r, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    QStringList params = r.param_names;
    for (const BtOptRow& row : r.rows)
        for (auto it = row.params.constBegin(); it != row.params.constEnd(); ++it)
            if (!params.contains(it.key()))
                params << it.key();
    const QStringList perf = {QStringLiteral("totalReturn"), QStringLiteral("cagr"),        QStringLiteral("sharpeRatio"),
                              QStringLiteral("sortinoRatio"), QStringLiteral("calmarRatio"), QStringLiteral("maxDrawdown"),
                              QStringLiteral("volatility"),  QStringLiteral("totalTrades"), QStringLiteral("winRate"),
                              QStringLiteral("profitFactor")};
    QStringList head{QStringLiteral("rank")};
    head << params;
    head << QStringLiteral("objective_") + (r.objective.isEmpty() ? QStringLiteral("value") : r.objective);
    head << perf;
    out += bt_ex_join(head) + QLatin1Char('\n');
    int rank = 0;
    for (const BtOptRow& row : r.rows) {
        QStringList cells{QString::number(++rank)};
        for (const QString& p : params) {
            const QJsonValue v = row.params.value(p);
            cells << (v.isDouble() ? bt_csv_number(v.toDouble())
                                   : v.isBool() ? (v.toBool() ? QStringLiteral("true") : QStringLiteral("false"))
                                                : v.toString());
        }
        cells << bt_csv_number(row.objective);
        for (const QString& k : perf)
            cells << bt_csv_number(row.perf_value(k));
        out += bt_ex_join(cells) + QLatin1Char('\n');
    }
    return out;
}

QString bt_walk_forward_csv(const BtWalkForwardResult& r, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    out += QStringLiteral("window,train_start,train_end,test_start,test_end,train_bars,test_bars,best_params,"
                          "is_return_fraction,is_sharpe,oos_return_fraction,oos_sharpe,oos_max_drawdown_fraction,"
                          "oos_trades\n");
    for (const BtWfWindow& w : r.windows) {
        out += bt_ex_join({QString::number(w.window), w.train_start, w.train_end, w.test_start, w.test_end,
                           w.train_bars >= 0 ? QString::number(w.train_bars) : QString(),
                           w.test_bars >= 0 ? QString::number(w.test_bars) : QString(), bt_ex_params(w.best_params),
                           bt_csv_number(w.is_return), bt_csv_number(w.is_sharpe), bt_csv_number(w.oos_return),
                           bt_csv_number(w.oos_sharpe), bt_csv_number(w.oos_max_dd), bt_csv_number(w.oos_trades)}) +
               QLatin1Char('\n');
    }
    return out;
}

QString bt_monte_carlo_csv(const BtMonteCarloResult& r, const BtExportMeta& meta) {
    QString out = bt_ex_start(meta);
    out += QStringLiteral("step,date,p5,p25,p50,p75,p95,original,currency\n");
    auto at = [](const QVector<double>& v, int i) { return i < v.size() ? v[i] : kBtNaN; };
    for (int i = 0; i < r.x.size(); ++i) {
        out += bt_ex_join({bt_csv_number(r.x[i]), r.x_dates.value(i), bt_csv_number(at(r.p5, i)),
                           bt_csv_number(at(r.p25, i)), bt_csv_number(at(r.p50, i)), bt_csv_number(at(r.p75, i)),
                           bt_csv_number(at(r.p95, i)), bt_csv_number(at(r.original, i)), meta.currency}) +
               QLatin1Char('\n');
    }
    return out;
}

QByteArray bt_result_json(const QJsonObject& payload, const BtExportMeta& meta) {
    QJsonObject m;
    m[QStringLiteral("command")] = meta.command;
    m[QStringLiteral("engine")] = meta.engine;
    m[QStringLiteral("engineImplementation")] = meta.engine_implementation;
    m[QStringLiteral("engineVersion")] = meta.engine_version;
    m[QStringLiteral("strategy")] = meta.strategy;
    m[QStringLiteral("symbols")] = meta.symbols;
    m[QStringLiteral("start")] = meta.start;
    m[QStringLiteral("end")] = meta.end;
    m[QStringLiteral("interval")] = meta.interval;
    m[QStringLiteral("currency")] = meta.currency;
    m[QStringLiteral("dataSource")] = meta.data_source;
    m[QStringLiteral("adjustment")] = meta.adjustment;
    m[QStringLiteral("fillModel")] = meta.fill_model;
    m[QStringLiteral("costModel")] = meta.cost_model;
    m[QStringLiteral("riskFree")] = meta.risk_free;
    m[QStringLiteral("synthetic")] = meta.synthetic;
    m[QStringLiteral("generatedAt")] =
        meta.generated_at.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODate) : meta.generated_at;
    m[QStringLiteral("units")] = QStringLiteral("returns, rates, weights, exposure and drawdowns are fractions; money "
                                                "in the currency above; holding periods in bars");
    QJsonObject root;
    root[QStringLiteral("export")] = m;
    root[QStringLiteral("result")] = payload;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace fincept::services::backtest
