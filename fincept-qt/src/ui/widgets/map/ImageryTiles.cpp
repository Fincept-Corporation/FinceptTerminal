// src/ui/widgets/map/ImageryTiles.cpp
#include "ui/widgets/map/ImageryTiles.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QThreadPool>

#include <algorithm>
#include <atomic>
#include <chrono>

namespace fincept::ui::map {

// Names carry an imt_ prefix: unity builds merge this file with other widget
// sources and an anonymous namespace does not isolate them there.
namespace {

constexpr int kImtMaxInFlight = 6;                      // concurrent downloads while the source is healthy
constexpr int kImtMaxDiskLookups = 4;                   // concurrent disk reads
constexpr qint64 kImtMemoryBytes = 64LL * 1024 * 1024;  // decoded tiles kept in memory
constexpr qint64 kImtDiskCapBytes = 200LL * 1024 * 1024; // disk cache ceiling
constexpr qint64 kImtDiskTrimBytes = 160LL * 1024 * 1024; // pruned down to this
constexpr int kImtExpiryDays = 30;                      // older files are refreshed, then pruned
constexpr int kImtOfflineAfter = 3;                     // consecutive failures before "offline"
constexpr int kImtPruneEveryWrites = 300;

std::atomic<bool> g_imt_shutting_down{false};

QThreadPool* imt_pool() {
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setMaxThreadCount(2);
        p->setObjectName(QStringLiteral("map-imagery"));
        return p;
    }();
    return pool;
}

QString imt_user_agent() {
    // A stable, identifying User-Agent (name + version + contact), as public
    // tile services ask — never a browser impersonation.
    const QString ver = QCoreApplication::applicationVersion();
    return QStringLiteral("FinceptTerminal/%1 (+https://fincept.in)").arg(ver.isEmpty() ? QStringLiteral("4") : ver);
}

QImage imt_decode(const QByteArray& bytes) {
    QImage img = QImage::fromData(bytes);
    if (img.isNull())
        return img;
    // One fast format for drawing (JPEG decodes to RGB32, PNG to ARGB32).
    return img.hasAlphaChannel() ? img.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                                 : img.convertToFormat(QImage::Format_RGB32);
}

qint64 imt_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

const QVector<ImagerySource>& imagery_sources() {
    // NASA GIBS: open data, no key or account; NASA asks clients to acknowledge
    // GIBS, which the map does while the imagery is on screen.
    static const QVector<ImagerySource> kSources = [] {
        QVector<ImagerySource> s = {
            {QStringLiteral("gibs_bluemarble"),
             QStringLiteral("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/BlueMarble_ShadedRelief_Bathymetry/"
                            "default/GoogleMapsCompatible_Level8/{z}/{y}/{x}.jpeg"),
             8, QStringLiteral("Imagery: NASA GIBS / Blue Marble")},
            {QStringLiteral("gibs_blackmarble"),
             QStringLiteral("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/VIIRS_Black_Marble/default/2016-01-01/"
                            "GoogleMapsCompatible_Level8/{z}/{y}/{x}.png"),
             8, QStringLiteral("Imagery: NASA GIBS / VIIRS Black Marble 2016")},
        };
        // FINCEPT_MAP_IMAGERY_HOST=<host[:port]> sends imagery requests elsewhere —
        // a mirror, or an unreachable address to exercise the offline fallback.
        const QString host = qEnvironmentVariable("FINCEPT_MAP_IMAGERY_HOST").trimmed();
        if (!host.isEmpty())
            for (auto& src : s)
                src.url.replace(QStringLiteral("gibs.earthdata.nasa.gov"), host);
        return s;
    }();
    return kSources;
}

ImageryTiles& ImageryTiles::instance() {
    static ImageryTiles* inst = new ImageryTiles(); // parented to the app; stops at aboutToQuit
    return *inst;
}

ImageryTiles::ImageryTiles() : QObject(QCoreApplication::instance()) {
    nam_ = new QNetworkAccessManager(this);
    nam_->setTransferTimeout(std::chrono::seconds{15});
    cache_root_ = fincept::AppPaths::cache() + QStringLiteral("/map_imagery");
    health_.resize(imagery_sources().size());
    pump_timer_.setSingleShot(true);
    connect(&pump_timer_, &QTimer::timeout, this, &ImageryTiles::pump);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        g_imt_shutting_down = true;
        for (auto& r : requests_) {
            if (r.reply) {
                QNetworkReply* reply = std::exchange(r.reply, nullptr);
                reply->abort();
            }
        }
        requests_.clear();
        imt_pool()->waitForDone(3000);
    });
    prune_disk_cache_async();
}

