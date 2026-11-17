#pragma once
// EquitySingleInstance — the Equity Trading screen is built once per panel.
//
// Why: the shell builds a screen inside its dock widget's content host
// (DockScreenRouter::materialize_screen) and records it only AFTER the factory
// returned and screen->show() ran. Showing this screen for the first time creates
// its chart's Qt WebEngine view; the first WebEngine (texture) widget in a window
// makes Qt destroy and re-create the top-level window (QWidget::setParent, Qt 6.4+),
// which hides and shows every widget in it. The dock widget's QEvent::Show emits
// CDockWidget::visibilityChanged(true), the router's handler calls
// materialize_screen() again — it does not know the first screen yet — and the
// factory builds a SECOND screen into the same host's QVBoxLayout: two complete
// screens stacked top / bottom, each owning streams, subscriptions and a ticket.
//
// The screen's constructor therefore asks whether a live screen of its class was
// already built inside the same host. When one exists, the newcomer stays an empty,
// hidden "twin" that forwards the shell's calls (link group, state, external ticket)
// to the first one and never subscribes, streams or places anything.
//
// Header-only, QtWidgets only; unit-tested in tst_equity_single_instance.

#include <QObject>
#include <QVariant>
#include <QWidget>

namespace fincept::screens::equity {

/// Dynamic property set on an inert twin (so a third build never picks it as the original).
inline constexpr const char* kEqInertTwinProperty = "fincept_eq_inert_twin";

/// The first live widget inheriting `class_name` that is a DIRECT child of `host`,
/// other than `self` and other than an inert twin; nullptr when there is none (then
/// `self` is the panel's one real screen). `class_name` is the QMetaObject class name
/// ("fincept::screens::EquityTradingScreen").
inline QWidget* eq_first_live_twin(const QWidget* host, const QObject* self, const char* class_name) {
    if (!host || !class_name)
        return nullptr;
    for (QObject* child : host->children()) {
        if (child == self || !child->isWidgetType() || !child->inherits(class_name))
            continue;
        if (child->property(kEqInertTwinProperty).toBool())
            continue;
        return static_cast<QWidget*>(child);
    }
    return nullptr;
}

} // namespace fincept::screens::equity
