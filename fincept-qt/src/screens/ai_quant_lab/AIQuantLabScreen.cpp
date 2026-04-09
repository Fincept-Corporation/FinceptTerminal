// src/screens/ai_quant_lab/AIQuantLabScreen.cpp
#include "screens/ai_quant_lab/AIQuantLabScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens {

using namespace fincept::services::quant;

// ── Constructor ───────────────────────────────────────────────────────────────

AIQuantLabScreen::AIQuantLabScreen(QWidget* parent) : QWidget(parent) {
    modules_ = all_quant_modules();
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });
    LOG_INFO("AIQuantLab", "Screen constructed");
}

void AIQuantLabScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_module_selected(0);
    }
}

void AIQuantLabScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

// ── UI construction ───────────────────────────────────────────────────────────

void AIQuantLabScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_top_bar());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    body->addWidget(build_left_sidebar());

    // Content stack — panels are lazy-constructed via get_or_create_panel()
    content_stack_ = new QStackedWidget(this);
    body->addWidget(content_stack_, 1);

    body->addWidget(build_right_sidebar());

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);

    root->addWidget(build_status_bar());

    refresh_theme();
}

// ── Top bar ───────────────────────────────────────────────────────────────────
// Badge bar: compact scrollable row of module short-labels (like EconomicsScreen).
// Replaces the old overflowing QHBoxLayout of 24 buttons.

QWidget* AIQuantLabScreen::build_top_bar() {
    top_bar_ = new QWidget(this);
    top_bar_->setFixedHeight(40);

    auto* hl = new QHBoxLayout(top_bar_);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(8);

    // Brand chip
    auto* brand = new QLabel("AI QUANT LAB", top_bar_);
    brand->setObjectName("aqBrand");
    hl->addWidget(brand);

    // Separator
    auto* div = new QWidget(top_bar_);
    div->setFixedSize(1, 20);
    div->setObjectName("aqTopDiv");
    hl->addWidget(div);

    // Scrollable badge bar
    badge_scroll_ = new QScrollArea(top_bar_);
    badge_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    badge_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    badge_scroll_->setFrameShape(QFrame::NoFrame);
    badge_scroll_->setWidgetResizable(true);
    badge_scroll_->setFixedHeight(34);

    badge_bar_ = new QWidget;
    auto* bhl = new QHBoxLayout(badge_bar_);
    bhl->setContentsMargins(0, 3, 0, 3);
    bhl->setSpacing(3);

    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        auto* btn = new QPushButton(mod.short_label, badge_bar_);
        btn->setToolTip(mod.label);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(26);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        bhl->addWidget(btn);
        badge_buttons_.append(btn);
    }
    bhl->addStretch();

    badge_scroll_->setWidget(badge_bar_);
    hl->addWidget(badge_scroll_, 1);

    // Module count chip
    auto* count_lbl = new QLabel(QString("%1 MODULES").arg(modules_.size()), top_bar_);
    count_lbl->setObjectName("aqModuleCount");
    hl->addWidget(count_lbl);

    return top_bar_;
}

// ── Left sidebar ──────────────────────────────────────────────────────────────

