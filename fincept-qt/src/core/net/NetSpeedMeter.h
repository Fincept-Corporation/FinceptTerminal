// src/core/net/NetSpeedMeter.h
//
// System-wide network throughput sampler.
//
// Samples OS interface counters once per second and emits bytes/sec deltas.
// Intended as a "liveness" indicator during long-running downloads (e.g. the
// first-run Python setup) — shows total machine network activity, not traffic
// from a specific process.
//
// Platform backends:
//   Windows : GetIfTable2 (Iphlpapi) — sums InOctets / OutOctets over all
//             non-loopback interfaces.
//   macOS   : getifaddrs + AF_LINK if_data — sums ifi_ibytes / ifi_obytes.
//   Linux   : /proc/net/dev — sums rx_bytes / tx_bytes (excludes `lo`).
//
// Thread-safety: single-threaded; sample on the owning QObject's thread.
#pragma once

#include <QObject>
#include <QTimer>
#include <cstdint>

namespace fincept::net {

class NetSpeedMeter : public QObject {
    Q_OBJECT
  public:
    explicit NetSpeedMeter(QObject* parent = nullptr);
    ~NetSpeedMeter() override;

    /// Start periodic sampling. Default 1-second cadence.
    void start(int interval_ms = 1000);

    /// Stop sampling (safe to call if not running).
    void stop();

    /// Last-measured down/up bytes per second (0 until first delta).
    qint64 down_bps() const { return last_down_bps_; }
    qint64 up_bps() const { return last_up_bps_; }

    /// Format a bytes/sec value as human-readable string, e.g. "3.2 MB/s".
    static QString format_bps(qint64 bytes_per_sec);

  signals:
    /// Fired on every sample after the first (need two samples to compute delta).
    void speed_changed(qint64 down_bps, qint64 up_bps);

  private:
    void sample();
    // Platform-specific cumulative counters; returns false on failure.
    bool read_counters(quint64& rx_bytes, quint64& tx_bytes) const;

    QTimer* timer_ = nullptr;
    bool has_prev_ = false;
    quint64 prev_rx_ = 0;
    quint64 prev_tx_ = 0;
    qint64 prev_tick_ms_ = 0;
    qint64 last_down_bps_ = 0;
    qint64 last_up_bps_ = 0;
};

} // namespace fincept::net
