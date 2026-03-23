// src/screens/forum/ForumThreadPanel.cpp
#include "screens/forum/ForumThreadPanel.h"
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
    if (iso.isEmpty()) return {};
    auto dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) dt = QDateTime::fromString(iso.left(19), "yyyy-MM-ddTHH:mm:ss");
    if (!dt.isValid()) return iso.left(10);
    qint64 sec = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (sec < 0) sec = 0;
    if (sec < 60)    return QString("%1s ago").arg(sec);
    if (sec < 3600)  return QString("%1m ago").arg(sec / 60);
    if (sec < 86400) return QString("%1h ago").arg(sec / 3600);
    return QString("%1d ago").arg(sec / 86400);
}

static QString det_color(const QString& s) {
    static const char* pal[] = {
        "#d97706","#06b6d4","#10b981","#8b5cf6","#f97316",
        "#3b82f6","#ec4899","#14b8a6","#84cc16","#ef4444"
    };
    return pal[qHash(s) % 10];
}

ForumThreadPanel::ForumThreadPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ForumThreadPanel::build_ui() {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget;

    // ── Page 0: Loading ───────────────────────────────────────────────────────
    auto* load_page = new QWidget;
    load_page->setStyleSheet("background:#080808;");
    {
        auto* vl = new QVBoxLayout(load_page);
        spin_lbl_ = new QLabel("⣾");
        spin_lbl_->setAlignment(Qt::AlignCenter);
        spin_lbl_->setStyleSheet(
            QString("color:#d97706;font-size:24px;background:transparent;%1").arg(M(24)));
        vl->addStretch();
        vl->addWidget(spin_lbl_);
        vl->addStretch();
    }
    stack_->addWidget(load_page); // 0

    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(90);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        static const QString f[] = {"⣾","⣽","⣻","⢿","⡿","⣟","⣯","⣷"};
        spin_lbl_->setText(f[spin_frame_++ % 8]);
    });

    // ── Page 1: Thread view ───────────────────────────────────────────────────
    thread_page_ = new QWidget;
    thread_page_->setStyleSheet("background:#080808;");
    auto* tp = new QVBoxLayout(thread_page_);
    tp->setContentsMargins(0, 0, 0, 0);
    tp->setSpacing(0);

    // ── Back bar ──────────────────────────────────────────────────────────────
    auto* back_bar = new QWidget;
    back_bar->setFixedHeight(32);
    back_bar->setStyleSheet("background:#060606;border-bottom:1px solid #0f0f0f;");
    auto* bb_hl = new QHBoxLayout(back_bar);
    bb_hl->setContentsMargins(16, 0, 16, 0);
    bb_hl->setSpacing(8);

    auto* back_btn = new QPushButton("←  BACK TO FEED");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedHeight(32);
    back_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:#333333;border:none;"
                "font-size:11px;font-weight:700;letter-spacing:0.5px;padding:0 4px;%1}"
                "QPushButton:hover{color:#888888;}").arg(M(11)));
    connect(back_btn, &QPushButton::clicked, this, [this]() {
        emit back_requested();
    });

    bb_hl->addWidget(back_btn);
    bb_hl->addStretch();
    tp->addWidget(back_bar);

    // ── Scrollable content ────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:#080808;}"
        "QScrollBar:vertical{width:4px;background:transparent;margin:0;}"
        "QScrollBar::handle:vertical{background:#151515;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* sw = new QWidget;
    sw->setStyleSheet("background:#080808;");
    auto* svl = new QVBoxLayout(sw);
    svl->setContentsMargins(0, 0, 0, 0);
    svl->setSpacing(0);

    // ── Post header — centered, wide ──────────────────────────────────────────
    auto* hdr_outer = new QWidget;
    hdr_outer->setStyleSheet("background:#080808;");
    auto* hdr_outer_hl = new QHBoxLayout(hdr_outer);
    hdr_outer_hl->setContentsMargins(0, 0, 0, 0);
    hdr_outer_hl->setSpacing(0);

    auto* hdr_w = new QWidget;
    hdr_w->setMaximumWidth(800);
    hdr_w->setStyleSheet("background:#080808;border-bottom:1px solid #0f0f0f;");
    auto* hdr_vl = new QVBoxLayout(hdr_w);
    hdr_vl->setContentsMargins(32, 20, 32, 16);
    hdr_vl->setSpacing(10);

    // Category chip + time row
    auto* chip_row = new QWidget;
    chip_row->setStyleSheet("background:transparent;");
    auto* chip_hl = new QHBoxLayout(chip_row);
    chip_hl->setContentsMargins(0, 0, 0, 0);
    chip_hl->setSpacing(10);

    t_cat_chip_ = new QLabel;
    t_cat_chip_->setStyleSheet(
        QString("color:#d97706;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                "background:transparent;border:1px solid rgba(217,119,6,0.4);"
                "padding:2px 8px;%1").arg(M(9)));

    t_meta_lbl_ = new QLabel;
    t_meta_lbl_->setStyleSheet(
        QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));

    chip_hl->addWidget(t_cat_chip_);
    chip_hl->addStretch();
    chip_hl->addWidget(t_meta_lbl_);
    hdr_vl->addWidget(chip_row);

    // Title — large
    t_title_lbl_ = new QLabel;
    t_title_lbl_->setWordWrap(true);
    t_title_lbl_->setStyleSheet(
        QString("color:#e5e5e5;font-size:18px;font-weight:700;line-height:1.3;"
                "background:transparent;%1").arg(M(18)));
    hdr_vl->addWidget(t_title_lbl_);

    // Author row with avatar
    t_author_lbl_ = new QLabel;
    t_author_lbl_->setTextFormat(Qt::RichText);
    t_author_lbl_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    t_author_lbl_->setOpenExternalLinks(false);
    t_author_lbl_->setStyleSheet(
        QString("color:#444444;font-size:11px;background:transparent;%1").arg(M(11)));
    connect(t_author_lbl_, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link.startsWith("author:")) emit author_clicked(link.mid(7));
    });
    hdr_vl->addWidget(t_author_lbl_);

    hdr_outer_hl->addStretch();
    hdr_outer_hl->addWidget(hdr_w, 1);
    hdr_outer_hl->addStretch();
    svl->addWidget(hdr_outer);

    // ── Post body — centered column ──────────────────────────────────────────
    auto* body_outer = new QWidget;
    body_outer->setStyleSheet("background:#080808;");
    auto* body_outer_hl = new QHBoxLayout(body_outer);
    body_outer_hl->setContentsMargins(0, 0, 0, 0);

    t_body_lbl_ = new QLabel;
    t_body_lbl_->setWordWrap(true);
    t_body_lbl_->setTextFormat(Qt::PlainText);
    t_body_lbl_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    t_body_lbl_->setMaximumWidth(800);
    t_body_lbl_->setStyleSheet(
        QString("color:#777777;font-size:13px;line-height:1.8;background:transparent;"
                "padding:24px 32px;%1").arg(M(13)));

    body_outer_hl->addStretch();
    body_outer_hl->addWidget(t_body_lbl_, 1);
    body_outer_hl->addStretch();
    svl->addWidget(body_outer);

    // ── Engagement bar — centered ─────────────────────────────────────────────
    auto* eng_outer = new QWidget;
    eng_outer->setStyleSheet("background:#060606;border-top:1px solid #0f0f0f;"
                             "border-bottom:1px solid #0f0f0f;");
    eng_outer->setFixedHeight(38);
    auto* eng_outer_hl = new QHBoxLayout(eng_outer);
    eng_outer_hl->setContentsMargins(0, 0, 0, 0);

    auto* eng_w = new QWidget;
    eng_w->setMaximumWidth(800);
    eng_w->setStyleSheet("background:transparent;");
    auto* eng_hl = new QHBoxLayout(eng_w);
    eng_hl->setContentsMargins(32, 0, 32, 0);
    eng_hl->setSpacing(0);

    auto* up_btn = new QPushButton("▲  UPVOTE");
    up_btn->setFixedSize(90, 26);
    up_btn->setCursor(Qt::PointingHandCursor);
    up_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:#666666;"
                "border:1px solid #161616;font-size:10px;font-weight:700;%1}"
                "QPushButton:hover{color:#d97706;border-color:rgba(217,119,6,0.4);"
                "background:rgba(217,119,6,0.06);}").arg(M(10)));
    connect(up_btn, &QPushButton::clicked, this, [this]() {
        if (!current_.post.post_uuid.isEmpty())
            emit vote_post(current_.post.post_uuid, "up");
    });

    t_likes_lbl_ = new QLabel("▲ 0");
    t_likes_lbl_->setStyleSheet(
        QString("color:#666666;font-size:11px;background:transparent;padding:0 16px;%1").arg(M(11)));
    t_replies_lbl_ = new QLabel("◆ 0 replies");
    t_replies_lbl_->setStyleSheet(
        QString("color:#606060;font-size:11px;background:transparent;padding:0 8px;%1").arg(M(11)));
    t_views_lbl_ = new QLabel("◉ 0 views");
    t_views_lbl_->setStyleSheet(
        QString("color:#1c1c1c;font-size:11px;background:transparent;%1").arg(M(11)));

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
    com_hdr_outer->setFixedHeight(28);
    com_hdr_outer->setStyleSheet("background:#060606;border-bottom:1px solid #0d0d0d;");
    auto* com_hdr_hl = new QHBoxLayout(com_hdr_outer);
    com_hdr_hl->setContentsMargins(0, 0, 0, 0);

    auto* com_hdr_inner = new QWidget;
    com_hdr_inner->setMaximumWidth(800);
    com_hdr_inner->setStyleSheet("background:transparent;");
    auto* chi = new QHBoxLayout(com_hdr_inner);
    chi->setContentsMargins(32, 0, 32, 0);
    auto* ch_lbl = new QLabel("REPLIES");
    ch_lbl->setStyleSheet(
        QString("color:#555555;font-size:9px;font-weight:700;letter-spacing:1.5px;"
                "background:transparent;%1").arg(M(9)));
    chi->addWidget(ch_lbl);

    com_hdr_hl->addStretch();
    com_hdr_hl->addWidget(com_hdr_inner, 1);
    com_hdr_hl->addStretch();
    svl->addWidget(com_hdr_outer);

    // ── Comments container — centered ─────────────────────────────────────────
    auto* com_outer = new QWidget;
    com_outer->setStyleSheet("background:#080808;");
    auto* com_outer_hl = new QHBoxLayout(com_outer);
    com_outer_hl->setContentsMargins(0, 0, 0, 0);

    t_comments_w_ = new QWidget;
    t_comments_w_->setMaximumWidth(800);
    t_comments_w_->setStyleSheet("background:#080808;");
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

    // ── Compose bar ───────────────────────────────────────────────────────────
    auto* compose = new QWidget;
    compose->setFixedHeight(46);
    compose->setStyleSheet("background:#060606;border-top:1px solid #111111;");
    auto* comp_outer_hl = new QHBoxLayout(compose);
    comp_outer_hl->setContentsMargins(0, 0, 0, 0);

    auto* comp_inner = new QWidget;
    comp_inner->setMaximumWidth(800);
    comp_inner->setStyleSheet("background:transparent;");
    auto* comp_hl = new QHBoxLayout(comp_inner);
    comp_hl->setContentsMargins(32, 8, 32, 8);
    comp_hl->setSpacing(8);

    t_reply_input_ = new QLineEdit;
    t_reply_input_->setPlaceholderText("write a reply...");
    t_reply_input_->setStyleSheet(
        QString("QLineEdit{background:#0a0a0a;color:#888888;border:1px solid #161616;"
                "padding:5px 12px;font-size:12px;%1}"
                "QLineEdit:focus{border-color:#666666;color:#e5e5e5;}"
                "QLineEdit::placeholder{color:#555555;}").arg(M(12)));

    auto* send_btn = new QPushButton("REPLY  ↵");
    send_btn->setFixedSize(80, 30);
    send_btn->setCursor(Qt::PointingHandCursor);
    send_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:#444444;"
                "border:1px solid #1a1a1a;font-size:10px;font-weight:700;%1}"
                "QPushButton:hover{background:rgba(217,119,6,0.2);color:#d97706;"
                "border-color:rgba(217,119,6,0.5);}").arg(M(10)));

    auto submit = [this]() {
        QString txt = t_reply_input_->text().trimmed();
        if (txt.isEmpty() || current_.post.post_uuid.isEmpty()) return;
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
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                "background:transparent;border:1px solid %1;"
                "padding:2px 8px;%2").arg(cc, M(9)));

    t_meta_lbl_->setText(rel_time(detail.post.created_at));
    t_title_lbl_->setText(detail.post.title);

    // Author
    QString avc = det_color(detail.post.author_display_name);
    QString ini = detail.post.author_display_name.isEmpty() ? "?"
                : detail.post.author_display_name.left(2).toUpper();
    t_author_lbl_->setText(
        QString("<span style='background:%1;color:#080808;font-size:10px;"
                "font-family:Consolas;padding:2px 5px;font-weight:700;'>%2</span>"
                "&nbsp;&nbsp;"
                "<a href='author:%3' style='color:#555555;text-decoration:none;"
                "font-family:Consolas;font-size:12px;'>%4</a>")
            .arg(avc, ini, detail.post.author_name,
                 detail.post.author_display_name));

    t_body_lbl_->setText(detail.post.content);

    // Engagement
    QString lc = detail.post.likes > 0 ? "#d97706" : "#2a2a2a";
    t_likes_lbl_->setText(
        QString("<span style='color:%1;font-family:Consolas;font-size:11px;'>"
                "▲ %2</span>").arg(lc).arg(detail.post.likes));
    t_likes_lbl_->setTextFormat(Qt::RichText);

    t_replies_lbl_->setText(
        QString("◆ %1 %2").arg(detail.total_comments)
            .arg(detail.total_comments == 1 ? "reply" : "replies"));
    t_views_lbl_->setText(QString("◉ %1 views").arg(detail.post.views));

    rebuild_comments();
    stack_->setCurrentIndex(1);
}

