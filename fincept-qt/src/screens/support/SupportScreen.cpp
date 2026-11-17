// src/screens/support/SupportScreen.cpp
//
// Core lifecycle: status/priority color and label helpers, ctor, apply_styles.
// Style helpers (SS_*, lbl, hsep, status_badge) live in SupportScreen_internal.h.
// Other concerns:
//   - SupportScreen_Layout.cpp   — build_sidebar / empty / create / detail
//   - SupportScreen_Actions.cpp  — load_tickets + create/send/close/reopen
#include "screens/support/SupportScreen.h"

#include "auth/AuthManager.h"
#include "auth/UserApi.h"
#include "screens/common/CommandRows.h"
#include "screens/support/SupportScreen_internal.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::screens::support_internal;

namespace {

// Which account the tickets on screen belong to: the e-mail, else the user id,
// else "~" (signed in, identity not known yet).
QString support_account_key(const auth::UserProfile& u) {
    const QString email = u.email.trimmed().toLower();
    if (!email.isEmpty())
        return email;
    return u.id > 0 ? QStringLiteral("id:%1").arg(u.id) : QStringLiteral("~");
}

} // namespace

QString SupportScreen::status_color(const QString& s) {
    QString l = s.toLower();
    if (l == "open" || l == "in_progress")
        return ui::colors::CYAN();
    // State colours (SUCCESS / WARNING / DANGER), not the price-direction pair;
    // the status text is always shown next to the colour.
    if (l == "resolved" || l == "closed")
        return ui::colors::SUCCESS();
    if (l == "pending")
        return ui::colors::WARNING();
    return ui::colors::TEXT_SECONDARY();
}

QString SupportScreen::priority_color(const QString& p) {
    QString l = p.toLower();
    if (l == "high" || l == "urgent")
        return ui::colors::DANGER();
    if (l == "medium")
        return ui::colors::WARNING();
    if (l == "low")
        return ui::colors::INFO();
    return ui::colors::TEXT_SECONDARY();
}

QString SupportScreen::status_label(const QString& s) {
    QString l = s.toLower();
    // Static method — qualify tr() through the class so lupdate ties the strings
    // to the SupportScreen translation context.
    if (l == "open")
        return tr("OPEN");
    if (l == "in_progress")
        return tr("IN PROGRESS");
    if (l == "resolved")
        return tr("RESOLVED");
    if (l == "closed")
        return tr("CLOSED");
    if (l == "pending")
        return tr("PENDING");
    return s.toUpper();
}

// ── Constructor ───────────────────────────────────────────────────────────────

SupportScreen::SupportScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    {
        auto* bar = new QWidget(this);
        bar->setFixedHeight(42);
        bar->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* bl = new QHBoxLayout(bar);
        bl->setContentsMargins(20, 0, 20, 0);
        bl->setSpacing(10);

        // Title (text set in retranslateUi)
        title_lbl_ = lbl(QString(), ui::colors::TEXT_PRIMARY(), 14, true);
        bl->addWidget(title_lbl_);

        // Breadcrumb separator + sub-label (text set in retranslateUi)
        bl->addWidget(lbl("/", ui::colors::BORDER_MED(), 13));
        breadcrumb_lbl_ = lbl(QString(), ui::colors::TEXT_TERTIARY(), 12);
        bl->addWidget(breadcrumb_lbl_);

        bl->addStretch();

        // Status indicator (text set in retranslateUi / set_busy). Colour follows
        // the "tone" property (support_set_tone) — the sheet is set once here.
        status_dot_ = status_label_widget(QStringLiteral("●"), -4, false);
        bl->addWidget(status_dot_);
        status_lbl_ = status_label_widget(QString(), -3, true);
        bl->addWidget(status_lbl_);

        // Separator
        auto* vsep = new QFrame;
        vsep->setFrameShape(QFrame::VLine);
        vsep->setFixedWidth(1);
        vsep->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
        bl->addWidget(vsep);

        refresh_btn_ = new QPushButton("↻");
        refresh_btn_->setMinimumSize(30, 26);
        refresh_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        refresh_btn_->setStyleSheet(SS_BTN_GHOST());
        connect(refresh_btn_, &QPushButton::clicked, this, &SupportScreen::load_tickets);
        bl->addWidget(refresh_btn_);

        root->addWidget(bar);
    }

    // ── Main splitter ─────────────────────────────────────────────────────────
    // Placed in the screen before it is filled: moving finished panels into a
    // new parent re-computes the stylesheet of every widget in them.
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(ui::colors::BORDER_DIM()));
    root->addWidget(splitter_, 1);

    splitter_->addWidget(build_sidebar());

    // Texts of the bars and the sidebar. Called before content_stack_ exists:
    // with the stack in place retranslateUi() rebuilds all three content
    // pages (for a language switch) — at construction that built them twice.
    retranslateUi();

    // Right: stacked content
    content_stack_ = new QStackedWidget(splitter_);
    splitter_->addWidget(content_stack_);
    content_stack_->addWidget(build_empty_state()); // 0
    content_stack_->addWidget(build_create_page()); // 1
    content_stack_->addWidget(build_detail_page()); // 2

    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({300, 700});

    // ── Theme wiring ──────────────────────────────────────────────────────────
    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled, and only the sheets whose text changed (the theme
    // manager has usually translated them already).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            [this]() { apply_styles(); });

    // ── Account wiring ────────────────────────────────────────────────────────
    // Nothing is requested while signed out (the requirement page shows instead);
    // signing in starts the first load, signing out / another account drops the
    // tickets on screen.
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &SupportScreen::apply_auth_gate);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::loading_changed, this,
            [this](bool) { apply_auth_gate(); });
    apply_auth_gate();
}

