#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QWidget>

namespace fincept::screens {

/// QuantLib module descriptor.
struct QuantModule {
    QString id;
    QString name;
    int endpoint_count;
    QStringList panels; // sub-panel names
};

/// QuantLib Suite — 18 quantitative analysis modules with 590+ endpoints.
/// Modules: Core, Analysis, Curves, Economics, Instruments, ML, Models,
/// Numerical, Physics, Portfolio, Pricing, Regulatory, Risk, Scheduling,
/// Solver, Statistics, Stochastic, Volatility.
/// All computations via api.fincept.in REST API.
class QuantLibScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit QuantLibScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "quantlib"; }
    int state_version() const override { return 1; }

  private slots:
    void on_module_changed(int index);
    void on_panel_changed(QListWidgetItem* item);
    void on_endpoint_changed(int index);
    void on_execute();

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_sidebar();
    QWidget* create_center_panel();
    QWidget* create_right_panel();
    QWidget* create_status_bar();

    void populate_panels(int module_index);
    void execute_api(const QString& endpoint, const QJsonObject& params);
    void display_result(const QJsonObject& result);
    void display_result_array(const QJsonArray& arr);
    void display_error(const QString& error);
    void set_loading(bool loading);

    // Endpoint example bodies (auto-filled on selection)
    static const QHash<QString, QString>& endpoint_examples();

    // Modules
    QList<QuantModule> modules_;
    int active_module_ = 0;
    QString active_panel_;
    bool loading_ = false;

    // Sidebar
    QTreeWidget* module_tree_ = nullptr;

    // Center
    QLabel* center_title_ = nullptr;
    QComboBox* endpoint_combo_ = nullptr;
    QLineEdit* param_input1_ = nullptr;
    QLineEdit* param_input2_ = nullptr;
    QLineEdit* param_input3_ = nullptr;
    QLineEdit* param_input4_ = nullptr;
    QPushButton* exec_btn_ = nullptr;

    // Right (results)
    QTextEdit* result_view_ = nullptr;
    QTableWidget* result_table_ = nullptr;
    QStackedWidget* result_stack_ = nullptr;
    QLabel* result_status_ = nullptr;

    // Status
    QLabel* status_module_ = nullptr;
    QLabel* status_panel_ = nullptr;
    QLabel* status_endpoint_ = nullptr;
};

} // namespace fincept::screens
