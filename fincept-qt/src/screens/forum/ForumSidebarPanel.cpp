// src/screens/forum/ForumSidebarPanel.cpp
#include "screens/forum/ForumSidebarPanel.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>

namespace fincept::screens {

static QString mono(int sz = 12) {
    return QString("font-family:'Consolas','Courier New',monospace;font-size:%1px;").arg(sz);
}

// ── Section header factory ─────────────────────────────────────────────────
static QWidget* make_section_hdr(const QString& title, const QString& accent = {}) {
    auto* w = new QWidget;
    w->setFixedHeight(22);
    w->setStyleSheet(QString("background:#0d0d0d;border-top:1px solid #1a1a1a;"
                             "border-bottom:1px solid #1a1a1a;"));
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(6);

    if (!accent.isEmpty()) {
        auto* dot = new QLabel("■");
        dot->setStyleSheet(QString("color:%1;font-size:7px;background:transparent;").arg(accent));
        hl->addWidget(dot);
    }

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        QString("color:#555555;font-size:9px;font-weight:700;letter-spacing:1.2px;"
                "background:transparent;%1").arg(mono(9)));
    hl->addWidget(lbl);
    hl->addStretch();
    return w;
}

// ── Thin divider ──────────────────────────────────────────────────────────
static QFrame* make_divider() {
    auto* f = new QFrame;
    f->setFixedHeight(1);
    f->setStyleSheet("background:#111111;border:none;");
    return f;
}

