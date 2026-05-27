#include "ui/charts/OverlayLayer.h"

namespace fincept::ui {

OverlayLayer::OverlayLayer(QObject* parent) : QObject(parent) {}

OverlayLayer::~OverlayLayer() = default;

void OverlayLayer::set_visible(bool v) {
    if (visible_ != v) {
        visible_ = v;
        emit visibility_changed(v);
    }
}

} // namespace fincept::ui
