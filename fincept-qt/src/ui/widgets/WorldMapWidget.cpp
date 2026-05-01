// src/ui/widgets/WorldMapWidget.cpp
#include "ui/widgets/WorldMapWidget.h"

#include "core/config/AppPaths.h"

#include <QGeoView/QGVCamera.h>
#include <QGeoView/QGVGlobal.h>
#include <QGeoView/QGVLayer.h>
#include <QGeoView/QGVLayerOSM.h>
#include <QGeoView/QGVMap.h>
#include <QGeoView/QGVWidgetScale.h>
#include <QGeoView/QGVWidgetZoom.h>
#include <QGeoView/Raster/QGVIcon.h>
#include <QGraphicsView>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::ui {

// ── Clickable pin marker ────────────────────────────────────────────────────
//
// QGVIcon itself is not interactive; we subclass it to receive map click
// events via `projOnMouseClick` and bounce them back through a stored
// callback. Pins that don't carry an id (the default -1) skip the wiring
// entirely so non-interactive maps stay free of click overhead.
namespace {
class ClickableIcon : public QGVIcon {
  public:
    explicit ClickableIcon(int id, QString tooltip, std::function<void(int)> on_click)
        : id_(id), tooltip_(std::move(tooltip)), on_click_(std::move(on_click)) {}

  protected:
    void projOnMouseClick(const QPointF& projPos) override {
        Q_UNUSED(projPos);
        if (on_click_)
            on_click_(id_);
    }
    QString projTooltip(const QPointF& projPos) const override {
        Q_UNUSED(projPos);
        return tooltip_;
    }

  private:
    int id_;
    QString tooltip_;
    std::function<void(int)> on_click_;
};
}  // namespace

// ── One-time network manager setup for QGeoView tile loading ────────────────
static void ensure_network_manager() {
    if (QGV::getNetworkManager())
        return;

    auto* nam = new QNetworkAccessManager(qApp);
    auto* cache = new QNetworkDiskCache(nam);
    QString cache_dir = fincept::AppPaths::cache() + "/map_tiles";
    cache->setCacheDirectory(cache_dir);
    cache->setMaximumCacheSize(100 * 1024 * 1024); // 100 MB
    nam->setCache(cache);
    QGV::setNetworkManager(nam);
}

// ── Constructor ─────────────────────────────────────────────────────────────

WorldMapWidget::WorldMapWidget(QWidget* parent) : QWidget(parent) {
    ensure_network_manager();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    map_ = new QGVMap(this);

    // Dark background for areas outside tile coverage
    if (auto* view = map_->findChild<QGraphicsView*>()) {
        view->setBackgroundBrush(QColor(0x08, 0x08, 0x08));
    }

    // Use CartoDB Dark Matter tiles
    auto* tiles = new QGVLayerOSM(QStringLiteral("https://basemaps.cartocdn.com/dark_all/${z}/${x}/${y}.png"));
    map_->addItem(tiles);

    // Zoom controls
    map_->addWidget(new QGVWidgetZoom());
    map_->addWidget(new QGVWidgetScale());

    // Enable mouse interactions
    map_->setMouseActions(QGV::MouseAction::All);

    // Marker layer
    marker_layer_ = new QGVLayer();
    map_->addItem(marker_layer_);

    layout->addWidget(map_);
    map_ready_ = true;
}

// ── Marker image generation ─────────────────────────────────────────────────

QImage WorldMapWidget::make_marker_image(const QColor& color, double radius) const {
    int size = static_cast<int>(radius * 4);
    if (size < 8)
        size = 8;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    double cx = size / 2.0;
    double cy = size / 2.0;
    double r = size / 2.0 - 1;

    // Outer glow
    QRadialGradient glow(cx, cy, r * 1.5);
    glow.setColorAt(0, QColor(color.red(), color.green(), color.blue(), 80));
    glow.setColorAt(1, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(QPointF(cx, cy), r * 1.4, r * 1.4);

    // Solid dot
    p.setBrush(color);
    p.setPen(QPen(color.lighter(150), 0.8));
    p.drawEllipse(QPointF(cx, cy), r * 0.6, r * 0.6);

    return img;
}

// ── Pin management ──────────────────────────────────────────────────────────

void WorldMapWidget::rebuild_markers() {
    if (!map_ready_ || !marker_layer_)
        return;

    // Remove all existing markers from the layer
    while (marker_layer_->countItems() > 0) {
        auto* item = marker_layer_->getItem(0);
        marker_layer_->removeItem(item);
        delete item;
    }

    // Add new markers
    for (const auto& pin : pins_) {
        QGVIcon* icon = nullptr;
        QGV::ItemFlags flags = QGV::ItemFlag::IgnoreScale | QGV::ItemFlag::IgnoreAzimuth;
        if (pin.id >= 0) {
            icon = new ClickableIcon(pin.id, pin.label, [this](int id) { emit pin_clicked(id); });
            flags |= QGV::ItemFlag::Clickable;
        } else {
            icon = new QGVIcon();
        }
        icon->setGeometry(QGV::GeoPos(pin.latitude, pin.longitude), QSizeF(20, 20));
        icon->loadImage(make_marker_image(pin.color, pin.radius));
        icon->setFlags(flags);
        marker_layer_->addItem(icon);
    }
}

void WorldMapWidget::set_pins(const QVector<MapPin>& pins) {
    pins_ = pins;
    rebuild_markers();
}

void WorldMapWidget::add_pin(const MapPin& pin) {
    pins_.append(pin);
    rebuild_markers();
}

void WorldMapWidget::clear_pins() {
    pins_.clear();
    rebuild_markers();
}

void WorldMapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Re-fit on resize so the camera aspect tracks the widget. Debounce
    // through a single-shot timer so live drag-resizing doesn't spam flyTo
    // animations — one re-fit shortly after the user lets go is enough.
    if (!map_ready_ || pins_.isEmpty())
        return;
    QTimer::singleShot(150, this, [this]() {
        if (!pins_.isEmpty())
            fit_to_pins();
    });
}

