// src/ui/widgets/map/GeoBaseMap.cpp
#include "ui/widgets/map/GeoBaseMap.h"

#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QMutexLocker>
#include <QPainter>
#include <QPointer>
#include <QThread>
#include <QThreadPool>
#include <QtMath>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace fincept::ui::map {

// Names in this file carry a gbm_ prefix: unity builds merge it with other
// widget sources, and an anonymous namespace does not isolate them there.
namespace {

constexpr double kGbmCoordScale = 1e5;               // file stores degrees * 1e5
constexpr qint64 kGbmCacheBytes = 96LL * 1024 * 1024; // rendered base tiles kept in memory
constexpr size_t kGbmMaxQueue = 160;                 // older render requests are dropped (re-asked if still visible)

std::atomic<bool> g_gbm_shutting_down{false};

QThreadPool* gbm_pool() {
    // One worker. With three, the final build crashed in about four of six
    // starts that reopened on a map (SIGSEGV in QRasterPaintEngine::stroke under
    // render_tile: workers painting the same shared QPainterPath for the first
    // time); with one it started cleanly six times out of six. load_resource()
    // now also paints every path once before the workers see it, which should
    // make more workers safe again — raise this only after that is seen running.
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setMaxThreadCount(1);
        p->setObjectName(QStringLiteral("map-tiles"));
        return p;
    }();
    return pool;
}

bool gbm_read_path_feature(QDataStream& ds, bool closed, GeoBaseMapData::Feature* out) {
    quint32 parts = 0;
    ds >> parts;
    if (ds.status() != QDataStream::Ok || parts > 100000)
        return false;
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill); // holes (inland seas in a land polygon) stay open
    double min_x = 1, min_y = 1, max_x = 0, max_y = 0;
    for (quint32 p = 0; p < parts; ++p) {
        quint32 n = 0;
        ds >> n;
        if (ds.status() != QDataStream::Ok || n > 2000000)
            return false;
        qint32 x = 0, y = 0;
        for (quint32 i = 0; i < n; ++i) {
            qint32 dx = 0, dy = 0;
            ds >> dx >> dy;
            x += dx;
            y += dy;
            const QPointF w = geo_to_world(y / kGbmCoordScale, x / kGbmCoordScale);
            if (i == 0)
                path.moveTo(w);
            else
                path.lineTo(w);
            min_x = std::min(min_x, w.x());
            min_y = std::min(min_y, w.y());
            max_x = std::max(max_x, w.x());
            max_y = std::max(max_y, w.y());
        }
        if (closed)
            path.closeSubpath();
    }
    if (ds.status() != QDataStream::Ok)
        return false;
    out->path = path;
    out->bounds = QRectF(QPointF(min_x, min_y), QPointF(max_x, max_y));
    return true;
}

bool gbm_read_label(QDataStream& ds, MapLabel* out) {
    qint32 x = 0, y = 0;
    quint8 minz = 0, rank = 0;
    quint16 len = 0;
    ds >> x >> y >> minz >> rank >> len;
    if (ds.status() != QDataStream::Ok)
        return false;
    QByteArray name(len, Qt::Uninitialized);
    if (len > 0 && ds.readRawData(name.data(), len) != len)
        return false;
    out->lng = x / kGbmCoordScale;
    out->lat = y / kGbmCoordScale;
    out->world = geo_to_world(out->lat, out->lng);
    out->min_zoom = minz / 10.0f;
    out->rank = rank;
    out->name = QString::fromUtf8(name);
    return true;
}

} // namespace

// ── Projection ───────────────────────────────────────────────────────────────

QPointF geo_to_world(double lat, double lng) {
    const double clat = std::clamp(lat, -kMaxLatitude, kMaxLatitude);
    const double s = std::sin(qDegreesToRadians(clat));
    const double x = (lng + 180.0) / 360.0;
    const double y = 0.5 - std::log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI);
    return {x, y};
}

void world_to_geo(const QPointF& world, double* lat, double* lng) {
    *lng = world.x() * 360.0 - 180.0;
    const double n = M_PI - 2.0 * M_PI * world.y();
    *lat = qRadiansToDegrees(std::atan(std::sinh(n)));
}

