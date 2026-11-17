// src/screens/dbnomics/DBnomicsDataTable.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QEvent>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

/// The observation grid under a DBnomics chart: the latest 50 periods, one column per series, each
/// value with its direction against the series' previous observation (▲ / ▼ and the gain / loss
/// colour). Loading is a table-shaped ui::LoadingSkeleton, nothing to show a compact ui::EmptyState.
class DBnomicsDataTable : public QWidget {
    Q_OBJECT
  public:
    explicit DBnomicsDataTable(QWidget* parent = nullptr);

    void set_data(const QVector<services::DbnDataPoint>& series);
    void clear();
    void set_loading(bool on);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    /// Row height for the font in effect (on appearance_settled).
    void apply_appearance();

    QStackedWidget* stack_ = nullptr;
    QLabel* header_label_ = nullptr; // (cached for retranslateUi)
    ui::LoadingSkeleton* skeleton_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVector<services::DbnDataPoint> series_; // what the table shows (re-rendered for a new theme)
};

} // namespace fincept::screens