ForumSidebarPanel::ForumSidebarPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ForumSidebarPanel::build_ui() {
    setFixedWidth(210);
    setStyleSheet("background:#080808;border-right:1px solid #111111;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Brand header ──────────────────────────────────────────────────────
    auto* brand = new QWidget;
    brand->setFixedHeight(38);
    brand->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #111111;");
    auto* brand_hl = new QHBoxLayout(brand);
    brand_hl->setContentsMargins(10, 0, 10, 0);
    brand_hl->setSpacing(8);

    auto* brand_icon = new QLabel("◈");
    brand_icon->setStyleSheet(
        QString("color:%1;font-size:16px;background:transparent;").arg(ui::colors::AMBER));
    auto* brand_vl = new QVBoxLayout;
    brand_vl->setSpacing(0);
    brand_vl->setContentsMargins(0, 0, 0, 0);
    auto* brand_title = new QLabel("FINCEPT FORUM");
    brand_title->setStyleSheet(
        QString("color:#e5e5e5;font-size:11px;font-weight:700;letter-spacing:0.5px;"
                "background:transparent;%1").arg(mono(11)));
    auto* brand_sub = new QLabel("COMMUNITY · LIVE");
    brand_sub->setStyleSheet(
        QString("color:#333333;font-size:9px;letter-spacing:0.8px;"
                "background:transparent;%1").arg(mono(9)));
    brand_vl->addWidget(brand_title);
    brand_vl->addWidget(brand_sub);

    brand_hl->addWidget(brand_icon);
    brand_hl->addLayout(brand_vl, 1);
    root->addWidget(brand);

    // ── Search ────────────────────────────────────────────────────────────
    auto* search_wrap = new QWidget;
    search_wrap->setFixedHeight(34);
    search_wrap->setStyleSheet("background:#080808;border-bottom:1px solid #111111;");
    auto* search_hl = new QHBoxLayout(search_wrap);
    search_hl->setContentsMargins(8, 5, 8, 5);
    search_hl->setSpacing(4);

    auto* search_icon = new QLabel("⌕");
    search_icon->setStyleSheet("color:#333333;font-size:13px;background:transparent;");
    search_icon->setFixedWidth(16);

    auto* search_input = new QLineEdit;
    search_input->setPlaceholderText("search posts...");
    search_input->setStyleSheet(
        QString("QLineEdit{background:transparent;color:#888888;border:none;"
                "font-size:11px;%1}"
                "QLineEdit:focus{color:#e5e5e5;}"
                "QLineEdit::placeholder{color:#2a2a2a;}").arg(mono(11)));

    auto* go_btn = new QPushButton("↵");
    go_btn->setFixedSize(22, 22);
    go_btn->setCursor(Qt::PointingHandCursor);
    go_btn->setStyleSheet(
        QString("QPushButton{background:#111111;color:#555555;border:1px solid #1a1a1a;"
                "font-size:12px;font-weight:700;%1}"
                "QPushButton:hover{background:%2;color:#080808;border-color:%2;}")
            .arg(mono(12), ui::colors::AMBER));

    connect(go_btn, &QPushButton::clicked, this,
            [this, search_input]() { emit search_requested(search_input->text().trimmed()); });
    connect(search_input, &QLineEdit::returnPressed, this,
            [this, search_input]() { emit search_requested(search_input->text().trimmed()); });

    search_hl->addWidget(search_icon);
    search_hl->addWidget(search_input, 1);
    search_hl->addWidget(go_btn);
    root->addWidget(search_wrap);

    // ── Scrollable body ───────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:#080808;}"
        "QScrollBar:vertical{width:0px;}");

    auto* body = new QWidget;
    body->setStyleSheet("background:#080808;");
    auto* body_vl = new QVBoxLayout(body);
    body_vl->setContentsMargins(0, 0, 0, 0);
    body_vl->setSpacing(0);

    // ── My profile card ───────────────────────────────────────────────────
    body_vl->addWidget(make_section_hdr("MY PROFILE", ui::colors::CYAN));

    auto* profile_card = new QWidget;
    profile_card->setFixedHeight(52);
    profile_card->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #111111;");
    auto* pc_hl = new QHBoxLayout(profile_card);
    pc_hl->setContentsMargins(10, 6, 10, 6);
    pc_hl->setSpacing(10);

    // Avatar circle
    avatar_lbl_ = new QLabel;
    avatar_lbl_->setFixedSize(32, 32);
    avatar_lbl_->setAlignment(Qt::AlignCenter);
    avatar_lbl_->setStyleSheet(
        QString("color:#080808;font-size:13px;font-weight:700;background:%1;"
                "border-radius:0px;%2").arg(ui::colors::AMBER, mono(13)));
    avatar_lbl_->setText("?");

    auto* profile_right = new QVBoxLayout;
    profile_right->setSpacing(2);
    profile_right->setContentsMargins(0, 0, 0, 0);

    profile_name_lbl_ = new QLabel("—");
    profile_name_lbl_->setStyleSheet(
        QString("color:#e5e5e5;font-size:12px;font-weight:700;background:transparent;%1")
            .arg(mono(12)));

    profile_sub_lbl_ = new QLabel("REP 0  ·  0 POSTS");
    profile_sub_lbl_->setStyleSheet(
        QString("color:#333333;font-size:10px;background:transparent;%1").arg(mono(10)));

    profile_right->addWidget(profile_name_lbl_);
    profile_right->addWidget(profile_sub_lbl_);

    pc_hl->addWidget(avatar_lbl_);
    pc_hl->addLayout(profile_right, 1);
    body_vl->addWidget(profile_card);

    // ── Community stats ───────────────────────────────────────────────────
    body_vl->addWidget(make_section_hdr("COMMUNITY STATS", ui::colors::AMBER));

    // Stats grid — 2x2
    auto* stats_grid = new QWidget;
    stats_grid->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #111111;");
    auto* sg_grid = new QHBoxLayout(stats_grid);
    sg_grid->setContentsMargins(0, 0, 0, 0);
    sg_grid->setSpacing(0);

    auto make_stat_cell = [&](const QString& label, QLabel*& val_out) -> QWidget* {
        auto* cell = new QWidget;
        cell->setStyleSheet("background:transparent;border-right:1px solid #111111;");
        auto* vl = new QVBoxLayout(cell);
        vl->setContentsMargins(10, 8, 10, 8);
        vl->setSpacing(2);
        val_out = new QLabel("—");
        val_out->setStyleSheet(
            QString("color:#e5e5e5;font-size:14px;font-weight:700;background:transparent;%1")
                .arg(mono(14)));
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color:#333333;font-size:9px;letter-spacing:0.5px;background:transparent;%1")
                .arg(mono(9)));
        vl->addWidget(val_out);
        vl->addWidget(lbl);
        return cell;
    };

    sg_grid->addWidget(make_stat_cell("POSTS",    stat_posts_val_));
    sg_grid->addWidget(make_stat_cell("COMMENTS", stat_comments_val_));
    sg_grid->addWidget(make_stat_cell("VOTES",    stat_votes_val_));
    auto* last_cell = make_stat_cell("24H",       stat_24h_val_);
    last_cell->setStyleSheet("background:transparent;");
    sg_grid->addWidget(last_cell);
    body_vl->addWidget(stats_grid);

    // ── Navigation ────────────────────────────────────────────────────────
    body_vl->addWidget(make_section_hdr("NAVIGATE", "#555555"));

    // Trending button
    auto* trending_btn = new QPushButton;
    trending_btn->setFixedHeight(30);
    trending_btn->setCursor(Qt::PointingHandCursor);
    trending_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:#888888;border:none;"
                "border-bottom:1px solid #111111;text-align:left;padding:0 10px;"
                "font-size:11px;%1}"
                "QPushButton:hover{background:#0d0d0d;color:#e5e5e5;}")
            .arg(mono(11)));
    trending_btn->setText("  ▲  TRENDING");
    connect(trending_btn, &QPushButton::clicked, this, &ForumSidebarPanel::trending_clicked);
    body_vl->addWidget(trending_btn);

    // ── Categories ────────────────────────────────────────────────────────
    body_vl->addWidget(make_section_hdr("CHANNELS", ui::colors::ORANGE));

    cat_container_ = new QWidget;
    cat_container_->setStyleSheet("background:transparent;");
    cat_layout_ = new QVBoxLayout(cat_container_);
    cat_layout_->setContentsMargins(0, 0, 0, 0);
    cat_layout_->setSpacing(0);
    body_vl->addWidget(cat_container_);

    // ── Top contributors ─────────────────────────────────────────────────
    body_vl->addWidget(make_section_hdr("TOP CONTRIBUTORS", ui::colors::CYAN));

    contrib_container_ = new QWidget;
    contrib_container_->setStyleSheet("background:transparent;");
    contrib_layout_ = new QVBoxLayout(contrib_container_);
    contrib_layout_->setContentsMargins(0, 0, 0, 0);
    contrib_layout_->setSpacing(0);

    // Placeholder row
    auto* ph = new QLabel("  loading...");
    ph->setFixedHeight(26);
    ph->setStyleSheet(
        QString("color:#222222;font-size:10px;background:transparent;%1").arg(mono(10)));
    contrib_layout_->addWidget(ph);
    body_vl->addWidget(contrib_container_);

    body_vl->addStretch();
    scroll->setWidget(body);
    root->addWidget(scroll, 1);
}

