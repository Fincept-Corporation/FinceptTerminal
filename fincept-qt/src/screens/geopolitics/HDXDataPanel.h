// src/screens/geopolitics/HDXDataPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// HDX Humanitarian Data Exchange panel — search, browse, and explore datasets.
class HDXDataPanel : public QWidget {
    Q_OBJECT
  public:
    explicit HDXDataPanel(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_hdx_results(const QString& context, QVector<fincept::services::geo::HDXDataset> datasets);
    void on_error(const QString& context, const QString& message);
    void on_view_changed(int index);

  private:
    void build_ui();
    void connect_service();
    void populate_table(const QVector<fincept::services::geo::HDXDataset>& datasets);
    void show_loading(bool on);
    void retranslateUi();

    // View tab buttons
    QVector<QPushButton*> view_buttons_;
    int active_view_ = 0;

    // Search
    QLineEdit* search_edit_ = nullptr;

    // Datasets table + loading overlay
    QTableWidget* datasets_table_ = nullptr;
    QLabel* loading_label_ = nullptr;

    // Explorer filter bar (shown only on Explorer tab)
    QWidget* explorer_bar_ = nullptr;
    QComboBox* country_combo_ = nullptr;
    QComboBox* topic_combo_ = nullptr;

    // Stats badge
    QLabel* dataset_count_ = nullptr;

    // Static text widgets (cached for retranslateUi)
    QLabel* title_lbl_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLabel* topic_lbl_ = nullptr;
    QPushButton* explore_btn_ = nullptr;
    // Fixed English view-tab labels, re-applied in retranslateUi (upper-cased).
    QStringList view_labels_;

    // Per-view result cache (avoids re-fetching on tab switch)
    QVector<fincept::services::geo::HDXDataset> cache_conflicts_;
    QVector<fincept::services::geo::HDXDataset> cache_humanitarian_;
    QVector<fincept::services::geo::HDXDataset> cache_datasets_;
};

} // namespace fincept::screens
