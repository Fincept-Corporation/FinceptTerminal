#include "ui/navigation/FKeyBar.h"

#include "ui/theme/Theme.h"

namespace fincept::ui {

TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet("background:#0e0e0e;border-bottom:1px solid #1a1a1a;");
    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* container = new QWidget;
    tab_layout_ = new QHBoxLayout(container);
    tab_layout_->setContentsMargins(4, 0, 4, 0);
    tab_layout_->setSpacing(0);

    // All 14 tabs matching Tauri's tab bar
    QVector<TabDef> tabs = {
        {"dashboard", "DASHBOARD"},
        {"markets", "MARKETS"},
        {"crypto_trading", "CRYPTO TRADING"},
        {"portfolio", "PORTFOLIO"},
        {"news", "NEWS"},
        {"ai_chat", "AI CHAT"},
        {"backtesting", "BACKTESTING"},
        {"algo_trading", "ALGO TRADING"},
        {"node_editor", "NODE EDITOR"},
        {"code_editor", "CODE EDITOR"},
        {"ai_quant_lab", "AI QUANT LAB"},
        {"quantlib", "QUANTLIB"},
        {"forum", "FORUM"},
        {"settings", "SETTINGS"},
        {"profile", "PROFILE"},
    };
    for (const auto& def : tabs)
        add_tab(def);
    scroll_area->setWidget(container);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll_area);
    update_styles();
}

void TabBar::add_tab(const TabDef& def) {
    auto* btn = new QPushButton(def.label);
    btn->setFixedHeight(32);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("tab_id", def.id);
    connect(btn, &QPushButton::clicked, this, [this, id = def.id]() {
        set_active(id);
        emit tab_changed(id);
    });
    tab_layout_->addWidget(btn);
    tab_buttons_.append(btn);
}

void TabBar::set_active(const QString& tab_id) {
    if (active_id_ == tab_id)
        return;
    active_id_ = tab_id;
    update_styles();
}

void TabBar::update_styles() {
    for (auto* btn : tab_buttons_) {
        bool active = btn->property("tab_id").toString() == active_id_;
        btn->setStyleSheet(active ? "QPushButton{background:#b45309;color:#e5e5e5;border:1px solid #4d3300;"
                                    "padding:0 12px;font-size:13px;font-weight:600;letter-spacing:1px;"
                                    "font-family:'Consolas',monospace;}"
                                  : "QPushButton{background:transparent;color:#ffffff;border:none;"
                                    "padding:0 12px;font-size:13px;letter-spacing:0.5px;"
                                    "font-family:'Consolas',monospace;}"
                                    "QPushButton:hover{color:#d97706;background:#111111;}");
    }
}

} // namespace fincept::ui
