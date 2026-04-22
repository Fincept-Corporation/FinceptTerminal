#include "core/keys/WindowCycler.h"

#include "app/DockScreenRouter.h"
#include "app/MainWindow.h"
#include "core/logging/Logger.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

namespace fincept {

WindowCycler& WindowCycler::instance() {
    static WindowCycler s;
    return s;
}

void WindowCycler::register_window(MainWindow* w) {
    if (!w)
        return;
    // Skip duplicates — idempotent register keeps callers simple.
    for (const QPointer<MainWindow>& p : windows_) {
        if (p == w)
            return;
    }
    windows_.append(QPointer<MainWindow>(w));
}

void WindowCycler::unregister_window(MainWindow* w) {
    if (!w)
        return;
    windows_.erase(std::remove_if(windows_.begin(), windows_.end(),
                                  [w](const QPointer<MainWindow>& p) { return p == w || p.isNull(); }),
                   windows_.end());
}

QList<MainWindow*> WindowCycler::windows() const {
    QList<MainWindow*> out;
    out.reserve(windows_.size());
    for (const QPointer<MainWindow>& p : windows_) {
        if (p)
            out.append(p.data());
    }
    std::sort(out.begin(), out.end(),
              [](MainWindow* a, MainWindow* b) { return a->window_id() < b->window_id(); });
    return out;
}

MainWindow* WindowCycler::current_focused() const {
    auto* aw = QApplication::activeWindow();
    if (auto* mw = qobject_cast<MainWindow*>(aw))
        return mw;
    // Fallback: first sorted window. Useful when activeWindow() is a modal
    // dialog or the OS just handed focus to another app.
    const auto list = windows();
    return list.isEmpty() ? nullptr : list.first();
}

void WindowCycler::focus_window_by_index(int n) {
    const auto list = windows();
    if (n < 0 || n >= list.size())
        return;
    auto* w = list.at(n);
    w->showNormal();  // de-minimise if needed
    w->raise();
    w->activateWindow();
    w->setFocus(Qt::ShortcutFocusReason);
}

void WindowCycler::cycle_windows(bool forward) {
    const auto list = windows();
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
    auto* w = new MainWindow(MainWindow::next_window_id());
    w->setAttribute(Qt::WA_DeleteOnClose);

    // Smart placement: find a screen that doesn't contain the source
    // window's centre. Matches the "new window" action in MainWindow's
    // toolbar handler — kept in sync intentionally.
    const auto screens = QGuiApplication::screens();
    if (src && screens.size() > 1) {
        const QPoint src_centre = src->geometry().center();
        for (QScreen* s : screens) {
            if (!s->geometry().contains(src_centre)) {
                const QRect sg = s->availableGeometry();
                w->resize(sg.width() * 9 / 10, sg.height() * 9 / 10);
                w->move(sg.center() - w->rect().center());
                break;
            }
        }
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