quint64 ImageryTiles::key_of(int source, int z, int x, int y) {
    return (static_cast<quint64>(source & 0xf) << 60) | (static_cast<quint64>(z & 0x1f) << 50) |
           (static_cast<quint64>(x & 0x1ffffff) << 25) | static_cast<quint64>(y & 0x1ffffff);
}

const QImage* ImageryTiles::cached(int source, int z, int x, int y) const {
    auto it = memory_.constFind(key_of(source, z, x, y));
    return it == memory_.constEnd() ? nullptr : &it.value();
}

bool ImageryTiles::is_wanted(quint64 key) const {
    for (auto it = wanted_.constBegin(); it != wanted_.constEnd(); ++it)
        if (it.value().contains(key))
            return true;
    return false;
}

ImageryTiles::State ImageryTiles::state(int source) const {
    return source >= 0 && source < health_.size() ? health_[source].state : State::Idle;
}

QString ImageryTiles::last_error(int source) const {
    return source >= 0 && source < health_.size() ? health_[source].last_error : QString();
}

void ImageryTiles::want(const QObject* owner, int source, const QVector<TileId>& tiles) {
    if (source < 0 || source >= imagery_sources().size() || g_imt_shutting_down)
        return;
    QSet<quint64> keys;
    QVector<quint64> ordered;
    keys.reserve(tiles.size());
    ordered.reserve(tiles.size());
    for (const TileId& t : tiles) {
        const quint64 key = key_of(source, t.z, t.x, t.y);
        if (keys.contains(key))
            continue;
        keys.insert(key);
        ordered.append(key);
        if (missing_.contains(key) || requests_.contains(key))
            continue;
        if (memory_.contains(key))
            continue; // already shown; a stale copy from disk keeps its refresh request alive below
        Request r;
        r.source = source;
        r.id = t;
        requests_.insert(key, r);
    }
    wanted_[owner] = keys;

    // This owner's tiles first, in its order (centre of the view first), then
    // whatever other maps still want.
    std::deque<quint64> order;
    for (quint64 k : ordered)
        if (requests_.contains(k))
            order.push_back(k);
    for (quint64 k : order_)
        if (!keys.contains(k) && requests_.contains(k))
            order.push_back(k);
    order_.swap(order);

    // Anything nobody wants any more: drop it, abort its download.
    for (auto it = requests_.begin(); it != requests_.end();) {
        if (is_wanted(it.key()) || it->disk_busy) {
            ++it;
            continue;
        }
        if (it->reply) {
            QNetworkReply* reply = std::exchange(it->reply, nullptr);
            --in_flight_;
            reply->abort(); // finished() fires; the request is gone so it is ignored
        }
        it = requests_.erase(it);
    }
    pump();
}

void ImageryTiles::release(const QObject* owner) {
    if (!wanted_.remove(owner))
        return;
    want(owner, 0, {}); // re-run the drop pass with this owner wanting nothing
    wanted_.remove(owner);
}

void ImageryTiles::schedule_pump(qint64 delay_ms) {
    delay_ms = std::max<qint64>(delay_ms, 10);
    if (pump_timer_.isActive() && pump_timer_.remainingTime() <= delay_ms)
        return;
    pump_timer_.start(static_cast<int>(std::min<qint64>(delay_ms, 10 * 60 * 1000)));
}

void ImageryTiles::pump() {
    if (g_imt_shutting_down)
        return;
    const qint64 now = imt_now_ms();
    qint64 next_wake = 0;
    std::deque<quint64> keep;
    for (quint64 key : order_) {
        auto it = requests_.find(key);
        if (it == requests_.end())
            continue;
        keep.push_back(key);
        Request& r = it.value();
        if (r.disk_busy || r.reply)
            continue;
        if (!r.disk_checked) {
            if (disk_in_flight_ < kImtMaxDiskLookups)
                start_disk_lookup(key, r);
            continue;
        }
        if (r.retry_at_ms > now) {
            next_wake = next_wake == 0 ? r.retry_at_ms : std::min(next_wake, r.retry_at_ms);
            continue;
        }
        SourceHealth& h = health_[r.source];
        const bool offline = h.state == State::Offline;
        if (offline && now < h.next_probe_ms) {
            next_wake = next_wake == 0 ? h.next_probe_ms : std::min(next_wake, h.next_probe_ms);
            continue;
        }
        if (in_flight_ >= (offline ? 1 : kImtMaxInFlight))
            continue;
        start_download(key, r);
    }
    order_.swap(keep);
    if (next_wake > 0)
        schedule_pump(next_wake - now);
}

