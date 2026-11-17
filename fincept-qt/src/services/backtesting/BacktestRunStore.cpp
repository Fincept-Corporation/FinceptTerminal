// src/services/backtesting/BacktestRunStore.cpp
#include "services/backtesting/BacktestRunStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <cmath>

namespace fincept::services::backtest {

namespace {

QString run_store_json(const QJsonObject& o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QJsonObject run_store_obj(const QString& s) {
    const QJsonDocument d = QJsonDocument::fromJson(s.toUtf8());
    return d.isObject() ? d.object() : QJsonObject();
}

void run_store_put(QJsonObject& out, const char* key, const QJsonValue& v) {
    if (v.isDouble() && std::isfinite(v.toDouble()))
        out.insert(QLatin1String(key), v);
}

/// 60 points of the equity curve (every n-th, last kept), rounded — the RUNS sparkline.
QJsonArray run_store_spark(const QJsonArray& equity, const char* key = "equity") {
    QJsonArray out;
    const qsizetype n = equity.size();
    if (n < 2)
        return out;
    const qsizetype step = std::max<qsizetype>(1, n / 60);
    for (qsizetype i = 0; i < n; i += step) {
        const QJsonValue v = equity.at(i).isObject() ? equity.at(i).toObject().value(QLatin1String(key)) : equity.at(i);
        if (v.isDouble() && std::isfinite(v.toDouble()))
            out.append(std::round(v.toDouble() * 100.0) / 100.0);
    }
    const QJsonValue last = equity.at(n - 1).isObject() ? equity.at(n - 1).toObject().value(QLatin1String(key))
                                                        : equity.at(n - 1);
    if ((n - 1) % step != 0 && last.isDouble())
        out.append(std::round(last.toDouble() * 100.0) / 100.0);
    return out;
}

constexpr const char* kRunStoreColumns = "id, created_at, provider, command, strategy, symbols, start_date, end_date, "
                                         "interval, currency, label, synthetic, summary_json";

SavedRun run_store_row(QSqlQuery& q) {
    SavedRun s;
    s.id = q.value(0).toLongLong();
    s.created_at = q.value(1).toString();
    s.provider = q.value(2).toString();
    s.command = q.value(3).toString();
    s.strategy = q.value(4).toString();
    s.symbols = q.value(5).toString();
    s.start_date = q.value(6).toString();
    s.end_date = q.value(7).toString();
    s.interval = q.value(8).toString();
    s.currency = q.value(9).toString();
    s.label = q.value(10).toString();
    s.synthetic = q.value(11).toInt() != 0;
    s.summary = run_store_obj(q.value(12).toString());
    return s;
}

} // namespace

BacktestRunStore& BacktestRunStore::instance() {
    static BacktestRunStore inst;
    return inst;
}

bool BacktestRunStore::available() {
    const int known = available_.load();
    if (known >= 0)
        return known == 1;
    auto r = Database::instance().execute(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='backtest_runs'"));
    const int now = (r.is_ok() && r.value().next()) ? 1 : 0;
    if (available_.exchange(now) < 0 && now == 0)
        LOG_WARN("Backtesting", "backtest_runs table missing (migration v056 not applied) - saved runs disabled");
    return now == 1;
}

QJsonObject BacktestRunStore::summary_of(const QString& command, const QJsonObject& p) {
    QJsonObject s;
    if (command == QLatin1String("backtest")) {
        const QJsonObject perf = p.value(QStringLiteral("performance")).toObject();
        const QJsonObject ts = p.value(QStringLiteral("tradeStats")).toObject();
        const QJsonObject st = p.value(QStringLiteral("statistics")).toObject();
        const QJsonObject adv = p.value(QStringLiteral("advancedMetrics")).toObject();
        for (const char* k : {"totalReturn", "annualizedReturn", "sharpeRatio", "sortinoRatio", "calmarRatio",
                              "maxDrawdown", "volatility", "alpha", "beta", "informationRatio"})
            run_store_put(s, k, perf.value(QLatin1String(k)));
        run_store_put(s, "winRate", ts.contains(QStringLiteral("winRate")) ? ts.value(QStringLiteral("winRate"))
                                                                           : perf.value(QStringLiteral("winRate")));
        run_store_put(s, "profitFactor", ts.contains(QStringLiteral("profitFactor"))
                                             ? ts.value(QStringLiteral("profitFactor"))
                                             : perf.value(QStringLiteral("profitFactor")));
        run_store_put(s, "trades", ts.contains(QStringLiteral("closedTrades")) ? ts.value(QStringLiteral("closedTrades"))
                                                                               : perf.value(QStringLiteral("totalTrades")));
        run_store_put(s, "expectancy", ts.value(QStringLiteral("expectancy")));
        run_store_put(s, "finalCapital", st.value(QStringLiteral("finalCapital")));
        run_store_put(s, "initialCapital", st.value(QStringLiteral("initialCapital")));
        run_store_put(s, "var95", adv.value(QStringLiteral("var95")));
        run_store_put(s, "benchmarkReturn", p.value(QStringLiteral("benchmark")).toObject().value(QStringLiteral("totalReturn")));
    } else if (command == QLatin1String("optimize")) {
        run_store_put(s, "bestObjectiveValue", p.value(QStringLiteral("bestObjectiveValue")));
        run_store_put(s, "iterations", p.value(QStringLiteral("iterations")));
        const QJsonObject bp = p.value(QStringLiteral("bestPerformance")).toObject();
        for (const char* k : {"totalReturn", "sharpeRatio", "maxDrawdown"})
            run_store_put(s, k, bp.value(QLatin1String(k)));
    } else if (command == QLatin1String("walk_forward")) {
        for (const char* k : {"avgOosReturn", "avgOosSharpe", "robustnessScore", "degradationRatio",
                              "compoundedOosReturn"})
            run_store_put(s, k, p.value(QLatin1String(k)));
    } else if (command == QLatin1String("monte_carlo")) {
        const QJsonObject st = p.value(QStringLiteral("stats")).toObject();
        for (const char* k : {"finalP5", "finalP50", "finalP95", "probLoss", "ddP95", "probRuin", "originalPercentile"})
            run_store_put(s, k, st.value(QLatin1String(k)));
    }
    // Identity of the engine that produced it (contract v2; absent in v1 payloads).
    const QJsonObject prov = p.value(QStringLiteral("provenance")).toObject();
    auto put_text = [&s](const char* key, const QString& v) {
        if (!v.isEmpty())
            s.insert(QLatin1String(key), v);
    };
    put_text("engine", p.value(QStringLiteral("engine")).toString(prov.value(QStringLiteral("engine")).toString()));
    put_text("engineImplementation", p.value(QStringLiteral("engineImplementation"))
                                         .toString(prov.value(QStringLiteral("engineImplementation")).toString()));
    const QJsonValue ev = p.contains(QStringLiteral("engineVersion")) ? p.value(QStringLiteral("engineVersion"))
                                                                      : prov.value(QStringLiteral("engineVersion"));
    put_text("engineVersion", ev.isString() ? ev.toString() : QString());
    run_store_put(s, "schemaVersion", p.value(QStringLiteral("schemaVersion")));
    if (command == QLatin1String("backtest")) {
        const QJsonObject perf = p.value(QStringLiteral("performance")).toObject();
        run_store_put(s, "cagr", perf.contains(QStringLiteral("cagr")) ? perf.value(QStringLiteral("cagr"))
                                                                       : perf.value(QStringLiteral("annualizedReturn")));
        run_store_put(s, "exposure", p.value(QStringLiteral("exposure")).toObject().value(QStringLiteral("timeInMarket")));
        run_store_put(s, "totalCosts", p.value(QStringLiteral("costs")).toObject().value(QStringLiteral("totalCosts")));
        const QJsonArray spark = run_store_spark(p.value(QStringLiteral("equity")).toArray());
        if (!spark.isEmpty())
            s.insert(QStringLiteral("spark"), spark);
    } else if (command == QLatin1String("walk_forward")) {
        const QJsonArray spark = run_store_spark(p.value(QStringLiteral("oosEquity")).toArray());
        if (!spark.isEmpty())
            s.insert(QStringLiteral("spark"), spark);
    } else if (command == QLatin1String("monte_carlo")) {
        const QJsonObject pb = p.value(QStringLiteral("percentileBands")).toObject();
        const QJsonArray spark =
            run_store_spark(pb.isEmpty() ? p.value(QStringLiteral("median")).toArray() : pb.value(QStringLiteral("p50")).toArray());
        if (!spark.isEmpty())
            s.insert(QStringLiteral("spark"), spark);
    }
    return s;
}

Result<qint64> BacktestRunStore::save(const QJsonObject& meta, const QJsonObject& inputs, const QJsonObject& payload,
                                      const QString& currency, const QString& signature) {
    if (!available())
        return Result<qint64>::err("backtest_runs table not available");
    QStringList syms;
    for (const QJsonValue& v : meta.value(QStringLiteral("symbols")).toArray())
        syms << v.toString();
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QString command = meta.value(QStringLiteral("command")).toString();
    QJsonObject summary = summary_of(command, payload);
    if (!signature.isEmpty())
        summary.insert(QStringLiteral("signature"), signature);
    auto& db = Database::instance();
    auto r = db.execute(
        QStringLiteral("INSERT INTO backtest_runs (created_at, provider, command, strategy, symbols, start_date, "
                       "end_date, interval, currency, label, synthetic, summary_json, inputs_json, payload, "
                       "payload_bytes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, '', ?, ?, ?, ?, ?)"),
        {QDateTime::currentDateTimeUtc().toString(Qt::ISODate), meta.value(QStringLiteral("providerSlug")).toString(),
         command, meta.value(QStringLiteral("strategy")).toString(), syms.join(QLatin1Char(',')),
         meta.value(QStringLiteral("startDate")).toString(), meta.value(QStringLiteral("endDate")).toString(),
         meta.value(QStringLiteral("interval")).toString(), currency,
         payload.value(QStringLiteral("usingSyntheticData")).toBool() ? 1 : 0,
         run_store_json(summary), run_store_json(inputs), qCompress(json, 6),
         static_cast<qint64>(json.size())});
    if (r.is_err()) {
        LOG_ERROR("Backtesting", QString("Saving run failed: %1").arg(QString::fromStdString(r.error())));
        return Result<qint64>::err(r.error());
    }
    const qint64 id = r.value().lastInsertId().toLongLong();
    // Cap the table: keep the newest kMaxRuns unlabelled runs; labelled (pinned) runs are never dropped
    // and do not count against the cap.
    db.execute(QStringLiteral("DELETE FROM backtest_runs WHERE label = '' AND id NOT IN "
                              "(SELECT id FROM backtest_runs WHERE label = '' ORDER BY id DESC LIMIT ?)"),
               {kMaxRuns});
    return Result<qint64>::ok(id);
}

Result<QVector<SavedRun>> BacktestRunStore::list(int limit) {
    if (!available())
        return Result<QVector<SavedRun>>::err("backtest_runs table not available");
    auto r = Database::instance().execute(
        QStringLiteral("SELECT %1 FROM backtest_runs ORDER BY id DESC LIMIT ?").arg(QLatin1String(kRunStoreColumns)),
        {limit});
    if (r.is_err())
        return Result<QVector<SavedRun>>::err(r.error());
    QVector<SavedRun> out;
    auto& q = r.value();
    while (q.next())
        out.append(run_store_row(q));
    return Result<QVector<SavedRun>>::ok(std::move(out));
}

Result<SavedRun> BacktestRunStore::latest(const QString& command, const QString& provider, const QString& strategy) {
    if (!available())
        return Result<SavedRun>::err("backtest_runs table not available");
    QString sql = QStringLiteral("SELECT %1 FROM backtest_runs WHERE command = ?").arg(QLatin1String(kRunStoreColumns));
    QVariantList args{command};
    if (!provider.isEmpty()) {
        sql += QStringLiteral(" AND provider = ?");
        args << provider;
    }
    if (!strategy.isEmpty()) {
        sql += QStringLiteral(" AND strategy = ?");
        args << strategy;
    }
    sql += QStringLiteral(" ORDER BY id DESC LIMIT 1");
    auto r = Database::instance().execute(sql, args);
    if (r.is_err())
        return Result<SavedRun>::err(r.error());
    if (!r.value().next())
        return Result<SavedRun>::ok(SavedRun{});
    return Result<SavedRun>::ok(run_store_row(r.value()));
}

Result<SavedRun> BacktestRunStore::row(qint64 id) {
    if (!available())
        return Result<SavedRun>::err("backtest_runs table not available");
    auto r = Database::instance().execute(
        QStringLiteral("SELECT %1 FROM backtest_runs WHERE id = ?").arg(QLatin1String(kRunStoreColumns)), {id});
    if (r.is_err())
        return Result<SavedRun>::err(r.error());
    if (!r.value().next())
        return Result<SavedRun>::ok(SavedRun{});
    return Result<SavedRun>::ok(run_store_row(r.value()));
}

Result<QJsonObject> BacktestRunStore::payload(qint64 id) {
    auto r = Database::instance().execute(QStringLiteral("SELECT payload FROM backtest_runs WHERE id = ?"), {id});
    if (r.is_err())
        return Result<QJsonObject>::err(r.error());
    if (!r.value().next())
        return Result<QJsonObject>::err("run not found");
    const QByteArray raw = qUncompress(r.value().value(0).toByteArray());
    const QJsonDocument d = QJsonDocument::fromJson(raw);
    if (!d.isObject())
        return Result<QJsonObject>::err("stored payload is not valid JSON");
    return Result<QJsonObject>::ok(d.object());
}

Result<QJsonObject> BacktestRunStore::inputs(qint64 id) {
    auto r = Database::instance().execute(QStringLiteral("SELECT inputs_json FROM backtest_runs WHERE id = ?"), {id});
    if (r.is_err())
        return Result<QJsonObject>::err(r.error());
    if (!r.value().next())
        return Result<QJsonObject>::err("run not found");
    return Result<QJsonObject>::ok(run_store_obj(r.value().value(0).toString()));
}

Result<void> BacktestRunStore::remove(qint64 id) {
    auto r = Database::instance().execute(QStringLiteral("DELETE FROM backtest_runs WHERE id = ?"), {id});
    return r.is_err() ? Result<void>::err(r.error()) : Result<void>::ok();
}

Result<void> BacktestRunStore::rename(qint64 id, const QString& label) {
    auto r = Database::instance().execute(QStringLiteral("UPDATE backtest_runs SET label = ? WHERE id = ?"),
                                          {label.left(120), id});
    return r.is_err() ? Result<void>::err(r.error()) : Result<void>::ok();
}

} // namespace fincept::services::backtest
