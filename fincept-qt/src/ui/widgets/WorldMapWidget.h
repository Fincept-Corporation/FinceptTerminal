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
    /// Caller-assigned identifier carried back through `pin_clicked`.
    /// Useful for mapping a click to a row in a side table or to a domain
    /// object — leave at -1 if the pin is not interactive.
    int id = -1;
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
    /// Emitted when a pin with `id != -1` is clicked. Carries the caller-
    /// assigned id so the consumer can resolve back to its own data.
    void pin_clicked(int id);

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void rebuild_markers();
    QImage make_marker_image(const QColor& color, double radius) const;

    QGVMap* map_ = nullptr;
    QGVLayer* marker_layer_ = nullptr;
    QVector<MapPin> pins_;
    bool map_ready_ = false;
};

} // namespace fincept::ui
