// src/ui/widgets/WorldMapWidget.h
#pragma once
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class QGVMap;
class QGVLayer;

namespace fincept::ui {

/// A map pin to plot on the world map.
struct MapPin {
    double latitude = 0;
    double longitude = 0;
    QString label;
    QColor color{255, 100, 0};
    double radius = 5.0;
};

/// Interactive world map widget backed by QGeoView (real OSM/CartoDB tiles).
/// Supports pan, zoom, and colored marker pins at lat/lng coordinates.
class WorldMapWidget : public QWidget {
    Q_OBJECT
  public:
    explicit WorldMapWidget(QWidget* parent = nullptr);

    void set_pins(const QVector<MapPin>& pins);
    void add_pin(const MapPin& pin);
    void clear_pins();
    int pin_count() const { return pins_.size(); }

    /// Fly the camera to show a specific region
    void fly_to(double lat, double lng, int zoom = 5);

    /// Auto-zoom to fit all current pins with padding
    void fit_to_pins();

  signals:
    void pin_clicked(const QString& label, double lat, double lng);

  private:
    void rebuild_markers();
    QImage make_marker_image(const QColor& color, double radius) const;

    QGVMap* map_ = nullptr;
    QGVLayer* marker_layer_ = nullptr;
    QVector<MapPin> pins_;
    bool map_ready_ = false;
};

} // namespace fincept::ui
