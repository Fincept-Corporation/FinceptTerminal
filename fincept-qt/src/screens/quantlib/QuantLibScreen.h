#pragma once

#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/quantlib/QuantLibAccess.h"

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTimer>
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
///
/// No empty results pane: when an endpoint is shown, its default view comes up
/// at once — the remembered result ("as of …"), or for an endpoint that costs
/// nothing on the current plan a run with the last / sample inputs behind a
/// skeleton (debounced, one at a time, cancelled when the view is left), or for
/// a paid / Pro-only endpoint a preview: what it returns, the inputs, the cost,
/// RUN. Nothing that costs a credit or would be refused ever runs on its own.
class QuantLibScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit QuantLibScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "quantlib"; }
    int state_version() const override { return 1; }

    // ISectionLinked — the modules by QuantModule::id ("core" "analysis" "curves" … "volatility";
    // "module:<id>" too), as a click on the module in the tree. `focus` = an endpoint, full
    // ("pricing/bs/greeks-full") or relative to the module ("bs/greeks-full"): picked in the endpoint
    // list (its default view follows the screen's own rules — nothing that costs a credit runs). A
    // section may also be a full endpoint path.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_module_changed(int index);
    void on_endpoint_changed(int index);
    /// Runs the current endpoint (Enter, Ctrl+Enter, RETRY). While a call is in
    /// flight it supersedes it: the old call is aborted and its answer dropped.
    void on_execute();
    /// EXECUTE button: runs, or cancels the call in flight (it reads CANCEL then).
    void on_exec_clicked();
    /// Cancels the call in flight (CANCEL, Esc); the previous result stays.
    void cancel_request();

  private:
    void setup_ui();
    void retranslateUi();
    QWidget* create_header();
    QWidget* create_sidebar();
    QWidget* create_center_panel();
    QWidget* create_status_bar();
    /// A click on a module (top level) or sub-panel item of the module tree.
    void on_tree_item_clicked(QTreeWidgetItem* item);

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
    /// `automatic` = the view's default run (not the user's): it is cancelled
    /// when the view is left and never draws under another endpoint.
    void execute_api(const QString& endpoint, const QJsonObject& params, bool automatic = false);
    /// Drops the call in flight without touching the results pane: its callback
    /// becomes stale and its transfer is aborted.
    void abandon_request();
    void set_loading(bool loading);
    void open_plans();

    // ── Default view of the current endpoint (never a blank results pane) ──
    /// One remembered answer per endpoint: in memory, and — when small — with
    /// the screen state, so it is back instantly after a restart.
    struct CachedResult {
        QJsonObject request;
        QJsonValue payload;   ///< Undefined until a restored entry is first shown (parsed lazily)
        QByteArray stored;    ///< compact {"p": payload} kept with the screen state ("" = not kept)
        QDateTime at;         ///< when it was computed (UTC)
        qint64 elapsed_ms = 0;
        bool restored = false; ///< from a previous session
    };
    /// Shows the current endpoint's default view: its run in flight, its
    /// remembered result, its last failure, a skeleton + scheduled default run
    /// (free on this plan), or the preview. `force` re-evaluates a view that
    /// already shows this endpoint (plan change, language change, re-show).
    void show_default_view(bool force = false);
    /// Debounce timer slot: starts the scheduled default run if still wanted.
    void start_default_run();
    /// Free on the current plan, signed in, not paused after a service error.
    bool can_auto_run(const QString& endpoint) const;
    void show_preview_for(const QString& endpoint, const QString& status_note = QString());
    bool show_cached(const QString& endpoint);
    void store_result(const QString& endpoint, const QJsonObject& request, const QJsonValue& payload,
                      qint64 elapsed_ms);
    /// The endpoint a fresh screen opens on (an instructive one for the plan).
    void select_landing_endpoint();
    void update_exec_button();
    void flash_status(const QString& text);

    // Endpoint example bodies (the parameter form's defaults)
    static const QHash<QString, QString>& endpoint_examples();

    // Modules
    QList<QuantModule> modules_;
    int active_module_ = 0;
    QString active_panel_;
    bool loading_ = false;
    quint64 request_seq_ = 0;   ///< bumped per call; a callback with an older value is stale
    quint64 request_id_ = 0;    ///< QuantLibClient id of the call in flight (0 = none / cached)
    QString loading_endpoint_;  ///< endpoint of the call in flight
    bool paid_plan_ = false;
    QString form_endpoint_; ///< endpoint the parameter form currently shows

    /// Last request body per endpoint (most recent last in body_order_),
    /// persisted with the screen state so inputs survive restarts.
    QHash<QString, QJsonObject> last_bodies_;
    QStringList body_order_;
    /// Endpoints the server answered 403 for this session, whatever the local
    /// tier table says (the server is authoritative).
    QSet<QString> pro_overrides_;

    QHash<QString, CachedResult> results_;
    QStringList result_order_;                               ///< most recent last
    QHash<QString, services::QuantLibFailure> failures_;     ///< last failure per endpoint (this session)
    QSet<QString> auto_ran_;      ///< endpoints whose default run was started this session
    QTimer* auto_timer_ = nullptr; ///< debounces the default run while the user browses
    QString auto_endpoint_;       ///< endpoint the scheduled default run is for
    bool loading_auto_ = false;   ///< the call in flight is a default run
    qint64 auto_paused_until_ms_ = 0; ///< no default runs until then (after a service / network error)
    QString view_endpoint_;       ///< endpoint the results pane's default view was built for
    QString plan_key_;            ///< signed-in / paid state the default views were built for

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
