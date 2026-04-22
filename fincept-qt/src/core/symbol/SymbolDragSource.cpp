#include "core/symbol/SymbolDragSource.h"

#include "core/symbol/SymbolMime.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFont>
#include <QFontMetrics>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

namespace fincept::symbol_dnd {

static QPixmap make_drag_pixmap(const SymbolRef& ref) {
    // Amber pill with the ticker. Small enough not to block the drop target.
    QFont font;
    font.setBold(true);
    font.setPointSize(10);
    QFontMetrics fm(font);
    const QString text = ref.symbol;
    const int text_w = fm.horizontalAdvance(text);
    const int w = text_w + 20;
    const int h = fm.height() + 10;

    // High-DPI: draw at devicePixelRatio so the pill doesn't look blurry on
    // mixed-DPI setups.
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(static_cast<int>(w * dpr), static_cast<int>(h * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, w, h), h / 2.0, h / 2.0);
    p.fillPath(path, QColor("#d97706")); // amber
    p.setPen(Qt::white);
    p.setFont(font);
    p.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, text);
    p.end();
    return pm;
}

void startSymbolDrag(QWidget* source, const SymbolRef& ref, SymbolGroup source_group) {
    if (!source || !ref.is_valid())
        return;
    auto* drag = new QDrag(source);
    drag->setMimeData(symbol_mime::pack(ref, source_group));
    const QPixmap pm = make_drag_pixmap(ref);
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(pm.width() / (2 * static_cast<int>(pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1)),
                            pm.height() / (2 * static_cast<int>(pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1))));
    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

SymbolDropFilter::SymbolDropFilter(QWidget* target, Handler handler)
    : QObject(target), handler_(std::move(handler)) {}

bool SymbolDropFilter::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
        case QEvent::DragEnter: {
            auto* e = static_cast<QDragEnterEvent*>(event);
            if (symbol_mime::has_symbol(e->mimeData())) {
                e->acceptProposedAction();
                return true;
            }
            return false;
        }
        case QEvent::DragMove: {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (symbol_mime::has_symbol(e->mimeData())) {
                e->acceptProposedAction();
                return true;
            }
            return false;
        }
        case QEvent::Drop: {
            auto* e = static_cast<QDropEvent*>(event);
            if (!symbol_mime::has_symbol(e->mimeData()))
                return false;
            const SymbolRef ref = symbol_mime::unpack(e->mimeData());
            const SymbolGroup src = symbol_mime::unpack_source_group(e->mimeData());
            e->acceptProposedAction();
            if (ref.is_valid() && handler_)
                handler_(ref, src);
            return true;
        }
        default:
            return QObject::eventFilter(watched, event);
    }
}

SymbolDropFilter* installDropFilter(QWidget* target, SymbolDropFilter::Handler handler) {
    if (!target)
        return nullptr;
    target->setAcceptDrops(true);
    auto* f = new SymbolDropFilter(target, std::move(handler));
    target->installEventFilter(f);
    return f;
}

// ── SymbolDragFilter ─────────────────────────────────────────────────────────

SymbolDragFilter::SymbolDragFilter(QWidget* source, RefProvider provider, SymbolGroup source_group)
    : QObject(source), source_(source), provider_(std::move(provider)), source_group_(source_group) {}

bool SymbolDragFilter::eventFilter(QObject* /*watched*/, QEvent* event) {
    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                press_pos_ = me->pos();
                pressed_ = true;
            }
            return false;
        }
        case QEvent::MouseMove: {
            if (!pressed_)
                return false;
            auto* me = static_cast<QMouseEvent*>(event);
            if (!(me->buttons() & Qt::LeftButton)) {
                pressed_ = false;
                return false;
            }
            if ((me->pos() - press_pos_).manhattanLength() < QApplication::startDragDistance())
                return false;
            pressed_ = false;
            if (!provider_)
                return false;
            const SymbolRef ref = provider_();
            if (!ref.is_valid())
                return false;
            startSymbolDrag(source_, ref, source_group_);
            return true;
        }
        case QEvent::MouseButtonRelease:
            pressed_ = false;
            return false;
        default:
            return false;
    }
}

SymbolDragFilter* installDragSource(QWidget* source, SymbolDragFilter::RefProvider provider,
                                     SymbolGroup source_group) {
    if (!source)
        return nullptr;
    auto* f = new SymbolDragFilter(source, std::move(provider), source_group);
    source->installEventFilter(f);
    return f;
}

} // namespace fincept::symbol_dnd
