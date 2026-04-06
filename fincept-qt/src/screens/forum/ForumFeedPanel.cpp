// src/screens/forum/ForumFeedPanel.cpp
#include "screens/forum/ForumFeedPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

#include <cmath>

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

static QString fmt_n(int n) {
    if (n >= 10000)
        return QString("%1K").arg(n / 1000);
    if (n >= 1000)
        return QString("%1.%2K").arg(n / 1000).arg((n % 1000) / 100);
    return QString::number(n);
}

ForumFeedPanel::ForumFeedPanel(QWidget* parent) : QWidget(parent) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) { setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY())); });
    build_ui();
}

void ForumFeedPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_toolbar();
    root->addWidget(toolbar_);

    // ── Scrollable content ────────────────────────────────────────────────────
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet(
        QString("QScrollArea{border:none;background:%1;}"
                "QScrollBar:vertical{width:6px;background:transparent;margin:0;}"
                "QScrollBar::handle:vertical{background:%2;min-height:40px;"
                "border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical"
                "{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    content_w_ = new QWidget;
    content_w_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    content_vl_ = new QVBoxLayout(content_w_);
    content_vl_->setContentsMargins(0, 0, 0, 0);
    content_vl_->setSpacing(0);

    // Posts container
    posts_container_ = new QWidget;
    posts_container_->setStyleSheet("background:transparent;");
    posts_vl_ = new QVBoxLayout(posts_container_);
    posts_vl_->setContentsMargins(0, 0, 0, 0);
    posts_vl_->setSpacing(0);
    content_vl_->addWidget(posts_container_);
    content_vl_->addStretch();

    scroll_->setWidget(content_w_);
    root->addWidget(scroll_, 1);

    skeleton_timer_ = new QTimer(this);
    skeleton_timer_->setInterval(350);
    connect(skeleton_timer_, &QTimer::timeout, this, &ForumFeedPanel::pulse_skeleton);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Toolbar: header + category chips + new post button
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::build_toolbar() {
    toolbar_ = new QWidget;
    toolbar_->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* tb_vl = new QVBoxLayout(toolbar_);
    tb_vl->setContentsMargins(0, 0, 0, 0);
    tb_vl->setSpacing(0);

    // ── Top row: header + stats + new post ────────────────────────────────────
    auto* top_row = new QWidget;
    top_row->setFixedHeight(48);
    top_row->setStyleSheet("background:transparent;");
    auto* top_hl = new QHBoxLayout(top_row);
    top_hl->setContentsMargins(20, 0, 16, 0);
    top_hl->setSpacing(12);

    // Channel icon + name
    auto* hash = new QLabel("#");
    hash->setStyleSheet(
        QString("color:%1;font-size:22px;font-weight:700;background:transparent;%2")
            .arg(ui::colors::AMBER(), M(22)));

    header_lbl_ = new QLabel("DISCUSSIONS");
    header_lbl_->setStyleSheet(
        QString("color:%1;font-size:15px;font-weight:700;letter-spacing:1px;"
                "background:transparent;%2")
            .arg(ui::colors::TEXT_PRIMARY(), M(15)));

    header_count_lbl_ = new QLabel;
    header_count_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY(), M(11)));

    // New post button
    auto* new_btn = new QPushButton("+ NEW POST");
    new_btn->setFixedHeight(30);
    new_btn->setCursor(Qt::PointingHandCursor);
    new_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;"
                "border:1px solid rgba(217,119,6,0.25);padding:0 16px;"
                "font-size:11px;font-weight:700;letter-spacing:0.5px;%2}"
                "QPushButton:hover{background:rgba(217,119,6,0.2);color:%3;"
                "border-color:rgba(217,119,6,0.5);}")
            .arg(ui::colors::TEXT_SECONDARY(), M(11), ui::colors::AMBER()));
    connect(new_btn, &QPushButton::clicked, this,
            [this]() { emit new_post_clicked(); });

    top_hl->addWidget(hash);
    top_hl->addWidget(header_lbl_);
    top_hl->addWidget(header_count_lbl_);
    top_hl->addStretch();
    top_hl->addWidget(new_btn);
    tb_vl->addWidget(top_row);

    // ── Bottom row: scrollable category chips ─────────────────────────────────
    auto* chips_scroll = new QScrollArea;
    chips_scroll->setFixedHeight(36);
    chips_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chips_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chips_scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;"
                "border-top:1px solid %1;}")
            .arg(ui::colors::BORDER_DIM()));
    chips_scroll->setWidgetResizable(false);

    chips_container_ = new QWidget;
    chips_container_->setFixedHeight(36);
    chips_container_->setStyleSheet("background:transparent;");
    chips_layout_ = new QHBoxLayout(chips_container_);
    chips_layout_->setContentsMargins(16, 0, 16, 0);
    chips_layout_->setSpacing(6);
    chips_layout_->addStretch();

    chips_container_->adjustSize();
    chips_scroll->setWidget(chips_container_);
    tb_vl->addWidget(chips_scroll);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Data setters
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::set_stats(const services::ForumStats& stats) {
    stats_ = stats;
    stats_loaded_ = true;
}

