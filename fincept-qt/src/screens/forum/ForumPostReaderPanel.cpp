// src/screens/forum/ForumPostReaderPanel.cpp
#include "screens/forum/ForumPostReaderPanel.h"

#include "services/forum/ForumService.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

namespace fincept::screens {

static QString mono(int sz = 12) {
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

static QString str_color(const QString& s) {
    static const QString palette[] = {"#d97706", "#06b6d4", "#10b981", "#8b5cf6", "#ef4444",
                                      "#f97316", "#3b82f6", "#84cc16", "#ec4899", "#14b8a6"};
    uint h = qHash(s);
    return palette[h % 10];
}

ForumPostReaderPanel::ForumPostReaderPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ForumPostReaderPanel::build_ui() {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);

    // ── Page 0: empty placeholder ─────────────────────────────────────────────
    auto* empty_page = new QWidget;
    empty_page->setStyleSheet("background:#080808;");
    {
        auto* vl = new QVBoxLayout(empty_page);
        auto* icon = new QLabel("◈");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(QString("color:#111111;font-size:36px;background:transparent;"));
        auto* lbl = new QLabel("SELECT A POST");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:#1a1a1a;font-size:13px;font-weight:700;letter-spacing:1.5px;"
                                   "background:transparent;%1")
                               .arg(mono(13)));
        auto* sub = new QLabel("Click any post from the feed");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(QString("color:#151515;font-size:11px;background:transparent;%1").arg(mono(11)));
        vl->addStretch();
        vl->addWidget(icon);
        vl->addSpacing(8);
        vl->addWidget(lbl);
        vl->addWidget(sub);
        vl->addStretch();
    }
    stack_->addWidget(empty_page); // 0

    // ── Page 1: loading ───────────────────────────────────────────────────────
    auto* load_page = new QWidget;
    load_page->setStyleSheet("background:#080808;");
    {
        auto* vl = new QVBoxLayout(load_page);
        spin_label_ = new QLabel("⣾");
        spin_label_->setAlignment(Qt::AlignCenter);
        spin_label_->setStyleSheet(
            QString("color:%1;font-size:20px;background:transparent;%2").arg(ui::colors::AMBER, mono(20)));
        vl->addStretch();
        vl->addWidget(spin_label_);
        vl->addStretch();
    }
    stack_->addWidget(load_page); // 1

    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(100);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        static const QString frames[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
        spin_label_->setText(frames[spin_frame_++ % 8]);
    });

    // ── Page 2: content ───────────────────────────────────────────────────────
    content_page_ = new QWidget;
    content_page_->setStyleSheet("background:#080808;");
    auto* content_root = new QVBoxLayout(content_page_);
    content_root->setContentsMargins(0, 0, 0, 0);
    content_root->setSpacing(0);

    // ── Post header zone ──────────────────────────────────────────────────────
    auto* post_hdr = new QWidget;
    post_hdr->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #111111;");
    auto* hdr_vl = new QVBoxLayout(post_hdr);
    hdr_vl->setContentsMargins(16, 12, 16, 10);
    hdr_vl->setSpacing(6);

    // Category chip + time row
    auto* chip_row = new QWidget;
    chip_row->setStyleSheet("background:transparent;");
    auto* chip_hl = new QHBoxLayout(chip_row);
    chip_hl->setContentsMargins(0, 0, 0, 0);
    chip_hl->setSpacing(8);

    category_chip_ = new QLabel;
    category_chip_->setStyleSheet(QString("color:#d97706;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                                          "background:rgba(217,119,6,0.08);border:1px solid rgba(217,119,6,0.3);"
                                          "padding:1px 6px;%1")
                                      .arg(mono(9)));

    meta_label_ = new QLabel;
    meta_label_->setStyleSheet(QString("color:#2a2a2a;font-size:10px;background:transparent;%1").arg(mono(10)));

    chip_hl->addWidget(category_chip_);
    chip_hl->addStretch();
    chip_hl->addWidget(meta_label_);
    hdr_vl->addWidget(chip_row);

    // Title
    title_label_ = new QLabel;
    title_label_->setWordWrap(true);
    title_label_->setStyleSheet(QString("color:#e5e5e5;font-size:15px;font-weight:700;line-height:1.4;"
                                        "background:transparent;%1")
                                    .arg(mono(15)));
    hdr_vl->addWidget(title_label_);

    // Author row
    author_label_ = new QLabel;
    author_label_->setTextFormat(Qt::RichText);
    author_label_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    author_label_->setOpenExternalLinks(false);
    author_label_->setStyleSheet(QString("color:#555555;font-size:11px;background:transparent;%1").arg(mono(11)));
    connect(author_label_, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link.startsWith("author:"))
            emit author_clicked(link.mid(7));
    });
    hdr_vl->addWidget(author_label_);
    content_root->addWidget(post_hdr);

    // ── Scrollable area ───────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea{border:none;background:#080808;}"
                          "QScrollBar:vertical{width:3px;background:transparent;margin:0;}"
                          "QScrollBar::handle:vertical{background:#1a1a1a;min-height:20px;}"
                          "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* scroll_w = new QWidget;
    scroll_w->setStyleSheet("background:#080808;");
    auto* scroll_vl = new QVBoxLayout(scroll_w);
    scroll_vl->setContentsMargins(0, 0, 0, 0);
    scroll_vl->setSpacing(0);

    // Post body
    body_label_ = new QLabel;
    body_label_->setWordWrap(true);
    body_label_->setTextFormat(Qt::PlainText);
    body_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    body_label_->setStyleSheet(QString("color:#888888;font-size:13px;line-height:1.7;background:transparent;"
                                       "padding:16px;border-bottom:1px solid #0d0d0d;%1")
                                   .arg(mono(13)));
    scroll_vl->addWidget(body_label_);

    // ── Engagement bar ────────────────────────────────────────────────────────
    auto* eng_bar = new QWidget;
    eng_bar->setFixedHeight(36);
    eng_bar->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #111111;");
    auto* eng_hl = new QHBoxLayout(eng_bar);
    eng_hl->setContentsMargins(16, 0, 16, 0);
    eng_hl->setSpacing(16);

    // Upvote button
    auto* up_btn = new QPushButton("▲  UPVOTE");
    up_btn->setFixedHeight(24);
    up_btn->setFixedWidth(90);
    up_btn->setCursor(Qt::PointingHandCursor);
    up_btn->setStyleSheet(QString("QPushButton{background:transparent;color:#333333;border:1px solid #1a1a1a;"
                                  "font-size:10px;font-weight:700;padding:0 8px;%1}"
                                  "QPushButton:hover{background:rgba(217,119,6,0.1);color:#d97706;"
                                  "border-color:rgba(217,119,6,0.4);}")
                              .arg(mono(10)));
    connect(up_btn, &QPushButton::clicked, this, [this]() {
        if (!current_detail_.post.post_uuid.isEmpty())
            emit vote_post(current_detail_.post.post_uuid, "up");
    });

    likes_label_ = new QLabel("▲ 0");
    likes_label_->setStyleSheet(QString("color:#444444;font-size:11px;background:transparent;%1").arg(mono(11)));

    replies_label_ = new QLabel("◆ 0 replies");
    replies_label_->setStyleSheet(QString("color:#333333;font-size:11px;background:transparent;%1").arg(mono(11)));

    views_label_ = new QLabel("◉ 0 views");
    views_label_->setStyleSheet(QString("color:#222222;font-size:11px;background:transparent;%1").arg(mono(11)));

    eng_hl->addWidget(up_btn);
    eng_hl->addWidget(likes_label_);
    eng_hl->addWidget(replies_label_);
    eng_hl->addStretch();
    eng_hl->addWidget(views_label_);
    scroll_vl->addWidget(eng_bar);

    // ── Comments header ───────────────────────────────────────────────────────
    auto* com_hdr = new QWidget;
    com_hdr->setFixedHeight(26);
    com_hdr->setStyleSheet("background:#090909;border-bottom:1px solid #111111;");
    auto* com_hdr_hl = new QHBoxLayout(com_hdr);
    com_hdr_hl->setContentsMargins(16, 0, 16, 0);
    auto* com_hdr_lbl = new QLabel("REPLIES");
    com_hdr_lbl->setStyleSheet(QString("color:#222222;font-size:10px;font-weight:700;letter-spacing:1px;"
                                       "background:transparent;%1")
                                   .arg(mono(10)));
    com_hdr_hl->addWidget(com_hdr_lbl);
    scroll_vl->addWidget(com_hdr);

    // Comments area
    comments_area_ = new QWidget;
    comments_area_->setStyleSheet("background:#080808;");
    comments_layout_ = new QVBoxLayout(comments_area_);
    comments_layout_->setContentsMargins(0, 0, 0, 0);
    comments_layout_->setSpacing(0);
    scroll_vl->addWidget(comments_area_);
    scroll_vl->addStretch();

    scroll->setWidget(scroll_w);
    content_root->addWidget(scroll, 1);

    // ── Reply compose bar ─────────────────────────────────────────────────────
    auto* compose = new QWidget;
    compose->setFixedHeight(42);
    compose->setStyleSheet("background:#0a0a0a;border-top:1px solid #111111;");
    auto* compose_hl = new QHBoxLayout(compose);
    compose_hl->setContentsMargins(12, 6, 12, 6);
    compose_hl->setSpacing(8);

    auto* compose_icon = new QLabel("◉");
    compose_icon->setStyleSheet("color:#222222;font-size:14px;background:transparent;");
    compose_icon->setFixedWidth(18);

    reply_input_ = new QLineEdit;
    reply_input_->setPlaceholderText("write a reply...");
    reply_input_->setStyleSheet(QString("QLineEdit{background:#0d0d0d;color:#888888;border:1px solid #1a1a1a;"
                                        "padding:4px 10px;font-size:12px;%1}"
                                        "QLineEdit:focus{border-color:#333333;color:#e5e5e5;}"
                                        "QLineEdit::placeholder{color:#2a2a2a;}")
                                    .arg(mono(12)));

    auto* send_btn = new QPushButton("SEND  ↵");
    send_btn->setFixedSize(72, 28);
    send_btn->setCursor(Qt::PointingHandCursor);
    send_btn->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:#555555;"
                                    "border:1px solid #1a1a1a;font-size:10px;font-weight:700;%1}"
                                    "QPushButton:hover{background:rgba(217,119,6,0.25);color:#d97706;"
                                    "border-color:rgba(217,119,6,0.4);}")
                                .arg(mono(10)));

    auto submit = [this, send_btn]() {
        QString text = reply_input_->text().trimmed();
        if (text.isEmpty() || current_detail_.post.post_uuid.isEmpty())
            return;
        emit comment_submitted(current_detail_.post.post_uuid, text);
        reply_input_->clear();
        Q_UNUSED(send_btn)
    };
    connect(send_btn, &QPushButton::clicked, this, submit);
    connect(reply_input_, &QLineEdit::returnPressed, this, submit);

    compose_hl->addWidget(compose_icon);
    compose_hl->addWidget(reply_input_, 1);
    compose_hl->addWidget(send_btn);
    content_root->addWidget(compose);

    stack_->addWidget(content_page_); // 2
    root->addWidget(stack_, 1);
    stack_->setCurrentIndex(0);
}

