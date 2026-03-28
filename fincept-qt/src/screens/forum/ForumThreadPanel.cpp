// src/screens/forum/ForumThreadPanel.cpp
#include "screens/forum/ForumThreadPanel.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

namespace fincept::screens {

static QString M(int sz = 12) {
    return QString("font-family:'Consolas','Courier New',monospace;font-size:%1px;").arg(sz);
}

static QString rel_time(const QString& iso) {
    if (iso.isEmpty())
        return {};
    auto dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso.left(19), "yyyy-MM-ddTHH:mm:ss");
    if (!dt.isValid())
        return iso.left(10);
    qint64 sec = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (sec < 0)
        sec = 0;
    if (sec < 60)
        return QString("%1s ago").arg(sec);
    if (sec < 3600)
        return QString("%1m ago").arg(sec / 60);
    if (sec < 86400)
        return QString("%1h ago").arg(sec / 3600);
    return QString("%1d ago").arg(sec / 86400);
}

static QString det_color(const QString& s) {
    static const char* pal[] = {"#d97706", "#06b6d4", "#10b981", "#8b5cf6", "#f97316",
                                "#3b82f6", "#ec4899", "#14b8a6", "#84cc16", "#ef4444"};
    return pal[qHash(s) % 10];
}

ForumThreadPanel::ForumThreadPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ForumThreadPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget;

    // ── Page 0: Loading ───────────────────────────────────────────────────────
    auto* load_page = new QWidget;
    load_page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    {
        auto* vl = new QVBoxLayout(load_page);
        vl->setAlignment(Qt::AlignCenter);
        vl->setSpacing(12);

        spin_lbl_ = new QLabel("⣾");
        spin_lbl_->setAlignment(Qt::AlignCenter);
        spin_lbl_->setStyleSheet(
            QString("color:%1;font-size:28px;background:transparent;%2")
                .arg(ui::colors::AMBER, M(28)));

        auto* loading_text = new QLabel("Loading thread...");
        loading_text->setAlignment(Qt::AlignCenter);
        loading_text->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY, M(11)));

        vl->addStretch();
        vl->addWidget(spin_lbl_);
        vl->addWidget(loading_text);
        vl->addStretch();
    }
    stack_->addWidget(load_page); // 0

    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(90);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        static const QString f[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
        spin_lbl_->setText(f[spin_frame_++ % 8]);
    });

    // ── Page 1: Thread view ───────────────────────────────────────────────────
    thread_page_ = new QWidget;
    thread_page_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* tp = new QVBoxLayout(thread_page_);
    tp->setContentsMargins(0, 0, 0, 0);
    tp->setSpacing(0);

    // ── Back bar with gradient accent ─────────────────────────────────────────
    auto* back_bar = new QWidget;
    back_bar->setFixedHeight(38);
    back_bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* bb_hl = new QHBoxLayout(back_bar);
    bb_hl->setContentsMargins(16, 0, 16, 0);
    bb_hl->setSpacing(10);

    auto* back_btn = new QPushButton("←  Back to Feed");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedHeight(28);
    back_btn->setStyleSheet(
        QString("QPushButton{background:rgba(255,255,255,0.03);"
                "color:%1;border:1px solid %2;"
                "font-size:11px;font-weight:600;padding:0 14px;"
                "border-radius:4px;%3}"
                "QPushButton:hover{color:%4;border-color:%5;"
                "background:rgba(255,255,255,0.06);}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM,
                 M(11), ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    connect(back_btn, &QPushButton::clicked, this,
            [this]() { emit back_requested(); });

    bb_hl->addWidget(back_btn);
    bb_hl->addStretch();
    tp->addWidget(back_bar);

    // ── Scrollable content ────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:%1;}"
                "QScrollBar:vertical{width:6px;background:transparent;margin:0;}"
                "QScrollBar::handle:vertical{background:%2;min-height:40px;"
                "border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical"
                "{height:0;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));

    auto* sw = new QWidget;
    sw->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* svl = new QVBoxLayout(sw);
    svl->setContentsMargins(0, 0, 0, 0);
    svl->setSpacing(0);

    // ── Post hero header — centered, max 860px ────────────────────────────────
    auto* hdr_outer = new QWidget;
    hdr_outer->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #0e0e0e,stop:1 %1);")
            .arg(ui::colors::BG_BASE));
    auto* hdr_outer_hl = new QHBoxLayout(hdr_outer);
    hdr_outer_hl->setContentsMargins(0, 0, 0, 0);

    auto* hdr_w = new QWidget;
    hdr_w->setMaximumWidth(860);
    hdr_w->setStyleSheet("background:transparent;");
    auto* hdr_vl = new QVBoxLayout(hdr_w);
    hdr_vl->setContentsMargins(32, 24, 32, 20);
    hdr_vl->setSpacing(12);

    // Category chip + time row
    auto* chip_row = new QWidget;
    chip_row->setStyleSheet("background:transparent;");
    auto* chip_hl = new QHBoxLayout(chip_row);
    chip_hl->setContentsMargins(0, 0, 0, 0);
    chip_hl->setSpacing(12);

    t_cat_chip_ = new QLabel;
    t_cat_chip_->setFixedHeight(20);
    t_cat_chip_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;"
                "background:transparent;border:1px solid %1;"
                "padding:2px 10px;border-radius:10px;%2")
            .arg(ui::colors::AMBER, M(9)));

    t_meta_lbl_ = new QLabel;
    t_meta_lbl_->setStyleSheet(
        QString("color:%1;font-size:10px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(10)));

    chip_hl->addWidget(t_cat_chip_);
    chip_hl->addStretch();
    chip_hl->addWidget(t_meta_lbl_);
    hdr_vl->addWidget(chip_row);

    // Title — hero size
    t_title_lbl_ = new QLabel;
    t_title_lbl_->setWordWrap(true);
    t_title_lbl_->setStyleSheet(
        QString("color:%1;font-size:22px;font-weight:700;line-height:1.3;"
                "background:transparent;%2")
            .arg(ui::colors::TEXT_PRIMARY, M(22)));
    hdr_vl->addWidget(t_title_lbl_);

    // Author row with circular avatar
    t_author_lbl_ = new QLabel;
    t_author_lbl_->setTextFormat(Qt::RichText);
    t_author_lbl_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    t_author_lbl_->setOpenExternalLinks(false);
    t_author_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(12)));
    connect(t_author_lbl_, &QLabel::linkActivated, this,
            [this](const QString& link) {
                if (link.startsWith("author:"))
                    emit author_clicked(link.mid(7));
            });
    hdr_vl->addWidget(t_author_lbl_);

    // Gradient separator
    auto* hdr_sep = new QWidget;
    hdr_sep->setFixedHeight(2);
    hdr_sep->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 %1,stop:0.4 %2,stop:1 transparent);")
            .arg(ui::colors::AMBER, ui::colors::BORDER_DIM));
    hdr_vl->addWidget(hdr_sep);

    hdr_outer_hl->addStretch();
    hdr_outer_hl->addWidget(hdr_w, 1);
    hdr_outer_hl->addStretch();
    svl->addWidget(hdr_outer);

    // ── Post body — centered, max 860px ───────────────────────────────────────
    auto* body_outer = new QWidget;
    body_outer->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* body_outer_hl = new QHBoxLayout(body_outer);
    body_outer_hl->setContentsMargins(0, 0, 0, 0);

    t_body_lbl_ = new QLabel;
    t_body_lbl_->setWordWrap(true);
    t_body_lbl_->setTextFormat(Qt::PlainText);
    t_body_lbl_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    t_body_lbl_->setMaximumWidth(860);
    t_body_lbl_->setStyleSheet(
        QString("color:%1;font-size:13px;line-height:1.8;background:transparent;"
                "padding:20px 32px;%2")
            .arg(ui::colors::TEXT_SECONDARY, M(13)));

    body_outer_hl->addStretch();
    body_outer_hl->addWidget(t_body_lbl_, 1);
    body_outer_hl->addStretch();
    svl->addWidget(body_outer);

    // ── Engagement bar — centered, modern pill buttons ────────────────────────
    auto* eng_outer = new QWidget;
    eng_outer->setFixedHeight(48);
    eng_outer->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;"
                "border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* eng_outer_hl = new QHBoxLayout(eng_outer);
    eng_outer_hl->setContentsMargins(0, 0, 0, 0);

    auto* eng_w = new QWidget;
    eng_w->setMaximumWidth(860);
    eng_w->setStyleSheet("background:transparent;");
    auto* eng_hl = new QHBoxLayout(eng_w);
    eng_hl->setContentsMargins(32, 0, 32, 0);
    eng_hl->setSpacing(12);

    auto* up_btn = new QPushButton("▲  Upvote");
    up_btn->setFixedHeight(30);
    up_btn->setCursor(Qt::PointingHandCursor);
    up_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.06);"
                "color:%1;border:1px solid rgba(217,119,6,0.2);"
                "font-size:11px;font-weight:700;padding:0 16px;"
                "border-radius:15px;%2}"
                "QPushButton:hover{color:%3;"
                "border-color:rgba(217,119,6,0.5);"
                "background:rgba(217,119,6,0.12);}"
                "QPushButton:pressed{color:%3;"
                "border-color:%3;"
                "background:rgba(217,119,6,0.25);}")
            .arg(ui::colors::TEXT_SECONDARY, M(11), ui::colors::AMBER));
    connect(up_btn, &QPushButton::clicked, this, [this]() {
        LOG_INFO("ForumThread", "Upvote clicked, uuid=" + current_.post.post_uuid);
        if (!current_.post.post_uuid.isEmpty())
            emit vote_post(current_.post.post_uuid, "up");
    });

    t_likes_lbl_ = new QLabel("▲ 0");
    t_likes_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:600;"
                "background:transparent;padding:0 8px;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(12)));

    t_replies_lbl_ = new QLabel("◆ 0 replies");
    t_replies_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;background:transparent;"
                "padding:0 8px;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(12)));

    t_views_lbl_ = new QLabel("◉ 0 views");
    t_views_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;background:transparent;%2")
            .arg(ui::colors::TEXT_DIM, M(12)));

    eng_hl->addWidget(up_btn);
    eng_hl->addWidget(t_likes_lbl_);
    eng_hl->addWidget(t_replies_lbl_);
    eng_hl->addStretch();
    eng_hl->addWidget(t_views_lbl_);

    eng_outer_hl->addStretch();
    eng_outer_hl->addWidget(eng_w, 1);
    eng_outer_hl->addStretch();
    svl->addWidget(eng_outer);

    // ── Comments section header ───────────────────────────────────────────────
    auto* com_hdr_outer = new QWidget;
    com_hdr_outer->setFixedHeight(32);
    com_hdr_outer->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* com_hdr_hl = new QHBoxLayout(com_hdr_outer);
    com_hdr_hl->setContentsMargins(0, 0, 0, 0);

    auto* com_hdr_inner = new QWidget;
    com_hdr_inner->setMaximumWidth(860);
    com_hdr_inner->setStyleSheet("background:transparent;");
    auto* chi = new QHBoxLayout(com_hdr_inner);
    chi->setContentsMargins(32, 0, 32, 0);
    chi->setSpacing(8);

    auto* com_dot = new QLabel("◆");
    com_dot->setStyleSheet(
        QString("color:%1;font-size:8px;background:transparent;")
            .arg(ui::colors::CYAN));
    auto* ch_lbl = new QLabel("REPLIES");
    ch_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1.5px;"
                "background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(10)));
    chi->addWidget(com_dot);
    chi->addWidget(ch_lbl);
    chi->addStretch();

    com_hdr_hl->addStretch();
    com_hdr_hl->addWidget(com_hdr_inner, 1);
    com_hdr_hl->addStretch();
    svl->addWidget(com_hdr_outer);

    // ── Comments container — centered ─────────────────────────────────────────
    auto* com_outer = new QWidget;
    com_outer->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* com_outer_hl = new QHBoxLayout(com_outer);
    com_outer_hl->setContentsMargins(0, 0, 0, 0);

    t_comments_w_ = new QWidget;
    t_comments_w_->setMaximumWidth(860);
    t_comments_w_->setStyleSheet(
        QString("background:%1;").arg(ui::colors::BG_BASE));
    t_comments_vl_ = new QVBoxLayout(t_comments_w_);
    t_comments_vl_->setContentsMargins(0, 0, 0, 0);
    t_comments_vl_->setSpacing(0);

    com_outer_hl->addStretch();
    com_outer_hl->addWidget(t_comments_w_, 1);
    com_outer_hl->addStretch();
    svl->addWidget(com_outer);
    svl->addStretch();

    scroll->setWidget(sw);
    tp->addWidget(scroll, 1);

    // ── Compose bar — modern with gradient accent ─────────────────────────────
    auto* compose = new QWidget;
    compose->setFixedHeight(54);
    compose->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* comp_vl = new QVBoxLayout(compose);
    comp_vl->setContentsMargins(0, 0, 0, 0);
    comp_vl->setSpacing(0);

    // Thin gradient accent
    auto* comp_accent = new QWidget;
    comp_accent->setFixedHeight(1);
    comp_accent->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 %1,stop:0.3 %2,stop:1 transparent);")
            .arg(ui::colors::AMBER, ui::colors::CYAN));
    comp_vl->addWidget(comp_accent);

    auto* comp_outer_hl = new QHBoxLayout;
    comp_outer_hl->setContentsMargins(0, 0, 0, 0);

    auto* comp_inner = new QWidget;
    comp_inner->setMaximumWidth(860);
    comp_inner->setStyleSheet("background:transparent;");
    auto* comp_hl = new QHBoxLayout(comp_inner);
    comp_hl->setContentsMargins(32, 8, 32, 8);
    comp_hl->setSpacing(10);

    t_reply_input_ = new QLineEdit;
    t_reply_input_->setPlaceholderText("Write a reply...");
    t_reply_input_->setFixedHeight(34);
    t_reply_input_->setStyleSheet(
        QString("QLineEdit{background:rgba(255,255,255,0.03);color:%1;"
                "border:1px solid %2;padding:6px 14px;font-size:12px;"
                "border-radius:4px;%3}"
                "QLineEdit:focus{border-color:%4;color:%5;"
                "background:rgba(255,255,255,0.05);}"
                "QLineEdit::placeholder{color:%6;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM,
                 M(12), ui::colors::BORDER_BRIGHT,
                 ui::colors::TEXT_PRIMARY, ui::colors::TEXT_DIM));

    auto* send_btn = new QPushButton("Reply");
    send_btn->setFixedSize(80, 34);
    send_btn->setCursor(Qt::PointingHandCursor);
    send_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;"
                "border:1px solid rgba(217,119,6,0.25);font-size:12px;"
                "font-weight:700;border-radius:4px;%2}"
                "QPushButton:hover{background:rgba(217,119,6,0.2);color:%3;"
                "border-color:rgba(217,119,6,0.5);}")
            .arg(ui::colors::TEXT_SECONDARY, M(12), ui::colors::AMBER));

    auto submit = [this]() {
        QString txt = t_reply_input_->text().trimmed();
        if (txt.isEmpty() || current_.post.post_uuid.isEmpty())
            return;
        emit comment_submitted(current_.post.post_uuid, txt);
        t_reply_input_->clear();
    };
    connect(send_btn, &QPushButton::clicked, this, submit);
    connect(t_reply_input_, &QLineEdit::returnPressed, this, submit);

    comp_hl->addWidget(t_reply_input_, 1);
    comp_hl->addWidget(send_btn);

    comp_outer_hl->addStretch();
    comp_outer_hl->addWidget(comp_inner, 1);
    comp_outer_hl->addStretch();
    comp_vl->addLayout(comp_outer_hl, 1);
    tp->addWidget(compose);

    stack_->addWidget(thread_page_); // 1
    root->addWidget(stack_, 1);
    stack_->setCurrentIndex(0);
}