void ForumFeedPanel::set_profile(const services::ForumProfile& profile) {
    profile_ = profile;
}

void ForumFeedPanel::set_categories(const QVector<services::ForumCategory>& cats,
                                     int active_id) {
    categories_ = cats;
    active_cat_id_ = active_id;
    rebuild_category_chips();
}

void ForumFeedPanel::set_header(const QString& text) {
    header_lbl_->setText(text.toUpper());
}

void ForumFeedPanel::set_loading(bool on) {
    if (on)
        show_skeleton();
    else {
        skeleton_timer_->stop();
        if (skeleton_w_) {
            skeleton_w_->deleteLater();
            skeleton_w_ = nullptr;
        }
    }
}

void ForumFeedPanel::clear_active() {
    active_uuid_.clear();
}

void ForumFeedPanel::set_active_post(const QString& uuid) {
    active_uuid_ = uuid;
}

void ForumFeedPanel::set_posts(const services::ForumPostsPage& pg,
                                const QString& cc) {
    page_ = pg;
    cat_color_ = cc;
    skeleton_timer_->stop();
    if (skeleton_w_) {
        skeleton_w_->deleteLater();
        skeleton_w_ = nullptr;
    }
    rebuild_posts();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Category chips
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::rebuild_category_chips() {
    while (chips_layout_->count() > 0) {
        auto* item = chips_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const auto& cat : categories_) {
        const bool active = (cat.id == active_cat_id_);
        QString cc = cat.color.isEmpty() ? ui::colors::AMBER() : cat.color;

        auto* chip = new QPushButton(cat.name.toUpper());
        chip->setFixedHeight(24);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(
            active
                ? QString("QPushButton{background:%1;color:%2;"
                          "border:none;padding:0 14px;"
                          "font-size:10px;font-weight:700;letter-spacing:0.5px;"
                          "border-radius:12px;%3}")
                      .arg(cc, ui::colors::BG_BASE(), M(10))
                : QString("QPushButton{background:transparent;color:%1;"
                          "border:1px solid %2;padding:0 14px;"
                          "font-size:10px;letter-spacing:0.3px;"
                          "border-radius:12px;%3}"
                          "QPushButton:hover{color:%4;border-color:%5;"
                          "background:rgba(255,255,255,0.02);}")
                      .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                           M(10), ui::colors::TEXT_PRIMARY(),
                           ui::colors::BORDER_MED()));
        connect(chip, &QPushButton::clicked, this,
                [this, cat]() {
                    emit category_clicked(cat.id, cat.name, cat.color);
                });
        chips_layout_->addWidget(chip);
    }

    chips_layout_->addStretch();
    chips_container_->adjustSize();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Posts
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::rebuild_posts() {
    posts_container_->setUpdatesEnabled(false);
    while (posts_vl_->count() > 0) {
        auto* item = posts_vl_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    header_count_lbl_->setText(
        page_.total > 0 ? QString("%1 posts").arg(page_.total) : "");

    if (page_.posts.isEmpty()) {
        auto* empty = new QWidget;
        empty->setMinimumHeight(200);
        empty->setStyleSheet("background:transparent;");
        auto* evl = new QVBoxLayout(empty);
        evl->setAlignment(Qt::AlignCenter);
        evl->setSpacing(8);

        auto* icon = new QLabel("◇");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(
            QString("color:%1;font-size:32px;background:transparent;")
                .arg(ui::colors::BORDER_DIM()));

        auto* msg = new QLabel("NO DISCUSSIONS YET");
        msg->setAlignment(Qt::AlignCenter);
        msg->setStyleSheet(
            QString("color:%1;font-size:14px;font-weight:700;letter-spacing:1.5px;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY(), M(14)));

        auto* sub = new QLabel("Be the first to start a conversation");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM(), M(11)));

        evl->addWidget(icon);
        evl->addWidget(msg);
        evl->addWidget(sub);
        posts_vl_->addWidget(empty);
        posts_container_->setUpdatesEnabled(true);
        return;
    }

    // ── Post cards with modern design ─────────────────────────────────────────
    auto* grid = new QWidget;
    grid->setStyleSheet("background:transparent;");
    auto* gvl = new QVBoxLayout(grid);
    gvl->setContentsMargins(16, 10, 16, 10);
    gvl->setSpacing(8);

    for (int i = 0; i < page_.posts.size(); ++i) {
        const auto& p = page_.posts[i];
        const bool active = (p.post_uuid == active_uuid_);
        QString cc = p.category_color.isEmpty() ? det_color(p.category_name)
                                                 : p.category_color;
        bool voted_up = (p.user_vote == "up");

        auto* card = new QWidget;
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(
            active
                ? QString("background:rgba(217,119,6,0.04);"
                          "border:1px solid rgba(217,119,6,0.2);"
                          "border-left:3px solid %1;border-radius:4px;")
                      .arg(cc)
                : QString("background:%1;"
                          "border:1px solid %2;"
                          "border-left:3px solid transparent;"
                          "border-radius:4px;")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(16, 12, 16, 10);
        cl->setSpacing(6);

        // ── Row 1: avatar + author + category + time ──────────────────────────
        auto* r1 = new QWidget;
        r1->setStyleSheet("background:transparent;");
        auto* r1h = new QHBoxLayout(r1);
        r1h->setContentsMargins(0, 0, 0, 0);
        r1h->setSpacing(8);

        // Circular avatar
        auto* av = new QLabel(p.author_display_name.left(2).toUpper());
        av->setFixedSize(28, 28);
        av->setAlignment(Qt::AlignCenter);
        QString avc = det_color(p.author_display_name);
        av->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;background:%2;"
                    "border-radius:14px;%3")
                .arg(ui::colors::BG_BASE(), avc, M(10)));

        // Author + time column
        auto* author_col = new QVBoxLayout;
        author_col->setSpacing(0);
        author_col->setContentsMargins(0, 0, 0, 0);

        auto* au = new QLabel(p.author_display_name);
        au->setStyleSheet(
            QString("color:%1;font-size:12px;font-weight:600;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_PRIMARY(), M(12)));

        auto* tm = new QLabel(rel_time(p.created_at));
        tm->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM(), M(10)));

        author_col->addWidget(au);
        author_col->addWidget(tm);

        // Category chip (pill style)
        auto* chip = new QLabel(p.category_name.toUpper());
        chip->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:transparent;"
                    "border:1px solid %1;padding:2px 8px;letter-spacing:0.5px;"
                    "border-radius:10px;%2")
                .arg(cc, M(9)));
        chip->setFixedHeight(18);

        r1h->addWidget(av);
        r1h->addLayout(author_col, 1);
        r1h->addWidget(chip);
        cl->addWidget(r1);

        // ── Row 2: title ──────────────────────────────────────────────────────
        auto* title = new QLabel(p.title);
        title->setWordWrap(true);
        title->setStyleSheet(
            active
                ? QString("color:%1;font-size:15px;font-weight:700;"
                          "background:transparent;line-height:1.3;%2")
                      .arg(ui::colors::TEXT_PRIMARY(), M(15))
                : QString("color:#c8c8c8;font-size:15px;font-weight:600;"
                          "background:transparent;line-height:1.3;%1")
                      .arg(M(15)));
        cl->addWidget(title);

        // ── Row 3: body preview ───────────────────────────────────────────────
        if (!p.content.isEmpty()) {
            QString preview = p.content.left(180).replace('\n', ' ');
            if (p.content.length() > 180)
                preview += "...";
            auto* body = new QLabel(preview);
            body->setWordWrap(true);
            body->setMaximumHeight(40);
            body->setStyleSheet(
                QString("color:%1;font-size:12px;background:transparent;"
                        "line-height:1.5;%2")
                    .arg(ui::colors::TEXT_SECONDARY(), M(12)));
            cl->addWidget(body);
        }

        // ── Row 4: engagement bar ─────────────────────────────────────────────
        auto* eng = new QWidget;
        eng->setStyleSheet("background:transparent;");
        auto* eng_hl = new QHBoxLayout(eng);
        eng_hl->setContentsMargins(0, 4, 0, 0);
        eng_hl->setSpacing(16);

        // Upvote button (clickable, not just a label)
        auto* up_btn = new QPushButton(
            QString("▲ %1").arg(fmt_n(p.likes)));
        up_btn->setFixedHeight(22);
        up_btn->setCursor(Qt::PointingHandCursor);
        QString like_col = voted_up || p.likes > 0 ? ui::colors::AMBER() : ui::colors::TEXT_DIM();
        up_btn->setStyleSheet(
            voted_up
                ? QString("QPushButton{background:rgba(217,119,6,0.12);"
                          "color:%1;border:1px solid rgba(217,119,6,0.3);"
                          "font-size:10px;font-weight:700;padding:0 8px;"
                          "border-radius:11px;%2}"
                          "QPushButton:hover{background:rgba(217,119,6,0.2);}")
                      .arg(ui::colors::AMBER(), M(10))
                : QString("QPushButton{background:transparent;"
                          "color:%1;border:1px solid transparent;"
                          "font-size:10px;padding:0 8px;"
                          "border-radius:11px;%2}"
                          "QPushButton:hover{color:%3;"
                          "background:rgba(217,119,6,0.06);"
                          "border-color:rgba(217,119,6,0.2);}")
                      .arg(like_col, M(10), ui::colors::AMBER()));
        auto post_uuid = p.post_uuid;
        connect(up_btn, &QPushButton::clicked, this,
                [this, post_uuid]() {
                    emit vote_post_requested(post_uuid, "up");
                });

        auto mk_eng = [&](const QString& icon, const QString& val,
                          const QString& col) {
            auto* lbl = new QLabel(icon + " " + val);
            lbl->setStyleSheet(
                QString("color:%1;font-size:10px;background:transparent;%2")
                    .arg(col, M(10)));
            return lbl;
        };

        QString rep_col = p.reply_count > 0 ? ui::colors::CYAN() : QString(ui::colors::BORDER_DIM());

        eng_hl->addWidget(up_btn);
        eng_hl->addWidget(mk_eng("◆", QString("%1 replies").arg(p.reply_count), rep_col));
        eng_hl->addWidget(mk_eng("◉", fmt_n(p.views) + " views", QString(ui::colors::BORDER_DIM())));
        eng_hl->addStretch();

        // Badges
        if (voted_up) {
            auto* voted = new QLabel("✓ VOTED");
            voted->setStyleSheet(
                QString("color:%1;font-size:9px;font-weight:700;"
                        "background:rgba(217,119,6,0.08);padding:2px 6px;"
                        "border-radius:8px;%2")
                    .arg(ui::colors::AMBER(), M(9)));
            eng_hl->addWidget(voted);
        }
        if (p.reply_count > 5) {
            auto* hot = new QLabel("● HOT");
            hot->setStyleSheet(
                QString("color:%1;font-size:9px;font-weight:700;"
                        "background:rgba(220,38,38,0.08);padding:2px 6px;"
                        "border-radius:8px;%2")
                    .arg(ui::colors::NEGATIVE(), M(9)));
            eng_hl->addWidget(hot);
        } else if (p.reply_count > 2) {
            auto* active_badge = new QLabel("● ACTIVE");
            active_badge->setStyleSheet(
                QString("color:%1;font-size:9px;font-weight:700;"
                        "background:rgba(16,185,129,0.08);padding:2px 6px;"
                        "border-radius:8px;%2")
                    .arg(ui::colors::POSITIVE(), M(9)));
            eng_hl->addWidget(active_badge);
        }

        cl->addWidget(eng);

        // ── Click overlay — lower behind engagement buttons ───────────────────
        auto* click_btn = new QPushButton(card);
        click_btn->setGeometry(0, 0, 5000, 400);
        click_btn->setFlat(true);
        click_btn->setStyleSheet("QPushButton{background:transparent;border:none;}");
        click_btn->lower(); // lower so upvote button stays clickable on top
        auto cp = p;
        connect(click_btn, &QPushButton::clicked, this, [this, cp]() {
            active_uuid_ = cp.post_uuid;
            emit post_selected(cp);
        });

        gvl->addWidget(card);
    }

    posts_vl_->addWidget(grid);

    // ── Load more button ──────────────────────────────────────────────────────
    if (page_.page < page_.pages) {
        int remaining = page_.total - page_.posts.size();
        auto* more = new QPushButton(
            QString("Load %1 more posts").arg(remaining));
        more->setFixedHeight(36);
        more->setCursor(Qt::PointingHandCursor);
        more->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;"
                    "border:1px solid %3;font-size:12px;font-weight:600;"
                    "margin:8px 16px;border-radius:4px;%4}"
                    "QPushButton:hover{color:%5;border-color:%6;"
                    "background:rgba(217,119,6,0.05);}")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(),
                     ui::colors::BORDER_DIM(), M(12),
                     ui::colors::AMBER(), ui::colors::BORDER_MED()));
        int np = page_.page + 1;
        connect(more, &QPushButton::clicked, this,
                [this, np]() { emit load_more_requested(np); });
        posts_vl_->addWidget(more);
    }

    posts_container_->setUpdatesEnabled(true);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Skeleton loading
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::show_skeleton() {
    if (skeleton_w_) {
        skeleton_w_->deleteLater();
        skeleton_w_ = nullptr;
    }
    skeleton_w_ = new QWidget(posts_container_);
    auto* vl = new QVBoxLayout(skeleton_w_);
    vl->setContentsMargins(16, 10, 16, 10);
    vl->setSpacing(8);

    for (int i = 0; i < 6; ++i) {
        auto* card = new QWidget;
        card->setStyleSheet(
            QString("background:%1;border:1px solid %2;border-radius:4px;")
                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(16, 14, 16, 12);
        cvl->setSpacing(10);

        // Avatar + name row
        auto* r1 = new QWidget;
        r1->setStyleSheet("background:transparent;");
        auto* r1h = new QHBoxLayout(r1);
        r1h->setContentsMargins(0, 0, 0, 0);
        r1h->setSpacing(10);
        auto* av_skel = new QWidget;
        av_skel->setFixedSize(28, 28);
        av_skel->setStyleSheet(
            QString("background:%1;border-radius:14px;")
                .arg(ui::colors::BORDER_DIM()));
        auto* name_skel = new QWidget;
        name_skel->setFixedSize(100 + i * 15, 10);
        name_skel->setStyleSheet(
            QString("background:%1;border-radius:5px;")
                .arg(ui::colors::BORDER_DIM()));
        r1h->addWidget(av_skel);
        r1h->addWidget(name_skel);
        r1h->addStretch();
        cvl->addWidget(r1);

        // Title bar
        auto* title_skel = new QWidget;
        title_skel->setFixedHeight(12);
        title_skel->setMaximumWidth(300 + i * 30);
        title_skel->setStyleSheet(
            QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                    "stop:0 %1,stop:%2 %3,stop:1 %1);border-radius:6px;")
                .arg(ui::colors::BORDER_DIM())
                .arg(0.3 + (i % 4) * 0.15)
                .arg(ui::colors::BORDER_MED()));
        cvl->addWidget(title_skel);

        // Body bar
        auto* body_skel = new QWidget;
        body_skel->setFixedHeight(8);
        body_skel->setMaximumWidth(200 + i * 20);
        body_skel->setStyleSheet(
            QString("background:%1;border-radius:4px;")
                .arg(ui::colors::BORDER_DIM()));
        cvl->addWidget(body_skel);

        vl->addWidget(card);
    }

    posts_vl_->insertWidget(0, skeleton_w_);
    skeleton_timer_->start();
}