void ForumPostReaderPanel::set_loading(bool on) {
    if (on) {
        spin_frame_ = 0;
        spin_timer_->start();
        stack_->setCurrentIndex(1);
    } else {
        spin_timer_->stop();
    }
}

void ForumPostReaderPanel::clear() {
    spin_timer_->stop();
    stack_->setCurrentIndex(0);
    current_detail_ = {};
}

void ForumPostReaderPanel::show_post(const services::ForumPostDetail& detail) {
    spin_timer_->stop();
    current_detail_ = detail;

    // Category chip
    QString cat_color =
        detail.post.category_color.isEmpty() ? str_color(detail.post.category_name) : detail.post.category_color;
    category_chip_->setText(detail.post.category_name.toUpper());
    category_chip_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                                          "background:transparent;border:1px solid %1;"
                                          "padding:1px 6px;%2")
                                      .arg(cat_color, mono(9)));

    // Time
    meta_label_->setText(rel_time(detail.post.created_at));

    // Title
    title_label_->setText(detail.post.title);

    // Author
    QString av_color = str_color(detail.post.author_display_name);
    QString initials =
        detail.post.author_display_name.isEmpty() ? "?" : detail.post.author_display_name.left(2).toUpper();
    QString author_link =
        QString("<span style='background:%1;color:#080808;font-size:9px;"
                "font-family:Consolas;padding:1px 4px;'>%2</span>"
                "&nbsp;"
                "<a href='author:%3' style='color:%4;text-decoration:none;"
                "font-family:Consolas;font-size:11px;'>%5</a>")
            .arg(av_color, initials, detail.post.author_name, "#555555", detail.post.author_display_name.toUpper());
    author_label_->setText(author_link);

    // Body
    body_label_->setText(detail.post.content);

    // Engagement
    likes_label_->setText(detail.post.likes > 0
                              ? QString("<span style='color:#d97706;'>▲ %1</span>").arg(detail.post.likes)
                              : QString("▲ 0"));
    likes_label_->setTextFormat(Qt::RichText);

    replies_label_->setText(
        QString("◆ %1 %2").arg(detail.total_comments).arg(detail.total_comments == 1 ? "reply" : "replies"));

    views_label_->setText(QString("◉ %1 views").arg(detail.post.views));

    rebuild_comments();
    stack_->setCurrentIndex(2);
}

