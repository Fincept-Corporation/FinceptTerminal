#include "screens/support/SupportScreen.h"
#include "auth/UserApi.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

namespace fincept::screens {

// ── Style constants ───────────────────────────────────────────────────────────

static const char* SS_INPUT =
    "QLineEdit, QTextEdit, QComboBox {"
    "  background: #080808; color: #e5e5e5;"
    "  border: 1px solid #222222; padding: 6px 8px;"
    "  font-size: 12px; font-family: 'Consolas','Courier New',monospace; }"
    "QLineEdit:focus, QTextEdit:focus { border-color: #333333; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #0a0a0a; color: #e5e5e5; border: 1px solid #222; }";

static const char* SS_BTN_AMBER =
    "QPushButton { background: #d97706; color: #000; border: none;"
    "  padding: 4px 14px; font-size: 11px; font-weight: bold;"
    "  font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #b45309; }"
    "QPushButton:disabled { background: #404040; color: #666; }";

static const char* SS_BTN_GREEN =
    "QPushButton { background: #16a34a; color: #000; border: none;"
    "  padding: 4px 14px; font-size: 11px; font-weight: bold;"
    "  font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #15803d; }"
    "QPushButton:disabled { background: #404040; color: #666; }";

static const char* SS_BTN_GRAY =
    "QPushButton { background: #1a1a1a; color: #808080; border: 1px solid #222;"
    "  padding: 4px 14px; font-size: 11px; font-weight: bold;"
    "  font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #222; color: #e5e5e5; }";

static const char* SS_BTN_RED_OUTLINE =
    "QPushButton { background: transparent; color: #dc2626; border: 1px solid #dc2626;"
    "  padding: 4px 10px; font-size: 10px; font-weight: bold;"
    "  font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #dc262620; }"
    "QPushButton:disabled { border-color: #404040; color: #404040; }";

static const char* SS_BTN_GREEN_OUTLINE =
    "QPushButton { background: transparent; color: #16a34a; border: 1px solid #16a34a;"
    "  padding: 4px 10px; font-size: 10px; font-weight: bold;"
    "  font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #16a34a20; }"
    "QPushButton:disabled { border-color: #404040; color: #404040; }";

static const char* SS_MONO =
    "font-family: 'Consolas','Courier New',monospace;";

// ── Helpers ───────────────────────────────────────────────────────────────────

QString SupportScreen::status_color(const QString& s) {
    QString l = s.toLower();
    if (l == "open" || l == "in_progress") return "#0891b2";
    if (l == "resolved" || l == "closed")  return "#16a34a";
    if (l == "pending")                    return "#ca8a04";
    return "#808080";
}

QString SupportScreen::priority_color(const QString& p) {
    QString l = p.toLower();
    if (l == "high" || l == "urgent") return "#dc2626";
    if (l == "medium")                return "#d97706";
    if (l == "low")                   return "#16a34a";
    return "#808080";
}

static QLabel* mono_label(const QString& text, const QString& color,
                           int size = 12, bool bold = false) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QString(
        "color: %1; font-size: %2px; background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;%3")
        .arg(color).arg(size).arg(bold ? " font-weight: bold;" : ""));
    return l;
}

static QWidget* hsep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setFixedHeight(1);
    f->setStyleSheet("background: #1a1a1a; border: none;");
    return f;
}

// ── Constructor ───────────────────────────────────────────────────────────────

