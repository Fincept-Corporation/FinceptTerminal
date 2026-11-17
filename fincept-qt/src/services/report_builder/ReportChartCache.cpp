// ReportChartCache.cpp — see ReportChartCache.h.

#include "services/report_builder/ReportChartCache.h"

#include "core/config/AppPaths.h"
#include "services/report_builder/ReportChartRenderer.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>

#include <algorithm>
#include <cmath>

namespace fincept::services {

namespace {

constexpr qint64 kRccMemoryCap = 160LL * 1024 * 1024;

QString rcc_path(const QString& key) {
    return ReportChartCache::cache_dir() + QLatin1Char('/') + key + QStringLiteral(".png");
}

QByteArray rcc_encode(const QImage& img) {
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}

void rcc_store(const QString& path, const QByteArray& bytes) {
    if (bytes.isEmpty())
        return;
    QDir().mkpath(ReportChartCache::cache_dir());
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size())
        f.commit();
    else
        f.cancelWriting();
}

} // namespace

ReportChartCache& ReportChartCache::instance() {
    static ReportChartCache* s = [] {
        auto* c = new ReportChartCache();
        if (auto* app = QCoreApplication::instance(); app && c->thread() != app->thread())
            c->moveToThread(app->thread());
        return c;
    }();
    return *s;
}

ReportChartCache::ReportChartCache() {
    pool_ = new QThreadPool(this);
    pool_->setMaxThreadCount(2); // charts never take the whole machine
    pool_->setExpiryTimeout(30'000);
    if (auto* app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() { pool_->clear(); });
}

QString ReportChartCache::cache_dir() {
    return AppPaths::cache() + QStringLiteral("/report_charts");
}

QImage ReportChartCache::image(const QString& key) const {
    return memory_.value(key);
}

void ReportChartCache::remember(const QString& key, const QImage& img) {
    if (img.isNull())
        return;
    if (memory_.contains(key)) {
        lru_.removeAll(key);
        lru_.append(key);
        return;
    }
    memory_.insert(key, img);
    lru_.append(key);
    memory_bytes_ += img.sizeInBytes();
    while (memory_bytes_ > kRccMemoryCap && lru_.size() > 1) {
        const QString old = lru_.takeFirst();
        memory_bytes_ -= memory_.value(old).sizeInBytes();
        memory_.remove(old);
    }
}

void ReportChartCache::request(const QString& key, const report::chart::ChartSpec& spec,
                               const report::chart::ChartPalette& pal, const QString& font_family,
                               const QSize& logical, qreal scale) {
    if (key.isEmpty() || memory_.contains(key) || pending_.contains(key))
        return;
    pending_.insert(key);
    const QPointer<ReportChartCache> self(this);
    pool_->start([self, key, spec, pal, font_family, logical, scale]() {
        QImage img;
        const QString path = rcc_path(key);
        if (QFileInfo(path).size() > 0)
            img.load(path, "PNG");
        if (img.isNull()) {
            img = report_chart::render_image(spec, pal, font_family, logical, scale);
            rcc_store(path, rcc_encode(img));
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, key, img]() {
                if (!self)
                    return;
                self->pending_.remove(key);
                self->remember(key, img);
                emit self->ready(key);
            },
            Qt::QueuedConnection);
    });
}

QByteArray ReportChartCache::png_for(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                                     const QString& font_family, const QSize& logical, qreal scale, QString* path_out) {
    const QString key = report_chart::cache_key(spec, pal, font_family, logical, scale);
    const QString path = rcc_path(key);
    if (path_out)
        *path_out = path;
    QFile f(path);
    if (f.size() > 0 && f.open(QIODevice::ReadOnly))
        return f.readAll();
    const QByteArray bytes = report_chart::render_png(spec, pal, font_family, logical, scale);
    rcc_store(path, bytes);
    return bytes;
}

QByteArray ReportChartCache::sparkline_png(const QVector<double>& values, const report::chart::ChartPalette& pal,
                                           const QSize& logical, qreal scale, QString* path_out) {
    QStringList parts{QStringLiteral("spark4"), pal.paper, pal.accent, QString::number(logical.width()),
                      QString::number(logical.height()), QString::number(scale)};
    for (double v : values)
        parts << QString::number(v, 'g', 12);
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(parts.join(QChar(0x1F)).toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString path = rcc_path(key);
    if (path_out)
        *path_out = path;
    QFile f(path);
    if (f.size() > 0 && f.open(QIODevice::ReadOnly))
        return f.readAll();
    QVector<double> v;
    for (double x : values)
        if (std::isfinite(x))
            v << x;
    if (v.size() < 2 || logical.isEmpty())
        return {};
    QImage img(qRound(logical.width() * scale), qRound(logical.height() * scale), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(pal.paper));
    const auto [mn, mx] = std::minmax_element(v.begin(), v.end());
    const double lo = *mn, hi = *mx, span = hi > lo ? hi - lo : 1.0;
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(scale, scale);
        QPen pen{QColor(pal.accent)};
        pen.setWidthF(1.5);
        p.setPen(pen);
        QPolygonF poly;
        const double w = logical.width(), h = logical.height();
        for (int i = 0; i < v.size(); ++i)
            poly << QPointF(1 + i * (w - 2.0) / (v.size() - 1), h - 1.5 - (v[i] - lo) / span * (h - 3.0));
        p.drawPolyline(poly);
    }
    const QByteArray bytes = rcc_encode(img);
    rcc_store(path, bytes);
    return bytes;
}

void ReportChartCache::trim(qint64 max_bytes) {
    // Earlier builds wrote charts into the OS temp folder as fincept_chart_*.png /
    // fincept_spark_*.png. Those are NOT swept here any more: the separate private
    // build of Fincept Terminal writes the same names there, so a file of that
    // pattern cannot be told to be ours, and one in use by the other build would
    // vanish from its open report. The OS temp cleanup removes old ones.
    struct Entry {
        QString path;
        qint64 size;
        QDateTime when;
    };
    QVector<Entry> files;
    qint64 total = 0;
    QDirIterator it(cache_dir(), {QStringLiteral("*.png")}, QDir::Files);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        files.append({fi.absoluteFilePath(), fi.size(), fi.lastModified()});
        total += fi.size();
    }
    if (total <= max_bytes)
        return;
    std::sort(files.begin(), files.end(), [](const Entry& a, const Entry& b) { return a.when < b.when; });
    for (const Entry& e : files) {
        if (total <= max_bytes * 3 / 4)
            break;
        if (QFile::remove(e.path))
            total -= e.size;
    }
}

} // namespace fincept::services
