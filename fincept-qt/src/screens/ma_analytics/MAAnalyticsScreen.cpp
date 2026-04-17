// src/screens/ma_analytics/MAAnalyticsScreen.cpp
#include "screens/ma_analytics/MAAnalyticsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ma_analytics/MAModulePanel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>

namespace fincept::screens {

using namespace fincept::services::ma;

// ── Constructor ──────────────────────────────────────────────────────────────
MAAnalyticsScreen::MAAnalyticsScreen(QWidget* parent) : QWidget(parent) {
    modules_ = all_modules();
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
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
    status_bar_ = build_status_bar();
    root->addWidget(status_bar_);
}

// ── Top Nav Bar ──────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);
    top_bar_ = bar;

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    // Branding badge
    brand_label_ = new QLabel("M&A ANALYTICS", bar);
    brand_label_->setFixedHeight(22);
    hl->addWidget(brand_label_);

    // Divider
    auto* div = new QWidget(bar);
    div->setFixedSize(1, 20);
    div->setObjectName("maTopDivider");
    hl->addWidget(div);

    // Module quick selector buttons
    for (int i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        auto* btn = new QPushButton(mod.short_label, bar);
        btn->setToolTip(mod.label);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_module_selected(i); });
        hl->addWidget(btn);
        quick_buttons_.append(btn);
    }

    hl->addStretch(1);

    // Ready badge
    ready_label_ = new QLabel("READY", bar);
    ready_label_->setFixedHeight(22);
    hl->addWidget(ready_label_);

    subtitle_label_ = new QLabel("CORPORATE FINANCE TOOLKIT", bar);
    hl->addWidget(subtitle_label_);

    return bar;
}

// ── Left Sidebar ─────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_left_sidebar() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(200);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setObjectName("maLeftHeader");
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    modules_title_ = new QLabel("MODULES", header);
    hhl->addWidget(modules_title_);
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
            cat_label->setObjectName("maCategoryLabel");
            left_items_layout_->addWidget(cat_label);
        }

        // Module button
        auto* btn = new QPushButton(mod.label, list_widget);
        btn->setCursor(Qt::PointingHandCursor);
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

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setObjectName("maRightHeader");
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    info_title_ = new QLabel("MODULE INFO", header);
    hhl->addWidget(info_title_);
    hhl->addStretch();
    vl->addWidget(header);

    // Content area
    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(8, 8, 8, 8);
    cl->setSpacing(8);

    // Module info card
    auto* info_card = new QWidget(content);
    info_card->setObjectName("maInfoCard");
    auto* icl = new QVBoxLayout(info_card);
    icl->setContentsMargins(8, 8, 8, 8);
    icl->setSpacing(4);

    right_title_ = new QLabel(content);
    icl->addWidget(right_title_);

    right_category_ = new QLabel(content);
    icl->addWidget(right_category_);
    cl->addWidget(info_card);

    // Capabilities section
    cap_header_ = new QLabel("CAPABILITIES", content);
    cl->addWidget(cap_header_);

    auto* cap_container = new QWidget(content);
    capabilities_layout_ = new QVBoxLayout(cap_container);
    capabilities_layout_->setContentsMargins(0, 0, 0, 0);
    capabilities_layout_->setSpacing(4);
    cl->addWidget(cap_container);

    // Quick stats
    stats_card_ = new QWidget(content);
    auto* sl = new QVBoxLayout(stats_card_);
    sl->setContentsMargins(8, 8, 8, 8);
    sl->setSpacing(4);

    stats_title_ = new QLabel("QUICK STATS", stats_card_);
    sl->addWidget(stats_title_);

    auto add_stat = [&](const QString& label, const QString& value) {
        auto* row = new QWidget(stats_card_);
        row->setObjectName("maStatRow");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(label, row);
        lbl->setObjectName("maStatLabel");
        auto* val = new QLabel(value, row);
        val->setObjectName("maStatValue");
        rl->addWidget(lbl);
        rl->addStretch();
        rl->addWidget(val);
        sl->addWidget(row);
    };
    add_stat("Total Modules", "8");
    add_stat("Valuation Methods", "15+");
    add_stat("Python Scripts", "30+");
    add_stat("Analysis Tools", "60+");
    cl->addWidget(stats_card_);

    // Tips
    tips_label_ = new QLabel("Click sidebar modules to switch views. "
                             "Each module provides professional-grade analytics with export capabilities.",
                             content);
    tips_label_->setWordWrap(true);
    cl->addWidget(tips_label_);

    cl->addStretch();
    scroll->setWidget(content);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────
QWidget* MAAnalyticsScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto add_item = [&](const QString& label, const QString& value, const QString& obj_name) {
        auto* lbl = new QLabel(label, bar);
        lbl->setObjectName("maStatusLabel");
        hl->addWidget(lbl);
        auto* val = new QLabel(value, bar);
        val->setObjectName(obj_name);
        hl->addWidget(val);
    };

    add_item("MODULE:", QString::fromUtf8("\xe2\x80\x94"), "maStatusModule");
    add_item("ENGINE:", "PYTHON + C++", "maStatusEngine");
    hl->addStretch();
    add_item("STATUS:", "READY", "maStatusReady");

    return bar;
}

