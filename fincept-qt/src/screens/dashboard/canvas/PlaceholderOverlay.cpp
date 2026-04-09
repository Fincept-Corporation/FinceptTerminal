#include "screens/dashboard/canvas/PlaceholderOverlay.h"

#include "ui/theme/Theme.h"

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
    QColor accent(ui::colors::AMBER());
    // Accent fill at ~9% opacity
    accent.setAlpha(22);
    p.fillRect(rect(), accent);
    // Dashed accent border
    accent.setAlpha(180);
    QPen pen(accent, 1, Qt::DashLine);
    p.setPen(pen);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

} // namespace fincept::screens
