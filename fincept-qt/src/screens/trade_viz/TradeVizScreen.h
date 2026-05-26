#pragma once
#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QHideEvent>
#include <QLabel>
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

  private:
    void setup_ui();

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

    // ── Timers ───────────────────────────────────────────────────────────────
    QTimer* clock_timer_ = nullptr;
};

} // namespace fincept::screens
