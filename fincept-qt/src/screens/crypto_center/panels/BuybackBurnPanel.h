#pragma once

#include <QString>
#include <QVariant>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;

namespace fincept::screens::panels {

/// BuybackBurnPanel — top section of the ROADMAP tab (Phase 5 §5.2).
///
/// Renders THIS EPOCH (revenue split, buyback, $FNCPT bought, $FNCPT burned
/// with Solscan-clickable signature) and ALL-TIME (cumulative burn, supply
/// remaining, total spent on buyback).
///
/// Subscribes (visibility-driven, P3):
///   - `treasury:buyback_epoch`  →  current epoch summary
///   - `treasury:burn_total`     →  all-time totals
///
/// Both topics are terminal-wide (no <pubkey> segment) — the same numbers
/// are shown to every user. When data is mocked (the worker endpoint isn't
/// configured), the head status pill reads "DEMO" so the user knows.
class BuybackBurnPanel : public QWidget {
    Q_OBJECT
  public:
    explicit BuybackBurnPanel(QWidget* parent = nullptr);
    ~BuybackBurnPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_epoch_update(const QVariant& v);
    void on_burn_total_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_burn_signature_clicked();

    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void update_demo_chip(bool any_mock);

    // Head
    QLabel* title_ = nullptr;
    QLabel* status_pill_ = nullptr;

    // THIS EPOCH
    QLabel* epoch_window_ = nullptr;
    QLabel* revenue_total_ = nullptr;
    QLabel* revenue_split_ = nullptr;
    QLabel* buyback_usd_ = nullptr;
    QLabel* staker_yield_usd_ = nullptr;
    QLabel* treasury_topup_usd_ = nullptr;
    QLabel* fncpt_bought_ = nullptr;
    QLabel* fncpt_burned_ = nullptr;
    QPushButton* burn_signature_link_ = nullptr;

    // ALL-TIME
    QLabel* total_burned_ = nullptr;
    QLabel* supply_remaining_ = nullptr;
    QLabel* spent_on_buyback_ = nullptr;

    // Error strip
    QWidget* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;

    // State
    QString current_burn_signature_;
    bool epoch_is_mock_ = false;
    bool burn_total_is_mock_ = false;
};

} // namespace fincept::screens::panels
