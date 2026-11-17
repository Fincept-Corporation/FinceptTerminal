// ReportChartObject.cpp — see ReportChartObject.h.
//
// Colours painted here are DOCUMENT ink (the placeholder sits on the report's paper).

#include "screens/report_builder/ReportChartObject.h"

#include "services/report_builder/ReportChartCache.h"
#include "services/report_builder/ReportChartRenderer.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QPainter>
#include <QTextDocument>

namespace fincept::screens {

using Cache = ::fincept::services::ReportChartCache;

ReportChartObject::ReportChartObject(Mode mode, QObject* parent) : QObject(parent), mode_(mode) {
    if (mode_ == Mode::Screen) {
        connect(&Cache::instance(), &Cache::ready, this, [this](const QString& key) {
            if (entries_.contains(key))
                emit chart_ready();
        });
    }
}

QString ReportChartObject::add(const report::chart::ChartSpec& spec, const report::chart::ChartPalette& pal,
                               const QString& font_family, const QSize& logical) {
    const double scale = mode_ == Mode::Screen ? kScreenScale : 1.0;
    const QString key = services::report_chart::cache_key(spec, pal, font_family, logical, scale);
    if (!entries_.contains(key))
        entries_.insert(key, Entry{spec, pal, font_family, logical});
    if (mode_ == Mode::Screen && Cache::instance().image(key).isNull())
        Cache::instance().request(key, spec, pal, font_family, logical, scale);
    return key;
}

QTextCharFormat ReportChartObject::object_format(const QString& key, const QSizeF& size) {
    QTextCharFormat f;
    f.setObjectType(kObjectType);
    f.setProperty(kKeyProperty, key);
    f.setProperty(kWidthProperty, size.width());
    f.setProperty(kHeightProperty, size.height());
    f.setVerticalAlignment(QTextCharFormat::AlignBottom);
    return f;
}

void ReportChartObject::install(QTextDocument* doc) {
    if (doc && doc->documentLayout())
        doc->documentLayout()->registerHandler(kObjectType, this);
}

QSizeF ReportChartObject::intrinsicSize(QTextDocument*, int, const QTextFormat& format) {
    return QSizeF(format.doubleProperty(kWidthProperty), format.doubleProperty(kHeightProperty));
}

void ReportChartObject::drawObject(QPainter* painter, const QRectF& rect, QTextDocument*, int,
                                   const QTextFormat& format) {
    const QString key = format.stringProperty(kKeyProperty);
    const auto it = entries_.constFind(key);
    if (it == entries_.constEnd())
        return;
    if (mode_ == Mode::Vector) {
        services::report_chart::paint(painter, rect, it->spec, it->pal, it->family);
        return;
    }
    const QImage img = Cache::instance().image(key);
    if (!img.isNull()) {
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(rect, img);
        painter->restore();
        return;
    }
    // Placeholder of the final size, on the report's paper, until the image arrives.
    painter->save();
    const QColor paper(it->pal.paper);
    QColor frame(it->pal.grid);
    frame.setAlphaF(0.8f);
    painter->fillRect(rect, paper);
    painter->setPen(QPen(frame, 1, Qt::DashLine));
    painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    QFont f;
    f.setFamilies({it->family, QStringLiteral("Arial")});
    f.setPixelSize(12);
    painter->setFont(f);
    painter->setPen(QColor(it->pal.muted));
    const QString title = it->spec.title.isEmpty() ? tr("Chart") : it->spec.title;
    painter->drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, tr("%1\nDrawing chart…").arg(title));
    painter->restore();
    // The request may have been dropped from memory since: ask again (idempotent).
    Cache::instance().request(key, it->spec, it->pal, it->family, it->size, kScreenScale);
}

} // namespace fincept::screens
