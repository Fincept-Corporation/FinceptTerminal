#pragma once
// ReportChartCache — chart images of the Report Builder page view, drawn OFF the GUI thread.
//
// The page shows a chart as a text object of a fixed size (screens/report_builder/
// ReportChartObject); while its image is not ready the object paints a placeholder, the
// typing never waits. request() draws the chart (services/report_builder/ReportChartRenderer,
// QPainter into a QImage) on a two-thread pool — or loads it from the disk cache — and
// emits ready(key); the page then only repaints (the layout never changes: the size is
// fixed). Identical charts share one image (key = content hash + palette + size + scale).
//
// Disk cache: <profile cache>/report_charts/<key>.png, capped (oldest removed beyond
// 200 MB, trimmed once per session); memory: the most recently used images up to ~160 MB.
// png_for() serves exports on any thread (disk cache first).

#include "core/report/ReportChartSpec.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

class QThreadPool;

namespace fincept::services {

class ReportChartCache : public QObject {
    Q_OBJECT
  public:
    static ReportChartCache& instance();

    /// The image for `key` when it is in memory; null otherwise. GUI thread, never blocks.
    QImage image(const QString& key) const;
    /// Draw (or load) the chart on a worker; ready(key) once it is in memory. Idempotent.
    void request(const QString& key, const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                 const QString& font_family, const QSize& logical, qreal scale);

    /// <profile cache>/report_charts
    static QString cache_dir();
    /// Any thread: the chart as PNG bytes (disk cache first, else drawn and stored).
    static QByteArray png_for(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                              const QString& font_family, const QSize& logical, qreal scale,
                              QString* path_out = nullptr);
    /// Any thread: a sparkline (values as a line in the accent colour) as PNG bytes.
    static QByteArray sparkline_png(const QVector<double>& values, const report::chart::ChartPalette& pal,
                                    const QSize& logical, qreal scale, QString* path_out = nullptr);
    /// Any thread (call off the GUI thread): oldest cache files removed beyond `max_bytes`;
    /// the chart PNGs earlier builds left in the OS temp folder go too.
    static void trim(qint64 max_bytes = 200LL * 1024 * 1024);

  signals:
    void ready(const QString& key);

  private:
    ReportChartCache();
    Q_DISABLE_COPY(ReportChartCache)
    void remember(const QString& key, const QImage& img);

    QThreadPool* pool_ = nullptr;
    QHash<QString, QImage> memory_;
    QStringList lru_; ///< most recent last
    qint64 memory_bytes_ = 0;
    QSet<QString> pending_;
};

} // namespace fincept::services
