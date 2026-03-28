// src/screens/forum/ForumScreen.cpp
#include "screens/forum/ForumScreen.h"

#include "core/logging/Logger.h"
#include "screens/forum/ForumFeedPanel.h"
#include "screens/forum/ForumSidebarPanel.h"
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
#include <QSplitter>
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
// build_ui — 3-column: sidebar | feed | thread (via splitter)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Splitter: sidebar | main content ──────────────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(
        QString("QSplitter{background:%1;}"
                "QSplitter::handle{background:%2;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM));

    // ── Left: Sidebar ─────────────────────────────────────────────────────────
    sidebar_ = new ForumSidebarPanel;

    // ── Center+Right: Stacked feed/thread ─────────────────────────────────────
    main_stack_ = new QStackedWidget;
    feed_ = new ForumFeedPanel;
    thread_ = new ForumThreadPanel;
    main_stack_->addWidget(feed_);   // 0
    main_stack_->addWidget(thread_); // 1
    main_stack_->setCurrentIndex(0);

    splitter_->addWidget(sidebar_);
    splitter_->addWidget(main_stack_);
    splitter_->setStretchFactor(0, 0); // sidebar fixed
    splitter_->setStretchFactor(1, 1); // feed stretches
    splitter_->setSizes({240, 800});

    root->addWidget(splitter_, 1);

    // ── Wire sidebar signals ──────────────────────────────────────────────────
    connect(sidebar_, &ForumSidebarPanel::category_selected,
            this, &ForumScreen::on_category_selected);

    connect(sidebar_, &ForumSidebarPanel::trending_clicked,
            this, &ForumScreen::on_trending);

    connect(sidebar_, &ForumSidebarPanel::search_requested,
            this, &ForumScreen::on_search);

    connect(sidebar_, &ForumSidebarPanel::new_post_requested,
            this, [this](int cat_id) { show_new_post_dialog(cat_id); });

    // ── Wire feed signals ─────────────────────────────────────────────────────
    connect(feed_, &ForumFeedPanel::post_selected,
            this, &ForumScreen::on_post_selected);

    connect(feed_, &ForumFeedPanel::category_clicked,
            this, &ForumScreen::on_category_selected);

    connect(feed_, &ForumFeedPanel::load_more_requested, this, [this](int page) {
        if (active_category_id_ > 0) {
            feed_->set_loading(true);
            services::ForumService::instance().fetch_posts(
                active_category_id_, page, "latest",
                [this](bool ok, services::ForumPostsPage p) {
                    if (ok)
                        feed_->set_posts(p, active_category_color_);
                    else
                        feed_->set_loading(false);
                });
        }
    });

    connect(feed_, &ForumFeedPanel::new_post_clicked,
            this, [this]() { on_new_post_requested(); });

    connect(feed_, &ForumFeedPanel::vote_post_requested,
            this, [this](const QString& uuid, const QString& vtype) {
                LOG_INFO("ForumScreen", "Feed vote: " + uuid + " type=" + vtype);
                services::ForumService::instance().vote_post(
                    uuid, vtype, [this](bool ok, const QString&) {
                        LOG_INFO("ForumScreen", QString("Feed vote result: ok=%1").arg(ok));
                        if (ok && active_category_id_ > 0) {
                            // Refresh current feed to show updated vote
                            services::ForumService::instance().fetch_posts(
                                active_category_id_, 1, "latest",
                                [this](bool ok2, services::ForumPostsPage p) {
                                    if (ok2)
                                        feed_->set_posts(p, active_category_color_);
                                });
                        }
                    });
            });

    // ── Wire thread signals ───────────────────────────────────────────────────
    connect(thread_, &ForumThreadPanel::back_requested,
            this, &ForumScreen::navigate_back_to_feed);

    connect(thread_, &ForumThreadPanel::comment_submitted,
            this, [this](const QString& uuid, const QString& content) {
                services::ForumService::instance().create_comment(
                    uuid, content, [this, uuid](bool ok, const QString&) {
                        if (ok) {
                            thread_->set_loading(true);
                            services::ForumService::instance().fetch_post(
                                uuid, [this](bool ok2, services::ForumPostDetail d) {
                                    if (ok2)
                                        thread_->show_post(d);
                                    else
                                        thread_->set_loading(false);
                                });
                        }
                    });
            });

    connect(thread_, &ForumThreadPanel::vote_post,
            this, [this](const QString& uuid, const QString& vtype) {
                LOG_INFO("ForumScreen", "Vote post: " + uuid + " type=" + vtype);
                services::ForumService::instance().vote_post(
                    uuid, vtype, [this, uuid](bool ok, const QString& msg) {
                        LOG_INFO("ForumScreen", QString("Vote result: ok=%1 msg=%2").arg(ok).arg(msg));
                        if (ok) {
                            services::ForumService::instance().fetch_post(
                                uuid, [this](bool ok2, services::ForumPostDetail d) {
                                    if (ok2)
                                        thread_->show_post(d);
                                });
                        }
                    });
            });

    connect(thread_, &ForumThreadPanel::vote_comment,
            this, [this](const QString& uuid, const QString& vtype) {
                services::ForumService::instance().vote_comment(
                    uuid, vtype, [this](bool, const QString&) {
                        if (!current_detail_uuid_.isEmpty()) {
                            services::ForumService::instance().fetch_post(
                                current_detail_uuid_,
                                [this](bool ok, services::ForumPostDetail d) {
                                    if (ok)
                                        thread_->show_post(d);
                                });
                        }
                    });
            });

    connect(thread_, &ForumThreadPanel::author_clicked,
            this, [this](const QString& username) {
                services::ForumService::instance().fetch_profile(
                    username, [this](bool ok, services::ForumProfile profile) {
                        if (!ok)
                            return;
                        // Profile popup
                        auto* dlg = new QDialog(this);
                        dlg->setWindowTitle("USER PROFILE");
                        dlg->setFixedSize(380, 340);
                        dlg->setStyleSheet(
                            QString("QDialog{background:%1;border:1px solid %2;}"
                                    "QLabel{background:transparent;"
                                    "font-family:'Consolas','Courier New',monospace;}")
                                .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
                        auto* vl = new QVBoxLayout(dlg);
                        vl->setContentsMargins(24, 20, 24, 18);
                        vl->setSpacing(10);

                        // Profile header with gradient accent
                        auto* hdr = new QWidget;
                        hdr->setFixedHeight(4);
                        QString avc = profile.avatar_color.isEmpty() ? ui::colors::AMBER : profile.avatar_color;
                        hdr->setStyleSheet(
                            QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                                    "stop:0 %1,stop:0.5 %2,stop:1 transparent);")
                                .arg(avc, ui::colors::AMBER));
                        vl->addWidget(hdr);

                        // Avatar + name
                        auto* top = new QWidget;
                        top->setStyleSheet("background:transparent;");
                        auto* th = new QHBoxLayout(top);
                        th->setContentsMargins(0, 0, 0, 0);
                        th->setSpacing(14);

                        auto* av = new QLabel(profile.display_name.left(2).toUpper());
                        av->setFixedSize(48, 48);
                        av->setAlignment(Qt::AlignCenter);
                        av->setStyleSheet(
                            QString("color:%1;font-size:16px;font-weight:700;"
                                    "background:%2;border-radius:24px;%3")
                                .arg(ui::colors::BG_BASE, avc, M(16)));

                        auto* info = new QVBoxLayout;
                        info->setSpacing(2);
                        auto* nm = new QLabel(profile.display_name.toUpper());
                        nm->setStyleSheet(
                            QString("color:%1;font-size:16px;font-weight:700;%2")
                                .arg(avc, M(16)));
                        auto* un = new QLabel("@" + profile.username);
                        un->setStyleSheet(
                            QString("color:%1;font-size:11px;%2")
                                .arg(ui::colors::TEXT_SECONDARY, M(11)));
                        info->addWidget(nm);
                        info->addWidget(un);
                        th->addWidget(av);
                        th->addLayout(info, 1);
                        vl->addWidget(top);

                        if (!profile.bio.isEmpty()) {
                            auto* bio = new QLabel(profile.bio);
                            bio->setWordWrap(true);
                            bio->setStyleSheet(
                                QString("color:%1;font-size:12px;font-style:italic;%2")
                                    .arg(ui::colors::TEXT_TERTIARY, M(12)));
                            vl->addWidget(bio);
                        }

                        auto* sep = new QFrame;
                        sep->setFixedHeight(1);
                        sep->setStyleSheet(
                            QString("background:%1;border:none;")
                                .arg(ui::colors::BORDER_DIM));
                        vl->addWidget(sep);

                        // Stats grid
                        auto* grid = new QWidget;
                        grid->setStyleSheet("background:transparent;");
                        auto* gh = new QHBoxLayout(grid);
                        gh->setContentsMargins(0, 0, 0, 0);
                        gh->setSpacing(0);

                        auto mk_stat = [&](const QString& val, const QString& lbl,
                                           const QString& col) {
                            auto* cell = new QWidget;
                            cell->setStyleSheet("background:transparent;");
                            auto* cv = new QVBoxLayout(cell);
                            cv->setContentsMargins(0, 8, 0, 8);
                            cv->setSpacing(2);
                            cv->setAlignment(Qt::AlignCenter);
                            auto* v = new QLabel(val);
                            v->setAlignment(Qt::AlignCenter);
                            v->setStyleSheet(
                                QString("color:%1;font-size:18px;font-weight:700;%2")
                                    .arg(col, M(18)));
                            auto* l = new QLabel(lbl);
                            l->setAlignment(Qt::AlignCenter);
                            l->setStyleSheet(
                                QString("color:%1;font-size:9px;letter-spacing:1px;%2")
                                    .arg(ui::colors::TEXT_TERTIARY, M(9)));
                            cv->addWidget(v);
                            cv->addWidget(l);
                            return cell;
                        };

                        gh->addWidget(mk_stat(QString::number(profile.reputation), "REP", ui::colors::AMBER));
                        gh->addWidget(mk_stat(QString::number(profile.posts_count), "POSTS", ui::colors::TEXT_PRIMARY));
                        gh->addWidget(mk_stat(QString::number(profile.comments_count), "REPLIES", ui::colors::CYAN));
                        gh->addWidget(mk_stat(QString::number(profile.likes_received), "LIKES", ui::colors::POSITIVE));
                        vl->addWidget(grid);

                        if (profile.is_own_profile) {
                            auto* edit_btn = new QPushButton("EDIT MY PROFILE");
                            edit_btn->setFixedHeight(32);
                            edit_btn->setCursor(Qt::PointingHandCursor);
                            edit_btn->setStyleSheet(
                                QString("QPushButton{background:rgba(217,119,6,0.08);"
                                        "color:%1;border:1px solid %2;"
                                        "font-size:11px;font-weight:700;%3}"
                                        "QPushButton:hover{color:%4;"
                                        "border-color:rgba(217,119,6,0.5);"
                                        "background:rgba(217,119,6,0.15);}")
                                    .arg(ui::colors::TEXT_TERTIARY,
                                         ui::colors::BORDER_DIM,
                                         M(11), ui::colors::AMBER));
                            connect(edit_btn, &QPushButton::clicked, this,
                                    [this, dlg, profile]() {
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

    // Stats
    services::ForumService::instance().fetch_stats(
        [this](bool ok, services::ForumStats s) {
            if (!ok)
                return;
            sidebar_->set_stats(s);
            feed_->set_stats(s);
        });

    // My profile
    services::ForumService::instance().fetch_my_profile(
        [this](bool ok, services::ForumProfile p) {
            if (!ok)
                return;
            sidebar_->set_my_profile(p);
            feed_->set_profile(p);
        });

    // Categories
    services::ForumService::instance().fetch_categories(
        [this](bool ok, QVector<services::ForumCategory> cats,
               services::ForumPermissions) {
            if (!ok || cats.isEmpty())
                return;
            categories_ = cats;
            sidebar_->set_categories(cats);

            // Auto-select first category with posts
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
            sidebar_->set_active_category(first.id);
            feed_->set_header(first.name);
            feed_->set_categories(cats, first.id);

            services::ForumService::instance().fetch_posts(
                first.id, 1, "latest",
                [this, first](bool ok2, services::ForumPostsPage p) {
                    if (ok2)
                        feed_->set_posts(p, first.color);
                    else
                        feed_->set_loading(false);
                });
        });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Navigation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::navigate_back_to_feed() {
    main_stack_->setCurrentIndex(0);
}

void ForumScreen::on_category_selected(int id, const QString& name,
                                        const QString& color) {
    active_category_id_ = id;
    active_category_name_ = name;
    active_category_color_ = color;
    feed_->set_header(name);
    feed_->set_loading(true);
    feed_->clear_active();
    feed_->set_categories(categories_, id);
    sidebar_->set_active_category(id);
    main_stack_->setCurrentIndex(0);

    services::ForumService::instance().fetch_posts(
        id, 1, "latest", [this, color](bool ok, services::ForumPostsPage p) {
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
    main_stack_->setCurrentIndex(1);

    services::ForumService::instance().fetch_post(
        post.post_uuid, [this](bool ok, services::ForumPostDetail d) {
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
    active_category_color_ = ui::colors::CYAN;
    feed_->set_header("SEARCH: " + query);
    feed_->set_loading(true);
    feed_->clear_active();
    main_stack_->setCurrentIndex(0);

    services::ForumService::instance().search(
        query, 1, [this](bool ok, services::ForumPostsPage p) {
            if (ok)
                feed_->set_posts(p, ui::colors::CYAN);
            else
                feed_->set_loading(false);
        });
}

void ForumScreen::on_trending() {
    active_category_id_ = 0;
    active_category_color_ = ui::colors::AMBER;
    feed_->set_header("TRENDING");
    feed_->set_loading(true);
    feed_->clear_active();
    main_stack_->setCurrentIndex(0);

    services::ForumService::instance().fetch_trending(
        [this](bool ok, services::ForumPostsPage p) {
            if (ok)
                feed_->set_posts(p, ui::colors::AMBER);
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
    dlg->setMinimumSize(560, 400);
    dlg->setStyleSheet(
        QString("QDialog{background:%1;border:1px solid %2;}"
                "QLabel{color:%3;font-size:11px;background:transparent;"
                "font-family:'Consolas','Courier New',monospace;}"
                "QLineEdit,QTextEdit{background:%4;color:%5;"
                "border:1px solid %2;font-size:13px;"
                "font-family:'Consolas','Courier New',monospace;padding:8px 12px;}"
                "QLineEdit:focus,QTextEdit:focus{border-color:%6;}")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM,
                 ui::colors::TEXT_SECONDARY, ui::colors::BG_BASE,
                 ui::colors::TEXT_PRIMARY, ui::colors::BORDER_BRIGHT));
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(24, 20, 24, 18);
    vl->setSpacing(12);

    // Gradient accent bar
    auto* accent = new QWidget;
    accent->setFixedHeight(3);
    accent->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 %1,stop:0.6 %2,stop:1 transparent);")
            .arg(ui::colors::AMBER, ui::colors::ORANGE));
    vl->addWidget(accent);

    auto* hdr = new QLabel("CREATE NEW POST");
    hdr->setStyleSheet(
        QString("color:%1;font-size:15px;font-weight:700;letter-spacing:1.5px;%2")
            .arg(ui::colors::TEXT_PRIMARY, M(15)));
    vl->addWidget(hdr);

    auto* sub = new QLabel("Share your insights with the community");
    sub->setStyleSheet(
        QString("color:%1;font-size:11px;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(11)));
    vl->addWidget(sub);

    vl->addSpacing(4);

    auto* title_lbl = new QLabel("TITLE");
    title_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(10)));
    auto* title_edit = new QLineEdit;
    title_edit->setPlaceholderText("Give your post a descriptive title...");
    title_edit->setFixedHeight(36);

    auto* content_lbl = new QLabel("CONTENT");
    content_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;%2")
            .arg(ui::colors::TEXT_TERTIARY, M(10)));
    auto* body_edit = new QTextEdit;
    body_edit->setPlaceholderText("Write your thoughts...");

    vl->addWidget(title_lbl);
    vl->addWidget(title_edit);
    vl->addWidget(content_lbl);
    vl->addWidget(body_edit, 1);

    auto* btn_row = new QWidget;
    btn_row->setStyleSheet("background:transparent;");
    auto* btn_hl = new QHBoxLayout(btn_row);
    btn_hl->setContentsMargins(0, 4, 0, 0);
    btn_hl->setSpacing(10);

    auto* cancel = new QPushButton("CANCEL");
    cancel->setFixedHeight(32);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;"
                "border:1px solid %2;font-size:11px;font-weight:700;"
                "padding:0 20px;%3}"
                "QPushButton:hover{color:%4;border-color:%5;}")
            .arg(ui::colors::TEXT_TERTIARY, ui::colors::BORDER_DIM,
                 M(11), ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED));
    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* submit = new QPushButton("PUBLISH POST");
    submit->setFixedHeight(32);
    submit->setCursor(Qt::PointingHandCursor);
    submit->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;"
                "border:1px solid rgba(217,119,6,0.3);font-size:11px;"
                "font-weight:700;padding:0 20px;%2}"
                "QPushButton:hover{color:%3;"
                "border-color:rgba(217,119,6,0.6);"
                "background:rgba(217,119,6,0.2);}")
            .arg(ui::colors::TEXT_SECONDARY, M(11), ui::colors::AMBER));
    connect(submit, &QPushButton::clicked, this,
            [this, dlg, title_edit, body_edit, category_id]() {
                QString title = title_edit->text().trimmed();
                QString content = body_edit->toPlainText().trimmed();
                if (title.isEmpty() || content.isEmpty())
                    return;
                dlg->accept();
                services::ForumService::instance().create_post(
                    category_id, title, content,
                    [this](bool ok, const QString& msg) {
                        if (ok)
                            on_category_selected(active_category_id_,
                                                  active_category_name_,
                                                  active_category_color_);
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
    dlg->setMinimumSize(440, 320);
    dlg->setStyleSheet(
        QString("QDialog{background:%1;border:1px solid %2;}"
                "QLabel{color:%3;font-size:11px;background:transparent;"
                "font-family:'Consolas','Courier New',monospace;}"
                "QLineEdit{background:%4;color:%5;border:1px solid %2;"
                "font-size:12px;font-family:'Consolas','Courier New',monospace;"
                "padding:6px 12px;}"
                "QLineEdit:focus{border-color:%6;}")
            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM,
                 ui::colors::TEXT_SECONDARY, ui::colors::BG_BASE,
                 ui::colors::TEXT_PRIMARY, ui::colors::BORDER_BRIGHT));
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(24, 20, 24, 18);
    vl->setSpacing(10);

    // Gradient accent
    auto* accent = new QWidget;
    accent->setFixedHeight(3);
    accent->setStyleSheet(
        QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 %1,stop:0.6 %2,stop:1 transparent);")
            .arg(ui::colors::CYAN, ui::colors::AMBER));
    vl->addWidget(accent);

    auto* hdr = new QLabel("EDIT PROFILE");
    hdr->setStyleSheet(
        QString("color:%1;font-size:15px;font-weight:700;letter-spacing:1.5px;%2")
            .arg(ui::colors::TEXT_PRIMARY, M(15)));
    vl->addWidget(hdr);
    vl->addSpacing(4);

    auto mk = [&](const QString& lbl, const QString& val) -> QLineEdit* {
        auto* l = new QLabel(lbl);
        l->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;%2")
                .arg(ui::colors::TEXT_TERTIARY, M(10)));
        vl->addWidget(l);
        auto* e = new QLineEdit(val);
        e->setFixedHeight(30);
        vl->addWidget(e);
        return e;
    };
    auto* name_e = mk("DISPLAY NAME", profile.display_name);
    auto* bio_e = mk("BIO", profile.bio);
    auto* sig_e = mk("SIGNATURE", profile.signature);
    auto* col_e = mk("AVATAR COLOR (HEX)", profile.avatar_color);

    auto* btn_row = new QWidget;
    btn_row->setStyleSheet("background:transparent;");
    auto* bh = new QHBoxLayout(btn_row);
    bh->setContentsMargins(0, 4, 0, 0);
    bh->setSpacing(10);

    auto* cc = new QPushButton("CANCEL");
    cc->setFixedHeight(32);
    cc->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;"
                "border:1px solid %2;font-size:11px;font-weight:700;"
                "padding:0 20px;%3}"
                "QPushButton:hover{color:%4;}")
            .arg(ui::colors::TEXT_TERTIARY, ui::colors::BORDER_DIM,
                 M(11), ui::colors::TEXT_SECONDARY));
    connect(cc, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* sc = new QPushButton("SAVE CHANGES");
    sc->setFixedHeight(32);
    sc->setCursor(Qt::PointingHandCursor);
    sc->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;"
                "border:1px solid rgba(217,119,6,0.3);font-size:11px;"
                "font-weight:700;padding:0 20px;%2}"
                "QPushButton:hover{color:%3;"
                "border-color:rgba(217,119,6,0.6);"
                "background:rgba(217,119,6,0.2);}")
            .arg(ui::colors::TEXT_SECONDARY, M(11), ui::colors::AMBER));
    connect(sc, &QPushButton::clicked, this,
            [this, dlg, name_e, bio_e, sig_e, col_e]() {
                dlg->accept();
                services::ForumService::instance().update_profile(
                    name_e->text().trimmed(), bio_e->text().trimmed(),
                    sig_e->text().trimmed(), col_e->text().trimmed(),
                    [this](bool ok, const QString&) {
                        if (ok) {
                            services::ForumService::instance().fetch_my_profile(
                                [this](bool ok2, services::ForumProfile p) {
                                    if (!ok2)
                                        return;
                                    sidebar_->set_my_profile(p);
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
