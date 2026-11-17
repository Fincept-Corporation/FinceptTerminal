// src/ui/widgets/map/ImageryTiles.h
//
// Optional online imagery for WorldMapWidget: NASA GIBS Web-Mercator tiles
// (public, no API key, no account). Everything here fails soft — the offline
// Natural Earth base map (GeoBaseMap.h) is always drawn underneath, and a source
// that cannot be reached is reported through state() so the map can say so.
//
//  • memory cache (64 MB) + disk cache (<cache>/map_imagery, 200 MB cap, files
//    older than 30 days are refreshed / pruned)
//  • at most 6 downloads in flight; queued and in-flight requests for tiles no
//    map wants any more (the user panned on) are dropped / aborted
//  • failed tiles retry with exponential back-off; after repeated failures the
//    source is marked offline and probed again with growing intervals
//  • disk reads, image decoding and cache writes run on worker threads
#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>

#include <deque>

class QNetworkAccessManager;
class QNetworkReply;

namespace fincept::ui::map {

struct ImagerySource {
    QString id;          // disk-cache folder name
    QString url;         // template: {z} {x} {y}
    int max_zoom = 8;    // deepest level the service publishes; deeper views upscale it
    QString attribution; // shown on the map while this source is on screen
};

/// The imagery the map offers, in a fixed order (index = source id below).
const QVector<ImagerySource>& imagery_sources();

class ImageryTiles : public QObject {
    Q_OBJECT
  public:
    enum class State { Idle, Loading, Ok, Offline };

    struct TileId {
        int z = 0;
        int x = 0;
        int y = 0;
    };

    static ImageryTiles& instance();

    /// Memory-cached tile or null. Never blocks, never requests.
    const QImage* cached(int source, int z, int x, int y) const;

    /// The tiles `owner` needs now, most important first. Replaces what that owner
    /// wanted before: queued requests nobody wants any more are dropped and their
    /// downloads aborted; missing tiles are looked up on disk, then downloaded.
    void want(const QObject* owner, int source, const QVector<TileId>& tiles);
    /// The owner no longer shows imagery (basemap switched, widget hidden/destroyed).
    void release(const QObject* owner);

    State state(int source) const;
    /// Short reason for the last failure of `source` (empty when none).
    QString last_error(int source) const;

  signals:
    void tile_ready();
    void state_changed();

  private:
    ImageryTiles();
    struct Request {
        int source = 0;
        TileId id;
        int attempts = 0;
        qint64 retry_at_ms = 0;
        bool disk_checked = false;
        bool disk_busy = false;
        QNetworkReply* reply = nullptr;
    };
    struct SourceHealth {
        State state = State::Idle;
        int consecutive_failures = 0;
        qint64 next_probe_ms = 0;
        int probe_step = 0;
        QString last_error;
    };

    static quint64 key_of(int source, int z, int x, int y);
    bool is_wanted(quint64 key) const;
    void pump();
    void start_disk_lookup(quint64 key, Request& r);
    void start_download(quint64 key, Request& r);
    void on_download_finished(quint64 key, QNetworkReply* reply);
    void on_tile_decoded(quint64 key, QImage image, bool from_disk, bool stale);
    void on_failure(quint64 key, const QString& reason, bool permanent);
    void insert_cached(quint64 key, QImage image);
    void schedule_pump(qint64 delay_ms);
    void set_state(int source, State s);
    QString cache_path(int source, int z, int x, int y) const;
    void prune_disk_cache_async();

    QNetworkAccessManager* nam_ = nullptr;
    QTimer pump_timer_;
    QString cache_root_;

    QHash<const QObject*, QSet<quint64>> wanted_;
    QHash<quint64, Request> requests_; // queued, on disk lookup or downloading
    std::deque<quint64> order_;        // request order, most important first
    QSet<quint64> missing_;            // outside the service's coverage — never asked again this session
    int in_flight_ = 0;
    int disk_in_flight_ = 0;
    int writes_since_prune_ = 0;

    QHash<quint64, QImage> memory_;
    std::deque<quint64> memory_order_;
    qint64 memory_bytes_ = 0;

    QVector<SourceHealth> health_;
};

} // namespace fincept::ui::map
