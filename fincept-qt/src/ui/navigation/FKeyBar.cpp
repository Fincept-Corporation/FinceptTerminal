#include "ui/navigation/FKeyBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

namespace fincept::ui {

TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* container = new QWidget(scroll_area);
    tab_layout_ = new QHBoxLayout(container);
    tab_layout_->setContentsMargins(4, 0, 4, 0);
    tab_layout_->setSpacing(2);

    QVector<TabDef> tabs = {
        {"dashboard", "DASHBOARD"}, {"markets", "MARKETS"},   {"crypto_trading", "CRYPTO"},  {"portfolio", "PORTFOLIO"},
        {"news", "NEWS"},           {"ai_chat", "AI CHAT"},   {"backtesting", "BACKTEST"},   {"algo_trading", "ALGO"},
        {"node_editor", "NODES"},   {"code_editor", "CODE"},  {"ai_quant_lab", "QUANT LAB"}, {"quantlib", "QUANTLIB"},
        {"forum", "FORUM"},         {"settings", "SETTINGS"}, {"profile", "PROFILE"},
    };
    for (const auto& def : tabs)
        add_tab(def);
    scroll_area->setWidget(container);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll_area);

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void TabBar::add_tab(const TabDef& def) {
    auto* btn = new QPushButton(def.label);
    btn->setFixedHeight(32);
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("tab_id", def.id);
    btn->setToolTip(def.label);
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

void TabBar::refresh_theme() {
    setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(colors::BG_BASE()).arg(colors::BORDER_DIM()));
    update_styles();
}

void TabBar::update_styles() {
    for (auto* btn : tab_buttons_) {
        bool active = btn->property("tab_id").toString() == active_id_;
        btn->setStyleSheet(active ? QString("QPushButton{background:%1;color:%2;border:1px solid %3;"
                                            "padding:0 8px;font-weight:600;letter-spacing:0.5px;}")
                                        .arg(colors::AMBER())
                                        .arg(colors::TEXT_PRIMARY())
                                        .arg(colors::AMBER_DIM())
                                  : QString("QPushButton{background:transparent;color:%1;border:none;"
                                            "padding:0 8px;letter-spacing:0.5px;}"
                                            "QPushButton:hover{color:%2;background:%3;}")
                                        .arg(colors::TEXT_PRIMARY())
                                        .arg(colors::AMBER())
                                        .arg(colors::BG_RAISED()));
    }
}

} // namespace fincept::ui
