#pragma once
#include <QList>
#include <QObject>
#include <QPointer>

namespace fincept {

class WindowFrame;

/// Process-global registry of live WindowFrame instances.
///
/// `WindowCycler` previously combined two responsibilities:
///   1. Storage of which windows exist (lifecycle).
///   2. Keyboard policy for moving focus / spawning windows.
///
/// `WindowRegistry` owns (1). `WindowCycler` keeps (2) but delegates to the
/// registry for the actual window list. This split lets the command bar,
/// link manager, telemetry, status bar, and the lock controller all observe
/// window add/remove without coupling to keyboard handling.
///
/// WindowFrame constructor calls `register_frame(this)`; destructor calls
/// `unregister_frame(this)`. Storage is weak (QPointer) so a destroyed
/// window is silently skipped until its explicit unregister tidies the slot.
///
/// Threading: UI-thread only.
class WindowRegistry : public QObject {
    Q_OBJECT
  public:
    static WindowRegistry& instance();

    void register_frame(WindowFrame* w);
    void unregister_frame(WindowFrame* w);

    /// All live windows sorted by `window_id` ascending. Stable ordering for
    /// keyboard shortcuts (`Ctrl+1` always = the lowest window_id).
    QList<WindowFrame*> frames() const;

    /// Count of live windows.
    int frame_count() const;

    /// True if every live window is currently in the minimised state. Used
    /// by the lock-on-minimise policy: per multi-window Bloomberg semantics,
    /// minimising one window is normal, but minimising the entire terminal
    /// (every frame) is a "walked away from desk" event that should trigger
    /// the inactivity lock.
    bool all_minimised() const;

    /// Frame ids in ascending order — used by SessionManager to persist
    /// "which windows were open at shutdown" without callers needing to
    /// touch each WindowFrame.
    QList<int> frame_ids() const;

  signals:
    /// Emitted after register_frame() inserts a new entry. Subscribers can
    /// hook into the new frame here (lock overlay injection in Phase 1b,
    /// link manager subscription in Phase 7).
    void frame_added(WindowFrame* w);

    /// Emitted before unregister_frame() removes the entry. The pointer is
    /// still valid at signal emission time so subscribers can do final
    /// cleanup (disconnect signals, snapshot state).
    void frame_removing(WindowFrame* w);

    /// Emitted whenever the result of `frames()` changes order or contents.
    /// Coarse signal that simplifies consumers who don't care about
    /// add-vs-remove specifics.
    void display_order_changed();

  private:
    WindowRegistry() = default;

    QList<QPointer<WindowFrame>> windows_;
};

} // namespace fincept
