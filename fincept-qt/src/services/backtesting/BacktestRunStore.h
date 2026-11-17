// src/services/backtesting/BacktestRunStore.h
//
// Saved Backtesting runs in the app database (table `backtest_runs`, created by
// migration v056). Every call is a single short statement; the Database hands out a
// connection per thread, so the screen calls the heavy ones (payload read / save,
// which (de)compress up to a megabyte) from a worker thread.
//
// No schema change: what the redesigned screen needs beyond the v056 columns lives
// in summary_json — "signature" (inputs hash of the run, for "Inputs changed"),
// "engine" / "engineImplementation" / "engineVersion", "schemaVersion", "spark"
// (60-point equity line for the RUNS list) and "pinned". A run is PINNED when it has
// a label; labelled runs are never dropped by the kMaxRuns cap.
#pragma once

#include "core/result/Result.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <atomic>

namespace fincept::services::backtest {

struct SavedRun {
    qint64 id = 0;
    QString created_at; ///< ISO-8601 UTC
    QString provider;
    QString command;
    QString strategy;
    QString symbols; ///< comma-separated
    QString start_date;
    QString end_date;
    QString interval;
    QString currency;
    QString label;
    bool synthetic = false;
    QJsonObject summary; ///< headline metrics + signature / engine / spark (see summary_of)

    bool pinned() const { return !label.isEmpty(); }
    QString signature() const { return summary.value(QStringLiteral("signature")).toString(); }
};

class BacktestRunStore {
  public:
    static BacktestRunStore& instance();

    /// False until migration v056 has created the table.
    bool available();

    /// Store a completed run; returns its id. `meta` carries provider (providerSlug), command,
    /// strategy, symbols[], startDate, endDate, interval; `inputs` the request args;
    /// `signature` the run signature (BacktestRequest.h run_signature). Keeps at most
    /// kMaxRuns unlabelled rows (oldest dropped first).
    Result<qint64> save(const QJsonObject& meta, const QJsonObject& inputs, const QJsonObject& payload,
                        const QString& currency, const QString& signature = {});
    /// Newest first.
    Result<QVector<SavedRun>> list(int limit = 500);
    /// The newest run of `command` (optionally of one provider and strategy); ok with id 0 when none.
    Result<SavedRun> latest(const QString& command, const QString& provider = {}, const QString& strategy = {});
    /// One row without its payload; ok with id 0 when it does not exist.
    Result<SavedRun> row(qint64 id);
    Result<QJsonObject> payload(qint64 id);
    Result<QJsonObject> inputs(qint64 id);
    Result<void> remove(qint64 id);
    /// A non-empty label pins the run (kept beyond the cap); an empty one unpins it.
    Result<void> rename(qint64 id, const QString& label);

    /// The headline metrics kept in summary_json for list / comparison views.
    static QJsonObject summary_of(const QString& command, const QJsonObject& payload);

    static constexpr int kMaxRuns = 300;

  private:
    BacktestRunStore() = default;
    std::atomic<int> available_{-1}; ///< -1 unknown, 0 no, 1 yes (any thread)
};

} // namespace fincept::services::backtest