QString ImageryTiles::cache_path(int source, int z, int x, int y) const {
    const QString ext = imagery_sources()[source].url.section(QLatin1Char('.'), -1);
    return QStringLiteral("%1/%2/%3/%4/%5.%6")
        .arg(cache_root_, imagery_sources()[source].id)
        .arg(z)
        .arg(x)
        .arg(y)
        .arg(ext);
}

void ImageryTiles::start_disk_lookup(quint64 key, Request& r) {
    r.disk_busy = true;
    ++disk_in_flight_;
    const QString path = cache_path(r.source, r.id.z, r.id.x, r.id.y);
    QPointer<ImageryTiles> self(this);
    imt_pool()->start([self, key, path]() {
        QImage img;
        bool stale = false;
        const QFileInfo fi(path);
        if (fi.exists()) {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
                img = imt_decode(f.readAll());
            stale = fi.lastModified().daysTo(QDateTime::currentDateTime()) > kImtExpiryDays;
        }
        if (g_imt_shutting_down)
            return;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, key, img = std::move(img), stale]() mutable {
                if (!self)
                    return;
                --self->disk_in_flight_;
                auto it = self->requests_.find(key);
                if (it != self->requests_.end()) {
                    it->disk_busy = false;
                    it->disk_checked = true;
                }
                if (!img.isNull()) {
                    self->insert_cached(key, std::move(img));
                    emit self->tile_ready();
                    // A fresh copy is the answer; a stale one is shown while it is
                    // re-downloaded (if the tile is still wanted).
                    if (!stale || !self->is_wanted(key))
                        self->requests_.remove(key);
                } else if (it != self->requests_.end() && !self->is_wanted(key)) {
                    self->requests_.erase(it);
                }
                self->pump();
            },
            Qt::QueuedConnection);
    });
}

void ImageryTiles::start_download(quint64 key, Request& r) {
    const ImagerySource& src = imagery_sources()[r.source];
    QString url = src.url;
    url.replace(QStringLiteral("{z}"), QString::number(r.id.z));
    url.replace(QStringLiteral("{x}"), QString::number(r.id.x));
    url.replace(QStringLiteral("{y}"), QString::number(r.id.y));
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, imt_user_agent());
    req.setRawHeader("Accept", "image/png,image/jpeg,image/*;q=0.8");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    QNetworkReply* reply = nam_->get(req);
    r.reply = reply;
    ++in_flight_;
    SourceHealth& h = health_[r.source];
    if (h.state == State::Idle)
        set_state(r.source, State::Loading);
    connect(reply, &QNetworkReply::finished, this, [this, key, reply]() { on_download_finished(key, reply); });
}

void ImageryTiles::on_download_finished(quint64 key, QNetworkReply* reply) {
    reply->deleteLater();
    auto it = requests_.find(key);
    if (it == requests_.end() || it->reply != reply)
        return; // aborted because nobody wants it any more
    it->reply = nullptr;
    --in_flight_;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && status == 200) {
        const QByteArray bytes = reply->readAll();
        const int source = it->source;
        const QString path = cache_path(source, it->id.z, it->id.x, it->id.y);
        it->disk_busy = true; // decoding + writing: pump() must not download it again meanwhile
        QPointer<ImageryTiles> self(this);
        imt_pool()->start([self, key, bytes, path]() {
            QImage img = imt_decode(bytes);
            if (!img.isNull()) {
                QDir().mkpath(QFileInfo(path).absolutePath());
                QSaveFile out(path);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(bytes);
                    out.commit();
                }
            }
            if (g_imt_shutting_down)
                return;
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, key, img = std::move(img)]() mutable {
                    if (self)
                        self->on_tile_decoded(key, std::move(img), false, false);
                },
                Qt::QueuedConnection);
        });
        return;
    }
    // 400 / 404: the service has no tile there (outside its coverage) — not a fault.
    if (status == 400 || status == 404) {
        on_failure(key, QString(), true);
        return;
    }
    const QString reason = status > 0 ? QStringLiteral("HTTP %1").arg(status)
                           : reply->error() == QNetworkReply::OperationCanceledError ? tr("timed out")
                                                                                       : reply->errorString();
    on_failure(key, reason, false);
}

