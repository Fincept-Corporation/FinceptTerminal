#pragma once

#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <functional>

namespace fincept {

/// Spreads panel-widget construction across UI ticks so layout loads don't freeze.
/// UI-thread only. Lower priority = runs sooner: 0=foreground/visible, 1=other tabs visible, 2=hidden frames.
class PanelMaterialiser : public QObject {
    Q_OBJECT
  public:
    static PanelMaterialiser& instance();

    /// Construction work to run later. The id is for logging/dedup;
    /// the lambda is the actual constructor invocation (typically a call
    /// into DockScreenRouter::navigate or materialize_screen).
    ///
    /// `owner` is an optional QObject whose lifetime the queued item is
    /// tied to. When `owner` is destroyed, queued items referencing it
    /// are silently dropped via `cancel_for(owner)`. Without an owner,
    /// items remain in the queue indefinitely (or until they fire).
    /// Pass `nullptr` for fire-and-forget semantics.
    void enqueue(const QString& id, std::function<void()> work, int priority = 1,
                 QObject* owner = nullptr);

    /// Drop every queued item whose `owner` matches. Called from
    /// WindowFrame's destructor so layouts that close mid-stage don't
    /// burn UI ticks draining no-op lambdas. O(n) over the buffered
    /// queue — buffered queues are typically &lt; 200 items so this is
    /// fast enough on the destruction path.
    void cancel_for(QObject* owner);

    /// Synchronous drain. Used by tests and tear-off paths; avoid during normal UI flow.
    void drain_all_now();

    int pending_count() const;

    void set_tick_interval_ms(int ms);

    void set_max_per_tick(int n);

    static constexpr int kDefaultMaxPerTick = 4;
    static constexpr int kDefaultTickIntervalMs = 16;

  signals:
    void queue_drained();

  private:
    PanelMaterialiser();

    void tick();

    struct Item {
        QString id;
        std::function<void()> work;
        // Optional owner. Item is dropped before invocation if the
        // QPointer becomes null (i.e. owner was destroyed). Items
        // enqueued without an owner have a default-constructed
        // QPointer that stays "valid" (it never had a target) — the
        // tick handler runs them unconditionally.
        QPointer<QObject> owner;
        bool has_owner = false;
    };

    QQueue<Item> q_priority_0_;
    QQueue<Item> q_priority_1_;
    QQueue<Item> q_priority_2_;

    QTimer tick_timer_;
    int max_per_tick_ = kDefaultMaxPerTick;
};

} // namespace fincept
