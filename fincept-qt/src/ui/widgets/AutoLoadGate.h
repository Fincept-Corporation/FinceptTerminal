#pragma once
// AutoLoadGate — the app-wide cap on AUTOMATIC runs (default runs of views on
// first show / refresh): at most kDefaultCapacity run at once (PythonRunner has
// 3 slots; one stays free for what the user clicks), the rest wait in priority
// order, FIFO within a priority. A queued owner shows "Queued…" and leaves the
// queue when it is hidden. Runs the user starts never pass through the gate.
//
//   quint64 ticket = 0;
//   if (ui::AutoLoadGate::instance().enter(this, [this] { begin(); }, 0, &ticket))
//       begin();                       // a slot was free: start now
//   ...                                // else begin() is called when a slot frees
//   ui::AutoLoadGate::instance().leave(ticket);   // finished / failed / cancelled / hidden
//
// ui::AutoLoadPane uses it for every automatic run; other code rarely needs it.
// The queue logic is ui::autoload::AutoLoadQueue (pure, unit-tested). GUI thread.

#include "ui/widgets/AutoLoadPolicy.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <functional>

namespace fincept::ui {

class AutoLoadGate : public QObject {
    Q_OBJECT
  public:
    static constexpr int kDefaultCapacity = 2;

    static AutoLoadGate& instance();

    /// Ask for a slot. Returns true when the caller may start NOW (it then does
    /// not get `start_later`). Returns false when queued: `start_later` runs
    /// (queued, on the GUI thread) once a slot is free — skipped if `owner` is
    /// gone. `*ticket` identifies the request for leave() / position().
    bool enter(QObject* owner, std::function<void()> start_later, int priority, quint64* ticket);
    /// The run of `ticket` ended (or it leaves the queue). Starts the next waiting ones.
    void leave(quint64 ticket);

    /// 0-based place in the queue; -1 when not waiting.
    int position(quint64 ticket) const { return queue_.position(ticket); }
    int running_count() const { return queue_.running_count(); }
    int queued_count() const { return queue_.queued_count(); }
    void set_capacity(int capacity);

  signals:
    /// Something entered, left or started: queued owners refresh their "Queued…" text.
    void queue_changed();

  private:
    AutoLoadGate() = default;

    struct Entry {
        QPointer<QObject> owner;
        std::function<void()> start_later;
    };

    autoload::AutoLoadQueue queue_{kDefaultCapacity};
    QHash<quint64, Entry> entries_;
    quint64 next_ticket_ = 0;
};

} // namespace fincept::ui
