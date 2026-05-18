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
    if (l == "open")
        return "OPEN";
    if (l == "in_progress")
        return "IN PROGRESS";
    if (l == "resolved")
        return "RESOLVED";
    if (l == "closed")
        return "CLOSED";
    if (l == "pending")
        return "PENDING";
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

        // Title
        auto* icon_lbl = lbl("🎟", ui::colors::AMBER(), 14);
        bl->addWidget(icon_lbl);
        auto* title = lbl("Support", ui::colors::TEXT_PRIMARY(), 14, true);
        bl->addWidget(title);

        // Breadcrumb separator
        bl->addWidget(lbl("/", ui::colors::BORDER_MED(), 13));
        auto* sub = lbl("Tickets", ui::colors::TEXT_TERTIARY(), 12);
        bl->addWidget(sub);

        bl->addStretch();

        // Status indicator
        status_dot_ = lbl("●", ui::colors::POSITIVE(), 10);
        bl->addWidget(status_dot_);
        status_lbl_ = lbl("Ready", ui::colors::POSITIVE(), 11, true);
        bl->addWidget(status_lbl_);

        // Separator
        auto* vsep = new QFrame;
        vsep->setFrameShape(QFrame::VLine);
        vsep->setFixedWidth(1);
        vsep->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
        bl->addWidget(vsep);

        refresh_btn_ = new QPushButton("↻");
        refresh_btn_->setFixedSize(30, 26);
        refresh_btn_->setToolTip("Refresh tickets");
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

// ── Sidebar ───────────────────────────────────────────────────────────────────

} // namespace fincept::screens
