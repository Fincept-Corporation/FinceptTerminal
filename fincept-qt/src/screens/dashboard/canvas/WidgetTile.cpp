#include "screens/dashboard/canvas/WidgetTile.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QPen>
#include <QVBoxLayout>

namespace fincept::screens {

// ── ResizeGrip ────────────────────────────────────────────────────────────────

ResizeGrip::ResizeGrip(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::SizeFDiagCursor);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    // Ensure this widget stays on top of siblings
    raise();
}

void ResizeGrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (hovered_ || pressed_) {
        QColor dot_color(ui::colors::AMBER());
        dot_color.setAlpha(pressed_ ? 255 : 160);
        const int dot = 2, gap = 3;
        const int ox = width() - 14;
        const int oy = height() - 14;
        p.setPen(Qt::NoPen);
        p.setBrush(dot_color);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                p.drawRect(ox + c * (dot + gap), oy + r * (dot + gap), dot, dot);
    }
}

void ResizeGrip::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        pressed_ = true;
        grabMouse(Qt::SizeFDiagCursor);
        emit grip_pressed(e->globalPosition().toPoint());
        update();
    }
}

void ResizeGrip::mouseMoveEvent(QMouseEvent* e) {
    if (pressed_)
        emit grip_moved(e->globalPosition().toPoint());
}

void ResizeGrip::mouseReleaseEvent(QMouseEvent* e) {
    if (pressed_) {
        pressed_ = false;
        releaseMouse();
        emit grip_released(e->globalPosition().toPoint());
        update();
    }
}

void ResizeGrip::enterEvent(QEnterEvent*) {
    hovered_ = true;
    update();
}

void ResizeGrip::leaveEvent(QEvent*) {
    hovered_ = false;
    update();
}

// ── WidgetTile ────────────────────────────────────────────────────────────────

WidgetTile::WidgetTile(const QString& instance_id, QWidget* parent) : QWidget(parent), instance_id_(instance_id) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });

    setMouseTracking(true); // track mouse for edge-resize cursor updates

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
}

void WidgetTile::set_content(widgets::BaseWidget* content) {
    if (!content || content_)
        return;
    content_ = content;
    // The factory created the content with this tile as its parent, so adding it
    // to the layout does not re-parent (and so does not re-style) it.
    if (content_->parentWidget() != this)
        content_->setParent(this);
    layout()->addWidget(content_);

    // Resize grip — transparent overlay in bottom-right corner, always on top
    grip_ = new ResizeGrip(this);
    grip_->setFixedSize(kGripSize, kGripSize);
    position_grip();

    // Wire grip signals to tile resize signals.
    // Grip now emits global screen coordinates (immune to tile repositioning).
    // Convert global → canvas (parent) coords for the canvas handler.
    connect(grip_, &ResizeGrip::grip_pressed, this, [this](QPoint global_pos) {
        set_resizing(true);
        QWidget* canvas = parentWidget();
        QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
        emit resize_started(this, canvas_pos);
    });
    connect(grip_, &ResizeGrip::grip_moved, this, [this](QPoint global_pos) {
        QWidget* canvas = parentWidget();
        QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
        emit resize_moved(this, canvas_pos);
    });
    connect(grip_, &ResizeGrip::grip_released, this, [this](QPoint global_pos) {
        Q_UNUSED(global_pos);
        set_resizing(false);
        emit resize_released(this);
    });

    // Setup drag from title bar
    setup_drag_handle();

    // Forward close/refresh
    connect(content_, &widgets::BaseWidget::close_requested, this, [this]() { emit close_requested(this); });
    connect(content_, &widgets::BaseWidget::refresh_requested, this, [this]() { emit refresh_requested(this); });
}

void WidgetTile::setup_drag_handle() {
    // The title bar is the drag handle — install event filter on it
    QWidget* title_bar = content_->drag_handle();
    if (!title_bar)
        return;

    title_bar->setCursor(Qt::SizeAllCursor);
    title_bar->setToolTip(tr("Drag to move · double-click to maximise / restore · right-click for more"));
    title_bar->installEventFilter(this);
    // Also filter children of title bar (labels, buttons on title bar)
    for (auto* child : title_bar->findChildren<QWidget*>()) {
        // Don't intercept actual buttons (close, refresh)
        if (qobject_cast<QPushButton*>(child))
            continue;
        child->installEventFilter(this);
    }
}

void WidgetTile::set_dragging(bool d) {
    dragging_ = d;
    update();
}

void WidgetTile::set_resizing(bool r) {
    resizing_ = r;
    update();
}

void WidgetTile::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    position_grip();
}

void WidgetTile::position_grip() {
    if (!grip_)
        return;
    grip_->move(width() - kGripSize, height() - kGripSize);
    grip_->raise(); // always on top
}

