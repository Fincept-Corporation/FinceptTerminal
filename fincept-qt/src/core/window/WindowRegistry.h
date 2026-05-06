#pragma once
#include <QList>
#include <QObject>
#include <QPointer>

namespace fincept {

class WindowFrame;

/// Process-global registry of live WindowFrames. UI-thread only; storage is QPointer (weak).
class WindowRegistry : public QObject {
    Q_OBJECT
  public:
    static WindowRegistry& instance();

    void register_frame(WindowFrame* w);
    void unregister_frame(WindowFrame* w);

    /// Sorted by window_id ascending — stable ordering for Ctrl+1..9.
    QList<WindowFrame*> frames() const;

    int frame_count() const;

    /// True only when every frame is minimised. Drives lock-on-minimise.
    bool all_minimised() const;

    QList<int> frame_ids() const;

  signals:
    void frame_added(WindowFrame* w);

    /// Pointer is still valid at emission time — subscribers can do final cleanup.
    void frame_removing(WindowFrame* w);

    void display_order_changed();

  private:
    WindowRegistry() = default;

    QList<QPointer<WindowFrame>> windows_;
};

} // namespace fincept
