// src/screens/geopolitics/HDXDataPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// HDX Humanitarian Data Exchange panel — search, browse, and explore datasets.
class HDXDataPanel : public QWidget {
    Q_OBJECT
  public:
    explicit HDXDataPanel(QWidget* parent = nullptr);

  private slots:
    void on_hdx_results(const QString& context,
                        QVector<fincept::services::geo::HDXDataset> datasets);
    void on_view_changed(int index);

  private:
    void build_ui();
    void connect_service();
    void populate_table(const QVector<fincept::services::geo::HDXDataset>& datasets);

    // Views
    QStackedWidget* view_stack_ = nullptr;
    QVector<QPushButton*> view_buttons_;
    int active_view_ = 0;

    // Search
    QLineEdit* search_edit_ = nullptr;

    // Datasets table
    QTableWidget* datasets_table_ = nullptr;

    // Explorer filters
    QComboBox* country_combo_ = nullptr;
    QComboBox* topic_combo_ = nullptr;

    // Stats
    QLabel* dataset_count_ = nullptr;
};

} // namespace fincept::screens