void SupportScreen::start_initial_load() {
    tickets_loaded_ = false;
    load_failed_ = false;
    // The ticket list does not depend on the categories: both go out at once, so
    // the sidebar fills as soon as its own answer arrives.
    load_tickets();

    // `this` is passed as the context so the reply handler is dropped if the
    // screen is destroyed first.
    auth::UserApi::instance().get_support_categories(
        [this](auth::ApiResponse r) {
            if (r.success) {
                // GET /support/categories answers {"ticket_categories":[...],
                // "feedback_categories":[...], "priorities":[...]}. Only
                // "categories" / "items" used to be read, so the server list was
                // never applied and three of the hard-coded fallback categories
                // (feature_request / account / other) were rejected by the API.
                QJsonArray cats;
                auto payload = r.data;
                if (payload.contains("data") && payload["data"].isObject())
                    payload = payload["data"].toObject();
                for (const char* key : {"ticket_categories", "categories", "items"}) {
                    if (payload.value(QLatin1String(key)).isArray()) {
                        cats = payload.value(QLatin1String(key)).toArray();
                        break;
                    }
                }

                if (!cats.isEmpty()) {
                    server_categories_.clear();
                    for (const auto& v : cats) {
                        const QString cat = v.isString() ? v.toString() : v.toObject()["name"].toString();
                        if (cat.isEmpty())
                            continue;
                        QString display;
                        if (cat == QLatin1String("technical"))
                            display = tr("Technical");
                        else if (cat == QLatin1String("billing"))
                            display = tr("Billing");
                        else if (cat == QLatin1String("general"))
                            display = tr("General");
                        else if (cat == QLatin1String("bug_report"))
                            display = tr("Bug Report");
                        else
                            display = cat.at(0).toUpper() + QString(cat.mid(1)).replace('_', ' ');
                        server_categories_.append({display, cat});
                    }
                    populate_category_combo();
                }
            }
        },
        this);
}

void SupportScreen::reset_tickets() {
    ++account_generation_;
    all_tickets_ = QJsonArray();
    tickets_loaded_ = false;
    load_failed_ = false;
    last_error_.clear();
    last_load_ms_ = 0;
    selected_ticket_id_ = -1;
    selected_is_closed_ = false;
    open_after_load_id_ = -1;
    list_in_flight_ = false; // a reply for the previous account is dropped on arrival
    if (status_lbl_)
        set_busy(false);
    if (stat_total_)
        stat_total_->setText(QStringLiteral("0"));
    if (stat_open_)
        stat_open_->setText(QStringLiteral("0"));
    if (stat_resolved_)
        stat_resolved_->setText(QStringLiteral("0"));
    // Text typed for the previous account must not be sent from the next one.
    if (subject_input_)
        subject_input_->clear();
    if (desc_input_)
        desc_input_->clear();
    if (msg_input_)
        msg_input_->clear();
    if (messages_container_)
        clear_layout_keep_stretch(qobject_cast<QVBoxLayout*>(messages_container_->layout()));
    if (content_stack_)
        content_stack_->setCurrentIndex(0);
    rebuild_ticket_rows();
    update_home_page();
}

