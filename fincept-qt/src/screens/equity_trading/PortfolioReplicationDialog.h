#pragma once
#include "trading/replication/PortfolioReplicationService.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QCheckBox;
class QTableWidget;
class QLabel;
class QPushButton;
class QProgressBar;

namespace fincept::ui {
class KpiStrip;
}

namespace fincept::screens {

// Replicate a source broker account's portfolio into a PAPER account as exact
// 1:1 orders. Target list is paper-only by construction. Shows every stock to be
// bought for confirmation, with a one-click paper-cash top-up when a large source
// portfolio exceeds the sandbox balance.
class PortfolioReplicationDialog : public QDialog {
    Q_OBJECT
  public:
    explicit PortfolioReplicationDialog(QWidget* parent = nullptr);

  private slots:
    void reload_plan();      // (re)fetch source + rebuild table for current pickers
    void recompute_footer(); // update required/available + top-up enablement
    void do_top_up();
    void do_replicate();

  private:
    void populate_accounts();
    // Second stage of reload_plan(): runs once both brokers' instrument masters are in
    // memory. `gen` is the plan generation it was started for — see plan_gen_.
    void fetch_source(const QString& src_id, quint64 gen);
    // `note` explains a partial source (one of holdings / positions could not be read).
    void apply_items(const QVector<trading::replication::SourceItem>& items, const QString& error,
                     const QString& note = QString());
    void fill_table();
    // After REPLICATE: each row's outcome in the Status column (the table becomes the
    // result view until the plan is refreshed).
    void show_results(const trading::replication::ReplicationResult& result);
    trading::replication::ReplicationOptions current_options() const;
    QString money(double v) const; // in the target paper portfolio's currency

    QComboBox* source_combo_ = nullptr;
    QComboBox* target_combo_ = nullptr;
    QCheckBox* inc_holdings_ = nullptr;
    QCheckBox* inc_positions_ = nullptr;
    QPushButton* refresh_btn_ = nullptr; // re-read the source and plan again
    ui::KpiStrip* kpis_ = nullptr;       // SELECTED / REQUIRED / PAPER AVAILABLE / SHORTFALL
    QTableWidget* table_ = nullptr;
    QPushButton* topup_btn_ = nullptr;
    QPushButton* replicate_btn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* status_ = nullptr;

    QVector<trading::replication::SourceItem> source_items_;
    trading::replication::ReplicationPlan plan_;
    // Bumped on every reload_plan(). An instrument-load or broker-fetch callback that comes
    // back for an older generation (the user changed a picker meanwhile) is discarded, so a
    // slow response can never fill the table for a selection that is no longer current.
    quint64 plan_gen_ = 0;
    // Currency of the target paper portfolio (money columns and tiles).
    QString currency_;
    // True while the table shows the outcome of the last run rather than a plan:
    // REPLICATE stays off until the plan is refreshed.
    bool showing_results_ = false;
};

} // namespace fincept::screens
