#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
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
    /// Number of REST endpoints exposed by this module. Populated from the
    /// module_endpoints() catalog at construction time -- the literal in
    /// build_modules() is only a fallback.
    int endpoint_count;
    QStringList panels; // sub-panel names
};

/// QuantLib Suite -- 18 quantitative analysis modules.
/// Modules: Core, Analysis, Curves, Economics, Instruments, ML, Models,
/// Numerical, Physics, Portfolio, Pricing, Regulatory, Risk, Scheduling,
/// Solver, Statistics, Stochastic, Volatility.
/// Endpoint counts are derived from the catalog in QuantLibScreen_Data.cpp.
/// All computations via api.fincept.in REST API.
class QuantLibScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit QuantLibScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "quantlib"; }
    int state_version() const override { return 1; }

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_module_changed(int index);
    void on_endpoint_changed(int index);
    void on_execute();

  private:
    void setup_ui();
    void retranslateUi();
    QWidget* create_header();
    QWidget* create_sidebar();
    QWidget* create_center_panel();
    QWidget* create_right_panel();
    QWidget* create_status_bar();

    /// Refill endpoint_combo_ for the active module, restricted to the active
    /// sub-panel when that panel maps onto a recognisable path segment.
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

    // Fixed chrome labels (cached for retranslateUi)
    QLabel* header_title_ = nullptr;
    QLabel* header_sub_ = nullptr;
    QLabel* header_badge_ = nullptr;
    QLabel* sidebar_title_ = nullptr;
    QLabel* endpoint_panel_title_ = nullptr;
    QLabel* json_body_label_ = nullptr;
    QLabel* results_title_ = nullptr;
    QLabel* status_left_ = nullptr;

    // Sidebar
    QTreeWidget* module_tree_ = nullptr;

    // Center
    QLabel* center_title_ = nullptr;
    QComboBox* endpoint_combo_ = nullptr;
    QLineEdit* param_input1_ = nullptr;
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
