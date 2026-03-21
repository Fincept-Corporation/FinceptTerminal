#include "ui/widgets/GeometricBackground.h"
#include <QPainter>
#include <QPen>
#include <QResizeEvent>

namespace fincept::ui {

GeometricBackground::GeometricBackground(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: #000000;");
}

void GeometricBackground::rebuild_cache() {
    if (width() <= 0 || height() <= 0) return;

    cache_ = QPixmap(size());
    cache_.fill(QColor("#000000"));

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(QColor(51, 51, 51, 122));  // #333333 at ~48% opacity
    pen.setWidthF(0.8);
    p.setPen(pen);

    int spacing = 60;
    int w = width() + spacing;
    int h = height() + spacing;

    for (int x = -h; x < w; x += spacing) {
        p.drawLine(x, 0, x + h, h);
    }
    for (int x = 0; x < w + h; x += spacing) {
        p.drawLine(x, 0, x - h, h);
    }
}

void GeometricBackground::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuild_cache();
}

void GeometricBackground::paintEvent(QPaintEvent*) {
    if (cache_.isNull()) rebuild_cache();
    if (cache_.isNull()) return;

    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

} // namespace fincept::ui
