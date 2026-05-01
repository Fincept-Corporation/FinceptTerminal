#pragma once

#include <QObject>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <functional>

namespace fincept {

/// Phase 8 / decision 9.3: staged panel-widget construction.
///
/// On layout load with many panels (50+ panels in a Bloomberg-style
/// workspace), materialising every panel's screen widget synchronously
/// freezes the UI for several seconds. This class spreads construction
/// across UI ticks so the user sees the foreground tab of each visible
/// frame appear immediately, then the rest fill in over the next ~200ms
/// without blocking.
///
/// Usage:
///   ```
///   PanelMaterialiser::instance().enqueue("watchlist",
///       [](){ DockScreenRouter::navigate("watchlist"); }, /*priority=*/0);
///   ```
///
/// Priorities (lower number = higher priority — runs sooner):
///   0  — foreground tab on a visible frame (drained immediately)
///   1  — non-foreground tabs on visible frames
///   2  — anything on hidden / minimised frames
///
/// Each tick the materialiser drains up to `kMaxPerTick` items, picking
/// from the highest-priority bucket first.
///
/// Threading: UI-thread only (we're calling into Qt widgets directly).
class PanelMaterialiser : public QObject {
    Q_OBJECT
  public:
    static PanelMaterialiser& instance();

    /// Construction work to run later. The id is for logging/dedup;
    /// the lambda is the actual constructor invocation (typically a call
    /// into DockScreenRouter::navigate or materialize_screen).
    void enqueue(const QString& id, std::function<void()> work, int priority = 1);

    /// Drain everything synchronously. Used by tests + by tear-off paths
    /// that need a panel up *now*. Avoid on the UI thread during normal
    /// operation — the whole point of this class is to NOT block.
    void drain_all_now();

    /// Number of items still queued. Inspectable from the debug overlay.
    int pending_count() const;

    /// Interval between materialisation ticks. Default 16ms (~60fps frame
    /// budget). Tunable for tests.
    void set_tick_interval_ms(int ms);

    /// How many items to drain per tick. Default 4. Lower = smoother UI
    /// during a layout load, higher = layout finishes faster.
    void set_max_per_tick(int n);

    /// Default max-per-tick. 4 keeps layout-load under ~16ms per frame
    /// budget on the panel constructors that take 2-4ms each (the typical
    /// case once factories are warm).
    static constexpr int kDefaultMaxPerTick = 4;
    static constexpr int kDefaultTickIntervalMs = 16;

  signals:
    /// Fires after every drain — useful for the debug overlay's pending
    /// count and for tests that want to wait until the queue is empty.
    void queue_drained();

  private:
    PanelMaterialiser();

    void tick();

    struct Item {
        QString id;
        std::function<void()> work;
    };

    // Three priority buckets. Drained in 0 → 1 → 2 order each tick.
    QQueue<Item> q_priority_0_;
    QQueue<Item> q_priority_1_;
    QQueue<Item> q_priority_2_;

    QTimer tick_timer_;
    int max_per_tick_ = kDefaultMaxPerTick;
};

} // namespace fincept
