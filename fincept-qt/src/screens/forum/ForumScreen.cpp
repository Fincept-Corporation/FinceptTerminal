// src/screens/forum/ForumScreen.cpp
#include "screens/forum/ForumScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/forum/ForumFeedPanel.h"
#include "screens/forum/ForumSidebarPanel.h"
#include "screens/forum/ForumThreadPanel.h"
#include "services/forum/ForumService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QMessageBox>
#include <QPointer>
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
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    });
    build_ui();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// build_ui — 3-column: sidebar | feed | thread (via splitter)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Splitter: sidebar | main content ──────────────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(QString("QSplitter{background:%1;}"
                                     "QSplitter::handle{background:%2;}")
                                 .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

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
    connect(sidebar_, &ForumSidebarPanel::category_selected, this, &ForumScreen::on_category_selected);

    connect(sidebar_, &ForumSidebarPanel::trending_clicked, this, &ForumScreen::on_trending);

    connect(sidebar_, &ForumSidebarPanel::search_requested, this, &ForumScreen::on_search);

    connect(sidebar_, &ForumSidebarPanel::new_post_requested, this, [this](int) { on_new_post_requested(); });

    // ── Wire feed signals ─────────────────────────────────────────────────────
    connect(feed_, &ForumFeedPanel::post_selected, this, &ForumScreen::on_post_selected);

    connect(feed_, &ForumFeedPanel::category_clicked, this, &ForumScreen::on_category_selected);

    // NOTE on lambda captures below: ForumService callbacks are plain
    // std::functions with no QObject context, so they are NOT auto-disconnected
    // when this screen is destroyed. Every one of them must hold a QPointer
    // guard, or a reply that lands after the screen closes dereferences a dead
    // `this`. (Several of these previously captured a raw [this].)
    connect(feed_, &ForumFeedPanel::load_more_requested, this, [this](int page) {
        feed_->set_loading(true);
        fetch_feed(page);
    });

    connect(feed_, &ForumFeedPanel::retry_requested, this, [this]() {
        if (categories_.isEmpty()) { // the forum itself never loaded — start over
            load_initial_data();
            return;
        }
        feed_->set_loading(true);
        fetch_feed(feed_page_);
    });

    connect(feed_, &ForumFeedPanel::new_post_clicked, this, [this]() { on_new_post_requested(); });

    connect(feed_, &ForumFeedPanel::vote_post_requested, this, [this](const QString& uuid, const QString& vtype) {
        if (votes_in_flight_.contains(uuid))
            return; // a vote for this post is already on its way
        votes_in_flight_.insert(uuid);
        QPointer<ForumScreen> self = this;
        services::ForumService::instance().vote_post(uuid, vtype, [self, uuid](bool ok, const QString& msg) {
            if (!self)
                return;
            self->votes_in_flight_.remove(uuid);
            if (!ok) {
                LOG_WARN("ForumScreen", "Vote failed: " + msg);
                QMessageBox::information(self, ForumScreen::tr("Vote not recorded"),
                                         msg.isEmpty() ? ForumScreen::tr("Your vote could not be recorded.") : msg);
                return;
            }
            // Reload the list the user is looking at (same view, same page).
            self->fetch_feed(self->feed_page_, /*show_error=*/false);
        });
    });

    // ── Wire thread signals ───────────────────────────────────────────────────
    connect(thread_, &ForumThreadPanel::back_requested, this, &ForumScreen::navigate_back_to_feed);

    connect(thread_, &ForumThreadPanel::comment_submitted, this, [this](const QString& uuid, const QString& content) {
        if (comment_in_flight_)
            return; // double-click / double-Enter would post the reply twice
        comment_in_flight_ = true;
        thread_->set_reply_busy(true);
        QPointer<ForumScreen> self = this;
        services::ForumService::instance().create_comment(uuid, content, [self, uuid](bool ok, const QString& msg) {
            if (!self)
                return;
            self->comment_in_flight_ = false;
            self->thread_->set_reply_busy(false);
            if (ok) {
                self->thread_->clear_reply_input(); // only clear once it actually posted
                // Reload in place (no loading page, so the thread keeps its
                // position) and then jump to the new reply.
                self->fetch_detail(uuid, /*initial=*/false, /*to_end=*/true);
            } else {
                LOG_WARN("ForumScreen", "Create comment failed: " + msg);
                QMessageBox::warning(self, ForumScreen::tr("Reply Failed"),
                                     msg.isEmpty() ? ForumScreen::tr("Could not post your reply. Please try again.")
                                                   : ForumScreen::tr("Could not post your reply: %1").arg(msg));
            }
        });
    });

    connect(thread_, &ForumThreadPanel::vote_post, this, [this](const QString& uuid, const QString& vtype) {
        if (votes_in_flight_.contains(uuid))
            return;
        votes_in_flight_.insert(uuid);
        QPointer<ForumScreen> self = this;
        services::ForumService::instance().vote_post(uuid, vtype, [self, uuid](bool ok, const QString& msg) {
            if (!self)
                return;
            self->votes_in_flight_.remove(uuid);
            if (!ok) {
                LOG_WARN("ForumScreen", "Vote failed: " + msg);
                QMessageBox::information(self, ForumScreen::tr("Vote not recorded"),
                                         msg.isEmpty() ? ForumScreen::tr("Your vote could not be recorded.") : msg);
                return;
            }
            self->fetch_detail(uuid, /*initial=*/false);
        });
    });

    connect(thread_, &ForumThreadPanel::vote_comment, this, [this](const QString& uuid, const QString& vtype) {
        if (votes_in_flight_.contains(uuid))
            return;
        votes_in_flight_.insert(uuid);
        QPointer<ForumScreen> self = this;
        services::ForumService::instance().vote_comment(uuid, vtype, [self, uuid](bool ok, const QString& msg) {
            if (!self)
                return;
            self->votes_in_flight_.remove(uuid);
            if (!ok) {
                LOG_WARN("ForumScreen", "Comment vote failed: " + msg);
                QMessageBox::information(self, ForumScreen::tr("Vote not recorded"),
                                         msg.isEmpty() ? ForumScreen::tr("Your vote could not be recorded.") : msg);
            }
            if (self->current_detail_uuid_.isEmpty())
                return;
            self->fetch_detail(self->current_detail_uuid_, /*initial=*/false);
        });
    });

    connect(thread_, &ForumThreadPanel::author_clicked, this, [this](const QString& username) {
        QPointer<ForumScreen> outer = this;
        services::ForumService::instance().fetch_profile(username, [outer](bool ok, services::ForumProfile profile) {
            if (!ok || !outer)
                return;
            ForumScreen* self = outer.data();
            // Profile popup. Freed after exec() returns — the previous version
            // leaked one QDialog per profile view for the screen's lifetime.
            auto* dlg = new QDialog(self);
            dlg->setWindowTitle(tr("USER PROFILE"));
            dlg->setFixedSize(380, 340);
            dlg->setStyleSheet(QString("QDialog{background:%1;border:1px solid %2;}"
                                       "QLabel{background:transparent;"
                                       "font-family:'Consolas','Courier New',monospace;}")
                                   .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
            auto* vl = new QVBoxLayout(dlg);
            vl->setContentsMargins(24, 20, 24, 18);
            vl->setSpacing(10);

            // Profile header with gradient accent
            auto* hdr = new QWidget(self);
            hdr->setFixedHeight(4);
            QString avc = services::forum_safe_color(profile.avatar_color, ui::colors::AMBER());
            hdr->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                                       "stop:0 %1,stop:0.5 %2,stop:1 transparent);")
                                   .arg(avc, ui::colors::AMBER()));
            vl->addWidget(hdr);

            // Avatar + name
            auto* top = new QWidget(self);
            top->setStyleSheet("background:transparent;");
            auto* th = new QHBoxLayout(top);
            th->setContentsMargins(0, 0, 0, 0);
            th->setSpacing(14);

            // Profile text is user-authored: PlainText so markup can't be rendered.
            auto* av = new QLabel(profile.display_name.left(2).toUpper());
            av->setTextFormat(Qt::PlainText);
            av->setFixedSize(48, 48);
            av->setAlignment(Qt::AlignCenter);
            av->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;"
                                      "background:%2;border-radius:24px;%3")
                                  .arg(ui::colors::BG_BASE(), avc, M(16)));

            auto* info = new QVBoxLayout;
            info->setSpacing(2);
            auto* nm = new QLabel(profile.display_name.toUpper());
            nm->setTextFormat(Qt::PlainText);
            nm->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;%2").arg(avc, M(16)));
            auto* un = new QLabel("@" + profile.username);
            un->setTextFormat(Qt::PlainText);
            un->setStyleSheet(QString("color:%1;font-size:11px;%2").arg(ui::colors::TEXT_SECONDARY(), M(11)));
            info->addWidget(nm);
            info->addWidget(un);
            th->addWidget(av);
            th->addLayout(info, 1);
            vl->addWidget(top);

            if (!profile.bio.isEmpty()) {
                auto* bio = new QLabel(profile.bio);
                bio->setTextFormat(Qt::PlainText);
                bio->setWordWrap(true);
                bio->setStyleSheet(
                    QString("color:%1;font-size:12px;font-style:italic;%2").arg(ui::colors::TEXT_TERTIARY(), M(12)));
                vl->addWidget(bio);
            }

            auto* sep = new QFrame;
            sep->setFixedHeight(1);
            sep->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
            vl->addWidget(sep);

            // Stats grid
            auto* grid = new QWidget(self);
            grid->setStyleSheet("background:transparent;");
            auto* gh = new QHBoxLayout(grid);
            gh->setContentsMargins(0, 0, 0, 0);
            gh->setSpacing(0);

            auto mk_stat = [&](const QString& val, const QString& lbl, const QString& col) {
                auto* cell = new QWidget(self);
                cell->setStyleSheet("background:transparent;");
                auto* cv = new QVBoxLayout(cell);
                cv->setContentsMargins(0, 8, 0, 8);
                cv->setSpacing(2);
                cv->setAlignment(Qt::AlignCenter);
                auto* v = new QLabel(val);
                v->setAlignment(Qt::AlignCenter);
                v->setStyleSheet(QString("color:%1;font-size:18px;font-weight:700;%2").arg(col, M(18)));
                auto* l = new QLabel(lbl);
                l->setAlignment(Qt::AlignCenter);
                l->setStyleSheet(
                    QString("color:%1;font-size:9px;letter-spacing:1px;%2").arg(ui::colors::TEXT_TERTIARY(), M(9)));
                cv->addWidget(v);
                cv->addWidget(l);
                return cell;
            };

            gh->addWidget(mk_stat(QString::number(profile.reputation), tr("REP"), ui::colors::AMBER()));
            gh->addWidget(mk_stat(QString::number(profile.posts_count), tr("POSTS"), ui::colors::TEXT_PRIMARY()));
            gh->addWidget(mk_stat(QString::number(profile.comments_count), tr("REPLIES"), ui::colors::CYAN()));
            gh->addWidget(mk_stat(QString::number(profile.likes_received), tr("LIKES"), ui::colors::POSITIVE()));
            vl->addWidget(grid);

            if (profile.is_own_profile) {
                auto* edit_btn = new QPushButton(tr("EDIT MY PROFILE"));
                edit_btn->setFixedHeight(32);
                edit_btn->setCursor(Qt::PointingHandCursor);
                edit_btn->setStyleSheet(
                    QString("QPushButton{background:rgba(217,119,6,0.08);"
                            "color:%1;border:1px solid %2;"
                            "font-size:11px;font-weight:700;%3}"
                            "QPushButton:hover{color:%4;"
                            "border-color:rgba(217,119,6,0.5);"
                            "background:rgba(217,119,6,0.15);}")
                        .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), M(11), ui::colors::AMBER()));
                QObject::connect(edit_btn, &QPushButton::clicked, self, [outer, dlg, profile]() {
                    dlg->accept();
                    if (outer)
                        outer->show_edit_profile_dialog(profile);
                });
                vl->addStretch();
                vl->addWidget(edit_btn);
            } else {
                vl->addStretch();
            }
            // The dialog is a child of the screen: if the screen is torn down while
            // it is open, exec() returns with `dlg` already destroyed.
            QPointer<QDialog> dlg_guard(dlg);
            dlg->exec();
            if (dlg_guard)
                dlg_guard->deleteLater(); // exec() returns on accept/reject; free the dialog
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
    QPointer<ForumScreen> self1 = this;
    services::ForumService::instance().fetch_stats([self1](bool ok, services::ForumStats s) {
        if (!ok || !self1)
            return;
        self1->sidebar_->set_stats(s);
        self1->feed_->set_stats(s);
    });

    // My profile
    QPointer<ForumScreen> self2 = this;
    services::ForumService::instance().fetch_my_profile([self2](bool ok, services::ForumProfile p) {
        if (!ok || !self2)
            return;
        self2->sidebar_->set_my_profile(p);
        self2->feed_->set_profile(p);
    });

    // Categories
    QPointer<ForumScreen> self3 = this;
    services::ForumService::instance().fetch_categories(
        [self3](bool ok, QVector<services::ForumCategory> cats, services::ForumPermissions) {
            if (!self3)
                return;
            if (!ok || cats.isEmpty()) {
                // The feed sat on an animated skeleton forever when the forum
                // API was unreachable. Stop it and say what happened (with a
                // Retry) rather than the misleading "no discussions yet".
                self3->feed_->set_error(ForumScreen::tr("Could not load the forum. Check your connection and try again."));
                self3->initial_load_done_ = false; // let the next show / Retry start over
                return;
            }
            self3->categories_ = cats;
            self3->sidebar_->set_categories(cats);

            // Keep a category restored from saved state (restore_state() may have
            // run before this reply arrived); otherwise auto-select the first
            // category with posts. The restored selection used to be overwritten
            // here.
            int sel_idx = -1;
            for (int i = 0; i < cats.size(); ++i) {
                if (cats[i].id == self3->active_category_id_) {
                    sel_idx = i;
                    break;
                }
            }
            if (sel_idx < 0) {
                sel_idx = 0;
                for (int i = 0; i < cats.size(); ++i) {
                    if (cats[i].post_count > 0) {
                        sel_idx = i;
                        break;
                    }
                }
            }
            const auto& sel = cats[sel_idx];
            self3->active_category_id_ = sel.id;
            self3->active_category_name_ = sel.name;
            self3->active_category_color_ = sel.color;
            self3->feed_kind_ = FeedKind::Category;
            self3->sidebar_->set_active_category(sel.id);
            self3->feed_->set_header(sel.name);
            self3->feed_->set_categories(cats, sel.id);

            // Always (re)fetch: it supersedes a restore-time request that may have
            // finished before the skeleton above was shown.
            self3->fetch_feed(1);
        });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Navigation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::navigate_back_to_feed() {
    main_stack_->setCurrentIndex(0);
}

