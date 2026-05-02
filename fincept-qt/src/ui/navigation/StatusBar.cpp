#include "ui/navigation/StatusBar.h"

#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "core/symbol/SymbolRef.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

namespace fincept::ui {

StatusBar::StatusBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
    setObjectName("appStatusBar");

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto mk = [](const QString& t, const QString& name) {
        auto* l = new QLabel(t);
        l->setObjectName(name);
        return l;
    };

    hl->addWidget(mk("v4.0.2", "sbVersion"));
    hl->addWidget(mk("  |  ", "sbSep"));
    const char* feeds[] = {"EQ", "FX", "CM", "FI", "CR"};
    for (auto& f : feeds) {
        hl->addWidget(mk(f, "sbFeed"));
        hl->addWidget(mk(" ", "sbSpacer"));
    }
    hl->addStretch();

    // Phase 7 polish: the active-symbol indicator. Sits left of READY so
    // it has natural prominence as the user's eye reaches the right side
    // of the status bar.
    link_label_ = mk(QString(), "sbLink");
    link_label_->setVisible(false); // hidden until first publish
    hl->addWidget(link_label_);
    hl->addWidget(mk("  |  ", "sbSep2"));

    ready_label_ = mk("READY", "sbReady");
    hl->addWidget(ready_label_);

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();

    wire_link_indicator();
}

void StatusBar::wire_link_indicator() {
    auto& ctx = fincept::SymbolContext::instance();
    // Update on every active-symbol publish. Phase 7 already broadcasts
    // these process-wide via the SymbolContext singleton.
    connect(&ctx, &fincept::SymbolContext::active_symbol_changed, this,
            [this](const fincept::SymbolRef& ref, QObject*) {
                // active_symbol doesn't carry a group; look it up by
                // walking the groups for the matching ref. Most common
                // case: the publisher just set group X to ref, so X
                // wins. Fall back to None if no group holds the ref.
                fincept::SymbolGroup matched = fincept::SymbolGroup::None;
                auto& sc = fincept::SymbolContext::instance();
                for (auto g : fincept::all_symbol_groups()) {
                    if (sc.has_group_symbol(g) && sc.group_symbol(g) == ref) {
                        matched = g;
                        break;
                    }
                }
                update_link_label(matched, ref);
            });
    connect(&ctx, &fincept::SymbolContext::group_symbol_changed, this,
            [this](fincept::SymbolGroup g, const fincept::SymbolRef& ref, QObject*) {
                // group_symbol_changed fires for unsetting too — ref invalid means
                // the group was cleared.
                if (!ref.is_valid())
                    return;
                update_link_label(g, ref);
            });

    // Seed with whatever's already in SymbolContext (might have been
    // restored from workspace before the status bar was constructed).
    if (ctx.active().is_valid()) {
        // Try to attribute to a group if there's one.
        fincept::SymbolGroup matched = fincept::SymbolGroup::None;
        for (auto g : fincept::all_symbol_groups()) {
            if (ctx.has_group_symbol(g) && ctx.group_symbol(g) == ctx.active()) {
                matched = g;
                break;
            }
        }
        update_link_label(matched, ctx.active());
    }
}

void StatusBar::update_link_label(fincept::SymbolGroup g, const fincept::SymbolRef& ref) {
    if (!ref.is_valid()) {
        link_label_->setVisible(false);
        return;
    }
    QString text;
    if (g == fincept::SymbolGroup::None) {
        text = ref.display();
    } else {
        text = QString("%1 · %2")
                   .arg(QChar(fincept::symbol_group_letter(g)))
                   .arg(ref.display());
    }
    link_label_->setText(text);
    link_label_->setVisible(true);

    // Tint the label with the group's colour so it visually echoes the
    // GroupBadge in the panel tab. Falls back to TEXT_DIM for unlinked.
    const QColor tint = (g == fincept::SymbolGroup::None)
                            ? QColor(colors::TEXT_DIM())
                            : SymbolGroupRegistry::instance().color(g);
    link_label_->setStyleSheet(QString("color:%1;background:transparent;font-weight:600;")
                                   .arg(tint.name()));
}

void StatusBar::refresh_theme() {
    setStyleSheet(QString("#appStatusBar { background:%1; border-top:1px solid %2; }"
                          "#sbVersion { color:%3; background:transparent; }"
                          "#sbSep { color:%2; background:transparent; }"
                          "#sbFeed { color:%3; background:transparent; }"
                          "#sbSpacer { background:transparent; }"
                          "#sbReady { color:%4; font-weight:700; background:transparent; }")
                      .arg(colors::BG_BASE())
                      .arg(colors::BORDER_DIM())
                      .arg(colors::TEXT_DIM())
                      .arg(colors::POSITIVE()));
}

void StatusBar::set_ready(bool ready) {
    ready_label_->setText(ready ? "READY" : "BUSY");
    ready_label_->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;")
                                    .arg(ready ? colors::POSITIVE() : colors::TEXT_TERTIARY()));
}

} // namespace fincept::ui