void WidgetTile::paintEvent(QPaintEvent* e) {
    QWidget::paintEvent(e);
    if (!dragging_ && !resizing_)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Accent glow border using theme accent color
    QColor ac(ui::colors::AMBER());
    p.setPen(QPen(ac, 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    ac.setAlpha(100);
    p.setPen(QPen(ac, 2));
    p.drawRect(rect().adjusted(-1, -1, 1, 1));
    ac.setAlpha(40);
    p.setPen(QPen(ac, 3));
    p.drawRect(rect().adjusted(-2, -2, 2, 2));
}

bool WidgetTile::is_on_resize_edge(const QPoint& pos) const {
    bool on_right = pos.x() >= width() - kEdgeZone && pos.y() > kGripSize;
    bool on_bottom = pos.y() >= height() - kEdgeZone && pos.x() > kGripSize;
    return on_right || on_bottom;
}

void WidgetTile::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && is_on_resize_edge(e->pos())) {
        edge_resizing_ = true;
        grabMouse(Qt::SizeFDiagCursor);
        set_resizing(true);
        QWidget* canvas = parentWidget();
        QPoint global_pos = e->globalPosition().toPoint();
        QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
        emit resize_started(this, canvas_pos);
        return;
    }
    QWidget::mousePressEvent(e);
}

void WidgetTile::mouseMoveEvent(QMouseEvent* e) {
    // After grabMouse() in the drag eventFilter, mouse events come here
    // instead of back through the eventFilter. Forward them to drag logic.
    if (drag_active_) {
        QWidget* canvas = parentWidget();
        QPoint global_pos = e->globalPosition().toPoint();
        QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
        emit drag_moved(this, canvas_pos);
        return;
    }
    if (edge_resizing_) {
        QWidget* canvas = parentWidget();
        QPoint global_pos = e->globalPosition().toPoint();
        QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
        emit resize_moved(this, canvas_pos);
        return;
    }
    // Update cursor when hovering near edges
    if (!dragging_ && !resizing_ && is_on_resize_edge(e->pos())) {
        bool on_right = e->pos().x() >= width() - kEdgeZone;
        bool on_bottom = e->pos().y() >= height() - kEdgeZone;
        if (on_right && on_bottom)
            setCursor(Qt::SizeFDiagCursor);
        else if (on_right)
            setCursor(Qt::SizeHorCursor);
        else
            setCursor(Qt::SizeVerCursor);
    } else if (!dragging_ && !resizing_) {
        unsetCursor();
    }
    QWidget::mouseMoveEvent(e);
}

void WidgetTile::mouseReleaseEvent(QMouseEvent* e) {
    // After grabMouse() in the drag eventFilter, release comes here too
    if (drag_active_) {
        QPoint global_pos = e->globalPosition().toPoint();
        drag_active_ = false;
        releaseMouse();
        setCursor(Qt::ArrowCursor);
        set_dragging(false);
        QWidget* canvas = parentWidget();
        QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
        emit drag_released(this, canvas_pos);
        return;
    }
    if (edge_resizing_) {
        edge_resizing_ = false;
        releaseMouse();
        unsetCursor();
        set_resizing(false);
        emit resize_released(this);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

} // namespace fincept::screens

// ── Event filter for title bar drag ───────────────────────────────────────────
// Defined outside the class body but still in the .cpp, after the namespace

void fincept::screens::WidgetTile::show_title_menu(const QPoint& global_pos) {
    // Heap + popup(): no nested event loop around a tile that may be destroyed.
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(maximized_ ? tr("Restore") : tr("Maximise")), &QAction::triggered, this,
            [this]() { emit maximize_requested(this); });
    connect(menu->addAction(tr("Duplicate")), &QAction::triggered, this, [this]() { emit duplicate_requested(this); });
    if (content_) {
        connect(menu->addAction(tr("Refresh")), &QAction::triggered, content_, &widgets::BaseWidget::request_refresh);
    }
    menu->addSeparator();
    connect(menu->addAction(tr("Close")), &QAction::triggered, this, [this]() { emit close_requested(this); });
    menu->popup(global_pos);
}

bool fincept::screens::WidgetTile::eventFilter(QObject* obj, QEvent* event) {
    // Buttons and the link-group badge keep their own clicks and menus.
    const bool own_input = qobject_cast<QAbstractButton*>(obj) || obj->inherits("fincept::ui::GroupBadge");
    if (event->type() == QEvent::MouseButtonDblClick && !own_input) {
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            emit maximize_requested(this);
            return true;
        }
    } else if (event->type() == QEvent::ContextMenu && !own_input) {
        show_title_menu(static_cast<QContextMenuEvent*>(event)->globalPos());
        return true;
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // Store press position in global coords — immune to tile repositioning
            drag_press_global_ = me->globalPosition().toPoint();
            drag_active_ = false;
            return false; // let the press through (for button clicks on title bar)
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->buttons() & Qt::LeftButton) {
            QPoint global_pos = me->globalPosition().toPoint();
            QPoint delta = global_pos - drag_press_global_;

            if (!drag_active_ && delta.manhattanLength() >= 4) {
                drag_active_ = true;
                grabMouse();
                setCursor(Qt::SizeAllCursor);
                set_dragging(true);
                // Convert global → canvas coords for stable positioning
                QWidget* canvas = parentWidget();
                QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
                emit drag_started(this, canvas_pos);
                return true;
            }
            if (drag_active_) {
                QWidget* canvas = parentWidget();
                QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
                emit drag_moved(this, canvas_pos);
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (drag_active_) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint global_pos = me->globalPosition().toPoint();
            drag_active_ = false;
            releaseMouse();
            setCursor(Qt::ArrowCursor);
            set_dragging(false);
            QWidget* canvas = parentWidget();
            QPoint canvas_pos = canvas ? canvas->mapFromGlobal(global_pos) : global_pos;
            emit drag_released(this, canvas_pos);
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}
