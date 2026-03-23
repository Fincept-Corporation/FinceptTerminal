// src/screens/dbnomics/DBnomicsSelectionPanel.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class DBnomicsSelectionPanel : public QWidget {
    Q_OBJECT
  public:
    explicit DBnomicsSelectionPanel(QWidget* parent = nullptr);

    void populate_providers(const QVector<services::DbnProvider>& providers);
    void populate_datasets(const QVector<services::DbnDataset>& datasets, const services::DbnPagination& page,
                           bool append);
    void populate_series(const QVector<services::DbnSeriesInfo>& series, const services::DbnPagination& page,
                         bool append);
    void populate_search_results(const QVector<services::DbnSearchResult>& results, const services::DbnPagination& page,
                                 bool append);
    void set_loading(bool loading);
    void set_status(const QString& message);
    void update_slot_series(int slot_index, const QVector<services::DbnDataPoint>& series);

    QString selected_provider() const { return selected_provider_; }
    QString selected_dataset() const { return selected_dataset_; }
    QString selected_series() const { return selected_series_; }

  signals:
    void provider_selected(const QString& code);
    void dataset_selected(const QString& code);
    void series_selected(const QString& provider, const QString& dataset, const QString& series);
    void load_more_datasets_requested(int next_offset);
    void load_more_series_requested(int next_offset);
    void load_more_search_requested(int next_offset);
    void global_search_changed(const QString& query);
    void series_search_changed(const QString& provider, const QString& dataset, const QString& query);
    void add_to_single_view_clicked();
    void clear_all_clicked();
    void add_slot_clicked();
    void add_to_slot_clicked(int slot_index);
    void remove_from_slot_clicked(int slot_index, const QString& series_id);
    void remove_slot_clicked(int slot_index);
    void search_result_selected(const QString& provider_code, const QString& dataset_code);

  public:
    void set_providers_loading(bool on);
    void set_datasets_loading(bool on);
    void set_series_loading(bool on);
    void set_search_loading(bool on);

  public slots:
    void add_comparison_slot();
    void remove_comparison_slot(int index);
    void clear_slots();

  private:
    void build_ui();
    void tick_anim();
    QWidget* build_search_section();
    QWidget* build_provider_section();
    QWidget* build_dataset_section();
    QWidget* build_series_section();
    QWidget* build_action_buttons();
    QWidget* build_comparison_slots_section();
    QLabel* make_section_label(const QString& text);
    QListWidget* make_styled_list(int fixed_height);
    QPushButton* make_load_more_button();

    QLineEdit* global_search_input_ = nullptr;
    QListWidget* search_results_list_ = nullptr;
    QPushButton* search_load_more_btn_ = nullptr;
    QLineEdit* provider_filter_input_ = nullptr;
    QListWidget* provider_list_ = nullptr;
    QListWidget* dataset_list_ = nullptr;
    QPushButton* dataset_load_more_btn_ = nullptr;
    QLineEdit* series_search_input_ = nullptr;
    QListWidget* series_list_ = nullptr;
    QPushButton* series_load_more_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QVBoxLayout* slots_layout_ = nullptr;
    int slot_count_ = 0;

    QString selected_provider_;
    QString selected_dataset_;
    QString selected_series_;

    int datasets_next_offset_ = 0;
    int series_next_offset_ = 0;
    int search_next_offset_ = 0;

    QVector<services::DbnProvider> all_providers_;

    // ── Loading state ──────────────────────────────────────────────────────
    QWidget* prov_content_ = nullptr;
    QWidget* ds_content_ = nullptr;
    QWidget* series_content_ = nullptr;
    QWidget* search_content_ = nullptr;
    QLabel* prov_spin_ = nullptr;
    QLabel* ds_spin_ = nullptr;
    QLabel* series_spin_ = nullptr;
    QLabel* search_spin_ = nullptr;
    QTimer* anim_timer_ = nullptr;
    int anim_frame_ = 0;
    bool prov_loading_ = false;
    bool ds_loading_ = false;
    bool series_loading_ = false;
    bool search_loading_ = false;
};

} // namespace fincept::screens
