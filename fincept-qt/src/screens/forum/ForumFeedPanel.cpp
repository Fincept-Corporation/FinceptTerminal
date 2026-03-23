// src/screens/forum/ForumFeedPanel.cpp
#include "screens/forum/ForumFeedPanel.h"
#include "ui/theme/Theme.h"

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
    if (iso.isEmpty()) return {};
    auto dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) dt = QDateTime::fromString(iso.left(19), "yyyy-MM-ddTHH:mm:ss");
    if (!dt.isValid()) return iso.left(10);
    qint64 sec = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (sec < 0) sec = 0;
    if (sec < 60)    return QString("%1s").arg(sec);
    if (sec < 3600)  return QString("%1m").arg(sec / 60);
    if (sec < 86400) return QString("%1h").arg(sec / 3600);
    return QString("%1d").arg(sec / 86400);
}

static QString det_color(const QString& s) {
    static const char* pal[] = {
        "#d97706","#06b6d4","#10b981","#8b5cf6","#f97316",
        "#3b82f6","#ec4899","#14b8a6","#84cc16","#ef4444"
    };
    return pal[qHash(s) % 10];
}

static QString fmt_n(int n) {
    if (n >= 10000) return QString("%1K").arg(n / 1000);
    if (n >= 1000)  return QString("%1.%2K").arg(n / 1000).arg((n % 1000) / 100);
    return QString::number(n);
}

// ── Section header helper ─────────────────────────────────────────────────────
static QWidget* make_section_hdr(const QString& title, const QString& accent = "#d97706") {
    auto* w = new QWidget;
    w->setFixedHeight(24);
    w->setStyleSheet("background:#060606;border-bottom:1px solid #0e0e0e;");
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(6);
    auto* dot = new QLabel("■");
    dot->setStyleSheet(QString("color:%1;font-size:6px;background:transparent;").arg(accent));
    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        QString("color:#707070;font-size:9px;font-weight:700;letter-spacing:1.5px;"
                "background:transparent;%1").arg(M(9)));
    hl->addWidget(dot);
    hl->addWidget(lbl);
    hl->addStretch();
    return w;
}

static QFrame* thin_line() {
    auto* f = new QFrame;
    f->setFixedHeight(1);
    f->setStyleSheet("background:#0e0e0e;border:none;");
    return f;
}