void SupportScreen::apply_auth_gate() {
    const auto& am = auth::AuthManager::instance();
    const ui::RequirementStatus st = ui::check_requirements({ui::Requirement::signed_in()});
    const bool signed_in = st.ok;
    const QString key = signed_in ? support_account_key(am.session().user_info) : QString();

    if (key != account_key_) {
        // The identity of the same sign-in arriving later is not a new account.
        const bool same_sign_in = signed_in && account_key_ == QLatin1String("~");
        // Signed out, or another account: nothing of the previous one stays.
        if (!account_key_.isEmpty() && !same_sign_in)
            reset_tickets();
        const bool start = signed_in && !same_sign_in;
        account_key_ = key;
        if (start)
            start_initial_load();
    }

    if (splitter_)
        splitter_->setVisible(signed_in);
    if (refresh_btn_)
        refresh_btn_->setVisible(signed_in);

    if (signed_in) {
        if (signin_state_)
            signin_state_->hide();
        if (signin_skeleton_)
            signin_skeleton_->hide();
        return;
    }

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    // A saved sign-in is still being checked: not "signed out" yet.
    if (am.is_loading()) {
        if (!signin_skeleton_) {
            signin_skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Cards, this);
            signin_skeleton_->set_message(tr("Checking your sign-in…"));
            if (root)
                root->addWidget(signin_skeleton_, 1);
        }
        if (signin_state_)
            signin_state_->hide();
        signin_skeleton_->show();
        if (status_lbl_) {
            status_lbl_->setText(tr("Checking sign-in…"));
            support_set_tone(status_lbl_, "busy");
            support_set_tone(status_dot_, "busy");
        }
        return;
    }

    if (!signin_state_) {
        signin_state_ = new ui::EmptyState(this);
        // SIGN IN: the sign-in screen (open_setup_link "login"; it does nothing while a
        // sign-in check is still running).
        connect(signin_state_, &ui::EmptyState::primary_clicked, this, [this]() {
            const ui::RequirementStatus now = ui::check_requirements({ui::Requirement::signed_in()});
            if (!now.ok && now.link.valid())
                ui::open_setup_link(now.link, this);
        });
        connect(signin_state_, &ui::EmptyState::secondary_clicked, this, []() {
            QDesktopServices::openUrl(QUrl(QStringLiteral("mailto:support@fincept.in")));
        });
        if (root)
            root->addWidget(signin_state_, 1);
    }
    ui::EmptyState::Spec spec = ui::EmptyState::needs_setup_spec(st);
    spec.message += QLatin1Char(' ') + tr("Support tickets belong to your Fincept account. Without an account you "
                                          "can e-mail support@fincept.in.");
    spec.secondary = tr("E-MAIL SUPPORT");
    spec.secondary_tooltip = QStringLiteral("support@fincept.in");
    signin_state_->apply(spec);
    if (signin_skeleton_)
        signin_skeleton_->hide();
    signin_state_->show();
    if (status_lbl_) {
        status_lbl_->setText(tr("Signed out"));
        support_set_tone(status_lbl_, "dim");
        support_set_tone(status_dot_, "dim");
    }
}

void SupportScreen::populate_category_combo() {
    if (!category_combo_ || server_categories_.isEmpty())
        return;
    const QString keep = category_combo_->currentData().toString();
    category_combo_->clear();
    for (const auto& [display, key] : server_categories_)
        category_combo_->addItem(display, key);
    const int idx = category_combo_->findData(keep);
    if (idx >= 0)
        category_combo_->setCurrentIndex(idx);
}

void SupportScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Sign-in is re-checked on every show (the gate also follows AuthManager).
    apply_auth_gate();
    if (account_key_.isEmpty())
        return;
    // The first load starts with the sign-in; this only covers returning to a
    // panel that has been sitting in the background.
    constexpr qint64 kStaleAfterMs = 60 * 1000;
    if (!list_in_flight_ && last_load_ms_ > 0 && QDateTime::currentMSecsSinceEpoch() - last_load_ms_ > kStaleAfterMs)
        load_tickets();
}