void ForumScreen::on_category_selected(int id, const QString& name, const QString& color) {
    feed_kind_ = FeedKind::Category;
    active_category_id_ = id;
    active_category_name_ = name;
    active_category_color_ = color;
    current_detail_uuid_.clear();
    ScreenStateManager::instance().notify_changed(this);
    feed_->set_header(name);
    feed_->set_loading(true);
    feed_->clear_active();
    feed_->set_categories(categories_, id);
    sidebar_->set_active_category(id);
    main_stack_->setCurrentIndex(0);
    fetch_feed(1);
}

void ForumScreen::on_post_selected(const services::ForumPost& post) {
    current_detail_uuid_ = post.post_uuid;
    ScreenStateManager::instance().notify_changed(this);
    feed_->set_active_post(post.post_uuid);
    thread_->set_loading(true);
    main_stack_->setCurrentIndex(1);
    fetch_detail(post.post_uuid, /*initial=*/true);
}

void ForumScreen::on_search(const QString& query) {
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        // Enter on an emptied search box: leave search mode, back to the category.
        if (feed_kind_ == FeedKind::Search && active_category_id_ > 0)
            on_category_selected(active_category_id_, active_category_name_, active_category_color_);
        return;
    }
    // The category selection is kept (so "new post" and saved state still point
    // at it); only the list being shown changes.
    feed_kind_ = FeedKind::Search;
    feed_query_ = q;
    sidebar_->set_active_category(0);
    feed_->set_header(tr("SEARCH: %1").arg(q));
    feed_->set_loading(true);
    feed_->clear_active();
    main_stack_->setCurrentIndex(0);
    fetch_feed(1);
}

