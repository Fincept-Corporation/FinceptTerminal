// UiHostTerminal.cpp — UiHost.h for the terminal: windows are WindowFrames,
// panels are the dock manager's dock widgets, screens come from each window's
// DockScreenRouter, the lock from InactivityGuard, running scripts from
// PythonRunner, symbol boxes are ui::SymbolSearchBox. Terminal only (the unit
// tests implement UiHost.h over plain widgets).

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "core/layout/WorkspaceShell.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolRef.h"
#include "core/window/WindowRegistry.h"
#include "mcp/core/UiHost.h"
#include "python/PythonRunner.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

#include <DockManager.h>
#include <DockWidget.h>
#include <memory>

namespace fincept::mcp::core::ui::host {

namespace ui_host_detail {

WindowFrame* as_frame(QWidget* w) {
    return qobject_cast<WindowFrame*>(w);
}

ads::CDockWidget* as_panel(QWidget* w) {
    return qobject_cast<ads::CDockWidget*>(w);
}

} // namespace ui_host_detail

using namespace ui_host_detail;

QList<QWidget*> frames() {
    QList<QWidget*> out;
    for (WindowFrame* f : WindowRegistry::instance().frames()) {
        if (f)
            out << f;
    }
    return out;
}

bool is_frame(const QObject* o) {
    return qobject_cast<const WindowFrame*>(o) != nullptr;
}

QWidget* panel_area(QWidget* frame) {
    WindowFrame* f = as_frame(frame);
    return f ? f->dock_manager() : nullptr;
}

int window_id(QWidget* frame) {
    WindowFrame* f = as_frame(frame);
    return f ? f->window_id() : -1;
}

bool frame_locked(QWidget* frame) {
    WindowFrame* f = as_frame(frame);
    return f && f->is_locked();
}

QList<QWidget*> panels(QWidget* frame) {
    QList<QWidget*> out;
    WindowFrame* f = as_frame(frame);
    if (!f || !f->dock_manager())
        return out;
    for (ads::CDockWidget* d : f->dock_manager()->openedDockWidgets())
        out << d;
    return out;
}

bool is_panel(const QObject* o) {
    return qobject_cast<const ads::CDockWidget*>(o) != nullptr;
}

bool panel_closed(QWidget* panel) {
    ads::CDockWidget* d = as_panel(panel);
    return !d || d->isClosed();
}

QString panel_id(QWidget* panel) {
    return panel ? panel->objectName() : QString();
}

QString panel_title(QWidget* panel) {
    if (!panel)
        return {};
    const QString t = panel->windowTitle().trimmed();
    return t.isEmpty() ? DockScreenRouter::title_for_id(panel->objectName().section(QLatin1Char('#'), 0, 0)) : t;
}

QWidget* panel_content(QWidget* panel) {
    ads::CDockWidget* d = as_panel(panel);
    return d ? d->widget() : nullptr;
}

QWidget* panel_screen(QWidget* panel) {
    ads::CDockWidget* d = as_panel(panel);
    if (!d)
        return nullptr;
    for (QWidget* p = d; p; p = p->parentWidget()) {
        if (WindowFrame* f = as_frame(p)) {
            if (DockScreenRouter* router = f->dock_router()) {
                if (QWidget* screen = router->screen_widget(d->objectName()))
                    return screen;
            }
            break;
        }
    }
    return d->widget();
}

QWidget* current_panel(QWidget* frame) {
    WindowFrame* f = as_frame(frame);
    if (!f || !f->dock_router())
        return nullptr;
    return f->dock_router()->find_dock_widget(f->dock_router()->current_screen_id());
}

void focus_panel(QWidget* panel) {
    ads::CDockWidget* d = as_panel(panel);
    if (!d)
        return;
    d->raise();
    d->setAsCurrentTab();
    if (QWidget* c = d->widget())
        c->setFocus(Qt::OtherFocusReason);
}

void close_panel(QWidget* panel) {
    if (ads::CDockWidget* d = as_panel(panel))
        d->toggleView(false);
}

QString title_for_id(const QString& screen_id) {
    return DockScreenRouter::title_for_id(screen_id);
}

SymbolInfo panel_symbol(QWidget* panel) {
    SymbolInfo out;
    if (auto* linked = dynamic_cast<IGroupLinked*>(panel_screen(panel))) {
        const SymbolRef ref = linked->current_symbol();
        out.symbol = ref.symbol;
        out.asset_class = ref.asset_class;
        out.exchange = ref.exchange;
    }
    return out;
}

TerminalStatus terminal_status() {
    TerminalStatus st;
    st.version = QCoreApplication::applicationVersion();
    auto& auth = auth::AuthManager::instance();
    st.signed_in = auth.is_authenticated();
    if (st.signed_in)
        st.plan = auth.session().plan_label();
    st.layout = layout::WorkspaceShell::current_name();
    st.locked = auth::InactivityGuard::instance().is_terminal_locked();
    return st;
}

bool terminal_locked() {
    return auth::InactivityGuard::instance().is_terminal_locked();
}

int python_pending() {
    return python::PythonRunner::instance().pending_count();
}

bool watch_symbol_box(QWidget* field, int timeout_ms, std::function<void(const QString& outcome)> done) {
    fincept::ui::SymbolSearchBox* box = nullptr;
    for (QWidget* p = field; p && !p->isWindow(); p = p->parentWidget()) {
        if ((box = qobject_cast<fincept::ui::SymbolSearchBox*>(p)))
            break;
    }
    if (!box || !done)
        return false;
    // One answer: the first of picked / unresolved, or nothing by the deadline.
    struct Watch {
        QMetaObject::Connection picked, many, failed;
        bool fired = false;
        std::function<void(const QString&)> done;
        void finish(const QString& outcome) {
            if (fired)
                return;
            fired = true;
            QObject::disconnect(picked);
            QObject::disconnect(many);
            QObject::disconnect(failed);
            if (outcome.isEmpty())
                return;
            auto cb = std::move(done);
            if (cb)
                cb(outcome);
        }
    };
    auto watch = std::make_shared<Watch>();
    watch->done = std::move(done);
    auto describe = [](const ::fincept::core::instrument::Instrument& inst) {
        QStringList parts{inst.display.isEmpty() ? inst.symbol : inst.display};
        if (!inst.name.isEmpty())
            parts << inst.name;
        if (!inst.venue.isEmpty())
            parts << inst.venue;
        return parts.join(QStringLiteral(" · "));
    };
    watch->picked = QObject::connect(box, &fincept::ui::SymbolSearchBox::instrument_picked, box,
                                     [watch, describe](const ::fincept::core::instrument::Instrument& inst) {
                                         watch->finish(describe(inst));
                                     });
    watch->many = QObject::connect(box, &fincept::ui::SymbolSearchBox::instruments_picked, box,
                                   [watch, describe](const QVector<::fincept::core::instrument::Instrument>& list) {
                                       QStringList names;
                                       for (const auto& inst : list)
                                           names << describe(inst);
                                       watch->finish(names.isEmpty() ? QStringLiteral("unresolved: nothing matched")
                                                                     : names.join(QStringLiteral("; ")));
                                   });
    watch->failed = QObject::connect(box, &fincept::ui::SymbolSearchBox::unresolved, box,
                                     [watch](const QStringList& texts, const QString& message) {
                                         watch->finish(QStringLiteral("unresolved: %1%2")
                                                           .arg(message.isEmpty() ? QStringLiteral("no match")
                                                                                  : message,
                                                                texts.isEmpty() ? QString()
                                                                                : QStringLiteral(" (%1)").arg(
                                                                                      texts.join(QStringLiteral(", ")))));
                                     });
    QTimer::singleShot(timeout_ms, box, [watch]() { watch->finish(QString()); });
    return true;
}

} // namespace fincept::mcp::core::ui::host
