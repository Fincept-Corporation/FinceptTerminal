// src/core/net/NetSpeedMeter.cpp
#include "core/net/NetSpeedMeter.h"

#include "core/logging/Logger.h"

#include <QDateTime>

#if defined(Q_OS_WIN)
// The project defines WIN32_LEAN_AND_MEAN globally, which strips winsock from
// <windows.h>. Pull the pieces we need explicitly in the right order so
// MIB_IF_ROW2 / GetIfTable2 (declared in netioapi.h) compile cleanly.
#  ifndef _WINSOCKAPI_
#    define _WINSOCKAPI_ // prevent windows.h from including winsock1
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <iphlpapi.h>
#  include <netioapi.h>
#  pragma comment(lib, "iphlpapi.lib")
#elif defined(Q_OS_MACOS)
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <net/if_dl.h>
#  include <sys/socket.h>
#  include <string.h>
#elif defined(Q_OS_LINUX)
#  include <QFile>
#  include <QRegularExpression>
#  include <QTextStream>
#endif

namespace fincept::net {

NetSpeedMeter::NetSpeedMeter(QObject* parent) : QObject(parent) {
    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &NetSpeedMeter::sample);
}

NetSpeedMeter::~NetSpeedMeter() = default;

void NetSpeedMeter::start(int interval_ms) {
    timer_->setInterval(interval_ms > 0 ? interval_ms : 1000);
    has_prev_ = false; // force fresh baseline on (re)start
    prev_rx_ = 0;
    prev_tx_ = 0;
    prev_tick_ms_ = 0;
    last_down_bps_ = 0;
    last_up_bps_ = 0;
    timer_->start();
    // Take the baseline sample immediately so the first delta is accurate.
    sample();
}

void NetSpeedMeter::stop() {
    if (timer_ && timer_->isActive())
        timer_->stop();
}

void NetSpeedMeter::sample() {
    quint64 rx = 0, tx = 0;
    if (!read_counters(rx, tx))
        return;

    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();

    if (!has_prev_) {
        prev_rx_ = rx;
        prev_tx_ = tx;
        prev_tick_ms_ = now_ms;
        has_prev_ = true;
        return;
    }

    const qint64 dt_ms = now_ms - prev_tick_ms_;
    if (dt_ms <= 0) {
        prev_rx_ = rx;
        prev_tx_ = tx;
        prev_tick_ms_ = now_ms;
        return;
    }

    // Guard against counter rollover / interface teardown — treat negative delta as 0.
    const quint64 drx = (rx >= prev_rx_) ? (rx - prev_rx_) : 0;
    const quint64 dtx = (tx >= prev_tx_) ? (tx - prev_tx_) : 0;

    last_down_bps_ = static_cast<qint64>((drx * 1000ULL) / static_cast<quint64>(dt_ms));
    last_up_bps_   = static_cast<qint64>((dtx * 1000ULL) / static_cast<quint64>(dt_ms));

    prev_rx_ = rx;
    prev_tx_ = tx;
    prev_tick_ms_ = now_ms;

    emit speed_changed(last_down_bps_, last_up_bps_);
}

QString NetSpeedMeter::format_bps(qint64 bps) {
    if (bps < 0) bps = 0;
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = 1024 * 1024;
    constexpr qint64 GB = 1024LL * 1024LL * 1024LL;
    if (bps >= GB) return QString::number(static_cast<double>(bps) / GB, 'f', 2) + " GB/s";
    if (bps >= MB) return QString::number(static_cast<double>(bps) / MB, 'f', 2) + " MB/s";
    if (bps >= KB) return QString::number(static_cast<double>(bps) / KB, 'f', 1) + " KB/s";
    return QString::number(bps) + " B/s";
}

// ─────────────────────────────────────────────────────────────────────────────
// Platform backends
// ─────────────────────────────────────────────────────────────────────────────

#if defined(Q_OS_WIN)

bool NetSpeedMeter::read_counters(quint64& rx_bytes, quint64& tx_bytes) const {
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table) {
        if (table) FreeMibTable(table);
        return false;
    }

    quint64 rx = 0, tx = 0;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        // Skip loopback and tunnel pseudo-interfaces.
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK ||
            row.Type == IF_TYPE_TUNNEL)
            continue;
        // Only count connected / operational interfaces — idle adapters still
        // report 0 deltas, but this avoids counting disabled virtual adapters.
        if (row.OperStatus != IfOperStatusUp)
            continue;

        rx += row.InOctets;
        tx += row.OutOctets;
    }

    FreeMibTable(table);
    rx_bytes = rx;
    tx_bytes = tx;
    return true;
}

#elif defined(Q_OS_MACOS)

bool NetSpeedMeter::read_counters(quint64& rx_bytes, quint64& tx_bytes) const {
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0 || !ifap)
        return false;

    quint64 rx = 0, tx = 0;
    for (struct ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK)
            continue;
        if (!ifa->ifa_name || strncmp(ifa->ifa_name, "lo", 2) == 0)
            continue;
        if (!(ifa->ifa_flags & IFF_UP))
            continue;

        const struct if_data* d = reinterpret_cast<struct if_data*>(ifa->ifa_data);
        if (!d)
            continue;
        rx += d->ifi_ibytes;
        tx += d->ifi_obytes;
    }

    freeifaddrs(ifap);
    rx_bytes = rx;
    tx_bytes = tx;
    return true;
}

#elif defined(Q_OS_LINUX)

bool NetSpeedMeter::read_counters(quint64& rx_bytes, quint64& tx_bytes) const {
    QFile f("/proc/net/dev");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream ts(&f);
    // First two lines are headers.
    ts.readLine();
    ts.readLine();

    quint64 rx = 0, tx = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QString iface = line.left(colon).trimmed();
        if (iface == QLatin1String("lo"))
            continue;

        const QStringList cols = line.mid(colon + 1).split(QRegularExpression("\\s+"),
                                                           Qt::SkipEmptyParts);
        // Layout: rx_bytes, rx_packets, ..., tx_bytes at index 8, ...
        if (cols.size() < 9)
            continue;
        bool ok1 = false, ok2 = false;
        const quint64 r = cols[0].toULongLong(&ok1);
        const quint64 t = cols[8].toULongLong(&ok2);
        if (!ok1 || !ok2)
            continue;
        rx += r;
        tx += t;
    }

    rx_bytes = rx;
    tx_bytes = tx;
    return true;
}

#else

bool NetSpeedMeter::read_counters(quint64&, quint64&) const {
    return false; // unsupported platform — meter silently stays at 0
}

#endif

} // namespace fincept::net
