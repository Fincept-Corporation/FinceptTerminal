#include "ui/widgets/GeometricBackground.h"

#include "ui/theme/ThemeManager.h"

#include <QPainter>
#include <QPen>
#include <QResizeEvent>

namespace fincept::ui {

GeometricBackground::GeometricBackground(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    tokens_ = ThemeManager::instance().tokens();
    setStyleSheet(QString("background: %1;").arg(QString(tokens_.bg_base)));

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens& t) {
        tokens_ = t;
        setStyleSheet(QString("background: %1;").arg(QString(tokens_.bg_base)));
        cache_ = QPixmap(); // invalidate cache
        update();
    });
}

void GeometricBackground::rebuild_cache() {
    if (width() <= 0 || height() <= 0)
        return;

    cache_ = QPixmap(size());
    cache_.fill(QColor(tokens_.bg_base));

    QPainter p(&cache_);
    // No antialiasing on thin lines — cheaper and crisper
    p.setRenderHint(QPainter::Antialiasing, false);

    // Grid line color: border_bright at ~48% opacity
    QColor line_color(tokens_.border_bright);
    line_color.setAlpha(122);
    QPen pen(line_color);
    pen.setWidthF(0.8);
    p.setPen(pen);

    int spacing = 60;
    int w = width() + spacing;
    int h = height() + spacing;

    for (int x = -h; x < w; x += spacing)
        p.drawLine(x, 0, x + h, h);
    for (int x = 0; x < w + h; x += spacing)
        p.drawLine(x, 0, x - h, h);
}

void GeometricBackground::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuild_cache();
}

void GeometricBackground::paintEvent(QPaintEvent*) {
    if (cache_.isNull())
        rebuild_cache();
    if (cache_.isNull())
        return;

    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

} // namespace fincept::ui