void ForumScreen::on_trending() {
    feed_kind_ = FeedKind::Trending;
    sidebar_->set_active_category(0);
    feed_->set_header(tr("TRENDING"));
    feed_->set_loading(true);
    feed_->clear_active();
    main_stack_->setCurrentIndex(0);
    fetch_feed(1);
}

void ForumScreen::on_new_post_requested() {
    // Trending / search have no category of their own: default to the last
    // selected one, else the first (the dialog lets the user change it). It used
    // to post into a hard-coded category id 1.
    int cat = active_category_id_;
    if (cat <= 0 && !categories_.isEmpty())
        cat = categories_.first().id;
    if (cat <= 0) { // categories not loaded yet (or the forum is unreachable)
        QMessageBox::information(this, tr("Forum not ready"),
                                 tr("The forum categories have not loaded yet. Try again in a moment."));
        return;
    }
    show_new_post_dialog(cat);
}

void ForumScreen::fetch_feed(int page, bool show_error) {
    const int seq = ++feed_seq_;
    feed_page_ = page;
    QString color;
    switch (feed_kind_) {
        case FeedKind::Category:
            color = active_category_color_;
            break;
        case FeedKind::Trending:
            color = ui::colors::AMBER();
            break;
        case FeedKind::Search:
            color = ui::colors::CYAN();
            break;
    }
    QPointer<ForumScreen> self = this;
    auto apply = [self, seq, color, show_error](bool ok, services::ForumPostsPage p) {
        if (!self || seq != self->feed_seq_)
            return; // superseded by a newer request
        if (ok) {
            self->feed_->set_posts(p, color);
        } else if (show_error) {
            self->feed_->set_error(ForumScreen::tr("Could not load posts. Check your connection and try again."));
        } else {
            self->feed_->set_loading(false);
        }
    };
    auto& svc = services::ForumService::instance();
    switch (feed_kind_) {
        case FeedKind::Category:
            if (active_category_id_ <= 0) {
                feed_->set_loading(false);
                return;
            }
            svc.fetch_posts(active_category_id_, page, "latest", apply);
            break;
        case FeedKind::Trending:
            svc.fetch_trending(apply);
            break;
        case FeedKind::Search:
            svc.search(feed_query_, page, apply);
            break;
    }
}