void ImageryTiles::on_tile_decoded(quint64 key, QImage image, bool /*from_disk*/, bool /*stale*/) {
    auto it = requests_.find(key);
    const int source = it != requests_.end() ? it->source : static_cast<int>(key >> 60);
    if (it != requests_.end())
        it->disk_busy = false;
    if (image.isNull()) {
        on_failure(key, tr("unreadable image"), false);
        return;
    }
    insert_cached(key, std::move(image));
    requests_.remove(key);
    SourceHealth& h = health_[source];
    h.consecutive_failures = 0;
    h.probe_step = 0;
    h.last_error.clear();
    if (h.state != State::Ok)
        set_state(source, State::Ok);
    emit tile_ready();
    if (++writes_since_prune_ >= kImtPruneEveryWrites) {
        writes_since_prune_ = 0;
        prune_disk_cache_async();
    }
    pump();
}

void ImageryTiles::on_failure(quint64 key, const QString& reason, bool permanent) {
    auto it = requests_.find(key);
    if (it == requests_.end())
        return;
    if (permanent) {
        missing_.insert(key);
        requests_.erase(it);
        pump();
        return;
    }
    const int source = it->source;
    SourceHealth& h = health_[source];
    const qint64 now = imt_now_ms();
    it->attempts += 1;
    // 2 s, 4 s, 8 s … capped at 2 min, with jitter so retries do not line up.
    const qint64 backoff = std::min<qint64>(120000, 2000LL << std::min(it->attempts - 1, 6));
    it->retry_at_ms = now + backoff + QRandomGenerator::global()->bounded(500);
    h.consecutive_failures += 1;
    h.last_error = reason;
    if (h.consecutive_failures >= kImtOfflineAfter) {
        if (h.state != State::Offline) {
            LOG_WARN("Map", QString("Imagery source %1 unreachable (%2) — showing the offline base map")
                                 .arg(imagery_sources()[source].id, reason));
            h.probe_step = 0;
        }
        // Probe again after 15 s, 30 s, 1 min … capped at 5 min.
        h.next_probe_ms = now + std::min<qint64>(300000, 15000LL << std::min(h.probe_step, 5));
        h.probe_step += 1;
        set_state(source, State::Offline);
    }
    if (!is_wanted(key))
        requests_.erase(it);
    pump();
}

void ImageryTiles::insert_cached(quint64 key, QImage image) {
    auto existing = memory_.find(key);
    if (existing != memory_.end()) {
        memory_bytes_ -= existing->sizeInBytes();
        memory_.erase(existing);
    } else {
        memory_order_.push_back(key);
    }
    memory_bytes_ += image.sizeInBytes();
    memory_.insert(key, std::move(image));
    while (memory_bytes_ > kImtMemoryBytes && !memory_order_.empty()) {
        const quint64 old = memory_order_.front();
        memory_order_.pop_front();
        auto it = memory_.find(old);
        if (it != memory_.end()) {
            memory_bytes_ -= it->sizeInBytes();
            memory_.erase(it);
        }
    }
}

void ImageryTiles::set_state(int source, State s) {
    if (health_[source].state == s)
        return;
    health_[source].state = s;
    emit state_changed();
}

void ImageryTiles::prune_disk_cache_async() {
    const QString root = cache_root_;
    // The QGeoView tile cache this map used to fill (up to 100 MB, including the
    // "API key required" placeholders CARTO now serves) is no longer read.
    const QString legacy = fincept::AppPaths::cache() + QStringLiteral("/map_tiles");
    imt_pool()->start([root, legacy]() {
        if (QDir(legacy).exists())
            QDir(legacy).removeRecursively();
        struct Entry {
            QString path;
            qint64 size;
            QDateTime mtime;
        };
        QVector<Entry> files;
        qint64 total = 0;
        const QDateTime expiry = QDateTime::currentDateTime().addDays(-kImtExpiryDays * 2);
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && !g_imt_shutting_down) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (fi.lastModified() < expiry) { // long expired (refresh never came): drop
                QFile::remove(fi.absoluteFilePath());
                continue;
            }
            files.append({fi.absoluteFilePath(), fi.size(), fi.lastModified()});
            total += fi.size();
        }
        if (total <= kImtDiskCapBytes)
            return;
        std::sort(files.begin(), files.end(), [](const Entry& a, const Entry& b) { return a.mtime < b.mtime; });
        for (const Entry& e : files) {
            if (total <= kImtDiskTrimBytes || g_imt_shutting_down)
                break;
            if (QFile::remove(e.path))
                total -= e.size;
        }
    });
}

} // namespace fincept::ui::map
