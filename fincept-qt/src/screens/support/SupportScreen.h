#pragma once
#include "screens/common/ISectionLinked.h"

#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

/// Support — two-panel layout: ticket sidebar + detail/create content area.
///
/// Internationalised: static chrome (top bar, sidebar header, filter combo,
/// stat captions) flows through tr() with member pointers re-set in
/// retranslateUi(). The content pages (empty / create / detail) are
/// rebuilt from build_*() on QEvent::LanguageChange, and load_tickets()
/// re-runs to refresh sidebar rows. In-progress form input is preserved
/// across language switches by snapshotting + restoring text fields.
class SupportScreen : public QWidget, public ISectionLinked {
    Q_OBJECT
  public:
    explicit SupportScreen(QWidget* parent = nullptr);

    /// ISectionLinked — "new" (the New Ticket form; focus = subject, typed in when the field is
    /// empty — nothing is sent until CREATE) | "tickets" (the ticket search box; focus = search text).
    /// "" + focus = search text.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void changeEvent(QEvent* event) override;
    /// Refresh the ticket list when the panel is shown again after a while —
    /// replies from the support team otherwise only appeared on a manual ↻.
    void showEvent(QShowEvent* event) override;

  private:
    // ── Layout ────────────────────────────────────────────────────────────────
    QSplitter* splitter_ = nullptr;
    QStackedWidget* content_stack_ = nullptr; // 0=empty, 1=create, 2=detail

    // ── Top bar ───────────────────────────────────────────────────────────────
    QLabel* title_lbl_ = nullptr;
    QLabel* breadcrumb_lbl_ = nullptr;

    // ── Sidebar ───────────────────────────────────────────────────────────────
    QPushButton* new_ticket_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QComboBox* filter_combo_ = nullptr;
    QWidget* ticket_container_ = nullptr;
    QScrollArea* list_scroll_ = nullptr;
    QLabel* stat_total_ = nullptr;
    QLabel* stat_open_ = nullptr;
    QLabel* stat_resolved_ = nullptr;
    // Stat captions ("Total" / "Open" / "Done") — separate from the counts.
    QLabel* stat_total_caption_ = nullptr;
    QLabel* stat_open_caption_ = nullptr;
    QLabel* stat_resolved_caption_ = nullptr;

    // ── Status bar ────────────────────────────────────────────────────────────
    QLabel* status_dot_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // ── Create view ───────────────────────────────────────────────────────────
    QLineEdit* subject_input_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QComboBox* priority_combo_ = nullptr;
    QTextEdit* desc_input_ = nullptr;
    QPushButton* create_btn_ = nullptr;
    QLabel* char_count_lbl_ = nullptr;

    // ── Detail view ───────────────────────────────────────────────────────────
    QLabel* detail_id_lbl_ = nullptr;
    QLabel* detail_subject_lbl_ = nullptr;
    QLabel* detail_status_lbl_ = nullptr;
    QLabel* detail_meta_lbl_ = nullptr;
    QLabel* detail_body_lbl_ = nullptr;
    QWidget* messages_container_ = nullptr;
    QScrollArea* detail_scroll_ = nullptr;
    QTextEdit* msg_input_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    QPushButton* reopen_btn_ = nullptr;
    QWidget* reply_box_ = nullptr;
    QWidget* closed_box_ = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    int selected_ticket_id_ = -1;
    bool selected_is_closed_ = false;
    QPushButton* active_row_btn_ = nullptr; // currently highlighted sidebar row

    /// Last successfully-fetched ticket set. Cached so the search box and status
    /// filter can re-render the sidebar without a network round-trip, and kept
    /// (last-known-good) when a refresh fails.
    QJsonArray all_tickets_;
    bool load_failed_ = false;
    /// Wall-clock ms of the last load_tickets() call; showEvent() only refetches
    /// when the data is older than a minute.
    qint64 last_load_ms_ = 0;
    /// Categories the support API returned as (display text, API key) pairs.
    /// Kept so a language switch — which rebuilds the create page with its
    /// default category list — can restore the server-provided set.
    QList<QPair<QString, QString>> server_categories_;
    /// Fill category_combo_ from server_categories_ (no-op until the API answered).
    void populate_category_combo();