SupportScreen::SupportScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    {
        auto* bar = new QWidget;
        bar->setFixedHeight(34);
        bar->setStyleSheet(QString(
            "background: %1; border-bottom: 1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
        auto* bl = new QHBoxLayout(bar);
        bl->setContentsMargins(14, 0, 14, 0);
        bl->setSpacing(8);

        refresh_btn_ = new QPushButton("↻  REFRESH");
        refresh_btn_->setFixedHeight(24);
        refresh_btn_->setStyleSheet(SS_BTN_AMBER);
        connect(refresh_btn_, &QPushButton::clicked, this, &SupportScreen::load_tickets);
        bl->addWidget(refresh_btn_);

        new_ticket_btn_ = new QPushButton("+  NEW TICKET");
        new_ticket_btn_->setFixedHeight(24);
        new_ticket_btn_->setStyleSheet(SS_BTN_GREEN);
        connect(new_ticket_btn_, &QPushButton::clicked, this, [this]() { switch_to(1); });
        bl->addWidget(new_ticket_btn_);

        back_btn_ = new QPushButton("←  BACK TO LIST");
        back_btn_->setFixedHeight(24);
        back_btn_->setStyleSheet(SS_BTN_GRAY);
        back_btn_->hide();
        connect(back_btn_, &QPushButton::clicked, this, [this]() { switch_to(0); });
        bl->addWidget(back_btn_);

        bl->addStretch();

        bl->addWidget(mono_label("TICKETS:", ui::colors::TEXT_TERTIARY, 11));
        stat_total_ = mono_label("0", ui::colors::CYAN, 11, true);
        bl->addWidget(stat_total_);
        bl->addWidget(mono_label("|", ui::colors::TEXT_TERTIARY, 11));
        bl->addWidget(mono_label("OPEN:", ui::colors::TEXT_TERTIARY, 11));
        stat_open_ = mono_label("0", "#ca8a04", 11, true);
        bl->addWidget(stat_open_);
        bl->addWidget(mono_label("|", ui::colors::TEXT_TERTIARY, 11));
        bl->addWidget(mono_label("RESOLVED:", ui::colors::TEXT_TERTIARY, 11));
        stat_resolved_ = mono_label("0", ui::colors::POSITIVE, 11, true);
        bl->addWidget(stat_resolved_);
        bl->addWidget(mono_label("|", ui::colors::TEXT_TERTIARY, 11));

        status_dot_ = new QLabel("●");
        status_dot_->setStyleSheet(QString(
            "color: %1; font-size: 14px; background: transparent;").arg(ui::colors::POSITIVE));
        bl->addWidget(status_dot_);

        status_lbl_ = mono_label("READY", ui::colors::POSITIVE, 11, true);
        bl->addWidget(status_lbl_);

        root->addWidget(bar);
    }

    // ── Paged content ─────────────────────────────────────────────────────────
    pages_ = new QStackedWidget;
    pages_->addWidget(build_list_page());    // 0
    pages_->addWidget(build_create_page()); // 1
    pages_->addWidget(build_detail_page()); // 2
    root->addWidget(pages_, 1);


    load_tickets();
}

// ── List page ─────────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_list_page() {
    auto* outer = new QWidget;
    outer->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(outer);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(8);

    auto* sec = mono_label("SUPPORT TICKETS", ui::colors::AMBER, 13, true);
    vl->addWidget(sec);
    vl->addWidget(hsep());

    list_scroll_ = new QScrollArea;
    list_scroll_->setWidgetResizable(true);
    list_scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    ticket_container_ = new QWidget;
    ticket_container_->setStyleSheet("background: transparent;");
    auto* tcl = new QVBoxLayout(ticket_container_);
    tcl->setContentsMargins(0, 0, 0, 0);
    tcl->setSpacing(6);
    tcl->addStretch();

    list_scroll_->setWidget(ticket_container_);
    vl->addWidget(list_scroll_, 1);

    return outer;
}

