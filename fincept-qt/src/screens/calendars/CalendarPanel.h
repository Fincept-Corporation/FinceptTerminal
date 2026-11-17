// src/screens/calendars/CalendarPanel.h
#pragma once
#include "services/fincept_data/FinceptDataModels.h"

#include <QDate>
#include <QDateTime>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVariantMap>
#include <QWidget>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QStackedWidget;
class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
} // namespace fincept::ui

namespace fincept::screens {

/// One sub-tab of the Calendars screen: earnings, IPOs, splits or economic events
/// from the Fincept /data API (GET /data/calendar/{kind}).
///
/// FROM / TO (default: today .. today + 7 days), an optional symbol filter where the
/// route has one, server-side paging (50 events per page, PREV / NEXT) and
/// click-to-sort columns. Double-clicking a row with a symbol opens it in Equity
/// Research.
///
/// Every /data call is billed, so nothing is requested by itself: the result area
/// is a ui::AutoLoadPane (Policy::Manual) that opens on the last loaded page with
/// its "as of" time (kept across sessions), or on a preview naming what LOAD will
/// fetch and what it costs. APPLY / LOAD / REFRESH / PREV / NEXT are the user's
/// requests; each carries a request id, so a late answer for an older query is
/// dropped.
class CalendarPanel : public QWidget {
    Q_OBJECT
  public:
    explicit CalendarPanel(services::fincept_data::CalendarKind kind, QWidget* parent = nullptr);
    ~CalendarPanel() override;

    services::fincept_data::CalendarKind kind() const { return kind_; }
    /// Stable key for saved state ("earnings", "ipos", "splits", "economic").
    QString key() const;
    /// The sub-tab is on screen: refresh the preview (the cost from the dataset
    /// catalogue, the dates). Requests nothing — every calendar call is billed.
    void activate();
    /// Re-fetch the page on screen (REFRESH / F5), or load the first page when
    /// nothing has been loaded yet.
    void refresh();

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// billing::PlanAccess price key of the /data calendar calls ("market_data").
    static QString plan_category();
    /// The account's plan includes calendar loads at no credit cost (the server's
    /// plan table says so; false without a table): loads then run like a free view.
    static bool plan_includes_loads();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void build_pane();
    void retranslate();
    /// Read the controls and load their page 1 (APPLY / Enter).
    void apply();
    /// The pane's start action: send the request for request `id`.
    void start_load(quint64 id);
    void on_answer(quint64 id, const services::fincept_data::DataResult& r);
    /// The pane's render action: draw a delivered or remembered page.
    void render_payload(const QJsonValue& payload, bool from_cache);
    /// False (with the reason) when the dates / range cannot be sent.
    bool inputs_valid(QString* why) const;
    /// Hash of the controls (dates, symbol, order) — the pane's input signature.
    QString input_signature() const;
    void update_signature();
    /// The preview of the empty pane: what LOAD fetches and what it costs.
    void update_preview();
    /// Cost, agent marker and policy from the plan table: Manual (every load billed)
    /// unless the plan includes market data, then AutoRun like a free view.
    void apply_plan_access();
    /// List price of one load from the dataset catalogue (0 = not known yet).
    double list_credits() const;
    /// Put the controls back to the query of the page on screen.
    void sync_controls_to_query();
    /// The designed page for a range with no events (instead of a bare header).
    void show_no_events();
    void populate();
    /// Per-column width modes: free text fills, everything else fits its content.
    void apply_column_fit();
    void update_pager();
    void update_freshness();
    void set_message(const QString& html, const QString& tone);
    void open_symbol_at(int row);
    void sort_by(int column);
    QStringList headers() const;
    int symbol_column() const;
    QString timing_text(const services::fincept_data::CalendarEvent& e) const;

    services::fincept_data::CalendarKind kind_;

    QWidget* header_ = nullptr; ///< dates / filter / refresh row
    QLabel* from_caption_ = nullptr;
    QLabel* to_caption_ = nullptr;
    QLabel* symbol_caption_ = nullptr;
    QLabel* sort_caption_ = nullptr;
    QDateEdit* from_edit_ = nullptr;
    QDateEdit* to_edit_ = nullptr;
    QLineEdit* symbol_edit_ = nullptr; ///< null where the route has no symbol filter
    QComboBox* sort_combo_ = nullptr;  ///< earnings only
    QPushButton* apply_btn_ = nullptr;
    QLabel* fresh_label_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* message_ = nullptr; ///< input problems (dates, unknown symbol with suggestions)
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* result_stack_ = nullptr; ///< table_ | no_events_
    QTableWidget* table_ = nullptr;
    ui::EmptyState* no_events_ = nullptr; ///< built on the first empty answer
    QWidget* pager_ = nullptr;
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QLabel* page_label_ = nullptr;
    QTimer* age_timer_ = nullptr;

    // The query of the page on screen (paging and refresh apply to it, not to edited controls).
    QDate q_start_;
    QDate q_end_;
    QString q_symbol_;
    QString q_sort_;

    int want_page_ = 0;          ///< page the next run asks for (PREV / NEXT / APPLY); 0 = the page on screen
    bool want_refresh_ = false;  ///< the next run is a REFRESH
    bool service_used_ = false;
    bool service_connected_ = false;
    bool has_data_ = false;
    bool remembered_ = false;    ///< the page on screen is a saved copy from an earlier session
    bool syncing_controls_ = false;
    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;
    services::fincept_data::CalendarPage data_;
    services::fincept_data::Pagination pagination_;
    services::fincept_data::DataResult last_ok_;
};

} // namespace fincept::screens