void ForumScreen::fetch_detail(const QString& uuid, bool initial, bool to_end) {
    const int seq = ++detail_seq_;
    QPointer<ForumScreen> self = this;
    services::ForumService::instance().fetch_post(
        uuid, [self, seq, initial, to_end](bool ok, services::ForumPostDetail d) {
            if (!self || seq != self->detail_seq_)
                return; // the user has opened a different thread since
            if (ok) {
                self->thread_->show_post(d);
                if (to_end)
                    self->thread_->scroll_to_end();
            } else if (initial) {
                self->thread_->show_load_error();
            }
        });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Dialogs
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void ForumScreen::show_new_post_dialog(int category_id) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("NEW POST"));
    dlg->setMinimumSize(560, 400);
    dlg->setStyleSheet(QString("QDialog{background:%1;border:1px solid %2;}"
                               "QLabel{color:%3;font-size:11px;background:transparent;"
                               "font-family:'Consolas','Courier New',monospace;}"
                               "QLineEdit,QTextEdit,QComboBox{background:%4;color:%5;"
                               "border:1px solid %2;font-size:13px;"
                               "font-family:'Consolas','Courier New',monospace;padding:8px 12px;}"
                               "QLineEdit:focus,QTextEdit:focus,QComboBox:focus{border-color:%6;}"
                               "QLabel#forumPostFieldLabel{color:%7;font-size:10px;font-weight:700;"
                               "letter-spacing:1px;}")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                                ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_BRIGHT(),
                                ui::colors::TEXT_TERTIARY()));
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(24, 20, 24, 18);
    vl->setSpacing(12);

    // Gradient accent bar
    auto* accent = new QWidget(this);
    accent->setFixedHeight(3);
    accent->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                                  "stop:0 %1,stop:0.6 %2,stop:1 transparent);")
                              .arg(ui::colors::AMBER(), ui::colors::ORANGE()));
    vl->addWidget(accent);

    auto* hdr = new QLabel(tr("CREATE NEW POST"));
    hdr->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;letter-spacing:1.5px;%2")
                           .arg(ui::colors::TEXT_PRIMARY(), M(15)));
    vl->addWidget(hdr);

    auto* sub = new QLabel(tr("Share your insights with the community"));
    sub->setStyleSheet(QString("color:%1;font-size:11px;%2").arg(ui::colors::TEXT_TERTIARY(), M(11)));
    vl->addWidget(sub);

    vl->addSpacing(4);

    // Category picker — posting used to go to whichever category happened to be
    // active (or a hard-coded id 1 from the trending / search views).
    QComboBox* cat_combo = nullptr;
    if (!categories_.isEmpty()) {
        auto* cat_lbl = new QLabel(tr("CATEGORY"));
        cat_lbl->setObjectName("forumPostFieldLabel"); // styled by the dialog's sheet
        cat_combo = new QComboBox;
        int sel = 0;
        for (int i = 0; i < categories_.size(); ++i) {
            cat_combo->addItem(categories_[i].name, categories_[i].id);
            if (categories_[i].id == category_id)
                sel = i;
        }
        cat_combo->setCurrentIndex(sel);
        cat_combo->setAccessibleName(tr("Post category"));
        vl->addWidget(cat_lbl);
        vl->addWidget(cat_combo);
    }

    auto* title_lbl = new QLabel(tr("TITLE"));
    title_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;%2")
                                 .arg(ui::colors::TEXT_TERTIARY(), M(10)));
    auto* title_edit = new QLineEdit;
    title_edit->setPlaceholderText(tr("Give your post a descriptive title..."));
    title_edit->setFixedHeight(36);

    auto* content_lbl = new QLabel(tr("CONTENT"));
    content_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;%2")
                                   .arg(ui::colors::TEXT_TERTIARY(), M(10)));
    auto* body_edit = new QTextEdit;
    body_edit->setPlaceholderText(tr("Write your thoughts..."));

    vl->addWidget(title_lbl);
    vl->addWidget(title_edit);
    vl->addWidget(content_lbl);
    vl->addWidget(body_edit, 1);

    auto* btn_row = new QWidget(this);
    btn_row->setStyleSheet("background:transparent;");
    auto* btn_hl = new QHBoxLayout(btn_row);
    btn_hl->setContentsMargins(0, 4, 0, 0);
    btn_hl->setSpacing(10);

    auto* cancel = new QPushButton(tr("CANCEL"));
    cancel->setFixedHeight(32);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QString("QPushButton{background:transparent;color:%1;"
                                  "border:1px solid %2;font-size:11px;font-weight:700;"
                                  "padding:0 20px;%3}"
                                  "QPushButton:hover{color:%4;border-color:%5;}")
                              .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), M(11),
                                   ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED()));
    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* submit = new QPushButton(tr("PUBLISH POST"));
    submit->setFixedHeight(32);
    submit->setCursor(Qt::PointingHandCursor);
    submit->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;"
                                  "border:1px solid rgba(217,119,6,0.3);font-size:11px;"
                                  "font-weight:700;padding:0 20px;%2}"
                                  "QPushButton:hover{color:%3;"
                                  "border-color:rgba(217,119,6,0.6);"
                                  "background:rgba(217,119,6,0.2);}")
                              .arg(ui::colors::TEXT_SECONDARY(), M(11), ui::colors::AMBER()));
    connect(submit, &QPushButton::clicked, this, [this, dlg, title_edit, body_edit, submit, category_id, cat_combo]() {
        QString title = title_edit->text().trimmed();
        QString content = body_edit->toPlainText().trimmed();
        // Silently doing nothing made PUBLISH look broken; say what's missing.
        if (title.isEmpty()) {
            QMessageBox::information(dlg, tr("Title required"), tr("Give your post a title."));
            title_edit->setFocus();
            return;
        }
        if (content.isEmpty()) {
            QMessageBox::information(dlg, tr("Content required"), tr("Write something in the body of your post."));
            body_edit->setFocus();
            return;
        }
        // Keep the dialog (and the user's typed post) open until the async call
        // returns. Only close on success; on failure re-enable Publish and show
        // the error so the text isn't lost. The old code accept()ed (discarding
        // the text) BEFORE the call and merely LOG_WARN'd on failure.
        submit->setEnabled(false);
        submit->setText(tr("PUBLISHING…"));
        const int target_id = cat_combo ? cat_combo->currentData().toInt() : category_id;
        QPointer<ForumScreen> self = this;
        QPointer<QDialog> dlg_guard(dlg);
        QPointer<QPushButton> submit_guard(submit);
        services::ForumService::instance().create_post(
            target_id, title, content, [self, dlg_guard, submit_guard, target_id](bool ok, const QString& msg) {
                if (ok) {
                    if (dlg_guard)
                        dlg_guard->accept();
                    if (!self)
                        return;
                    // Show the category the post went to (it may differ from the
                    // one that was open).
                    for (const auto& c : std::as_const(self->categories_)) {
                        if (c.id == target_id) {
                            self->on_category_selected(c.id, c.name, c.color);
                            return;
                        }
                    }
                    self->on_category_selected(self->active_category_id_, self->active_category_name_,
                                               self->active_category_color_);
                    return;
                }
                LOG_WARN("ForumScreen", "Create post failed: " + msg);
                if (submit_guard) {
                    submit_guard->setEnabled(true);
                    submit_guard->setText(ForumScreen::tr("PUBLISH POST"));
                }
                if (dlg_guard)
                    QMessageBox::warning(dlg_guard, ForumScreen::tr("Post Failed"),
                                         msg.isEmpty() ? ForumScreen::tr("Could not publish your post. Please try again.")
                                                       : ForumScreen::tr("Could not publish your post: %1").arg(msg));
            });
    });

    btn_hl->addStretch();
    btn_hl->addWidget(cancel);
    btn_hl->addWidget(submit);
    vl->addWidget(btn_row);

    title_edit->setAccessibleName(tr("Post title"));
    body_edit->setAccessibleName(tr("Post content"));
    dlg->setTabOrder(title_edit, body_edit);
    dlg->setTabOrder(body_edit, submit);
    dlg->setTabOrder(submit, cancel);
    title_edit->setFocus();

    QPointer<QDialog> dlg_guard(dlg); // see the profile dialog: the screen may be gone after exec()
    dlg->exec();
    if (dlg_guard)
        dlg_guard->deleteLater(); // otherwise one QDialog leaks per new-post attempt
}

