// src/screens/support/SupportScreen.cpp
//
// Core lifecycle: status/priority color and label helpers, ctor, apply_styles.
// Style helpers (SS_*, lbl, hsep, status_badge) live in SupportScreen_internal.h.
// Other concerns:
//   - SupportScreen_Layout.cpp   — build_sidebar / empty / create / detail
//   - SupportScreen_Actions.cpp  — load_tickets + create/send/close/reopen
#include "screens/support/SupportScreen.h"

#include "auth/UserApi.h"
#include "ui/theme/Theme.h"

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
#include "screens/support/SupportScreen_internal.h"

namespace fincept::screens {

using namespace fincept::screens::support_internal;

QString SupportScreen::status_color(const QString& s) {
    QString l = s.toLower();
    if (l == "open" || l == "in_progress")
        return ui::colors::CYAN();
    if (l == "resolved" || l == "closed")
        return ui::colors::POSITIVE();
    if (l == "pending")
        return ui::colors::WARNING();
    return ui::colors::TEXT_SECONDARY();
}

QString SupportScreen::priority_color(const QString& p) {
    QString l = p.toLower();
    if (l == "high" || l == "urgent")
        return ui::colors::NEGATIVE();
    if (l == "medium")
        return ui::colors::AMBER();
    if (l == "low")
        return ui::colors::POSITIVE();
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

        // Status indicator (text set in retranslateUi / set_busy)
        status_dot_ = lbl("●", ui::colors::POSITIVE(), 10);
        bl->addWidget(status_dot_);
        status_lbl_ = lbl(QString(), ui::colors::POSITIVE(), 11, true);
        bl->addWidget(status_lbl_);

        // Separator
        auto* vsep = new QFrame;
        vsep->setFrameShape(QFrame::VLine);
        vsep->setFixedWidth(1);
        vsep->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
        bl->addWidget(vsep);

        refresh_btn_ = new QPushButton("↻");
        refresh_btn_->setFixedSize(30, 26);
        refresh_btn_->setStyleSheet(SS_BTN_GHOST());
        connect(refresh_btn_, &QPushButton::clicked, this, &SupportScreen::load_tickets);
        bl->addWidget(refresh_btn_);

        root->addWidget(bar);
    }

    // ── Main splitter ─────────────────────────────────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(ui::colors::BORDER_DIM()));

    splitter_->addWidget(build_sidebar());

    // Right: stacked content
    content_stack_ = new QStackedWidget;
    content_stack_->addWidget(build_empty_state()); // 0
    content_stack_->addWidget(build_create_page()); // 1
    content_stack_->addWidget(build_detail_page()); // 2
    splitter_->addWidget(content_stack_);

    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({300, 700});

    root->addWidget(splitter_, 1);

    retranslateUi();

    // ── Theme wiring ──────────────────────────────────────────────────────────
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_styles(); });

    // Load categories from API, then load tickets
    auth::UserApi::instance().get_support_categories([this](auth::ApiResponse r) {
        if (r.success && category_combo_) {
            // Try common response shapes: {"categories":[...]} or bare array
            QJsonArray cats;
            auto payload = r.data;
            if (payload.contains("data") && payload["data"].isObject())
                payload = payload["data"].toObject();
            if (payload.contains("categories") && payload["categories"].isArray())
                cats = payload["categories"].toArray();
            else if (payload.contains("items") && payload["items"].isArray())
                cats = payload["items"].toArray();

            if (!cats.isEmpty()) {
                category_combo_->clear();
                for (const auto& v : cats) {
                    QString cat = v.isString() ? v.toString() : v.toObject()["name"].toString();
                    if (!cat.isEmpty())
                        category_combo_->addItem(cat[0].toUpper() + cat.mid(1).replace('_', ' '), cat);
                }
            }
        }
        load_tickets();
    });
}

// ── apply_styles ──────────────────────────────────────────────────────────────

