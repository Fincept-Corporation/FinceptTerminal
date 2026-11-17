// src/ui/widgets/WorldMapWidget.h
#pragma once
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QVariantAnimation;

namespace fincept::ui {

/// A map pin to plot on the world map.
struct MapPin {
    double latitude = 0;
    double longitude = 0;
    QString label;
    /// Content colour (category / status), written as it looks on the dark theme.
    /// On the themed base map it is drawn through colors::legible_color(); on
    /// satellite imagery as is.
    QColor color{255, 100, 0};
    double radius = 5.0;
    /// Caller-assigned identifier carried back through `pin_clicked`.
    /// Useful for mapping a click to a row in a side table or to a domain
    /// object — leave at -1 if the pin is not interactive. Interactive pins
    /// (id >= 0) are clustered when there are many of them; non-interactive
    /// ones (trails, port markers) are always drawn individually, in order.
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

/// Reference layers drawn from bundled, offline data.
enum class MapLayer {
    Ports = 0x1,       ///< Natural Earth major ports (~1,100)
    Chokepoints = 0x2, ///< maritime straits / canals
};

/// Interactive world map. The base map is bundled Natural Earth data (public
/// domain) drawn locally in the terminal's theme colours, so it works offline and
/// never shows a blank rectangle; SATELLITE / NIGHT LIGHTS add NASA GIBS imagery
/// (public, no key) on top and fall back to the base map when it cannot be
/// fetched. Pan (drag), zoom (wheel, double-click, +/-), hover tooltips, click
/// selection, marker clustering, user-drawn shapes and a voyage polyline.
/// Everything heavy (tile rasterising, image decoding, disk I/O) runs off the
/// GUI thread; painting only blits cached images.
class WorldMapWidget : public QWidget {
    Q_OBJECT
  public:
    explicit WorldMapWidget(QWidget* parent = nullptr);
    ~WorldMapWidget() override;

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
    /// (MAP, SATELLITE, NIGHT LIGHTS). Use to populate a QComboBox; the index
    /// maps 1:1 to set_basemap().
    static QStringList basemap_labels();
    /// Switch the active basemap by index into basemap_labels(). Preserves the
    /// current camera, pins, and drawn shapes. Out-of-range indices are ignored.
    void set_basemap(int index);
    int current_basemap_index() const { return basemap_index_; }

    /// Show / hide a bundled reference layer.
    void set_reference_layer(MapLayer layer, bool on);
    bool reference_layer(MapLayer layer) const { return (layers_ & static_cast<int>(layer)) != 0; }

    /// Fly the camera to a point. Keeps the current zoom when it is already closer.
    void fly_to(double lat, double lng, int zoom = 5);

    /// Auto-zoom to fit all current pins with padding. The first call after
    /// restore_view_state() is skipped so a restored view survives the first load.
    void fit_to_pins();

    /// Highlight the pin carrying `id` (-1 clears). `reveal` pans it into view when
    /// it is off-screen.
    void set_selected_pin(int id, bool reveal = true);
    int selected_pin() const { return selected_id_; }

    /// A short message drawn over the map (e.g. why there is no data); empty hides it.
    void set_overlay_message(const QString& text);

    /// Camera + basemap + reference layers, for IStatefulScreen persistence.
    QVariantMap view_state() const;
    void restore_view_state(const QVariantMap& state);

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
    /// The camera, basemap or a layer changed and has settled (for persistence).
    void view_changed();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    struct Cluster {
        QPointF world;        // mean position of the members
        QVector<int> members; // indices into pins_
        QColor color;         // most frequent member colour
        double radius = 5.0;  // largest member radius
    };
    /// One thing on screen that reacts to the mouse (rebuilt every paint).
    struct HitTarget {
        QPointF pos;
        double radius = 0;
        int cluster = -1; // index into clusters_[hit_zoom_] (2+ members)
        int pin = -1;     // index into pins_
        QString ref;      // reference-layer feature (port / chokepoint): its tooltip
    };