QWidget* AIQuantLabScreen::build_left_sidebar() {
    left_panel_ = new QWidget(this);
    left_panel_->setFixedWidth(200);

    auto* vl = new QVBoxLayout(left_panel_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(left_panel_);
    header->setObjectName("aqSidebarHeader");
    header->setFixedHeight(36);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    sidebar_title_ = new QLabel("MODULES", header);
    sidebar_title_->setObjectName("aqSidebarTitle");
    hhl->addWidget(sidebar_title_);
    hhl->addStretch();
    vl->addWidget(header);

    // Scrollable module list
    auto* scroll = new QScrollArea(left_panel_);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }"
                          "QScrollBar:vertical { width:4px; }"
                          "QScrollBar::handle:vertical { border-radius:2px; }");

    auto* list = new QWidget(scroll);
    left_items_layout_ = new QVBoxLayout(list);
    left_items_layout_->setContentsMargins(0, 8, 0, 8);
    left_items_layout_->setSpacing(0);

    QString last_cat;
    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        if (mod.category != last_cat) {
            last_cat = mod.category;
            QString cat_label = mod.category;
            cat_label.replace('_', '/');
            auto* cat = new QLabel(cat_label, list);
            cat->setObjectName("aqCatLabel");
            left_items_layout_->addWidget(cat);
        }

        auto* btn = new QPushButton(mod.label, list);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        left_items_layout_->addWidget(btn);
        module_buttons_.append(btn);
    }
    left_items_layout_->addStretch();
    scroll->setWidget(list);
    vl->addWidget(scroll, 1);

    return left_panel_;
}

// ── Right sidebar ─────────────────────────────────────────────────────────────

QWidget* AIQuantLabScreen::build_right_sidebar() {
    right_panel_ = new QWidget(this);
    right_panel_->setFixedWidth(220);

    auto* vl = new QVBoxLayout(right_panel_);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    auto* info_title = new QLabel("MODULE INFO", right_panel_);
    info_title->setObjectName("aqInfoTitle");
    vl->addWidget(info_title);

    right_title_ = new QLabel(right_panel_);
    right_title_->setObjectName("aqRightTitle");
    vl->addWidget(right_title_);

    right_category_ = new QLabel(right_panel_);
    right_category_->setObjectName("aqRightCategory");
    vl->addWidget(right_category_);

    right_desc_ = new QLabel(right_panel_);
    right_desc_->setObjectName("aqRightDesc");
    right_desc_->setWordWrap(true);
    vl->addWidget(right_desc_);

    right_script_ = new QLabel(right_panel_);
    right_script_->setObjectName("aqRightScript");
    vl->addWidget(right_script_);

    // Platform stats card
    stats_card_ = new QWidget(right_panel_);
    stats_card_->setObjectName("aqStatsCard");
    auto* svl = new QVBoxLayout(stats_card_);
    svl->setContentsMargins(10, 10, 10, 10);
    svl->setSpacing(4);

    stats_title_ = new QLabel("PLATFORM STATS", stats_card_);
    stats_title_->setObjectName("aqStatsTitle");
    svl->addWidget(stats_title_);

    auto add_stat = [&](const QString& label, const QString& value) {
        auto* row = new QWidget(stats_card_);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(label, row);
        lbl->setObjectName("aqStatLabel");
        auto* val = new QLabel(value, row);
        val->setObjectName("aqStatValue");
        rl->addWidget(lbl);
        rl->addStretch();
        rl->addWidget(val);
        svl->addWidget(row);
    };
    add_stat("Modules",       QString::number(modules_.size()));
    add_stat("ML Models",     "30+");
    add_stat("RL Algorithms", "5");
    add_stat("Python Scripts","25+");
    vl->addWidget(stats_card_);

    vl->addStretch();
    return right_panel_;
}

// ── Status bar ────────────────────────────────────────────────────────────────

QWidget* AIQuantLabScreen::build_status_bar() {
    status_bar_ = new QWidget(this);
    status_bar_->setFixedHeight(24);

    auto* hl = new QHBoxLayout(status_bar_);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto* engine_lbl = new QLabel("ENGINE:", status_bar_);
    engine_lbl->setObjectName("aqStatusLabel");
    status_engine_val_ = new QLabel("QLIB + GS QUANT + PYTHON", status_bar_);
    status_engine_val_->setObjectName("aqStatusValue");
    hl->addWidget(engine_lbl);
    hl->addWidget(status_engine_val_);
    hl->addStretch();

    status_ready_lbl_ = new QLabel("READY", status_bar_);
    status_ready_lbl_->setObjectName("aqStatusReady");
    hl->addWidget(status_ready_lbl_);

    return status_bar_;
}

