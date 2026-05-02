#pragma once

#include <QString>
#include <QVariant>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;

namespace fincept::screens::panels {

/// TreasuryPanel — bottom section of the ROADMAP tab (Phase 5 §5.2).
///
/// Renders treasury reserves (USDC, SOL + USD value), runway months at
/// current burn, and a multi-sig label that deeplinks to Squads.
///
/// Subscribes (visibility-driven, P3):
///   - `treasury:reserves`  →  USDC / SOL / total / multisig label/url
///   - `treasury:runway`    →  months derived from reserves
///
/// When data is mocked the head pill reads "DEMO".
class TreasuryPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TreasuryPanel(QWidget* parent = nullptr);
    ~TreasuryPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_reserves_update(const QVariant& v);
    void on_runway_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_multisig_clicked();

    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void update_demo_chip();

    // Head
    QLabel* status_pill_ = nullptr;

    // Body
    QLabel* usdc_value_ = nullptr;
    QLabel* sol_value_ = nullptr;
    QLabel* total_usd_value_ = nullptr;
    QLabel* runway_value_ = nullptr;
    QPushButton* multisig_button_ = nullptr;

    // Error strip
    QWidget* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;

    // State
    QString multisig_url_;
    bool reserves_is_mock_ = false;
    bool runway_is_mock_ = false;
};

} // namespace fincept::screens::panels