    // Camera.
    double world_px() const; // world width in px at the current zoom
    QPointF world_to_screen(const QPointF& w) const;
    QPointF screen_to_world(const QPointF& s) const;
    /// Copies of the (horizontally repeating) world that touch the view.
    void visible_copies(int* first, int* last) const;
    double min_zoom() const;
    void clamp_camera();
    void camera_moved(bool interactive);
    void zoom_at(const QPointF& anchor, double new_zoom, bool animate);
    void animate_to(const QPointF& center, double zoom);
    void fit_world_rect(const QRectF& world_rect, double max_zoom, bool animate);
    void do_fit_pins();
    void apply_initial_view();
    void view_settled();
    bool pixel_to_geo(const QPointF& px, double* lat, double* lng) const;

    // Drawing passes.
    void refresh_palette();
    void draw_base(QPainter& p, bool lines_only);
    void draw_imagery(QPainter& p);
    void draw_labels(QPainter& p);
    void draw_reference_layers(QPainter& p);
    void draw_shapes(QPainter& p);
    void draw_track(QPainter& p);
    void draw_pins(QPainter& p);
    void draw_chrome(QPainter& p);
    void request_imagery();
    const QPixmap& pin_pixmap(const QColor& color, double radius);
    const QPixmap& label_pixmap(const QString& text, int style);
    const QVector<Cluster>& clusters_for_zoom(int zoom);
    void rebuild_pin_world();
    int hit_test(const QPointF& pos) const; // index into hits_, -1 = none
    void update_hover(const QPointF& pos);
    void clear_hover();
    QString tooltip_for(const HitTarget& hit) const;
    QRectF zoom_button_rect(int which) const; // 0 = +, 1 = −
    int imagery_source() const;               // -1 = offline base map only
    QString status_text() const;
    void schedule_repaint();

    // Camera state.
    QPointF center_{0.53, 0.40};
    double zoom_ = 1.5;
    bool camera_initialised_ = false; // the first real size sets the initial view
    bool view_preset_ = false;        // a restored / flown-to view waits for that first size
    bool suppress_next_fit_ = false;
    bool fit_pending_ = false; // fit_to_pins() before the widget had a size
    int basemap_index_ = 0;
    int layers_ = 0;

    // Content.
    QVector<MapPin> pins_;
    QVector<QPointF> pin_world_;
    QHash<int, QVector<Cluster>> clusters_; // per integer zoom, rebuilt lazily
    int clusterable_count_ = 0;
    int selected_id_ = -1;
    QVector<QPointF> track_world_;
    QString overlay_message_;

    // Hit testing / hover.
    QVector<HitTarget> hits_;
    int hit_zoom_ = 0;
    int hover_pin_ = -1;      // pins_ index under the mouse
    int hover_cluster_ = -1;  // clusters_[hit_zoom_] index under the mouse
    QString hover_ref_;

    // Interaction.
    bool pressed_ = false;
    bool dragged_ = false;
    QPointF press_pos_;
    QPointF press_center_;
    bool interacting_ = false; // dragging / animating: draw from cache, request when settled
    QTimer settle_timer_;
    QTimer repaint_timer_;
    QVariantAnimation* anim_ = nullptr;
    QMetaObject::Connection anim_conn_;

    // Drawing state.
    MapDrawMode draw_mode_ = MapDrawMode::None;
    QVector<MapShape> shapes_;
    bool drawing_ = false;
    double drag_start_lat_ = 0, drag_start_lng_ = 0; // anchor of in-progress shape
    MapShape preview_shape_;                         // shape being dragged
    bool has_preview_ = false;

    // Theme-derived colours and caches.
    struct PaletteData {
        QColor water, land, coast, border;
        QColor label, label_sea, halo_land, halo_sea;
        QColor accent, ring, chrome_bg, chrome_border, chrome_text, chrome_dim;
        QColor port, chokepoint, pin_outline;
        QColor over_imagery_line, over_imagery_text, over_imagery_halo;
        quint32 base_key = 0, lines_key = 0;
    } pal_;
    QHash<QString, QPixmap> pin_cache_;
    QHash<QString, QPixmap> label_cache_;
};

} // namespace fincept::ui
