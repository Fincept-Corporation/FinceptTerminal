#pragma once

#include <QString>
#include <QVariant>
#include <QWidget>

class QButtonGroup;
class QFrame;
class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;

namespace fincept::screens::panels {
class HoldingsTable;
}

namespace fincept::screens {

/// HOME tab inside the Crypto Center workspace.
/// Phase 1's three panels (Wallet · Balance · Roadmap) repackaged into a
/// stand-alone tab. Subscribes to `wallet:balance:<pubkey>` and
/// `market:price:fncpt` independently of HoldingsBar — DataHub fans out a
/// single producer publish to both consumers, no double-fetch.
///
/// What lives here vs HoldingsBar:
///   - HoldingsBar       — identity-level: SOL · $FNCPT · USD · feed pill
///   - HomeTab           — panel-level detail, wallet identity rows, mode
///                         toggle (mirrored from Settings), REFRESH button,
///                         tab-local error strip, roadmap text.
class HomeTab : public QWidget {
    Q_OBJECT
  public:
    explicit HomeTab(QWidget* parent = nullptr);
    ~HomeTab() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_topic_error(const QString& topic, const QString& error);
    void on_mode_changed(bool is_stream);

    void clear_error_strip();
    void show_error_strip(const QString& msg);
    void apply_mode_to_buttons(bool is_stream);

  signals:
    /// Forwarded from the embedded HoldingsTable when the user clicks a row.
    /// CryptoCenterScreen wires this to SwapPanel's FROM combo.
    void select_token(QString mint);

  private:
    // Wallet panel
    QFrame* wallet_panel_ = nullptr;
    QLabel* row_label_value_ = nullptr;
    QLabel* row_pubkey_value_ = nullptr;
    QLabel* row_connected_value_ = nullptr;
    QPushButton* copy_button_ = nullptr;
    QPushButton* disconnect_button_ = nullptr;

    // Holdings panel — replaces the three stat boxes (Stage 2A.5).
    QFrame* balance_panel_ = nullptr;
    QButtonGroup* mode_group_ = nullptr;
    QPushButton* mode_poll_button_ = nullptr;
    QPushButton* mode_stream_button_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
    QFrame* error_strip_ = nullptr;
    QLabel* error_strip_text_ = nullptr;
    panels::HoldingsTable* holdings_table_ = nullptr;

    // Roadmap panel
    QFrame* roadmap_panel_ = nullptr;
    QLabel* roadmap_body_ = nullptr;

    QString current_pubkey_;
};

} // namespace fincept::screens