void SupportScreen::apply_styles() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    refresh_btn_->setStyleSheet(SS_BTN_GHOST());
    if (search_input_)
        search_input_->setStyleSheet(SS_INPUT());
    if (filter_combo_)
        filter_combo_->setStyleSheet(SS_INPUT());
    if (subject_input_)
        subject_input_->setStyleSheet(SS_INPUT());
    if (category_combo_)
        category_combo_->setStyleSheet(SS_INPUT());
    if (priority_combo_)
        priority_combo_->setStyleSheet(SS_INPUT());
    if (desc_input_)
        desc_input_->setStyleSheet(SS_INPUT());
    if (create_btn_)
        create_btn_->setStyleSheet(SS_BTN_SUCCESS());
    if (msg_input_)
        msg_input_->setStyleSheet(SS_INPUT());
    if (send_btn_)
        send_btn_->setStyleSheet(SS_BTN_SUCCESS());
    if (close_btn_)
        close_btn_->setStyleSheet(SS_BTN_DANGER());
    if (reopen_btn_)
        reopen_btn_->setStyleSheet(SS_BTN_OUTLINE());
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
    if (title_lbl_)      title_lbl_->setText(tr("Support"));
    if (breadcrumb_lbl_) breadcrumb_lbl_->setText(tr("Tickets"));
    if (status_lbl_)     status_lbl_->setText(tr("Ready"));
    if (refresh_btn_)    refresh_btn_->setToolTip(tr("Refresh tickets"));

    // ── Sidebar header ────────────────────────────────────────────────────────
    if (new_ticket_btn_) new_ticket_btn_->setText(tr("＋  New Ticket"));
    if (search_input_)   search_input_->setPlaceholderText(tr("Search tickets…"));

    // Filter combo — preserve current row across the rebuild.
    if (filter_combo_) {
        const int idx = filter_combo_->currentIndex();
        filter_combo_->blockSignals(true);
        filter_combo_->clear();
        filter_combo_->addItems({tr("All Tickets"), tr("Open"), tr("In Progress"),
                                 tr("Pending"), tr("Resolved"), tr("Closed")});
        if (idx >= 0 && idx < filter_combo_->count())
            filter_combo_->setCurrentIndex(idx);
        filter_combo_->blockSignals(false);
    }

    if (stat_total_caption_)     stat_total_caption_->setText(tr("Total"));
    if (stat_open_caption_)      stat_open_caption_->setText(tr("Open"));
    if (stat_resolved_caption_)  stat_resolved_caption_->setText(tr("Done"));

    // ── Content pages: rebuild from build_*() so every label/button/placeholder
    //     picks up the new language. Snapshot form text first so a language
    //     switch mid-compose doesn't discard user input. ──────────────────────
    if (content_stack_) {
        const QString saved_subject = subject_input_ ? subject_input_->text() : QString();
        const QString saved_desc    = desc_input_    ? desc_input_->toPlainText() : QString();
        const QString saved_reply   = msg_input_     ? msg_input_->toPlainText() : QString();
        const int saved_view = content_stack_->currentIndex();
        const int saved_ticket = selected_ticket_id_;

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
        demo_box_ = nullptr;
        closed_box_ = nullptr;

        content_stack_->addWidget(build_empty_state());
        content_stack_->addWidget(build_create_page());
        content_stack_->addWidget(build_detail_page());
        content_stack_->setCurrentIndex(saved_view);

        // Restore form text the user was composing.
        if (subject_input_ && !saved_subject.isEmpty()) subject_input_->setText(saved_subject);
        if (desc_input_ && !saved_desc.isEmpty())       desc_input_->setPlainText(saved_desc);
        if (msg_input_ && !saved_reply.isEmpty())       msg_input_->setPlainText(saved_reply);

        // Reload tickets so sidebar row text + the currently-shown detail
        // re-render with new translations. selected_ticket_id_ is preserved.
        selected_ticket_id_ = saved_ticket;
        load_tickets();
    }
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

} // namespace fincept::screens
