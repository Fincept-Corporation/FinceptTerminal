// src/screens/economics/panels/EconPanelBase.h
// Base class for all economics source panels.
// Provides: headline stat cards (latest + as-of date, change, YoY, range) + a time-series chart of
// the result when it is dated (one line, or one per series / numeric column) + sortable, paged data
// table with locale-formatted numbers and gain/loss colouring of change columns + CSV/clipboard
// export + a provenance line (units · frequency · seasonal adjustment · source · latest observation ·
// retrieval time) + generic persistence of the panel's query inputs.
//
// Never blank: the results area is a ui::AutoLoadPane. On the first show the panel's query runs
// with the restored (or default) inputs behind a loading skeleton — or, for a source that needs an
// API key the user has not stored, a "needs setup" page names the key; a class C source (costs
// money / tightly rate-limited) shows a designed preview with one RUN button instead. The last
// result is remembered (also across restarts) and shown at once with its "as of" time; it is
// refreshed quietly when older than the panel's max age. Failures show a plain sentence with
// RETRY; a result on screen stays when a refresh fails. Leaving the panel during an automatic
// load cancels it (the script is killed); FETCH supersedes a query in flight. When the query
// inputs no longer match the result on screen, the pane says "Inputs changed — RUN AGAIN".
//
// Never stalls the GUI thread: EconomicsService parses script answers on its decode pool, and
// display() hands the rows to a worker (econ_table::analyse: column order, sort, stat cards, chart
// lines); the GUI thread only applies the finished analysis — a page of at most 250 rows through a
// model (EconPageModel, no item per cell) and chart lines thinned to the plot's pixel width.
// Header-click sorts, CSV export and "copy all rows" run on the worker too. Panels whose own
// shaping of a large answer is heavy use offload().
//
// Subclasses call build_base_ui() from their constructor and keep their on_fetch() / on_result()
// shape: on_fetch() runs inside the pane's run; show_loading() / display() / show_error() /
// show_empty() / show_content_page() report to the pane. EconomicsService calls go through
// execute_query() so the run can be cancelled.
// For panels with a left selector sidebar, pass the right-side container
// to build_base_ui(container) instead.
#pragma once

#include "services/economics/EconomicsService.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/PaginationBar.h"

#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QFuture>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPointF>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QVariantMap>
#include <QVector>
#include <QWidget>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

class QAction;
class QSplitter;
class QTableView;
class QThreadPool;

namespace fincept::ui {
class TimeSeriesChart;
}

namespace fincept::screens {

namespace econ_table {
struct Analysis;
class EconPageModel;
} // namespace econ_table

class EconPanelBase : public QWidget {
    Q_OBJECT
  public:
    /// `color` is the provider's identity colour ("#rrggbb", as it looks on the dark theme). It is a
    /// brand colour, not a theme token: every place that draws it passes it through
    /// ui::colors::legible() / tint(), so it stays readable on the light theme too.
    explicit EconPanelBase(const QString& source_id, const QString& color, QWidget* parent = nullptr);
    ~EconPanelBase() override;

    /// Called by EconomicsScreen when the user switches to this panel (before it is shown). The
    /// default query itself is started by the results pane on show; activate() only prepares what
    /// the panel needs besides it (selector lists, the date of today, …).
    virtual void activate() = 0;

    /// True while the panel shows a result or has a query in flight. EconomicsScreen then skips
    /// activate() on re-selection.
    bool keeps_state_on_activate() const;

    /// Panel-level state for persistence. The default implementation remembers every query input the
    /// panel owns — toolbar and sidebar combos, text fields, spin/date boxes, list selections — so the
    /// last query comes back after a restart. Override only to add state that is not held in a widget.
    virtual QVariantMap save_panel_state() const;
    virtual void restore_panel_state(const QVariantMap& state);

    /// Compact number for cards/labels: 21.43T / 2.14B / 3.51M / 12,345 / 310.326.
    static QString format_number(double v);

    /// The worker pool of the economics panels (row analysis, sorts, exports, offload()): two
    /// threads, shared by every panel.
    static QThreadPool* worker_pool();

  signals:
    /// A query was started (FETCH / Enter / RETRY). EconomicsScreen persists the inputs then.
    void query_started();