void ForumFeedPanel::pulse_skeleton() {
    if (!skeleton_w_)
        return;
    static int off = 0;
    off = (off + 12) % 100;
    double s = off / 100.0, e = std::min(1.0, s + 0.3);

    QString bg = QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                         "stop:0 %1,stop:%2 %3,stop:%4 %1);border-radius:6px;")
                     .arg(ui::colors::BORDER_DIM())
                     .arg(s, 0, 'f', 2)
                     .arg(ui::colors::BORDER_MED())
                     .arg(e, 0, 'f', 2);

    auto* vl = qobject_cast<QVBoxLayout*>(skeleton_w_->layout());
    if (!vl)
        return;
    for (int i = 0; i < vl->count(); ++i) {
        auto* ci = vl->itemAt(i);
        if (!ci || !ci->widget())
            continue;
        auto* cvl = qobject_cast<QVBoxLayout*>(ci->widget()->layout());
        if (!cvl)
            continue;
        for (int j = 0; j < cvl->count(); ++j) {
            auto* bi = cvl->itemAt(j);
            if (bi && bi->widget() && bi->widget()->maximumWidth() < 2000 &&
                bi->widget()->height() >= 8 && bi->widget()->height() <= 14)
                bi->widget()->setStyleSheet(bg);
        }
    }
}

} // namespace fincept::screens