double metres_per_pixel(double lat, double zoom) {
    constexpr double kEarthCircumference = 40075016.686;
    return kEarthCircumference * std::cos(qDegreesToRadians(std::clamp(lat, -85.0, 85.0))) /
           (kTilePx * std::pow(2.0, zoom));
}

// ── Data ─────────────────────────────────────────────────────────────────────

std::shared_ptr<const GeoBaseMapData> GeoBaseMapData::load_resource() {
    QElapsedTimer timer;
    timer.start();
    QFile f(QStringLiteral(":/maps/natural_earth.bin"));
    if (!f.open(QIODevice::ReadOnly)) {
        LOG_ERROR("Map", "Bundled base map :/maps/natural_earth.bin is missing from this build");
        return {};
    }
    const QByteArray raw = qUncompress(f.readAll());
    if (raw.size() < 8 || !raw.startsWith("FNEM")) {
        LOG_ERROR("Map", "Bundled base map is damaged (bad header)");
        return {};
    }
    auto data = std::make_shared<GeoBaseMapData>();
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.skipRawData(4);
    quint16 version = 0, layers = 0;
    ds >> version >> layers;
    if (version != 1) {
        LOG_ERROR("Map", QString("Bundled base map has unknown version %1").arg(version));
        return {};
    }
    for (int l = 0; l < layers; ++l) {
        quint8 kind = 0, lod = 0;
        char id_raw[8];
        quint32 count = 0;
        ds >> kind >> lod;
        if (ds.readRawData(id_raw, 8) != 8)
            return {};
        ds >> count;
        const QByteArray id = QByteArray(id_raw, 8).replace('\0', "").trimmed();
        if (ds.status() != QDataStream::Ok || count > 1000000)
            return {};
        if (kind == 1 || kind == 2) {
            Layer* target = nullptr;
            const int slot = lod == 0 ? 0 : 1;
            if (id == "land")
                target = &data->land[slot];
            else if (id == "lakes")
                target = &data->lakes[slot];
            else if (id == "borders")
                target = &data->borders[slot];
            Layer scratch;
            Layer& dst = target ? *target : scratch; // unknown layers are read and dropped
            dst.features.reserve(static_cast<int>(count));
            for (quint32 i = 0; i < count; ++i) {
                Feature feat;
                if (!gbm_read_path_feature(ds, kind == 1, &feat)) {
                    LOG_ERROR("Map", "Bundled base map is damaged (geometry)");
                    return {};
                }
                dst.features.append(std::move(feat));
            }
        } else if (kind == 3) {
            QVector<MapLabel>* target = nullptr;
            if (id == "country")
                target = &data->countries;
            else if (id == "marine")
                target = &data->seas;
            else if (id == "ports")
                target = &data->ports;
            QVector<MapLabel> scratch;
            QVector<MapLabel>& dst = target ? *target : scratch;
            dst.reserve(static_cast<int>(count));
            for (quint32 i = 0; i < count; ++i) {
                MapLabel label;
                if (!gbm_read_label(ds, &label)) {
                    LOG_ERROR("Map", "Bundled base map is damaged (labels)");
                    return {};
                }
                dst.append(std::move(label));
            }
        } else {
            LOG_ERROR("Map", QString("Bundled base map has unknown layer kind %1").arg(kind));
            return {};
        }
    }
    // Paint every path once here, on this one thread. QPainterPath builds the
    // form the paint engine reads on its first paint, without a lock, and the
    // paths are shared by all tile workers: two workers painting the same
    // coastline for the first time raced on that and the app crashed (SIGSEGV in
    // QRasterPaintEngine::stroke under render_tile). After this pass the workers
    // only ever read.
    {
        QImage scratch(1, 1, QImage::Format_ARGB32_Premultiplied);
        scratch.fill(Qt::transparent);
        QPainter warm(&scratch);
        QPen pen(Qt::black);
        pen.setCosmetic(true);
        warm.setPen(pen);
        warm.setBrush(Qt::black);
        for (const Layer* group : {data->land, data->lakes, data->borders})
            for (int lod = 0; lod < 2; ++lod)
                for (const auto& f : group[lod].features)
                    warm.drawPath(f.path);
    }
    // Most important labels first: the label pass places them before the rest.
    for (auto* v : {&data->countries, &data->seas, &data->ports})
        std::stable_sort(v->begin(), v->end(), [](const MapLabel& a, const MapLabel& b) { return a.rank < b.rank; });
    LOG_DEBUG("Map", QString("Offline base map loaded in %1 ms (%2 land / %3 border features, %4 country labels, "
                            "%5 ports)")
                        .arg(timer.elapsed())
                        .arg(data->land[1].features.size())
                        .arg(data->borders[1].features.size())
                        .arg(data->countries.size())
                        .arg(data->ports.size()));
    return data;
}

