// src/ui/widgets/WorldMapWidget.cpp
#include "ui/widgets/WorldMapWidget.h"

#include <QGeoView/QGVGlobal.h>
#include <QGeoView/QGVMap.h>
#include <QGeoView/QGVCamera.h>
#include <QGeoView/QGVLayer.h>
#include <QGeoView/QGVLayerOSM.h>
#include <QGeoView/QGVWidgetZoom.h>
#include <QGeoView/QGVWidgetScale.h>
#include <QGeoView/Raster/QGVIcon.h>

#include <QGraphicsView>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QPainter>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace fincept::ui {

// ── One-time network manager setup for QGeoView tile loading ────────────────
static void ensure_network_manager() {
    if (QGV::getNetworkManager()) return;

    auto* nam = new QNetworkAccessManager(qApp);
    auto* cache = new QNetworkDiskCache(nam);
    QString cache_dir = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + "/map_tiles";
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

    // Use CartoDB Dark Matter tiles (same as the Tauri/React geopolitics map)
    auto* tiles = new QGVLayerOSM(
        QStringLiteral("https://basemaps.cartocdn.com/dark_all/${z}/${x}/${y}.png"));
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
    if (size < 8) size = 8;
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
    if (!map_ready_ || !marker_layer_) return;

    // Remove all existing markers from the layer
    while (marker_layer_->countItems() > 0) {
        auto* item = marker_layer_->getItem(0);
        marker_layer_->removeItem(item);
        delete item;
    }

    // Add new markers
    for (const auto& pin : pins_) {
        auto* icon = new QGVIcon();
        icon->setGeometry(QGV::GeoPos(pin.latitude, pin.longitude),
                          QSizeF(20, 20));
        icon->loadImage(make_marker_image(pin.color, pin.radius));
        icon->setFlags(QGV::ItemFlag::IgnoreScale | QGV::ItemFlag::IgnoreAzimuth);
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

void WorldMapWidget::fly_to(double lat, double lng, int zoom) {
    Q_UNUSED(zoom);
    if (!map_ready_) return;
    double d = 5.0;
    auto target = QGV::GeoRect(
        QGV::GeoPos(lat + d, lng - d * 1.5),
        QGV::GeoPos(lat - d, lng + d * 1.5));
    map_->flyTo(QGVCameraActions(map_).scaleTo(target));
}

void WorldMapWidget::fit_to_pins() {
    if (!map_ready_ || pins_.isEmpty()) return;

    double min_lat = 90, max_lat = -90, min_lng = 180, max_lng = -180;
    for (const auto& pin : pins_) {
        if (pin.latitude < min_lat) min_lat = pin.latitude;
        if (pin.latitude > max_lat) max_lat = pin.latitude;
        if (pin.longitude < min_lng) min_lng = pin.longitude;
        if (pin.longitude > max_lng) max_lng = pin.longitude;
    }

    // Add padding (at least 2 degrees, or 20% of the span)
    double lat_span = std::max(max_lat - min_lat, 1.0);
    double lng_span = std::max(max_lng - min_lng, 1.0);
    double lat_pad = std::max(lat_span * 0.2, 2.0);
    double lng_pad = std::max(lng_span * 0.2, 2.0);

    auto target = QGV::GeoRect(
        QGV::GeoPos(max_lat + lat_pad, min_lng - lng_pad),
        QGV::GeoPos(min_lat - lat_pad, max_lng + lng_pad));
    map_->flyTo(QGVCameraActions(map_).scaleTo(target));
}

} // namespace fincept::ui
