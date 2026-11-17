#pragma once
// ReportChartObject — a Report Builder chart inside a page's QTextDocument.
//
// A chart is a text object of a FIXED size (the block's width x its aspect), registered on
// every section document. Its drawing depends on where the page goes:
//   Screen: the chart image comes from services::ReportChartCache, drawn off the GUI thread;
//           until it arrives a placeholder of the same size is painted, and when it arrives
//           only a repaint is needed (chart_ready) — the layout never moves.
//   Vector: the PDF export draws the chart with the same painter code straight into the PDF
//           (services/report_builder/ReportChartRenderer::paint) — sharp at any zoom, no bitmap.
// The handler is owned by the page view / the export job and outlives their documents.

#include "core/report/ReportChartSpec.h"

#include <QHash>
#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QTextCharFormat>
#include <QTextObjectInterface>

namespace fincept::screens {

class ReportChartObject : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
  public:
    enum class Mode { Screen, Vector };
    static constexpr int kObjectType = QTextFormat::UserObject + 61;
    static constexpr int kKeyProperty = QTextFormat::UserProperty + 61;
    static constexpr int kWidthProperty = QTextFormat::UserProperty + 62;
    static constexpr int kHeightProperty = QTextFormat::UserProperty + 63;
    /// Pixel density of the screen images (2x the 96-dpi page).
    static constexpr double kScreenScale = 2.0;

    explicit ReportChartObject(Mode mode, QObject* parent = nullptr);

    /// Register a chart drawn at `logical` px (96 dpi); returns its key. Screen mode asks the
    /// cache for its image at once (off the GUI thread).
    QString add(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                const QString& font_family, const QSize& logical);
    /// The character format that places chart `key` of `size` in a document.
    static QTextCharFormat object_format(const QString& key, const QSizeF& size);
    /// Install this handler on `doc` (idempotent per document).
    void install(QTextDocument* doc);

    QSizeF intrinsicSize(QTextDocument* doc, int pos, const QTextFormat& format) override;
    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int pos,
                    const QTextFormat& format) override;

  signals:
    /// Screen: a chart image arrived — repaint (no relayout needed).
    void chart_ready();

  private:
    struct Entry {
        report::chart::ChartSpec spec;
        report::chart::ChartPalette pal;
        QString family;
        QSize size;
    };
    Mode mode_;
    QHash<QString, Entry> entries_;
};

} // namespace fincept::screens