// ── Create page ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_create_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(8);

    vl->addWidget(mono_label("CREATE NEW TICKET", ui::colors::AMBER, 13, true));
    vl->addWidget(hsep());

    // Form card
    auto* card = new QWidget;
    card->setStyleSheet(
        "background: #0a0a0a; border: 1px solid #1a1a1a; border-left: 3px solid #16a34a;");
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(18, 14, 18, 16);
    cl->setSpacing(10);

    auto add_label = [&](const QString& t) {
        cl->addWidget(mono_label(t, ui::colors::TEXT_SECONDARY, 11, true));
    };

    add_label("SUBJECT *");
    subject_input_ = new QLineEdit;
    subject_input_->setPlaceholderText("Brief description of your issue");
    subject_input_->setFixedHeight(32);
    subject_input_->setStyleSheet(SS_INPUT);
    cl->addWidget(subject_input_);

    // Category + Priority row
    auto* cr = new QWidget;
    cr->setStyleSheet("background: transparent;");
    auto* crl = new QHBoxLayout(cr);
    crl->setContentsMargins(0, 0, 0, 0);
    crl->setSpacing(14);

    auto* cat_vl = new QVBoxLayout;
    cat_vl->setSpacing(4);
    cat_vl->addWidget(mono_label("CATEGORY *", ui::colors::TEXT_SECONDARY, 11, true));
    category_combo_ = new QComboBox;
    category_combo_->addItems({"technical", "billing", "feature", "bug", "account", "other"});
    category_combo_->setFixedHeight(32);
    category_combo_->setStyleSheet(SS_INPUT);
    cat_vl->addWidget(category_combo_);
    crl->addLayout(cat_vl);

    auto* pri_vl = new QVBoxLayout;
    pri_vl->setSpacing(4);
    pri_vl->addWidget(mono_label("PRIORITY", ui::colors::TEXT_SECONDARY, 11, true));
    priority_combo_ = new QComboBox;
    priority_combo_->addItems({"low", "medium", "high"});
    priority_combo_->setCurrentIndex(1);
    priority_combo_->setFixedHeight(32);
    priority_combo_->setStyleSheet(SS_INPUT);
    pri_vl->addWidget(priority_combo_);
    crl->addLayout(pri_vl);

    cl->addWidget(cr);

    add_label("DESCRIPTION *");
    desc_input_ = new QTextEdit;
    desc_input_->setPlaceholderText("Provide detailed information about your issue...");
    desc_input_->setMinimumHeight(200);
    desc_input_->setStyleSheet(SS_INPUT);
    cl->addWidget(desc_input_);

    create_btn_ = new QPushButton("CREATE TICKET");
    create_btn_->setFixedHeight(34);
    create_btn_->setStyleSheet(SS_BTN_GREEN);
    connect(create_btn_, &QPushButton::clicked, this, &SupportScreen::on_create_ticket);
    cl->addWidget(create_btn_);

    vl->addWidget(card);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

