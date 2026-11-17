#include "core/components/PopularityTracker.h"

#include "core/config/AppSettings.h"

#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <atomic>
#include <tuple>

namespace fincept {

static constexpr const char* kGroup = "component_usage";
static constexpr int kFlushDelayMs = 2000; // quiet period before the batched write

namespace {

// Serialises the batched writes (pool thread) with each other, with the quit
// flush and with clear_all(). QSettings is reentrant and its file access is
// process-wide locked, so writing from a pool thread is safe; the mutex only
// keeps two batches from interleaving.
QMutex& write_mutex() {
    static QMutex m;
    return m;
}

// Bumped by clear_all(): a batch captured before the clear must not bring the
// counts back.
std::atomic<int> g_epoch{0};

void write_counts(const QHash<QString, int>& counts, int epoch) {
    QMutexLocker lock(&write_mutex());
    if (epoch != g_epoch.load())
        return;
    QSettings settings = app_settings::open();
    settings.beginGroup(QString::fromLatin1(kGroup));
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        // Counts only grow: never let a batch replace a larger stored value.
        if (settings.value(it.key(), 0).toInt() < it.value())
            settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
    settings.sync(); // QSettings writes the file via a temp file + rename (atomic)
}

} // namespace

PopularityTracker& PopularityTracker::instance() {
    static PopularityTracker s;
    return s;
}

PopularityTracker::PopularityTracker() {
    load();
    flush_timer_ = new QTimer(this);
    flush_timer_->setSingleShot(true);
    flush_timer_->setInterval(kFlushDelayMs);
    connect(flush_timer_, &QTimer::timeout, this, &PopularityTracker::flush_async);
    if (auto* app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &PopularityTracker::flush_now);
}

void PopularityTracker::load() {
    QSettings settings = app_settings::open();
    settings.beginGroup(QString::fromLatin1(kGroup));
    const QStringList keys = settings.childKeys();
    for (const QString& k : keys) {
        const int n = settings.value(k, 0).toInt();
        if (n > 0)
            counts_[k] = n;
    }
    settings.endGroup();
}

void PopularityTracker::increment(const QString& id) {
    if (id.isEmpty())
        return;
    const int next = counts_.value(id, 0) + 1;
    counts_[id] = next;
    pending_[id] = next;
    if (!flush_timer_->isActive())
        flush_timer_->start(); // not restarted per click: a burst is written ~2 s after its first click
    emit usage_changed(id, next);
}

void PopularityTracker::flush_async() {
    if (pending_.isEmpty())
        return;
    const QHash<QString, int> batch = pending_;
    pending_.clear();
    const int epoch = g_epoch.load();
    std::ignore = QtConcurrent::run([batch, epoch]() { write_counts(batch, epoch); });
}

void PopularityTracker::flush_now() {
    flush_timer_->stop();
    if (pending_.isEmpty())
        return;
    const QHash<QString, int> batch = pending_;
    pending_.clear();
    write_counts(batch, g_epoch.load());
}

int PopularityTracker::use_count(const QString& id) const {
    return counts_.value(id, 0);
}

void PopularityTracker::clear_all() {
    flush_timer_->stop();
    pending_.clear();
    counts_.clear();
    QMutexLocker lock(&write_mutex());
    ++g_epoch;
    QSettings settings = app_settings::open();
    settings.beginGroup(QString::fromLatin1(kGroup));
    settings.remove("");
    settings.endGroup();
}

} // namespace fincept