void ForumPostReaderPanel::rebuild_comments() {
    while (comments_layout_->count() > 0) {
        auto* item = comments_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (current_detail_.comments.isEmpty()) {
        auto* no_com = new QWidget;
        no_com->setFixedHeight(52);
        no_com->setStyleSheet("background:transparent;");
        auto* vl = new QVBoxLayout(no_com);
        auto* lbl = new QLabel("NO REPLIES YET");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:#1a1a1a;font-size:11px;font-weight:700;letter-spacing:1px;"
                                   "background:transparent;%1")
                               .arg(mono(11)));
        auto* sub = new QLabel("Be the first to reply");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(QString("color:#141414;font-size:10px;background:transparent;%1").arg(mono(10)));
        vl->addWidget(lbl);
        vl->addWidget(sub);
        comments_layout_->addWidget(no_com);
        return;
    }

    for (int i = 0; i < current_detail_.comments.size(); ++i) {
        const auto& c = current_detail_.comments[i];

        auto* row = new QWidget;
        row->setStyleSheet(i % 2 == 0 ? "background:#080808;border-bottom:1px solid #0d0d0d;"
                                      : "background:#090909;border-bottom:1px solid #0d0d0d;");
        auto* row_vl = new QVBoxLayout(row);
        row_vl->setContentsMargins(16, 8, 16, 6);
        row_vl->setSpacing(4);

        // ── Comment header ─────────────────────────────────────────────────────
        auto* com_top = new QWidget;
        com_top->setStyleSheet("background:transparent;");
        auto* com_hl = new QHBoxLayout(com_top);
        com_hl->setContentsMargins(0, 0, 0, 0);
        com_hl->setSpacing(6);

        // Thread line + avatar
        auto* thread_line = new QWidget;
        thread_line->setFixedWidth(2);
        thread_line->setStyleSheet(QString("background:%1;").arg(str_color(c.author_display_name)));

        // Avatar
        auto* av = new QLabel(c.author_display_name.left(2).toUpper());
        av->setFixedSize(20, 20);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet(QString("color:#080808;font-size:9px;font-weight:700;background:%1;%2")
                              .arg(str_color(c.author_display_name), mono(9)));

        // Author name
        auto* author_lbl = new QLabel(c.author_display_name.toUpper().left(18));
        author_lbl->setStyleSheet(
            QString("color:#666666;font-size:11px;font-weight:600;background:transparent;%1").arg(mono(11)));

        // Time
        auto* time_lbl = new QLabel(rel_time(c.created_at));
        time_lbl->setStyleSheet(QString("color:#252525;font-size:10px;background:transparent;%1").arg(mono(10)));

        // Likes
        bool voted = (c.user_vote == "up");
        QString like_col = (voted || c.likes > 0) ? "#d97706" : "#252525";
        auto* likes_lbl = new QLabel(QString("▲ %1").arg(c.likes));
        likes_lbl->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;%2").arg(like_col, mono(10)));

        com_hl->addWidget(thread_line);
        com_hl->addSpacing(6);
        com_hl->addWidget(av);
        com_hl->addWidget(author_lbl);
        com_hl->addStretch();
        com_hl->addWidget(time_lbl);
        com_hl->addSpacing(8);
        com_hl->addWidget(likes_lbl);
        row_vl->addWidget(com_top);

        // ── Comment body ───────────────────────────────────────────────────────
        auto* body = new QLabel(c.content);
        body->setWordWrap(true);
        body->setTextFormat(Qt::PlainText);
        body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        body->setStyleSheet(QString("color:#666666;font-size:12px;line-height:1.5;"
                                    "background:transparent;padding-left:20px;%1")
                                .arg(mono(12)));
        row_vl->addWidget(body);

        // ── Action row ─────────────────────────────────────────────────────────
        auto* act_row = new QWidget;
        act_row->setStyleSheet("background:transparent;");
        auto* act_hl = new QHBoxLayout(act_row);
        act_hl->setContentsMargins(20, 0, 0, 0);
        act_hl->setSpacing(8);

        auto make_act = [&](const QString& label, const QString& color, const QString& vtype) {
            auto* btn = new QPushButton(label);
            btn->setFixedHeight(16);
            btn->setCursor(Qt::PointingHandCursor);
            bool active = (c.user_vote == vtype);
            btn->setStyleSheet(active ? QString("QPushButton{background:transparent;color:%1;border:none;"
                                                "font-size:10px;font-weight:700;padding:0;%2}")
                                            .arg(color, mono(10))
                                      : QString("QPushButton{background:transparent;color:#252525;border:none;"
                                                "font-size:10px;padding:0;%1}"
                                                "QPushButton:hover{color:%2;}")
                                            .arg(mono(10), color));
            auto uuid = c.comment_uuid;
            connect(btn, &QPushButton::clicked, this, [this, uuid, vtype]() { emit vote_comment(uuid, vtype); });
            return btn;
        };

        act_hl->addWidget(make_act("▲ upvote", "#d97706", "up"));
        act_hl->addWidget(make_act("▼ downvote", "#555555", "down"));
        act_hl->addStretch();
        row_vl->addWidget(act_row);

        comments_layout_->addWidget(row);
    }
}

} // namespace fincept::screens