// ── Theme refresh ─────────────────────────────────────────────────────────────

void AIQuantLabScreen::refresh_theme() {
    using namespace ui::colors;
    using namespace ui::fonts;

    // Root
    setStyleSheet(QString("background:%1;").arg(BG_BASE()));

    // Top bar
    top_bar_->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(BG_RAISED(), BORDER_DIM()));

    // Brand chip
    // findChild is safe here — it's a named child of top_bar_
    if (auto* brand = top_bar_->findChild<QLabel*>("aqBrand"))
        brand->setStyleSheet(
            QString("color:%1; font-weight:700; font-size:%2px;"
                    "padding:3px 10px; background:rgba(157,78,221,0.10);"
                    "border:1px solid rgba(157,78,221,0.28); border-radius:2px;")
                .arg(INFO(), TINY));

    if (auto* div = top_bar_->findChild<QWidget*>("aqTopDiv"))
        div->setStyleSheet(QString("background:%1;").arg(BORDER_DIM()));

    // Badge scroll area + bar
    badge_scroll_->setStyleSheet(
        QString("QScrollArea { background:%1; border:none; }"
                "QScrollBar:horizontal { height:0px; }").arg(BG_RAISED()));
    badge_bar_->setStyleSheet(
        QString("background:%1;").arg(BG_RAISED()));

    if (auto* lbl = top_bar_->findChild<QLabel*>("aqModuleCount"))
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700;"
                    "padding:3px 8px; background:rgba(22,163,74,0.08);"
                    "border:1px solid rgba(22,163,74,0.25); border-radius:2px;")
                .arg(POSITIVE(), TINY));

    // Left sidebar
    left_panel_->setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(BG_SURFACE(), BORDER_DIM()));

    if (auto* hdr = left_panel_->findChild<QWidget*>("aqSidebarHeader"))
        hdr->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(BG_SURFACE(), BORDER_DIM()));

    sidebar_title_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; letter-spacing:1px;")
            .arg(CYAN(), TINY));

    // Category labels
    for (auto* lbl : left_panel_->findChildren<QLabel*>("aqCatLabel"))
        lbl->setStyleSheet(
            QString("color:%1; font-weight:700; font-size:%2px;"
                    "padding:8px 12px 4px 12px; letter-spacing:1px; background:transparent;")
                .arg(TEXT_TERTIARY(), TINY));

    // Right sidebar
    right_panel_->setStyleSheet(
        QString("background:%1; border-left:1px solid %2;").arg(BG_SURFACE(), BORDER_DIM()));

    if (auto* t = right_panel_->findChild<QLabel*>("aqInfoTitle"))
        t->setStyleSheet(
            QString("color:%1; font-weight:700; font-size:%2px; letter-spacing:1px;")
                .arg(CYAN(), TINY));

    right_category_->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_SECONDARY(), TINY));

    right_desc_->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_PRIMARY(), TINY));

    right_script_->setStyleSheet(
        QString("color:%1; font-size:%2px; padding:6px;"
                "background:%3; border:1px solid %4; border-radius:2px;")
            .arg(TEXT_TERTIARY(), QString::number(TINY), BG_RAISED(), BORDER_DIM()));

    stats_card_->setStyleSheet(
        QString("background:%1; border:1px solid %2; border-radius:3px;")
            .arg(BG_RAISED(), BORDER_DIM()));

    stats_title_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; letter-spacing:0.5px;")
            .arg(CYAN(), TINY));

    for (auto* lbl : stats_card_->findChildren<QLabel*>("aqStatLabel"))
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_TERTIARY(), TINY));

    for (auto* val : stats_card_->findChildren<QLabel*>("aqStatValue"))
        val->setStyleSheet(
            QString("color:%1; font-weight:700; font-size:%2px; background:transparent;")
                .arg(TEXT_PRIMARY(), TINY));

    // Status bar
    status_bar_->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(BG_RAISED(), BORDER_DIM()));

    for (auto* lbl : status_bar_->findChildren<QLabel*>("aqStatusLabel"))
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; background:transparent;").arg(TEXT_TERTIARY(), TINY));

    status_engine_val_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; background:transparent;")
            .arg(POSITIVE(), TINY));

    status_ready_lbl_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:%2px; background:transparent;")
            .arg(POSITIVE(), TINY));

    // Re-apply selection styles so active module stays highlighted after theme switch
    update_sidebar_selection();
}

