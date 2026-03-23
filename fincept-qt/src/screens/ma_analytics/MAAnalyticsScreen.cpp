// src/screens/ma_analytics/MAAnalyticsScreen.cpp
#include "screens/ma_analytics/MAAnalyticsScreen.h"
#include "screens/ma_analytics/MAModulePanel.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>

namespace fincept::screens {

using namespace fincept::services::ma;

// ── Constructor ──────────────────────────────────────────────────────────────
MAAnalyticsScreen::MAAnalyticsScreen(QWidget* parent) : QWidget(parent) {
    modules_ = all_modules();
    build_ui();
    LOG_INFO("MAAnalytics", "Screen constructed");
}

// ── Visibility lifecycle ─────────────────────────────────────────────────────
void MAAnalyticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_module_selected(0);
    }
    LOG_INFO("MAAnalytics", "Screen shown");
}

void MAAnalyticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    LOG_INFO("MAAnalytics", "Screen hidden");
}

// ── Build UI ─────────────────────────────────────────────────────────────────
void MAAnalyticsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top nav bar
    root->addWidget(build_top_bar());

    // 3-panel body
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    left_panel_ = build_left_sidebar();
    body->addWidget(left_panel_);

    // Center content stack
    content_stack_ = new QStackedWidget(this);
    content_stack_->setObjectName("maContentStack");

    // Create one panel per module
    for (int i = 0; i < modules_.size(); ++i) {
        auto* panel = new MAModulePanel(modules_[i], this);
        content_stack_->addWidget(panel);
    }
    body->addWidget(content_stack_, 1);

    right_panel_ = build_right_sidebar();
    body->addWidget(right_panel_);

    auto* body_widget = new QWidget(this);
    body_widget->setLayout(body);
    root->addWidget(body_widget, 1);

    // Status bar
    root->addWidget(build_status_bar());

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
}