void ForumThreadPanel::set_loading(bool on) {
    if (on) {
        spin_frame_ = 0;
        spin_timer_->start();
        stack_->setCurrentIndex(0);
    } else {
        spin_timer_->stop();
    }
}

void ForumThreadPanel::clear() {
    spin_timer_->stop();
    current_ = {};
}

void ForumThreadPanel::show_post(const services::ForumPostDetail& detail) {
    spin_timer_->stop();
    current_ = detail;

    QString cc = detail.post.category_color.isEmpty()
                     ? det_color(detail.post.category_name)
                     : detail.post.category_color;

    t_cat_chip_->setText(detail.post.category_name.toUpper());
    t_cat_chip_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;"
                "background:transparent;border:1px solid %1;"
                "padding:2px 10px;border-radius:10px;%2")
            .arg(cc, M(9)));

    t_meta_lbl_->setText(rel_time(detail.post.created_at));
    t_title_lbl_->setText(detail.post.title);

    // Author with circular avatar
    QString avc = det_color(detail.post.author_display_name);
    QString ini = detail.post.author_display_name.isEmpty()
                      ? "?"
                      : detail.post.author_display_name.left(2).toUpper();
    t_author_lbl_->setText(
        QString("<span style='background:%1;color:%2;font-size:10px;"
                "font-family:Consolas;padding:3px 6px;font-weight:700;"
                "border-radius:12px;'>%3</span>"
                "&nbsp;&nbsp;"
                "<a href='author:%4' style='color:%5;text-decoration:none;"
                "font-family:Consolas;font-size:12px;font-weight:600;'>%6</a>")
            .arg(avc, ui::colors::BG_BASE, ini, detail.post.author_name,
                 ui::colors::TEXT_SECONDARY, detail.post.author_display_name));

    t_body_lbl_->setText(detail.post.content);

    // Engagement
    bool voted = (detail.post.user_vote == "up");
    QString lc = voted || detail.post.likes > 0 ? ui::colors::AMBER
                                                 : ui::colors::TEXT_DIM;
    t_likes_lbl_->setText(QString("▲ %1").arg(detail.post.likes));
    t_likes_lbl_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:600;"
                "background:transparent;padding:0 8px;%2")
            .arg(lc, M(12)));

    t_replies_lbl_->setText(
        QString("◆ %1 %2")
            .arg(detail.total_comments)
            .arg(detail.total_comments == 1 ? "reply" : "replies"));
    t_views_lbl_->setText(QString("◉ %1 views").arg(detail.post.views));

    rebuild_comments();
    stack_->setCurrentIndex(1);
}