void ForumSidebarPanel::set_my_profile(const services::ForumProfile& profile) {
    if (profile_name_lbl_)
        profile_name_lbl_->setText(profile.display_name.toUpper().left(14));
    if (profile_sub_lbl_)
        profile_sub_lbl_->setText(
            QString("REP %1  ·  %2 POSTS").arg(profile.reputation).arg(profile.posts_count));

    if (avatar_lbl_) {
        QString color = profile.avatar_color.isEmpty() ? ui::colors::AMBER : profile.avatar_color;
        QString initials = profile.display_name.isEmpty() ? "?" :
                           profile.display_name.left(2).toUpper();
        avatar_lbl_->setText(initials);
        avatar_lbl_->setStyleSheet(
            QString("color:#080808;font-size:11px;font-weight:700;background:%1;"
                    "border-radius:0px;%2").arg(color, mono(11)));
    }
}

void ForumSidebarPanel::set_stats(const services::ForumStats& stats) {
    cached_stats_ = stats;

    auto set_val = [](QLabel* lbl, const QString& v) { if (lbl) lbl->setText(v); };

    // Format large numbers
    auto fmt = [](int n) -> QString {
        if (n >= 1000) return QString("%1K").arg(n / 1000);
        return QString::number(n);
    };

    set_val(stat_posts_val_,    fmt(stats.total_posts));
    set_val(stat_comments_val_, fmt(stats.total_comments));
    set_val(stat_votes_val_,    fmt(stats.total_votes));
    set_val(stat_24h_val_,      QString::number(stats.recent_posts_24h));

    // Rebuild contributors
    contributors_ = stats.top_contributors;
    rebuild_contributors();
}