ForumFeedPanel::ForumFeedPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ForumFeedPanel::build_ui() {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet(
        "QScrollArea{border:none;background:#080808;}"
        "QScrollBar:vertical{width:4px;background:transparent;margin:0;}"
        "QScrollBar::handle:vertical{background:#151515;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    content_w_ = new QWidget;
    content_w_->setStyleSheet("background:#080808;");
    content_vl_ = new QVBoxLayout(content_w_);
    content_vl_->setContentsMargins(0, 0, 0, 0);
    content_vl_->setSpacing(0);

    // ── 1. Stats bar (rebuilt by set_stats) ───────────────────────────────────
    stats_section_ = new QWidget;
    stats_section_->setFixedHeight(0);
    content_vl_->addWidget(stats_section_);

    // ── 2. Two-column: profile + contributors ─────────────────────────────────
    auto* mid_row = new QWidget;
    mid_row->setStyleSheet("background:#080808;");
    auto* mid_hl = new QHBoxLayout(mid_row);
    mid_hl->setContentsMargins(0, 0, 0, 0);
    mid_hl->setSpacing(0);

    profile_section_ = new QWidget;
    profile_section_->setStyleSheet("background:#080808;border-right:1px solid #0e0e0e;");
    mid_hl->addWidget(profile_section_, 1);

    contrib_section_ = new QWidget;
    contrib_section_->setStyleSheet("background:#080808;");
    mid_hl->addWidget(contrib_section_, 1);
    content_vl_->addWidget(mid_row);

    // ── 3. Category cards ─────────────────────────────────────────────────────
    cats_section_ = new QWidget;
    cats_section_->setFixedHeight(0);
    content_vl_->addWidget(cats_section_);

    // ── 4. Posts header ───────────────────────────────────────────────────────
    auto* posts_hdr = new QWidget;
    posts_hdr->setFixedHeight(28);
    posts_hdr->setStyleSheet("background:#060606;border-bottom:1px solid #0e0e0e;"
                             "border-top:1px solid #0e0e0e;");
    auto* ph_hl = new QHBoxLayout(posts_hdr);
    ph_hl->setContentsMargins(16, 0, 16, 0);
    ph_hl->setSpacing(8);

    auto* ph_dot = new QLabel("■");
    ph_dot->setStyleSheet("color:#d97706;font-size:6px;background:transparent;");
    posts_header_lbl_ = new QLabel("LATEST POSTS");
    posts_header_lbl_->setStyleSheet(
        QString("color:#707070;font-size:9px;font-weight:700;letter-spacing:1.5px;"
                "background:transparent;%1").arg(M(9)));
    posts_count_lbl_ = new QLabel;
    posts_count_lbl_->setStyleSheet(
        QString("color:#555555;font-size:9px;background:transparent;%1").arg(M(9)));

    ph_hl->addWidget(ph_dot);
    ph_hl->addWidget(posts_header_lbl_);
    ph_hl->addStretch();
    ph_hl->addWidget(posts_count_lbl_);
    content_vl_->addWidget(posts_hdr);

    // ── 5. Posts container ────────────────────────────────────────────────────
    posts_container_ = new QWidget;
    posts_container_->setStyleSheet("background:#080808;");
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
// Dashboard data setters
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ForumFeedPanel::set_stats(const services::ForumStats& stats) {
    stats_ = stats;
    stats_loaded_ = true;
    rebuild_stats();
    rebuild_contributors();
}

void ForumFeedPanel::set_profile(const services::ForumProfile& profile) {
    profile_ = profile;
    profile_loaded_ = true;
    rebuild_profile();
}

void ForumFeedPanel::set_categories(const QVector<services::ForumCategory>& cats, int active_id) {
    categories_ = cats;
    active_cat_id_ = active_id;
    cats_loaded_ = true;
    rebuild_categories();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Stats bar — 5 metric cells in a row
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::rebuild_stats() {
    // Clear old
    delete stats_section_->layout();
    auto children = stats_section_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* c : children) c->deleteLater();
    stats_section_->setFixedHeight(QWIDGETSIZE_MAX);

    auto* vl = new QVBoxLayout(stats_section_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(make_section_hdr("COMMUNITY OVERVIEW", "#d97706"));

    auto* row = new QWidget;
    row->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #0e0e0e;");
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    struct StatDef { QString label; QString value; QString color; };
    StatDef defs[] = {
        {"CATEGORIES", fmt_n(stats_.total_categories), "#555555"},
        {"POSTS",      fmt_n(stats_.total_posts),      "#e5e5e5"},
        {"COMMENTS",   fmt_n(stats_.total_comments),   "#06b6d4"},
        {"VOTES",      fmt_n(stats_.total_votes),       "#10b981"},
        {"24H ACTIVE", fmt_n(stats_.recent_posts_24h),  "#d97706"},
    };

    for (int i = 0; i < 5; ++i) {
        auto* cell = new QWidget;
        cell->setStyleSheet(
            i < 4 ? "background:transparent;border-right:1px solid #0e0e0e;"
                  : "background:transparent;");
        auto* cvl = new QVBoxLayout(cell);
        cvl->setContentsMargins(16, 10, 16, 10);
        cvl->setSpacing(2);
        auto* val_lbl = new QLabel(defs[i].value);
        val_lbl->setStyleSheet(
            QString("color:%1;font-size:16px;font-weight:700;background:transparent;%2")
                .arg(defs[i].color, M(16)));
        auto* lbl = new QLabel(defs[i].label);
        lbl->setStyleSheet(
            QString("color:#606060;font-size:8px;letter-spacing:1px;background:transparent;%1")
                .arg(M(8)));
        cvl->addWidget(val_lbl);
        cvl->addWidget(lbl);
        hl->addWidget(cell, 1);
    }
    vl->addWidget(row);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Profile card
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::rebuild_profile() {
    delete profile_section_->layout();
    auto children = profile_section_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* c : children) c->deleteLater();

    auto* vl = new QVBoxLayout(profile_section_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(make_section_hdr("MY PROFILE", "#06b6d4"));

    auto* card = new QWidget;
    card->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #0e0e0e;");
    auto* card_vl = new QVBoxLayout(card);
    card_vl->setContentsMargins(16, 10, 16, 10);
    card_vl->setSpacing(6);

    // Avatar + name row
    auto* top = new QWidget;
    top->setStyleSheet("background:transparent;");
    auto* top_hl = new QHBoxLayout(top);
    top_hl->setContentsMargins(0, 0, 0, 0);
    top_hl->setSpacing(10);

    QString av_col = profile_.avatar_color.isEmpty() ? "#d97706" : profile_.avatar_color;
    auto* avatar = new QLabel(profile_.display_name.left(2).toUpper());
    avatar->setFixedSize(32, 32);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        QString("color:#080808;font-size:12px;font-weight:700;background:%1;%2")
            .arg(av_col, M(12)));

    auto* name_col = new QVBoxLayout;
    name_col->setSpacing(1);
    name_col->setContentsMargins(0, 0, 0, 0);
    auto* name_lbl = new QLabel(profile_.display_name.toUpper());
    name_lbl->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;background:transparent;%2")
            .arg(av_col, M(12)));
    auto* handle = new QLabel("@" + profile_.username);
    handle->setStyleSheet(
        QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));
    name_col->addWidget(name_lbl);
    name_col->addWidget(handle);

    top_hl->addWidget(avatar);
    top_hl->addLayout(name_col, 1);
    card_vl->addWidget(top);

    // Bio
    if (!profile_.bio.isEmpty()) {
        auto* bio = new QLabel(profile_.bio);
        bio->setWordWrap(true);
        bio->setStyleSheet(
            QString("color:#444444;font-size:11px;font-style:italic;"
                    "background:transparent;%1").arg(M(11)));
        card_vl->addWidget(bio);
    }

    // Stats row
    auto* stat_row = new QWidget;
    stat_row->setStyleSheet("background:transparent;");
    auto* sr_hl = new QHBoxLayout(stat_row);
    sr_hl->setContentsMargins(0, 4, 0, 0);
    sr_hl->setSpacing(14);

    auto mk_ps = [&](const QString& lbl, int val, const QString& col) {
        auto* w = new QWidget;
        w->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(w);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);
        auto* v = new QLabel(QString::number(val));
        v->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;background:transparent;%2")
                .arg(col, M(11)));
        auto* l = new QLabel(lbl);
        l->setStyleSheet(
            QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));
        hl->addWidget(v);
        hl->addWidget(l);
        return w;
    };

    sr_hl->addWidget(mk_ps("REP", profile_.reputation, "#d97706"));
    sr_hl->addWidget(mk_ps("posts", profile_.posts_count, "#888888"));
    sr_hl->addWidget(mk_ps("comments", profile_.comments_count, "#888888"));
    sr_hl->addWidget(mk_ps("likes", profile_.likes_given, "#10b981"));
    sr_hl->addStretch();
    card_vl->addWidget(stat_row);

    // Signature
    if (!profile_.signature.isEmpty()) {
        auto* sig = new QLabel("\"" + profile_.signature + "\"");
        sig->setStyleSheet(
            QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));
        card_vl->addWidget(sig);
    }

    vl->addWidget(card);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Top contributors
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::rebuild_contributors() {
    delete contrib_section_->layout();
    auto children = contrib_section_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* c : children) c->deleteLater();

    auto* vl = new QVBoxLayout(contrib_section_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(make_section_hdr("TOP CONTRIBUTORS", "#10b981"));

    static const char* rank_col[] = {"#d97706", "#aaaaaa", "#cd7f32", "#555555", "#555555"};

    for (int i = 0; i < std::min(5, (int)stats_.top_contributors.size()); ++i) {
        const auto& c = stats_.top_contributors[i];
        auto* row = new QWidget;
        row->setFixedHeight(28);
        row->setStyleSheet(
            i % 2 == 0 ? "background:#0a0a0a;border-bottom:1px solid #0d0d0d;"
                       : "background:#090909;border-bottom:1px solid #0d0d0d;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(16, 0, 16, 0);
        hl->setSpacing(8);

        // Rank
        auto* rank_lbl = new QLabel(QString("#%1").arg(i + 1));
        rank_lbl->setFixedWidth(22);
        rank_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;background:transparent;%2")
                .arg(rank_col[i], M(10)));

        // Avatar
        auto* av = new QLabel(c.display_name.left(2).toUpper());
        av->setFixedSize(18, 18);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet(
            QString("color:#080808;font-size:8px;font-weight:700;background:%1;%2")
                .arg(det_color(c.display_name), M(8)));

        // Name
        auto* name = new QLabel(c.display_name.left(16));
        name->setStyleSheet(
            QString("color:#666666;font-size:11px;background:transparent;%1").arg(M(11)));

        // Rep + posts
        auto* rep = new QLabel(QString("▲ %1").arg(c.reputation));
        rep->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(rank_col[i], M(10)));

        auto* posts = new QLabel(QString("%1 posts").arg(c.posts_count));
        posts->setStyleSheet(
            QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));

        hl->addWidget(rank_lbl);
        hl->addWidget(av);
        hl->addWidget(name, 1);
        hl->addWidget(posts);
        hl->addSpacing(6);
        hl->addWidget(rep);
        vl->addWidget(row);
    }

    if (stats_.top_contributors.isEmpty()) {
        auto* empty = new QLabel("  No contributors yet");
        empty->setFixedHeight(28);
        empty->setStyleSheet(
            QString("color:#1a1a1a;font-size:10px;background:#0a0a0a;%1").arg(M(10)));
        vl->addWidget(empty);
    }

    vl->addStretch();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Category cards — horizontal row
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumFeedPanel::rebuild_categories() {
    delete cats_section_->layout();
    auto children = cats_section_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* c : children) c->deleteLater();
    cats_section_->setFixedHeight(QWIDGETSIZE_MAX);

    auto* vl = new QVBoxLayout(cats_section_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(make_section_hdr("CHANNELS", "#8b5cf6"));

    auto* row = new QWidget;
    row->setStyleSheet("background:#080808;border-bottom:1px solid #0e0e0e;");
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(6);

    for (const auto& cat : categories_) {
        const bool active = (cat.id == active_cat_id_);
        QString cc = cat.color.isEmpty() ? "#d97706" : cat.color;

        auto* card = new QWidget;
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(
            active
                ? QString("background:#111111;border:none;border-bottom:2px solid %1;").arg(cc)
                : "background:#0a0a0a;border:none;border-bottom:2px solid transparent;");
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(12, 8, 12, 8);
        cvl->setSpacing(3);

        // Name + count
        auto* name_row = new QWidget;
        name_row->setStyleSheet("background:transparent;");
        auto* nr_hl = new QHBoxLayout(name_row);
        nr_hl->setContentsMargins(0, 0, 0, 0);
        nr_hl->setSpacing(6);

        auto* dot = new QLabel("●");
        dot->setStyleSheet(
            QString("color:%1;font-size:8px;background:transparent;").arg(cc));
        auto* nm = new QLabel(cat.name.toUpper());
        nm->setStyleSheet(
            active
                ? QString("color:#e5e5e5;font-size:11px;font-weight:700;background:transparent;%1")
                      .arg(M(11))
                : QString("color:#888888;font-size:11px;font-weight:600;background:transparent;%1")
                      .arg(M(11)));
        auto* cnt = new QLabel(QString::number(cat.post_count));
        cnt->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;background:transparent;%2")
                .arg(cc, M(10)));

        nr_hl->addWidget(dot);
        nr_hl->addWidget(nm);
        nr_hl->addStretch();
        nr_hl->addWidget(cnt);
        cvl->addWidget(name_row);

        // Description
        if (!cat.description.isEmpty()) {
            auto* desc = new QLabel(cat.description);
            desc->setWordWrap(true);
            desc->setStyleSheet(
                QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));
            desc->setMaximumHeight(28);
            cvl->addWidget(desc);
        }

        // Click handler
        auto* click = new QPushButton(card);
        click->setGeometry(0, 0, 4000, 200);
        click->setFlat(true);
        click->setStyleSheet("QPushButton{background:transparent;border:none;}");
        click->raise();
        auto cat_copy = cat;
        connect(click, &QPushButton::clicked, this, [this, cat_copy]() {
            emit category_clicked(cat_copy.id, cat_copy.name, cat_copy.color);
        });

        hl->addWidget(card, 1);
    }

    vl->addWidget(row);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Posts
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ForumFeedPanel::set_header(const QString& text) {
    posts_header_lbl_->setText(text.toUpper());
}

