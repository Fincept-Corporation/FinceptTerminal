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
class QPoint;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace fincept::trading {
struct AggRow;
}

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

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
    void hideEvent(QHideEvent* event) override;

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
    /// What a tree page shows: its rows, a skeleton while the first live refresh
    /// after activate() is on its way, or a designed empty state saying why it
    /// is empty (no account / broker not connected / nothing open).
    enum class TreeState { Rows, Loading, NoAccount, Disconnected, Empty };
    struct TreePage {
        QStackedWidget* stack = nullptr;
        QTreeWidget* tree = nullptr;
        ui::LoadingSkeleton* skeleton = nullptr;
        ui::EmptyState* empty = nullptr;
        TreeState state = TreeState::Rows;
        QString spec_key;      ///< what `empty` currently says (re-applied only on change)
        bool awaiting = false; ///< LIVE: first refresh after activate() not in yet
    };

    QWidget* build_summary_bar();
    QTreeWidget* make_tree(bool holdings, QWidget* parent);
    /// Tree + skeleton + empty state in one stacked page, built under `parent`.
    void build_tree_page(TreePage& page, bool holdings, QWidget* parent);
    /// Picks rows / loading / empty for one page from the service's model.
    void update_tree_state(bool holdings);
    /// LIVE with accounts: the next positions/holdings refresh is awaited (skeleton
    /// on an empty model) for at most kAwaitRefreshMs.
    void begin_awaiting_refresh();
    void on_empty_action(bool holdings, bool primary);
    /// Font-derived sizes (float button, summary height) — on appearance_settled only.
    void fit_to_font();
    void rebuild_tree(QTreeWidget* tree, const QVector<trading::AggRow>& rows, bool holdings,
                      QHash<QString, QTreeWidgetItem*>& index);
    void fill_parent(QTreeWidgetItem* item, const trading::AggRow& row, bool holdings);
    void confirm_exit_child(const QString& account_id, const QString& broker_label, const QString& symbol,
                            const QString& exchange, const QString& product, double qty);
    /// LIVE holdings EXIT: a delivery holding is not in the broker's positions book, so
    /// close_position() answers "Position not found" — sell it with a delivery MARKET order.
    void confirm_sell_holding(const QString& account_id, const QString& broker_label, const QString& symbol,
                              const QString& exchange, double qty);
    void confirm_exit_symbol(const QString& symbol, const QString& exchange, bool holdings, int n_accounts);
    /// Right-click on a row: "Open in …" (nav.open_symbol), copy, CSV export.
    void show_row_menu(QTreeWidget* tree, bool holdings, const QPoint& pos);
    void export_csv(bool holdings);
    /// Re-applies what follows the theme but is not a stylesheet: font-derived
    /// sizes and the trees' item colours.
    void refresh_theme();

    // Summary bar
    QScrollArea* summary_scroll_ = nullptr; // hosts the summary bar; its height follows the bar's
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
    TreePage pos_page_;
    TreePage hold_page_;
    QTimer* await_timer_ = nullptr;

    // "symbol|exchange" → parent item, for in-place tick patching
    QHash<QString, QTreeWidgetItem*> pos_index_;
    QHash<QString, QTreeWidgetItem*> hold_index_;

    bool activated_ = false;
};

} // namespace fincept::screens