// ── Module selection ─────────────────────────────────────────────────────────
void MAAnalyticsScreen::on_module_selected(int index) {
    if (index < 0 || index >= modules_.size())
        return;
    active_index_ = index;
    content_stack_->setCurrentIndex(index);
    update_sidebar_selection();
    update_right_panel();
    LOG_INFO("MAAnalytics", QString("Switched to module: %1").arg(modules_[index].label));
    ScreenStateManager::instance().notify_changed(this);
}

void MAAnalyticsScreen::update_sidebar_selection() {
    for (int i = 0; i < module_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        bool active = (i == active_index_);
        auto rgb = QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue());
        module_buttons_[i]->setStyleSheet(QString("QPushButton { text-align:left; padding:8px 12px; border:none;"
                                                  "border-left:2px solid %1; color:%2;"
                                                  "font-size:%3px; font-family:%4; font-weight:%5;"
                                                  "background:%6; }"
                                                  "QPushButton:hover { background:rgba(%7,0.1); color:%8;"
                                                  "border-left:2px solid %8; }")
                                              .arg(active ? mod.color.name() : "transparent")
                                              .arg(active ? mod.color.name() : ui::colors::TEXT_SECONDARY())
                                              .arg(ui::fonts::SMALL)
                                              .arg(ui::fonts::DATA_FAMILY)
                                              .arg(active ? "700" : "400")
                                              .arg(active ? QString("rgba(%1,0.08)").arg(rgb) : "transparent")
                                              .arg(rgb)
                                              .arg(mod.color.name()));
    }

    // Update quick buttons
    for (int i = 0; i < quick_buttons_.size(); ++i) {
        const auto& mod = modules_[i];
        bool active = (i == active_index_);
        auto rgb = QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue());
        quick_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                                 "padding:4px 8px; border:1px solid %4;"
                                                 "background:%5; font-weight:%6; }"
                                                 "QPushButton:hover { background:rgba(%7,0.1); color:%8; }")
                                             .arg(active ? mod.color.name() : ui::colors::TEXT_TERTIARY())
                                             .arg(ui::fonts::TINY)
                                             .arg(ui::fonts::DATA_FAMILY)
                                             .arg(active ? QString("rgba(%1,0.3)").arg(rgb) : "transparent")
                                             .arg(active ? QString("rgba(%1,0.12)").arg(rgb) : "transparent")
                                             .arg(active ? "700" : "400")
                                             .arg(rgb)
                                             .arg(mod.color.name()));
    }
}

void MAAnalyticsScreen::update_right_panel() {
    const auto& mod = modules_[active_index_];
    auto rgb = QString("%1,%2,%3").arg(mod.color.red()).arg(mod.color.green()).arg(mod.color.blue());

    right_title_->setText(mod.label.toUpper());
    right_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                    .arg(mod.color.name())
                                    .arg(ui::fonts::SMALL)
                                    .arg(ui::fonts::DATA_FAMILY));
    right_category_->setText(mod.category + " module");
    right_category_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                       .arg(ui::colors::TEXT_SECONDARY())
                                       .arg(ui::fonts::TINY)
                                       .arg(ui::fonts::DATA_FAMILY));

    // Update info card border
    auto* card = right_panel_->findChild<QWidget*>("maInfoCard");
    if (card) {
        card->setStyleSheet(QString("background:rgba(%1,0.04); border:1px solid rgba(%1,0.18);"
                                    "border-left:3px solid %2;")
                                .arg(rgb)
                                .arg(mod.color.name()));
    }

    // Update capabilities
    while (capabilities_layout_->count() > 0) {
        auto* item = capabilities_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    for (const auto& cap : module_capabilities(mod.id)) {
        auto* row = new QLabel(QString("> %1").arg(cap));
        row->setWordWrap(true);
        row->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_PRIMARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY));
        capabilities_layout_->addWidget(row);
    }
}

