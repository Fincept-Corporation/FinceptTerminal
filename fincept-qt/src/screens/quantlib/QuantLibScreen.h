#pragma once

#include "screens/common/IStatefulScreen.h"
#include "services/quantlib/QuantLibAccess.h"

#include <QComboBox>
#include <QEvent>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QWidget>

namespace fincept::screens {

class QuantLibParamForm;
class QuantLibResultView;

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

/// QuantLib Suite -- 18 quantitative analysis modules over the
/// api.fincept.in/quantlib REST API (catalog in QuantLibScreen_Data.cpp).
///
/// Left: modules / sub-panels with the plan's access badge (FREE · 1 CR · PRO).
/// Centre: endpoint picker (with search across all modules), the access note
/// for the current plan, and a typed parameter form. Right: results as a
/// table, chart or raw JSON, with explicit error states (403 → upgrade,
/// 402 → out of credits, network → retry).
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
    QWidget* create_status_bar();

    /// Refill endpoint_combo_ for the active module (restricted to the active
    /// sub-panel when that panel maps onto a recognisable path segment), or for
    /// the search text across every module. Keeps the selection — and the
    /// form's unsent edits — when the current endpoint survives the refill.
    void populate_panels(int module_index);
    /// Endpoints of `module_index`, narrowed to `panel` when it matches any.
    QStringList endpoints_for(int module_index, const QString& panel, bool* filtered = nullptr) const;

    /// Plan-dependent chrome: header note / upgrade button, tree and endpoint badges.
    void refresh_plan_ui();
    void refresh_tree_badges();
    void decorate_endpoint_items();
    void update_access_label();
    services::QuantLibCost cost_of(const QString& endpoint) const;
    QString current_endpoint() const;

    void remember_body(const QString& endpoint, const QJsonObject& body);
    void execute_api(const QString& endpoint, const QJsonObject& params);
    void set_loading(bool loading);
    void open_plans();

    // Endpoint example bodies (the parameter form's defaults)
    static const QHash<QString, QString>& endpoint_examples();

    // Modules
    QList<QuantModule> modules_;
    int active_module_ = 0;
    QString active_panel_;
    bool loading_ = false;
    bool paid_plan_ = false;
    QString form_endpoint_; ///< endpoint the parameter form currently shows

    /// Last request body per endpoint (most recent last in body_order_),
    /// persisted with the screen state so inputs survive restarts.
    QHash<QString, QJsonObject> last_bodies_;
    QStringList body_order_;
    /// Endpoints the server answered 403 for this session, whatever the local
    /// tier table says (the server is authoritative).
    QSet<QString> pro_overrides_;

    // Header
    QLabel* header_title_ = nullptr;
    QLabel* header_sub_ = nullptr;
    QLabel* header_badge_ = nullptr; ///< plan: FREE PLAN / PRO PLAN
    QLabel* plan_note_ = nullptr;
    QPushButton* upgrade_btn_ = nullptr;

    // Sidebar
    QLabel* sidebar_title_ = nullptr;
    QTreeWidget* module_tree_ = nullptr;

    // Centre
    QLabel* center_title_ = nullptr;
    QLabel* endpoint_panel_title_ = nullptr;
    QLineEdit* filter_edit_ = nullptr;
    QComboBox* endpoint_combo_ = nullptr;
    QLabel* access_label_ = nullptr;
    QLabel* params_title_ = nullptr;
    QuantLibParamForm* param_form_ = nullptr;
    QPushButton* exec_btn_ = nullptr;

    // Results
    QuantLibResultView* result_view_ = nullptr;

    // Status
    QLabel* status_left_ = nullptr;
    QLabel* status_module_ = nullptr;
    QLabel* status_panel_ = nullptr;
    QLabel* status_endpoint_ = nullptr;
};

} // namespace fincept::screens
