// src/ui/widgets/WorldMapWidget.h
#pragma once
#include <QColor>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QGVMap;
class QGVLayer;
class QGVItem;

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

/// A user-drawn selection area on the map. Rectangle and Circle only.
/// Geometry is stored in geographic coordinates so it survives pan/zoom.
struct MapShape {
    enum class Type { Rectangle, Circle };
    Type type = Type::Rectangle;

    // Rectangle: the axis-aligned geo bounds.
    // Circle: bounds is the bounding box of the circle (for quick union /
    // bbox use); centre + radius_km carry the true circle for point tests.
    double min_lat = 0, max_lat = 0, min_lng = 0, max_lng = 0;

    // Circle only.
    double centre_lat = 0, centre_lng = 0;
    double radius_km = 0;

    /// True if (lat,lng) lies inside this shape. Rectangle = bbox test;
    /// Circle = great-circle distance <= radius.
    bool contains(double lat, double lng) const;
};

/// Active map drawing tool. None disables drawing (normal pan/zoom).
enum class MapDrawMode { None, Rectangle, Circle };

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

    /// Draw a connecting polyline through the ordered lat/lng points (a voyage
    /// track), rendered beneath the marker pins. Each QPointF is (lat, lng).
    /// Fewer than 2 points clears it. set_pins() also clears any existing path.
    void set_track_path(const QVector<QPointF>& latlng_points);
    void clear_track_path();

    // ── Basemap selection ───────────────────────────────────────────────────
    /// Display labels for the available basemaps, in selector order
    /// (SATELLITE, DARK, OCEAN, STREETS, LIGHT, TERRAIN). Use to populate a
    /// QComboBox; the index maps 1:1 to set_basemap().
    static QStringList basemap_labels();
    /// Switch the active basemap by index into basemap_labels(). Preserves the
    /// current camera, pins, and drawn shapes. Out-of-range indices are ignored.
    void set_basemap(int index);
    int current_basemap_index() const { return basemap_index_; }

    /// Fly the camera to show a specific region
    void fly_to(double lat, double lng, int zoom = 5);

    /// Auto-zoom to fit all current pins with padding
    void fit_to_pins();

    // ── Drawing ─────────────────────────────────────────────────────────────
    /// Set the active draw tool. While a tool is active the map suppresses
    /// pan so a drag draws a shape instead. None restores normal navigation.
    void set_draw_mode(MapDrawMode mode);
    MapDrawMode draw_mode() const { return draw_mode_; }
    /// Remove all user-drawn shapes (emits shapes_changed).
    void clear_shapes();
    const QVector<MapShape>& shapes() const { return shapes_; }

  signals:
    /// Emitted when a pin with `id != -1` is clicked. Carries the caller-
    /// assigned id so the consumer can resolve back to its own data.
    void pin_clicked(int id);
    /// Emitted whenever the set of drawn shapes changes (new shape finalized
    /// or shapes cleared). The consumer reads shapes() to react.
    void shapes_changed();

  protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    /// Tear down the current tile layer(s) and install the basemap at `index`
    /// from the internal catalog. Sends the new tiles to the back so the
    /// marker and shape layers keep drawing on top.
    void apply_basemap(int index);
    void rebuild_markers();
    void rebuild_path_overlay();
    void rebuild_shape_overlays();
    QImage make_marker_image(const QColor& color, double radius) const;
    /// Convert a viewport pixel (in the QGraphicsView) to geographic lat/lng.
    bool pixel_to_geo(const QPoint& viewport_px, double* out_lat, double* out_lng) const;
    /// Memoized wrapper over make_marker_image: pins sharing a (color, radius)
    /// reuse one rendered QImage instead of repainting an antialiased gradient
    /// each. Most pin sets use 1-2 distinct styles, so this collapses N paints
    /// to a couple regardless of pin count.
    const QImage& marker_image(const QColor& color, double radius) const;

    QGVMap* map_ = nullptr;
    QGVLayer* marker_layer_ = nullptr;
    QGVLayer* path_layer_ = nullptr;
    QGVLayer* shape_layer_ = nullptr;
    /// Ordered (lat, lng) points of the current track polyline, if any.
    QVector<QPointF> track_points_;
    /// Active basemap tile layer(s). Owned by the map once added; replaced
    /// wholesale on set_basemap(). Usually one layer.
    QVector<QGVItem*> tile_layers_;
    int basemap_index_ = 0;  // index into the basemap catalog; 0 = SATELLITE
    QVector<MapPin> pins_;
    mutable QHash<QString, QImage> marker_cache_;
    bool map_ready_ = false;

    // Drawing state.
    MapDrawMode draw_mode_ = MapDrawMode::None;
    QVector<MapShape> shapes_;
    bool drawing_ = false;
    double drag_start_lat_ = 0, drag_start_lng_ = 0;  // anchor of in-progress shape
    MapShape preview_shape_;                           // shape being dragged
    bool has_preview_ = false;
};

} // namespace fincept::ui