void ForumThreadPanel::rebuild_comments() {
    while (t_comments_vl_->count() > 0) {
        auto* item = t_comments_vl_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (current_.comments.isEmpty()) {
        auto* noc = new QWidget;
        noc->setMinimumHeight(80);
        noc->setStyleSheet("background:transparent;");
        auto* nvl = new QVBoxLayout(noc);
        nvl->setAlignment(Qt::AlignCenter);
        nvl->setSpacing(6);

        auto* icon = new QLabel("◇");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(
            QString("color:%1;font-size:24px;background:transparent;")
                .arg(ui::colors::BORDER_DIM));

        auto* nl = new QLabel("NO REPLIES YET");
        nl->setAlignment(Qt::AlignCenter);
        nl->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1.5px;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY, M(11)));

        auto* sub = new QLabel("Be the first to share your thoughts");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM, M(10)));

        nvl->addWidget(icon);
        nvl->addWidget(nl);
        nvl->addWidget(sub);
        t_comments_vl_->addWidget(noc);
        return;
    }

    for (int i = 0; i < current_.comments.size(); ++i) {
        const auto& c = current_.comments[i];
        QString avc = det_color(c.author_display_name);

        auto* card = new QWidget;
        card->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
                .arg(i % 2 == 0 ? ui::colors::BG_BASE : ui::colors::BG_SURFACE,
                     ui::colors::BORDER_DIM));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(32, 12, 32, 10);
        cvl->setSpacing(6);

        // ── Comment header ────────────────────────────────────────────────────
        auto* hr = new QWidget;
        hr->setStyleSheet("background:transparent;");
        auto* hrh = new QHBoxLayout(hr);
        hrh->setContentsMargins(0, 0, 0, 0);
        hrh->setSpacing(10);

        // Thread accent line
        auto* tline = new QWidget;
        tline->setFixedSize(3, 22);
        tline->setStyleSheet(
            QString("background:%1;border-radius:1px;").arg(avc));

        // Circular avatar
        auto* cav = new QLabel(c.author_display_name.left(2).toUpper());
        cav->setFixedSize(24, 24);
        cav->setAlignment(Qt::AlignCenter);
        cav->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:%2;"
                    "border-radius:12px;%3")
                .arg(ui::colors::BG_BASE, avc, M(9)));

        // Author name
        auto* cname = new QLabel(c.author_display_name);
        cname->setStyleSheet(
            QString("color:%1;font-size:12px;font-weight:600;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_PRIMARY, M(12)));

        // Time
        auto* ctime = new QLabel(rel_time(c.created_at));
        ctime->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM, M(10)));

        // Likes
        bool voted = (c.user_vote == "up");
        auto* clikes = new QLabel(QString("▲ %1").arg(c.likes));
        clikes->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:600;"
                    "background:transparent;%2")
                .arg(voted || c.likes > 0 ? ui::colors::AMBER
                                           : ui::colors::TEXT_DIM,
                     M(10)));

        hrh->addWidget(tline);
        hrh->addWidget(cav);
        hrh->addWidget(cname);
        hrh->addStretch();
        hrh->addWidget(ctime);
        hrh->addSpacing(8);
        hrh->addWidget(clikes);
        cvl->addWidget(hr);

        // Body
        auto* cbody = new QLabel(c.content);
        cbody->setWordWrap(true);
        cbody->setTextFormat(Qt::PlainText);
        cbody->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        cbody->setStyleSheet(
            QString("color:%1;font-size:12px;line-height:1.6;"
                    "background:transparent;padding-left:38px;%2")
                .arg(ui::colors::TEXT_SECONDARY, M(12)));
        cvl->addWidget(cbody);

        // Actions
        auto* ar = new QWidget;
        ar->setStyleSheet("background:transparent;");
        auto* arh = new QHBoxLayout(ar);
        arh->setContentsMargins(38, 0, 0, 0);
        arh->setSpacing(14);

        auto mk_act = [&](const QString& lbl, const QString& col,
                          const QString& vt) {
            auto* b = new QPushButton(lbl);
            b->setFixedHeight(18);
            b->setCursor(Qt::PointingHandCursor);
            bool a = (c.user_vote == vt);
            b->setStyleSheet(
                a ? QString("QPushButton{background:transparent;color:%1;"
                            "border:none;font-size:10px;font-weight:700;"
                            "padding:0;%2}")
                      .arg(col, M(10))
                : QString("QPushButton{background:transparent;color:%1;"
                          "border:none;font-size:10px;padding:0;%2}"
                          "QPushButton:hover{color:%3;}")
                      .arg(ui::colors::TEXT_DIM, M(10), col));
            auto uuid = c.comment_uuid;
            connect(b, &QPushButton::clicked, this,
                    [this, uuid, vt]() { emit vote_comment(uuid, vt); });
            return b;
        };
        arh->addWidget(mk_act("▲ upvote", ui::colors::AMBER, "up"));
        arh->addWidget(mk_act("▼ downvote", ui::colors::TEXT_TERTIARY, "down"));
        arh->addStretch();
        cvl->addWidget(ar);

        t_comments_vl_->addWidget(card);
    }
}

} // namespace fincept::screens