// ── Top Nav Bar ──────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);
    bar->setStyleSheet(QString(
        "background:%1; border-bottom:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    // Branding badge
    auto* brand = new QLabel("M&A ANALYTICS", bar);
    brand->setStyleSheet(QString(
        "color:#FF6B35; font-size:%1px; font-weight:700; font-family:%2;"
        "padding:4px 12px; background:rgba(255,107,53,0.08);"
        "border:1px solid rgba(255,107,53,0.25); border-radius:2px;")
        .arg(ui::fonts::TINY).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(brand);

    // Divider
    auto* div = new QWidget(bar);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    hl->addWidget(div);

    // Module quick selector buttons
    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        auto* btn = new QPushButton(mod.short_label, bar);
        btn->setToolTip(mod.label);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { color:%1; font-size:9px; font-family:%2;"
            "padding:4px 8px; border:1px solid transparent; border-radius:2px;"
            "background:transparent; font-weight:400; }"
            "QPushButton:hover { background:rgba(%3,0.06); color:%4; }")
            .arg(ui::colors::TEXT_TERTIARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(QString("%1,%2,%3")
                .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
            .arg(mod.color.name()));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        hl->addWidget(btn);
        quick_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Ready badge
    auto* ready = new QLabel("READY", bar);
    ready->setStyleSheet(QString(
        "color:%1; font-size:9px; font-family:%2; padding:3px 8px;"
        "background:rgba(22,163,74,0.08); border:1px solid rgba(22,163,74,0.25);"
        "border-radius:2px;")
        .arg(ui::colors::POSITIVE).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(ready);

    auto* subtitle = new QLabel("CORPORATE FINANCE TOOLKIT", bar);
    subtitle->setStyleSheet(QString(
        "color:%1; font-size:9px; font-family:%2; letter-spacing:1px;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(subtitle);

    return bar;
}

// ── Left Sidebar ─────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_left_sidebar() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(200);
    panel->setStyleSheet(QString(
        "background:%1; border-right:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setStyleSheet(QString("border-bottom:1px solid %1;").arg(ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* title = new QLabel("MODULES", header);
    title->setStyleSheet(QString(
        "color:#FF6B35; font-size:9px; font-weight:700; font-family:%1; letter-spacing:1px;")
        .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    vl->addWidget(header);

    // Scrollable module list
    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* list_widget = new QWidget(scroll);
    left_items_layout_ = new QVBoxLayout(list_widget);
    left_items_layout_->setContentsMargins(0, 8, 0, 8);
    left_items_layout_->setSpacing(0);

    QString last_cat;
    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];

        // Category header
        if (mod.category != last_cat) {
            last_cat = mod.category;
            auto* cat_label = new QLabel(mod.category, list_widget);
            cat_label->setStyleSheet(QString(
                "color:%1; font-size:8px; font-weight:700; font-family:%2;"
                "padding:8px 12px 4px 12px; letter-spacing:1px;")
                .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
            left_items_layout_->addWidget(cat_label);
        }

        // Module button
        auto* btn = new QPushButton(mod.label, list_widget);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { text-align:left; padding:8px 12px; border:none;"
            "border-left:2px solid transparent; color:%1;"
            "font-size:10px; font-family:%2; background:transparent; }"
            "QPushButton:hover { background:rgba(%3,0.06); color:%4;"
            "border-left:2px solid %4; }")
            .arg(ui::colors::TEXT_SECONDARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
            .arg(mod.color.name()));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        left_items_layout_->addWidget(btn);
        module_buttons_.append(btn);
    }

    left_items_layout_->addStretch();
    scroll->setWidget(list_widget);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Right Sidebar ────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_right_sidebar() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(220);
    panel->setStyleSheet(QString(
        "background:%1; border-left:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setStyleSheet(QString("border-bottom:1px solid %1;").arg(ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* title = new QLabel("MODULE INFO", header);
    title->setStyleSheet(QString(
        "color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
        .arg(ui::colors::CYAN).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    vl->addWidget(header);

    // Content area
    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(12, 12, 12, 12);
    cl->setSpacing(12);

    // Module info card
    auto* info_card = new QWidget(content);
    info_card->setObjectName("maInfoCard");
    auto* icl = new QVBoxLayout(info_card);
    icl->setContentsMargins(12, 12, 12, 12);
    icl->setSpacing(6);

    right_title_ = new QLabel(content);
    right_title_->setStyleSheet(QString(
        "font-size:10px; font-weight:700; font-family:%1;")
        .arg(ui::fonts::DATA_FAMILY));
    icl->addWidget(right_title_);

    right_category_ = new QLabel(content);
    right_category_->setStyleSheet(QString(
        "color:%1; font-size:9px; font-family:%2;")
        .arg(ui::colors::TEXT_SECONDARY).arg(ui::fonts::DATA_FAMILY));
    icl->addWidget(right_category_);
    cl->addWidget(info_card);

    // Capabilities section
    auto* cap_header = new QLabel("CAPABILITIES", content);
    cap_header->setStyleSheet(QString(
        "color:#FF6B35; font-size:9px; font-weight:700; font-family:%1; letter-spacing:1px;")
        .arg(ui::fonts::DATA_FAMILY));
    cl->addWidget(cap_header);

    auto* cap_container = new QWidget(content);
    capabilities_layout_ = new QVBoxLayout(cap_container);
    capabilities_layout_->setContentsMargins(0, 0, 0, 0);
    capabilities_layout_->setSpacing(4);
    cl->addWidget(cap_container);

    // Quick stats
    auto* stats_card = new QWidget(content);
    stats_card->setStyleSheet(QString(
        "background:%1; border:1px solid %2; border-radius:2px; padding:10px;")
        .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    auto* sl = new QVBoxLayout(stats_card);
    sl->setContentsMargins(10, 10, 10, 10);
    sl->setSpacing(6);

    auto* stats_title = new QLabel("QUICK STATS", stats_card);
    stats_title->setStyleSheet(QString(
        "color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
        .arg(ui::colors::CYAN).arg(ui::fonts::DATA_FAMILY));
    sl->addWidget(stats_title);

    auto add_stat = [&](const QString& label, const QString& value) {
        auto* row = new QWidget(stats_card);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(label, row);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
            .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(value, row);
        val->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;")
            .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::DATA_FAMILY));
        rl->addWidget(lbl);
        rl->addStretch();
        rl->addWidget(val);
        sl->addWidget(row);
    };
    add_stat("Total Modules", "8");
    add_stat("Valuation Methods", "15+");
    add_stat("Python Scripts", "30+");
    add_stat("Analysis Tools", "60+");
    cl->addWidget(stats_card);

    // Tips
    auto* tips = new QLabel(
        "Click sidebar modules to switch views. "
        "Each module provides professional-grade analytics with export capabilities.",
        content);
    tips->setWordWrap(true);
    tips->setStyleSheet(QString(
        "color:%1; font-size:8px; font-family:%2; line-height:1.5;"
        "background:%3; border:1px solid %4; border-radius:2px; padding:10px;")
        .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY)
        .arg(ui::colors::BG_RAISED).arg(ui::colors::BORDER_DIM));
    cl->addWidget(tips);

    cl->addStretch();
    scroll->setWidget(content);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet(QString(
        "background:%1; border-top:1px solid %2;")
        .arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto add_item = [&](const QString& label, const QString& value, const QString& color = "") {
        auto* lbl = new QLabel(label, bar);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2;")
            .arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
        hl->addWidget(lbl);
        auto* val = new QLabel(value, bar);
        val->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
            .arg(color.isEmpty() ? QString(ui::colors::TEXT_PRIMARY) : color)
            .arg(ui::fonts::DATA_FAMILY));
        hl->addWidget(val);
    };

    add_item("MODULE:", "—");
    add_item("ENGINE:", "PYTHON + C++", ui::colors::POSITIVE);
    hl->addStretch();
    add_item("STATUS:", "READY", ui::colors::POSITIVE);

    return bar;
}

// ── Module selection ─────────────────────────────────────────────────────────
void MAAnalyticsScreen::on_module_selected(int index) {
    if (index < 0 || index >= modules_.size()) return;
    active_index_ = index;
    content_stack_->setCurrentIndex(index);
    update_sidebar_selection();
    update_right_panel();
    LOG_INFO("MAAnalytics", QString("Switched to module: %1").arg(modules_[index].label));
}

void MAAnalyticsScreen::update_sidebar_selection() {
    for (int i = 0; i < module_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        bool active = (i == active_index_);
        module_buttons_[i]->setStyleSheet(QString(
            "QPushButton { text-align:left; padding:8px 12px; border:none;"
            "border-left:2px solid %1; color:%2;"
            "font-size:10px; font-family:%3; font-weight:%4;"
            "background:%5; }"
            "QPushButton:hover { background:rgba(%6,0.1); color:%7;"
            "border-left:2px solid %7; }")
            .arg(active ? mod.color.name() : "transparent")
            .arg(active ? mod.color.name() : ui::colors::TEXT_SECONDARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(active ? "700" : "400")
            .arg(active ? QString("rgba(%1,%2,%3,0.08)")
                .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue())
                : "transparent")
            .arg(QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
            .arg(mod.color.name()));
    }

    // Update quick buttons
    for (int i = 0; i < quick_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        bool active = (i == active_index_);
        quick_buttons_[i]->setStyleSheet(QString(
            "QPushButton { color:%1; font-size:9px; font-family:%2;"
            "padding:4px 8px; border:1px solid %3; border-radius:2px;"
            "background:%4; font-weight:%5; }"
            "QPushButton:hover { background:rgba(%6,0.1); color:%7; }")
            .arg(active ? mod.color.name() : ui::colors::TEXT_TERTIARY)
            .arg(ui::fonts::DATA_FAMILY)
            .arg(active ? QString("rgba(%1,%2,%3,0.3)")
                .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue())
                : "transparent")
            .arg(active ? QString("rgba(%1,%2,%3,0.12)")
                .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue())
                : "transparent")
            .arg(active ? "700" : "400")
            .arg(QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue()))
            .arg(mod.color.name()));
    }
}

void MAAnalyticsScreen::update_right_panel() {
    const auto& mod = modules_[active_index_];
    right_title_->setText(mod.label.toUpper());
    right_title_->setStyleSheet(QString(
        "color:%1; font-size:10px; font-weight:700; font-family:%2;")
        .arg(mod.color.name()).arg(ui::fonts::DATA_FAMILY));
    right_category_->setText(mod.category + " module");

    // Update info card border
    auto* card = right_panel_->findChild<QWidget*>("maInfoCard");
    if (card) {
        card->setStyleSheet(QString(
            "background:rgba(%1,%2,%3,0.04); border:1px solid rgba(%1,%2,%3,0.18);"
            "border-left:3px solid %4; border-radius:2px;")
            .arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue())
            .arg(mod.color.name()));
    }

    // Update capabilities
    while (capabilities_layout_->count() > 0) {
        auto* item = capabilities_layout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    for (const auto& cap : module_capabilities(mod.id)) {
        auto* row = new QLabel(QString("> %1").arg(cap));
        row->setStyleSheet(QString(
            "color:%1; font-size:9px; font-family:%2;")
            .arg(ui::colors::TEXT_PRIMARY).arg(ui::fonts::DATA_FAMILY));
        capabilities_layout_->addWidget(row);
    }
}

} // namespace fincept::screens
