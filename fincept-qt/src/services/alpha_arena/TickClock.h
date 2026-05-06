#pragma once
// TickClock — single-thread monotonic scheduler for one competition.
//
// Owned by AlphaArenaEngine. There is exactly one TickClock per active
// competition; v1 supports only a single competition at a time.
//
// Cadence is wall-clock fixed interval (Nof1 S1 was 2–3 minutes; we default
// to 180s, configurable per competition). Drift handling: if the previous
// tick is still in flight when the timer fires, we *skip* the new tick and
// emit `tick_skipped(seq)`. We do not queue make-ups — that's the Nof1
// behaviour and the only sane choice.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §3 (Tick clock).

#include "services/alpha_arena/AlphaArenaTypes.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace fincept::services::alpha_arena {

class TickClock : public QObject {
    Q_OBJECT
  public:
    explicit TickClock(QObject* parent = nullptr);

    /// Set cadence between ticks. Effective on next start() or natural fire.
    void set_cadence_seconds(int seconds);
    int cadence_seconds() const { return cadence_seconds_; }

    /// Mark the previous tick as complete. The clock will continue firing on
    /// schedule; if a tick is fired while another is in_flight, that tick is
    /// skipped (logged + signalled).
    void tick_complete();

    /// Returns true between `tick(…)` emission and `tick_complete()`.
    bool in_flight() const { return in_flight_; }

    /// Manual fire. Useful for the FORCE TICK button. If a tick is already
    /// in flight this is a no-op (and emits `tick_skipped`).
    void force_tick();

    /// Last completed tick seq, or 0 if none yet.
    int last_seq() const { return last_seq_; }

  public slots:
    void start();
    void stop();

  signals:
    /// Emitted when a tick fires. Listener must call tick_complete() when
    /// done — the clock will not fire again while in_flight is true (it
    /// will instead emit tick_skipped for any timer fire that lands in that
    /// window).
    void tick(fincept::services::alpha_arena::Tick t);

    /// Emitted when a scheduled tick is skipped because the previous one is
    /// still in flight.
    void tick_skipped(int would_be_seq);

  private slots:
    void on_timeout();

  private:
    QTimer* timer_;
    int cadence_seconds_ = 180;
    int next_seq_ = 1;
    int last_seq_ = 0;
    bool in_flight_ = false;
};

} // namespace fincept::services::alpha_arena