// ── Detail page ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_detail_page() {
    detail_scroll_ = new QScrollArea;
    detail_scroll_->setWidgetResizable(true);
    detail_scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(8);

    // Section title: "TICKET DETAILS - #N"
    detail_header_lbl_ = mono_label("TICKET DETAILS", ui::colors::AMBER, 13, true);
    vl->addWidget(detail_header_lbl_);
    vl->addWidget(hsep());

    // Demo banner (hidden by default)
    demo_banner_ = new QLabel(
        "DEMO TICKET — This shows how the support system works. "
        "Create your own ticket to get real support!");
    demo_banner_->setWordWrap(true);
    demo_banner_->setStyleSheet(
        "background: #2563eb; color: #000; font-size: 11px; font-weight: bold;"
        " padding: 8px 12px; font-family: 'Consolas','Courier New',monospace;");
    demo_banner_->hide();
    vl->addWidget(demo_banner_);

    // Ticket info card
    auto* info_card = new QWidget;
    info_card->setStyleSheet("background: #0a0a0a; border: 1px solid #1a1a1a;");
    auto* icl = new QVBoxLayout(info_card);
    icl->setContentsMargins(14, 12, 14, 12);
    icl->setSpacing(8);

    // Subject + action buttons row
    auto* subj_row = new QWidget;
    subj_row->setStyleSheet("background: transparent;");
    auto* srl = new QHBoxLayout(subj_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(8);

    detail_subject_lbl_ = mono_label("", ui::colors::TEXT_PRIMARY, 14, true);
    detail_subject_lbl_->setWordWrap(true);
    srl->addWidget(detail_subject_lbl_, 1);

    close_btn_ = new QPushButton("CLOSE");
    close_btn_->setFixedHeight(26);
    close_btn_->setStyleSheet(SS_BTN_RED_OUTLINE);
    close_btn_->hide();
    connect(close_btn_, &QPushButton::clicked, this, &SupportScreen::on_close_ticket);
    srl->addWidget(close_btn_);

    reopen_btn_ = new QPushButton("REOPEN");
    reopen_btn_->setFixedHeight(26);
    reopen_btn_->setStyleSheet(SS_BTN_GREEN_OUTLINE);
    reopen_btn_->hide();
    connect(reopen_btn_, &QPushButton::clicked, this, &SupportScreen::on_reopen_ticket);
    srl->addWidget(reopen_btn_);

    icl->addWidget(subj_row);

    // Meta: status | priority | category | created
    detail_meta_lbl_ = mono_label("", ui::colors::TEXT_SECONDARY, 11);
    icl->addWidget(detail_meta_lbl_);

    // Description body
    detail_body_lbl_ = new QLabel;
    detail_body_lbl_->setWordWrap(true);
    detail_body_lbl_->setTextFormat(Qt::PlainText);
    detail_body_lbl_->setStyleSheet(
        "color: #e5e5e5; font-size: 12px; background: #080808;"
        " border-left: 3px solid #2563eb; padding: 10px 12px;"
        " font-family: 'Consolas','Courier New',monospace;");
    icl->addWidget(detail_body_lbl_);

    vl->addWidget(info_card);

    // Conversation
    vl->addWidget(mono_label("CONVERSATION", ui::colors::AMBER, 12, true));
    vl->addWidget(hsep());

    auto* msg_scroll = new QScrollArea;
    msg_scroll->setWidgetResizable(true);
    msg_scroll->setMinimumHeight(160);
    msg_scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    messages_container_ = new QWidget;
    messages_container_->setStyleSheet("background: transparent;");
    auto* mcl = new QVBoxLayout(messages_container_);
    mcl->setContentsMargins(0, 0, 0, 0);
    mcl->setSpacing(6);
    mcl->addStretch();

    msg_scroll->setWidget(messages_container_);
    vl->addWidget(msg_scroll, 1);

    // Reply box
    reply_box_ = new QWidget;
    reply_box_->setStyleSheet(
        "background: #0a0a0a; border: 1px solid #1a1a1a; border-left: 3px solid #16a34a;");
    auto* rbl = new QVBoxLayout(reply_box_);
    rbl->setContentsMargins(14, 10, 14, 12);
    rbl->setSpacing(8);
    rbl->addWidget(mono_label("ADD MESSAGE", ui::colors::POSITIVE, 11, true));

    msg_input_ = new QTextEdit;
    msg_input_->setPlaceholderText("Type your message here...");
    msg_input_->setFixedHeight(80);
    msg_input_->setStyleSheet(SS_INPUT);
    rbl->addWidget(msg_input_);

    send_btn_ = new QPushButton("✉  SEND MESSAGE");
    send_btn_->setFixedHeight(30);
    send_btn_->setStyleSheet(SS_BTN_GREEN);
    connect(send_btn_, &QPushButton::clicked, this, &SupportScreen::on_send_message);
    rbl->addWidget(send_btn_);

    vl->addWidget(reply_box_);

    // Demo-ticket read-only notice
    demo_box_ = new QWidget;
    demo_box_->setStyleSheet(
        "background: #0a0a0a; border: 1px solid #2563eb; border-left: 3px solid #2563eb;");
    auto* dbl = new QVBoxLayout(demo_box_);
    dbl->setContentsMargins(14, 10, 14, 12);
    dbl->setSpacing(6);
    dbl->addWidget(mono_label("DEMO TICKET — READ ONLY", "#2563eb", 11, true));
    dbl->addWidget(mono_label(
        "This is a demonstration ticket. You cannot add messages here.\n"
        "Click BACK TO LIST → + NEW TICKET to create a real support request.",
        ui::colors::TEXT_TERTIARY, 11));
    demo_box_->hide();
    vl->addWidget(demo_box_);

    detail_scroll_->setWidget(page);
    return detail_scroll_;
}

// ── switch_to ─────────────────────────────────────────────────────────────────

void SupportScreen::switch_to(int idx) {
    pages_->setCurrentIndex(idx);
    bool on_list   = (idx == 0);
    bool on_create = (idx == 1);
    bool on_detail = (idx == 2);

    refresh_btn_->setVisible(on_list);
    new_ticket_btn_->setVisible(on_list);
    back_btn_->setVisible(on_create || on_detail);

    // Update footer view label
    const char* view_name = on_list ? "LIST" : on_create ? "CREATE" : "DETAILS";
}

// ── set_busy ──────────────────────────────────────────────────────────────────

void SupportScreen::set_busy(bool busy) {
    QString col = busy ? ui::colors::AMBER : ui::colors::POSITIVE;
    status_dot_->setStyleSheet(QString(
        "color: %1; font-size: 14px; background: transparent;").arg(col));
    status_lbl_->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-weight: bold; background: transparent;"
        " font-family: 'Consolas','Courier New',monospace;").arg(col));
    status_lbl_->setText(busy ? "UPDATING…" : "READY");
    refresh_btn_->setEnabled(!busy);
    create_btn_->setEnabled(!busy);
    send_btn_->setEnabled(!busy);

    // Update footer status
}

// ── load_tickets ──────────────────────────────────────────────────────────────