  protected:
    /// Describes the series passed to the next display() call. Every field is optional; whatever is
    /// known is printed on the provenance line and drives the stat cards (change in percentage
    /// points for rates, relative % for levels, projections excluded from LATEST).
    struct SeriesMeta {
        QString units;        // "Percent", "Billions of chained 2017 dollars", "Index 2015=100", "bp"
        QString frequency;    // "Monthly", "Quarterly", ... — empty: inferred from the dates
        QString seasonal_adj; // "Seasonally adjusted", "Not seasonally adjusted", ...
        QString source;       // attribution; empty: the panel's default source
        QString last_updated; // the provider's own revision / release stamp, when it gives one
        QString note;         // short extra caveat shown on the provenance line
        int percent = -1;     // 1: values are rates/percentages (changes in pp), 0: levels, -1: infer
        QString value_column; // column the stat cards summarise; empty: auto
        QDate stats_cutoff;   // observations dated after this are projections — kept out of the cards
    };

    // ── Subclass API ──────────────────────────────────────────────────────────
    /// Add source-specific controls into the toolbar layout.
    virtual void build_controls(QHBoxLayout* toolbar_layout) = 0;
    virtual void on_fetch() = 0;
    virtual void on_result(const QString& request_id, const services::EconomicsResult& result) = 0;

    // ── UI construction ───────────────────────────────────────────────────────
    /// Build toolbar + stat cards + table stack into the given container widget.
    /// Call this from the subclass constructor (pass 'this' or a right-side pane).
    void build_base_ui(QWidget* container);

    // ── How the panel's default query behaves (ui::AutoLoadPane) ───────────────
    /// Read once, on the first show (the subclass is fully built by then).
    struct LoadPolicy {
        /// Class C — the source costs money or is tightly rate-limited: nothing runs by itself; with
        /// no remembered result a designed preview (what it returns, the inputs, the cost) offers RUN.
        bool manual = false;
        /// A remembered result older than this is refreshed quietly (AutoRun) or marked out of date
        /// (manual). -1 = never (the data never changes).
        int max_age_s = 6 * 3600;
        /// Give up (and offer RETRY) after this long.
        int timeout_s = 150;
        /// Shape of the loading skeleton: a chart above a table for a series, a table for a list,
        /// List for a list of events / items.
        ui::LoadingSkeleton::Shape shape = ui::LoadingSkeleton::Shape::Result;
        /// Manual panels: the preview's title, what a run returns, the button and the cost note.
        QString preview_title;
        QString preview_message;
        QString preview_run;
        QString preview_note;
        /// Optional second button on the preview (connect load_pane()->preview()'s secondary_clicked).
        QString preview_secondary;
    };
    virtual LoadPolicy load_policy() const;
    /// What a query needs (an API key …), re-evaluated on every show: missing → the "needs setup"
    /// page naming it (with OPEN SETTINGS > CREDENTIALS) instead of a run. Default: nothing.
    virtual QList<ui::Requirement> load_requirements() const { return {}; }
    /// Re-evaluate load_requirements() now (an input switched to a source that needs a key).
    void recheck_load_requirements();
    /// The remembered result no longer fits the inputs the panel set by itself (a calendar moved on
    /// to a new week): the query runs again on show.
    void invalidate_result();
    /// The saved value still waiting for an asynchronously filled combo / list ({"d": data, "t":
    /// text}), or an invalid QVariant when nothing waits.
    QVariant pending_input(QWidget* input) const;
    /// A small status label coloured by tone through the panel stylesheet (no setStyleSheet at run
    /// time): "ok" (success), "warn" (warning) or "" (secondary text).
    static void set_label_tone(QLabel* label, const char* tone);
    /// The results pane (preview actions, notices).
    ui::AutoLoadPane* load_pane() const { return pane_; }
    /// Panels whose VIEW selector shows different kinds of result: each view keeps its own remembered
    /// result (and, for a manual panel, its own preview — load_pane()->set_view_preview()). The view
    /// is entered with the current inputs' signature, so its remembered result is compared with them.
    void set_load_view(const QString& view);
    /// Per-view look and age when the views differ (a list of events vs a table, a daily feed vs a
    /// yearly one): skeleton shape and max age of `view` (max_age_s -1 = never refresh by itself).
    void set_view_load_policy(const QString& view, ui::LoadingSkeleton::Shape shape, int max_age_s);
    /// An input that only filters or picks within the result on screen (a series of the fetched
    /// category, a search box over a selector list) — not part of the query: it never makes the
    /// pane say "Inputs changed". It is still remembered across restarts.
    void mark_local_input(QWidget* input);

