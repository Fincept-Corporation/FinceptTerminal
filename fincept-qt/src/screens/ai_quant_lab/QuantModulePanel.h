// src/screens/ai_quant_lab/QuantModulePanel.h
#pragma once
#include "services/ai_quant_lab/AIQuantLabTypes.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHash>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::screens {

/// Dynamic panel for a single AI Quant Lab module.
/// Builds input forms and result display based on module type.
///
/// Every sub-view (a tab, a CFA analysis, a single-page module) is declared
/// with a ViewSpec and shares ONE ui::AutoLoadPane under the forms: the pane
/// switches to the visible view, shows that view's remembered result at once,
/// runs its default action on first show (cost Free / Fetch) or shows a
/// designed preview (Heavy), keeps a result per view, drops late replies by
/// request id and cancels a default run when its view is left. Tab pages are
/// built on first view; their inputs are remembered per view (ui::InputMemory).
/// See QuantModulePanel_AutoLoad.cpp.
class QuantModulePanel : public QWidget {
    Q_OBJECT
  public:
    explicit QuantModulePanel(const fincept::services::quant::QuantModule& mod, QWidget* parent = nullptr);
    ~QuantModulePanel() override;

    /// Deep link (AIQuantLabScreen::open_section "module:<id>/<view>"): true when the module has a
    /// TAB view `key` (exact, else case-insensitive). With `select` it switches to that tab — the tab
    /// click path, so the view behaves as on a click (a Heavy view never runs by itself).
    bool open_view_tab(const QString& key, bool select);

    /// Cost class of a view (empty-views audit, PART 1-3).
    enum class ViewCost {
        Free,  ///< A: local and quick — runs on first show; a remembered result never goes stale
        Fetch, ///< B: free but slow or networked — runs on first show behind the skeleton
        Heavy  ///< C: trains / calls an LLM / writes state / long — never runs by itself
    };

    /// How a view is declared (the only per-tab code the mechanism needs).
    struct ViewSpec {
        QString key{};                 ///< unique in the module; InputMemory view and pane view
        ViewCost cost = ViewCost::Fetch;
        QString loading{};             ///< skeleton caption until the action says more
        ui::LoadingSkeleton::Shape shape = ui::LoadingSkeleton::Shape::Result;
        int max_age_s = -2;          ///< remembered-result refresh (s); -2 = by cost (Free: never, Fetch: 6 h)
        QString pane_view{};           ///< tabs sharing one result (HFT): the common pane view key
        bool qlib_dataset = false;   ///< reads the module's local qlib dataset (waits for / gates on the bar)
        bool llm = false;            ///< needs an LLM profile
        bool pane = true;            ///< false: the view shows its own widgets; the pane hides for it
        /// Heavy views that may run by themselves once a prerequisite exists (a saved model path…).
        std::function<bool()> auto_when{};
        /// Called after a reply of this view was delivered (ok) or failed: refresh a list another
        /// view shows, keep a produced id for the next view, reset bespoke progress widgets.
        std::function<void(bool ok, const QString& command, const QJsonObject& data)> after{};
        // Preview of a view that does not run by itself (the designed empty state).
        QString title{};               ///< what the view is
        QString returns{};             ///< what a run returns, one sentence
        QString run_text{};            ///< the one primary action ("TRAIN MODEL")
        QString cost_note{};           ///< chip: "Trains a model · about 2 min of CPU"
    };

