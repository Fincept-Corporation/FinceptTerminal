// src/screens/dbnomics/DBnomicsScreen.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QComboBox>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class DBnomicsSelectionPanel;
class DBnomicsChartWidget;
class DBnomicsDataTable;

class DBnomicsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit DBnomicsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_providers_loaded(const QVector<services::DbnProvider>& providers);
    void on_datasets_loaded(const QVector<services::DbnDataset>& datasets, const services::DbnPagination& page,
                            bool append);
    void on_series_loaded(const QVector<services::DbnSeriesInfo>& series, const services::DbnPagination& page,
                          bool append);
    void on_observations_loaded(const services::DbnDataPoint& data);
    void on_search_results_loaded(const QVector<services::DbnSearchResult>& results,
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

    services::DbnViewMode view_mode_ = services::DbnViewMode::Single;
    services::DbnChartType single_chart_type_ = services::DbnChartType::Line;
    QVector<services::DbnDataPoint> single_series_;
    QVector<services::DbnSlot> slots_;
    services::DbnDataPoint last_loaded_data_;
    bool has_pending_data_ = false;
    int provider_count_ = 0;

    QWidget* comparison_content_ = nullptr;
    QVBoxLayout* comparison_layout_ = nullptr;
    QVector<SlotCard> slot_cards_;
};

} // namespace fincept::screens
