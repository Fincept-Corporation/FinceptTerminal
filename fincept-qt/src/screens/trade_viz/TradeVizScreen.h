#pragma once
#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QList>
#include <QPointer>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Trade Flow visualization.
/// Chord diagram showing bilateral trade flows + partner ranking table.
class TradeVizScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit TradeVizScreen(QWidget* parent = nullptr);

    // IStatefulScreen — persists the filter combo selections
    // (country/order/period/year).
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "trade_viz"; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void setup_ui();
    void retranslateUi();

    // ── Sub-builders ─────────────────────────────────────────────────────────
    QWidget* build_tab_bar();
    QWidget* build_filter_bar();
    QWidget* build_partner_table();

    void populate_partner_table();
    void update_clock();

    // ── Widgets ──────────────────────────────────────────────────────────────
    QTableWidget* partner_table_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QComboBox* country_combo_ = nullptr;
    QComboBox* order_combo_ = nullptr;
    QComboBox* period_combo_ = nullptr;
    QComboBox* year_combo_ = nullptr;

    // Text-bearing widgets cached for retranslateUi.
    QList<QLabel*> tab_labels_;   // Table / Settings / Export / Notes
    QLabel* flow_title_ = nullptr;
    QLabel* browse_label_ = nullptr;
    QLabel* order_caption_ = nullptr;
    QLabel* period_caption_ = nullptr;
    QPointer<QWidget> chord_widget_; // chord diagram (painter text re-rendered on update())

    // ── Timers ───────────────────────────────────────────────────────────────
    QTimer* clock_timer_ = nullptr;
};

} // namespace fincept::screens