    /// A panel-specific content page (add_content_page) as JSON, so a remembered result can be shown
    /// again after a restart. Null = the page cannot be rebuilt from JSON (the query re-runs).
    virtual QJsonValue page_payload(int page) const;
    /// Rebuild content page `page` from page_payload(). False = not possible.
    virtual bool restore_page(int page, const QJsonValue& payload);
    /// The query in flight was cancelled (the user left the panel during an automatic load, pressed
    /// CANCEL, or started another one). Panels that keep their own per-request state reset it here.
    virtual void on_query_cancelled() {}

    /// Run the query now with the current inputs (what FETCH / Enter / RETRY do).
    void run_query();
    /// EconomicsService::execute() for the query in flight: tracked, so leaving the panel during an
    /// automatic load (or starting another query) kills the script and drops its answer.
    /// `fresh` drops the service's cached answer first (REFRESH), so the script runs again.
    void execute_query(const QString& source_id, const QString& script, const QString& command,
                       const QStringList& args, const QString& request_id, bool fresh = false);
    /// The query in flight waits for a service other than EconomicsService (the panel delivers or
    /// fails it itself) — otherwise a query that issued no request is reported as "nothing to fetch".
    void mark_query_pending();
    /// The query in flight was started automatically (first show, quiet refresh) rather than by FETCH.
    bool query_is_automatic() const;
    /// A query is in flight.
    bool query_running() const;

    /// Shape a large answer off the GUI thread: `work` runs on worker_pool() — it must be pure (capture
    /// values only: QJsonObject / QJsonArray / QString copies are cheap, never `this` or a widget) —
    /// and `then(result)` runs here afterwards, unless the panel is gone or the query that was in
    /// flight when offload() was called has been cancelled or superseded meanwhile (the result is
    /// then dropped). Outside a query, `then` always runs.
    template <typename Work, typename Then>
    void offload(Work work, Then then) {
        using Result = std::invoke_result_t<Work&>;
        const quint64 run = run_id_;
        if (run != 0)
            run_issued_ = true; // the query is busy, not "nothing to fetch"
        QPointer<EconPanelBase> self(this);
        (void)QtConcurrent::run(worker_pool(), std::move(work))
            .then(this, [self, run, then = std::move(then)](Result result) mutable {
                if (!self || (run != 0 && (run != self->run_id_ || !self->query_running())))
                    return; // the panel or its query is gone
                then(std::move(result));
            });
    }

    // ── State helpers ─────────────────────────────────────────────────────────
    /// Caption of the loading skeleton for the query in flight ("Fetching BCB: Selic…").
    void show_loading(const QString& msg = {});
    /// The query failed: a plain sentence and RETRY (a missing / rejected key or a rate limit gets
    /// its own page). With a result on screen the result stays and a notice says the refresh failed.
    void show_error(const QString& msg);
    /// Same, plus one extra action (e.g. "VIEW PLANS" when out of credits).
    void show_error_with_action(const QString& msg, const QString& action_label, std::function<void()> action);
    /// The query has nothing to show for these inputs (or the inputs are incomplete): a designed
    /// "no data" page saying `msg`. Outside a query (an intro text) it changes nothing.
    void show_empty(const QString& msg = {});
    void show_table();

    /// Populate the shared table + update stat cards. `column_order` (optional) fixes the column
    /// order — remaining keys follow alphabetically. Rows with a date column are shown newest first
    /// unless `keep_row_order` is set (event calendars want chronological order). The rows are
    /// analysed on a worker; the table, cards and chart fill when that is done (the query's result
    /// reaches the pane then).
    void display(const QJsonArray& rows, const QString& title = {}, const QStringList& column_order = {},
                 bool keep_row_order = false);
    void export_csv();

    /// Describe the series for the NEXT display() call (cleared after it).
    void set_series_meta(const SeriesMeta& meta);
    /// When the data was retrieved and whether it came from a cache. Captured automatically for
    /// EconomicsService results of this panel's source; panels with their own service call it.
    void set_retrieval_info(const QDateTime& fetched_utc, bool from_cache);
    /// Warning line under the title (e.g. "OFFLINE — showing cached data"). Cleared by display()
    /// unless set again after it; pass an empty string to clear.
    void set_banner(const QString& text);

    /// Append a subclass-owned page to the shared content stack (build it under content_host()).
    /// Returns the page index (>= 1; 0 = table page), or -1 if build_base_ui() has not run yet.
    int add_content_page(QWidget* page);
    /// Switch the content stack to a page returned by add_content_page() — the query's result.
    void show_content_page(int index);
    /// Parent for subclass content pages (inside the results pane).
    QWidget* content_host() const;

