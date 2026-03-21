#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>

namespace fincept::ui { class TabHeader; class TabFooter; }

namespace fincept::screens {

/// Support ticket system — list / create / detail views matching Tauri UX.
class SupportScreen : public QWidget {
    Q_OBJECT
public:
    explicit SupportScreen(QWidget* parent = nullptr);

private:
    // ── Views ─────────────────────────────────────────────────────────────────
    QStackedWidget* pages_       = nullptr;

    // Header / Footer
    ui::TabHeader* tab_header_   = nullptr;
    ui::TabFooter* tab_footer_   = nullptr;

    // Header stats
    QLabel* stat_total_  = nullptr;
    QLabel* stat_open_   = nullptr;
    QLabel* stat_resolved_ = nullptr;
    QLabel* status_dot_  = nullptr;
    QLabel* status_lbl_  = nullptr;

    // Toolbar buttons (shown/hidden per view)
    QPushButton* refresh_btn_    = nullptr;
    QPushButton* new_ticket_btn_ = nullptr;
    QPushButton* back_btn_       = nullptr;

    // List view
    QWidget*     ticket_container_ = nullptr;   // inner VBox holding ticket rows
    QScrollArea* list_scroll_      = nullptr;

    // Create view
    QLineEdit*   subject_input_   = nullptr;
    QComboBox*   category_combo_  = nullptr;
    QComboBox*   priority_combo_  = nullptr;
    QTextEdit*   desc_input_      = nullptr;
    QPushButton* create_btn_      = nullptr;

    // Detail view
    QLabel*    detail_header_lbl_  = nullptr;   // "TICKET DETAILS - #N"
    QLabel*    detail_subject_lbl_ = nullptr;
    QLabel*    detail_meta_lbl_    = nullptr;
    QLabel*    detail_body_lbl_    = nullptr;
    QWidget*   messages_container_ = nullptr;
    QScrollArea* detail_scroll_    = nullptr;
    QTextEdit* msg_input_          = nullptr;
    QPushButton* send_btn_         = nullptr;
    QPushButton* close_btn_        = nullptr;
    QPushButton* reopen_btn_       = nullptr;
    QLabel*    demo_banner_        = nullptr;
    QWidget*   reply_box_          = nullptr;
    QWidget*   demo_box_           = nullptr;

    // State
    int  selected_ticket_id_    = -1;
    bool selected_is_demo_      = false;
    bool selected_is_closed_    = false;

    // ── Builders ─────────────────────────────────────────────────────────────
    QWidget* build_list_page();
    QWidget* build_create_page();
    QWidget* build_detail_page();

    // ── Helpers ───────────────────────────────────────────────────────────────
    void set_busy(bool busy);
    void switch_to(int page_idx);   // 0=list, 1=create, 2=detail
    static QString status_color(const QString& s);
    static QString priority_color(const QString& p);

    // ── Slots ─────────────────────────────────────────────────────────────────
    void load_tickets();
    void on_create_ticket();
    void on_send_message();
    void on_close_ticket();
    void on_reopen_ticket();
};

} // namespace fincept::screens
