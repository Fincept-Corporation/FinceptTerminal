#pragma once

#include <QEvent>
#include <QString>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

namespace fincept::screens {

/// SETTINGS tab — Phase 2 lands the configuration that previously lived
/// inline on the HOME panel.
///
/// Sections:
///   - Balance refresh mode (POLL / STREAM, mirrored on HOME)
///   - Helius API key entry (writes to SecureStorage `solana.helius_api_key`)
///   - Default slippage (writes to SecureStorage `wallet.default_slippage_bps`)
///
/// No data subscriptions; all reads/writes go through SecureStorage and
/// WalletService. Safe to construct eagerly.
class SettingsTab : public QWidget {
    Q_OBJECT
  public:
    explicit SettingsTab(QWidget* parent = nullptr);
    ~SettingsTab() override;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_theme();
    void retranslateUi();

    void on_mode_changed(bool is_stream);
    void on_save_helius_key();
    void on_clear_helius_key();
    void on_slippage_changed(int bps);
    void on_show_unverified_toggled(bool checked);

    void apply_mode_to_buttons(bool is_stream);
    void load_initial_values();

    QButtonGroup* mode_group_ = nullptr;
    QPushButton* mode_poll_button_ = nullptr;
    QPushButton* mode_stream_button_ = nullptr;

    QLineEdit* helius_input_ = nullptr;
    QPushButton* helius_save_button_ = nullptr;
    QPushButton* helius_clear_button_ = nullptr;
    QLabel* helius_status_ = nullptr;

    QSlider* slippage_slider_ = nullptr;
    QLabel* slippage_value_ = nullptr;

    QCheckBox* show_unverified_checkbox_ = nullptr;

    // Fixed UI text (cached for retranslateUi)
    QLabel* balance_panel_title_ = nullptr;
    QLabel* balance_panel_sub_ = nullptr;
    QLabel* balance_hint_ = nullptr;
    QLabel* helius_panel_title_ = nullptr;
    QLabel* helius_panel_sub_ = nullptr;
    QLabel* helius_hint_ = nullptr;
    QLabel* slippage_panel_title_ = nullptr;
    QLabel* slippage_panel_sub_ = nullptr;
    QLabel* slippage_hint_ = nullptr;
    QLabel* filters_panel_title_ = nullptr;
    QLabel* filters_panel_sub_ = nullptr;
    QLabel* filters_hint_ = nullptr;
};

} // namespace fincept::screens