    /// What a series input reads when its view's source is a ticker (D1).
    enum class SeriesKind {
        Closes,       ///< daily closes
        Returns,      ///< daily simple returns
        Open,
        High,
        Low,
        Volume,
        Signal,       ///< 20-day momentum (close / close 20 days earlier − 1), aligned with FwdReturns
        FwdReturns,   ///< next-day return, aligned with Signal
        OhlcvJson,    ///< {"open":[…],"high":[…],"low":[…],"close":[…],"volume":[…]}
        FactorsJson,  ///< {"momentum_20d":[…],"reversal_5d":[…],"volatility_20d":[…],…} aligned with FwdReturns
        ReturnsMatrixJson, ///< list: [[r_a, r_b, …], …] one row per day
        CovarianceJson,    ///< list: annualised covariance matrix of daily returns
        Assets,            ///< list: the symbols, comma-separated
        ExpReturns,        ///< list: annualised mean daily return per symbol, comma-separated
        SeriesMatrixJson,  ///< list: {"SPY":[returns…],"QQQ":[…],…}
        CrossMomentum,     ///< list: 20-day momentum per symbol (a cross-sectional signal), comma-separated
        EqualWeights,      ///< list: 1 per symbol (equal caps / weights), comma-separated
        ActualCloses,      ///< closes from the second day on, aligned with NaiveForecast
        NaiveForecast,     ///< the previous day's close as the forecast of each day (same length as ActualCloses)
        HoldoutTrain,      ///< closes except the last 20 days (the history a forecast is made from)
        HoldoutActual,     ///< the last 20 closes (what happened)
        HoldoutNaive,      ///< the last training close repeated over the 20 hold-out days (a naive forecast)
    };
    /// Which tickers a view's source asks for.
    enum class SeriesShape { Single, Pair, List };

  protected:
    void changeEvent(QEvent* event) override;
    // The loading spinner is a UI animation timer: run it only while the panel is on screen.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_result(const QString& module_id, const QString& command, const QJsonObject& data);
    void on_error(const QString& module_id, const QString& message);

  private:
    void build_ui();
    void connect_service();

    /// Re-apply tr() lookups to persistent, member-stored widgets after a
    /// QEvent::LanguageChange. Module header label/category are data-derived
    /// (services::quant::QuantModule) and kept verbatim; dynamically-rebuilt
    /// input forms and result cards re-render in the active language on their
    /// next build/fetch.
    void retranslateUi();

    // Module-specific builders
    QWidget* build_deep_agent_panel();
    QWidget* build_rd_agent_tab(QComboBox* llm_combo);
    QWidget* build_generic_panel();
    QWidget* build_gs_quant_panel();
    QWidget* build_functime_panel();
    QWidget* build_statsmodels_panel();
    QWidget* build_fortitudo_panel();
    QWidget* build_gluonts_panel();
    QWidget* build_cfa_quant_panel();
    QWidget* build_backtesting_panel();
    QWidget* build_rl_trading_panel();
    QWidget* build_advanced_models_panel();
    QWidget* build_factor_discovery_panel();
    QWidget* build_model_library_panel();
    QWidget* build_live_signals_panel();
    QWidget* build_online_learning_panel();
    QWidget* build_meta_learning_panel();
    QWidget* build_hft_panel();
    QWidget* build_rolling_retraining_panel();
    QWidget* build_feature_engineering_panel();
    QWidget* build_portfolio_opt_panel();
    QWidget* build_factor_evaluation_panel();
    QWidget* build_strategy_builder_panel();
    QWidget* build_data_processors_panel();
    QWidget* build_quant_reporting_panel();

