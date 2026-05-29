#pragma once
// Action Center Screen — semi-auto order approval UI (Phase 3 §2).
//
// Renders the pending/approved/rejected orders queued by ActionCenter and lets
// the user Approve / Reject them (per-row and in bulk). A per-account Auto ↔
// Semi-Auto mode toggle controls whether new orders are queued. The screen is
// a pure view (P6): it reads from and calls ActionCenter (the service); it
// never spawns Python, makes HTTP calls, or owns business logic.
//
// Live updates: subscribes to ActionCenter signals (pending_order_created,
// order_approved, order_rejected, stats_updated) to refresh the table and
// stats bar without polling — no QTimer is needed (D3).

#include "trading/ActionCenter.h"

#include <QComboBox>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QString>
#include <QTableWidget>
#include <QWidget>

class QPushButton;

namespace fincept::screens {

class ActionCenterScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ActionCenterScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void reload_accounts();        // refresh the account selector from AccountManager
    void refresh();                // reload table + stats for the active filter
    void refresh_stats();
    QString selected_account() const; // empty == all accounts

    // Slots wired to ActionCenter signals.
    void on_pending_created(const fincept::trading::PendingOrder& po);
    void on_order_approved(const QString& pending_id, const QString& broker_order_id);
    void on_order_rejected(const QString& pending_id, const QString& reason);
    void on_stats_updated(const QString& account_id);

    // Row actions.
    void approve_row(const QString& pending_id);
    void reject_row(const QString& pending_id);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QComboBox* account_combo_ = nullptr;
    QComboBox* status_filter_ = nullptr;
    QComboBox* mode_combo_ = nullptr;

    QLabel* stat_pending_ = nullptr;
    QLabel* stat_approved_ = nullptr;
    QLabel* stat_rejected_ = nullptr;
    QLabel* stat_buy_ = nullptr;
    QLabel* stat_sell_ = nullptr;

    QTableWidget* table_ = nullptr;

    QPushButton* approve_all_btn_ = nullptr;
    QPushButton* reject_all_btn_ = nullptr;

    bool wired_ = false; // guard so we connect ActionCenter signals once
};

} // namespace fincept::screens