void SupportScreen::load_tickets() {
    set_busy(true);

    auth::UserApi::instance().get_tickets([this](auth::ApiResponse r) {
        set_busy(false);

        // Clear existing ticket rows (keep the trailing stretch)
        auto* lay = qobject_cast<QVBoxLayout*>(ticket_container_->layout());
        if (!lay) return;
        while (lay->count() > 1)
            delete lay->takeAt(0)->widget();

        QJsonArray tickets;
        if (r.success) {
            auto data = r.data;
            if (data.contains("data")) data = data["data"].toObject();
            tickets = data["tickets"].toArray();
        }

        // Inject demo ticket at top
        QJsonObject demo;
        demo["id"]          = "DEMO-001";
        demo["subject"]     = "Welcome to Fincept Support";
        demo["status"]      = "resolved";
        demo["priority"]    = "low";
        demo["category"]    = "general";
        demo["created_at"]  = "2026-01-01T00:00:00Z";

        QJsonArray all;
        all.append(demo);
        for (const auto& v : tickets) all.append(v);

        // Stats
        int open_count = 0, resolved_count = 0;
        for (const auto& v : all) {
            QString st = v.toObject()["status"].toString().toLower();
            if (st == "open" || st == "in_progress" || st == "pending") ++open_count;
            else if (st == "resolved" || st == "closed")                 ++resolved_count;
        }
        stat_total_->setText(QString::number(all.size()));
        stat_open_->setText(QString::number(open_count));
        stat_resolved_->setText(QString::number(resolved_count));

        // Build ticket rows
        for (const auto& v : all) {
            auto obj        = v.toObject();
            QString id      = obj["id"].toVariant().toString();
            QString subject = obj["subject"].toString();
            QString status  = obj["status"].toString();
            QString priority= obj["priority"].toString();
            QString category= obj["category"].toString();
            QString created = obj["created_at"].toString();
            bool is_demo    = (id == "DEMO-001");

            // Format date
            QString date_str = "N/A";
            if (!created.isEmpty()) {
                QDateTime dt = QDateTime::fromString(created, Qt::ISODate);
                if (dt.isValid())
                    date_str = dt.toLocalTime().toString("MMM d, yyyy hh:mm");
            }

            auto* row = new QWidget;
            row->setStyleSheet(QString(
                "background: #0a0a0a;"
                "border: 1px solid %1;"
                "border-left: 3px solid %2;")
                .arg(is_demo ? "#2563eb" : "#1a1a1a",
                     status_color(status)));
            row->setCursor(Qt::PointingHandCursor);

            auto* rl = new QGridLayout(row);
            rl->setContentsMargins(12, 10, 12, 10);
            rl->setHorizontalSpacing(16);
            rl->setVerticalSpacing(2);

            // Col 0: ID
            rl->addWidget(mono_label("#" + id, ui::colors::TEXT_TERTIARY, 11), 0, 0);

            // Col 1: Subject (stretchy)
            auto* subj = mono_label(subject, ui::colors::TEXT_PRIMARY, 12, true);
            subj->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            rl->addWidget(subj, 0, 1);

            // Col 2: Status badge
            auto* st_lbl = mono_label(status.toUpper(), status_color(status), 10, true);
            rl->addWidget(st_lbl, 0, 2);

            // Col 3: Priority badge
            auto* pr_lbl = new QLabel(priority.toUpper());
            pr_lbl->setStyleSheet(QString(
                "background: %1; color: #000; font-size: 9px; font-weight: bold;"
                " padding: 2px 6px; font-family: 'Consolas','Courier New',monospace;")
                .arg(priority_color(priority)));
            pr_lbl->setFixedHeight(18);
            rl->addWidget(pr_lbl, 0, 3);

            // Col 4: Category
            rl->addWidget(mono_label(category.toUpper(), ui::colors::CYAN, 10), 0, 4);

            // Col 5: Date
            rl->addWidget(mono_label(date_str, ui::colors::TEXT_TERTIARY, 10), 0, 5);

            rl->setColumnStretch(1, 1);

            // Demo badge
            if (is_demo) {
                auto* badge = new QLabel("DEMO");
                badge->setStyleSheet(
                    "background: #2563eb; color: #000; font-size: 9px; font-weight: bold;"
                    " padding: 2px 6px; font-family: 'Consolas','Courier New',monospace;");
                rl->addWidget(badge, 0, 6);
            }

            const int     id_copy   = is_demo ? -1 : id.toInt();
            const QString sub_copy  = subject;
            const QString st_copy   = status;
            const QString pr_copy   = priority;
            const QString cat_copy  = category;
            const QString cr_copy   = date_str;
            const QString body_copy = obj["description"].toString();
            const bool    demo_copy = is_demo;

            // Wrap row in a flat QPushButton for click + hover handling
            auto* btn = new QPushButton;
            btn->setFlat(true);
            btn->setStyleSheet(
                "QPushButton { border: none; background: transparent; padding: 0; }"
                "QPushButton:hover > QWidget { background: #111111; }");
            auto* inner_lay = new QHBoxLayout(btn);
            inner_lay->setContentsMargins(0, 0, 0, 0);
            row->setParent(btn);
            inner_lay->addWidget(row);

            connect(btn, &QPushButton::clicked, this, [=]() {
                selected_ticket_id_ = id_copy;
                selected_is_demo_   = demo_copy;
                selected_is_closed_ = (st_copy.toLower() == "closed" ||
                                       st_copy.toLower() == "resolved");

                detail_header_lbl_->setText(
                    QString("TICKET DETAILS  —  #%1").arg(demo_copy ? "DEMO-001" : QString::number(id_copy)));
                detail_subject_lbl_->setText(sub_copy);
                detail_meta_lbl_->setText(
                    QString("Status: %1   |   Priority: %2   |   Category: %3   |   Created: %4")
                    .arg(st_copy.toUpper(), pr_copy.toUpper(),
                         cat_copy.toUpper(), cr_copy));
                detail_body_lbl_->setText(body_copy.isEmpty()
                    ? "(No description provided)"
                    : body_copy);

                demo_banner_->setVisible(demo_copy);
                reply_box_->setVisible(!demo_copy && !selected_is_closed_);
                demo_box_->setVisible(demo_copy);
                close_btn_->setVisible(!demo_copy && !selected_is_closed_);
                reopen_btn_->setVisible(!demo_copy && selected_is_closed_);

                // Clear messages
                auto* mcl2 = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                while (mcl2->count() > 1)
                    delete mcl2->takeAt(0)->widget();

                if (demo_copy) {
                    // Add a canned support message
                    auto* m = new QWidget;
                    m->setStyleSheet(
                        "background: #0a0a0a; border: 1px solid #1a1a1a;"
                        " border-left: 3px solid #9333ea;");
                    auto* ml = new QVBoxLayout(m);
                    ml->setContentsMargins(12, 8, 12, 10);
                    ml->setSpacing(4);
                    auto* mh = new QHBoxLayout;
                    mh->addWidget(mono_label("SUPPORT TEAM", "#9333ea", 11, true));
                    mh->addStretch();
                    mh->addWidget(mono_label("Jan 1, 2026 00:01", ui::colors::TEXT_TERTIARY, 10));
                    ml->addLayout(mh);
                    ml->addWidget(mono_label(
                        "Welcome! This demo ticket shows how our support system works.\n"
                        "Create a real ticket and our team will respond within 24 hours.",
                        ui::colors::TEXT_PRIMARY, 11));
                    mcl2->insertWidget(mcl2->count() - 1, m);
                } else {
                    // Load real messages from API
                    set_busy(true);
                    auth::UserApi::instance().get_ticket_details(id_copy,
                        [this](auth::ApiResponse dr) {
                            set_busy(false);
                            if (!dr.success) return;

                            auto msgs = dr.data["messages"].toArray();
                            if (msgs.isEmpty()) msgs = dr.data["data"].toObject()["messages"].toArray();

                            auto* mcl3 = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                            while (mcl3->count() > 1)
                                delete mcl3->takeAt(0)->widget();

                            for (const auto& mv : msgs) {
                                auto mo = mv.toObject();
                                bool is_user = mo["sender_type"].toString() == "user";
                                QString sender = is_user ? "YOU" : "SUPPORT TEAM";
                                QString msg_color = is_user ? ui::colors::CYAN : "#9333ea";
                                QString border_color = msg_color;

                                QString ts;
                                QDateTime mdt = QDateTime::fromString(
                                    mo["created_at"].toString(), Qt::ISODate);
                                if (mdt.isValid())
                                    ts = mdt.toLocalTime().toString("MMM d, yyyy hh:mm");

                                auto* m = new QWidget;
                                m->setStyleSheet(QString(
                                    "background: #0a0a0a; border: 1px solid #1a1a1a;"
                                    " border-left: 3px solid %1;").arg(border_color));
                                auto* ml = new QVBoxLayout(m);
                                ml->setContentsMargins(12, 8, 12, 10);
                                ml->setSpacing(4);
                                auto* mh = new QHBoxLayout;
                                mh->addWidget(mono_label(sender, msg_color, 11, true));
                                mh->addStretch();
                                mh->addWidget(mono_label(ts, ui::colors::TEXT_TERTIARY, 10));
                                ml->addLayout(mh);
                                ml->addWidget(mono_label(
                                    mo["message"].toString(),
                                    ui::colors::TEXT_PRIMARY, 11));
                                mcl3->insertWidget(mcl3->count() - 1, m);
                            }
                        });
                }

                switch_to(2);
            });

            lay->insertWidget(lay->count() - 1, btn);
        }

        if (all.isEmpty()) {
            lay->insertWidget(0,
                mono_label("No tickets found.", ui::colors::TEXT_TERTIARY, 12));
        }
    });
}

