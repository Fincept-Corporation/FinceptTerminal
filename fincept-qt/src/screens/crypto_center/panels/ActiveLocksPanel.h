#pragma once

#include "services/wallet/WalletTypes.h"

#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QFrame;
class QHideEvent;
class QLabel;
class QShowEvent;
class QTableWidget;

namespace fincept::screens::panels {

/// ActiveLocksPanel — middle section of the STAKE tab (Phase 3 §3.2).
///
/// Renders the user's `LockPosition`s as a table with columns:
///   LOCKED · DURATION · UNLOCKS · WEIGHT · YIELD (LIFETIME)
/// plus a TOTAL row at the bottom.
///
/// Subscribes (visibility-driven, P3): `wallet:locks:<pubkey>`. Empty state
/// shows "No active locks" with a CTA pointing at LockPanel above.
///
/// Click an unexpired row → menu with Extend (Withdraw disabled with a
/// tooltip "available after <date>"). Click an expired row → Withdraw
/// enabled.
///
/// Mock-mode handling: when published `LockPosition::is_mock` is true, the
/// head pill reads "DEMO" and any Extend/Withdraw click shows a toast
/// "fincept_lock not deployed yet — see Settings" via the error strip.
class ActiveLocksPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ActiveLocksPanel(QWidget* parent = nullptr);
    ~ActiveLocksPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_locks_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_row_context_menu(const QPoint& pos);

    void rebuild_table();
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void update_demo_chip(bool is_mock);

    // Head
    QLabel* summary_label_ = nullptr;
    QLabel* status_pill_ = nullptr;

    // Body
    QTableWidget* table_ = nullptr;
    QLabel* empty_state_ = nullptr;

    // Error strip
    QFrame* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;

    // State
    QString current_pubkey_;
    QString current_topic_;
    QVector<fincept::wallet::LockPosition> latest_;
};

} // namespace fincept::screens::panels
