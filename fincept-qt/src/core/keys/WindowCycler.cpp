#include "core/keys/WindowCycler.h"

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

namespace fincept {

WindowCycler& WindowCycler::instance() {
    static WindowCycler s;
    return s;
}

void WindowCycler::register_window(WindowFrame* w) {
    WindowRegistry::instance().register_frame(w);
}

void WindowCycler::unregister_window(WindowFrame* w) {
    WindowRegistry::instance().unregister_frame(w);
}

WindowFrame* WindowCycler::current_focused() const {
    auto* aw = QApplication::activeWindow();
    if (auto* mw = qobject_cast<WindowFrame*>(aw))
        return mw;
    // Fallback: first sorted window. Useful when activeWindow() is a modal
    // dialog or the OS just handed focus to another app.
    const auto list = WindowRegistry::instance().frames();
    return list.isEmpty() ? nullptr : list.first();
}

void WindowCycler::focus_window_by_index(int n) {
    const auto list = WindowRegistry::instance().frames();
    if (n < 0 || n >= list.size())
        return;
    auto* w = list.at(n);
    w->showNormal();  // de-minimise if needed
    w->raise();
    w->activateWindow();
    w->setFocus(Qt::ShortcutFocusReason);
}

void WindowCycler::cycle_windows(bool forward) {
    const auto list = WindowRegistry::instance().frames();
    if (list.size() < 2)
        return;
    auto* current = current_focused();
    int idx = current ? list.indexOf(current) : -1;
    if (idx < 0)
        idx = 0;
    else
        idx = (idx + (forward ? 1 : -1) + list.size()) % list.size();
    auto* next = list.at(idx);
    next->showNormal();
    next->raise();
    next->activateWindow();
    next->setFocus(Qt::ShortcutFocusReason);
}

void WindowCycler::cycle_panels_in_current_window(bool forward) {
    auto* w = current_focused();
    if (!w)
        return;
    if (auto* router = w->dock_router())
        router->cycle_focused_panel(forward);
}

void WindowCycler::new_window_on_next_monitor() {
    auto* src = current_focused();
    auto* w = new WindowFrame(WindowFrame::next_window_id());
    w->setAttribute(Qt::WA_DeleteOnClose);

    // Decision 5.6: prefer the first monitor that has no frames at all,
    // since that's the case the user most often wants ("put it on my empty
    // 3rd monitor"). Fall back to "first monitor not containing focused
    // window's centre" (the pre-Phase 1 behaviour) when every monitor is
    // populated. Final fallback: don't reposition, let the constructor's
    // default placement run.
    const auto screens = QGuiApplication::screens();
    QScreen* target = nullptr;

    if (screens.size() > 1) {
        // Build a set of screens currently hosting at least one frame.
        const auto frames = WindowRegistry::instance().frames();
        QList<QScreen*> occupied;
        for (WindowFrame* mw : frames) {
            if (QScreen* s = mw->screen()) {
                if (!occupied.contains(s))
                    occupied.append(s);
            }
        }
        // Tier 1: empty monitor.
        for (QScreen* s : screens) {
            if (!occupied.contains(s)) {
                target = s;
                break;
            }
        }
        // Tier 2: monitor not containing source centre.
        if (!target && src) {
            const QPoint centre = src->geometry().center();
            for (QScreen* s : screens) {
                if (!s->geometry().contains(centre)) {
                    target = s;
                    break;
                }
            }
        }
    }

    if (target) {
        const QRect sg = target->availableGeometry();
        w->resize(sg.width() * 9 / 10, sg.height() * 9 / 10);
        w->move(sg.center() - w->rect().center());
    }

    w->show();
    w->raise();
    w->activateWindow();
}

void WindowCycler::move_current_window_to_monitor(int monitor_index) {
    const auto screens = QGuiApplication::screens();
    if (monitor_index < 0 || monitor_index >= screens.size()) {
        LOG_DEBUG("WindowCycler",
                  QString("Ignoring move-to-monitor %1 — only %2 screen(s) connected")
                      .arg(monitor_index).arg(screens.size()));
        return;
    }
    auto* w = current_focused();
    if (!w)
        return;
    w->move_to_screen(screens.at(monitor_index));
}

} // namespace fincept
