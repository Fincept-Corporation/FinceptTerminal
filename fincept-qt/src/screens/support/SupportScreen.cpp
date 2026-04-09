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

namespace fincept::screens {

// ── Font constant ─────────────────────────────────────────────────────────────
static const char* MF = "font-family:'Inter','Segoe UI','Consolas',monospace;";

// ── Style helpers ─────────────────────────────────────────────────────────────

static QString SS_INPUT() {
    return QString("QLineEdit, QTextEdit, QComboBox {"
                   "  background: %1; color: %2; border: 1px solid %3;"
                   "  padding: 7px 10px; font-size: 12px; %4 }"
                   "QLineEdit:focus, QTextEdit:focus { border-color: %5; outline: none; }"
                   "QComboBox::drop-down { border: none; width: 22px; }"
                   "QComboBox::down-arrow { width: 10px; height: 10px; }"
                   "QComboBox QAbstractItemView {"
                   "  background: %6; color: %2; border: 1px solid %3;"
                   "  selection-background-color: %5; selection-color: %7; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), MF, ui::colors::AMBER(),
             ui::colors::BG_RAISED(), ui::colors::BG_BASE());
}

static QString SS_BTN_PRIMARY() {
    return QString("QPushButton { background: %1; color: %2; border: none;"
                   "  padding: 8px 20px; font-size: 12px; font-weight: 600; %3 }"
                   "QPushButton:hover { background: %4; }"
                   "QPushButton:pressed { background: %5; }"
                   "QPushButton:disabled { background: %6; color: %7; }")
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), MF, ui::colors::AMBER_DIM(), "#92400e",
             ui::colors::BORDER_MED(), ui::colors::TEXT_DIM());
}

static QString SS_BTN_SUCCESS() {
    return QString("QPushButton { background: %1; color: %2; border: none;"
                   "  padding: 8px 20px; font-size: 12px; font-weight: 600; %3 }"
                   "QPushButton:hover { background: #15803d; }"
                   "QPushButton:pressed { background: #14532d; }"
                   "QPushButton:disabled { background: %4; color: %5; }")
        .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE(), MF, ui::colors::BORDER_MED(), ui::colors::TEXT_DIM());
}

static QString SS_BTN_DANGER() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                   "  padding: 6px 14px; font-size: 11px; font-weight: 600; %2 }"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { border-color: %4; color: %4; }")
        .arg(ui::colors::NEGATIVE(), MF, ui::colors::BG_BASE(), ui::colors::TEXT_DIM());
}

static QString SS_BTN_OUTLINE() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                   "  padding: 6px 14px; font-size: 11px; font-weight: 600; %2 }"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { border-color: %4; color: %4; }")
        .arg(ui::colors::POSITIVE(), MF, ui::colors::BG_BASE(), ui::colors::TEXT_DIM());
}

static QString SS_BTN_GHOST() {
    return QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
                   "  padding: 5px 12px; font-size: 11px; font-weight: 500; %3 }"
                   "QPushButton:hover { background: %2; color: %4; }"
                   "QPushButton:pressed { background: %5; }")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), MF, ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_DIM());
}

// ── Label factory ─────────────────────────────────────────────────────────────

static QLabel* lbl(const QString& text, const QString& color, int px = 12, bool bold = false, bool wrap = false) {
    auto* l = new QLabel(text);
    l->setWordWrap(wrap);
    l->setStyleSheet(QString("color:%1;font-size:%2px;background:transparent;%3%4")
                         .arg(color)
                         .arg(px)
                         .arg(MF)
                         .arg(bold ? "font-weight:600;" : ""));
    return l;
}

static QFrame* hsep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
    return f;
}

static QLabel* status_badge(const QString& text, const QString& bg, const QString& fg) {
    auto* b = new QLabel(text);
    b->setAlignment(Qt::AlignCenter);
    b->setFixedHeight(20);
    b->setStyleSheet(QString("background:%1;color:%2;font-size:9px;font-weight:700;"
                             "padding:0 8px;letter-spacing:0.5px;%3")
                         .arg(bg, fg, MF));
    return b;
}

// ── Color helpers ─────────────────────────────────────────────────────────────

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
            auto data = r.data;
            if (data.contains("data") && data["data"].isObject())
                data = data["data"].toObject();
            if (data.contains("categories") && data["categories"].isArray())
                cats = data["categories"].toArray();
            else if (data.contains("items") && data["items"].isArray())
                cats = data["items"].toArray();

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

