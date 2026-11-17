// src/ui/widgets/map/GeoBaseMap.h
//
// The offline base map behind WorldMapWidget: Natural Earth land, lakes, country
// borders and label points (public domain, bundled as :/maps/natural_earth.bin),
// rasterised into 256 px Web-Mercator tiles on worker threads.
//
// Nothing here needs the network, so the map always has a base layer — online
// imagery (ImageryTiles.h) is drawn over it when the user picks it and fails soft
// to this one.
#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVector>

#include <deque>
#include <memory>

namespace fincept::ui::map {

// ── Web-Mercator (EPSG:3857) in normalised world units ──────────────────────
// x and y run 0..1 across the whole world; (0,0) is the top-left corner
// (lng -180, lat +85.0511). A zoom-z map is 256 * 2^z px wide.
constexpr double kMaxLatitude = 85.05112878;
constexpr int kTilePx = 256;

QPointF geo_to_world(double lat, double lng);
void world_to_geo(const QPointF& world, double* lat, double* lng);
/// Ground metres per screen pixel at `lat` for zoom `zoom` (for the scale bar).
double metres_per_pixel(double lat, double zoom);

/// A named point from the base data: a country, sea or port label.
struct MapLabel {
    QPointF world;        // normalised Web-Mercator position
    double lat = 0;
    double lng = 0;
    float min_zoom = 0;   // shown from this zoom on
    int rank = 0;         // lower = more important
    QString name;
};

/// Colours one base tile is drawn with. Captured on the GUI thread from the theme
/// tokens and handed to the workers by value.
struct BaseMapPalette {
    QColor water;
    QColor land;
    QColor coast;
    QColor border;
    /// Reference overlay for imagery: no fills, only coastline + border lines.
    bool lines_only = false;
    /// Changes whenever the colours do — part of the tile cache key.
    quint32 key = 0;
};

/// The parsed Natural Earth data. Immutable once built, shared read-only by the
/// tile workers and the GUI thread.
class GeoBaseMapData {
  public:
    struct Feature {
        QRectF bounds; // normalised world units
        QPainterPath path;
    };
    struct Layer {
        QVector<Feature> features;
    };

    /// Low detail (110m) below zoom 3, 50m from there on.
    static int lod_for_zoom(int zoom) { return zoom < 3 ? 0 : 1; }

    Layer land[2];
    Layer lakes[2];
    Layer borders[2];
    QVector<MapLabel> countries;
    QVector<MapLabel> seas;
    QVector<MapLabel> ports;

    /// Draw tile (z, x, y) — x already wrapped into [0, 2^z). Thread-safe.
    QImage render_tile(int z, int x, int y, const BaseMapPalette& palette, qreal dpr) const;

    /// Parse the bundled resource. Empty (null) on a damaged or missing resource.
    static std::shared_ptr<const GeoBaseMapData> load_resource();
};

/// Process-wide owner of the base data and the base-tile cache + render queue.
/// GUI-thread object; rendering happens on its own small thread pool.
class GeoBaseMap : public QObject {
    Q_OBJECT
  public:
    static GeoBaseMap& instance();

    /// The data, or null while it is still loading (loading starts on first call).
    std::shared_ptr<const GeoBaseMapData> data();
    /// True once the resource failed to load (the map then shows a plain sea colour).
    bool failed() const { return failed_; }

    /// Cached tile or null. A miss queues a render (newest request first) unless
    /// `queue` is false; tile_ready() fires when it is in the cache.
    const QImage* tile(int z, int x, int y, const BaseMapPalette& palette, qreal dpr, bool queue = true);

  signals:
    void data_ready();
    void tile_ready();

  private:
    GeoBaseMap();
    struct Job {
        quint64 key;
        int z, x, y;
        BaseMapPalette palette;
        qreal dpr;
    };
    void start_workers();
    void worker_loop();
    void deliver(quint64 key, QImage image);
    static quint64 tile_key(int z, int x, int y, const BaseMapPalette& palette, qreal dpr);

    std::shared_ptr<const GeoBaseMapData> data_;
    bool loading_ = false;
    bool failed_ = false;

    // GUI thread only.
    QHash<quint64, QImage> cache_;
    std::deque<quint64> cache_order_; // insertion order, oldest first (FIFO eviction)
    qint64 cache_bytes_ = 0;
    QSet<quint64> pending_;

    // Shared with the workers.
    QMutex queue_mutex_;
    std::deque<Job> queue_; // newest at the back
    int active_workers_ = 0;
};

/// Major maritime chokepoints (straits and canals that carry a large share of
/// seaborne trade) — static reference geography for the Maritime map.
const QVector<MapLabel>& maritime_chokepoints();

} // namespace fincept::ui::map