// ── Lazy panel construction ───────────────────────────────────────────────────

QuantModulePanel* AIQuantLabScreen::get_or_create_panel(int index) {
    if (panels_.contains(index))
        return panels_[index];

    auto* panel = new QuantModulePanel(modules_[index], nullptr);
    panels_[index] = panel;
    content_stack_->addWidget(panel);
    LOG_INFO("AIQuantLab", "Created panel for: " + modules_[index].id);
    return panel;
}

// ── Module selection ──────────────────────────────────────────────────────────

void AIQuantLabScreen::on_module_selected(int index) {
    if (index < 0 || index >= modules_.size())
        return;
    active_index_ = index;

    QuantModulePanel* panel = get_or_create_panel(index);
    content_stack_->setCurrentWidget(panel);

    update_sidebar_selection();
    update_right_panel();
    ScreenStateManager::instance().notify_changed(this);
}

void AIQuantLabScreen::update_sidebar_selection() {
    using namespace ui::colors;

    for (int i = 0; i < module_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        const bool active = (i == active_index_);
        const QString rgb = QString("%1,%2,%3")
            .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue());

        module_buttons_[i]->setStyleSheet(
            QString("QPushButton { text-align:left; padding:6px 12px; border:none;"
                    "border-left:2px solid %1; color:%2; font-size:10px;"
                    "font-weight:%3; background:%4; }"
                    "QPushButton:hover { background:rgba(%5,0.08); color:%6; }")
                .arg(active ? mod.color.name() : "transparent")
                .arg(active ? mod.color.name() : QString(TEXT_SECONDARY()))
                .arg(active ? "700" : "400")
                .arg(active ? QString("rgba(%1,0.08)").arg(rgb) : "transparent")
                .arg(rgb, mod.color.name()));
    }

    for (int i = 0; i < badge_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        const bool active = (i == active_index_);
        const QString rgb = QString("%1,%2,%3")
            .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue());

        badge_buttons_[i]->setStyleSheet(
            QString("QPushButton { color:%1; font-size:9px; font-weight:%2;"
                    "padding:0 8px; border:1px solid %3; border-radius:2px; background:%4; }"
                    "QPushButton:hover { color:%5; border-color:rgba(%6,0.4);"
                    "background:rgba(%6,0.08); }")
                .arg(active ? mod.color.name() : QString(TEXT_TERTIARY()))
                .arg(active ? "700" : "400")
                .arg(active ? QString("rgba(%1,0.35)").arg(rgb) : "transparent")
                .arg(active ? QString("rgba(%1,0.12)").arg(rgb) : "transparent")
                .arg(mod.color.name(), rgb));
    }
}

void AIQuantLabScreen::update_right_panel() {
    const auto& mod = modules_[active_index_];
    right_title_->setText(mod.label.toUpper());
    right_title_->setStyleSheet(
        QString("color:%1; font-weight:700; font-size:11px; background:transparent;")
            .arg(mod.color.name()));
    right_category_->setText(QString(mod.category).replace('_', '/') + " module");
    right_desc_->setText(mod.description);
    right_script_->setText("Script: " + mod.script);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AIQuantLabScreen::save_state() const {
    return {{"module_index", active_index_}};
}

void AIQuantLabScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("module_index", 0).toInt();
    if (idx >= 0 && idx < modules_.size())
        on_module_selected(idx);
}

} // namespace fincept::screens
