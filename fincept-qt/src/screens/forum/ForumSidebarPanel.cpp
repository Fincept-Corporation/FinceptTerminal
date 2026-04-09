// src/screens/forum/ForumSidebarPanel.cpp
#include "screens/forum/ForumSidebarPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>

namespace fincept::screens {

static QString M(int sz = 12) {
    return QString("font-family:'Consolas','Courier New',monospace;font-size:%1px;").arg(sz);
}

static QString det_color(const QString& s) {
    static const char* pal[] = {"#d97706", "#06b6d4", "#10b981", "#8b5cf6", "#f97316",
                                "#3b82f6", "#ec4899", "#14b8a6", "#84cc16", "#ef4444"};
    return pal[qHash(s) % 10];
}

ForumSidebarPanel::ForumSidebarPanel(QWidget* parent) : QWidget(parent) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    });
    build_ui();
}

void ForumSidebarPanel::build_ui() {
    setFixedWidth(240);
    setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Brand header with gradient accent ─────────────────────────────────────
    auto* brand = new QWidget(this);
    brand->setFixedHeight(56);
    brand->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                                 "stop:0 %1,stop:1 %2);"
                                 "border-bottom:1px solid %3;")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* brand_vl = new QVBoxLayout(brand);
    brand_vl->setContentsMargins(16, 10, 16, 0);
    brand_vl->setSpacing(2);

    auto* brand_row = new QWidget(this);
    brand_row->setStyleSheet("background:transparent;");
    auto* brand_hl = new QHBoxLayout(brand_row);
    brand_hl->setContentsMargins(0, 0, 0, 0);
    brand_hl->setSpacing(10);

    auto* brand_icon = new QLabel("◈");
    brand_icon->setStyleSheet(QString("color:%1;font-size:20px;background:transparent;").arg(ui::colors::AMBER()));

    auto* brand_title = new QLabel("COMMUNITY");
    brand_title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:2px;"
                                       "background:transparent;%2")
                                   .arg(ui::colors::TEXT_PRIMARY(), M(13)));

    brand_hl->addWidget(brand_icon);
    brand_hl->addWidget(brand_title, 1);
    brand_vl->addWidget(brand_row);

    // Gradient accent line
    brand_accent_ = new QWidget(this);
    brand_accent_->setFixedHeight(2);
    brand_accent_->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                                         "stop:0 %1,stop:0.4 %2,stop:0.7 %3,stop:1 transparent);")
                                     .arg(ui::colors::AMBER(), ui::colors::ORANGE(), ui::colors::CYAN()));
    brand_vl->addWidget(brand_accent_);

    root->addWidget(brand);

    // ── My Profile card ───────────────────────────────────────────────────────
    auto* profile_card = new QWidget(this);
    profile_card->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* pc_hl = new QHBoxLayout(profile_card);
    pc_hl->setContentsMargins(14, 12, 14, 12);
    pc_hl->setSpacing(12);

    avatar_lbl_ = new QLabel("?");
    avatar_lbl_->setFixedSize(36, 36);
    avatar_lbl_->setAlignment(Qt::AlignCenter);
    avatar_lbl_->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;background:%2;"
                                       "border-radius:18px;%3")
                                   .arg(ui::colors::BG_BASE(), ui::colors::AMBER(), M(14)));

    auto* profile_info = new QVBoxLayout;
    profile_info->setSpacing(1);
    profile_info->setContentsMargins(0, 0, 0, 0);

    profile_name_lbl_ = new QLabel("Loading...");
    profile_name_lbl_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;%2")
                                         .arg(ui::colors::TEXT_PRIMARY(), M(12)));

    profile_sub_lbl_ = new QLabel("...");
    profile_sub_lbl_->setStyleSheet(
        QString("color:%1;font-size:10px;background:transparent;%2").arg(ui::colors::TEXT_TERTIARY(), M(10)));

    profile_rep_lbl_ = new QLabel;
    profile_rep_lbl_->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;background:transparent;%2").arg(ui::colors::AMBER(), M(10)));

    profile_info->addWidget(profile_name_lbl_);
    profile_info->addWidget(profile_sub_lbl_);

    pc_hl->addWidget(avatar_lbl_);
    pc_hl->addLayout(profile_info, 1);
    pc_hl->addWidget(profile_rep_lbl_);
    root->addWidget(profile_card);

    // ── Search bar ────────────────────────────────────────────────────────────
    auto* search_wrap = new QWidget(this);
    search_wrap->setFixedHeight(40);
    search_wrap->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* search_hl = new QHBoxLayout(search_wrap);
    search_hl->setContentsMargins(12, 6, 12, 6);
    search_hl->setSpacing(6);

    auto* search_icon = new QLabel("⌕");
    search_icon->setStyleSheet(
        QString("color:%1;font-size:14px;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
    search_icon->setFixedWidth(16);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search discussions...");
    search_input_->setStyleSheet(QString("QLineEdit{background:rgba(255,255,255,0.03);color:%1;"
                                         "border:1px solid %2;padding:3px 10px;"
                                         "font-size:11px;border-radius:3px;%3}"
                                         "QLineEdit:focus{border-color:%4;color:%5;"
                                         "background:rgba(255,255,255,0.05);}"
                                         "QLineEdit::placeholder{color:%6;}")
                                     .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), M(11),
                                          ui::colors::BORDER_BRIGHT(), ui::colors::TEXT_PRIMARY(),
                                          ui::colors::TEXT_DIM()));

    connect(search_input_, &QLineEdit::returnPressed, this,
            [this]() { emit search_requested(search_input_->text().trimmed()); });

    search_hl->addWidget(search_icon);
    search_hl->addWidget(search_input_, 1);
    root->addWidget(search_wrap);

    // ── Scrollable body ───────────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:%1;}"
                                  "QScrollBar:vertical{width:0px;}")
                              .arg(ui::colors::BG_BASE()));

    auto* body = new QWidget(this);
    body->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* body_vl = new QVBoxLayout(body);
    body_vl->setContentsMargins(0, 0, 0, 0);
    body_vl->setSpacing(0);

    // ── Quick stats row ───────────────────────────────────────────────────────
    auto* stats_hdr = new QWidget(this);
    stats_hdr->setFixedHeight(22);
    stats_hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* sh_hl = new QHBoxLayout(stats_hdr);
    sh_hl->setContentsMargins(14, 0, 14, 0);
    auto* sh_lbl = new QLabel("ACTIVITY");
    sh_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1.5px;"
                                  "background:transparent;%2")
                              .arg(ui::colors::TEXT_TERTIARY(), M(9)));
    sh_hl->addWidget(sh_lbl);
    sh_hl->addStretch();
    body_vl->addWidget(stats_hdr);

    auto* stats_row = new QWidget(this);
    stats_row->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* sr_hl = new QHBoxLayout(stats_row);
    sr_hl->setContentsMargins(0, 0, 0, 0);
    sr_hl->setSpacing(0);

    auto mk_mini_stat = [&](const QString& label, const QString& color, QLabel*& out) -> QWidget* {
        auto* cell = new QWidget(this);
        cell->setStyleSheet("background:transparent;");
        auto* cvl = new QVBoxLayout(cell);
        cvl->setContentsMargins(14, 8, 14, 8);
        cvl->setSpacing(1);
        out = new QLabel("--");
        out->setStyleSheet(
            QString("color:%1;font-size:16px;font-weight:700;background:transparent;%2").arg(color, M(16)));
        auto* l = new QLabel(label);
        l->setStyleSheet(QString("color:%1;font-size:8px;letter-spacing:0.8px;background:transparent;%2")
                             .arg(ui::colors::TEXT_DIM(), M(8)));
        cvl->addWidget(out);
        cvl->addWidget(l);
        return cell;
    };

    sr_hl->addWidget(mk_mini_stat("POSTS", ui::colors::TEXT_PRIMARY(), stat_posts_val_));
    sr_hl->addWidget(mk_mini_stat("REPLIES", ui::colors::CYAN(), stat_comments_val_));
    sr_hl->addWidget(mk_mini_stat("TODAY", ui::colors::AMBER(), stat_active_val_));
    body_vl->addWidget(stats_row);

    // ── Trending button ───────────────────────────────────────────────────────
    auto* trending_btn = new QPushButton;
    trending_btn->setFixedHeight(34);
    trending_btn->setCursor(Qt::PointingHandCursor);
    trending_btn->setText("  ▲  TRENDING POSTS");
    trending_btn->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.04);color:%1;border:none;"
                "border-bottom:1px solid %2;text-align:left;padding:0 14px;"
                "font-size:11px;font-weight:600;%3}"
                "QPushButton:hover{background:rgba(217,119,6,0.1);color:%4;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), M(11), ui::colors::AMBER()));
    connect(trending_btn, &QPushButton::clicked, this, &ForumSidebarPanel::trending_clicked);
    body_vl->addWidget(trending_btn);

    // ── Channels section ──────────────────────────────────────────────────────
    auto* chan_hdr = new QWidget(this);
    chan_hdr->setFixedHeight(26);
    chan_hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* ch_hl = new QHBoxLayout(chan_hdr);
    ch_hl->setContentsMargins(14, 0, 14, 0);
    ch_hl->setSpacing(6);
    auto* ch_dot = new QLabel("●");
    ch_dot->setStyleSheet(QString("color:%1;font-size:6px;background:transparent;").arg(ui::colors::AMBER()));
    auto* ch_lbl = new QLabel("CHANNELS");
    ch_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1.5px;"
                                  "background:transparent;%2")
                              .arg(ui::colors::TEXT_TERTIARY(), M(9)));

    auto* new_post_btn = new QPushButton("+");
    new_post_btn->setFixedSize(18, 18);
    new_post_btn->setCursor(Qt::PointingHandCursor);
    new_post_btn->setToolTip("New post");
    new_post_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;"
                                        "font-size:14px;font-weight:700;%2}"
                                        "QPushButton:hover{color:%3;}")
                                    .arg(ui::colors::TEXT_DIM(), M(14), ui::colors::AMBER()));
    connect(new_post_btn, &QPushButton::clicked, this, [this]() {
        int cat_id = active_category_id_ > 0 ? active_category_id_ : 1;
        emit new_post_requested(cat_id);
    });

    ch_hl->addWidget(ch_dot);
    ch_hl->addWidget(ch_lbl);
    ch_hl->addStretch();
    ch_hl->addWidget(new_post_btn);
    body_vl->addWidget(chan_hdr);

    cat_container_ = new QWidget(this);
    cat_container_->setStyleSheet("background:transparent;");
    cat_layout_ = new QVBoxLayout(cat_container_);
    cat_layout_->setContentsMargins(0, 0, 0, 0);
    cat_layout_->setSpacing(0);
    body_vl->addWidget(cat_container_);

    // ── Leaderboard section ───────────────────────────────────────────────────
    auto* lb_hdr = new QWidget(this);
    lb_hdr->setFixedHeight(26);
    lb_hdr->setStyleSheet(QString("background:%1;border-top:1px solid %2;"
                                  "border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* lb_hl = new QHBoxLayout(lb_hdr);
    lb_hl->setContentsMargins(14, 0, 14, 0);
    lb_hl->setSpacing(6);
    auto* lb_dot = new QLabel("★");
    lb_dot->setStyleSheet(QString("color:%1;font-size:8px;background:transparent;").arg(ui::colors::AMBER()));
    auto* lb_lbl = new QLabel("LEADERBOARD");
    lb_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1.5px;"
                                  "background:transparent;%2")
                              .arg(ui::colors::TEXT_TERTIARY(), M(9)));
    lb_hl->addWidget(lb_dot);
    lb_hl->addWidget(lb_lbl);
    lb_hl->addStretch();
    body_vl->addWidget(lb_hdr);

    contrib_container_ = new QWidget(this);
    contrib_container_->setStyleSheet("background:transparent;");
    contrib_layout_ = new QVBoxLayout(contrib_container_);
    contrib_layout_->setContentsMargins(0, 0, 0, 0);
    contrib_layout_->setSpacing(0);

    auto* ph = new QLabel("  loading...");
    ph->setFixedHeight(28);
    ph->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;%2").arg(ui::colors::TEXT_DIM(), M(10)));
    contrib_layout_->addWidget(ph);
    body_vl->addWidget(contrib_container_);

    body_vl->addStretch();
    scroll->setWidget(body);
    root->addWidget(scroll, 1);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Data setters
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumSidebarPanel::set_my_profile(const services::ForumProfile& profile) {
    QString color = profile.avatar_color.isEmpty() ? ui::colors::AMBER() : profile.avatar_color;
    QString initials = profile.display_name.isEmpty() ? "?" : profile.display_name.left(2).toUpper();
    avatar_lbl_->setText(initials);
    avatar_lbl_->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;background:%2;"
                                       "border-radius:18px;%3")
                                   .arg(ui::colors::BG_BASE(), color, M(14)));

    profile_name_lbl_->setText(profile.display_name);
    profile_sub_lbl_->setText("@" + profile.username);
    profile_rep_lbl_->setText(QString("★ %1").arg(profile.reputation));
}

