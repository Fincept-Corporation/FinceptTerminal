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
    void reload_plan();          // (re)fetch source + rebuild table for current pickers
    void recompute_footer();     // update required/available + top-up enablement
    void do_top_up();
    void do_replicate();

  private:
    void populate_accounts();
    void apply_items(const QVector<trading::replication::SourceItem>& items, const QString& error);
    void fill_table();
    trading::replication::ReplicationOptions current_options() const;

    QComboBox*    source_combo_ = nullptr;
    QComboBox*    target_combo_ = nullptr;
    QCheckBox*    inc_holdings_ = nullptr;
    QCheckBox*    inc_positions_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel*       footer_ = nullptr;
    QPushButton*  topup_btn_ = nullptr;
    QPushButton*  replicate_btn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel*       status_ = nullptr;

    QVector<trading::replication::SourceItem> source_items_;
    trading::replication::ReplicationPlan plan_;
    bool loading_ = false;
};

} // namespace fincept::screens
