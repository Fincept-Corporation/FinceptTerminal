// src/screens/ma_analytics/MAModulePanel.h
#pragma once
#include "services/company_snapshot/CompanySnapshotService.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "services/ma_analytics/MAAnalyticsTypes.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QPair>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <functional>

namespace fincept::ui {
class AddToReportButton;
class AutoLoadPane;
class EmptyState;
class InputMemory;
class ResultView;
class SensitivityTable;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::report {
struct ReportPayload;
}

namespace fincept::services::ma {
struct MaCase;
}

class QHBoxLayout;

namespace fincept::screens {

// Make MAAnalyticsService visible to every panel TU that includes this
// header — each panel calls MAAnalyticsService::instance() unqualified.
using fincept::services::ma::MAAnalyticsService;

class MaDealsWorkspace;
class MaDealGrid;

/// Renders the input form + results area for a single M&A module.
/// Each module has sub-tabs (methods), input fields, a Run button, and result display.
///
/// Never an empty result area: the module's result area is ONE ui::AutoLoadPane whose
/// view is the visible sub-tab's analysis (service context). The first time a sub-tab
/// is shown, its last result appears at once (memory, else the persistent cache) and
/// its analysis runs with the inputs on screen behind a loading skeleton (or as a
/// quiet refresh); RUN re-runs it; a reply is drawn only under its own sub-tab (the
/// pane's request ids); leaving a sub-tab or the module cancels its default run
/// (MAAnalyticsService::cancel). Analyses that call a rate-limited data source
/// (Trading Comps) are Manual: a designed preview until RUN.
class MAModulePanel : public QWidget {
    Q_OBJECT
  public:
    explicit MAModulePanel(const fincept::services::ma::ModuleInfo& info, QWidget* parent = nullptr);
    ~MAModulePanel() override;

  public slots:
    void refresh_theme();

  signals:
    /// "Open the Deal Database" (from an empty comparables state): the screen
    /// switches to the module with this id (services::ma::ModuleId as int).
    void open_module_requested(int module_id);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result_ready(const QString& context, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);
    /// The script needs inputs it was not given: show its message and take the user to those inputs.
    void on_input_required(const QString& context, const QString& message, const QStringList& missing_inputs);

  private:
    void build_ui();
    void connect_service();
    void apply_tab_stylesheet();
    void retranslateUi();

    // Add a sub-tab and remember its untranslated source label so retranslateUi
    // can re-apply tr() on a live language change. Pass the label via
    // QT_TR_NOOP("...") so lupdate still extracts the literal.
    void add_sub_tab(QWidget* page, const char* source_label);

    // Add a sub-tab whose content `build` fills into the (empty) page the first
    // time the tab is shown — the first tab at once. A module has up to ten
    // input forms and only one is visible, so building them all made opening
    // a module slow. Every run builds the remaining tabs first (begin_run), as
    // some runs read inputs that live on another tab.
    void add_lazy_sub_tab(const char* source_label, std::function<void(QWidget* page)> build);
    void build_sub_tab(QWidget* page);
    void build_all_sub_tabs();

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

    // Result display: the pane (inside results_container_) around a ResultView and the
    // "nothing to compare yet" state of a needs_data reply.
    void ensure_result_view();
    void display_result(const QJsonObject& data, const QString& context);
    /// An input check of the visible sub-tab's run failed: fail that run (the pane
    /// shows the message with RETRY).
    void display_error(const QString& msg);
    void clear_results();
    /// Charts the generic renderer cannot infer for `context` (contribution split,
    /// football field, process-quality gauge, Monte Carlo range).
    void add_context_widgets(const QString& context, const QJsonObject& payload);
    /// Before every run button's own handler: builds every form (some runs read
    /// another sub-tab's inputs) and, when the USER pressed it, starts a pane run
    /// of the visible sub-tab (its request id is what the reply is matched to).
    void begin_run(QPushButton* button);
    /// True when `context` is one of this module's analyses.
    bool owns_context(const QString& context) const;
    /// Outline the panel inputs behind the script parameter `keys` of `context` (WARNING) until edited;
    /// `focus` also switches to their sub-tab and focuses the first one. Returns the inputs found.
    int flag_inputs(const QString& context, const QStringList& keys, bool focus);
    /// A "needs_data" reply: nothing was computed; say what data is needed and offer what fills it.
    void display_needs_data(const QJsonObject& needs);