// ── apply_styles ──────────────────────────────────────────────────────────────

void SupportScreen::apply_styles() {
    // setStyleSheet() re-styles a widget even for an identical sheet: only on change.
    const auto put = [](QWidget* w, const QString& sheet) {
        if (w && w->styleSheet() != sheet)
            w->setStyleSheet(sheet);
    };
    put(this, QString("background:%1;").arg(ui::colors::BG_BASE()));
    put(refresh_btn_, SS_BTN_GHOST());
    const QString input = SS_INPUT();
    for (QWidget* w : {static_cast<QWidget*>(search_input_), static_cast<QWidget*>(filter_combo_),
                       static_cast<QWidget*>(subject_input_), static_cast<QWidget*>(category_combo_),
                       static_cast<QWidget*>(priority_combo_), static_cast<QWidget*>(desc_input_),
                       static_cast<QWidget*>(msg_input_)})
        put(w, input);
    const QString success = SS_BTN_SUCCESS();
    put(create_btn_, success);
    put(send_btn_, success);
    put(close_btn_, SS_BTN_DANGER());
    put(reopen_btn_, SS_BTN_OUTLINE());
}

// ── Re-translation ────────────────────────────────────────────────────────────
//
// Two-step refresh:
//   1. Set localised text on every cached member pointer (static chrome —
//      titles, captions, button labels, placeholders, filter combo items).
//   2. Rebuild the content_stack_ pages from build_*() and re-run
//      load_tickets() so the dynamically-built sidebar rows + detail
//      messages pick up the new language.
//
// In-progress form text in the create page is restored after the rebuild
// so a language switch doesn't wipe a half-written ticket.

void SupportScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SupportScreen::retranslateUi() {
    // ── Top bar ───────────────────────────────────────────────────────────────
    if (title_lbl_)
        title_lbl_->setText(tr("Support"));
    if (breadcrumb_lbl_)
        breadcrumb_lbl_->setText(tr("Tickets"));
    if (status_lbl_)
        status_lbl_->setText(tr("Ready"));
    if (refresh_btn_)
        refresh_btn_->setToolTip(tr("Refresh tickets"));

    // ── Sidebar header ────────────────────────────────────────────────────────
    if (new_ticket_btn_)
        new_ticket_btn_->setText(tr("＋  New Ticket"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search tickets…"));

    // Filter combo — preserve current row across the rebuild.
    if (filter_combo_) {
        const int idx = filter_combo_->currentIndex();
        filter_combo_->blockSignals(true);
        filter_combo_->clear();
        // Matches the API's statuses (open / in_progress / resolved / closed);
        // the old "Pending" entry could never match anything.
        filter_combo_->addItems({tr("All Tickets"), tr("Open"), tr("In Progress"), tr("Resolved"), tr("Closed")});
        if (idx >= 0 && idx < filter_combo_->count())
            filter_combo_->setCurrentIndex(idx);
        filter_combo_->blockSignals(false);
    }

    if (stat_total_caption_)
        stat_total_caption_->setText(tr("Total"));
    if (stat_open_caption_)
        stat_open_caption_->setText(tr("Open"));
    if (stat_resolved_caption_)
        stat_resolved_caption_->setText(tr("Done"));

    // ── Content pages: rebuild from build_*() so every label/button/placeholder
    //     picks up the new language. Snapshot form text first so a language
    //     switch mid-compose doesn't discard user input. ──────────────────────
    if (content_stack_) {
        const QString saved_subject = subject_input_ ? subject_input_->text() : QString();
        const QString saved_desc = desc_input_ ? desc_input_->toPlainText() : QString();
        const QString saved_reply = msg_input_ ? msg_input_->toPlainText() : QString();
        const int saved_view = content_stack_->currentIndex();

        // Tear down the three existing pages.
        while (content_stack_->count() > 0) {
            auto* w = content_stack_->widget(0);
            content_stack_->removeWidget(w);
            w->deleteLater();
        }
        // Reset member pointers that build_* re-assign — anything not re-set
        // in the rebuild is now dangling and must not be touched until
        // build_*() repopulates it.
        subject_input_ = nullptr;
        category_combo_ = nullptr;
        priority_combo_ = nullptr;
        desc_input_ = nullptr;
        create_btn_ = nullptr;
        char_count_lbl_ = nullptr;
        detail_id_lbl_ = nullptr;
        detail_subject_lbl_ = nullptr;
        detail_status_lbl_ = nullptr;
        detail_meta_lbl_ = nullptr;
        detail_body_lbl_ = nullptr;
        messages_container_ = nullptr;
        detail_scroll_ = nullptr;
        msg_input_ = nullptr;
        send_btn_ = nullptr;
        close_btn_ = nullptr;
        reopen_btn_ = nullptr;
        reply_box_ = nullptr;
        closed_box_ = nullptr;
        home_state_ = nullptr;
        home_skeleton_ = nullptr;

        content_stack_->addWidget(build_empty_state());
        content_stack_->addWidget(build_create_page());
        content_stack_->addWidget(build_detail_page());
        // The rebuilt detail page is blank (its labels/messages were widgets of the
        // old page), and keeping it on screen with a live selected_ticket_id_ would
        // let a reply be typed and sent to a ticket the user can no longer see.
        // Open the same ticket again from the cached list (one fetch of its
        // conversation); without it, fall back to page 0.
        const int reopen_id = saved_view == 2 ? selected_ticket_id_ : -1;
        if (saved_view == 2) {
            selected_ticket_id_ = -1;
            selected_is_closed_ = false;
            content_stack_->setCurrentIndex(0);
        } else {
            content_stack_->setCurrentIndex(saved_view);
        }
        update_home_page();
        if (reopen_id > 0) {
            for (const auto& v : all_tickets_) {
                if (v.toObject()["id"].toVariant().toInt() == reopen_id) {
                    open_ticket(v.toObject());
                    break;
                }
            }
        }
        populate_category_combo(); // restore the server-provided category list

        // Restore form text the user was composing.
        if (subject_input_ && !saved_subject.isEmpty())
            subject_input_->setText(saved_subject);
        if (desc_input_ && !saved_desc.isEmpty())
            desc_input_->setPlainText(saved_desc);
        if (msg_input_ && !saved_reply.isEmpty())
            msg_input_->setPlainText(saved_reply);

        // Re-render the sidebar rows (or its designed state) from the cached
        // ticket set so their text picks up the new language — no network
        // round-trip. (The constructor's first retranslateUi() runs before
        // content_stack_ exists, so none of this runs then.)
        rebuild_ticket_rows();
        // Sign-in page / status texts in the new language.
        apply_auth_gate();
    }
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

// ── ISectionLinked ────────────────────────────────────────────────────────────

bool SupportScreen::open_section(const QString& section, const QString& focus) {
    const QString key = section.trimmed().toLower();
    const QString text = focus.trimmed();
    if (key == QLatin1String("new")) {
        new_ticket_btn_->click(); // the New Ticket button
        if (!text.isEmpty() && subject_input_ && subject_input_->text().trimmed().isEmpty())
            subject_input_->setText(text);
        if (subject_input_)
            subject_input_->setFocus(Qt::OtherFocusReason);
        return true;
    }
    if (key.isEmpty() || key == QLatin1String("tickets")) {
        // The ticket list filters live on its search box (cached list, no request).
        if (!text.isEmpty())
            search_input_->setText(text);
        if (key == QLatin1String("tickets") || !text.isEmpty())
            search_input_->setFocus(Qt::OtherFocusReason);
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> sup_command_rows() {
    namespace rows = command_rows;
    const QString where = SupportScreen::tr("Support");
    const QString screen = QStringLiteral("support");
    return {
        rows::section(screen, QStringLiteral("new"), SupportScreen::tr("New support ticket"),
                      {QStringLiteral("ticket"), QStringLiteral("contact support"), QStringLiteral("bug report"),
                       QStringLiteral("problem"), QStringLiteral("issue"), QStringLiteral("help request")},
                      where),
        rows::section(screen, QStringLiteral("tickets"), SupportScreen::tr("My support tickets"),
                      {QStringLiteral("tickets"), QStringLiteral("support requests"), QStringLiteral("replies"),
                       QStringLiteral("ticket status")},
                      where),
    };
}

void sup_install_commands() {
    command_rows::install(QStringLiteral("support"), &sup_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(sup_install_commands)

} // namespace fincept::screens
