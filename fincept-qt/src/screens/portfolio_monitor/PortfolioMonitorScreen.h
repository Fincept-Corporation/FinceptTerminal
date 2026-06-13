#pragma once
// PortfolioMonitorScreen — standalone dockable "all accounts" view: positions &
// holdings aggregated across every active INR broker account, with full control
// (exit / square-off / new order) routed per-account through
// UnifiedPortfolioService. Design:
// docs/superpowers/specs/2026-06-10-portfolio-monitor-design.md

#include <QHash>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace fincept::trading {
struct AggRow;
}

namespace fincept::screens {

class PortfolioMonitorScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioMonitorScreen(QWidget* parent = nullptr);

    /// Flip the ⧉ detach button into a ⤓ dock-back button (feeds pattern —
    /// the host reparents this widget; we only signal intent).
    void set_floating(bool floating);

  signals:
    void float_requested(); // user clicked ⧉ while docked
    void dock_requested();  // user clicked ⤓ while floating

  protected:
    void showEvent(QShowEvent* event) override;

  private slots:
    void rebuild_positions();
    void rebuild_holdings();
    void patch_position(const QString& symbol);
    void patch_holding(const QString& symbol);
    void update_summary();
    void on_action_finished(bool ok, const QString& message);
    void on_new_order();
    void on_mode_toggled(bool live);

  private:
    QWidget* build_summary_bar();
    QTreeWidget* make_tree(bool holdings);
    void rebuild_tree(QTreeWidget* tree, const QVector<trading::AggRow>& rows, bool holdings,
                      QHash<QString, QTreeWidgetItem*>& index);
    void fill_parent(QTreeWidgetItem* item, const trading::AggRow& row, bool holdings);
    void confirm_exit_child(const QString& account_id, const QString& broker_label, const QString& symbol,
                            const QString& exchange, const QString& product, double qty);
    void confirm_exit_symbol(const QString& symbol, const QString& exchange, bool holdings, int n_accounts);

    // Summary bar
    QPushButton* mode_btn_ = nullptr;  // PAPER ⇄ LIVE (checked == LIVE)
    QPushButton* float_btn_ = nullptr; // ⧉ detach / ⤓ dock back
    bool floating_ = false;
    QLabel* sum_positions_pnl_ = nullptr;
    QLabel* sum_day_pnl_ = nullptr;
    QLabel* sum_invested_ = nullptr;
    QLabel* sum_current_ = nullptr;
    QLabel* sum_holdings_pnl_ = nullptr;
    QLabel* sum_return_ = nullptr;
    QLabel* sum_accounts_ = nullptr;
    QLabel* toast_ = nullptr;

    QTabWidget* tabs_ = nullptr;
    QTreeWidget* positions_tree_ = nullptr;
    QTreeWidget* holdings_tree_ = nullptr;

    // "symbol|exchange" → parent item, for in-place tick patching
    QHash<QString, QTreeWidgetItem*> pos_index_;
    QHash<QString, QTreeWidgetItem*> hold_index_;

    bool activated_ = false;
};

} // namespace fincept::screens
