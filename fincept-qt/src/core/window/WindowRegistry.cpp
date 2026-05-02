#include "core/window/WindowRegistry.h"

#include "app/WindowFrame.h"
#include "core/logging/Logger.h"

#include <algorithm>

namespace fincept {

WindowRegistry& WindowRegistry::instance() {
    static WindowRegistry s;
    return s;
}

void WindowRegistry::register_frame(WindowFrame* w) {
    if (!w)
        return;
    for (const QPointer<WindowFrame>& p : windows_) {
        if (p == w)
            return; // idempotent
    }
    windows_.append(QPointer<WindowFrame>(w));
    LOG_DEBUG("WindowRegistry", QString("Frame registered: window_id=%1, total=%2")
                                    .arg(w->window_id()).arg(windows_.size()));
    emit frame_added(w);
    emit display_order_changed();
}

void WindowRegistry::unregister_frame(WindowFrame* w) {
    if (!w)
        return;
    bool removed = false;
    for (auto it = windows_.begin(); it != windows_.end();) {
        if (*it == w || it->isNull()) {
            if (*it == w) {
                emit frame_removing(w);
                removed = true;
            }
            it = windows_.erase(it);
        } else {
            ++it;
        }
    }
    if (removed) {
        LOG_DEBUG("WindowRegistry", QString("Frame unregistered: window_id=%1, total=%2")
                                        .arg(w->window_id()).arg(windows_.size()));
        emit display_order_changed();
    }
}

QList<WindowFrame*> WindowRegistry::frames() const {
    QList<WindowFrame*> out;
    out.reserve(windows_.size());
    for (const QPointer<WindowFrame>& p : windows_) {
        if (p)
            out.append(p.data());
    }
    std::sort(out.begin(), out.end(),
              [](WindowFrame* a, WindowFrame* b) { return a->window_id() < b->window_id(); });
    return out;
}

int WindowRegistry::frame_count() const {
    int n = 0;
    for (const QPointer<WindowFrame>& p : windows_) {
        if (p)
            ++n;
    }
    return n;
}

bool WindowRegistry::all_minimised() const {
    bool any = false;
    for (const QPointer<WindowFrame>& p : windows_) {
        if (!p)
            continue;
        any = true;
        if (!p->isMinimized())
            return false;
    }
    // Empty registry is *not* "all minimised" — there's nothing to minimise.
    return any;
}

QList<int> WindowRegistry::frame_ids() const {
    QList<int> ids;
    for (WindowFrame* w : frames())
        ids.append(w->window_id());
    return ids;
}

} // namespace fincept
