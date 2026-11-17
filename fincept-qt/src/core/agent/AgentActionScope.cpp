// AgentActionScope.cpp — see AgentActionScope.h.

#include "core/agent/AgentActionScope.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QThread>

#include <algorithm>

namespace fincept::core::agent {

namespace agent_scope_detail {

constexpr int kMaxScopeDepth = 32;

struct ScopeEntry {
    quint64 id = 0;
    AgentActor actor;
    int loop_level = 0;   ///< the event-loop level it was opened at
    quint64 epoch = 0;    ///< the person-input epoch it was opened in
};

struct HoldEntry {
    quint64 id = 0;
    AgentActor actor;
    qint64 expires_ms = 0;
};

/// Drops every hold when a person presses a mouse button, a key or touches:
/// a spontaneous input event comes from the window system, never from the
/// engine (whose events are sent, not spontaneous).
class PersonInputWatcher : public QObject {
  public:
    using QObject::QObject;
    bool eventFilter(QObject* watched, QEvent* e) override;
};

struct State {
    QList<ScopeEntry> scopes; // innermost last
    QList<HoldEntry> holds;   // newest last
    quint64 next_id = 0;
    quint64 epoch = 0;        // bumped by a person's input
    QPointer<PersonInputWatcher> watcher;
    QElapsedTimer clock;
};

State& state() {
    static State s;
    if (!s.clock.isValid())
        s.clock.start();
    return s;
}

bool on_gui_thread() {
    const QCoreApplication* app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

int loop_level() {
    return QThread::currentThread()->loopLevel();
}

qint64 now_ms() {
    return state().clock.elapsed();
}

void purge_expired() {
    State& s = state();
    const qint64 now = now_ms();
    s.holds.erase(std::remove_if(s.holds.begin(), s.holds.end(),
                                 [now](const HoldEntry& h) { return h.expires_ms <= now; }),
                  s.holds.end());
}

void watch_person_input(bool on) {
    State& s = state();
    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return;
    if (on && !s.watcher) {
        s.watcher = new PersonInputWatcher(app);
        app->installEventFilter(s.watcher);
    } else if (!on && s.watcher && s.holds.isEmpty() && s.scopes.isEmpty()) {
        // Idle: no filter on every event. Removed now, deleted later — this can
        // run while the application is still delivering an event.
        app->removeEventFilter(s.watcher);
        s.watcher->deleteLater();
        s.watcher = nullptr;
    }
}

bool PersonInputWatcher::eventFilter(QObject* watched, QEvent* e) {
    if (e->spontaneous()) {
        switch (e->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
            case QEvent::KeyPress:
            case QEvent::TouchBegin:
            case QEvent::TabletPress: {
                State& s = state();
                ++s.epoch; // scopes opened before this no longer count
                s.holds.clear();
                break;
            }
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, e);
}

} // namespace agent_scope_detail

using namespace agent_scope_detail;

AgentActionScope::AgentActionScope(const AgentActor& actor) {
    Q_ASSERT_X(on_gui_thread(), "AgentActionScope", "GUI thread only");
    if (!on_gui_thread())
        return;
    State& s = state();
    if (s.scopes.size() >= kMaxScopeDepth)
        return;
    id_ = ++s.next_id;
    s.scopes.append({id_, actor, loop_level(), s.epoch});
    watch_person_input(true);
}

AgentActionScope::~AgentActionScope() {
    if (!id_ || !on_gui_thread())
        return;
    State& s = state();
    for (qsizetype i = s.scopes.size() - 1; i >= 0; --i) {
        if (s.scopes[i].id == id_) {
            s.scopes.removeAt(i);
            break;
        }
    }
    watch_person_input(false);
}

AgentActionHold::AgentActionHold(const AgentActor& actor, int max_ms) {
    Q_ASSERT_X(on_gui_thread(), "AgentActionHold", "GUI thread only");
    if (!on_gui_thread())
        return;
    State& s = state();
    purge_expired();
    while (s.holds.size() >= kMaxHolds)
        s.holds.removeFirst(); // the oldest goes: a hold is a short memo, never a store
    id_ = ++s.next_id;
    s.holds.append({id_, actor, now_ms() + std::clamp(max_ms, 1, kHardCapMs)});
    watch_person_input(true);
}

AgentActionHold::~AgentActionHold() {
    release();
}

void AgentActionHold::release() {
    if (!id_ || !on_gui_thread())
        return;
    State& s = state();
    for (qsizetype i = 0; i < s.holds.size(); ++i) {
        if (s.holds[i].id == id_) {
            s.holds.removeAt(i);
            break;
        }
    }
    id_ = 0;
    purge_expired();
    watch_person_input(false);
}

bool AgentActionHold::active() const {
    if (!id_ || !on_gui_thread())
        return false;
    purge_expired();
    for (const HoldEntry& h : state().holds) {
        if (h.id == id_)
            return true;
    }
    return false;
}

std::optional<AgentActor> current_agent_actor() {
    if (!on_gui_thread())
        return std::nullopt;
    State& s = state();
    const int level = loop_level();
    for (qsizetype i = s.scopes.size() - 1; i >= 0; --i) {
        const ScopeEntry& e = s.scopes[i];
        if (e.epoch == s.epoch && e.loop_level == level)
            return e.actor;
    }
    purge_expired();
    if (!s.holds.isEmpty())
        return s.holds.last().actor;
    return std::nullopt;
}

QString agent_order_source(const QString& client_id) {
    QString id;
    id.reserve(client_id.size());
    for (const QChar c : client_id.toLower()) {
        const bool keep = (c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
                          (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        if (keep)
            id += c;
        else if (!id.endsWith(QLatin1Char('_')))
            id += QLatin1Char('_'); // any other run of characters (incl. '_') is one '_'
    }
    id = id.left(40);
    while (id.startsWith(QLatin1Char('_')))
        id.remove(0, 1);
    while (id.endsWith(QLatin1Char('_')))
        id.chop(1);
    return QStringLiteral("agent:") + (id.isEmpty() ? QStringLiteral("unknown") : id);
}

QString current_agent_order_source() {
    const std::optional<AgentActor> actor = current_agent_actor();
    return actor ? agent_order_source(actor->client_id) : QString();
}

int active_agent_holds() {
    if (!on_gui_thread())
        return 0;
    purge_expired();
    return static_cast<int>(state().holds.size());
}

} // namespace fincept::core::agent
