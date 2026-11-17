#pragma once
// ThemeIcon.h — small line icons for common chrome glyphs (close, refresh,
// settings, ...) drawn in the active theme's icon colours. Use instead of
// QStyle::standardIcon(), whose platform pixmaps ignore the theme:
//
//     btn->setIcon(ui::theme_icon(ui::ThemeGlyph::Refresh));
//
// The icon is painted when Qt asks for it, reading the colours at that moment,
// so it follows theme changes without the caller doing anything:
//   Normal → ICON_DIM, Active / Selected (hover, pressed) → ICON_HOVER,
//   Disabled → TEXT_TERTIARY.
// Header-only: no moc, no source-list entry.

#include "ui/theme/Theme.h"

#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>

namespace fincept::ui {

enum class ThemeGlyph {
    Close,
    Refresh,
    Settings,
    Plus,
    Minus,
    ChevronUp,
    ChevronDown,
    ChevronLeft,
    ChevronRight,
    Search,
    Menu,
    Detach,
    Check,
};

class ThemeIconEngine : public QIconEngine {
  public:
    explicit ThemeIconEngine(ThemeGlyph glyph) : glyph_(glyph) {}

    QIconEngine* clone() const override { return new ThemeIconEngine(glyph_); }
    QString key() const override { return QStringLiteral("fincept.theme_icon"); }

    /// The base implementation paints onto a pixmap it never clears, which
    /// leaves whatever was in memory around the glyph; start from transparent.
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pm;
    }

    void paint(QPainter* p, const QRect& rect, QIcon::Mode mode, QIcon::State) override {
        QColor color;
        switch (mode) {
            case QIcon::Active:
            case QIcon::Selected:
                color = QColor(colors::ICON_HOVER());
                break;
            case QIcon::Disabled:
                color = QColor(colors::TEXT_TERTIARY());
                break;
            case QIcon::Normal:
                color = QColor(colors::ICON_DIM());
                break;
        }

        // Glyphs are drawn in a 16 x 16 box, centred and scaled into `rect`.
        const qreal side = std::min(rect.width(), rect.height());
        if (side <= 0)
            return;
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        p->scale(side / 16.0, side / 16.0);
        QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);

        switch (glyph_) {
            case ThemeGlyph::Close:
                p->drawLine(QPointF(4, 4), QPointF(12, 12));
                p->drawLine(QPointF(12, 4), QPointF(4, 12));
                break;
            case ThemeGlyph::Refresh: {
                // A 300° arc with an arrowhead at its open end.
                const QRectF circle(3, 3, 10, 10);
                p->drawArc(circle, 60 * 16, 300 * 16);
                const qreal a = qDegreesToRadians(60.0);
                const QPointF tip(8 + 5 * std::cos(a), 8 - 5 * std::sin(a));
                QPainterPath head;
                head.moveTo(tip + QPointF(-3.2, -0.6));
                head.lineTo(tip);
                head.lineTo(tip + QPointF(0.4, -3.2));
                p->drawPath(head);
                break;
            }
            case ThemeGlyph::Settings: {
                // Gear: a ring with eight teeth around a hub.
                p->drawEllipse(QPointF(8, 8), 3.6, 3.6);
                p->drawEllipse(QPointF(8, 8), 1.3, 1.3);
                for (int i = 0; i < 8; ++i) {
                    const qreal a = qDegreesToRadians(i * 45.0);
                    p->drawLine(QPointF(8 + 4.4 * std::cos(a), 8 + 4.4 * std::sin(a)),
                                QPointF(8 + 6.4 * std::cos(a), 8 + 6.4 * std::sin(a)));
                }
                break;
            }
            case ThemeGlyph::Plus:
                p->drawLine(QPointF(8, 3.5), QPointF(8, 12.5));
                p->drawLine(QPointF(3.5, 8), QPointF(12.5, 8));
                break;
            case ThemeGlyph::Minus:
                p->drawLine(QPointF(3.5, 8), QPointF(12.5, 8));
                break;
            case ThemeGlyph::ChevronUp:
                p->drawPolyline(QPolygonF({QPointF(4, 10), QPointF(8, 6), QPointF(12, 10)}));
                break;
            case ThemeGlyph::ChevronDown:
                p->drawPolyline(QPolygonF({QPointF(4, 6), QPointF(8, 10), QPointF(12, 6)}));
                break;
            case ThemeGlyph::ChevronLeft:
                p->drawPolyline(QPolygonF({QPointF(10, 4), QPointF(6, 8), QPointF(10, 12)}));
                break;
            case ThemeGlyph::ChevronRight:
                p->drawPolyline(QPolygonF({QPointF(6, 4), QPointF(10, 8), QPointF(6, 12)}));
                break;
            case ThemeGlyph::Search:
                p->drawEllipse(QPointF(7, 7), 3.8, 3.8);
                p->drawLine(QPointF(9.8, 9.8), QPointF(13, 13));
                break;
            case ThemeGlyph::Menu:
                p->drawLine(QPointF(3, 5), QPointF(13, 5));
                p->drawLine(QPointF(3, 8), QPointF(13, 8));
                p->drawLine(QPointF(3, 11), QPointF(13, 11));
                break;
            case ThemeGlyph::Detach:
                p->drawRect(QRectF(3, 7, 6, 6));
                p->drawPolyline(QPolygonF({QPointF(9, 3), QPointF(13, 3), QPointF(13, 7)}));
                p->drawLine(QPointF(8, 8), QPointF(13, 3));
                break;
            case ThemeGlyph::Check:
                p->drawPolyline(QPolygonF({QPointF(3.5, 8.5), QPointF(6.5, 11.5), QPointF(12.5, 4.5)}));
                break;
        }
        p->restore();
    }

  private:
    ThemeGlyph glyph_;
};

/// A theme-coloured icon for `glyph` (see the header comment).
inline QIcon theme_icon(ThemeGlyph glyph) {
    return QIcon(new ThemeIconEngine(glyph));
}

} // namespace fincept::ui
