#include "ui/navigation/FKeyBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QEvent>
#include <QStyle>
#include <QVariant>

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

    tab_defs_ = {
        {"dashboard", "DASHBOARD"},   {"markets", "MARKETS"},      {"crypto_trading", "CRYPTO"},
        {"equity_trading", "EQUITY"}, {"portfolio", "PORTFOLIO"},  {"news", "NEWS"},
        {"ai_chat", "AI CHAT"},       {"backtesting", "BACKTEST"}, {"algo_trading", "ALGO"},
        {"node_editor", "NODES"},     {"code_editor", "CODE"},     {"ai_quant_lab", "QUANT LAB"},
        {"quantlib", "QUANTLIB"},     {"settings", "SETTINGS"},    {"profile", "PROFILE"},
    };
    for (const auto& def : tab_defs_)
        add_tab(def);
    scroll_area->setWidget(container);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll_area);

    retranslateUi();

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void TabBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(e);
}

void TabBar::retranslateUi() {
    // Tab definitions and button order are stable — walk in parallel and
    // reapply translated text + tooltip via the source label as the tr() key.
    for (int i = 0; i < tab_defs_.size() && i < tab_buttons_.size(); ++i) {
        const QString text = tr(tab_defs_[i].source_label.toUtf8().constData());
        tab_buttons_[i]->setText(text);
        tab_buttons_[i]->setToolTip(text);
    }
}

void TabBar::add_tab(const TabDef& def) {
    auto* btn = new QPushButton(def.source_label);
    btn->setFixedHeight(32);
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("tab_id", def.id);
    btn->setToolTip(def.source_label);
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
    // One sheet for the whole bar. The per-button setStyleSheet() this replaces ran
    // for ALL tabs on every navigation (set_active -> update_styles), i.e. a CSS
    // re-parse per tab per screen change. The active tab is now a dynamic property
    // ("active") matched by an attribute selector, so a navigation only re-polishes
    // the two buttons whose state actually changed. The bare `*` rule keeps the
    // previous selector-less look (it applied to the bar and its children). The
    // active rule comes last so it also wins over :hover, as before.
    setStyleSheet(QString("*{background:%1;border-bottom:1px solid %2;}"
                          "QPushButton{background:transparent;color:%3;border:none;"
                          "padding:0 8px;letter-spacing:0.5px;}"
                          "QPushButton:hover{color:%4;background:%5;}"
                          "QPushButton[active=\"true\"]{background:%4;color:%3;border:1px solid %6;"
                          "padding:0 8px;font-weight:600;letter-spacing:0.5px;}")
                      .arg(colors::BG_BASE())
                      .arg(colors::BORDER_DIM())
                      .arg(colors::TEXT_PRIMARY())
                      .arg(colors::AMBER())
                      .arg(colors::BG_RAISED())
                      .arg(colors::AMBER_DIM()));
    update_styles();
}

void TabBar::update_styles() {
    for (auto* btn : tab_buttons_) {
        const bool active = btn->property("tab_id").toString() == active_id_;
        const QVariant current = btn->property("active");
        if (current.isValid() && current.toBool() == active)
            continue; // unchanged — nothing to re-polish
        btn->setProperty("active", active);
        // Dynamic-property selectors are only re-evaluated on a fresh polish.
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

} // namespace fincept::ui