// ── on_create_ticket ──────────────────────────────────────────────────────────

void SupportScreen::on_create_ticket() {
    QString subject = subject_input_->text().trimmed();
    QString desc    = desc_input_->toPlainText().trimmed();
    if (subject.isEmpty() || desc.isEmpty()) return;

    set_busy(true);
    create_btn_->setText("CREATING…");

    auth::UserApi::instance().create_ticket(
        subject, desc,
        category_combo_->currentText(),
        priority_combo_->currentText(),
        [this](auth::ApiResponse r) {
            set_busy(false);
            create_btn_->setText("CREATE TICKET");
            if (r.success) {
                subject_input_->clear();
                desc_input_->clear();
                switch_to(0);
                load_tickets();
            }
        });
}

// ── on_send_message ───────────────────────────────────────────────────────────

void SupportScreen::on_send_message() {
    if (selected_ticket_id_ < 0 || selected_is_demo_) return;
    QString msg = msg_input_->toPlainText().trimmed();
    if (msg.isEmpty()) return;

    set_busy(true);
    send_btn_->setText("SENDING…");

    auth::UserApi::instance().add_ticket_message(
        selected_ticket_id_, msg,
        [this, msg](auth::ApiResponse r) {
            set_busy(false);
            send_btn_->setText("✉  SEND MESSAGE");
            if (r.success) {
                msg_input_->clear();
                auto* mcl = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                if (!mcl) return;
                auto* m = new QWidget;
                m->setStyleSheet(
                    "background: #0a0a0a; border: 1px solid #1a1a1a;"
                    " border-left: 3px solid #0891b2;");
                auto* ml = new QVBoxLayout(m);
                ml->setContentsMargins(12, 8, 12, 10);
                ml->setSpacing(4);
                auto* mh = new QHBoxLayout;
                mh->addWidget(mono_label("YOU", ui::colors::CYAN, 11, true));
                mh->addStretch();
                mh->addWidget(mono_label(
                    QDateTime::currentDateTime().toString("MMM d, yyyy hh:mm"),
                    ui::colors::TEXT_TERTIARY, 10));
                ml->addLayout(mh);
                ml->addWidget(mono_label(msg, ui::colors::TEXT_PRIMARY, 11));
                mcl->insertWidget(mcl->count() - 1, m);
            }
        });
}

// ── on_close_ticket / on_reopen_ticket ───────────────────────────────────────

void SupportScreen::on_close_ticket() {
    if (selected_ticket_id_ < 0) return;
    set_busy(true);
    auth::UserApi::instance().update_ticket_status(
        selected_ticket_id_, "closed",
        [this](auth::ApiResponse r) {
            set_busy(false);
            if (r.success) {
                selected_is_closed_ = true;
                close_btn_->hide();
                reopen_btn_->show();
                reply_box_->hide();
                switch_to(0);
                load_tickets();
            }
        });
}

void SupportScreen::on_reopen_ticket() {
    if (selected_ticket_id_ < 0) return;
    set_busy(true);
    auth::UserApi::instance().update_ticket_status(
        selected_ticket_id_, "open",
        [this](auth::ApiResponse r) {
            set_busy(false);
            if (r.success) {
                selected_is_closed_ = false;
                reopen_btn_->hide();
                close_btn_->show();
                reply_box_->show();
                switch_to(0);
                load_tickets();
            }
        });
}

} // namespace fincept::screens
