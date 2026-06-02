#pragma once

#include <QEvent>
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
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_theme();
    void retranslateUi();

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

    // Section titles + fixed captions (cached for retranslateUi)
    QLabel* epoch_section_title_ = nullptr;   // "THIS EPOCH"
    QLabel* alltime_section_title_ = nullptr; // "ALL-TIME"
    QLabel* revenue_caption_ = nullptr;       // "REVENUE"
    QLabel* buyback_caption_ = nullptr;       // "BUYBACK (50%)"
    QLabel* staker_caption_ = nullptr;        // "STAKER YIELD (25%)"
    QLabel* treasury_caption_ = nullptr;      // "TREASURY TOPUP (25%)"
    QLabel* bought_caption_ = nullptr;        // "$FNCPT BOUGHT"
    QLabel* burned_caption_ = nullptr;        // "$FNCPT BURNED"
    QLabel* burn_tx_caption_ = nullptr;       // "BURN TX"
    QLabel* total_burned_caption_ = nullptr;  // "BURNED"
    QLabel* supply_caption_ = nullptr;        // "SUPPLY REMAINING"
    QLabel* spent_caption_ = nullptr;         // "SPENT ON BUYBACK"

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
