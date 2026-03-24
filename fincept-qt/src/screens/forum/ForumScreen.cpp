// src/screens/forum/ForumScreen.cpp
#include "screens/forum/ForumScreen.h"

#include "core/logging/Logger.h"
#include "screens/forum/ForumFeedPanel.h"
#include "screens/forum/ForumThreadPanel.h"
#include "services/forum/ForumService.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTextEdit>
#include <QVBoxLayout>

namespace fincept::screens {

static QString M(int sz = 12) {
    return QString("font-family:'Consolas','Courier New',monospace;font-size:%1px;").arg(sz);
}

ForumScreen::ForumScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// build_ui — single stacked widget, NO splitter, NO columns
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::build_ui() {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_toolbar();
    root->addWidget(toolbar_);

    // ── Main content: stacked feed / thread ───────────────────────────────────
    main_stack_ = new QStackedWidget;
    feed_ = new ForumFeedPanel;
    thread_ = new ForumThreadPanel;
    main_stack_->addWidget(feed_);   // 0
    main_stack_->addWidget(thread_); // 1
    main_stack_->setCurrentIndex(0);

    root->addWidget(main_stack_, 1);

    // ── Wire signals ──────────────────────────────────────────────────────────
    connect(feed_, &ForumFeedPanel::post_selected, this, &ForumScreen::on_post_selected);

    connect(feed_, &ForumFeedPanel::category_clicked, this, &ForumScreen::on_category_selected);

    connect(feed_, &ForumFeedPanel::load_more_requested, this, [this](int page) {
        if (active_category_id_ > 0) {
            feed_->set_loading(true);
            services::ForumService::instance().fetch_posts(active_category_id_, page, "latest",
                                                           [this](bool ok, services::ForumPostsPage p) {
                                                               if (ok)
                                                                   feed_->set_posts(p, active_category_color_);
                                                               else
                                                                   feed_->set_loading(false);
                                                           });
        }
    });

    // Thread panel back button → return to feed
    connect(thread_, &ForumThreadPanel::back_requested, this, &ForumScreen::navigate_back_to_feed);

    connect(thread_, &ForumThreadPanel::comment_submitted, this, [this](const QString& uuid, const QString& content) {
        services::ForumService::instance().create_comment(uuid, content, [this, uuid](bool ok, const QString&) {
            if (ok) {
                thread_->set_loading(true);
                services::ForumService::instance().fetch_post(uuid, [this](bool ok2, services::ForumPostDetail d) {
                    if (ok2)
                        thread_->show_post(d);
                    else
                        thread_->set_loading(false);
                });
            }
        });
    });

    connect(thread_, &ForumThreadPanel::vote_post, this, [this](const QString& uuid, const QString& vtype) {
        services::ForumService::instance().vote_post(uuid, vtype, [this, uuid](bool ok, const QString&) {
            if (ok) {
                services::ForumService::instance().fetch_post(uuid, [this](bool ok2, services::ForumPostDetail d) {
                    if (ok2)
                        thread_->show_post(d);
                });
            }
        });
    });

    connect(thread_, &ForumThreadPanel::vote_comment, this, [this](const QString& uuid, const QString& vtype) {
        services::ForumService::instance().vote_comment(uuid, vtype, [this](bool, const QString&) {
            if (!current_detail_uuid_.isEmpty()) {
                services::ForumService::instance().fetch_post(current_detail_uuid_,
                                                              [this](bool ok, services::ForumPostDetail d) {
                                                                  if (ok)
                                                                      thread_->show_post(d);
                                                              });
            }
        });
    });

    connect(thread_, &ForumThreadPanel::author_clicked, this, [this](const QString& username) {
        services::ForumService::instance().fetch_profile(username, [this](bool ok, services::ForumProfile profile) {
            if (!ok)
                return;
            // Profile popup dialog
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle("USER PROFILE");
            dlg->setFixedSize(340, 280);
            dlg->setStyleSheet("QDialog{background:#0a0a0a;border:1px solid #1a1a1a;}"
                               "QLabel{background:transparent;"
                               "font-family:'Consolas','Courier New',monospace;}");
            auto* vl = new QVBoxLayout(dlg);
            vl->setContentsMargins(20, 16, 20, 14);
            vl->setSpacing(8);

            auto* name_lbl = new QLabel(profile.display_name.toUpper());
            name_lbl->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;%2")
                                        .arg(profile.avatar_color.isEmpty() ? "#d97706" : profile.avatar_color, M(16)));
            vl->addWidget(name_lbl);

            if (!profile.bio.isEmpty()) {
                auto* bio = new QLabel(profile.bio);
                bio->setWordWrap(true);
                bio->setStyleSheet(QString("color:#444444;font-size:12px;font-style:italic;%1").arg(M(12)));
                vl->addWidget(bio);
            }

            auto* sep = new QFrame;
            sep->setFixedHeight(1);
            sep->setStyleSheet("background:#151515;border:none;");
            vl->addWidget(sep);

            auto mk_row = [&](const QString& l, const QString& v, const QString& vc) {
                auto* w = new QWidget;
                w->setStyleSheet("background:transparent;");
                auto* hl = new QHBoxLayout(w);
                hl->setContentsMargins(0, 0, 0, 0);
                auto* ll = new QLabel(l);
                ll->setStyleSheet(QString("color:#252525;font-size:11px;%1").arg(M(11)));
                auto* vl2 = new QLabel(v);
                vl2->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;%2").arg(vc, M(11)));
                hl->addWidget(ll);
                hl->addStretch();
                hl->addWidget(vl2);
                return w;
            };
            vl->addWidget(mk_row("REPUTATION", QString::number(profile.reputation), "#d97706"));
            vl->addWidget(mk_row("POSTS", QString::number(profile.posts_count), "#888888"));
            vl->addWidget(mk_row("COMMENTS", QString::number(profile.comments_count), "#888888"));
            vl->addWidget(mk_row("LIKES GIVEN", QString::number(profile.likes_given), "#16a34a"));

            if (profile.is_own_profile) {
                auto* edit_btn = new QPushButton("EDIT MY PROFILE");
                edit_btn->setFixedHeight(28);
                edit_btn->setCursor(Qt::PointingHandCursor);
                edit_btn->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.1);"
                                                "color:#444444;border:1px solid #1e1e1e;"
                                                "font-size:11px;font-weight:700;%1}"
                                                "QPushButton:hover{color:#d97706;"
                                                "border-color:rgba(217,119,6,0.4);}")
                                            .arg(M(11)));
                connect(edit_btn, &QPushButton::clicked, this, [this, dlg, profile]() {
                    dlg->accept();
                    show_edit_profile_dialog(profile);
                });
                vl->addStretch();
                vl->addWidget(edit_btn);
            } else {
                vl->addStretch();
            }
            dlg->exec();
        });
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Toolbar: brand | scrollable category chips | live stats | search | compose btn
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::build_toolbar() {
    toolbar_ = new QWidget;
    toolbar_->setFixedHeight(44);
    toolbar_->setStyleSheet("background:#050505;border-bottom:1px solid #111111;");
    auto* tb = new QHBoxLayout(toolbar_);
    tb->setContentsMargins(0, 0, 0, 0);
    tb->setSpacing(0);

    // ── Brand block ───────────────────────────────────────────────────────────
    auto* brand = new QWidget;
    brand->setFixedWidth(120);
    brand->setStyleSheet("background:transparent;border-right:1px solid #111111;");
    auto* brand_hl = new QHBoxLayout(brand);
    brand_hl->setContentsMargins(14, 0, 14, 0);
    brand_hl->setSpacing(8);

    auto* diamond = new QLabel("◈");
    diamond->setStyleSheet("color:#d97706;font-size:16px;background:transparent;");
    auto* forum_lbl = new QLabel("FORUM");
    forum_lbl->setStyleSheet(QString("color:#e5e5e5;font-size:12px;font-weight:700;letter-spacing:0.8px;"
                                     "background:transparent;%1")
                                 .arg(M(12)));
    brand_hl->addWidget(diamond);
    brand_hl->addWidget(forum_lbl);
    tb->addWidget(brand);

    // ── Category chips (horizontal scrollable) ────────────────────────────────
    auto* chips_scroll = new QScrollArea;
    chips_scroll->setFixedHeight(44);
    chips_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chips_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chips_scroll->setStyleSheet("QScrollArea{border:none;background:transparent;border-right:1px solid #111111;}");
    chips_scroll->setWidgetResizable(false);

    chips_container_ = new QWidget;
    chips_container_->setFixedHeight(44);
    chips_container_->setStyleSheet("background:transparent;");
    chips_layout_ = new QHBoxLayout(chips_container_);
    chips_layout_->setContentsMargins(8, 0, 8, 0);
    chips_layout_->setSpacing(4);

    // Trending chip — always present
    auto* trend_chip = new QPushButton("▲ TRENDING");
    trend_chip->setFixedHeight(26);
    trend_chip->setCursor(Qt::PointingHandCursor);
    trend_chip->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.08);color:#d97706;"
                                      "border:1px solid rgba(217,119,6,0.25);padding:0 12px;"
                                      "font-size:10px;font-weight:700;letter-spacing:0.5px;%1}"
                                      "QPushButton:hover{background:rgba(217,119,6,0.15);"
                                      "border-color:rgba(217,119,6,0.5);}")
                                  .arg(M(10)));
    connect(trend_chip, &QPushButton::clicked, this, &ForumScreen::on_trending);
    chips_layout_->addWidget(trend_chip);

    chips_layout_->addStretch();
    chips_container_->adjustSize();
    chips_scroll->setWidget(chips_container_);
    tb->addWidget(chips_scroll, 1);

    // ── Live stats chips ──────────────────────────────────────────────────────
    auto* stats_w = new QWidget;
    stats_w->setFixedWidth(160);
    stats_w->setStyleSheet("background:transparent;border-right:1px solid #111111;");
    auto* stats_hl = new QHBoxLayout(stats_w);
    stats_hl->setContentsMargins(10, 0, 10, 0);
    stats_hl->setSpacing(10);

    stat_posts_lbl_ = new QLabel("—");
    stat_posts_lbl_->setStyleSheet(QString("color:#666666;font-size:11px;background:transparent;%1").arg(M(11)));
    stat_active_lbl_ = new QLabel("—");
    stat_active_lbl_->setStyleSheet(
        QString("color:#d97706;font-size:11px;font-weight:700;background:transparent;%1").arg(M(11)));

    stats_hl->addWidget(stat_posts_lbl_);
    stats_hl->addStretch();
    stats_hl->addWidget(stat_active_lbl_);
    tb->addWidget(stats_w);

    // ── Search + profile avatar + compose ─────────────────────────────────────
    auto* right_w = new QWidget;
    right_w->setFixedWidth(300);
    right_w->setStyleSheet("background:transparent;");
    auto* right_hl = new QHBoxLayout(right_w);
    right_hl->setContentsMargins(8, 7, 10, 7);
    right_hl->setSpacing(6);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("search posts...");
    search_input_->setStyleSheet(QString("QLineEdit{background:#0a0a0a;color:#888888;border:1px solid #141414;"
                                         "padding:3px 10px;font-size:11px;%1}"
                                         "QLineEdit:focus{border-color:#252525;color:#e5e5e5;}"
                                         "QLineEdit::placeholder{color:#555555;}")
                                     .arg(M(11)));
    connect(search_input_, &QLineEdit::returnPressed, this, [this]() { on_search(search_input_->text().trimmed()); });

    // Profile avatar
    profile_avatar_ = new QLabel("?");
    profile_avatar_->setFixedSize(28, 28);
    profile_avatar_->setAlignment(Qt::AlignCenter);
    profile_avatar_->setStyleSheet(QString("color:#080808;font-size:11px;font-weight:700;"
                                           "background:#d97706;%1")
                                       .arg(M(11)));

    // New post button
    auto* new_btn = new QPushButton("+ NEW POST");
    new_btn->setFixedHeight(28);
    new_btn->setCursor(Qt::PointingHandCursor);
    new_btn->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.1);color:#555555;"
                                   "border:1px solid #1a1a1a;padding:0 12px;"
                                   "font-size:10px;font-weight:700;%1}"
                                   "QPushButton:hover{color:#d97706;border-color:rgba(217,119,6,0.4);"
                                   "background:rgba(217,119,6,0.18);}")
                               .arg(M(10)));
    connect(new_btn, &QPushButton::clicked, this, [this]() { on_new_post_requested(); });

    right_hl->addWidget(search_input_, 1);
    right_hl->addWidget(profile_avatar_);
    right_hl->addWidget(new_btn);
    tb->addWidget(right_w);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lifecycle
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initial_load_done_)
        load_initial_data();
}

void ForumScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

void ForumScreen::load_initial_data() {
    initial_load_done_ = true;
    feed_->set_loading(true);

    // Stats → toolbar + feed dashboard
    services::ForumService::instance().fetch_stats([this](bool ok, services::ForumStats s) {
        if (!ok)
            return;
        stat_posts_lbl_->setText(QString("%1 posts").arg(s.total_posts));
        stat_active_lbl_->setText(QString("%1 today").arg(s.recent_posts_24h));
        feed_->set_stats(s);
    });

    // My profile → toolbar avatar + feed dashboard
    services::ForumService::instance().fetch_my_profile([this](bool ok, services::ForumProfile p) {
        if (!ok)
            return;
        QString col = p.avatar_color.isEmpty() ? "#d97706" : p.avatar_color;
        QString ini = p.display_name.isEmpty() ? "?" : p.display_name.left(2).toUpper();
        profile_avatar_->setText(ini);
        profile_avatar_->setStyleSheet(QString("color:#080808;font-size:11px;font-weight:700;"
                                               "background:%1;%2")
                                           .arg(col, M(11)));
        feed_->set_profile(p);
    });

    // Categories → toolbar chips + feed dashboard cards
    services::ForumService::instance().fetch_categories(
        [this](bool ok, QVector<services::ForumCategory> cats, services::ForumPermissions) {
            if (!ok || cats.isEmpty())
                return;
            categories_ = cats;
            rebuild_category_chips();

            // Auto-select first category with posts, else just first
            int sel_idx = 0;
            for (int i = 0; i < cats.size(); ++i) {
                if (cats[i].post_count > 0) {
                    sel_idx = i;
                    break;
                }
            }
            const auto& first = cats[sel_idx];
            active_category_id_ = first.id;
            active_category_name_ = first.name;
            active_category_color_ = first.color;
            feed_->set_header(first.name);
            feed_->set_categories(cats, first.id);

            services::ForumService::instance().fetch_posts(first.id, 1, "latest",
                                                           [this, first](bool ok2, services::ForumPostsPage p) {
                                                               if (ok2)
                                                                   feed_->set_posts(p, first.color);
                                                               else
                                                                   feed_->set_loading(false);
                                                           });
        });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Category chip bar
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::rebuild_category_chips() {
    // Remove everything except trending chip (index 0) and stretch
    while (chips_layout_->count() > 1) {
        auto* item = chips_layout_->takeAt(1);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const auto& cat : categories_) {
        const bool active = (cat.id == active_category_id_);
        QString cc = cat.color.isEmpty() ? "#d97706" : cat.color;

        auto* chip = new QPushButton(cat.name.toUpper());
        chip->setFixedHeight(26);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(active ? QString("QPushButton{background:%1;color:#080808;"
                                             "border:1px solid %1;padding:0 12px;"
                                             "font-size:10px;font-weight:700;letter-spacing:0.3px;%2}")
                                         .arg(cc, M(10))
                                   : QString("QPushButton{background:transparent;color:#777777;"
                                             "border:1px solid #1a1a1a;padding:0 12px;"
                                             "font-size:10px;letter-spacing:0.3px;%1}"
                                             "QPushButton:hover{color:#888888;border-color:#2a2a2a;}")
                                         .arg(M(10)));
        connect(chip, &QPushButton::clicked, this,
                [this, cat]() { on_category_selected(cat.id, cat.name, cat.color); });
        chips_layout_->addWidget(chip);
    }

    chips_layout_->addStretch();
    chips_container_->adjustSize();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Navigation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::navigate_back_to_feed() {
    main_stack_->setCurrentIndex(0);
}

void ForumScreen::on_category_selected(int id, const QString& name, const QString& color) {
    active_category_id_ = id;
    active_category_name_ = name;
    active_category_color_ = color;
    feed_->set_header(name);
    feed_->set_loading(true);
    feed_->clear_active();
    feed_->set_categories(categories_, id);
    main_stack_->setCurrentIndex(0); // Go back to feed if in thread
    rebuild_category_chips();

    services::ForumService::instance().fetch_posts(id, 1, "latest", [this, color](bool ok, services::ForumPostsPage p) {
        if (ok)
            feed_->set_posts(p, color);
        else
            feed_->set_loading(false);
    });
}

void ForumScreen::on_post_selected(const services::ForumPost& post) {
    current_detail_uuid_ = post.post_uuid;
    feed_->set_active_post(post.post_uuid);
    thread_->set_loading(true);
    main_stack_->setCurrentIndex(1); // Switch to thread view — FULL SCREEN

    services::ForumService::instance().fetch_post(post.post_uuid, [this](bool ok, services::ForumPostDetail d) {
        if (ok)
            thread_->show_post(d);
        else
            thread_->set_loading(false);
    });
}

void ForumScreen::on_search(const QString& query) {
    if (query.isEmpty())
        return;
    active_category_id_ = 0;
    active_category_color_ = "#06b6d4";
    feed_->set_header("SEARCH: " + query);
    feed_->set_loading(true);
    feed_->clear_active();
    main_stack_->setCurrentIndex(0);
    rebuild_category_chips();

    services::ForumService::instance().search(query, 1, [this](bool ok, services::ForumPostsPage p) {
        if (ok)
            feed_->set_posts(p, "#06b6d4");
        else
            feed_->set_loading(false);
    });
}

void ForumScreen::on_trending() {
    active_category_id_ = 0;
    active_category_color_ = "#d97706";
    feed_->set_header("TRENDING");
    feed_->set_loading(true);
    feed_->clear_active();
    main_stack_->setCurrentIndex(0);
    rebuild_category_chips();

    services::ForumService::instance().fetch_trending([this](bool ok, services::ForumPostsPage p) {
        if (ok)
            feed_->set_posts(p, "#d97706");
        else
            feed_->set_loading(false);
    });
}

void ForumScreen::on_new_post_requested() {
    show_new_post_dialog(active_category_id_ > 0 ? active_category_id_ : 1);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Dialogs
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::show_new_post_dialog(int category_id) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("NEW POST");
    dlg->setMinimumSize(520, 340);
    dlg->setStyleSheet("QDialog{background:#0a0a0a;border:1px solid #1a1a1a;}"
                       "QLabel{color:#888888;font-size:11px;background:transparent;"
                       "font-family:'Consolas','Courier New',monospace;}"
                       "QLineEdit,QTextEdit{background:#080808;color:#e5e5e5;"
                       "border:1px solid #1a1a1a;font-size:12px;"
                       "font-family:'Consolas','Courier New',monospace;padding:4px 10px;}"
                       "QLineEdit:focus,QTextEdit:focus{border-color:#2a2a2a;}");
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(18, 16, 18, 14);
    vl->setSpacing(10);

    auto* hdr = new QLabel("NEW POST");
    hdr->setStyleSheet(QString("color:#d97706;font-size:14px;font-weight:700;letter-spacing:1px;%1").arg(M(14)));
    vl->addWidget(hdr);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#111111;border:none;");
    vl->addWidget(sep);

    auto* title_edit = new QLineEdit;
    title_edit->setPlaceholderText("Post title...");
    title_edit->setFixedHeight(32);
    auto* body_edit = new QTextEdit;
    body_edit->setPlaceholderText("Write your post content...");

    vl->addWidget(new QLabel("TITLE"));
    vl->addWidget(title_edit);
    vl->addWidget(new QLabel("CONTENT"));
    vl->addWidget(body_edit, 1);

    auto* btn_row = new QWidget;
    btn_row->setStyleSheet("background:transparent;");
    auto* btn_hl = new QHBoxLayout(btn_row);
    btn_hl->setContentsMargins(0, 0, 0, 0);
    btn_hl->setSpacing(8);

    auto* cancel = new QPushButton("CANCEL");
    cancel->setFixedHeight(28);
    cancel->setStyleSheet(QString("QPushButton{background:transparent;color:#333333;"
                                  "border:1px solid #1a1a1a;font-size:11px;font-weight:700;"
                                  "padding:0 16px;%1}"
                                  "QPushButton:hover{color:#888888;}")
                              .arg(M(11)));
    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* submit = new QPushButton("SUBMIT POST");
    submit->setFixedHeight(28);
    submit->setCursor(Qt::PointingHandCursor);
    submit->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:#555555;"
                                  "border:1px solid #1e1e1e;font-size:11px;font-weight:700;"
                                  "padding:0 16px;%1}"
                                  "QPushButton:hover{color:#d97706;"
                                  "border-color:rgba(217,119,6,0.4);}")
                              .arg(M(11)));
    connect(submit, &QPushButton::clicked, this, [this, dlg, title_edit, body_edit, category_id]() {
        QString title = title_edit->text().trimmed();
        QString content = body_edit->toPlainText().trimmed();
        if (title.isEmpty() || content.isEmpty())
            return;
        dlg->accept();
        services::ForumService::instance().create_post(
            category_id, title, content, [this](bool ok, const QString& msg) {
                if (ok)
                    on_category_selected(active_category_id_, active_category_name_, active_category_color_);
                else
                    LOG_WARN("ForumScreen", "Create post failed: " + msg);
            });
    });

    btn_hl->addStretch();
    btn_hl->addWidget(cancel);
    btn_hl->addWidget(submit);
    vl->addWidget(btn_row);
    dlg->exec();
}