    // ── Pane wiring ──────────────────────────────────────────────────────────
    /// Analysis (service context) run by sub-tab `index` / by the sub-tab holding `w`.
    QString context_of_tab(int index) const;
    QString context_of_widget(const QWidget* w) const;
    QPushButton* run_button_of_tab(int index) const;
    QString tab_title(int index) const;
    /// The visible sub-tab changed: switch the pane to its analysis.
    void select_view();
    /// The pane starts run `id` of its view: a default run, RETRY or the preview's RUN
    /// clicks the sub-tab's RUN button; a user's RUN is already under way.
    void start_run(quint64 id);
    void cancel_run(quint64 id);
    void render_payload(const QJsonValue& payload);
    /// The Manual preview of `context` (what it does, its inputs, why it is not automatic).
    void configure_preview(const QString& context);
    /// A sub-tab page was built: remember its inputs (ui::InputMemory, screen "ma_analytics", view =
    /// its analysis) — named inputs by their field name, the rest (deal boxes) by position — and put
    /// the last session's values back before anything runs.
    void attach_memory(QWidget* page);

    fincept::ui::AutoLoadPane* pane_ = nullptr;
    fincept::ui::EmptyState* needs_empty_ = nullptr; // inside the pane's content
    QHash<QString, quint64> run_ids_;                 // context -> the pane's request id of its latest run
    QHash<QString, fincept::ui::InputMemory*> memories_; // context -> the remembered inputs of its sub-tab
    bool pane_clicking_ = false;                      // start_run is clicking a RUN button
    bool user_clicking_ = false;                      // begin_run is starting a user's run
    /// Comparables grids by analysis (Precedent Txns, Regression) — the empty state's USE SAMPLE fills them.
    QHash<QString, MaDealGrid*> deal_grids_;

    fincept::services::ma::ModuleInfo module_;
    QTabWidget* sub_tabs_ = nullptr;
    // (sub-tab page, untranslated source label) — for live retranslateUi.
    QVector<QPair<QWidget*, QString>> sub_tab_sources_;
    // Sub-tab pages not built yet, with their builders (add_lazy_sub_tab).
    QHash<QWidget*, std::function<void(QWidget*)>> pending_sub_tabs_;
    bool lazy_tabs_wired_ = false;
    QWidget* header_bar_ = nullptr;
    QLabel* header_title_ = nullptr;
    QLabel* header_category_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QWidget* results_container_ = nullptr;
    fincept::ui::ResultView* result_view_ = nullptr;
    QVector<QPointer<QPushButton>> run_buttons_;
    QLabel* status_label_ = nullptr;

    // Input field maps for gathering params
    QHash<QString, QDoubleSpinBox*> double_inputs_;
    QHash<QString, QSpinBox*> int_inputs_;
    QHash<QString, QComboBox*> combo_inputs_;

    // Deal Database module only (built by build_deals_panel).
    MaDealsWorkspace* deals_ws_ = nullptr;

    // ── MA-3: deal / comparables grids ───────────────────────────────────────
    /// The Comparison module's one deal grid (above its sub-tabs; every Comparison run reads it).
    MaDealGrid* comparison_grid_ = nullptr;
    /// Valid rows of `grid`, or fail the visible run with a plain message (false) when it has none.
    bool grid_deals(MaDealGrid* grid, QJsonArray* out, bool allow_empty);
    /// Inputs of the module itself (company symbols, the Comparison grid), not of one sub-tab.
    fincept::ui::InputMemory* module_memory_ = nullptr;
    /// The signature a remembered result of `context` must match: its sub-tab inputs (+ module inputs).
    QString signature_of(const QString& context) const;
    void attach_module_memory();