void ForumScreen::show_edit_profile_dialog(const services::ForumProfile& profile) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("EDIT PROFILE"));
    dlg->setMinimumSize(440, 320);
    dlg->setStyleSheet(QString("QDialog{background:%1;border:1px solid %2;}"
                               "QLabel{color:%3;font-size:11px;background:transparent;"
                               "font-family:'Consolas','Courier New',monospace;}"
                               "QLineEdit{background:%4;color:%5;border:1px solid %2;"
                               "font-size:12px;font-family:'Consolas','Courier New',monospace;"
                               "padding:6px 12px;}"
                               "QLineEdit:focus{border-color:%6;}")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                                ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_BRIGHT()));
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(24, 20, 24, 18);
    vl->setSpacing(10);

    // Gradient accent
    auto* accent = new QWidget(this);
    accent->setFixedHeight(3);
    accent->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                                  "stop:0 %1,stop:0.6 %2,stop:1 transparent);")
                              .arg(ui::colors::CYAN(), ui::colors::AMBER()));
    vl->addWidget(accent);

    auto* hdr = new QLabel(tr("EDIT PROFILE"));
    hdr->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;letter-spacing:1.5px;%2")
                           .arg(ui::colors::TEXT_PRIMARY(), M(15)));
    vl->addWidget(hdr);
    vl->addSpacing(4);

    auto mk = [&](const QString& lbl, const QString& val) -> QLineEdit* {
        auto* l = new QLabel(lbl);
        l->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;%2")
                             .arg(ui::colors::TEXT_TERTIARY(), M(10)));
        vl->addWidget(l);
        auto* e = new QLineEdit(val);
        e->setFixedHeight(30);
        vl->addWidget(e);
        return e;
    };
    auto* name_e = mk(tr("DISPLAY NAME"), profile.display_name);
    auto* bio_e = mk(tr("BIO"), profile.bio);
    auto* sig_e = mk(tr("SIGNATURE"), profile.signature);
    auto* col_e = mk(tr("AVATAR COLOR (HEX)"), profile.avatar_color);

    auto* btn_row = new QWidget(this);
    btn_row->setStyleSheet("background:transparent;");
    auto* bh = new QHBoxLayout(btn_row);
    bh->setContentsMargins(0, 4, 0, 0);
    bh->setSpacing(10);

    auto* cc = new QPushButton(tr("CANCEL"));
    cc->setFixedHeight(32);
    cc->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;"
                "border:1px solid %2;font-size:11px;font-weight:700;"
                "padding:0 20px;%3}"
                "QPushButton:hover{color:%4;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), M(11), ui::colors::TEXT_SECONDARY()));
    connect(cc, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* sc = new QPushButton(tr("SAVE CHANGES"));
    sc->setFixedHeight(32);
    sc->setCursor(Qt::PointingHandCursor);
    sc->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;"
                              "border:1px solid rgba(217,119,6,0.3);font-size:11px;"
                              "font-weight:700;padding:0 20px;%2}"
                              "QPushButton:hover{color:%3;"
                              "border-color:rgba(217,119,6,0.6);"
                              "background:rgba(217,119,6,0.2);}")
                          .arg(ui::colors::TEXT_SECONDARY(), M(11), ui::colors::AMBER()));
    connect(sc, &QPushButton::clicked, this, [this, dlg, name_e, bio_e, sig_e, col_e]() {
        // Validate before closing, so a typo doesn't lose the other fields.
        if (name_e->text().trimmed().isEmpty()) {
            QMessageBox::information(dlg, tr("Display name required"), tr("Enter a display name."));
            name_e->setFocus();
            return;
        }
        const QString color_text = col_e->text().trimmed();
        if (!color_text.isEmpty() && services::forum_safe_color(color_text, QString()).isEmpty()) {
            QMessageBox::information(dlg, tr("Invalid color"),
                                     tr("The avatar color must be a hex value such as #d97706."));
            col_e->setFocus();
            return;
        }
        dlg->accept();
        QPointer<ForumScreen> self = this;
        services::ForumService::instance().update_profile(
            name_e->text().trimmed(), bio_e->text().trimmed(), sig_e->text().trimmed(), col_e->text().trimmed(),
            [self](bool ok, const QString& msg) {
                if (!self)
                    return;
                if (!ok) {
                    // Previously the failure path was completely silent — the
                    // dialog closed and the user assumed the edit had saved.
                    QMessageBox::warning(self, ForumScreen::tr("Profile not saved"),
                                         msg.isEmpty() ? ForumScreen::tr("Your profile changes could not be saved.")
                                                       : ForumScreen::tr("Your profile changes could not be "
                                                                         "saved:\n%1")
                                                             .arg(msg));
                    return;
                }
                services::ForumService::instance().fetch_my_profile([self](bool ok2, services::ForumProfile p) {
                    if (self && ok2)
                        self->sidebar_->set_my_profile(p);
                });
            });
    });

    bh->addStretch();
    bh->addWidget(cc);
    bh->addWidget(sc);
    vl->addStretch();
    vl->addWidget(btn_row);
    QPointer<QDialog> dlg_guard(dlg); // the screen may be gone after exec()
    dlg->exec();
    if (dlg_guard)
        dlg_guard->deleteLater(); // otherwise one QDialog leaks per profile edit
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap ForumScreen::save_state() const {
    QVariantMap state{
        {"category_id", active_category_id_},
        {"category_name", active_category_name_},
        {"category_color", active_category_color_},
        {"detail_uuid", current_detail_uuid_},
    };
    if (thread_)
        state["reply_draft"] = thread_->reply_draft();
    return state;
}

void ForumScreen::restore_state(const QVariantMap& state) {
    const int cat_id = state.value("category_id", 0).toInt();
    const QString cat_name = state.value("category_name").toString();
    const QString cat_color = state.value("category_color").toString();
    const QString uuid = state.value("detail_uuid").toString();

    if (cat_id > 0 && !cat_name.isEmpty()) {
        // Trigger normal category-load flow (also sets active_category_*)
        on_category_selected(cat_id, cat_name, cat_color.isEmpty() ? ui::colors::INFO() : cat_color);
    }
    // Individual post detail can't be re-fetched without a full ForumPost object;
    // we record uuid for reference but leave the feed view active on restore.
    current_detail_uuid_ = uuid;
    if (thread_ && state.contains("reply_draft"))
        thread_->set_reply_draft(state.value("reply_draft").toString());
}

} // namespace fincept::screens
