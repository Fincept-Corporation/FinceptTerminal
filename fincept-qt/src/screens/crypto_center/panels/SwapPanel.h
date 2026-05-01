#pragma once

#include "core/result/Result.h"
#include "services/wallet/WalletTypes.h"

#include <QHash>
#include <QString>
#include <QVariant>
#include <QWidget>

class QComboBox;
class QFrame;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTimer;

namespace fincept::screens::panels {

/// SWAP panel inside the TRADE tab. Phase 2 rev 2 — PumpPortal-backed.
///
/// Stage 2A.5.10 generalised the FROM/TO selectors to combos populated from
/// real holdings. PumpPortal `trade-local` only routes SOL ↔ $FNCPT (it's the
/// issuer's own endpoint, not a generalised router), so the SWAP button is
/// gated to those two pairs in Phase 2. Selecting any other FROM mint shows
/// a "Phase 3 supports arbitrary pairs" message and disables submit. The
/// combo plumbing is forward-compatible: when Stage 2C lifts the constraint,
/// no UI rework is needed.
///
/// Flow:
///   1. User picks FROM and TO from combos populated by `wallet:balance:<pk>`
///      and the Jupiter verified list (TO only). Default selection mirrors a
///      BUY $FNCPT direction (FROM=SOL, TO=$FNCPT).
///   2. User edits the YOU PAY amount. The panel computes estimated output
///      locally from `market:price:token:<mint>` for both legs, times the
///      slippage budget. PumpPortal has no quote endpoint.
///   3. User clicks SWAP → `PumpFunSwapService::build_swap(...)` returns the
///      unsigned tx → `WalletActionConfirmDialog` → `sign_and_send` → poll
///      `getSignatureStatuses` until confirmed.
///
/// Public `set_from_mint(mint)` lets `HomeTab::select_token` pre-fill the
/// FROM combo when the user clicks a row in the holdings table.
class SwapPanel : public QWidget {
    Q_OBJECT
  public:
    explicit SwapPanel(QWidget* parent = nullptr);
    ~SwapPanel() override;

    /// Pre-fill the FROM combo with the given mint. If the mint isn't in the
    /// FROM combo yet (e.g. holdings haven't published), it's queued and
    /// applied as soon as the next `wallet:balance:<pk>` publish lands.
    /// No-op if `mint` is empty.
    void set_from_mint(const QString& mint);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_balance_update(const QVariant& v);
    void on_price_update(const QString& mint, const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_amount_changed(const QString& s);
    void on_from_changed(int idx);
    void on_to_changed(int idx);
    void on_max_clicked();
    void on_swap_clicked();

    void rebuild_from_combo();
    void rebuild_to_combo();
    void resubscribe_prices();
    void update_balance_label();
    void recompute_estimate();
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void set_busy(bool busy);
    bool can_submit() const;
    bool is_supported_pair() const;
    QString unsupported_pair_message() const;
    int slippage_pct() const;
    int slippage_bps() const;

    /// Poll `getSignatureStatuses` after `sign_and_send` until the tx is
    /// confirmed, errored, or the attempt cap is reached. Owned by a parented
    /// QTimer; auto-cleans on destruction.
    void start_status_poll(const QString& sig);

    /// Find a holding by mint in `latest_balance_.tokens`, returning nullopt
    /// if not held. Native SOL (sol_lamports) is surfaced as a synthetic
    /// `TokenHolding` for the wrapped-SOL mint.
    const fincept::wallet::TokenHolding* holding_for(const QString& mint) const;

    /// Native-SOL holding wrapped as a TokenHolding for combo display.
    /// Stored on the panel so we can return a stable pointer from holding_for.
    mutable fincept::wallet::TokenHolding sol_holding_;

    // UI
    QComboBox* from_combo_ = nullptr;
    QComboBox* to_combo_ = nullptr;
    QLineEdit* amount_input_ = nullptr;
    QPushButton* max_button_ = nullptr;
    QLabel* in_balance_label_ = nullptr;
    QLabel* out_amount_label_ = nullptr;
    QLabel* route_label_ = nullptr;
    QLabel* impact_label_ = nullptr;
    QLabel* slippage_label_ = nullptr;
    QFrame* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;
    QPushButton* swap_button_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Debounce for estimate recompute on every keystroke.
    QTimer* debounce_timer_ = nullptr;

    // State
    QString current_pubkey_;
    QString current_balance_topic_;
    fincept::wallet::WalletBalance latest_balance_;
    QString from_mint_;
    QString to_mint_;
    /// Set when a caller (HomeTab::select_token) wants the FROM combo
    /// switched to a specific mint that may not be present in the combo yet.
    /// Cleared once applied.
    QString pending_from_mint_;
    QHash<QString, double> price_usd_;     ///< mint → USD per 1 token
    QHash<QString, double> price_sol_;     ///< mint → SOL per 1 token
    QHash<QString, QString> price_topic_;  ///< mint → topic (for unsubscribe)
    bool busy_ = false;
};

} // namespace fincept::screens::panels
