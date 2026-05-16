// src/screens/support/SupportScreen_Layout.cpp
//
// UI construction: build_sidebar, build_empty_state, build_create_page,
// build_detail_page, select_ticket_row, set_busy.
//
// Part of the partial-class split of SupportScreen.cpp.

#include "screens/support/SupportScreen.h"
#include "screens/support/SupportScreen_internal.h"

#include "auth/UserApi.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::screens::support_internal;

QWidget* SupportScreen::build_sidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setMinimumWidth(240);
    sidebar->setMaximumWidth(360);
    sidebar->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(sidebar);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Sidebar header ────────────────────────────────────────────────────────
    {
        auto* hdr = new QWidget(this);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* hl = new QVBoxLayout(hdr);
        hl->setContentsMargins(14, 10, 14, 10);
        hl->setSpacing(8);

        // New ticket button — full width
        auto* new_btn = new QPushButton("＋  New Ticket");
        new_btn->setFixedHeight(34);
        new_btn->setStyleSheet(SS_BTN_PRIMARY());
        connect(new_btn, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(1); });
        hl->addWidget(new_btn);

        // Search
        search_input_ = new QLineEdit;
        search_input_->setPlaceholderText("🔍  Search tickets…");
        search_input_->setFixedHeight(30);
        search_input_->setStyleSheet(SS_INPUT());
        hl->addWidget(search_input_);

        // Filter
        filter_combo_ = new QComboBox;
        filter_combo_->addItems({"All Tickets", "Open", "In Progress", "Pending", "Resolved", "Closed"});
        filter_combo_->setFixedHeight(28);
        filter_combo_->setStyleSheet(SS_INPUT());
        hl->addWidget(filter_combo_);

        vl->addWidget(hdr);
    }

    // ── Stats strip ───────────────────────────────────────────────────────────
    {
        auto* stats = new QWidget(this);
        stats->setFixedHeight(32);
        stats->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
        auto* sl = new QHBoxLayout(stats);
        sl->setContentsMargins(14, 0, 14, 0);
        sl->setSpacing(0);

        auto add_stat = [&](const QString& label, QLabel*& ref, const QString& color) {
            auto* w = new QWidget(this);
            w->setStyleSheet("background:transparent;");
            auto* wl = new QHBoxLayout(w);
            wl->setContentsMargins(0, 0, 0, 0);
            wl->setSpacing(4);
            wl->addWidget(lbl(label, ui::colors::TEXT_TERTIARY(), 10));
            ref = lbl("0", color, 10, true);
            wl->addWidget(ref);
            return w;
        };

        sl->addWidget(add_stat("Total", stat_total_, ui::colors::TEXT_SECONDARY()));
        sl->addStretch();
        sl->addWidget(add_stat("Open", stat_open_, ui::colors::CYAN()));
        sl->addStretch();
        sl->addWidget(add_stat("Done", stat_resolved_, ui::colors::POSITIVE()));

        vl->addWidget(stats);
    }

    // ── Ticket list ───────────────────────────────────────────────────────────
    list_scroll_ = new QScrollArea;
    list_scroll_->setWidgetResizable(true);
    list_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_scroll_->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                                "QScrollBar:vertical { background:transparent; width:4px; }"
                                "QScrollBar::handle:vertical { background:#444; border-radius:2px; }"
                                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    ticket_container_ = new QWidget(this);
    ticket_container_->setStyleSheet("background:transparent;");
    auto* tcl = new QVBoxLayout(ticket_container_);
    tcl->setContentsMargins(0, 4, 0, 4);
    tcl->setSpacing(0);
    tcl->addStretch();

    list_scroll_->setWidget(ticket_container_);
    vl->addWidget(list_scroll_, 1);

    return sidebar;
}