void ForumSidebarPanel::rebuild_contributors() {
    // Clear
    while (contrib_layout_->count() > 0) {
        auto* item = contrib_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (contributors_.isEmpty()) {
        auto* ph = new QLabel("  —");
        ph->setFixedHeight(26);
        ph->setStyleSheet(
            QString("color:#222222;font-size:10px;background:transparent;%1").arg(mono(10)));
        contrib_layout_->addWidget(ph);
        return;
    }

    static const QString rank_colors[] = {
        ui::colors::AMBER, "#aaaaaa", "#cd7f32", "#555555", "#555555"
    };

    for (int i = 0; i < std::min(5, (int)contributors_.size()); ++i) {
        const auto& c = contributors_[i];
        auto* row = new QWidget;
        row->setFixedHeight(26);
        row->setStyleSheet(
            "background:#080808;border-bottom:1px solid #0d0d0d;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 0, 10, 0);
        hl->setSpacing(6);

        // Rank badge
        auto* rank = new QLabel(QString::number(i + 1));
        rank->setFixedWidth(14);
        rank->setAlignment(Qt::AlignCenter);
        rank->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:transparent;%2")
                .arg(rank_colors[i], mono(9)));

        // Name
        auto* name = new QLabel(c.display_name.left(14));
        name->setStyleSheet(
            QString("color:#888888;font-size:11px;background:transparent;%1").arg(mono(11)));

        // Rep chip
        auto* rep = new QLabel(QString("▲%1").arg(c.reputation));
        rep->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(rank_colors[i], mono(10)));
        rep->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        hl->addWidget(rank);
        hl->addWidget(name, 1);
        hl->addWidget(rep);
        contrib_layout_->addWidget(row);
    }
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

void ForumSidebarPanel::rebuild_categories() {
    while (cat_layout_->count() > 0) {
        auto* item = cat_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (const auto& cat : categories_) {
        const bool active = (cat.id == active_category_id_);
        QString color = cat.color.isEmpty() ? ui::colors::ORANGE : cat.color;

        auto* row = new QWidget;
        row->setFixedHeight(30);
        row->setCursor(Qt::PointingHandCursor);
        row->setStyleSheet(
            active
                ? QString("background:#0d0d0d;border-bottom:1px solid #111111;"
                          "border-left:2px solid %1;").arg(color)
                : "background:transparent;border-bottom:1px solid #0d0d0d;border-left:2px solid transparent;");

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(8, 0, 6, 0);
        hl->setSpacing(6);

        // Color chip
        auto* chip = new QLabel("●");
        chip->setFixedWidth(10);
        chip->setStyleSheet(
            QString("color:%1;font-size:8px;background:transparent;").arg(color));

        // Name
        auto* name_lbl = new QLabel(cat.name.toUpper());
        name_lbl->setStyleSheet(
            active
                ? QString("color:#e5e5e5;font-size:11px;font-weight:700;background:transparent;%1")
                      .arg(mono(11))
                : QString("color:#555555;font-size:11px;background:transparent;%1")
                      .arg(mono(11)));

        // Post count
        auto* count_lbl = new QLabel(QString::number(cat.post_count));
        count_lbl->setStyleSheet(
            QString("color:#333333;font-size:10px;background:transparent;%1").arg(mono(10)));
        count_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        count_lbl->setFixedWidth(28);

        // "+" new post — appears on hover (always visible but dim)
        auto* new_btn = new QPushButton("+");
        new_btn->setFixedSize(20, 20);
        new_btn->setCursor(Qt::PointingHandCursor);
        new_btn->setToolTip("New post in " + cat.name);
        new_btn->setStyleSheet(
            QString("QPushButton{background:transparent;color:#2a2a2a;border:none;"
                    "font-size:13px;font-weight:700;%1}"
                    "QPushButton:hover{color:%2;background:rgba(217,119,6,0.1);}").arg(mono(13), ui::colors::AMBER));
        connect(new_btn, &QPushButton::clicked, this,
                [this, cat]() { emit new_post_requested(cat.id); });

        // Click entire row to select
        auto* click_btn = new QPushButton(row);
        click_btn->setGeometry(0, 0, 2000, 30);
        click_btn->setFlat(true);
        click_btn->setStyleSheet("QPushButton{background:transparent;border:none;}");
        click_btn->lower();
        connect(click_btn, &QPushButton::clicked, this, [this, cat]() {
            active_cat_for_post_ = cat.id;
            set_active_category(cat.id);
            emit category_selected(cat.id, cat.name, cat.color);
        });

        hl->addWidget(chip);
        hl->addWidget(name_lbl, 1);
        hl->addWidget(count_lbl);
        hl->addWidget(new_btn);
        cat_layout_->addWidget(row);
    }
}

} // namespace fincept::screens
