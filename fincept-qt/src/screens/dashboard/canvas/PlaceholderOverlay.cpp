#include "screens/dashboard/canvas/PlaceholderOverlay.h"

#include <QPainter>
#include <QPen>

namespace fincept::screens {

PlaceholderOverlay::PlaceholderOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlags(Qt::Widget);
    hide();
}

void PlaceholderOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    // Orange fill, very low alpha — matches react-grid-layout placeholder
    p.fillRect(rect(), QColor(217, 119, 6, 22)); // AMBER at ~9% opacity
    // Dashed orange border
    QPen pen(QColor(217, 119, 6, 180), 1, Qt::DashLine);
    p.setPen(pen);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

} // namespace fincept::screens
