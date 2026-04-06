// src/screens/forum/ForumPostReaderPanel.cpp
#include "screens/forum/ForumPostReaderPanel.h"

#include "services/forum/ForumService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

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

ForumPostReaderPanel::ForumPostReaderPanel(QWidget* parent) : QWidget(parent) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) { setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY())); });
    build_ui();
}

void ForumPostReaderPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);

    // ── Page 0: empty placeholder ─────────────────────────────────────────────
    auto* empty_page = new QWidget;
    empty_page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    {
        auto* vl = new QVBoxLayout(empty_page);
        vl->setAlignment(Qt::AlignCenter);
        vl->setSpacing(10);

        auto* icon = new QLabel("◈");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(
            QString("color:%1;font-size:40px;background:transparent;")
                .arg(ui::colors::BORDER_DIM));

        auto* lbl = new QLabel("SELECT A POST");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(
            QString("color:%1;font-size:14px;font-weight:700;letter-spacing:2px;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY, M(14)));

        auto* sub = new QLabel("Click any post from the feed to read it");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM, M(11)));

        vl->addWidget(icon);
        vl->addSpacing(4);
        vl->addWidget(lbl);
        vl->addWidget(sub);
    }
    stack_->addWidget(empty_page); // 0

    // ── Page 1: loading ───────────────────────────────────────────────────────
    auto* load_page = new QWidget;
    load_page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    {
        auto* vl = new QVBoxLayout(load_page);
        vl->setAlignment(Qt::AlignCenter);
        vl->setSpacing(10);

        spin_label_ = new QLabel("⣾");
        spin_label_->setAlignment(Qt::AlignCenter);
        spin_label_->setStyleSheet(
            QString("color:%1;font-size:24px;background:transparent;%2")
                .arg(ui::colors::AMBER, M(24)));

        auto* lt = new QLabel("Loading...");
        lt->setAlignment(Qt::AlignCenter);
        lt->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY, M(11)));

        vl->addWidget(spin_label_);
        vl->addWidget(lt);
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
    content_page_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* content_root = new QVBoxLayout(content_page_);
    content_root->setContentsMargins(0, 0, 0, 0);
    content_root->setSpacing(0);

    // ── Post header with gradient background ──────────────────────────────────
    auto* post_hdr = new QWidget;
    post_hdr->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #0e0e0e,stop:1 %1);"
                "border-bottom:1px solid %2;")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));
    auto* hdr_vl = new QVBoxLayout(post_hdr);
    hdr_vl->setContentsMargins(20, 16, 20, 14);
    hdr_vl->setSpacing(8);

    // Category chip + time row
    auto* chip_row = new QWidget;
    chip_row->setStyleSheet("background:transparent;");
    auto* chip_hl = new QHBoxLayout(chip_row);
    chip_hl->setContentsMargins(0, 0, 0, 0);
    chip_hl->setSpacing(8);

    category_chip_ = new QLabel;
    category_chip_->setFixedHeight(18);
    category_chip_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                "background:transparent;border:1px solid %1;"
                "padding:1px 8px;border-radius:9px;%2")
            .arg(ui::colors::AMBER, M(9)));

    meta_label_ = new QLabel;
    meta_label_->setStyleSheet(
        QString("color:%1;font-size:10px;background:transparent;%2")
            .arg(ui::colors::TEXT_DIM, M(10)));

    chip_hl->addWidget(category_chip_);
    chip_hl->addStretch();
    chip_hl->addWidget(meta_label_);
    hdr_vl->addWidget(chip_row);

    // Title
    title_label_ = new QLabel;
    title_label_->setWordWrap(true);
    title_label_->setStyleSheet(
        QString("color:%1;font-size:16px;font-weight:700;line-height:1.4;"
                "background:transparent;%2")
            .arg(ui::colors::TEXT_PRIMARY, M(16)));
    hdr_vl->addWidget(title_label_);

    // Author row
    author_label_ = new QLabel;
    author_label_->setTextFormat(Qt::RichText);
    author_label_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    author_label_->setOpenExternalLinks(false);
    author_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(11)));
    connect(author_label_, &QLabel::linkActivated, this,
            [this](const QString& link) {
                if (link.startsWith("author:"))
                    emit author_clicked(link.mid(7));
            });
    hdr_vl->addWidget(author_label_);

    // Gradient separator
    auto* hdr_sep = new QWidget;
    hdr_sep->setFixedHeight(2);
    hdr_sep->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 %1,stop:0.3 %2,stop:1 transparent);")
            .arg(ui::colors::AMBER, ui::colors::BORDER_DIM));
    hdr_vl->addWidget(hdr_sep);
    content_root->addWidget(post_hdr);

    // ── Scrollable area ───────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:%1;}"
                "QScrollBar:vertical{width:6px;background:transparent;margin:0;}"
                "QScrollBar::handle:vertical{background:%2;min-height:30px;"
                "border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical"
                "{height:0;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));

    auto* scroll_w = new QWidget;
    scroll_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* scroll_vl = new QVBoxLayout(scroll_w);
    scroll_vl->setContentsMargins(0, 0, 0, 0);
    scroll_vl->setSpacing(0);

    // Post body
    body_label_ = new QLabel;
    body_label_->setWordWrap(true);
    body_label_->setTextFormat(Qt::PlainText);
    body_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    body_label_->setStyleSheet(
        QString("color:%1;font-size:13px;line-height:1.7;background:transparent;"
                "padding:16px 20px;border-bottom:1px solid %2;%3")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, M(13)));
    scroll_vl->addWidget(body_label_);

    // ── Engagement bar ────────────────────────────────────────────────────────
    auto* eng_bar = new QWidget;
    eng_bar->setFixedHeight(40);
    eng_bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* eng_hl = new QHBoxLayout(eng_bar);
    eng_hl->setContentsMargins(20, 0, 20, 0);
    eng_hl->setSpacing(14);

    auto* up_btn = new QPushButton("▲  Upvote");
    up_btn->setFixedHeight(26);
    up_btn->setCursor(Qt::PointingHandCursor);
    up_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.06);"
                "color:%1;border:1px solid rgba(217,119,6,0.2);"
                "font-size:10px;font-weight:700;padding:0 12px;"
                "border-radius:13px;%2}"
                "QPushButton:hover{background:rgba(217,119,6,0.12);"
                "color:%3;border-color:rgba(217,119,6,0.5);}")
            .arg(ui::colors::TEXT_SECONDARY, M(10), ui::colors::AMBER));
    connect(up_btn, &QPushButton::clicked, this, [this]() {
        if (!current_detail_.post.post_uuid.isEmpty())
            emit vote_post(current_detail_.post.post_uuid, "up");
    });

    likes_label_ = new QLabel("▲ 0");
    likes_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(11)));

    replies_label_ = new QLabel("◆ 0 replies");
    replies_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(11)));

    views_label_ = new QLabel("◉ 0 views");
    views_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2")
            .arg(ui::colors::TEXT_DIM, M(11)));

    eng_hl->addWidget(up_btn);
    eng_hl->addWidget(likes_label_);
    eng_hl->addWidget(replies_label_);
    eng_hl->addStretch();
    eng_hl->addWidget(views_label_);
    scroll_vl->addWidget(eng_bar);

    // ── Comments header ───────────────────────────────────────────────────────
    auto* com_hdr = new QWidget;
    com_hdr->setFixedHeight(28);
    com_hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* com_hdr_hl = new QHBoxLayout(com_hdr);
    com_hdr_hl->setContentsMargins(20, 0, 20, 0);
    com_hdr_hl->setSpacing(6);
    auto* com_dot = new QLabel("◆");
    com_dot->setStyleSheet(
        QString("color:%1;font-size:7px;background:transparent;")
            .arg(ui::colors::CYAN));
    auto* com_hdr_lbl = new QLabel("REPLIES");
    com_hdr_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1.5px;"
                "background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(10)));
    com_hdr_hl->addWidget(com_dot);
    com_hdr_hl->addWidget(com_hdr_lbl);
    com_hdr_hl->addStretch();
    scroll_vl->addWidget(com_hdr);

    // Comments area
    comments_area_ = new QWidget;
    comments_area_->setStyleSheet(
        QString("background:%1;").arg(ui::colors::BG_BASE));
    comments_layout_ = new QVBoxLayout(comments_area_);
    comments_layout_->setContentsMargins(0, 0, 0, 0);
    comments_layout_->setSpacing(0);
    scroll_vl->addWidget(comments_area_);
    scroll_vl->addStretch();

    scroll->setWidget(scroll_w);
    content_root->addWidget(scroll, 1);

    // ── Reply compose bar ─────────────────────────────────────────────────────
    auto* compose = new QWidget;
    compose->setFixedHeight(48);
    compose->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* compose_hl = new QHBoxLayout(compose);
    compose_hl->setContentsMargins(16, 8, 16, 8);
    compose_hl->setSpacing(8);

    reply_input_ = new QLineEdit;
    reply_input_->setPlaceholderText("Write a reply...");
    reply_input_->setFixedHeight(32);
    reply_input_->setStyleSheet(
        QString("QLineEdit{background:rgba(255,255,255,0.03);color:%1;"
                "border:1px solid %2;padding:4px 12px;font-size:12px;"
                "border-radius:4px;%3}"
                "QLineEdit:focus{border-color:%4;color:%5;}"
                "QLineEdit::placeholder{color:%6;}")
            .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM,
                 M(12), ui::colors::BORDER_BRIGHT,
                 ui::colors::TEXT_PRIMARY, ui::colors::TEXT_DIM));

    auto* send_btn = new QPushButton("Reply");
    send_btn->setFixedSize(72, 32);
    send_btn->setCursor(Qt::PointingHandCursor);
    send_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;"
                "border:1px solid rgba(217,119,6,0.25);font-size:11px;"
                "font-weight:700;border-radius:4px;%2}"
                "QPushButton:hover{background:rgba(217,119,6,0.2);"
                "color:%3;border-color:rgba(217,119,6,0.5);}")
            .arg(ui::colors::TEXT_SECONDARY, M(11), ui::colors::AMBER));

    auto submit = [this]() {
        QString text = reply_input_->text().trimmed();
        if (text.isEmpty() || current_detail_.post.post_uuid.isEmpty())
            return;
        emit comment_submitted(current_detail_.post.post_uuid, text);
        reply_input_->clear();
    };
    connect(send_btn, &QPushButton::clicked, this, submit);
    connect(reply_input_, &QLineEdit::returnPressed, this, submit);

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

    QString cat_color = detail.post.category_color.isEmpty()
                            ? det_color(detail.post.category_name)
                            : detail.post.category_color;

    category_chip_->setText(detail.post.category_name.toUpper());
    category_chip_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                "background:transparent;border:1px solid %1;"
                "padding:1px 8px;border-radius:9px;%2")
            .arg(cat_color, M(9)));

    meta_label_->setText(rel_time(detail.post.created_at));
    title_label_->setText(detail.post.title);

    // Author
    QString av_color = det_color(detail.post.author_display_name);
    QString initials = detail.post.author_display_name.isEmpty()
                           ? "?"
                           : detail.post.author_display_name.left(2).toUpper();
    author_label_->setText(
        QString("<span style='background:%1;color:%2;font-size:9px;"
                "font-family:Consolas;padding:2px 5px;font-weight:700;"
                "border-radius:10px;'>%3</span>"
                "&nbsp;"
                "<a href='author:%4' style='color:%5;text-decoration:none;"
                "font-family:Consolas;font-size:11px;font-weight:600;'>%6</a>")
            .arg(av_color, ui::colors::BG_BASE, initials,
                 detail.post.author_name, ui::colors::TEXT_SECONDARY,
                 detail.post.author_display_name));

    body_label_->setText(detail.post.content);

    // Engagement
    bool voted = (detail.post.user_vote == "up");
    QString lc = voted || detail.post.likes > 0
                     ? ui::colors::AMBER
                     : ui::colors::TEXT_DIM;
    likes_label_->setText(QString("▲ %1").arg(detail.post.likes));
    likes_label_->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:600;"
                "background:transparent;%2")
            .arg(lc, M(11)));

    replies_label_->setText(
        QString("◆ %1 %2")
            .arg(detail.total_comments)
            .arg(detail.total_comments == 1 ? "reply" : "replies"));

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
        no_com->setMinimumHeight(60);
        no_com->setStyleSheet("background:transparent;");
        auto* vl = new QVBoxLayout(no_com);
        vl->setAlignment(Qt::AlignCenter);
        vl->setSpacing(4);

        auto* icon = new QLabel("◇");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(
            QString("color:%1;font-size:18px;background:transparent;")
                .arg(ui::colors::BORDER_DIM));

        auto* lbl = new QLabel("NO REPLIES YET");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY, M(11)));

        vl->addWidget(icon);
        vl->addWidget(lbl);
        comments_layout_->addWidget(no_com);
        return;
    }

    for (int i = 0; i < current_detail_.comments.size(); ++i) {
        const auto& c = current_detail_.comments[i];
        QString avc = det_color(c.author_display_name);

        auto* row = new QWidget;
        row->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
                .arg(i % 2 == 0 ? ui::colors::BG_BASE : ui::colors::BG_SURFACE,
                     ui::colors::BORDER_DIM));
        auto* row_vl = new QVBoxLayout(row);
        row_vl->setContentsMargins(20, 10, 20, 8);
        row_vl->setSpacing(5);

        // ── Comment header ────────────────────────────────────────────────────
        auto* com_top = new QWidget;
        com_top->setStyleSheet("background:transparent;");
        auto* com_hl = new QHBoxLayout(com_top);
        com_hl->setContentsMargins(0, 0, 0, 0);
        com_hl->setSpacing(8);

        auto* thread_line = new QWidget;
        thread_line->setFixedSize(3, 20);
        thread_line->setStyleSheet(
            QString("background:%1;border-radius:1px;").arg(avc));

        auto* av = new QLabel(c.author_display_name.left(2).toUpper());
        av->setFixedSize(22, 22);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:%2;"
                    "border-radius:11px;%3")
                .arg(ui::colors::BG_BASE, avc, M(9)));

        auto* author_lbl = new QLabel(c.author_display_name);
        author_lbl->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:600;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_PRIMARY, M(11)));

        auto* time_lbl = new QLabel(rel_time(c.created_at));
        time_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM, M(10)));

        bool voted = (c.user_vote == "up");
        auto* likes_lbl = new QLabel(QString("▲ %1").arg(c.likes));
        likes_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:600;"
                    "background:transparent;%2")
                .arg(voted || c.likes > 0 ? ui::colors::AMBER
                                           : ui::colors::TEXT_DIM,
                     M(10)));

        com_hl->addWidget(thread_line);
        com_hl->addWidget(av);
        com_hl->addWidget(author_lbl);
        com_hl->addStretch();
        com_hl->addWidget(time_lbl);
        com_hl->addSpacing(6);
        com_hl->addWidget(likes_lbl);
        row_vl->addWidget(com_top);

        // ── Comment body ──────────────────────────────────────────────────────
        auto* body = new QLabel(c.content);
        body->setWordWrap(true);
        body->setTextFormat(Qt::PlainText);
        body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        body->setStyleSheet(
            QString("color:%1;font-size:12px;line-height:1.5;"
                    "background:transparent;padding-left:28px;%2")
                .arg(ui::colors::TEXT_SECONDARY, M(12)));
        row_vl->addWidget(body);

        // ── Action row ────────────────────────────────────────────────────────
        auto* act_row = new QWidget;
        act_row->setStyleSheet("background:transparent;");
        auto* act_hl = new QHBoxLayout(act_row);
        act_hl->setContentsMargins(28, 0, 0, 0);
        act_hl->setSpacing(12);

        auto make_act = [&](const QString& label, const QString& color,
                            const QString& vtype) {
            auto* btn = new QPushButton(label);
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            bool active = (c.user_vote == vtype);
            btn->setStyleSheet(
                active
                    ? QString("QPushButton{background:transparent;color:%1;"
                              "border:none;font-size:10px;font-weight:700;"
                              "padding:0;%2}")
                          .arg(color, M(10))
                    : QString("QPushButton{background:transparent;color:%1;"
                              "border:none;font-size:10px;padding:0;%2}"
                              "QPushButton:hover{color:%3;}")
                          .arg(ui::colors::TEXT_DIM, M(10), color));
            auto uuid = c.comment_uuid;
            connect(btn, &QPushButton::clicked, this,
                    [this, uuid, vtype]() { emit vote_comment(uuid, vtype); });
            return btn;
        };

        act_hl->addWidget(make_act("▲ upvote", ui::colors::AMBER, "up"));
        act_hl->addWidget(
            make_act("▼ downvote", ui::colors::TEXT_TERTIARY, "down"));
        act_hl->addStretch();
        row_vl->addWidget(act_row);

        comments_layout_->addWidget(row);
    }
}

} // namespace fincept::screens
