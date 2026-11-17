#include "ui/navigation/FKeyBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/ThemeIcon.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>

namespace fincept::ui {

TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    metrics::follow_fixed_height(this, 32); // grows with the user's font
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
    scroll_ = scroll_area;
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(scroll_area, 1);

    // Tabs that do not fit (the scroll area shows no scrollbar) used to be
    // simply out of reach — SETTINGS / PROFILE at a 24 px font. This button
    // appears only then and lists them.
    overflow_btn_ = new QToolButton(this);
    overflow_btn_->setIcon(theme_icon(ThemeGlyph::ChevronRight));
    overflow_btn_->setCursor(Qt::PointingHandCursor);
    overflow_btn_->setPopupMode(QToolButton::InstantPopup);
    overflow_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* overflow_menu = new QMenu(overflow_btn_);
    overflow_btn_->setMenu(overflow_menu);
    connect(overflow_menu, &QMenu::aboutToShow, this, [this, overflow_menu]() {
        overflow_menu->clear();
        const int view_left = scroll_->horizontalScrollBar()->value();
        const int view_right = view_left + scroll_->viewport()->width();
        for (int i = 0; i < tab_buttons_.size() && i < tab_defs_.size(); ++i) {
            QPushButton* btn = tab_buttons_[i];
            const QRect g = btn->geometry();
            if (g.left() >= view_left && g.right() <= view_right)
                continue; // fully in view
            const QString id = tab_defs_[i].id;
            QAction* a = overflow_menu->addAction(literal_ampersands(btn->text()), this, [this, id, btn]() {
                set_active(id);
                scroll_->ensureWidgetVisible(btn);
                emit tab_changed(id);
            });
            a->setCheckable(true);
            a->setChecked(id == active_id_);
        }
    });
    overflow_btn_->setVisible(false);
    root->addWidget(overflow_btn_);

    retranslateUi();

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    // Tab widths follow the font: re-fit once the change is fully applied.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { update_overflow(); });
    refresh_theme();
}

void TabBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslateUi();
        update_overflow();
    }
    QWidget::changeEvent(e);
}

void TabBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    update_overflow();
}

void TabBar::update_overflow() {
    if (!scroll_ || !overflow_btn_ || !scroll_->widget())
        return;
    // Not while the theme_changed handlers run. The bar's height follows the
    // font, so a font change resizes it right there (resizeEvent -> here), and
    // showing the overflow button then activates every layout up to the
    // window, which styles the whole application widget by widget: 1.1 s of a
    // 3.4 s freeze on a 14 -> 24 px change, measured, and every stylesheet the
    // later handlers set then re-styled its subtree on the spot.
    // appearance_settled() runs this once the change is in.
    if (ThemeManager::instance().notifying())
        return;
    // From the tabs' own size hints (font metrics), against the whole bar.
    const bool overflow = scroll_->widget()->sizeHint().width() > width();
    if (overflow_btn_->isHidden() == overflow)
        overflow_btn_->setVisible(overflow);
    if (overflow) {
        // Keep the active tab in view when the bar gets narrower.
        for (int i = 0; i < tab_buttons_.size() && i < tab_defs_.size(); ++i) {
            if (tab_defs_[i].id == active_id_) {
                scroll_->ensureWidgetVisible(tab_buttons_[i], 0, 0);
                break;
            }
        }
    }
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
    metrics::follow_fixed_height(btn, 32);
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
    // A tab activated from elsewhere (command bar, menu) is scrolled into view.
    if (scroll_ && overflow_btn_ && !overflow_btn_->isHidden()) {
        for (int i = 0; i < tab_buttons_.size() && i < tab_defs_.size(); ++i) {
            if (tab_defs_[i].id == tab_id) {
                scroll_->ensureWidgetVisible(tab_buttons_[i], 0, 0);
                break;
            }
        }
    }
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
                          "QPushButton[active=\"true\"]{background:%4;color:%7;border:1px solid %6;"
                          "padding:0 8px;font-weight:600;letter-spacing:0.5px;}"
                          "QToolButton{background:transparent;border:none;border-left:1px solid %2;"
                          "border-bottom:1px solid %2;padding:0 6px;}"
                          "QToolButton:hover{background:%5;}"
                          "QToolButton::menu-indicator{image:none;width:0;}"
                          "QMenu{background:%8;color:%3;border:1px solid %2;padding:4px 0;}"
                          "QMenu::item{padding:5px 24px 5px 12px;}"
                          "QMenu::item:selected{background:%5;}")
                      .arg(colors::BG_BASE())
                      .arg(colors::BORDER_MED())
                      .arg(colors::TEXT_PRIMARY())
                      .arg(colors::AMBER())
                      .arg(colors::BG_RAISED())
                      .arg(colors::AMBER_DIM())
                      .arg(colors::TEXT_ON_ACCENT()) // %7: text on the accent-filled active tab
                      .arg(colors::BG_SURFACE()));
    if (overflow_btn_) {
        const int glyph = QFontMetrics(fonts::font(0)).height() * 3 / 4;
        overflow_btn_->setIconSize(QSize(glyph, glyph));
    }
    update_styles();
    // Tab widths follow the font — but not from inside the theme_changed handler
    // (see update_overflow()): the appearance_settled() connection re-fits the
    // bar after a change. Outside one (construction) the fit is deferred a turn,
    // until the bar has its place in the window.
    if (!ThemeManager::instance().notifying())
        QTimer::singleShot(0, this, [this]() { update_overflow(); });
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
