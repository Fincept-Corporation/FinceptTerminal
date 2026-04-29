#pragma once

#include "core/result/Result.h"

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
/// Flow:
///   1. User edits the YOU PAY amount. The panel computes the **estimated
///      output** locally from the live `market:price:fncpt` spot times the
///      slippage budget. There is no quote-API round-trip — PumpPortal has
///      no quote endpoint, only "build me a tx now."
///   2. User clicks SWAP → SwapPanel calls
///      `PumpFunSwapService::build_swap(action, mint, amount, …)` to
///      materialise the unsigned versioned tx body, then constructs a
///      `WalletActionConfirmDialog`. On Confirm, it forwards to
///      `WalletService::sign_and_send` and polls `getSignatureStatuses`
///      until `confirmed`.
///
/// Refuses to enable SWAP if there's no spot price yet or no wallet is
/// connected. All visual error reporting goes through the panel's own
/// error strip, not stderr or LOG_*.
class SwapPanel : public QWidget {
    Q_OBJECT
  public:
    explicit SwapPanel(QWidget* parent = nullptr);
    ~SwapPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    enum class Mode { BuyFncpt, SellFncpt };

    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_balance_update(const QVariant& v);
    void on_price_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_amount_changed(const QString& s);
    void on_mode_changed(int idx);
    void on_max_clicked();
    void on_swap_clicked();

    void update_inputs_from_balance();
    void recompute_estimate();
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void set_busy(bool busy);
    bool can_submit() const;
    int slippage_pct() const;
    int slippage_bps() const;

    // UI
    QComboBox* mode_combo_ = nullptr; // BUY $FNCPT / SELL $FNCPT
    QLineEdit* amount_input_ = nullptr;
    QPushButton* max_button_ = nullptr;
    QLabel* in_token_label_ = nullptr;
    QLabel* in_balance_label_ = nullptr;
    QLabel* out_amount_label_ = nullptr;
    QLabel* out_token_label_ = nullptr;
    QLabel* route_label_ = nullptr;
    QLabel* impact_label_ = nullptr;
    QLabel* slippage_label_ = nullptr;
    QFrame* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;
    QPushButton* swap_button_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Debounce timer for amount input → quote topic switch.
    QTimer* debounce_timer_ = nullptr;

    // State
    QString current_pubkey_;
    QString current_balance_topic_;
    Mode mode_ = Mode::BuyFncpt;
    double sol_balance_ = 0.0;
    double fncpt_balance_ = 0.0;
    int fncpt_decimals_ = 6;        // pump.fun default; refreshed on balance publish
    double last_fncpt_usd_price_ = 0.0;  // refreshed on market:price:fncpt publish
    double last_fncpt_sol_price_ = 0.0;  // FNCPT priced in SOL
    bool busy_ = false;
};

} // namespace fincept::screens::panels