    /// Allow/forbid the LATEST · CHANGE · YoY · MIN · MAX · AVG · POINTS row. Call with `false`
    /// whenever the displayed rows are a cross-section (one row per country, wide year columns, …).
    /// Even when allowed, the row is only shown for a single, dated time series.
    void set_stats_visible(bool visible);

    /// Saved inputs whose combo/list was still empty at restore time (filled asynchronously) are kept
    /// pending; call this after populating such a widget to apply them.
    void reapply_pending_inputs();

    /// Per-provider accent color stylesheet (uses live theme tokens). Set once, before the panel's
    /// children exist; ThemeManager's re-style pass keeps it in step with theme and font changes.
    QString panel_style() const;

    /// Called once the new appearance has settled (ThemeManager::appearance_settled): re-renders
    /// what stylesheets do not reach (the table's colours). Override for extra widgets.
    virtual void refresh_panel_theme();

    // ── i18n ──────────────────────────────────────────────────────────────────
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    /// Re-apply tr() lookups to the shared base widgets (FETCH/CSV buttons, stat
    /// card labels, current status message, record count). Subclasses that have
    /// their own translatable widgets override retranslateUi() and call this base
    /// implementation at the end.
    virtual void retranslateUi();

    // ── Shared utilities ──────────────────────────────────────────────────────
    /// Filter a QListWidget by hiding items that don't contain text (case-insensitive).
    static void filter_list(QListWidget* list, const QString& text);

    // ── Reusable token-based style helpers for sidebar panels ─────────────────
    static QString sidebar_style();      // left panel background + border
    static QString section_hdr_style();  // section header bar (e.g. "COUNTRY")
    static QString section_lbl_style();  // section header label text
    static QString search_input_style(); // search/filter QLineEdit
    static QString ctrl_label_style();   // toolbar control labels ("YEARS", "PRESET")
    QString list_style() const;          // QListWidget using source accent color
    static QString notice_style();       // warning/info notice label
    /// Fixed width for a toolbar input sized for its text: `design_px` at the default font size,
    /// widened in step with the user's font size (and re-applied when it changes) so text never clips.
    static void set_text_width(QWidget* w, int design_px);

    // ── Shared widgets (accessible to subclasses) ─────────────────────────────
    QLabel* stat_latest_ = nullptr;
    QLabel* stat_change_ = nullptr;
    QLabel* stat_yoy_ = nullptr;
    QLabel* stat_min_ = nullptr;
    QLabel* stat_max_ = nullptr;
    QLabel* stat_avg_ = nullptr;
    QLabel* stat_count_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    QString source_id_;
    QString color_;

  private:
    void trigger_fetch(); // FETCH / Enter / RETRY → query_started() + run_query()
    // ── Results pane ──────────────────────────────────────────────────────────
    void configure_pane();            ///< once, on the first show
    void start_run(quint64 id);       ///< the pane's StartFn
    void cancel_run(quint64 id);      ///< the pane's CancelFn
    void finish_run();                ///< after a delivery / failure: stop what the run still waits for
    quint64 ensure_run();             ///< the run a result belongs to (adopts one when it arrived outside a run)
    void check_run_started(quint64 id);
    void queue_delivery();
    void flush_delivery();
    void render_payload(const QJsonValue& payload, bool from_cache);
    void schedule_preview_refresh();
    void refresh_preview();
    QList<QPair<QString, QString>> input_summary() const;
    void fit_toolbar();
    // ── Inputs signature ("Inputs changed — RUN AGAIN") ───────────────────────
    /// Signature of the query inputs as they are now (local inputs excluded).
    QString query_signature() const;
    /// The inputs changed: tell the pane, once they are settled (no saved value still waiting for
    /// its list) — coalesced to one update per event-loop turn.
    void schedule_signature();
    void watch_inputs();
    // ── Table / cards / chart ─────────────────────────────────────────────────
    /// Synchronous part of showing a table payload: provenance facts, title, banner cleared, the
    /// previous result's rows / cards / chart taken down. The rows themselves come with the analysis.
    void begin_table(const QJsonObject& table);
    /// Analyse `table` on the worker pool; `live_run` != 0: the analysis belongs to that query (its
    /// result is delivered to the pane once applied, and dropped if the query is cancelled).
    void start_table_analysis(const QJsonObject& table, quint64 live_run);
    void apply_table(const std::shared_ptr<const econ_table::Analysis>& analysis);
    void apply_stats();
    void apply_chart();
    /// Re-thin the chart lines for the visible x range (zoom / pan / resize).
    void schedule_chart_refine();
    void refine_chart();
    int chart_px() const;
    void render_page();
    void refresh_provenance();
    void apply_stats_visibility();
    void request_sort(int column, Qt::SortOrder order);
    void copy_rows(bool selection_only);
    QString default_source() const;
    QList<QWidget*> persistable_inputs() const;
    QHash<QString, QWidget*> keyed_inputs(QStringList* signature = nullptr) const;