    // ── Designed states (never a blank pane) ─────────────────────────────────
    /// Sidebar list area: 0 = the ticket rows (list_scroll_), then a skeleton
    /// and a compact EmptyState, both created on first use.
    QStackedWidget* list_stack_ = nullptr;
    ui::LoadingSkeleton* list_skeleton_ = nullptr;
    ui::EmptyState* list_state_ = nullptr;
    enum class ListAction { None, Retry, ClearFilters };
    ListAction list_action_ = ListAction::None;
    /// Content page 0 ("home"): a designed EmptyState, or a skeleton while the
    /// first list is on its way. Rebuilt with the page (retranslateUi).
    ui::EmptyState* home_state_ = nullptr;
    ui::LoadingSkeleton* home_skeleton_ = nullptr;
    enum class HomeAction { None, Create, Retry };
    HomeAction home_action_ = HomeAction::None;
    /// Shown instead of the splitter while signed out (sign-in requirement
    /// page) or while the saved sign-in is still being checked (skeleton).
    ui::EmptyState* signin_state_ = nullptr;
    ui::LoadingSkeleton* signin_skeleton_ = nullptr;
    /// The account whose tickets are shown ("" = signed out / not loaded).
    QString account_key_;
    /// A ticket list reply arrived for this account (an empty list included).
    bool tickets_loaded_ = false;
    bool list_in_flight_ = false;
    /// Bumped by reset_tickets(): a list reply started for an earlier account is dropped.
    quint64 account_generation_ = 0;
    /// Raw failure text of the last list load (behind the error page's DETAILS).
    QString last_error_;
    /// Ticket just created: opened when the refreshed list arrives.
    int open_after_load_id_ = -1;

    /// Signed in → the screen; signed out → the sign-in requirement page;
    /// still checking the saved sign-in → a skeleton. Starts the first load and
    /// drops a previous account's tickets on an account change.
    void apply_auth_gate();
    /// Categories + tickets for the signed-in account (both in parallel).
    void start_initial_load();
    /// Forget the tickets, the selection and any half-written text (sign-out / account change).
    void reset_tickets();
    /// Content page 0's state for the current list state.
    void update_home_page();
    /// Back / Cancel from the create page: the ticket that was open, else the
    /// default ticket, else page 0.
    void show_home();
    /// Open the first open / in-progress ticket, else the newest one. False when there is none.
    bool open_default_ticket();
    void on_list_state_action();
    void on_home_action();

    // ── Builders ─────────────────────────────────────────────────────────────
    QWidget* build_sidebar();
    QWidget* build_empty_state();
    QWidget* build_create_page();
    QWidget* build_detail_page();

    // ── Helpers ───────────────────────────────────────────────────────────────
    void set_busy(bool busy);
    void select_ticket_row(QPushButton* btn);
    void apply_styles();
    void retranslateUi();
    static QString status_color(const QString& s);
    static QString priority_color(const QString& p);
    static QString status_label(const QString& s);

    /// Re-render the sidebar rows from all_tickets_, honouring the search box
    /// and the status filter combo. Called by load_tickets() and whenever
    /// either control changes — no refetch.
    void rebuild_ticket_rows();

    /// Show a ticket from the cached list in the detail pane, then fetch its
    /// full record (description + conversation) from the API.
    void open_ticket(const QJsonObject& ticket);
    void load_ticket_details(int ticket_id);
    /// Header / meta / reply-box state for `status` ("open", "resolved", …).
    void apply_detail_status(const QString& status);

    // ── Slots ─────────────────────────────────────────────────────────────────
    void load_tickets();
    void on_create_ticket();
    void on_send_message();
    void on_close_ticket();
    void on_reopen_ticket();
};

} // namespace fincept::screens