QWidget* SupportScreen::build_sidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setMinimumWidth(240);
    sidebar->setMaximumWidth(360);
    sidebar->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(sidebar);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Sidebar header ────────────────────────────────────────────────────────
    {
        auto* hdr = new QWidget(this);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* hl = new QVBoxLayout(hdr);
        hl->setContentsMargins(14, 10, 14, 10);
        hl->setSpacing(8);

        // New ticket button — full width
        auto* new_btn = new QPushButton("＋  New Ticket");
        new_btn->setFixedHeight(34);
        new_btn->setStyleSheet(SS_BTN_PRIMARY());
        connect(new_btn, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(1); });
        hl->addWidget(new_btn);

        // Search
        search_input_ = new QLineEdit;
        search_input_->setPlaceholderText("🔍  Search tickets…");
        search_input_->setFixedHeight(30);
        search_input_->setStyleSheet(SS_INPUT());
        hl->addWidget(search_input_);

        // Filter
        filter_combo_ = new QComboBox;
        filter_combo_->addItems({"All Tickets", "Open", "In Progress", "Pending", "Resolved", "Closed"});
        filter_combo_->setFixedHeight(28);
        filter_combo_->setStyleSheet(SS_INPUT());
        hl->addWidget(filter_combo_);

        vl->addWidget(hdr);
    }

    // ── Stats strip ───────────────────────────────────────────────────────────
    {
        auto* stats = new QWidget(this);
        stats->setFixedHeight(32);
        stats->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
        auto* sl = new QHBoxLayout(stats);
        sl->setContentsMargins(14, 0, 14, 0);
        sl->setSpacing(0);

        auto add_stat = [&](const QString& label, QLabel*& ref, const QString& color) {
            auto* w = new QWidget(this);
            w->setStyleSheet("background:transparent;");
            auto* wl = new QHBoxLayout(w);
            wl->setContentsMargins(0, 0, 0, 0);
            wl->setSpacing(4);
            wl->addWidget(lbl(label, ui::colors::TEXT_TERTIARY(), 10));
            ref = lbl("0", color, 10, true);
            wl->addWidget(ref);
            return w;
        };

        sl->addWidget(add_stat("Total", stat_total_, ui::colors::TEXT_SECONDARY()));
        sl->addStretch();
        sl->addWidget(add_stat("Open", stat_open_, ui::colors::CYAN()));
        sl->addStretch();
        sl->addWidget(add_stat("Done", stat_resolved_, ui::colors::POSITIVE()));

        vl->addWidget(stats);
    }

    // ── Ticket list ───────────────────────────────────────────────────────────
    list_scroll_ = new QScrollArea;
    list_scroll_->setWidgetResizable(true);
    list_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_scroll_->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                                "QScrollBar:vertical { background:transparent; width:4px; }"
                                "QScrollBar::handle:vertical { background:#444; border-radius:2px; }"
                                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    ticket_container_ = new QWidget(this);
    ticket_container_->setStyleSheet("background:transparent;");
    auto* tcl = new QVBoxLayout(ticket_container_);
    tcl->setContentsMargins(0, 4, 0, 4);
    tcl->setSpacing(0);
    tcl->addStretch();

    list_scroll_->setWidget(ticket_container_);
    vl->addWidget(list_scroll_, 1);

    return sidebar;
}

// ── Empty state ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_empty_state() {
    auto* w = new QWidget(this);
    w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignCenter);
    vl->setSpacing(12);

    auto* icon = lbl("🎟", ui::colors::TEXT_TERTIARY(), 40);
    icon->setAlignment(Qt::AlignCenter);
    vl->addWidget(icon);

    auto* t = lbl("Select a ticket to view details", ui::colors::TEXT_SECONDARY(), 13, true);
    t->setAlignment(Qt::AlignCenter);
    vl->addWidget(t);

    auto* s = lbl("or create a new support request", ui::colors::TEXT_TERTIARY(), 11, false, true);
    s->setAlignment(Qt::AlignCenter);
    vl->addWidget(s);

    vl->addSpacing(16);

    auto* btn = new QPushButton("＋  Create New Ticket");
    btn->setFixedSize(200, 36);
    btn->setStyleSheet(SS_BTN_PRIMARY());
    connect(btn, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(1); });
    vl->addWidget(btn, 0, Qt::AlignCenter);

    return w;
}