void ForumFeedPanel::set_loading(bool on) {
    if (on) show_skeleton();
    else {
        skeleton_timer_->stop();
        if (skeleton_w_) { skeleton_w_->deleteLater(); skeleton_w_ = nullptr; }
    }
}

void ForumFeedPanel::show_skeleton() {
    if (skeleton_w_) { skeleton_w_->deleteLater(); skeleton_w_ = nullptr; }
    skeleton_w_ = new QWidget(posts_container_);
    auto* vl = new QVBoxLayout(skeleton_w_);
    vl->setContentsMargins(12, 6, 12, 6);
    vl->setSpacing(6);
    for (int i = 0; i < 5; ++i) {
        auto* card = new QWidget;
        card->setFixedHeight(90);
        card->setStyleSheet("background:#0a0a0a;border:1px solid #0d0d0d;");
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(16, 12, 16, 12);
        cvl->setSpacing(8);
        for (int j = 0; j < 3; ++j) {
            auto* bar = new QWidget;
            int w[] = {130, 300 + i * 25, 160};
            bar->setFixedHeight(j == 1 ? 11 : 7);
            bar->setMaximumWidth(w[j]);
            bar->setStyleSheet(
                QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                        "stop:0 #0e0e0e,stop:%1 #181818,stop:1 #0e0e0e);")
                    .arg(0.3 + (i % 4) * 0.15));
            cvl->addWidget(bar);
        }
        cvl->addStretch();
        vl->addWidget(card);
    }
    posts_vl_->insertWidget(0, skeleton_w_);
    skeleton_timer_->start();
}