    // Helpers
    QWidget* build_input_row(const QString& label, QWidget* input, QWidget* parent);
    QDoubleSpinBox* make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                     QWidget* parent);
    QPushButton* make_run_button(const QString& text, QWidget* parent);
    /// "LOAD SYNTHETIC SAMPLE" button that fills `edit` with `generator()`.
    /// The filled text is remembered on the edit, so a run whose visible inputs
    /// still hold the synthetic sample is flagged SAMPLE INPUT on its results.
    QPushButton* make_sample_button(QLineEdit* edit, QWidget* parent, const QString& tip,
                                    std::function<QString()> generator);
    /// True when a visible input of the panel still holds unedited sample text.
    bool visible_inputs_are_sample() const;
    QWidget* build_llm_picker(QWidget* parent, QComboBox** out_combo);
    QJsonObject llm_config_from_combo(QComboBox* combo) const;
    void display_result(const QJsonObject& data);
    void display_backtest_result(const QJsonObject& data);
    void display_cfa_result(const QString& command, const QJsonObject& data);
    void display_gs_result(const QString& command, const QJsonObject& data);
    void display_functime_result(const QString& command, const QJsonObject& data);
    void display_statsmodels_result(const QString& command, const QJsonObject& data);
    void display_fortitudo_result(const QString& command, const QJsonObject& data);
    void display_quant_reporting_result(const QString& command, const QJsonObject& data);
    void display_gluonts_result(const QString& command, const QJsonObject& data);
    // ADVANCED-category panel renderers (card-based, mirror analytics pattern).
    // hft and rolling_retraining have no display_*_result: on_result() renders them
    // into their own in-tab cards / tables / progress bar.
    void display_advanced_models_result(const QString& command, const QJsonObject& data);
    void display_feature_engineering_result(const QString& command, const QJsonObject& data);
    // AI/ML-category renderers (rl_trading, online_learning, meta_learning)
    void display_rl_trading_result(const QString& command, const QJsonObject& data);
    void display_online_learning_result(const QString& command, const QJsonObject& data);
    void display_meta_learning_result(const QString& command, const QJsonObject& data);
    // CORE-category renderers (factor_discovery, model_library, live_signals)
    // backtesting kept on its existing display_backtest_result KPI grid.
    void display_factor_discovery_result(const QString& command, const QJsonObject& data);
    void display_model_library_result(const QString& command, const QJsonObject& data);
    void display_live_signals_result(const QString& command, const QJsonObject& data);
    void display_portfolio_opt_result(const QString& command, const QJsonObject& data);
    void display_factor_evaluation_result(const QString& command, const QJsonObject& data);
    void display_strategy_builder_result(const QString& command, const QJsonObject& data);
    void display_data_processors_result(const QString& command, const QJsonObject& data);
    void show_loading(const QString& message);
    void display_error(const QString& msg);
    void clear_results();

    void refresh_theme();

    // ── Views, the result pane, request routing (QuantModulePanel_AutoLoad.cpp) ──
  public:
    /// Builds a tab page's form: `page` with a ready QVBoxLayout `layout` (margins 12, spacing 8).
    using ViewBuildFn = std::function<void(QWidget* page, QVBoxLayout* layout)>;

  private:
    /// The module's tab widget: pages are built on first view; the pane follows the current tab.
    QTabWidget* make_view_tabs(QWidget* parent);
    /// A lazily built tab. `build` creates the form and calls set_action() for its button(s).
    void add_view_tab(QTabWidget* tabs, const ViewSpec& spec, const QString& title, ViewBuildFn build);
    /// A view whose form is built inline (single-page module, CFA analysis): set_action() and the
    /// series calls between begin_view() and end_view() belong to it.
    void begin_view(const ViewSpec& spec);
    void end_view();
    /// `button` runs `action` through the pane for the view being built. The first default action
    /// of a view is what its first show runs (Free / Fetch) or its preview's button runs (Heavy).
    /// `timeout_s` >= 0: this button's runs use that time-out instead of the view's (0 = none; e.g.
    /// a multi-minute download in a Fetch view).
    void set_action(QPushButton* button, std::function<void()> action, bool is_default = true, int timeout_s = -1);
    /// A default action with no button of its own (e.g. a list shown under a create form).
    void set_default_action(std::function<void()> action);
    /// One RUN button serving several inline views (CFA: one button, the analysis picked in a combo):
    /// `action` is the default of each of `views`, and the button runs the current one.
    void set_shared_action(QPushButton* button, std::function<void()> action, const QStringList& views);
    /// The result pane under the forms (replaces the old shared results area). Call once, last.
    void add_result_pane(QVBoxLayout* layout);
    /// Shows view `key` (tab switch, CFA analysis change): its remembered result, default run or preview.
    void select_view(const QString& key);
    /// Builds a lazily built view now (another view needs its inputs).
    void ensure_view_built(const QString& key);
    /// Switches to the tab of view `key` (e.g. "Create a config first" → the Create tab).
    void open_view(const QString& key);
    /// Re-run the visible view's default (a create / delete changed what a list shows).
    void invalidate_view(const QString& key);
    QString current_view() const { return current_view_; }

    // ── Saved-id pickers (QuantModulePanel_AutoLoad.cpp) ──
    /// Ids a script keeps on disk, offered in editable combos: the qlib models TRAIN saved
    /// (qlib_service.py list_trained_models) and the data-processing pipelines CREATE saved
    /// (qlib_data_processors.py list_pipelines).
    enum class SavedIds { TrainedModels, Pipelines };
    /// An editable combo of the saved ids of `kind` for the view being built, registered as
    /// combo_inputs_[key] so the view's InputMemory remembers what was picked or typed. `fallback`
    /// (the last id trained / created) is its text until something is remembered or listed. The list
    /// is read when the combo is created and again after a train / create. With nothing entered the
    /// newest id is picked (`prefer_feature_importance`: the newest tree model, for Feature Importance).
    QComboBox* make_saved_id_combo(SavedIds kind, const QString& key, const QString& fallback, QWidget* parent,
                                   bool prefer_feature_importance = false);
    /// Re-reads the saved ids of `kind` (never inside a view's run); `select` — an id just trained or
    /// created — becomes the value of every combo of that kind.
    void refresh_saved_ids(SavedIds kind, const QString& select = {});
    /// A list reply (on_result): refills the combos of its kind. False for any other command.
    bool apply_saved_ids(const QString& command, const QJsonObject& data);
    /// The id a saved-id combo holds (picked or typed), trimmed.
    static QString saved_id(const QComboBox* combo);
    /// False only when the list of `kind` is known and lacks `id` (a deleted model / pipeline): a view
    /// does not run by itself on such an id and shows its designed preview instead.
    bool saved_id_listed(SavedIds kind, const QString& id) const;
    struct SavedIdList {
        QList<QPointer<QComboBox>> combos;
        QJsonArray rows;      ///< the last list reply's rows
        bool known = false;   ///< a list reply has arrived
        bool pending = false; ///< a list request is in flight
        QString select;       ///< applied when the pending reply arrives
    };
    void fill_saved_id_combo(QComboBox* combo, SavedIds kind, const QString& select);
    QHash<int, SavedIdList> saved_ids_;
    bool saved_ids_watch_ = false; ///< listening for trains / backtests of other panels

    // ── D1 series source (QuantModulePanel_Series.cpp) ──
    /// The real-data source of the view being built: Source (Ticker / Custom series), the ticker(s)
    /// (shared symbol search) and the period. Paste boxes marked with mark_series_input() become the
    /// "Custom series" alternative.
    void add_series_source(QWidget* page, QVBoxLayout* layout, SeriesShape shape,
                           const QString& second_label = QString());
    void mark_series_input(QLineEdit* edit, SeriesKind kind, int role = 0);
    /// A widget of the view that belongs to one source only: shown in Ticker mode (`ticker_only`, e.g.
    /// "with a ticker, the signal is …") or only with a Custom series (an input a ticker run ignores).
    void mark_series_widget(QWidget* widget, bool ticker_only);
    /// What a run reads from text input `key`: the downloaded series in Ticker mode, else the box text.
    QString series_text(const QString& key) const;

    // internals
    struct SeriesSource;
    struct SeriesData;
    struct ViewState {
        ViewSpec spec;
        QString title;
        QPointer<QWidget> page;
        ViewBuildFn build;            ///< pending lazy build
        std::function<void()> action; ///< default action
        QList<QPointer<QPushButton>> buttons;
        ui::InputMemory* memory = nullptr;
        QStringList input_keys;       ///< named inputs created by the view's form
        std::shared_ptr<SeriesSource> series;
        bool built = false;
    };
    struct RunContext {
        quint64 pane_id = 0;
        QString view;
        bool failed = false;
    };
    struct PendingRun {
        quint64 pane_id = 0;
        QString view;
        std::shared_ptr<std::atomic<bool>> cancel;
        QJsonObject meta;
    };
    QString pane_view_of(const QString& view) const;
    QWidget* named_input(const QString& name) const;
    int view_timeout(const QString& view) const;
    void build_view(ViewState& v);
    void attach_memory(ViewState& v, const QStringList& extra_keys);
    void attach_inline_memory();
    QString view_signature(const QString& view) const;
    void configure_preview(const ViewState& v);
    void sync_view_tabs_policy();
    void sync_result_host_policy();
    void start_view_run(quint64 pane_id);
    void execute_view_action(quint64 pane_id, const QString& view, const std::function<void()>& action,
                             const QJsonObject& meta);
    void cancel_view_run(quint64 pane_id);
    void render_view_payload(const QJsonValue& payload);
    void on_request_finished(quint64 request_id, const QString& module_id, const QString& command,
                             const QJsonObject& data);
    void on_request_failed(quint64 request_id, const QString& module_id, const QString& message);
    /// Renders one module result into results_layout_ (shared by the pane and untagged replies).
    void render_result(const QString& command, const QJsonObject& payload);
    void decorate_result(const QString& command, const QJsonObject& payload, const QJsonObject& meta);
    bool page_inputs_are_sample(const QWidget* page) const;
    void set_view_buttons_busy(const QString& pane_view, bool busy);
    void qlib_dataset_known(); ///< the dataset bar's probe answered (waiting runs go on)
    void apply_qlib_defaults_to_view(const QString& key);
    // series internals (QuantModulePanel_Series.cpp)
    SeriesSource* series_source(const QString& view) const;
    bool series_ticker_mode(const QString& view) const;
    void sync_series_mode(const QString& view);
    QStringList series_symbols(const QString& view) const;
    QString series_default_symbol();
    void fetch_series(quint64 pane_id, const QString& view, std::function<void(const QJsonObject& meta)> then);
    void cancel_series_fetch(quint64 pane_id);
    QString format_series(const SeriesData& d, SeriesKind kind, int role) const;
    QStringList series_signature_skip(const QString& view) const;

    QHash<QString, ViewState> views_;
    QStringList view_order_;
    QHash<QWidget*, QString> view_of_page_;
    QString building_view_;
    QStringList inline_before_; ///< named inputs that existed at begin_view()
    ui::InputMemory* form_memory_ = nullptr; ///< inputs outside the tabs (HFT bar, CFA data) + inline views
    QStringList shared_keys_;   ///< named inputs no view owns (part of every view's signature)
    QString current_view_;
    QPointer<QTabWidget> view_tabs_;
    ui::AutoLoadPane* pane_ = nullptr;
    QWidget* result_host_ = nullptr;
    RunContext* run_ctx_ = nullptr;
    std::function<void()> button_action_; ///< a button's action for the run about to start
    QString button_action_view_;
    QHash<quint64, PendingRun> pending_;  ///< service request id → pane run
    QHash<quint64, QPointer<QObject>> fetches_; ///< pane run → its price download (series views)
    QHash<QString, std::shared_ptr<SeriesData>> series_data_; ///< view → the last download
    QHash<quint64, std::function<void()>> qlib_waiting_; ///< pane runs waiting for the dataset probe
    QJsonObject qlib_dataset_;            ///< the dataset bar's chosen entry ({} = none / unknown)
    QString default_symbol_;
    bool rendering_ = false;

  private:
    fincept::services::quant::QuantModule module_;
    QVBoxLayout* results_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QWidget* panel_header_ = nullptr; // the fixed header bar inside build_ui()
    QLabel* header_title_ = nullptr;
    QLabel* header_cat_ = nullptr;
    QTextEdit* agent_output_ = nullptr;     // deep_agent LangGraph tab
    QTextEdit* rd_agent_output_ = nullptr;  // rd_agent tab output
    QTableWidget* rd_task_table_ = nullptr; // rd_agent task monitor

    // RL Trading subtab — live progress UI
    class QProgressBar* rl_progress_bar_ = nullptr;
    QLabel* rl_progress_stats_ = nullptr;
    class QPlainTextEdit* rl_log_console_ = nullptr;
    QPushButton* rl_train_button_ = nullptr;

    /// Set by show_loading() when the run was fed LOAD SAMPLE data; on_result()
    /// then puts a SAMPLE INPUT banner above the results.
    bool sample_input_pending_ = false;

    QHash<QString, QDoubleSpinBox*> double_inputs_;
    QHash<QString, QLineEdit*> text_inputs_;
    QHash<QString, QComboBox*> combo_inputs_;
    QHash<QString, QSpinBox*> int_inputs_;
    QHash<QString, QDateEdit*> date_inputs_;
};

} // namespace fincept::screens