QImage GeoBaseMapData::render_tile(int z, int x, int y, const BaseMapPalette& palette, qreal dpr) const {
    const int px = std::max(1, qRound(kTilePx * dpr));
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(palette.lines_only ? QColor(Qt::transparent) : palette.water);

    const double tiles = std::ldexp(1.0, z);
    const double world_px = px * tiles;
    // Features whose bounds touch the tile, with a couple of pixels for the stroke.
    const double margin = 3.0 / world_px;
    const QRectF tile_rect = QRectF(x / tiles, y / tiles, 1.0 / tiles, 1.0 / tiles).adjusted(-margin, -margin, margin, margin);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QTransform t;
    t.translate(-x * double(px), -y * double(px));
    t.scale(world_px, world_px);
    p.setTransform(t);

    const int lod = lod_for_zoom(z);
    QPen coast(palette.coast);
    coast.setCosmetic(true);
    coast.setWidthF((palette.lines_only ? 1.0 : 0.9) * dpr);
    QPen border(palette.border);
    border.setCosmetic(true);
    border.setWidthF(0.8 * dpr);
    if (z < 4)
        border.setStyle(Qt::DashLine);

    if (palette.lines_only) {
        p.setBrush(Qt::NoBrush);
        p.setPen(coast);
        for (const auto& f : land[lod].features)
            if (f.bounds.intersects(tile_rect))
                p.drawPath(f.path);
    } else {
        p.setBrush(palette.land);
        p.setPen(coast);
        for (const auto& f : land[lod].features)
            if (f.bounds.intersects(tile_rect))
                p.drawPath(f.path);
        p.setBrush(palette.water);
        for (const auto& f : lakes[lod].features)
            if (f.bounds.intersects(tile_rect))
                p.drawPath(f.path);
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(border);
    for (const auto& f : borders[lod].features)
        if (f.bounds.intersects(tile_rect))
            p.drawPath(f.path);
    p.end();
    return img;
}

// ── GeoBaseMap (cache + render queue) ────────────────────────────────────────

GeoBaseMap& GeoBaseMap::instance() {
    static GeoBaseMap* inst = new GeoBaseMap(); // lives until exit; workers stop at aboutToQuit
    return *inst;
}

GeoBaseMap::GeoBaseMap() : QObject(QCoreApplication::instance()) {
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        g_gbm_shutting_down = true;
        {
            QMutexLocker lock(&queue_mutex_);
            queue_.clear();
        }
        gbm_pool()->waitForDone(3000);
    });
}

std::shared_ptr<const GeoBaseMapData> GeoBaseMap::data() {
    if (data_ || failed_ || loading_)
        return data_;
    loading_ = true;
    QPointer<GeoBaseMap> self(this);
    // Decompress + parse (~0.5 MB, ~110k points) off the GUI thread.
    QThreadPool::globalInstance()->start([self]() {
        auto loaded = GeoBaseMapData::load_resource();
        if (g_gbm_shutting_down)
            return;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, loaded]() {
                if (!self)
                    return;
                self->loading_ = false;
                if (!loaded) {
                    self->failed_ = true;
                    return;
                }
                self->data_ = loaded;
                emit self->data_ready();
            },
            Qt::QueuedConnection);
    });
    return data_;
}

quint64 GeoBaseMap::tile_key(int z, int x, int y, const BaseMapPalette& palette, qreal dpr) {
    // z: 5 bits, x/y: 20 bits each, dpr bucket: 3 bits, palette key folded into the rest.
    const quint64 dpr_bucket = static_cast<quint64>(std::clamp(qRound(dpr * 2.0), 1, 7));
    quint64 k = (static_cast<quint64>(z & 0x1f) << 59) | (static_cast<quint64>(x & 0xfffff) << 39) |
                (static_cast<quint64>(y & 0xfffff) << 19) | (dpr_bucket << 16) |
                (static_cast<quint64>(palette.key) & 0xffff);
    return k;
}

