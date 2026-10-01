#pragma once
#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QEvent>
#include <QHash>
#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QString>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVector>
#include <QWidget>

namespace fincept::services {
struct EconomicsResult;
}

namespace fincept::screens {

/// One bilateral trade partner of the selected reporting country. Values are USD millions.
struct TradePartner {
    QString name;
    QString code;         // short label drawn on the chord (ISO3 for live data)
    double imports = 0.0; // reporter's imports FROM this partner
    double exports = 0.0; // reporter's exports TO this partner
    double total() const { return imports + exports; }
    double balance() const { return exports - imports; }
};

/// Trade Flow visualization.
/// Chord diagram showing bilateral trade flows + partner ranking table. Data is the
/// reporter's annual merchandise trade by partner from UN Comtrade (via
/// EconomicsService); a small static US snapshot is shown only until/unless the
/// live fetch for that selection is unavailable.
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

    // ── Data ─────────────────────────────────────────────────────────────────
    // Fetch the selected country/year (both flows) — a no-op when that dataset is
    // already shown or in flight. Cache-aware: EconomicsService answers repeats locally.
    void refresh_data();
    void on_econ_result(const QString& request_id, const fincept::services::EconomicsResult& result);
    // Show `all` (any order) as the current dataset and repaint chord + table.
    void set_dataset(const QVector<TradePartner>& all, const QString& note);
    // No dataset to draw (loading / error / empty): clear both views and tell the user why.
    void show_message(const QString& note, const QString& message);
    // Static US snapshot, used only when live data for the US/2024 selection is unavailable.
    void show_static_fallback(const QString& why);
    // Re-sort the current dataset by the "Order by" filter and repaint chord + table.
    void rebuild_views();
    void export_csv();
    void update_clock();

    // ── Widgets ──────────────────────────────────────────────────────────────
    QTableWidget* partner_table_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QComboBox* country_combo_ = nullptr;
    QComboBox* order_combo_ = nullptr;
    QComboBox* period_combo_ = nullptr;
    QComboBox* year_combo_ = nullptr;
    QToolButton* prev_year_btn_ = nullptr;
    QToolButton* next_year_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Text-bearing widgets cached for retranslateUi.
    QList<QLabel*> tab_labels_; // Table (the active view)
    QLabel* flow_title_ = nullptr;
    QLabel* browse_label_ = nullptr;
    QLabel* order_caption_ = nullptr;
    QLabel* period_caption_ = nullptr;
    QPointer<QWidget> chord_widget_; // chord diagram (painter text re-rendered on update())

    // ── Dataset state ────────────────────────────────────────────────────────
    QVector<TradePartner> partners_;       // current dataset, all partners (views show the top 15)
    QVector<TradePartner> shown_;          // partners_ sorted by "Order by", capped at 15 — what is drawn
    QString shown_key_;                    // "<un code>_<year>" the dataset belongs to
    QString loading_key_;                  // key of the request in flight (empty when idle)
    QHash<QString, QJsonArray> flow_rows_; // "X" / "M" rows received for loading_key_
    QString dataset_note_;                 // provenance line for the chord widget
    QString empty_message_;                // chord centre text while partners_ is empty

    // ── Timers ───────────────────────────────────────────────────────────────
    QTimer* clock_timer_ = nullptr;
};

} // namespace fincept::screens
