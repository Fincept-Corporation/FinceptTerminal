#pragma once

#include "screens/crypto_center/WalletActionSummary.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace fincept::screens {

/// Modal "are you sure" dialog shown before any fund-moving wallet action.
///
/// Security-relevant behaviour:
///   - Renders every row of `WalletActionSummary` verbatim. The terminal
///     decodes the action **here**, not in the wallet — even users who blindly
///     approve in Phantom must see what they're signing first.
///   - The primary button is **disabled** for `summary.arm_delay_ms` after
///     show, with a visible countdown ("CONFIRM in 1.5s"). Resists muscle
///     memory and accidental double-clicks.
///   - Cancel always available. The cancel path emits `cancelled()` so the
///     caller can clean up any held-open bridge tokens.
///
/// UI compliance:
///   - Single global stylesheet keyed by `objectName` (Phase 1.5 §1.5.3 #1).
///   - No QTimer in constructor — `arm_timer_` only starts after `exec()`
///     fires `showEvent`.
class WalletActionConfirmDialog : public QDialog {
    Q_OBJECT
  public:
    explicit WalletActionConfirmDialog(WalletActionSummary summary,
                                       QWidget* parent = nullptr);
    ~WalletActionConfirmDialog() override;

    /// Convenience: show modally, return true on confirm, false on cancel /
    /// reject / close. Equivalent to `exec() == QDialog::Accepted`.
    bool prompt();

  signals:
    void confirmed();
    void cancelled();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();
    void apply_theme();
    void on_arm_tick();

    WalletActionSummary summary_;

    QLabel* title_label_ = nullptr;
    QLabel* lede_label_ = nullptr;
    QVBoxLayout* rows_layout_ = nullptr;
    QVBoxLayout* warnings_layout_ = nullptr;
    QPushButton* primary_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;

    QTimer* arm_timer_ = nullptr;
    int arm_remaining_ms_ = 0;
};

} // namespace fincept::screens
