#pragma once
#include "core/identity/Uuid.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

namespace ads {
class CDockWidget;
}

namespace fincept {

class WindowFrame;

/// Shell-side representation of a single panel (a screen instance docked
/// into some `WindowFrame`).
///
/// The handle exists whether or not the panel's widget is actually
/// materialised on screen. That distinction matters at scale: a layout with
/// 200 panels keeps 200 handles in `PanelRegistry` cheaply, but only the
/// visible ones own live `CDockWidget` + screen `QWidget` instances. When
/// a panel becomes visible (`State::Active`) the handle gains a non-null
/// `dock_widget()`; when it's dematerialised it returns to `State::Suspended`
/// and the dock pointer goes back to null.
///
/// State machine (decisions 1.8 + 2.7):
///
///   Spawning    — handle just registered; no widget yet.
///   Materialising — widget under construction (factory running).
///   Active      — widget alive, dock widget added to a frame's manager.
///   Suspended   — widget dematerialised, state_blob preserved.
///   Closing     — close request in flight; flushing pending state.
///   Closed      — terminal state; handle about to be unregistered.
///
/// Phase 4 ships the handle and the registry. Phase 5 (tear-off) and
/// Phase 7 (linking) consume them. Phase 8 wires the dematerialisation
/// budget that drives Suspended ↔ Active transitions.
class PanelHandle : public QObject {
    Q_OBJECT
  public:
    enum class State {
        Spawning,
        Materialising,
        Active,
        Suspended,
        Closing,
        Closed,
    };
    Q_ENUM(State)

    PanelHandle(PanelInstanceId instance_id,
                QString type_id,
                QString title,
                WindowId frame_id,
                QObject* parent = nullptr);

    PanelInstanceId instance_id() const { return instance_id_; }
    const QString& type_id() const { return type_id_; }
    const QString& title() const { return title_; }
    WindowId frame_id() const { return frame_id_; }
    State state() const { return state_; }

    /// Optional user-visible link group ("red", "green", ... or "" = unlinked).
    /// Phase 7 (LinkManager) will mutate this; Phase 4 just stores it.
    const QString& link_group() const { return link_group_; }

    /// Last serialised IStatefulScreen state blob. Preserved across the
    /// Suspended ↔ Active transition so re-materialisation restores filters,
    /// scroll, sub-panel state without round-tripping the cache db.
    const QByteArray& last_state_blob() const { return last_state_blob_; }
    int last_state_version() const { return last_state_version_; }

    /// Live dock widget pointer, or nullptr while Suspended/Closed.
    ads::CDockWidget* dock_widget() const { return dock_widget_.data(); }

    /// Render-time samples (most-recent first). Phase 8 surfaces these in
    /// the debug overlay; Phase 4 stores them so the pipeline is in place.
    const QVector<int>& render_time_samples_ms() const { return render_time_samples_ms_; }

    // ── Setters (called by PanelRegistry / the router) ─────────────────────

    void set_title(const QString& t);
    void set_frame_id(WindowId id);
    void set_link_group(const QString& g);
    void set_state(State s);
    void set_dock_widget(ads::CDockWidget* dw);
    void set_state_blob(const QByteArray& blob, int version);

    /// Push a render-time sample. Caps the buffer at `kRenderSampleLimit`.
    void record_render_time_ms(int ms);
    static constexpr int kRenderSampleLimit = 64;

  signals:
    void title_changed(const QString& title);
    void frame_id_changed(WindowId id);
    void link_group_changed(const QString& group);
    void state_changed(PanelHandle::State new_state);

  private:
    PanelInstanceId instance_id_;
    QString type_id_;
    QString title_;
    WindowId frame_id_;
    QString link_group_;

    State state_ = State::Spawning;
    QPointer<ads::CDockWidget> dock_widget_;

    QByteArray last_state_blob_;
    int last_state_version_ = 0;

    QVector<int> render_time_samples_ms_;
};

} // namespace fincept