// ── Create page ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_create_page() {
    auto* outer = new QScrollArea;
    outer->setWidgetResizable(true);
    outer->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                         "QScrollBar:vertical { background:transparent; width:4px; }"
                         "QScrollBar::handle:vertical { background:#444; border-radius:2px; }"
                         "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(32, 28, 32, 28);
    vl->setSpacing(0);

    // Page header
    {
        auto* cancel_btn = new QPushButton("← Back");
        cancel_btn->setFlat(true);
        cancel_btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:none;"
                                          "font-size:11px;padding:0;%2}"
                                          "QPushButton:hover{color:%3;}")
                                      .arg(ui::colors::TEXT_TERTIARY(), MF, ui::colors::AMBER()));
        connect(cancel_btn, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(0); });
        vl->addWidget(cancel_btn, 0, Qt::AlignLeft);
        vl->addSpacing(16);

        vl->addWidget(lbl("Create Support Ticket", ui::colors::TEXT_PRIMARY(), 18, true));
        vl->addSpacing(4);
        vl->addWidget(lbl("Describe your issue in detail. We typically respond within 24 hours.",
                          ui::colors::TEXT_TERTIARY(), 12, false, true));
        vl->addSpacing(24);
    }

    // ── Form card ─────────────────────────────────────────────────────────────
    auto* card = new QWidget(this);
    card->setStyleSheet(
        QString("background:%1;border:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(24, 20, 24, 24);
    cl->setSpacing(16);

    // Field helper
    auto add_field = [&](const QString& label_text, QWidget* input, bool required = false) {
        auto* fw = new QWidget(this);
        fw->setStyleSheet("background:transparent;");
        auto* fvl = new QVBoxLayout(fw);
        fvl->setContentsMargins(0, 0, 0, 0);
        fvl->setSpacing(6);

        auto* row = new QHBoxLayout;
        row->addWidget(lbl(label_text, ui::colors::TEXT_SECONDARY(), 11, true));
        if (required) {
            auto* req = lbl("*", ui::colors::AMBER(), 11, true);
            row->addWidget(req);
        }
        row->addStretch();
        fvl->addLayout(row);
        fvl->addWidget(input);
        cl->addWidget(fw);
    };

    subject_input_ = new QLineEdit;
    subject_input_->setPlaceholderText("Brief summary of your issue");
    subject_input_->setFixedHeight(38);
    subject_input_->setStyleSheet(SS_INPUT());
    add_field("Subject", subject_input_, true);

    // Category + Priority side by side
    {
        auto* row_w = new QWidget(this);
        row_w->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row_w);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(16);

        auto make_combo_field = [&](const QString& label_text, QComboBox*& ref, const QStringList& items, int def = 0) {
            auto* fw = new QWidget(this);
            fw->setStyleSheet("background:transparent;");
            auto* fvl = new QVBoxLayout(fw);
            fvl->setContentsMargins(0, 0, 0, 0);
            fvl->setSpacing(6);
            fvl->addWidget(lbl(label_text, ui::colors::TEXT_SECONDARY(), 11, true));
            ref = new QComboBox;
            ref->addItems(items);
            ref->setCurrentIndex(def);
            ref->setFixedHeight(38);
            ref->setStyleSheet(SS_INPUT());
            fvl->addWidget(ref);
            rl->addLayout(fvl);
        };

        make_combo_field("Category", category_combo_,
                         {"Technical", "Billing", "Feature Request", "Bug Report", "Account", "Other"});
        make_combo_field("Priority", priority_combo_, {"Low", "Medium", "High"}, 1);

        cl->addWidget(row_w);
    }

    // Description with char counter
    {
        auto* hdr = new QHBoxLayout;
        hdr->addWidget(lbl("Description", ui::colors::TEXT_SECONDARY(), 11, true));
        auto* req = lbl("*", ui::colors::AMBER(), 11, true);
        hdr->addWidget(req);
        hdr->addStretch();
        char_count_lbl_ = lbl("0 / 2000", ui::colors::TEXT_TERTIARY(), 10);
        hdr->addWidget(char_count_lbl_);
        cl->addLayout(hdr);

        desc_input_ = new QTextEdit;
        desc_input_->setPlaceholderText("Please describe:\n"
                                        "• What were you doing?\n"
                                        "• What did you expect to happen?\n"
                                        "• What actually happened?\n"
                                        "• Steps to reproduce (if applicable)");
        desc_input_->setMinimumHeight(160);
        desc_input_->setMaximumHeight(280);
        desc_input_->setStyleSheet(SS_INPUT());
        cl->addWidget(desc_input_);

        connect(desc_input_, &QTextEdit::textChanged, this, [this]() {
            int n = desc_input_->toPlainText().length();
            char_count_lbl_->setText(QString("%1 / 2000").arg(n));
            char_count_lbl_->setStyleSheet(
                QString("color:%1;font-size:10px;background:transparent;%2")
                    .arg(n > 2000 ? ui::colors::NEGATIVE() : ui::colors::TEXT_TERTIARY(), MF));
        });
    }

    vl->addWidget(card);
    vl->addSpacing(16);

    // Action row
    {
        auto* ar = new QHBoxLayout;

        auto* cancel2 = new QPushButton("Cancel");
        cancel2->setFixedHeight(38);
        cancel2->setStyleSheet(SS_BTN_GHOST());
        connect(cancel2, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(0); });
        ar->addWidget(cancel2);
        ar->addStretch();

        create_btn_ = new QPushButton("Submit Ticket →");
        create_btn_->setFixedHeight(38);
        create_btn_->setMinimumWidth(160);
        create_btn_->setStyleSheet(SS_BTN_SUCCESS());
        connect(create_btn_, &QPushButton::clicked, this, &SupportScreen::on_create_ticket);
        ar->addWidget(create_btn_);

        vl->addLayout(ar);
    }

    // Tips panel below
    {
        vl->addSpacing(20);
        auto* tips = new QWidget(this);
        tips->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid %3;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
        auto* tvl = new QVBoxLayout(tips);
        tvl->setContentsMargins(16, 12, 16, 14);
        tvl->setSpacing(8);

        tvl->addWidget(lbl("Tips for a faster response", ui::colors::AMBER(), 11, true));
        tvl->addWidget(hsep());

        const char* tip_items[] = {
            "✓  One issue per ticket — easier to track and resolve",
            "✓  Include your OS, version, and any error messages",
            "✓  Describe steps to reproduce if it's a bug",
            "✓  Billing questions resolved within 4 hours",
        };
        for (const auto* t : tip_items) {
            auto* tl = lbl(t, ui::colors::TEXT_SECONDARY(), 11, false, true);
            tvl->addWidget(tl);
        }
        vl->addWidget(tips);
    }

    vl->addStretch();
    outer->setWidget(page);
    return outer;
}

