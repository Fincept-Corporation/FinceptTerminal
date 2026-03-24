// src/screens/equity_research/EquityTalippTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QChartView>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>

namespace fincept::screens {

class EquityTalippTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityTalippTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  private slots:
    void on_category_clicked(const QString& cat_id);
    void on_indicator_changed(const QString& indicator);
    void on_compute_clicked();
    void on_talipp_result(QString indicator, QVector<double> values, QVector<qint64> timestamps);

  private:
    void build_ui();
    void rebuild_indicator_combo(const QString& cat_id);
    void rebuild_param_form(const QString& indicator);
    void rebuild_last_values(const QString& indicator, const QVector<double>& values);
    void rebuild_chart(const QString& indicator, const QVector<double>& values, const QVector<qint64>& timestamps);
    QVariantMap collect_params() const;

    QString current_symbol_;
    QString active_category_ = "trend";

    // Category tab buttons
    QMap<QString, QPushButton*> cat_buttons_;

    QComboBox* indicator_combo_ = nullptr;
    QWidget* param_widget_ = nullptr;
    QFormLayout* param_form_ = nullptr;
    QLabel* status_label_ = nullptr;
    QWidget* last_values_area_ = nullptr;
    QChartView* chart_view_ = nullptr;
    QWidget* results_widget_ = nullptr;
    QWidget* empty_widget_ = nullptr;
    QLabel* data_points_lbl_ = nullptr;
    QPushButton* compute_btn_ = nullptr;

    struct IndicatorDef {
        QString id;
        QString label;
        QString data_type; // "prices" | "ohlcv"
    };

    struct CategoryDef {
        QString id;
        QString label;
        QString color;
        QList<IndicatorDef> items;
    };

    static const QList<CategoryDef>& categories();
    static const QMap<QString, QList<QPair<QString, double>>>& param_defs();
    static QString cat_color(const QString& cat_id);

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