    // ── MA-4: start from a ticker (MaCompanyPrefill.cpp) ─────────────────────
    /// "Company" strip above the sub-tabs: a symbol box + LOAD per role ("target"; Merger also "acquirer").
    QWidget* build_company_strip(QWidget* parent);
    void load_company(const QString& role);
    void apply_snapshot(const QString& role, const fincept::services::CompanySnapshot& s);
    /// Put `value` into input `name` with a "from <source>" caption. Skips a field the user edited after the
    /// last fill of the same company. False when the input does not exist or the value is not finite.
    bool prefill(const QString& name, double value, const QString& caption);
    /// Premium analysis reference prices (1 day / 4 weeks / 52-week high) from one year of daily closes.
    void fetch_reference_prices(const QString& symbol, const QString& source_caption);
    void suggest_peers();
    QHash<QString, QPointer<fincept::ui::SymbolSearchBox>> company_boxes_; ///< role -> box
    QHash<QString, QPointer<QLabel>> company_status_;                    ///< role -> status line
    QHash<QString, quint64> company_requests_;                           ///< role -> snapshot request id
    QHash<QString, QString> prefilled_symbol_;                           ///< role -> symbol last applied
    QHash<QString, fincept::services::CompanySnapshot> snapshots_;       ///< role -> last snapshot applied
    QHash<QWidget*, QPointer<QLabel>> source_labels_;                    ///< input -> its "from ..." caption
    QSet<QWidget*> user_edited_;                                         ///< inputs edited after their fill
    bool prefilling_ = false;
    quint64 history_request_ = 0;
    quint64 peers_request_ = 0;
    QString peers_symbol_;

    // ── MA-5 / MA-6 / MA-7 context widgets ───────────────────────────────────
    /// The last successful result of each analysis of this module (football field inputs); also kept in the
    /// persistent cache so a restart still has them.
    void remember_run(const QString& context, const QJsonObject& payload);
    QJsonObject last_run(const QString& context) const;
    QHash<QString, QJsonObject> last_runs_;
    void build_football_field_tab(QWidget* page);
    /// The football-field result from the remembered runs and the tab's inputs (local, synchronous).
    QJsonObject football_field_payload() const;
    void add_sensitivity_widgets(const QString& context, const QJsonObject& payload);
    void add_football_field_widget(const QJsonObject& payload);
    void add_merger_widgets(const QJsonObject& payload);

    // ── MA-9: result actions (MaResultActions.cpp) ───────────────────────────
    void build_result_actions(QWidget* parent);
    void update_result_actions();
    QString result_title() const;
    /// Label -> shown value of every input of the visible sub-tab (the assumptions behind the result).
    QVector<QPair<QString, QString>> inputs_echo() const;
    void copy_result();
    void export_csv();
    void export_png();
    fincept::report::ReportPayload report_payload();
    QWidget* actions_bar_ = nullptr;
    QLabel* result_title_label_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QPushButton* png_btn_ = nullptr;
    fincept::ui::AddToReportButton* report_btn_ = nullptr;
    /// Sensitivity grids and charts of the shown result (exported / sent with it).
    QList<QPointer<fincept::ui::SensitivityTable>> shown_grids_;
    QList<QPair<QString, QPointer<QWidget>>> shown_charts_;
    QHash<QWidget*, QString> input_labels_; ///< input -> its row label (inputs echo)

    // ── MA-8: saved cases (MaCases.cpp) ──────────────────────────────────────
    void build_case_buttons(QHBoxLayout* header);
    void save_case();
    void show_cases_menu();
    void load_case(const fincept::services::ma::MaCase& c);
    void compare_case(const fincept::services::ma::MaCase& c);
    QPushButton* save_case_btn_ = nullptr;
    QPushButton* cases_btn_ = nullptr;
};

} // namespace fincept::screens