    QWidget* container_ = nullptr;
    QWidget* toolbar_ = nullptr;
    QWidget* cards_row_ = nullptr;
    QWidget* yoy_card_ = nullptr;
    QWidget* title_bar_ = nullptr;
    ui::AutoLoadPane* pane_ = nullptr; // results area: remembered result / skeleton / preview / setup / error
    QStackedWidget* stack_ = nullptr;  // 0 = chart + table + pager, >= 1 = subclass content pages
    QLabel* asof_lbl_ = nullptr;       // "as of 14:02 · refreshing…" (kept by the pane)
    QTableView* table_ = nullptr;
    econ_table::EconPageModel* model_ = nullptr; // the page of rows the table shows
    QSplitter* results_split_ = nullptr;      // chart above, table below
    ui::TimeSeriesChart* chart_ = nullptr;    // hidden unless the result is a dated series
    QAction* copy_sel_act_ = nullptr;
    QAction* copy_all_act_ = nullptr;
    QAction* export_act_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* row_count_ = nullptr;
    QLabel* meta_lbl_ = nullptr;
    QLabel* banner_lbl_ = nullptr;
    ui::PaginationBar* pager_ = nullptr;

    // Stat-card title / sub labels (cached for retranslateUi)
    QLabel* stat_latest_lbl_ = nullptr;
    QLabel* stat_change_lbl_ = nullptr;
    QLabel* stat_yoy_lbl_ = nullptr;
    QLabel* stat_min_lbl_ = nullptr;
    QLabel* stat_max_lbl_ = nullptr;
    QLabel* stat_avg_lbl_ = nullptr;
    QLabel* stat_count_lbl_ = nullptr;
    QLabel* stat_latest_sub_ = nullptr;
    QLabel* stat_change_sub_ = nullptr;
    QLabel* stat_yoy_sub_ = nullptr;

    // ── Query in flight (one pane run) ────────────────────────────────────────
    bool pane_configured_ = false;
    quint64 run_id_ = 0;         ///< pane request id of the query in flight (0 = none)
    bool run_issued_ = false;    ///< the query sent a request (or said it would)
    bool adopting_ = false;      ///< a result arrived outside a run: open one without calling on_fetch()
    bool in_render_ = false;     ///< drawing a remembered result: no new delivery
    bool page_shown_in_render_ = false; ///< restore_page() showed its page itself
    QStringList run_requests_;   ///< EconomicsService request ids of the query in flight
    QJsonObject pending_;        ///< the result being assembled (table / page / custom), delivered once
    bool delivery_posted_ = false;
    QString rendered_serial_;    ///< serial of the payload drawn live (not drawn again by the pane)
    bool preview_posted_ = false;
    QString run_env_key_;        ///< credential the last failed request of the query named (BLS_API_KEY)

    // ── Rows on screen (from the last applied analysis) ───────────────────────
    std::shared_ptr<const econ_table::Analysis> analysis_;
    quint64 table_gen_ = 0;          ///< bumped by every table shown: an older analysis is not applied
    quint64 live_gen_ = 0;           ///< generation of the query's latest display()
    bool live_table_pending_ = false; ///< the query's own table is still being analysed (delivery waits)
    bool pending_table_drawn_ = false; ///< pending_'s table was drawn (its serial may mark it as on screen)
    quint64 sort_gen_ = 0;
    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::DescendingOrder;
    QString title_;

    // Chart: the analysis' full-resolution lines, plotted thinned to the pixel width.
    QVector<int> chart_series_;   ///< chart series handle per analysis line
    bool chart_refine_posted_ = false;

    // Series description + derived facts for the provenance line / stat cards.
    SeriesMeta next_meta_;    // set_series_meta() → consumed by the next display()
    SeriesMeta meta_;         // describes what is on screen now
    QString freq_label_;      // "Monthly" … (given or inferred)
    bool stats_allowed_ = true;
    QDateTime retrieved_at_; // UTC
    bool retrieved_from_cache_ = false;

    // Saved inputs not yet applied (their combo/list fills asynchronously).
    QVariantMap pending_inputs_;
    bool signature_posted_ = false;
    bool inputs_watched_ = false;
};

} // namespace fincept::screens
