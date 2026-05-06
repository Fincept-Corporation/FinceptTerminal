#include "screens/fno/FnoScreen.h"

#include "core/logging/Logger.h"
#include "screens/fno/ChainSubTab.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QString>
#include <QVBoxLayout>

namespace fincept::screens::fno {

using namespace fincept::ui;

namespace {

struct TabDef {
    const char* label;
    const char* description;
};

constexpr TabDef kTabDefs[FnoScreen::TabCount] = {
    {"Chain", "Live option chain (Phase 2)"},
    {"Builder", "Strategy builder + payoff (Phase 5)"},
    {"OI", "Open Interest analytics (Phase 7)"},
    {"Multi-Stra", "Multi straddle / strangle charts (Phase 9)"},
    {"FII / DII", "Institutional flows (Phase 8)"},
    {"Screener", "Chain screener (Phase 9)"},
};

}  // namespace

FnoScreen::FnoScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoScreen");
    setStyleSheet(QStringLiteral(
        "#fnoScreen { background:%1; }"
        "#fnoTabBar { background:%1; border-bottom:1px solid %2; }"
        "#fnoTabBtn { background:transparent; color:%3; border:none; padding:8px 18px; "
        "             font-size:10px; font-weight:700; letter-spacing:0.5px; }"
        "#fnoTabBtn:hover { background:%4; color:%5; }"
        "#fnoTabBtn[active=\"true\"] { color:%6; border-bottom:2px solid %6; }"
        "#fnoComingSoon { color:%3; font-size:14px; background:transparent; }"
        "#fnoComingSoonHint { color:%7; font-size:11px; background:transparent; }")
                      .arg(colors::BG_BASE(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(), colors::BG_HOVER(),
                           colors::TEXT_PRIMARY(), colors::AMBER(), colors::TEXT_DIM()));

    setup_ui();
    LOG_INFO("FnoScreen", "Constructed");
}

void FnoScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_tab_bar());

    stack_ = new QStackedWidget(this);
    // Pre-create all 6 slots with placeholders so QStackedWidget indices map
    // 1:1 to SubTab values. The Chain slot is replaced with the real widget
    // immediately so the user sees data on first reveal.
    for (int i = 0; i < TabCount; ++i) {
        auto* placeholder =
            build_placeholder(QString::fromLatin1(kTabDefs[i].label), QString::fromLatin1(kTabDefs[i].description));
        stack_->addWidget(placeholder);
        tabs_.insert(i, placeholder);
    }
    root->addWidget(stack_, 1);

    ensure_tab_built(TabChain);
    stack_->setCurrentIndex(TabChain);
    refresh_tab_button_styles();
}

QWidget* FnoScreen::build_tab_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("fnoTabBar");
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(2);

    tab_btns_.reserve(TabCount);
    for (int i = 0; i < TabCount; ++i) {
        auto* btn = new QPushButton(QString::fromLatin1(kTabDefs[i].label).toUpper(), bar);
        btn->setObjectName("fnoTabBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == int(active_tab_));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_tab_clicked(i); });
        lay->addWidget(btn);
        tab_btns_.append(btn);
    }
    lay->addStretch(1);
    return bar;
}

QWidget* FnoScreen::build_placeholder(const QString& tab_name, const QString& detail) {
    auto* wrap = new QWidget;
    auto* lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addStretch(1);
    auto* title = new QLabel(tab_name + " — coming in a later phase", wrap);
    title->setObjectName("fnoComingSoon");
    title->setAlignment(Qt::AlignCenter);
    auto* hint = new QLabel(detail, wrap);
    hint->setObjectName("fnoComingSoonHint");
    hint->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);
    lay->addWidget(hint);
    lay->addStretch(2);
    return wrap;
}

void FnoScreen::ensure_tab_built(SubTab which) {
    if (which == TabChain && !chain_tab_) {
        chain_tab_ = new ChainSubTab(this);
        // Replace the placeholder at slot 0 with the real chain widget.
        QWidget* old = stack_->widget(TabChain);
        stack_->removeWidget(old);
        stack_->insertWidget(TabChain, chain_tab_);
        tabs_.insert(TabChain, chain_tab_);
        if (old)
            old->deleteLater();
    }
    // Phase 2 only: Chain tab. Other tabs stay as placeholders until their
    // phase lands.
}

void FnoScreen::on_tab_clicked(int index) {
    if (index < 0 || index >= TabCount)
        return;
    active_tab_ = SubTab(index);
    ensure_tab_built(active_tab_);
    stack_->setCurrentIndex(index);
    refresh_tab_button_styles();
}

void FnoScreen::refresh_tab_button_styles() {
    for (int i = 0; i < tab_btns_.size(); ++i) {
        QPushButton* btn = tab_btns_.at(i);
        btn->setProperty("active", i == int(active_tab_));
        // Re-evaluate stylesheet when a dynamic property changes.
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

QVariantMap FnoScreen::save_state() const {
    QVariantMap m;
    m["active_tab"] = int(active_tab_);
    if (chain_tab_)
        m["chain"] = chain_tab_->save_state();
    return m;
}

void FnoScreen::restore_state(const QVariantMap& state) {
    if (state.contains("active_tab")) {
        const int idx = state.value("active_tab").toInt();
        if (idx >= 0 && idx < TabCount) {
            active_tab_ = SubTab(idx);
            ensure_tab_built(active_tab_);
            if (stack_)
                stack_->setCurrentIndex(idx);
            refresh_tab_button_styles();
        }
    }
    if (state.contains("chain")) {
        ensure_tab_built(TabChain);
        if (chain_tab_)
            chain_tab_->restore_state(state.value("chain").toMap());
    }
}

void FnoScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    // F&O symbol groups: a Yellow-group ticker change should switch the
    // chain underlying when the symbol matches a known F&O underlying.
    // Phase 2 keeps this minimal — just remember the symbol; the picker
    // will pick it up next time the user opens the screen.
    Q_UNUSED(ref);
}

fincept::SymbolRef FnoScreen::current_symbol() const {
    fincept::SymbolRef ref;
    if (chain_tab_) {
        // Surface the underlying so other group-linked panels can follow.
        // Empty when chain has no selection yet.
    }
    return ref;
}

void FnoScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
}

void FnoScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

} // namespace fincept::screens::fno
