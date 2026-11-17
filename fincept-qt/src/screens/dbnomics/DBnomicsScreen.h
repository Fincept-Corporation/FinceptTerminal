// src/screens/dbnomics/DBnomicsScreen.h
#pragma once
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/dbnomics/DBnomicsModels.h"

#include <QComboBox>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

class DBnomicsSelectionPanel;
class DBnomicsChartWidget;
class DBnomicsDataTable;

/// DBnomics explorer: providers → datasets → series on the left, the SINGLE chart + observation
/// table or the COMPARE grid on the right.
///
/// Never blank: a first-ever open loads an example (IMF World Economic Outlook, US real GDP growth)
/// through the same provider → dataset → series path a click takes; a remembered state brings its
/// selection and charts back. A clicked series is shown at once as the chart's "preview" (ADD TO
/// SINGLE VIEW keeps it on the chart and the next click previews beside it); while nothing is on the
/// chart the SINGLE view shows a chart skeleton, the failure page (RETRY) or a designed empty state
/// (LOAD EXAMPLE SERIES). COMPARE opened with no slot starts slot 1 from the SINGLE view's series.
class DBnomicsScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit DBnomicsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "dbnomics"; }
    int state_version() const override { return 1; }

    /// ISectionLinked — "single" | "compare" (also "view:<id>"; "comparison" too): the SINGLE /
    /// COMPARE toggle (set_view_mode, its click path). Before the first show the choice replaces the
    /// remembered view mode. Nothing is fetched; focus is unused.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_providers_loaded(const QVector<services::DbnProvider>& providers);
    void on_datasets_loaded(const QString& provider_code, const QVector<services::DbnDataset>& datasets,
                            const services::DbnPagination& page, bool append);
    void on_series_loaded(const QString& provider_code, const QString& dataset_code, const QString& query,
                          const QVector<services::DbnSeriesInfo>& series, const services::DbnPagination& page,
                          bool append);
    void on_observations_loaded(const services::DbnDataPoint& data);
    void on_observations_failed(const QString& series_id, const QString& message);
    void on_search_results_loaded(const QString& query, const QVector<services::DbnSearchResult>& results,
                                  const services::DbnPagination& page, bool append);
    void on_service_error(const QString& context, const QString& message);
    void on_fetch_clicked();
    void on_refresh_clicked();
    void on_export_csv();
    void on_provider_selected(const QString& code);
    void on_dataset_selected(const QString& code);
    void on_series_selected(const QString& prov, const QString& ds, const QString& code);
    void on_chart_type_changed(int index);
    void on_add_to_single_view();
    void on_clear_all();
    void on_add_slot();
    void on_add_to_slot(int slot_index);
    void on_remove_from_slot(int slot_index, const QString& series_id);
    void on_remove_slot(int slot_index);

  private:
    void build_ui();
    QWidget* build_toolbar();
    void set_status(const QString& message);
    void render_single_view();
    void assign_series_colors();
    void rebuild_comparison_view();
    void retranslateUi();
    /// Series colours and everything drawn with them (charts, tables, slot legends), once the new
    /// appearance has settled.
    void apply_appearance();

    /// First show (one event-loop turn later, after restore_state()): the remembered state, else
    /// the example series.
    void first_show();
    void apply_restored_state(const QVariantMap& state);
    /// IMF / WEO:latest / USA.NGDP_RPCH.pcent_change through provider → dataset → series.
    void load_example_series();
    void set_view_mode(services::DbnViewMode mode);
    /// The SINGLE view's series: the ones kept (ADD TO SINGLE VIEW) and the clicked preview.
    QVector<services::DbnDataPoint> single_view_series() const;
    /// Chart + table, the skeleton, the failure page or the empty state.
    void update_single_page();

    // DataHub (CLAUDE.md D4): observations are read from `dbnomics:<provider>:<dataset>:<series>`
    // topics, not from the service's fetch_observations() signal. Catalogue browsing (providers /
    // datasets / series / search) stays on the service's one-shot callbacks.
    static QString series_topic(const QString& series_id); // "PROV/DS/CODE" -> topic ("" if malformed)
    void watch_series(const QString& series_id);           // subscribe (idempotent) + remember
    void unwatch_unused_series();                          // drop topics no view / slot / selection uses

    struct SlotCard {
        DBnomicsChartWidget* chart = nullptr;
        DBnomicsDataTable* table = nullptr;
        QComboBox* chart_combo = nullptr;
    };

    DBnomicsSelectionPanel* selection_panel_ = nullptr;
    DBnomicsChartWidget* chart_widget_ = nullptr;
    DBnomicsDataTable* data_table_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* stats_label_ = nullptr;
    QComboBox* chart_type_combo_ = nullptr;
    QPushButton* single_btn_ = nullptr;
    QPushButton* compare_btn_ = nullptr;

    // SINGLE page: content (chart + table) | skeleton | designed state (empty / failed)
    QStackedWidget* single_stack_ = nullptr;
    QWidget* single_content_ = nullptr;
    ui::LoadingSkeleton* single_skeleton_ = nullptr;
    ui::EmptyState* single_state_ = nullptr;
    bool single_failed_ = false;    // the pending selection failed with nothing else on the chart
    QString single_error_;
    QString failed_series_id_;
    bool failed_was_restore_ = false;
    bool selection_loading_ = false; // pending_series_id_ requested, not arrived yet

    // COMPARE page: slot grid | designed state (no slot and nothing to seed one with)
    QStackedWidget* compare_stack_ = nullptr;
    QWidget* compare_grid_page_ = nullptr;
    ui::EmptyState* compare_empty_ = nullptr;

    // Toolbar + toggle chrome (cached for retranslateUi)
    QLabel* toolbar_title_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* chart_type_label_ = nullptr;
    QLabel* attribution_label_ = nullptr;

    services::DbnViewMode view_mode_ = services::DbnViewMode::Single;
    services::DbnChartType single_chart_type_ = services::DbnChartType::Line;
    QVector<services::DbnDataPoint> single_series_; // kept on the chart (ADD TO SINGLE VIEW)
    services::DbnDataPoint preview_;                 // the last clicked series, until kept or replaced
    bool has_preview_ = false;
    QVector<services::DbnSlot> slots_;
    services::DbnDataPoint last_loaded_data_;
    bool has_pending_data_ = false;
    int provider_count_ = 0;

    QSet<QString> watched_series_; // series ids subscribed on the hub (re-subscribed on every show)
    QString pending_series_id_;    // the series the user just picked, until its observations arrive
    // Series ids saved with the views (restore_state) that have not arrived yet; re-added on arrival.
    QStringList restore_single_;
    QVector<QStringList> restore_slots_; // parallel to slots_

    bool first_show_done_ = false;
    QVariantMap pending_state_; // restore_state() before the first show: applied by first_show()

    QWidget* comparison_content_ = nullptr;
    QVBoxLayout* comparison_layout_ = nullptr;
    QVector<SlotCard> slot_cards_;
};

} // namespace fincept::screens