// ── Empty state ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_empty_state() {
    auto* w = new QWidget(this);
    w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignCenter);
    vl->setSpacing(12);

    auto* icon = lbl("🎟", ui::colors::TEXT_TERTIARY(), 40);
    icon->setAlignment(Qt::AlignCenter);
    vl->addWidget(icon);

    auto* t = lbl("Select a ticket to view details", ui::colors::TEXT_SECONDARY(), 13, true);
    t->setAlignment(Qt::AlignCenter);
    vl->addWidget(t);

    auto* s = lbl("or create a new support request", ui::colors::TEXT_TERTIARY(), 11, false, true);
    s->setAlignment(Qt::AlignCenter);
    vl->addWidget(s);

    vl->addSpacing(16);

    auto* btn = new QPushButton("＋  Create New Ticket");
    btn->setFixedSize(200, 36);
    btn->setStyleSheet(SS_BTN_PRIMARY());
    connect(btn, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(1); });
    vl->addWidget(btn, 0, Qt::AlignCenter);

    return w;
}

// ── Create page ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_create_page() {
    auto* outer = new QScrollArea;
    outer->setWidgetResizable(true);
    outer->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                         "QScrollBar:vertical { background:transparent; width:4px; }"
                         "QScrollBar::handle:vertical { background:#444; border-radius:2px; }"
                         "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(32, 28, 32, 28);
    vl->setSpacing(0);

    // Page header
    {
        auto* cancel_btn = new QPushButton("← Back");
        cancel_btn->setFlat(true);
        cancel_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;"
                                          "font-size:11px;padding:0;%2}"
                                          "QPushButton:hover{color:%3;}")
                                      .arg(ui::colors::TEXT_TERTIARY(), MF, ui::colors::AMBER()));
        connect(cancel_btn, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(0); });
        vl->addWidget(cancel_btn, 0, Qt::AlignLeft);
        vl->addSpacing(16);

        vl->addWidget(lbl("Create Support Ticket", ui::colors::TEXT_PRIMARY(), 18, true));
        vl->addSpacing(4);
        vl->addWidget(lbl("Describe your issue in detail. We typically respond within 24 hours.",
                          ui::colors::TEXT_TERTIARY(), 12, false, true));
        vl->addSpacing(24);
    }

    // ── Form card ─────────────────────────────────────────────────────────────
    auto* card = new QWidget(this);
    card->setStyleSheet(
        QString("background:%1;border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(24, 20, 24, 24);
    cl->setSpacing(16);

    // Field helper
    auto add_field = [&](const QString& label_text, QWidget* input, bool required = false) {
        auto* fw = new QWidget(this);
        fw->setStyleSheet("background:transparent;");
        auto* fvl = new QVBoxLayout(fw);
        fvl->setContentsMargins(0, 0, 0, 0);
        fvl->setSpacing(6);

        auto* row = new QHBoxLayout;
        row->addWidget(lbl(label_text, ui::colors::TEXT_SECONDARY(), 11, true));
        if (required) {
            auto* req = lbl("*", ui::colors::AMBER(), 11, true);
            row->addWidget(req);
        }
        row->addStretch();
        fvl->addLayout(row);
        fvl->addWidget(input);
        cl->addWidget(fw);
    };

    subject_input_ = new QLineEdit;
    subject_input_->setPlaceholderText("Brief summary of your issue");
    subject_input_->setFixedHeight(38);
    subject_input_->setStyleSheet(SS_INPUT());
    add_field("Subject", subject_input_, true);

    // Category + Priority side by side
    {
        auto* row_w = new QWidget(this);
        row_w->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row_w);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(16);

        auto make_combo_field = [&](const QString& label_text, QComboBox*& ref, const QStringList& items, int def = 0) {
            auto* fw = new QWidget(this);
            fw->setStyleSheet("background:transparent;");
            auto* fvl = new QVBoxLayout(fw);
            fvl->setContentsMargins(0, 0, 0, 0);
            fvl->setSpacing(6);
            fvl->addWidget(lbl(label_text, ui::colors::TEXT_SECONDARY(), 11, true));
            ref = new QComboBox;
            ref->addItems(items);
            ref->setCurrentIndex(def);
            ref->setFixedHeight(38);
            ref->setStyleSheet(SS_INPUT());
            fvl->addWidget(ref);
            rl->addLayout(fvl);
        };

        make_combo_field("Category", category_combo_,
                         {"Technical", "Billing", "Feature Request", "Bug Report", "Account", "Other"});
        make_combo_field("Priority", priority_combo_, {"Low", "Medium", "High"}, 1);

        cl->addWidget(row_w);
    }

    // Description with char counter
    {
        auto* hdr = new QHBoxLayout;
        hdr->addWidget(lbl("Description", ui::colors::TEXT_SECONDARY(), 11, true));
        auto* req = lbl("*", ui::colors::AMBER(), 11, true);
        hdr->addWidget(req);
        hdr->addStretch();
        char_count_lbl_ = lbl("0 / 2000", ui::colors::TEXT_TERTIARY(), 10);
        hdr->addWidget(char_count_lbl_);
        cl->addLayout(hdr);

        desc_input_ = new QTextEdit;
        desc_input_->setPlaceholderText("Please describe:\n"
                                        "• What were you doing?\n"
                                        "• What did you expect to happen?\n"
                                        "• What actually happened?\n"
                                        "• Steps to reproduce (if applicable)");
        desc_input_->setMinimumHeight(160);
        desc_input_->setMaximumHeight(280);
        desc_input_->setStyleSheet(SS_INPUT());
        cl->addWidget(desc_input_);

        connect(desc_input_, &QTextEdit::textChanged, this, [this]() {
            int n = desc_input_->toPlainText().length();
            char_count_lbl_->setText(QString("%1 / 2000").arg(n));
            char_count_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;background:transparent;%2")
                    .arg(n > 2000 ? ui::colors::NEGATIVE() : ui::colors::TEXT_TERTIARY(), MF));
        });
    }

    vl->addWidget(card);
    vl->addSpacing(16);

    // Action row
    {
        auto* ar = new QHBoxLayout;

        auto* cancel2 = new QPushButton("Cancel");
        cancel2->setFixedHeight(38);
        cancel2->setStyleSheet(SS_BTN_GHOST());
        connect(cancel2, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(0); });
        ar->addWidget(cancel2);
        ar->addStretch();

        create_btn_ = new QPushButton("Submit Ticket →");
        create_btn_->setFixedHeight(38);
        create_btn_->setMinimumWidth(160);
        create_btn_->setStyleSheet(SS_BTN_SUCCESS());
        connect(create_btn_, &QPushButton::clicked, this, &SupportScreen::on_create_ticket);
        ar->addWidget(create_btn_);

        vl->addLayout(ar);
    }

    // Tips panel below
    {
        vl->addSpacing(20);
        auto* tips = new QWidget(this);
        tips->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid %3;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
        auto* tvl = new QVBoxLayout(tips);
        tvl->setContentsMargins(16, 12, 16, 14);
        tvl->setSpacing(8);

        tvl->addWidget(lbl("Tips for a faster response", ui::colors::AMBER(), 11, true));
        tvl->addWidget(hsep());

        const char* tip_items[] = {
            "✓  One issue per ticket — easier to track and resolve",
            "✓  Include your OS, version, and any error messages",
            "✓  Describe steps to reproduce if it's a bug",
            "✓  Billing questions resolved within 4 hours",
        };
        for (const auto* t : tip_items) {
            auto* tl = lbl(t, ui::colors::TEXT_SECONDARY(), 11, false, true);
            tvl->addWidget(tl);
        }
        vl->addWidget(tips);
    }

    vl->addStretch();
    outer->setWidget(page);
    return outer;
}