void ForumScreen::show_edit_profile_dialog(const services::ForumProfile& profile) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("EDIT PROFILE");
    dlg->setMinimumSize(400, 280);
    dlg->setStyleSheet("QDialog{background:#0a0a0a;border:1px solid #1a1a1a;}"
                       "QLabel{color:#888888;font-size:11px;background:transparent;"
                       "font-family:'Consolas','Courier New',monospace;}"
                       "QLineEdit{background:#080808;color:#e5e5e5;border:1px solid #1a1a1a;"
                       "font-size:12px;font-family:'Consolas','Courier New',monospace;"
                       "padding:3px 10px;}"
                       "QLineEdit:focus{border-color:#2a2a2a;}");
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(18, 16, 18, 14);
    vl->setSpacing(8);

    auto* hdr = new QLabel("EDIT PROFILE");
    hdr->setStyleSheet(QString("color:#d97706;font-size:14px;font-weight:700;letter-spacing:1px;%1").arg(M(14)));
    vl->addWidget(hdr);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:#111111;border:none;");
    vl->addWidget(sep);

    auto mk = [&](const QString& lbl, const QString& val) -> QLineEdit* {
        vl->addWidget(new QLabel(lbl));
        auto* e = new QLineEdit(val);
        e->setFixedHeight(28);
        vl->addWidget(e);
        return e;
    };
    auto* name_e = mk("DISPLAY NAME", profile.display_name);
    auto* bio_e = mk("BIO", profile.bio);
    auto* sig_e = mk("SIGNATURE", profile.signature);
    auto* col_e = mk("AVATAR COLOR", profile.avatar_color);

    auto* btn_row = new QWidget;
    btn_row->setStyleSheet("background:transparent;");
    auto* bh = new QHBoxLayout(btn_row);
    bh->setContentsMargins(0, 0, 0, 0);
    bh->setSpacing(8);

    auto* cc = new QPushButton("CANCEL");
    cc->setFixedHeight(28);
    cc->setStyleSheet(QString("QPushButton{background:transparent;color:#333333;"
                              "border:1px solid #1a1a1a;font-size:11px;font-weight:700;"
                              "padding:0 16px;%1}"
                              "QPushButton:hover{color:#888888;}")
                          .arg(M(11)));
    connect(cc, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* sc = new QPushButton("SAVE");
    sc->setFixedHeight(28);
    sc->setCursor(Qt::PointingHandCursor);
    sc->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:#555555;"
                              "border:1px solid #1e1e1e;font-size:11px;font-weight:700;"
                              "padding:0 16px;%1}"
                              "QPushButton:hover{color:#d97706;"
                              "border-color:rgba(217,119,6,0.4);}")
                          .arg(M(11)));
    connect(sc, &QPushButton::clicked, this, [this, dlg, name_e, bio_e, sig_e, col_e]() {
        dlg->accept();
        services::ForumService::instance().update_profile(
            name_e->text().trimmed(), bio_e->text().trimmed(), sig_e->text().trimmed(), col_e->text().trimmed(),
            [this](bool ok, const QString&) {
                if (ok) {
                    // Refresh avatar
                    services::ForumService::instance().fetch_my_profile([this](bool ok2, services::ForumProfile p) {
                        if (!ok2)
                            return;
                        QString col = p.avatar_color.isEmpty() ? "#d97706" : p.avatar_color;
                        QString ini = p.display_name.isEmpty() ? "?" : p.display_name.left(2).toUpper();
                        profile_avatar_->setText(ini);
                        profile_avatar_->setStyleSheet(QString("color:#080808;font-size:11px;"
                                                               "font-weight:700;background:%1;%2")
                                                           .arg(col, M(11)));
                    });
                }
            });
    });

    bh->addStretch();
    bh->addWidget(cc);
    bh->addWidget(sc);
    vl->addStretch();
    vl->addWidget(btn_row);
    dlg->exec();
}

} // namespace fincept::screens
