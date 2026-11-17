#pragma once

#include "services/wallet/WalletTypes.h"

#include <QEvent>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QFrame;
class QHideEvent;
class QLabel;
class QShowEvent;
class QTableWidget;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens::panels {

/// ActiveLocksPanel — middle section of the STAKE tab (Phase 3 §3.2).
///
/// Renders the user's `LockPosition`s as a table with columns:
///   LOCKED · DURATION · UNLOCKS · WEIGHT · YIELD (LIFETIME)
/// plus a TOTAL row at the bottom.
///
/// Subscribes (visibility-driven, P3): `wallet:locks:<pubkey>` — only while
/// `StakingService::staking_live()`. In this build staking is not live, so
/// the panel never subscribes and shows a compact ui::EmptyState that says
/// why (note chip COMING SOON); it never shows positions that are not on-chain.
/// No positions / feed error: the same EmptyState ("No active locks" /
/// "Locks unavailable" with RETRY).
///
/// Row context menu: Extend / Withdraw, both disabled with a "coming soon"
/// tooltip — the extend / withdraw transactions are not implemented.
class ActiveLocksPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ActiveLocksPanel(QWidget* parent = nullptr);
    ~ActiveLocksPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_theme();
    void retranslateUi();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_locks_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_row_context_menu(const QPoint& pos);

    void rebuild_table();
    /// The empty / not-live / error page, built on first need under the table.
    fincept::ui::EmptyState* ensure_empty_state();
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    /// Head pill: LIVE (staking live, feed healthy), UNAVAILABLE (feed error)
    /// or COMING SOON (staking not live in this build).
    void set_pill(const QString& text, bool highlighted);

    // Head
    QLabel* title_ = nullptr; // "ACTIVE LOCKS"
    QLabel* summary_label_ = nullptr;
    QLabel* status_pill_ = nullptr;

    // Body
    QTableWidget* table_ = nullptr;
    fincept::ui::EmptyState* empty_state_ = nullptr; ///< lazily built (ensure_empty_state)

    // Error strip
    QFrame* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;

    // State
    QString current_pubkey_;
    QString current_topic_;
    QVector<fincept::wallet::LockPosition> latest_;
    /// True after the locks feed reported an error and before the next good
    /// publish. Lets the empty state say "unavailable" instead of asserting
    /// the wallet has no locks (which the producer never established).
    bool feed_error_ = false;
};

} // namespace fincept::screens::panels
