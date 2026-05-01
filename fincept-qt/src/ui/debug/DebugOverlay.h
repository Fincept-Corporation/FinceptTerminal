#pragma once

#include <QFrame>
#include <QLabel>
#include <QTimer>

namespace fincept::ui {

/// Phase 10 trim: minimal debug overlay shown on a WindowFrame.
///
/// Surfaces the data the user most often wants when debugging a slowdown:
///   - This frame's UUID + panel count + active-for-work flag
///   - PanelRegistry total panel count (across all frames)
///   - DataHub: total topic count + cumulative msgs/sec (last 1s)
///
/// The full Phase 10 overlay (per-panel render-time histograms, thread pool
/// stats, paint frame time) needs a richer instrumentation pipeline —
/// deferred. This trim ships the most-asked-for surface today.
///
/// Toggle via the `debug.overlay` action (Ctrl+Shift+D by default).
/// Per-frame instance: each WindowFrame owns its own overlay so stats are
/// scoped to the frame the user is looking at.
///
/// The widget paints with a semi-transparent dark background so it doesn't
/// occlude content underneath — usable while the user is actually working.
/// Ignores mouse input (`setAttribute(Qt::WA_TransparentForMouseEvents)`).
class DebugOverlay : public QFrame {
    Q_OBJECT
  public:
    explicit DebugOverlay(QWidget* parent = nullptr);

    /// Reposition to the bottom-right of the parent's content area.
    /// Call from the parent's resizeEvent.
    void reposition();

    /// Toggle visibility. The action handler calls this on every frame's
    /// overlay so all frames flip state in lockstep.
    void toggle_visible();

  private:
    void refresh();

    QLabel* label_ = nullptr;
    QTimer refresh_timer_;
    qint64 last_msgs_count_ = 0;
    qint64 last_msgs_sample_ms_ = 0;
};

} // namespace fincept::ui
