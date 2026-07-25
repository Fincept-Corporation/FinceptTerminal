#pragma once
// PendingOrdersPanel — queued-order approval popover (replaces the standalone
// Action Center dock tab). Renders the orders queued by ActionCenter from the
// headless paths (AI/workflow/broadcast/basket) and lets the user Approve /
// Reject them (per-row and in bulk). Shown as a frameless Qt::Popup anchored to
// the status-bar PendingOrdersBadge.
//
// The per-account Auto ↔ Semi-Auto mode toggle now lives in Account Management,
// not here. This is a pure view (P6): it reads from and calls ActionCenter;
// it never spawns Python, makes HTTP calls, or owns business logic.
//
// Live updates: subscribes to ActionCenter signals (pending_order_created,
// order_approved, order_rejected, stats_updated) in the constructor and
// refreshes without polling (D3).

#include "trading/ActionCenter.h"

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QLabel>
#include <QPoint>
#include <QString>
#include <QTableWidget>
#include <QWidget>

class QPushButton;

namespace fincept::screens {

class PendingOrdersPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PendingOrdersPanel(QWidget* parent = nullptr);

    // Reload + show the popover with its bottom-right corner at `global_anchor`.
    void popup_at(const QPoint& global_anchor);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void reload_accounts(); // refresh the account selector from AccountManager
    void refresh();         // reload table + stats for the active filter
    void refresh_stats();
    QString selected_account() const; // empty == all accounts

    // Slots wired to ActionCenter signals.
    void on_pending_created(const fincept::trading::PendingOrder& po);
    void on_order_approved(const QString& pending_id, const QString& broker_order_id);
    void on_order_rejected(const QString& pending_id, const QString& reason);
    void on_stats_updated(const QString& account_id);

    // Row actions. Return true when the action was dispatched (or the row is
    // gone), false when the user cancelled and the row buttons should re-enable.
    bool approve_row(const QString& pending_id);
    bool reject_row(const QString& pending_id);

    // Age of a queued order as display text; sets *stale_out when the order has
    // been waiting long enough that executing it at market is questionable.
    static QString order_age_text(const QDateTime& created, bool* stale_out = nullptr);

    // A queued order older than this is flagged in the table and in the approval
    // confirmation — and REFUSED by ActionCenter::approve_order().
    //
    // Derived from ActionCenter's constant rather than duplicated: this panel's
    // warning copy and the engine's enforcement must describe the same
    // threshold. If they drifted, the UI would either nag about orders that
    // still execute fine, or stay silent about ones about to be refused —
    // both worse than no warning at all.
    static constexpr qint64 kStaleOrderSecs = fincept::trading::ActionCenter::kPendingOrderTtlMinutes * 60;

    // ── Widgets ──────────────────────────────────────────────────────────────
    QLabel* title_label_ = nullptr;
    QLabel* account_caption_ = nullptr;
    QLabel* show_caption_ = nullptr;
    QComboBox* account_combo_ = nullptr;
    QComboBox* status_filter_ = nullptr;

    QLabel* stat_pending_cap_ = nullptr;
    QLabel* stat_approved_cap_ = nullptr;
    QLabel* stat_rejected_cap_ = nullptr;
    QLabel* stat_buy_cap_ = nullptr;
    QLabel* stat_sell_cap_ = nullptr;

    QLabel* stat_pending_ = nullptr;
    QLabel* stat_approved_ = nullptr;
    QLabel* stat_rejected_ = nullptr;
    QLabel* stat_buy_ = nullptr;
    QLabel* stat_sell_ = nullptr;

    QTableWidget* table_ = nullptr;

    QPushButton* approve_all_btn_ = nullptr;
    QPushButton* reject_all_btn_ = nullptr;
};

} // namespace fincept::screens