void ForumFeedPanel::pulse_skeleton() {
    if (!skeleton_w_) return;
    static int off = 0;
    off = (off + 12) % 100;
    double s = off / 100.0, e = std::min(1.0, s + 0.28);
    QString bg = QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                         "stop:0 #090909,stop:%1 #1d1d1d,stop:%2 #090909);")
                     .arg(s, 0, 'f', 2).arg(e, 0, 'f', 2);
    auto* vl = qobject_cast<QVBoxLayout*>(skeleton_w_->layout());
    if (!vl) return;
    for (int i = 0; i < vl->count(); ++i) {
        auto* ci = vl->itemAt(i);
        if (!ci || !ci->widget()) continue;
        auto* cvl = qobject_cast<QVBoxLayout*>(ci->widget()->layout());
        if (!cvl) continue;
        for (int j = 0; j < cvl->count(); ++j) {
            auto* bi = cvl->itemAt(j);
            if (bi && bi->widget() && bi->widget()->height() <= 14)
                bi->widget()->setStyleSheet(bg);
        }
    }
}

void ForumFeedPanel::clear_active() { active_uuid_.clear(); }
void ForumFeedPanel::set_active_post(const QString& uuid) { active_uuid_ = uuid; }

void ForumFeedPanel::set_posts(const services::ForumPostsPage& pg, const QString& cc) {
    page_ = pg;
    cat_color_ = cc;
    skeleton_timer_->stop();
    if (skeleton_w_) { skeleton_w_->deleteLater(); skeleton_w_ = nullptr; }
    rebuild_posts();
}