// ── Detail page ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_detail_page() {
    auto* outer = new QWidget(this);
    outer->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* root_vl = new QVBoxLayout(outer);
    root_vl->setContentsMargins(0, 0, 0, 0);
    root_vl->setSpacing(0);

    // ── Detail header bar ─────────────────────────────────────────────────────
    {
        auto* hdr = new QWidget(this);
        hdr->setFixedHeight(50);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* hl = new QHBoxLayout(hdr);
        hl->setContentsMargins(20, 0, 20, 0);
        hl->setSpacing(10);

        detail_id_lbl_ = lbl("", ui::colors::TEXT_TERTIARY(), 11);
        hl->addWidget(detail_id_lbl_);

        detail_subject_lbl_ = lbl("", ui::colors::TEXT_PRIMARY(), 14, true);
        detail_subject_lbl_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        detail_subject_lbl_->setWordWrap(false);
        hl->addWidget(detail_subject_lbl_, 1);

        detail_status_lbl_ = new QLabel;
        detail_status_lbl_->setFixedHeight(22);
        detail_status_lbl_->setAlignment(Qt::AlignCenter);
        detail_status_lbl_->setStyleSheet(QString("background:%1;color:%2;font-size:10px;font-weight:700;"
                                                  "padding:0 10px;letter-spacing:0.5px;%3")
                                              .arg(ui::colors::CYAN(), ui::colors::BG_BASE(), MF));
        hl->addWidget(detail_status_lbl_);

        close_btn_ = new QPushButton("Close Ticket");
        close_btn_->setFixedHeight(28);
        close_btn_->setStyleSheet(SS_BTN_DANGER());
        close_btn_->hide();
        connect(close_btn_, &QPushButton::clicked, this, &SupportScreen::on_close_ticket);
        hl->addWidget(close_btn_);

        reopen_btn_ = new QPushButton("Reopen");
        reopen_btn_->setFixedHeight(28);
        reopen_btn_->setStyleSheet(SS_BTN_OUTLINE());
        reopen_btn_->hide();
        connect(reopen_btn_, &QPushButton::clicked, this, &SupportScreen::on_reopen_ticket);
        hl->addWidget(reopen_btn_);

        root_vl->addWidget(hdr);
    }

    // ── Ticket info strip ─────────────────────────────────────────────────────
    {
        auto* info_strip = new QWidget(this);
        info_strip->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                      .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* il = new QVBoxLayout(info_strip);
        il->setContentsMargins(20, 10, 20, 10);
        il->setSpacing(6);

        detail_meta_lbl_ = lbl("", ui::colors::TEXT_TERTIARY(), 10);
        il->addWidget(detail_meta_lbl_);

        detail_body_lbl_ = new QLabel;
        detail_body_lbl_->setWordWrap(true);
        detail_body_lbl_->setTextFormat(Qt::PlainText);
        detail_body_lbl_->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;%2").arg(ui::colors::TEXT_SECONDARY(), MF));
        il->addWidget(detail_body_lbl_);

        root_vl->addWidget(info_strip);
    }

    // ── Conversation area (scrollable) ────────────────────────────────────────
    detail_scroll_ = new QScrollArea;
    detail_scroll_->setWidgetResizable(true);
    detail_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detail_scroll_->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                                  "QScrollBar:vertical { background:transparent; width:4px; }"
                                  "QScrollBar::handle:vertical { background:#444; border-radius:2px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    messages_container_ = new QWidget(this);
    messages_container_->setStyleSheet("background:transparent;");
    auto* mcl = new QVBoxLayout(messages_container_);
    mcl->setContentsMargins(20, 16, 20, 16);
    mcl->setSpacing(12);
    mcl->addStretch();

    detail_scroll_->setWidget(messages_container_);
    root_vl->addWidget(detail_scroll_, 1);

    // ── Bottom area: reply / demo / closed notices ────────────────────────────
    {
        // Demo notice
        demo_box_ = new QWidget(this);
        demo_box_->setStyleSheet(
            QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* dbl = new QHBoxLayout(demo_box_);
        dbl->setContentsMargins(20, 12, 20, 12);
        auto* di = lbl("ℹ", ui::colors::INFO(), 14);
        dbl->addWidget(di);
        auto* dt = lbl("This is a demo ticket. Open a real ticket to get support from our team.",
                       ui::colors::TEXT_SECONDARY(), 11, false, true);
        dbl->addWidget(dt, 1);
        auto* db = new QPushButton("New Ticket");
        db->setFixedHeight(28);
        db->setStyleSheet(SS_BTN_PRIMARY());
        connect(db, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(1); });
        dbl->addWidget(db);
        demo_box_->hide();
        root_vl->addWidget(demo_box_);

        // Closed notice
        closed_box_ = new QWidget(this);
        closed_box_->setStyleSheet(
            QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cbl = new QHBoxLayout(closed_box_);
        cbl->setContentsMargins(20, 12, 20, 12);
        cbl->addWidget(lbl("✓  This ticket is closed.", ui::colors::POSITIVE(), 11));
        cbl->addStretch();
        closed_box_->hide();
        root_vl->addWidget(closed_box_);

        // Reply box
        reply_box_ = new QWidget(this);
        reply_box_->setStyleSheet(
            QString("background:%1;border-top:2px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::POSITIVE()));
        auto* rbl = new QVBoxLayout(reply_box_);
        rbl->setContentsMargins(20, 12, 20, 14);
        rbl->setSpacing(10);

        auto* reply_hdr = new QHBoxLayout;
        reply_hdr->addWidget(lbl("Reply", ui::colors::TEXT_PRIMARY(), 12, true));
        reply_hdr->addStretch();
        reply_hdr->addWidget(lbl("Ctrl+Enter to send", ui::colors::TEXT_TERTIARY(), 10));
        rbl->addLayout(reply_hdr);

        msg_input_ = new QTextEdit;
        msg_input_->setPlaceholderText("Type your reply…");
        msg_input_->setFixedHeight(80);
        msg_input_->setStyleSheet(SS_INPUT());
        rbl->addWidget(msg_input_);

        auto* send_row = new QHBoxLayout;
        send_row->addStretch();
        send_btn_ = new QPushButton("Send Reply →");
        send_btn_->setFixedHeight(34);
        send_btn_->setMinimumWidth(140);
        send_btn_->setStyleSheet(SS_BTN_SUCCESS());
        connect(send_btn_, &QPushButton::clicked, this, &SupportScreen::on_send_message);
        send_row->addWidget(send_btn_);
        rbl->addLayout(send_row);

        root_vl->addWidget(reply_box_);
    }

    return outer;
}

// ── select_ticket_row ─────────────────────────────────────────────────────────

void SupportScreen::select_ticket_row(QPushButton* btn) {
    if (active_row_btn_ && active_row_btn_ != btn) {
        active_row_btn_->setStyleSheet(QString("QPushButton { background:transparent; border:none;"
                                               " border-bottom:1px solid %1; padding:0; text-align:left; }"
                                               "QPushButton:hover { background:%2; }")
                                           .arg(ui::colors::BORDER_DIM(), ui::colors::BG_RAISED()));
    }
    active_row_btn_ = btn;
    btn->setStyleSheet(QString("QPushButton { background:%1; border:none;"
                               " border-bottom:1px solid %2; border-left:3px solid %3;"
                               " padding:0; text-align:left; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
}

// ── set_busy ──────────────────────────────────────────────────────────────────

void SupportScreen::set_busy(bool busy) {
    QString col = busy ? ui::colors::AMBER() : ui::colors::POSITIVE();
    status_dot_->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;").arg(col));
    status_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;%2").arg(col, MF));
    status_lbl_->setText(busy ? "Updating…" : "Ready");
    refresh_btn_->setEnabled(!busy);
    if (create_btn_)
        create_btn_->setEnabled(!busy);
    if (send_btn_)
        send_btn_->setEnabled(!busy);
}

// ── load_tickets ──────────────────────────────────────────────────────────────

} // namespace fincept::screens