void WorldMapWidget::fly_to(double lat, double lng, int zoom) {
    Q_UNUSED(zoom);
    if (!map_ready_)
        return;
    double d = 5.0;
    auto target = QGV::GeoRect(QGV::GeoPos(lat + d, lng - d * 1.5), QGV::GeoPos(lat - d, lng + d * 1.5));
    map_->flyTo(QGVCameraActions(map_).scaleTo(target));
}

void WorldMapWidget::fit_to_pins() {
    if (!map_ready_ || pins_.isEmpty())
        return;

    double min_lat = 90, max_lat = -90, min_lng = 180, max_lng = -180;
    for (const auto& pin : pins_) {
        if (pin.latitude < min_lat)  min_lat = pin.latitude;
        if (pin.latitude > max_lat)  max_lat = pin.latitude;
        if (pin.longitude < min_lng) min_lng = pin.longitude;
        if (pin.longitude > max_lng) max_lng = pin.longitude;
    }

    // World-view fallback rectangle. Avoids the polar tile-coverage gap and
    // matches the standard Web-Mercator content extent we want filling the
    // viewport when pins are too sparse / globally scattered to fit usefully.
    constexpr double kWorldMinLat = -60.0, kWorldMaxLat = 75.0;
    constexpr double kWorldMinLng = -175.0, kWorldMaxLng = 175.0;

    double lat_span = std::max(max_lat - min_lat, 0.0);
    double lng_span = std::max(max_lng - min_lng, 0.0);

    // Globally-scattered pins → snap to world view; tighter zoom would just
    // wrap pins around the dateline gutter and leave big dark gaps.
    if (lng_span > 200.0 || lat_span > 110.0) {
        auto world = QGV::GeoRect(QGV::GeoPos(kWorldMaxLat, kWorldMinLng),
                                  QGV::GeoPos(kWorldMinLat, kWorldMaxLng));
        map_->flyTo(QGVCameraActions(map_).scaleTo(world));
        return;
    }

    // Pad proportionally to the span — 10% per side. Floor of 4° / 6° keeps a
    // single pin or tightly-clustered pins from zooming in absurdly far while
    // staying visually tight enough to avoid empty black space.
    double lat_pad = std::max(lat_span * 0.1, 4.0);
    double lng_pad = std::max(lng_span * 0.1, 6.0);

    double box_min_lat = min_lat - lat_pad;
    double box_max_lat = max_lat + lat_pad;
    double box_min_lng = min_lng - lng_pad;
    double box_max_lng = max_lng + lng_pad;

    // Match the widget's aspect ratio so QGeoView doesn't letterbox the
    // viewport with dark gutters on the long axis. Mercator distorts vertical
    // distance by ~1/cos(lat); use the centre latitude as a first-order
    // correction so the inflation feels right at high latitudes.
    double w = std::max(1, width());
    double h = std::max(1, height());
    double widget_aspect = w / h;
    double centre_lat = (box_min_lat + box_max_lat) * 0.5;
    double mercator_k = std::cos(centre_lat * 3.14159265358979 / 180.0);
    if (mercator_k < 0.2) mercator_k = 0.2;

    double box_lat_h = box_max_lat - box_min_lat;
    double box_lng_w = box_max_lng - box_min_lng;
    // Effective screen-aspect of the box (lng_w * cos(lat)) / lat_h.
    double box_aspect = (box_lng_w * mercator_k) / std::max(box_lat_h, 1e-6);

    if (box_aspect < widget_aspect) {
        // Box is too tall → widen it.
        double need_lng_w = (widget_aspect * box_lat_h) / mercator_k;
        double extra = (need_lng_w - box_lng_w) * 0.5;
        box_min_lng -= extra;
        box_max_lng += extra;
    } else {
        // Box is too wide → grow it vertically.
        double need_lat_h = (box_lng_w * mercator_k) / widget_aspect;
        double extra = (need_lat_h - box_lat_h) * 0.5;
        box_min_lat -= extra;
        box_max_lat += extra;
    }

    // Clamp to sensible viewable bounds. Beyond ±70° lat the OSM dark tiles
    // are mostly empty ocean / ice and produce visible black gutters at the
    // bottom of the map; above ±175° lng we cross the antimeridian gap.
    box_min_lat = std::max(box_min_lat, kWorldMinLat);
    box_max_lat = std::min(box_max_lat, kWorldMaxLat);
    box_min_lng = std::max(box_min_lng, kWorldMinLng);
    box_max_lng = std::min(box_max_lng, kWorldMaxLng);

    // After clamping, if the box now covers most of the world horizontally,
    // skip straight to the canonical world view to avoid an off-centre look.
    if ((box_max_lng - box_min_lng) > (kWorldMaxLng - kWorldMinLng) * 0.85) {
        auto world = QGV::GeoRect(QGV::GeoPos(kWorldMaxLat, kWorldMinLng),
                                  QGV::GeoPos(kWorldMinLat, kWorldMaxLng));
        map_->flyTo(QGVCameraActions(map_).scaleTo(world));
        return;
    }

    auto target = QGV::GeoRect(QGV::GeoPos(box_max_lat, box_min_lng),
                               QGV::GeoPos(box_min_lat, box_max_lng));
    map_->flyTo(QGVCameraActions(map_).scaleTo(target));
}

} // namespace fincept::ui