void ForumFeedPanel::rebuild_posts() {
    posts_container_->setUpdatesEnabled(false);
    while (posts_vl_->count() > 0) {
        auto* item = posts_vl_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    posts_count_lbl_->setText(QString("%1 posts · page %2/%3")
        .arg(page_.total).arg(page_.page).arg(std::max(1, page_.pages)));

    if (page_.posts.isEmpty()) {
        auto* empty = new QWidget;
        empty->setMinimumHeight(100);
        empty->setStyleSheet("background:transparent;");
        auto* evl = new QVBoxLayout(empty);
        auto* lbl = new QLabel("No posts in this category yet. Be the first!");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(
            QString("color:#555555;font-size:12px;background:transparent;%1").arg(M(12)));
        evl->addStretch();
        evl->addWidget(lbl);
        evl->addStretch();
        posts_vl_->addWidget(empty);
        posts_container_->setUpdatesEnabled(true);
        return;
    }

    // Grid: 12px padding on sides
    auto* grid = new QWidget;
    grid->setStyleSheet("background:transparent;");
    auto* gvl = new QVBoxLayout(grid);
    gvl->setContentsMargins(12, 6, 12, 6);
    gvl->setSpacing(6);

    for (int i = 0; i < page_.posts.size(); ++i) {
        const auto& p = page_.posts[i];
        const bool active = (p.post_uuid == active_uuid_);
        QString cc = p.category_color.isEmpty() ? det_color(p.category_name) : p.category_color;
        bool voted_up = (p.user_vote == "up");

        auto* card = new QWidget;
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(
            active
                ? QString("background:#0c0c0c;border:1px solid #181818;"
                          "border-left:3px solid %1;").arg(cc)
                : "background:#0a0a0a;border:1px solid #0e0e0e;"
                  "border-left:3px solid #0e0e0e;");

        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(14, 10, 14, 8);
        cl->setSpacing(5);

        // ── Row 1: avatar · author · category · time ─────────────────────────
        auto* r1 = new QWidget;
        r1->setStyleSheet("background:transparent;");
        auto* r1h = new QHBoxLayout(r1);
        r1h->setContentsMargins(0, 0, 0, 0);
        r1h->setSpacing(7);

        auto* av = new QLabel(p.author_display_name.left(2).toUpper());
        av->setFixedSize(22, 22);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet(
            QString("color:#080808;font-size:9px;font-weight:700;background:%1;%2")
                .arg(det_color(p.author_display_name), M(9)));

        auto* au = new QLabel(p.author_display_name.left(20));
        au->setStyleSheet(
            QString("color:#555555;font-size:11px;font-weight:600;background:transparent;%1")
                .arg(M(11)));

        auto* chip = new QLabel(p.category_name.toUpper().left(14));
        chip->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:transparent;"
                    "border:1px solid %1;padding:0px 5px;letter-spacing:0.3px;%2")
                .arg(cc, M(9)));

        auto* tm = new QLabel(rel_time(p.created_at));
        tm->setStyleSheet(
            QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));

        r1h->addWidget(av);
        r1h->addWidget(au);
        r1h->addSpacing(4);
        r1h->addWidget(chip);
        r1h->addStretch();
        r1h->addWidget(tm);
        cl->addWidget(r1);

        // ── Row 2: title ─────────────────────────────────────────────────────
        auto* title = new QLabel(p.title);
        title->setWordWrap(true);
        title->setStyleSheet(
            active
                ? QString("color:#f0f0f0;font-size:14px;font-weight:700;"
                          "background:transparent;%1").arg(M(14))
                : QString("color:#c0c0c0;font-size:14px;font-weight:600;"
                          "background:transparent;%1").arg(M(14)));
        cl->addWidget(title);

        // ── Row 3: body preview ──────────────────────────────────────────────
        if (!p.content.isEmpty()) {
            QString preview = p.content.left(160).replace('\n', ' ');
            if (p.content.length() > 160) preview += "...";
            auto* body_lbl = new QLabel(preview);
            body_lbl->setWordWrap(true);
            body_lbl->setMaximumHeight(36);
            body_lbl->setStyleSheet(
                QString("color:#777777;font-size:12px;background:transparent;"
                        "line-height:1.4;%1").arg(M(12)));
            cl->addWidget(body_lbl);
        }

        // ── Row 4: engagement ────────────────────────────────────────────────
        auto* eng = new QWidget;
        eng->setStyleSheet("background:transparent;");
        auto* eng_hl = new QHBoxLayout(eng);
        eng_hl->setContentsMargins(0, 2, 0, 0);
        eng_hl->setSpacing(14);

        auto mk_eng = [&](const QString& icon, const QString& val, const QString& col) {
            auto* w = new QWidget;
            w->setStyleSheet("background:transparent;");
            auto* hl = new QHBoxLayout(w);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(3);
            auto* ic = new QLabel(icon);
            ic->setStyleSheet(
                QString("color:%1;font-size:10px;background:transparent;%2").arg(col, M(10)));
            auto* v = new QLabel(val);
            v->setStyleSheet(
                QString("color:#666666;font-size:10px;background:transparent;%1").arg(M(10)));
            hl->addWidget(ic);
            hl->addWidget(v);
            return w;
        };

        QString like_col = voted_up ? "#d97706" : (p.likes > 0 ? "#d97706" : "#1e1e1e");
        eng_hl->addWidget(mk_eng("▲", fmt_n(p.likes), like_col));
        eng_hl->addWidget(mk_eng("◆", QString("%1 replies").arg(p.reply_count),
                                  p.reply_count > 0 ? "#06b6d4" : "#1e1e1e"));
        eng_hl->addWidget(mk_eng("◉", fmt_n(p.views) + " views", "#1e1e1e"));
        eng_hl->addStretch();

        if (voted_up) {
            auto* voted_badge = new QLabel("✓ VOTED");
            voted_badge->setStyleSheet(
                QString("color:#d97706;font-size:9px;font-weight:700;"
                        "background:transparent;%1").arg(M(9)));
            eng_hl->addWidget(voted_badge);
        }
        if (p.reply_count > 3) {
            auto* hot = new QLabel("● ACTIVE");
            hot->setStyleSheet(
                QString("color:#ef4444;font-size:9px;font-weight:700;"
                        "background:transparent;letter-spacing:0.5px;%1").arg(M(9)));
            eng_hl->addWidget(hot);
        }

        cl->addWidget(eng);

        // Click overlay
        auto* click_btn = new QPushButton(card);
        click_btn->setGeometry(0, 0, 5000, 400);
        click_btn->setFlat(true);
        click_btn->setStyleSheet("QPushButton{background:transparent;border:none;}");
        click_btn->raise();
        auto cp = p;
        connect(click_btn, &QPushButton::clicked, this, [this, cp]() {
            active_uuid_ = cp.post_uuid;
            emit post_selected(cp);
        });

        gvl->addWidget(card);
    }

    posts_vl_->addWidget(grid);

    // Load more
    if (page_.page < page_.pages) {
        auto* more = new QPushButton(
            QString("↓  LOAD MORE  ·  %1 remaining").arg(page_.total - page_.posts.size()));
        more->setFixedHeight(34);
        more->setCursor(Qt::PointingHandCursor);
        more->setStyleSheet(
            QString("QPushButton{background:#0a0a0a;color:#252525;border:1px solid #0e0e0e;"
                    "font-size:11px;font-weight:700;letter-spacing:0.5px;margin:0 12px;%1}"
                    "QPushButton:hover{color:#d97706;border-color:#1a1a1a;}").arg(M(11)));
        int np = page_.page + 1;
        connect(more, &QPushButton::clicked, this, [this, np]() {
            emit load_more_requested(np);
        });
        posts_vl_->addWidget(more);
    }

    posts_container_->setUpdatesEnabled(true);
}

} // namespace fincept::screens
