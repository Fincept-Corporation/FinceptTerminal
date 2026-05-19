#pragma once
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// Support — two-panel layout: ticket sidebar + detail/create content area.
///
/// Internationalised: static chrome (top bar, sidebar header, filter combo,
/// stat captions) flows through tr() with member pointers re-set in
/// retranslateUi(). The content pages (empty / create / detail) are
/// rebuilt from build_*() on QEvent::LanguageChange, and load_tickets()
/// re-runs to refresh sidebar rows. In-progress form input is preserved
/// across language switches by snapshotting + restoring text fields.
class SupportScreen : public QWidget {
    Q_OBJECT
  public:
    explicit SupportScreen(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

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
    QWidget* demo_box_ = nullptr;
    QWidget* closed_box_ = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    int selected_ticket_id_ = -1;
    bool selected_is_demo_ = false;
    bool selected_is_closed_ = false;
    QPushButton* active_row_btn_ = nullptr; // currently highlighted sidebar row

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

    // ── Slots ─────────────────────────────────────────────────────────────────
    void load_tickets();
    void on_create_ticket();
    void on_send_message();
    void on_close_ticket();
    void on_reopen_ticket();
};

} // namespace fincept::screens