// ── Theme refresh ───────────────────────────────────────────────────────────
void MAAnalyticsScreen::refresh_theme() {
    // Root background
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    // Top bar
    if (top_bar_)
        top_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    // Brand badge — uses theme accent
    if (brand_label_)
        brand_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                            "padding:2px 6px; background:rgba(%4,0.08);"
                                            "border:1px solid rgba(%4,0.25);")
                                        .arg(ui::colors::AMBER())
                                        .arg(ui::fonts::TINY)
                                        .arg(ui::fonts::DATA_FAMILY())
                                        .arg([]() {
                                            QColor c(ui::colors::AMBER());
                                            return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
                                        }()));

    // Top bar divider
    auto* div = top_bar_ ? top_bar_->findChild<QWidget*>("maTopDivider") : nullptr;
    if (div)
        div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));

    // Ready badge
    if (ready_label_) {
        QColor pos(ui::colors::POSITIVE());
        ready_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                                            "background:rgba(%4,0.08); border:1px solid rgba(%4,0.25);")
                                        .arg(ui::colors::POSITIVE())
                                        .arg(ui::fonts::TINY)
                                        .arg(ui::fonts::DATA_FAMILY())
                                        .arg(QString("%1,%2,%3").arg(pos.red()).arg(pos.green()).arg(pos.blue())));
    }

    // Subtitle
    if (subtitle_label_)
        subtitle_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; letter-spacing:1px;")
                                           .arg(ui::colors::TEXT_TERTIARY())
                                           .arg(ui::fonts::TINY)
                                           .arg(ui::fonts::DATA_FAMILY()));

    // Left panel
    if (left_panel_)
        left_panel_->setStyleSheet(QString("background:%1; border-right:1px solid %2;")
                                       .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // Left header
    auto* left_hdr = left_panel_ ? left_panel_->findChild<QWidget*>("maLeftHeader") : nullptr;
    if (left_hdr)
        left_hdr->setStyleSheet(QString("border-bottom:1px solid %1;").arg(ui::colors::BORDER_DIM()));

    // Modules title — uses theme accent
    if (modules_title_)
        modules_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(ui::colors::AMBER())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY()));

    // Category labels in sidebar
    if (left_panel_) {
        auto cat_labels = left_panel_->findChildren<QLabel*>("maCategoryLabel");
        for (auto* cat : cat_labels)
            cat->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       "padding:8px 12px 4px 12px; letter-spacing:1px;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY()));
    }

    // Right panel
    if (right_panel_)
        right_panel_->setStyleSheet(QString("background:%1; border-left:1px solid %2;")
                                        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // Right header
    auto* right_hdr = right_panel_ ? right_panel_->findChild<QWidget*>("maRightHeader") : nullptr;
    if (right_hdr)
        right_hdr->setStyleSheet(QString("border-bottom:1px solid %1;").arg(ui::colors::BORDER_DIM()));

    // Info title
    if (info_title_)
        info_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(ui::colors::CYAN())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY()));

    // Capabilities header — uses theme accent
    if (cap_header_)
        cap_header_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(ui::colors::AMBER())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY()));

    // Stats card
    if (stats_card_)
        stats_card_->setStyleSheet(QString("background:%1; border:1px solid %2; padding:8px;")
                                       .arg(ui::colors::BG_RAISED())
                                       .arg(ui::colors::BORDER_DIM()));

    // Stats title
    if (stats_title_)
        stats_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(ui::colors::CYAN())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY()));

    // Stat labels and values
    if (stats_card_) {
        auto stat_labels = stats_card_->findChildren<QLabel*>("maStatLabel");
        for (auto* lbl : stat_labels)
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY()));
        auto stat_values = stats_card_->findChildren<QLabel*>("maStatValue");
        for (auto* val : stat_values)
            val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                   .arg(ui::colors::TEXT_PRIMARY())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY()));
    }

    // Tips label
    if (tips_label_)
        tips_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; line-height:1.4;"
                                           "background:%4; border:1px solid %5; padding:8px;")
                                       .arg(ui::colors::TEXT_TERTIARY())
                                       .arg(ui::fonts::TINY)
                                       .arg(ui::fonts::DATA_FAMILY())
                                       .arg(ui::colors::BG_RAISED())
                                       .arg(ui::colors::BORDER_DIM()));

    // Status bar
    if (status_bar_)
        status_bar_->setStyleSheet(
            QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    // Status bar labels
    if (status_bar_) {
        auto status_labels = status_bar_->findChildren<QLabel*>("maStatusLabel");
        for (auto* lbl : status_labels)
            lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                   .arg(ui::colors::TEXT_TERTIARY())
                                   .arg(ui::fonts::TINY)
                                   .arg(ui::fonts::DATA_FAMILY()));
        auto* mod_val = status_bar_->findChild<QLabel*>("maStatusModule");
        if (mod_val)
            mod_val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                       .arg(ui::colors::TEXT_PRIMARY())
                                       .arg(ui::fonts::TINY)
                                       .arg(ui::fonts::DATA_FAMILY()));
        auto* eng_val = status_bar_->findChild<QLabel*>("maStatusEngine");
        if (eng_val)
            eng_val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                       .arg(ui::colors::POSITIVE())
                                       .arg(ui::fonts::TINY)
                                       .arg(ui::fonts::DATA_FAMILY()));
        auto* ready_val = status_bar_->findChild<QLabel*>("maStatusReady");
        if (ready_val)
            ready_val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                                         .arg(ui::colors::POSITIVE())
                                         .arg(ui::fonts::TINY)
                                         .arg(ui::fonts::DATA_FAMILY()));
    }

    // Re-apply sidebar selection styles with current theme colors
    update_sidebar_selection();
    update_right_panel();

    // Propagate to all module panels
    for (int i = 0; i < content_stack_->count(); ++i) {
        auto* panel = qobject_cast<MAModulePanel*>(content_stack_->widget(i));
        if (panel)
            panel->refresh_theme();
    }
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap MAAnalyticsScreen::save_state() const {
    return {{"module_index", active_index_}};
}

void MAAnalyticsScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("module_index", 0).toInt();
    if (idx >= 0 && idx < modules_.size())
        on_module_selected(idx);
}

} // namespace fincept::screens
