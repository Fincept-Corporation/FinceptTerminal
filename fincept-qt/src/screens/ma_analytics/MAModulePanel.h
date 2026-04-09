// src/screens/ma_analytics/MAModulePanel.h
#pragma once
#include "services/ma_analytics/MAAnalyticsTypes.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Renders the input form + results area for a single M&A module.
/// Each module has sub-tabs (methods), input fields, a Run button, and result display.
class MAModulePanel : public QWidget {
    Q_OBJECT
  public:
    explicit MAModulePanel(const fincept::services::ma::ModuleInfo& info, QWidget* parent = nullptr);

  public slots:
    void refresh_theme();

  private slots:
    void on_result_ready(const QString& context, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    void connect_service();
    void apply_tab_stylesheet();

    // Module-specific builders
    QWidget* build_valuation_panel();
    QWidget* build_merger_panel();
    QWidget* build_deals_panel();
    QWidget* build_startup_panel();
    QWidget* build_fairness_panel();
    QWidget* build_industry_panel();
    QWidget* build_advanced_panel();
    QWidget* build_comparison_panel();

    // Helpers for building input forms
    QWidget* build_input_row(const QString& label, QWidget* input, QWidget* parent);
    QDoubleSpinBox* make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                     QWidget* parent);
    QSpinBox* make_int_spin(int min, int max, int val, QWidget* parent);
    QPushButton* make_run_button(const QString& text, QWidget* parent);

    // Result display
    void display_result(const QJsonObject& data);
    void display_error(const QString& msg);
    void clear_results();
    QWidget* build_metric_card(const QString& label, const QString& value, const QString& color, QWidget* parent);

    fincept::services::ma::ModuleInfo module_;
    QTabWidget* sub_tabs_ = nullptr;
    QWidget* header_bar_ = nullptr;
    QLabel* header_title_ = nullptr;
    QLabel* header_category_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QWidget* results_container_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Input field maps for gathering params
    QHash<QString, QDoubleSpinBox*> double_inputs_;
    QHash<QString, QSpinBox*> int_inputs_;
    QHash<QString, QComboBox*> combo_inputs_;
};

} // namespace fincept::screens
