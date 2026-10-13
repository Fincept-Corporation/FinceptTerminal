// src/screens/economics/panels/EconPanelBase.h
// Base class for all economics source panels.
// Provides: stat cards + empty/loading/error(RETRY) state + sortable, paged data table + CSV/clipboard
// export + a provenance line (units · frequency · seasonal adjustment · source · latest observation ·
// retrieval time) + generic persistence of the panel's query inputs.
// Subclasses call build_base_ui() from their constructor.
// For panels with a left selector sidebar, pass the right-side container
// to build_base_ui(container) instead.
#pragma once

#include "services/economics/EconomicsService.h"
#include "ui/widgets/PaginationBar.h"

#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVariantMap>
#include <QWidget>

#include <functional>

class QAction;

namespace fincept::screens {

class EconPanelBase : public QWidget {
    Q_OBJECT
  public:
    explicit EconPanelBase(const QString& source_id, const QString& color, QWidget* parent = nullptr);

    /// Called by EconomicsScreen when user switches to this panel.
    virtual void activate() = 0;

    /// True while the panel shows fetched data (table or a panel-specific content page) or has a
    /// request in flight. EconomicsScreen then skips activate() on re-selection — activate() resets
    /// the panel to its intro text, which used to wipe the table (and the loading state) every time
    /// the user tabbed away and back.
    bool keeps_state_on_activate() const;

    /// Panel-level state for persistence. The default implementation remembers every query input the
    /// panel owns — toolbar and sidebar combos, text fields, spin/date boxes, list selections — so the
    /// last query comes back after a restart. Override only to add state that is not held in a widget.
    virtual QVariantMap save_panel_state() const;
    virtual void restore_panel_state(const QVariantMap& state);

    /// Compact number for cards/labels: 21.43T / 2.14B / 3.51M / 12,345 / 310.326.
    static QString format_number(double v);

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

    // ── State helpers ─────────────────────────────────────────────────────────
    // Empty msg → a translated default is used (see .cpp).
    void show_loading(const QString& msg = {});
    /// Error state with a RETRY button that re-runs on_fetch().
    void show_error(const QString& msg);
    /// Error state with RETRY plus one extra action (e.g. "VIEW PLANS" when out of credits).
    void show_error_with_action(const QString& msg, const QString& action_label, std::function<void()> action);
    void show_empty(const QString& msg = {});
    void show_table();

    /// Populate the shared table + update stat cards. `column_order` (optional) fixes the column
    /// order — remaining keys follow alphabetically. Rows with a date column are shown newest first
    /// unless `keep_row_order` is set (event calendars want chronological order).
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

    /// Append a subclass-owned page to the shared content stack.
    /// Returns the page index (>= 2; 0 = status page, 1 = table page), or -1
    /// if build_base_ui() has not run yet.
    int add_content_page(QWidget* page);
    /// Switch the content stack to a page returned by add_content_page().
    void show_content_page(int index);

    /// Allow/forbid the LATEST · CHANGE · YoY · MIN · MAX · AVG · POINTS row. Call with `false`
    /// whenever the displayed rows are a cross-section (one row per country, wide year columns, …).
    /// Even when allowed, the row is only shown for a single, dated time series.
    void set_stats_visible(bool visible);

    /// Saved inputs whose combo/list was still empty at restore time (filled asynchronously) are kept
    /// pending; call this after populating such a widget to apply them.
    void reapply_pending_inputs();

    /// Per-provider accent color stylesheet (uses live theme tokens).
    QString panel_style() const;

    /// Called when theme changes — re-applies panel_style(). Override for extra widgets.
    virtual void refresh_panel_theme();

    // ── i18n ──────────────────────────────────────────────────────────────────
    void changeEvent(QEvent* event) override;
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
    void trigger_fetch(); // FETCH / Enter / RETRY → query_started() + on_fetch()
    void update_stats();
    void render_page();
    void refresh_provenance();
    void apply_stats_visibility();
    void sort_rows(int column, Qt::SortOrder order);
    void copy_rows(bool selection_only) const;
    QString default_source() const;
    QList<QWidget*> persistable_inputs() const;
    QHash<QString, QWidget*> keyed_inputs(QStringList* signature = nullptr) const;

    QWidget* container_ = nullptr;
    QWidget* cards_row_ = nullptr;
    QWidget* yoy_card_ = nullptr;
    QWidget* title_bar_ = nullptr;
    QStackedWidget* stack_ = nullptr; // 0=empty/status  1=table+pager
    QLabel* empty_lbl_ = nullptr;
    QPushButton* retry_btn_ = nullptr;
    QPushButton* action_btn_ = nullptr;
    std::function<void()> action_cb_;
    QTableWidget* table_ = nullptr;
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

    // Last status shown — so a language switch can re-render the message.
    enum class StatusKind { Empty, Loading, Error };
    StatusKind status_kind_ = StatusKind::Empty;
    QString status_msg_; // raw message last passed to show_empty/loading/error

    QJsonArray all_rows_;
    QStringList columns_;
    QString title_;

    // Series description + derived facts for the provenance line / stat cards.
    SeriesMeta next_meta_;    // set_series_meta() → consumed by the next display()
    SeriesMeta meta_;         // describes what is on screen now
    QString latest_period_;   // most recent observation period actually present
    QString stats_column_;    // column the cards summarise
    QString freq_label_;      // "Monthly" … (given or inferred)
    int excluded_projections_ = 0;
    bool stats_allowed_ = true;
    bool stats_applicable_ = false;
    QDateTime retrieved_at_; // UTC
    bool retrieved_from_cache_ = false;
    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::DescendingOrder;

    // Saved inputs not yet applied (their combo/list fills asynchronously).
    QVariantMap pending_inputs_;
};

} // namespace fincept::screens