void ForumThreadPanel::rebuild_comments() {
    while (t_comments_vl_->count() > 0) {
        auto* item = t_comments_vl_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (current_.comments.isEmpty()) {
        auto* noc = new QWidget;
        noc->setFixedHeight(60);
        noc->setStyleSheet("background:transparent;");
        auto* nvl = new QVBoxLayout(noc);
        auto* icon = new QLabel("◇");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("color:#111111;font-size:18px;background:transparent;");
        auto* nl = new QLabel("NO REPLIES YET");
        nl->setAlignment(Qt::AlignCenter);
        nl->setStyleSheet(
            QString("color:#555555;font-size:10px;font-weight:700;letter-spacing:1.5px;"
                    "background:transparent;%1").arg(M(10)));
        nvl->addWidget(icon);
        nvl->addWidget(nl);
        t_comments_vl_->addWidget(noc);
        return;
    }

    for (int i = 0; i < current_.comments.size(); ++i) {
        const auto& c = current_.comments[i];

        auto* row = new QWidget;
        row->setStyleSheet(
            i % 2 == 0
                ? "background:#080808;border-bottom:1px solid #0d0d0d;"
                : "background:#090909;border-bottom:1px solid #0d0d0d;");
        auto* rvl = new QVBoxLayout(row);
        rvl->setContentsMargins(32, 10, 32, 8);
        rvl->setSpacing(5);

        // ── Comment header ────────────────────────────────────────────────────
        auto* hr = new QWidget;
        hr->setStyleSheet("background:transparent;");
        auto* hrh = new QHBoxLayout(hr);
        hrh->setContentsMargins(0, 0, 0, 0);
        hrh->setSpacing(8);

        // Thread accent
        auto* tline = new QWidget;
        tline->setFixedSize(2, 18);
        tline->setStyleSheet(
            QString("background:%1;").arg(det_color(c.author_display_name)));

        // Avatar
        auto* cav = new QLabel(c.author_display_name.left(2).toUpper());
        cav->setFixedSize(20, 20);
        cav->setAlignment(Qt::AlignCenter);
        cav->setStyleSheet(
            QString("color:#080808;font-size:9px;font-weight:700;background:%1;%2")
                .arg(det_color(c.author_display_name), M(9)));

        auto* cname = new QLabel(c.author_display_name.left(20));
        cname->setStyleSheet(
            QString("color:#555555;font-size:11px;font-weight:600;background:transparent;%1")
                .arg(M(11)));

        auto* ctime = new QLabel(rel_time(c.created_at));
        ctime->setStyleSheet(
            QString("color:#555555;font-size:10px;background:transparent;%1").arg(M(10)));

        bool voted = (c.user_vote == "up");
        auto* clikes = new QLabel(QString("▲ %1").arg(c.likes));
        clikes->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(voted || c.likes > 0 ? "#d97706" : "#1e1e1e", M(10)));

        hrh->addWidget(tline);
        hrh->addSpacing(2);
        hrh->addWidget(cav);
        hrh->addWidget(cname);
        hrh->addStretch();
        hrh->addWidget(ctime);
        hrh->addSpacing(10);
        hrh->addWidget(clikes);
        rvl->addWidget(hr);

        // Body
        auto* cbody = new QLabel(c.content);
        cbody->setWordWrap(true);
        cbody->setTextFormat(Qt::PlainText);
        cbody->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        cbody->setStyleSheet(
            QString("color:#666666;font-size:12px;line-height:1.5;background:transparent;"
                    "padding-left:28px;%1").arg(M(12)));
        rvl->addWidget(cbody);

        // Actions
        auto* ar = new QWidget;
        ar->setStyleSheet("background:transparent;");
        auto* arh = new QHBoxLayout(ar);
        arh->setContentsMargins(28, 0, 0, 0);
        arh->setSpacing(12);

        auto mk_act = [&](const QString& lbl, const QString& col, const QString& vt) {
            auto* b = new QPushButton(lbl);
            b->setFixedHeight(16);
            b->setCursor(Qt::PointingHandCursor);
            bool a = (c.user_vote == vt);
            b->setStyleSheet(
                a ? QString("QPushButton{background:transparent;color:%1;border:none;"
                            "font-size:10px;font-weight:700;padding:0;%2}").arg(col, M(10))
                  : QString("QPushButton{background:transparent;color:#555555;border:none;"
                            "font-size:10px;padding:0;%1}"
                            "QPushButton:hover{color:%2;}").arg(M(10), col));
            auto uuid = c.comment_uuid;
            connect(b, &QPushButton::clicked, this, [this, uuid, vt]() {
                emit vote_comment(uuid, vt);
            });
            return b;
        };
        arh->addWidget(mk_act("▲ upvote", "#d97706", "up"));
        arh->addWidget(mk_act("▼ downvote", "#555555", "down"));
        arh->addStretch();
        rvl->addWidget(ar);

        t_comments_vl_->addWidget(row);
    }
}

} // namespace fincept::screens
