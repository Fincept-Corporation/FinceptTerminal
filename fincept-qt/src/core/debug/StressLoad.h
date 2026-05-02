#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace fincept::debug_tools {

/// Phase 10: synthetic load harness for verifying the 16/200/1000 scale
/// claim from the design doc.
///
/// `debug.stress` action triggers `start(n_frames, m_panels_per_frame, k_ticks_per_sec)`.
/// The harness spawns N WindowFrames, opens M panels per frame (cycling
/// through registered factories), then pumps K synthetic publishes per
/// second into DataHub on `stress:tick:N` topics. Each frame's panels
/// subscribe to a slice of those topics so the fanout pipeline is
/// exercised end-to-end.
///
/// `stop()` halts the tick loop and closes the spawned frames. The harness
/// is reentrant — calling start() while already running stops first.
///
/// Threading: tick generation runs on a single QTimer on the UI thread.
/// At K=1000 this is ~1ms per fire — well under the budget on idle CPU.
/// If you want to stress the multi-thread paths, set K very high; the
/// hub queues to its own thread internally.
class StressLoad : public QObject {
    Q_OBJECT
  public:
    static StressLoad& instance();

    /// Start the harness. Returns true on successful start; false if
    /// arguments are invalid or out of range.
    /// n_frames: clamped to [1, 16]
    /// m_panels_per_frame: clamped to [1, 64]
    /// k_ticks_per_sec: clamped to [1, 10000]
    bool start(int n_frames, int m_panels_per_frame, int k_ticks_per_sec);

    /// Stop the harness. Closes the spawned frames and halts ticks.
    /// No-op if not running.
    void stop();

    bool is_running() const { return running_; }

    /// Stats for the debug overlay — total ticks fired so far.
    qint64 total_ticks() const { return total_ticks_; }

  private:
    StressLoad();

    void on_tick();
    void close_spawned_frames();

    bool running_ = false;
    int n_frames_ = 0;
    int m_panels_ = 0;
    int k_ticks_per_sec_ = 0;
    QTimer tick_timer_;
    qint64 total_ticks_ = 0;

    /// window_ids of frames spawned by this harness, so stop() can close
    /// only what we created — leaves any pre-existing user frames alone.
    QList<int> spawned_window_ids_;
};

} // namespace fincept::debug_tools