void ForumSidebarPanel::set_stats(const services::ForumStats& stats) {
    cached_stats_ = stats;

    auto fmt = [](int n) -> QString {
        if (n >= 10000)
            return QString("%1K").arg(n / 1000);
        if (n >= 1000)
            return QString("%1.%2K").arg(n / 1000).arg((n % 1000) / 100);
        return QString::number(n);
    };

    if (stat_posts_val_)
        stat_posts_val_->setText(fmt(stats.total_posts));
    if (stat_comments_val_)
        stat_comments_val_->setText(fmt(stats.total_comments));
    if (stat_active_val_)
        stat_active_val_->setText(QString::number(stats.recent_posts_24h));

    contributors_ = stats.top_contributors;
    rebuild_contributors();
}

void ForumSidebarPanel::set_categories(const QVector<services::ForumCategory>& cats) {
    categories_ = cats;
    rebuild_categories();
}

void ForumSidebarPanel::set_active_category(int id) {
    active_category_id_ = id;
    rebuild_categories();
}

void ForumSidebarPanel::set_loading(bool on) {
    Q_UNUSED(on)
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Categories
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumSidebarPanel::rebuild_categories() {
    while (cat_layout_->count() > 0) {
        auto* item = cat_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const auto& cat : categories_) {
        const bool active = (cat.id == active_category_id_);
        QString color = cat.color.isEmpty() ? ui::colors::AMBER() : cat.color;

        auto* row = new QWidget(this);
        row->setFixedHeight(34);
        row->setCursor(Qt::PointingHandCursor);
        row->setStyleSheet(active ? QString("background:rgba(255,255,255,0.03);"
                                            "border-bottom:1px solid %1;"
                                            "border-left:2px solid %2;")
                                        .arg(ui::colors::BORDER_DIM(), color)
                                  : QString("background:transparent;"
                                            "border-bottom:1px solid %1;"
                                            "border-left:2px solid transparent;")
                                        .arg(ui::colors::BORDER_DIM()));

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 0, 10, 0);
        hl->setSpacing(8);

        // Hash icon
        auto* hash = new QLabel("#");
        hash->setFixedWidth(14);
        hash->setStyleSheet(QString("color:%1;font-size:13px;font-weight:600;background:transparent;%2")
                                .arg(active ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_DIM(), M(13)));

        // Name
        auto* name_lbl = new QLabel(cat.name.toLower().replace(' ', '-'));
        name_lbl->setStyleSheet(active ? QString("color:%1;font-size:12px;font-weight:600;"
                                                 "background:transparent;%2")
                                             .arg(ui::colors::TEXT_PRIMARY(), M(12))
                                       : QString("color:%1;font-size:12px;background:transparent;%2")
                                             .arg(ui::colors::TEXT_SECONDARY(), M(12)));

        // Post count badge
        auto* count_lbl = new QLabel;
        if (cat.post_count > 0) {
            count_lbl->setText(QString::number(cat.post_count));
            count_lbl->setFixedHeight(18);
            count_lbl->setMinimumWidth(24);
            count_lbl->setAlignment(Qt::AlignCenter);
            count_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;"
                                             "background:rgba(255,255,255,0.04);"
                                             "border-radius:9px;padding:0 6px;%2")
                                         .arg(ui::colors::TEXT_TERTIARY(), M(9)));
        }

        // Click handler
        auto* click_btn = new QPushButton(row);
        click_btn->setGeometry(0, 0, 2000, 34);
        click_btn->setFlat(true);
        click_btn->setStyleSheet("QPushButton{background:transparent;border:none;}");
        click_btn->lower();
        connect(click_btn, &QPushButton::clicked, this, [this, cat]() {
            set_active_category(cat.id);
            emit category_selected(cat.id, cat.name, cat.color);
        });

        hl->addWidget(hash);
        hl->addWidget(name_lbl, 1);
        if (cat.post_count > 0)
            hl->addWidget(count_lbl);
        cat_layout_->addWidget(row);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Leaderboard
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumSidebarPanel::rebuild_contributors() {
    while (contrib_layout_->count() > 0) {
        auto* item = contrib_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (contributors_.isEmpty()) {
        auto* ph = new QLabel("  No contributors yet");
        ph->setFixedHeight(28);
        ph->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2").arg(ui::colors::TEXT_DIM(), M(10)));
        contrib_layout_->addWidget(ph);
        return;
    }

    // Medal icons for top 3
    static const char* medals[] = {"🥇", "🥈", "🥉", " 4", " 5"};
    const QString rank_colors[] = {ui::colors::AMBER(), ui::colors::TEXT_SECONDARY(), ui::colors::WARNING(),
                                   ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_TERTIARY()};

    for (int i = 0; i < std::min(5, (int)contributors_.size()); ++i) {
        const auto& c = contributors_[i];
        auto* row = new QWidget(this);
        row->setFixedHeight(32);
        row->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
                .arg(i % 2 == 0 ? ui::colors::BG_BASE() : ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(14, 0, 14, 0);
        hl->setSpacing(8);

        // Rank
        auto* rank = new QLabel(i < 3 ? medals[i] : QString::number(i + 1));
        rank->setFixedWidth(18);
        rank->setAlignment(Qt::AlignCenter);
        rank->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;"
                                    "background:transparent;%3")
                                .arg(rank_colors[i])
                                .arg(i < 3 ? 12 : 10)
                                .arg(M(i < 3 ? 12 : 10)));

        // Avatar
        auto* av = new QLabel(c.display_name.left(1).toUpper());
        av->setFixedSize(22, 22);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;background:%2;"
                                  "border-radius:11px;%3")
                              .arg(ui::colors::BG_BASE(), det_color(c.display_name), M(10)));

        // Name
        auto* name = new QLabel(c.display_name.left(12));
        name->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2").arg(ui::colors::TEXT_SECONDARY(), M(11)));

        // Rep
        auto* rep = new QLabel(QString("★%1").arg(c.reputation));
        rep->setStyleSheet(QString("color:%1;font-size:10px;font-weight:600;"
                                   "background:transparent;%2")
                               .arg(rank_colors[i], M(10)));
        rep->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        hl->addWidget(rank);
        hl->addWidget(av);
        hl->addWidget(name, 1);
        hl->addWidget(rep);
        contrib_layout_->addWidget(row);
    }
}

} // namespace fincept::screens
