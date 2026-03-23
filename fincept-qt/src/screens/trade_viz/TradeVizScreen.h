#pragma once
#include <QComboBox>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Bloomberg ECTR-style Trade Flow visualization.
/// Chord diagram showing bilateral trade flows + partner ranking table.
class TradeVizScreen : public QWidget {
    Q_OBJECT
  public:
    explicit TradeVizScreen(QWidget* parent = nullptr);

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
    QLabel*       clock_label_   = nullptr;
    QComboBox*    country_combo_ = nullptr;
    QComboBox*    order_combo_   = nullptr;
    QComboBox*    period_combo_  = nullptr;
    QComboBox*    year_combo_    = nullptr;

    // ── Timers ───────────────────────────────────────────────────────────────
    QTimer* clock_timer_ = nullptr;
};

} // namespace fincept::screens