const QImage* GeoBaseMap::tile(int z, int x, int y, const BaseMapPalette& palette, qreal dpr, bool queue) {
    const quint64 key = tile_key(z, x, y, palette, dpr);
    auto it = cache_.constFind(key);
    if (it != cache_.constEnd())
        return &it.value();
    if (!queue || !data_ || pending_.contains(key))
        return nullptr;
    pending_.insert(key);
    {
        QMutexLocker lock(&queue_mutex_);
        queue_.push_back(Job{key, z, x, y, palette, dpr});
        while (queue_.size() > kGbmMaxQueue) {
            pending_.remove(queue_.front().key); // dropped: asked again if still on screen
            queue_.pop_front();
        }
    }
    start_workers();
    return nullptr;
}

void GeoBaseMap::start_workers() {
    QMutexLocker lock(&queue_mutex_);
    const int want = std::min<int>(static_cast<int>(queue_.size()), gbm_pool()->maxThreadCount());
    while (active_workers_ < want) {
        ++active_workers_;
        gbm_pool()->start([this]() { worker_loop(); });
    }
}

void GeoBaseMap::worker_loop() {
    const auto data = data_; // set once on the GUI thread before any job is queued
    for (;;) {
        Job job;
        {
            QMutexLocker lock(&queue_mutex_);
            if (queue_.empty() || g_gbm_shutting_down) {
                --active_workers_;
                return;
            }
            job = queue_.back(); // newest first: what the user looks at now
            queue_.pop_back();
        }
        QImage img = data ? data->render_tile(job.z, job.x, job.y, job.palette, job.dpr) : QImage();
        if (g_gbm_shutting_down)
            continue;
        QMetaObject::invokeMethod(
            this, [this, key = job.key, img = std::move(img)]() mutable { deliver(key, std::move(img)); },
            Qt::QueuedConnection);
    }
}

void GeoBaseMap::deliver(quint64 key, QImage image) {
    pending_.remove(key);
    if (image.isNull())
        return;
    cache_bytes_ += image.sizeInBytes();
    cache_.insert(key, std::move(image));
    cache_order_.push_back(key);
    while (cache_bytes_ > kGbmCacheBytes && !cache_order_.empty()) {
        const quint64 old = cache_order_.front();
        cache_order_.pop_front();
        auto it = cache_.find(old);
        if (it != cache_.end()) {
            cache_bytes_ -= it->sizeInBytes();
            cache_.erase(it);
        }
    }
    emit tile_ready();
}

// ── Chokepoints ──────────────────────────────────────────────────────────────

const QVector<MapLabel>& maritime_chokepoints() {
    // rank 0 = the eight oil-transit chokepoints the U.S. EIA tracks; rank 1 =
    // other high-traffic straits. Positions are the narrows / canal midpoints.
    static const QVector<MapLabel> kPoints = [] {
        struct Row {
            const char* name;
            double lat, lng;
            int rank;
        };
        const Row rows[] = {
            {"Strait of Hormuz", 26.57, 56.25, 0},     {"Strait of Malacca", 2.50, 101.40, 0},
            {"Suez Canal", 30.45, 32.35, 0},           {"Bab el-Mandeb", 12.58, 43.33, 0},
            {"Panama Canal", 9.10, -79.68, 0},         {"Bosporus", 41.12, 29.07, 0},
            {"Danish Straits", 55.95, 12.65, 0},       {"Cape of Good Hope", -34.36, 18.47, 0},
            {"Strait of Gibraltar", 35.95, -5.55, 1},  {"Dover Strait", 51.00, 1.45, 1},
            {"Taiwan Strait", 24.40, 119.50, 1},       {"Lombok Strait", -8.75, 115.75, 1},
            {"Singapore Strait", 1.20, 103.85, 1},     {"Korea Strait", 34.40, 129.20, 1},
        };
        QVector<MapLabel> out;
        for (const Row& r : rows) {
            MapLabel l;
            l.name = QString::fromUtf8(r.name);
            l.lat = r.lat;
            l.lng = r.lng;
            l.world = geo_to_world(r.lat, r.lng);
            l.rank = r.rank;
            l.min_zoom = r.rank == 0 ? 0.0f : 3.0f;
            out.append(l);
        }
        return out;
    }();
    return kPoints;
}

} // namespace fincept::ui::map