// ── Detail page ───────────────────────────────────────────────────────────────

QWidget* SupportScreen::build_detail_page() {
    auto* outer = new QWidget(this);
    outer->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* root_vl = new QVBoxLayout(outer);
    root_vl->setContentsMargins(0, 0, 0, 0);
    root_vl->setSpacing(0);

    // ── Detail header bar ─────────────────────────────────────────────────────
    {
        auto* hdr = new QWidget(this);
        hdr->setFixedHeight(50);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* hl = new QHBoxLayout(hdr);
        hl->setContentsMargins(20, 0, 20, 0);
        hl->setSpacing(10);

        detail_id_lbl_ = lbl("", ui::colors::TEXT_TERTIARY(), 11);
        hl->addWidget(detail_id_lbl_);

        detail_subject_lbl_ = lbl("", ui::colors::TEXT_PRIMARY(), 14, true);
        detail_subject_lbl_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        detail_subject_lbl_->setWordWrap(false);
        hl->addWidget(detail_subject_lbl_, 1);

        detail_status_lbl_ = new QLabel;
        detail_status_lbl_->setFixedHeight(22);
        detail_status_lbl_->setAlignment(Qt::AlignCenter);
        detail_status_lbl_->setStyleSheet(QString("background:%1;color:%2;font-size:10px;font-weight:700;"
                                                  "padding:0 10px;letter-spacing:0.5px;%3")
                                              .arg(ui::colors::CYAN(), ui::colors::BG_BASE(), MF));
        hl->addWidget(detail_status_lbl_);

        close_btn_ = new QPushButton("Close Ticket");
        close_btn_->setFixedHeight(28);
        close_btn_->setStyleSheet(SS_BTN_DANGER());
        close_btn_->hide();
        connect(close_btn_, &QPushButton::clicked, this, &SupportScreen::on_close_ticket);
        hl->addWidget(close_btn_);

        reopen_btn_ = new QPushButton("Reopen");
        reopen_btn_->setFixedHeight(28);
        reopen_btn_->setStyleSheet(SS_BTN_OUTLINE());
        reopen_btn_->hide();
        connect(reopen_btn_, &QPushButton::clicked, this, &SupportScreen::on_reopen_ticket);
        hl->addWidget(reopen_btn_);

        root_vl->addWidget(hdr);
    }

    // ── Ticket info strip ─────────────────────────────────────────────────────
    {
        auto* info_strip = new QWidget(this);
        info_strip->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                      .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* il = new QVBoxLayout(info_strip);
        il->setContentsMargins(20, 10, 20, 10);
        il->setSpacing(6);

        detail_meta_lbl_ = lbl("", ui::colors::TEXT_TERTIARY(), 10);
        il->addWidget(detail_meta_lbl_);

        detail_body_lbl_ = new QLabel;
        detail_body_lbl_->setWordWrap(true);
        detail_body_lbl_->setTextFormat(Qt::PlainText);
        detail_body_lbl_->setStyleSheet(
            QString("color:%1;font-size:12px;background:transparent;%2").arg(ui::colors::TEXT_SECONDARY(), MF));
        il->addWidget(detail_body_lbl_);

        root_vl->addWidget(info_strip);
    }

    // ── Conversation area (scrollable) ────────────────────────────────────────
    detail_scroll_ = new QScrollArea;
    detail_scroll_->setWidgetResizable(true);
    detail_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detail_scroll_->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                                  "QScrollBar:vertical { background:transparent; width:4px; }"
                                  "QScrollBar::handle:vertical { background:#444; border-radius:2px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    messages_container_ = new QWidget(this);
    messages_container_->setStyleSheet("background:transparent;");
    auto* mcl = new QVBoxLayout(messages_container_);
    mcl->setContentsMargins(20, 16, 20, 16);
    mcl->setSpacing(12);
    mcl->addStretch();

    detail_scroll_->setWidget(messages_container_);
    root_vl->addWidget(detail_scroll_, 1);

    // ── Bottom area: reply / demo / closed notices ────────────────────────────
    {
        // Demo notice
        demo_box_ = new QWidget(this);
        demo_box_->setStyleSheet(
            QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* dbl = new QHBoxLayout(demo_box_);
        dbl->setContentsMargins(20, 12, 20, 12);
        auto* di = lbl("ℹ", ui::colors::INFO(), 14);
        dbl->addWidget(di);
        auto* dt = lbl("This is a demo ticket. Open a real ticket to get support from our team.",
                       ui::colors::TEXT_SECONDARY(), 11, false, true);
        dbl->addWidget(dt, 1);
        auto* db = new QPushButton("New Ticket");
        db->setFixedHeight(28);
        db->setStyleSheet(SS_BTN_PRIMARY());
        connect(db, &QPushButton::clicked, this, [this]() { content_stack_->setCurrentIndex(1); });
        dbl->addWidget(db);
        demo_box_->hide();
        root_vl->addWidget(demo_box_);

        // Closed notice
        closed_box_ = new QWidget(this);
        closed_box_->setStyleSheet(
            QString("background:%1;border-top:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cbl = new QHBoxLayout(closed_box_);
        cbl->setContentsMargins(20, 12, 20, 12);
        cbl->addWidget(lbl("✓  This ticket is closed.", ui::colors::POSITIVE(), 11));
        cbl->addStretch();
        closed_box_->hide();
        root_vl->addWidget(closed_box_);

        // Reply box
        reply_box_ = new QWidget(this);
        reply_box_->setStyleSheet(
            QString("background:%1;border-top:2px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::POSITIVE()));
        auto* rbl = new QVBoxLayout(reply_box_);
        rbl->setContentsMargins(20, 12, 20, 14);
        rbl->setSpacing(10);

        auto* reply_hdr = new QHBoxLayout;
        reply_hdr->addWidget(lbl("Reply", ui::colors::TEXT_PRIMARY(), 12, true));
        reply_hdr->addStretch();
        reply_hdr->addWidget(lbl("Ctrl+Enter to send", ui::colors::TEXT_TERTIARY(), 10));
        rbl->addLayout(reply_hdr);

        msg_input_ = new QTextEdit;
        msg_input_->setPlaceholderText("Type your reply…");
        msg_input_->setFixedHeight(80);
        msg_input_->setStyleSheet(SS_INPUT());
        rbl->addWidget(msg_input_);

        auto* send_row = new QHBoxLayout;
        send_row->addStretch();
        send_btn_ = new QPushButton("Send Reply →");
        send_btn_->setFixedHeight(34);
        send_btn_->setMinimumWidth(140);
        send_btn_->setStyleSheet(SS_BTN_SUCCESS());
        connect(send_btn_, &QPushButton::clicked, this, &SupportScreen::on_send_message);
        send_row->addWidget(send_btn_);
        rbl->addLayout(send_row);

        root_vl->addWidget(reply_box_);
    }

    return outer;
}

// ── select_ticket_row ─────────────────────────────────────────────────────────

void SupportScreen::select_ticket_row(QPushButton* btn) {
    if (active_row_btn_ && active_row_btn_ != btn) {
        active_row_btn_->setStyleSheet(QString("QPushButton { background:transparent; border:none;"
                                               " border-bottom:1px solid %1; padding:0; text-align:left; }"
                                               "QPushButton:hover { background:%2; }")
                                           .arg(ui::colors::BORDER_DIM(), ui::colors::BG_RAISED()));
    }
    active_row_btn_ = btn;
    btn->setStyleSheet(QString("QPushButton { background:%1; border:none;"
                               " border-bottom:1px solid %2; border-left:3px solid %3;"
                               " padding:0; text-align:left; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
}

// ── set_busy ──────────────────────────────────────────────────────────────────

void SupportScreen::set_busy(bool busy) {
    QString col = busy ? ui::colors::AMBER() : ui::colors::POSITIVE();
    status_dot_->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;").arg(col));
    status_lbl_->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;%2").arg(col, MF));
    status_lbl_->setText(busy ? "Updating…" : "Ready");
    refresh_btn_->setEnabled(!busy);
    if (create_btn_)
        create_btn_->setEnabled(!busy);
    if (send_btn_)
        send_btn_->setEnabled(!busy);
}

// ── load_tickets ──────────────────────────────────────────────────────────────

void SupportScreen::load_tickets() {
    set_busy(true);

    auth::UserApi::instance().get_tickets([this](auth::ApiResponse r) {
        set_busy(false);

        auto* lay = qobject_cast<QVBoxLayout*>(ticket_container_->layout());
        if (!lay)
            return;
        while (lay->count() > 1)
            delete lay->takeAt(0)->widget();
        active_row_btn_ = nullptr;

        // Robust response unwrap:
        // API may return: {"tickets":[...]}  or  {"data":{"tickets":[...]}}
        // or a bare array at top level, or {"success":true,"data":[...]}
        QJsonArray tickets;
        if (r.success) {
            auto data = r.data;
            // Unwrap {"data": ...}
            if (data.contains("data") && data["data"].isObject())
                data = data["data"].toObject();
            // Try known array keys
            if (data.contains("tickets") && data["tickets"].isArray())
                tickets = data["tickets"].toArray();
            else if (data.contains("items") && data["items"].isArray())
                tickets = data["items"].toArray();
            else if (data.contains("results") && data["results"].isArray())
                tickets = data["results"].toArray();
            // If the whole response data is itself an array key at root
            if (tickets.isEmpty()) {
                for (auto it = data.begin(); it != data.end(); ++it) {
                    if (it.value().isArray()) {
                        tickets = it.value().toArray();
                        break;
                    }
                }
            }
        }

        // Demo ticket
        QJsonObject demo;
        demo["id"] = "DEMO-001";
        demo["subject"] = "Welcome to Fincept Support";
        demo["status"] = "resolved";
        demo["priority"] = "low";
        demo["category"] = "general";
        demo["created_at"] = "2026-01-01T00:00:00Z";

        QJsonArray all;
        all.append(demo);
        for (const auto& v : tickets)
            all.append(v);

        // Stats
        int open_c = 0, done_c = 0;
        for (const auto& v : all) {
            QString st = v.toObject()["status"].toString().toLower();
            if (st == "open" || st == "in_progress" || st == "pending")
                ++open_c;
            else if (st == "resolved" || st == "closed")
                ++done_c;
        }
        stat_total_->setText(QString::number(all.size()));
        stat_open_->setText(QString::number(open_c));
        stat_resolved_->setText(QString::number(done_c));

        if (all.isEmpty()) {
            auto* empty = lbl("No tickets yet", ui::colors::TEXT_TERTIARY(), 12);
            empty->setAlignment(Qt::AlignCenter);
            empty->setContentsMargins(0, 40, 0, 0);
            lay->insertWidget(0, empty);
            return;
        }

        // Build sidebar rows
        for (const auto& v : all) {
            auto obj = v.toObject();
            QString id = obj["id"].toVariant().toString();
            QString subject = obj["subject"].toString();
            QString status = obj["status"].toString();
            QString priority = obj["priority"].toString();
            QString category = obj["category"].toString();
            QString created = obj["created_at"].toString();
            bool is_demo = (id == "DEMO-001");

            QString date_str;
            if (!created.isEmpty()) {
                QDateTime dt = QDateTime::fromString(created, Qt::ISODate);
                if (dt.isValid())
                    date_str = dt.toLocalTime().toString("d MMM yyyy");
            }

            // Row widget (inner content)
            auto* row_inner = new QWidget(this);
            row_inner->setStyleSheet("background:transparent;");
            auto* rl = new QVBoxLayout(row_inner);
            rl->setContentsMargins(14, 10, 14, 10);
            rl->setSpacing(5);

            // Top: subject + status dot
            auto* top = new QHBoxLayout;
            auto* sdot = lbl("●", status_color(status), 9);
            sdot->setFixedWidth(14);
            top->addWidget(sdot);
            auto* subj = lbl(subject, ui::colors::TEXT_PRIMARY(), 12, true);
            subj->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            // Elide long subjects
            subj->setMaximumWidth(220);
            top->addWidget(subj, 1);

            if (is_demo) {
                auto* demo_tag = status_badge("DEMO", ui::colors::INFO(), ui::colors::BG_BASE());
                top->addWidget(demo_tag);
            }
            rl->addLayout(top);

            // Bottom: meta row
            auto* meta = new QHBoxLayout;
            meta->setSpacing(8);
            meta->addWidget(lbl(category.toLower(), ui::colors::TEXT_TERTIARY(), 10));
            meta->addWidget(lbl("·", ui::colors::BORDER_MED(), 10));
            auto* pr = lbl(priority.toLower(), priority_color(priority), 10);
            meta->addWidget(pr);
            meta->addStretch();
            if (!date_str.isEmpty())
                meta->addWidget(lbl(date_str, ui::colors::TEXT_TERTIARY(), 9));
            rl->addLayout(meta);

            // Wrap in QPushButton for click
            auto* btn = new QPushButton;
            btn->setFixedHeight(66);
            btn->setStyleSheet(QString("QPushButton { background:transparent; border:none;"
                                       " border-bottom:1px solid %1; padding:0; text-align:left; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::BORDER_DIM(), ui::colors::BG_RAISED()));
            auto* btn_lay = new QHBoxLayout(btn);
            btn_lay->setContentsMargins(0, 0, 0, 0);
            row_inner->setParent(btn);
            btn_lay->addWidget(row_inner);

            const int id_int = is_demo ? -1 : id.toInt();
            const QString sub_c = subject;
            const QString st_c = status;
            const QString pr_c = priority;
            const QString cat_c = category;
            const QString cr_c = date_str;
            const QString body_c = obj["description"].toString();
            const bool demo_c = is_demo;

            connect(btn, &QPushButton::clicked, this, [=]() {
                select_ticket_row(btn);

                selected_ticket_id_ = id_int;
                selected_is_demo_ = demo_c;
                selected_is_closed_ = (st_c.toLower() == "closed" || st_c.toLower() == "resolved");

                // Populate header
                detail_id_lbl_->setText(QString("Ticket #%1").arg(demo_c ? "DEMO" : QString::number(id_int)));
                detail_subject_lbl_->setText(sub_c);

                detail_status_lbl_->setText(" " + status_label(st_c) + " ");
                detail_status_lbl_->setStyleSheet(QString("background:%1;color:%2;font-size:10px;font-weight:700;"
                                                          "padding:0 10px;letter-spacing:0.5px;%3")
                                                      .arg(status_color(st_c), ui::colors::BG_BASE(), MF));

                detail_meta_lbl_->setText(
                    QString("%1  ·  %2  ·  Opened %3").arg(cat_c.toLower(), pr_c.toLower(), cr_c));

                detail_body_lbl_->setText(body_c.isEmpty() ? "No description provided." : body_c);

                // Show/hide bottom widgets
                demo_box_->setVisible(demo_c);
                closed_box_->setVisible(!demo_c && selected_is_closed_);
                reply_box_->setVisible(!demo_c && !selected_is_closed_);
                close_btn_->setVisible(!demo_c && !selected_is_closed_);
                reopen_btn_->setVisible(!demo_c && selected_is_closed_);

                // Clear messages
                auto* mcl2 = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                while (mcl2->count() > 1)
                    delete mcl2->takeAt(0)->widget();

                if (demo_c) {
                    // Canned demo message
                    auto* m = new QWidget(this);
                    m->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid #9333ea;")
                                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
                    auto* ml = new QVBoxLayout(m);
                    ml->setContentsMargins(14, 10, 14, 12);
                    ml->setSpacing(6);
                    auto* mh = new QHBoxLayout;
                    mh->addWidget(lbl("Support Team", "#9333ea", 11, true));
                    mh->addStretch();
                    mh->addWidget(lbl("1 Jan 2026", ui::colors::TEXT_TERTIARY(), 10));
                    ml->addLayout(mh);
                    auto* mb = lbl("Welcome to Fincept! This demo ticket shows how the support system works.\n"
                                   "Create a real ticket and our team will respond within 24 hours.",
                                   ui::colors::TEXT_PRIMARY(), 12, false, true);
                    ml->addWidget(mb);
                    mcl2->insertWidget(mcl2->count() - 1, m);
                } else {
                    set_busy(true);
                    auth::UserApi::instance().get_ticket_details(id_int, [this](auth::ApiResponse dr) {
                        set_busy(false);
                        if (!dr.success)
                            return;

                        // Robust message unwrap — try all known shapes
                        auto dr_data = dr.data;
                        if (dr_data.contains("data") && dr_data["data"].isObject())
                            dr_data = dr_data["data"].toObject();
                        QJsonArray msgs;
                        for (const auto& key : {"messages", "replies", "comments", "items"}) {
                            if (dr_data.contains(key) && dr_data[key].isArray()) {
                                msgs = dr_data[key].toArray();
                                break;
                            }
                        }

                        auto* mcl3 = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                        while (mcl3->count() > 1)
                            delete mcl3->takeAt(0)->widget();

                        for (const auto& mv : msgs) {
                            auto mo = mv.toObject();
                            // Handle various field name conventions the API may use
                            QString sender_type = mo["sender_type"].toString();
                            if (sender_type.isEmpty())
                                sender_type = mo["role"].toString();
                            if (sender_type.isEmpty())
                                sender_type = mo["type"].toString();
                            if (sender_type.isEmpty())
                                sender_type = mo["sender"].toString();
                            bool is_user = (sender_type == "user" || sender_type == "customer");
                            QString sender = is_user ? "You" : "Support Team";
                            QString bubble_color = is_user ? ui::colors::CYAN() : "#9333ea";

                            QString ts;
                            QDateTime mdt = QDateTime::fromString(mo["created_at"].toString(), Qt::ISODate);
                            if (mdt.isValid())
                                ts = mdt.toLocalTime().toString("d MMM yyyy  hh:mm");

                            auto* m = new QWidget(this);
                            m->setStyleSheet(
                                QString("background:%1;border:1px solid %2;"
                                        "border-left:3px solid %3;")
                                    .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), bubble_color));
                            auto* ml = new QVBoxLayout(m);
                            ml->setContentsMargins(14, 10, 14, 12);
                            ml->setSpacing(6);
                            auto* mh = new QHBoxLayout;
                            mh->addWidget(lbl(sender, bubble_color, 11, true));
                            mh->addStretch();
                            mh->addWidget(lbl(ts, ui::colors::TEXT_TERTIARY(), 10));
                            ml->addLayout(mh);
                            auto* mb = lbl(mo["message"].toString(), ui::colors::TEXT_PRIMARY(), 12, false, true);
                            ml->addWidget(mb);
                            mcl3->insertWidget(mcl3->count() - 1, m);
                        }

                        if (msgs.isEmpty()) {
                            mcl3->insertWidget(
                                0, lbl("No messages yet — be the first to reply.", ui::colors::TEXT_TERTIARY(), 11));
                        }

                        // Scroll to bottom
                        QTimer::singleShot(50, this, [this]() {
                            detail_scroll_->verticalScrollBar()->setValue(
                                detail_scroll_->verticalScrollBar()->maximum());
                        });
                    });
                }

                content_stack_->setCurrentIndex(2);
            });

            lay->insertWidget(lay->count() - 1, btn);
        }
    });
}

// ── on_create_ticket ──────────────────────────────────────────────────────────

void SupportScreen::on_create_ticket() {
    QString subject = subject_input_->text().trimmed();
    QString desc = desc_input_->toPlainText().trimmed();
    if (subject.isEmpty() || desc.isEmpty())
        return;

    set_busy(true);
    create_btn_->setText("Submitting…");

    auth::UserApi::instance().create_ticket(subject, desc, category_combo_->currentText().toLower().replace(' ', '_'),
                                            priority_combo_->currentText().toLower(), [this](auth::ApiResponse r) {
                                                set_busy(false);
                                                create_btn_->setText("Submit Ticket →");
                                                if (r.success) {
                                                    subject_input_->clear();
                                                    desc_input_->clear();
                                                    content_stack_->setCurrentIndex(0);
                                                    load_tickets();
                                                }
                                            });
}

// ── on_send_message ───────────────────────────────────────────────────────────

void SupportScreen::on_send_message() {
    if (selected_ticket_id_ < 0 || selected_is_demo_)
        return;
    QString msg = msg_input_->toPlainText().trimmed();
    if (msg.isEmpty())
        return;

    set_busy(true);
    send_btn_->setText("Sending…");

    auth::UserApi::instance().add_ticket_message(selected_ticket_id_, msg, [this, msg](auth::ApiResponse r) {
        set_busy(false);
        send_btn_->setText("Send Reply →");
        if (!r.success)
            return;

        msg_input_->clear();
        auto* mcl = qobject_cast<QVBoxLayout*>(messages_container_->layout());
        if (!mcl)
            return;

        auto* m = new QWidget(this);
        m->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid %3;")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::CYAN()));
        auto* ml = new QVBoxLayout(m);
        ml->setContentsMargins(14, 10, 14, 12);
        ml->setSpacing(6);
        auto* mh = new QHBoxLayout;
        mh->addWidget(lbl("You", ui::colors::CYAN(), 11, true));
        mh->addStretch();
        mh->addWidget(lbl(QDateTime::currentDateTime().toString("d MMM yyyy  hh:mm"), ui::colors::TEXT_TERTIARY(), 10));
        ml->addLayout(mh);
        auto* mb = lbl(msg, ui::colors::TEXT_PRIMARY(), 12, false, true);
        ml->addWidget(mb);
        mcl->insertWidget(mcl->count() - 1, m);

        // Scroll to bottom
        QTimer::singleShot(50, this, [this]() {
            detail_scroll_->verticalScrollBar()->setValue(detail_scroll_->verticalScrollBar()->maximum());
        });
    });
}

// ── on_close_ticket / on_reopen_ticket ────────────────────────────────────────

void SupportScreen::on_close_ticket() {
    if (selected_ticket_id_ < 0)
        return;
    set_busy(true);
    auth::UserApi::instance().update_ticket_status(selected_ticket_id_, "closed", [this](auth::ApiResponse r) {
        set_busy(false);
        if (!r.success)
            return;
        selected_is_closed_ = true;
        close_btn_->hide();
        reopen_btn_->show();
        reply_box_->hide();
        closed_box_->show();
        load_tickets();
    });
}

void SupportScreen::on_reopen_ticket() {
    if (selected_ticket_id_ < 0)
        return;
    set_busy(true);
    auth::UserApi::instance().update_ticket_status(selected_ticket_id_, "open", [this](auth::ApiResponse r) {
        set_busy(false);
        if (!r.success)
            return;
        selected_is_closed_ = false;
        reopen_btn_->hide();
        close_btn_->show();
        reply_box_->show();
        closed_box_->hide();
        load_tickets();
    });
}

} // namespace fincept::screens
