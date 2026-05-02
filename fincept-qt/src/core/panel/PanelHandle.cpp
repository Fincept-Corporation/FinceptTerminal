#include "core/panel/PanelHandle.h"

#include <DockWidget.h>

namespace fincept {

PanelHandle::PanelHandle(PanelInstanceId instance_id,
                         QString type_id,
                         QString title,
                         WindowId frame_id,
                         QObject* parent)
    : QObject(parent),
      instance_id_(instance_id),
      type_id_(std::move(type_id)),
      title_(std::move(title)),
      frame_id_(frame_id) {}

void PanelHandle::set_title(const QString& t) {
    if (title_ == t) return;
    title_ = t;
    emit title_changed(t);
}

void PanelHandle::set_frame_id(WindowId id) {
    if (frame_id_ == id) return;
    frame_id_ = id;
    emit frame_id_changed(id);
}

void PanelHandle::set_link_group(const QString& g) {
    if (link_group_ == g) return;
    link_group_ = g;
    emit link_group_changed(g);
}

void PanelHandle::set_state(State s) {
    if (state_ == s) return;
    state_ = s;
    emit state_changed(s);
}

void PanelHandle::set_dock_widget(ads::CDockWidget* dw) {
    dock_widget_ = dw;
}

void PanelHandle::set_state_blob(const QByteArray& blob, int version) {
    last_state_blob_ = blob;
    last_state_version_ = version;
}

void PanelHandle::record_render_time_ms(int ms) {
    // Append; trim head when over limit. Vector is small (<= kRenderSampleLimit)
    // so the front-erase O(n) cost is dominated by the cache-friendly contiguous
    // layout. A circular buffer would be marginally faster but adds API surface
    // we don't need this phase.
    render_time_samples_ms_.append(ms);
    while (render_time_samples_ms_.size() > kRenderSampleLimit)
        render_time_samples_ms_.removeFirst();
}

} // namespace fincept
