// src/screens/backtesting/BtWorkbenchTabs.h
//
// The Backtesting tab's "workbench" tabs that are not result views of one analysis:
//   BtAssumptionsView  run view #7 (inside the run pane): everything a reader needs to trust a number —
//                      warnings (most severe first), data provenance per symbol, execution model, cost model,
//                      metric conventions (annualisation, risk-free source, VaR method), NOT MODELLED list,
//                      ignored settings, engine identity (engine / implementation / version / run id / started /
//                      duration / seed), data disclosures (model-priced options, derived futures, survivorship),
//                      metrics left blank and why (undefinedMetrics), the provider log, RAW JSON. Never empty:
//                      a v1 payload shows what it has plus "This engine does not report data provenance yet".
//   BtRunsTab          the run history (BacktestRunStore): filter, facets, pinned only, model/view table with an
//                      equity sparkline per run; OPEN · RE-RUN · LOAD INPUTS · RENAME / PIN · COMPARE · EXPORT ·
//                      DELETE. Read on a worker. "No saved runs yet" page when empty.
//   BtCompareTab       2..6 saved runs side by side (BtCompareView): the selection is remembered, bundles are read
//                      on a worker, "Nothing to compare yet" page with fewer than 2.
//   BtResearchTab      the eight non-strategy tools of the engines (indicator, indicator signals, signal
//                      generators, ML labels, labels → signals, CV splits, returns analysis, indicator sweep), each
//                      with its own settings strip, the exact request keys of the old screen, an AutoLoadPane
//                      (light tools AutoRun, the sweep Manual) and a KPI row + chart + rows table result.
#pragma once

#include "screens/backtesting/BtHost.h"
#include "screens/backtesting/BtTabKit.h"

#include <QVector>
#include <QWidget>

namespace fincept::screens::bt {

class BtAssumptionsView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtAssumptionsView(BtSelection* selection, QWidget* parent = nullptr);
    ~BtAssumptionsView() override;
    void show_run(const BtRunPtr& run) override;

  signals:
    /// RAW JSON was asked for (the screen opens its viewer with the active payload).
    void raw_json_requested();

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    struct Impl; // BtAssumptionsView.cpp
    std::unique_ptr<Impl> d;
};

class BtRunsTab : public QWidget {
    Q_OBJECT
  public:
    explicit BtRunsTab(BtHost* host, QWidget* parent = nullptr);
    ~BtRunsTab() override;
    /// Re-read the list (worker) — after a run was saved, renamed or deleted.
    void refresh();
    /// Select a run's row (after OPEN from elsewhere).
    void select_run(qint64 store_id);

  signals:
    /// COMPARE TICKED / context menu "Compare": the ids to show in COMPARE.
    void compare_requested(const QVector<qint64>& store_ids);
    /// The list changed (a run deleted / renamed / pinned).
    void runs_changed();

  private:
    struct Impl; // BtRunsTab.cpp
    std::unique_ptr<Impl> d;
};

class BtCompareTab : public QWidget {
    Q_OBJECT
  public:
    explicit BtCompareTab(BtHost* host, QWidget* parent = nullptr);
    ~BtCompareTab() override;
    /// Replace the selection (2..6 ids; more are cut).
    void set_runs(const QVector<qint64>& store_ids);
    /// Add one run (the active run: Ctrl+Shift+C). Ignored when already in or full.
    void add_run(qint64 store_id);
    QVector<qint64> run_ids() const;
    /// Drop ids that no longer exist (after a delete in RUNS).
    void prune();

  signals:
    void ids_changed();

  private:
    struct Impl; // BtCompareTab.cpp
    std::unique_ptr<Impl> d;
};

class BtResearchTab : public QWidget {
    Q_OBJECT
  public:
    explicit BtResearchTab(BtHost* host, QWidget* parent = nullptr);
    ~BtResearchTab() override;
    /// The engine / symbols changed: tools the engine lacks are disabled with the reason; signature refresh.
    void refresh();
    /// The primary action (RUN the selected tool).
    void run();
    void cancel();
    bool is_running() const;

  signals:
    void running_changed(bool running);

  private:
    struct Impl; // BtResearchTab.cpp
    std::unique_ptr<Impl> d;
};

} // namespace fincept::screens::bt
