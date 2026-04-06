#include "screens/dashboard/canvas/DashboardCanvas.h"

#include "core/logging/Logger.h"
#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QUuid>

#include <algorithm>

namespace fincept::screens {

DashboardCanvas::DashboardCanvas(QWidget* parent) : QWidget(parent) {
    tokens_ = ui::ThemeManager::instance().tokens();
    setStyleSheet(QString("background: %1;").arg(QString(tokens_.bg_base)));
    setMinimumHeight(200);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens& t) {
                tokens_ = t;
                setStyleSheet(QString("background: %1;").arg(QString(tokens_.bg_base)));
                update();
            });

    placeholder_ = new PlaceholderOverlay(this);

    scroll_timer_ = new QTimer(this);
    scroll_timer_->setInterval(40);
    connect(scroll_timer_, &QTimer::timeout, this, &DashboardCanvas::on_scroll_tick);
}

DashboardCanvas::~DashboardCanvas() {
    for (auto* t : tiles_)
        t->deleteLater();
    tiles_.clear();
}

// ── Layout management ─────────────────────────────────────────────────────────

void DashboardCanvas::load_layout(const GridLayout& layout) {
    for (auto* t : tiles_)
        t->deleteLater();
    tiles_.clear();
    layout_ = layout;

    // Ensure responsive columns
    if (width() > 0)
        layout_.cols = responsive_cols(width());

    for (const auto& item : layout_.items) {
        const WidgetMeta* meta = WidgetRegistry::instance().find(item.id);
        if (!meta) {
            LOG_WARN("Canvas", QString("Unknown widget type: %1").arg(item.id));
            continue;
        }
        auto* widget = meta->factory();
        auto* tile = new WidgetTile(item.instance_id, widget, this);
        connect_tile(tile);
        tiles_.append(tile);
        tile->show();
    }

    reflow_tiles();
    update_canvas_height();
    emit widget_count_changed(tiles_.size());
    LOG_INFO("Canvas", QString("Loaded layout with %1 tiles, %2 cols").arg(tiles_.size()).arg(layout_.cols));
}

void DashboardCanvas::apply_template(const QString& template_id) {
    const auto tmpls = all_dashboard_templates();
    for (const auto& t : tmpls) {
        if (t.id == template_id) {
            GridLayout layout;
            layout.cols = width() > 0 ? responsive_cols(width()) : 12;
            layout.row_h = layout_.row_h;
            layout.margin = layout_.margin;
            for (auto item : t.items) {
                item.instance_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                // Clamp item to current column count
                item.cell.x = std::min(item.cell.x, layout.cols - item.cell.w);
                if (item.cell.x < 0) {
                    item.cell.x = 0;
                    item.cell.w = std::min(item.cell.w, layout.cols);
                }
                layout.items.append(item);
            }
            layout.items = compact_vertical(layout.items);
            load_layout(layout);
            emit layout_changed(layout_);
            return;
        }
    }
    LOG_WARN("Canvas", QString("Template not found: %1").arg(template_id));
}

void DashboardCanvas::add_widget(const QString& widget_type_id) {
    const WidgetMeta* meta = WidgetRegistry::instance().find(widget_type_id);
    if (!meta)
        return;

    GridItem item;
    item.id = widget_type_id;
    item.instance_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int w = std::min(meta->default_w, layout_.cols);
    int h = meta->default_h;
    item.cell = find_first_fit(layout_.items, w, h, layout_.cols);
    item.cell.min_w = meta->min_w;
    item.cell.min_h = meta->min_h;

    layout_.items.append(item);
    layout_.items = compact_vertical(layout_.items);

    auto* widget = meta->factory();
    auto* tile = new WidgetTile(item.instance_id, widget, this);
    connect_tile(tile);
    tiles_.append(tile);
    tile->show();

    reflow_tiles();
    update_canvas_height();
    emit widget_count_changed(tiles_.size());
    emit layout_changed(layout_);
}

void DashboardCanvas::remove_widget(const QString& instance_id) {
    auto it = std::find_if(layout_.items.begin(), layout_.items.end(),
                           [&](const GridItem& i) { return i.instance_id == instance_id; });
    if (it != layout_.items.end())
        layout_.items.erase(it);

    auto* tile = tile_for_id(instance_id);
    if (tile) {
        tiles_.removeOne(tile);
        tile->deleteLater();
    }

    layout_.items = compact_vertical(layout_.items);
    reflow_tiles(true);
    update_canvas_height();
    emit widget_count_changed(tiles_.size());
    emit layout_changed(layout_);
}

void DashboardCanvas::set_row_height(int px) {
    layout_.row_h = px;
    reflow_tiles(true);
    update_canvas_height();
    emit layout_changed(layout_);
}

// ── Drag handling (swap on collision) ─────────────────────────────────────────

void DashboardCanvas::on_drag_started(WidgetTile* tile, QPoint canvas_pos) {
    dragging_tile_ = tile;
    drag_offset_ = canvas_pos - tile->pos();
    pre_drag_layout_ = layout_;

    auto* item = item_for_id(tile->instance_id());
    if (item)
        ghost_cell_ = item->cell;

    tile->set_dragging(true);
    tile->raise();
    update_placeholder(ghost_cell_);
    scroll_timer_->start();
}

void DashboardCanvas::on_drag_moved(WidgetTile* tile, QPoint canvas_pos) {
    last_drag_canvas_pos_ = canvas_pos;
    if (!dragging_tile_ || dragging_tile_ != tile)
        return;

    // Move tile freely
    QPoint wanted = canvas_pos - drag_offset_;
    tile->move(wanted);

    // Compute grid target
    QPoint center(wanted.x() + tile->width() / 2, wanted.y() + tile->height() / 2);
    GridCell new_ghost = rect_to_grid(
        QRect(center.x() - tile->width() / 2, center.y() - tile->height() / 2, tile->width(), tile->height()),
        layout_.cols, width(), layout_.row_h, layout_.margin, ghost_cell_.min_w, ghost_cell_.min_h);

    // Keep original size
    auto* orig = item_for_id(tile->instance_id());
    new_ghost.x = std::max(0, std::min(new_ghost.x, layout_.cols - (orig ? orig->cell.w : new_ghost.w)));
    new_ghost.y = std::max(0, new_ghost.y);
    new_ghost.w = orig ? orig->cell.w : new_ghost.w;
    new_ghost.h = orig ? orig->cell.h : new_ghost.h;
    new_ghost.min_w = orig ? orig->cell.min_w : 2;
    new_ghost.min_h = orig ? orig->cell.min_h : 3;

    if (new_ghost == ghost_cell_)
        return;
    ghost_cell_ = new_ghost;

    // Swap-based collision resolution
    GridItem moving_item;
    moving_item.instance_id = tile->instance_id();
    moving_item.cell = ghost_cell_;
    auto resolved = resolve_collisions_with_swap(pre_drag_layout_.items, moving_item);
    resolved = compact_vertical(resolved);

    // Reflow non-dragging tiles
    for (const auto& ri : resolved) {
        if (ri.instance_id == tile->instance_id())
            continue;
        auto* t = tile_for_id(ri.instance_id);
        if (t)
            t->setGeometry(grid_to_rect(ri.cell, layout_.cols, width(), layout_.row_h, layout_.margin));
    }

    update_placeholder(ghost_cell_);
}

void DashboardCanvas::on_drag_released(WidgetTile* tile, QPoint /*canvas_pos*/) {
    if (!dragging_tile_ || dragging_tile_ != tile)
        return;
    scroll_timer_->stop();

    // Apply swap resolution to the actual layout
    GridItem moving_item;
    moving_item.instance_id = tile->instance_id();
    moving_item.cell = ghost_cell_;

    layout_.items = resolve_collisions_with_swap(pre_drag_layout_.items, moving_item);
    // Update the moving item's cell
    for (auto& li : layout_.items) {
        if (li.instance_id == tile->instance_id()) {
            li.cell = ghost_cell_;
            break;
        }
    }
    layout_.items = compact_vertical(layout_.items);

    dragging_tile_ = nullptr;
    tile->set_dragging(false);
    hide_placeholder();
    reflow_tiles(true);
    update_canvas_height();
    emit layout_changed(layout_);
}

// ── Resize handling (push-down, reflows ALL tiles) ────────────────────────────

void DashboardCanvas::on_resize_started(WidgetTile* tile, QPoint /*canvas_pos*/) {
    resizing_tile_ = tile;
    pre_resize_layout_ = layout_;

    auto* item = item_for_id(tile->instance_id());
    if (item)
        resize_origin_cell_ = item->cell;

    // Initialize ghost to current cell so first move doesn't snap
    ghost_cell_ = resize_origin_cell_;

    tile->set_resizing(true);
    update_placeholder(resize_origin_cell_);
}

void DashboardCanvas::on_resize_moved(WidgetTile* tile, QPoint canvas_pos) {
    if (!resizing_tile_ || resizing_tile_ != tile)
        return;

    int col_w = (width() - layout_.margin * (layout_.cols + 1)) / layout_.cols;
    if (col_w <= 0)
        return;

    // canvas_pos is now computed from global screen coordinates in WidgetTile,
    // so it's stable even if the tile gets repositioned by setGeometry.
    // Compute tile origin from the fixed origin cell (doesn't change during resize).
    int tile_px_x = layout_.margin + resize_origin_cell_.x * (col_w + layout_.margin);
    int tile_px_y = layout_.margin + resize_origin_cell_.y * (layout_.row_h + layout_.margin);

    int new_px_w = canvas_pos.x() - tile_px_x;
    int new_px_h = canvas_pos.y() - tile_px_y;

    // Convert pixel delta to grid columns/rows.
    // Use truncation (not rounding) so the cell only grows when the mouse
    // has clearly crossed the midpoint of the next column/row. This prevents
    // the widget from shrinking on the initial click.
    int new_w = std::max(1, (new_px_w + col_w + layout_.margin / 2) / (col_w + layout_.margin));
    int new_h = std::max(1, (new_px_h + layout_.row_h + layout_.margin / 2) / (layout_.row_h + layout_.margin));

    new_w = std::max(resize_origin_cell_.min_w, std::min(new_w, layout_.cols - resize_origin_cell_.x));
    new_h = std::max(resize_origin_cell_.min_h, new_h);

    GridCell target = resize_origin_cell_;
    target.w = new_w;
    target.h = new_h;

    // Early-out: skip expensive collision resolution if nothing changed
    if (target == ghost_cell_)
        return;
    ghost_cell_ = target;

    // Build working layout with resized item
    auto working = pre_resize_layout_.items;
    for (auto& wi : working) {
        if (wi.instance_id == tile->instance_id()) {
            wi.cell = target;
            break;
        }
    }

    // Resolve + compact ALL tiles
    GridItem moving;
    moving.instance_id = tile->instance_id();
    moving.cell = target;
    working = resolve_collisions(working, moving);
    working = compact_vertical(working);

    // Reflow non-resizing tiles only.
    // The resizing tile stays where it is — we only update its geometry
    // on release, so it can't shift the coordinate basis mid-drag.
    for (const auto& wi : working) {
        if (wi.instance_id == tile->instance_id())
            continue;
        auto* t = tile_for_id(wi.instance_id);
        if (t)
            t->setGeometry(grid_to_rect(wi.cell, layout_.cols, width(), layout_.row_h, layout_.margin));
    }

    // But do resize the tile itself to reflect the new size (anchored at origin)
    QRect resizing_rect = grid_to_rect(target, layout_.cols, width(), layout_.row_h, layout_.margin);
    tile->setGeometry(resizing_rect);

    update_placeholder(target);
    update_canvas_height();
}

void DashboardCanvas::on_resize_released(WidgetTile* tile) {
    if (!resizing_tile_ || resizing_tile_ != tile)
        return;

    auto* item = item_for_id(tile->instance_id());
    if (item)
        item->cell = ghost_cell_;

    GridItem moving;
    moving.instance_id = tile->instance_id();
    moving.cell = ghost_cell_;
    layout_.items = resolve_collisions(layout_.items, moving);
    layout_.items = compact_vertical(layout_.items);

    resizing_tile_ = nullptr;
    tile->set_resizing(false);
    hide_placeholder();
    reflow_tiles(true);
    update_canvas_height();
    emit layout_changed(layout_);
}

// ── Close / scroll ────────────────────────────────────────────────────────────

void DashboardCanvas::on_tile_close(WidgetTile* tile) {
    remove_widget(tile->instance_id());
}

void DashboardCanvas::on_scroll_tick() {
    auto* scroll = qobject_cast<QScrollArea*>(parent() ? parent()->parent() : nullptr);
    if (!scroll)
        return;
    auto* bar = scroll->verticalScrollBar();
    if (!bar)
        return;

    int edge = last_drag_canvas_pos_.y() - bar->value();
    int h = scroll->viewport()->height();
    if (edge > h - 40)
        bar->setValue(bar->value() + 10);
    else if (edge < 40)
        bar->setValue(bar->value() - 10);
}

// ── Qt events ─────────────────────────────────────────────────────────────────

void DashboardCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);

    // Update responsive columns
    int new_cols = responsive_cols(width());
    if (new_cols != layout_.cols) {
        layout_.cols = new_cols;
        // Clamp items to new column count
        for (auto& item : layout_.items) {
            item.cell.w = std::min(item.cell.w, layout_.cols);
            item.cell.x = std::min(item.cell.x, layout_.cols - item.cell.w);
        }
        layout_.items = compact_vertical(layout_.items);
    }

    // During an active drag or resize, don't reflow — the operation handlers
    // manage tile positions themselves. A full reflow would fight the user.
    if (!dragging_tile_ && !resizing_tile_)
        reflow_tiles();

    update_canvas_height();
}

void DashboardCanvas::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    // Fill only the exposed region (clip rect) instead of blitting a full-height pixmap
    QRect clip = event->rect();
    p.fillRect(clip, QColor(tokens_.bg_base));

    if (layout_.cols <= 0 || width() <= 0)
        return;

    // Use border_dim at low opacity for grid lines
    QColor grid_line(tokens_.border_dim);
    grid_line.setAlpha(40);
    p.setPen(QPen(grid_line, 1));

    const int total_margin = layout_.margin * (layout_.cols + 1);
    const int col_w = (width() - total_margin) / layout_.cols;

    // Only draw grid lines that intersect the clip rect
    for (int c = 0; c <= layout_.cols; ++c) {
        int x = layout_.margin + c * (col_w + layout_.margin);
        if (x >= clip.left() - 1 && x <= clip.right() + 1)
            p.drawLine(x, clip.top(), x, clip.bottom());
    }
    int row_step = layout_.row_h + layout_.margin;
    if (row_step > 0) {
        // Start from the first row visible in the clip rect
        int first_row = std::max(0, (clip.top() - layout_.margin) / row_step) & ~1; // align to even
        for (int row = first_row; row * row_step < clip.bottom(); row += 2) {
            int y = layout_.margin + row * row_step;
            if (y >= clip.top() - 1 && y <= clip.bottom() + 1)
                p.drawLine(clip.left(), y, clip.right(), y);
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DashboardCanvas::reflow_tiles(bool animate) {
    if (width() <= 0)
        return;

    for (const auto& item : layout_.items) {
        auto* tile = tile_for_id(item.instance_id);
        if (!tile)
            continue;
        QRect target = grid_to_rect(item.cell, layout_.cols, width(), layout_.row_h, layout_.margin);

        if (animate && tile->geometry() != target) {
            // Stop any in-flight animation on this tile to prevent fighting
            auto* existing = tile->findChild<QPropertyAnimation*>(QString(), Qt::FindDirectChildrenOnly);
            if (existing)
                existing->stop();

            auto* anim = new QPropertyAnimation(tile, "geometry", tile);
            anim->setDuration(200);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->setEndValue(target);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            tile->setGeometry(target);
        }
    }
}

void DashboardCanvas::update_canvas_height() {
    int rows = max_row(layout_.items);
    int h = layout_.margin + rows * (layout_.row_h + layout_.margin);
    int min_h = std::max(h, 400);
    setMinimumHeight(min_h);
    setFixedHeight(min_h);
}

void DashboardCanvas::update_placeholder(const GridCell& cell) {
    if (width() <= 0)
        return;
    QRect r = grid_to_rect(cell, layout_.cols, width(), layout_.row_h, layout_.margin);
    placeholder_->setGeometry(r);
    placeholder_->raise();
    if (dragging_tile_)
        dragging_tile_->raise();
    placeholder_->show();
}

void DashboardCanvas::hide_placeholder() {
    placeholder_->hide();
}

void DashboardCanvas::rebuild_grid_cache() {
    // Grid lines are now painted directly in paintEvent using the clip rect.
    // This avoids allocating a full-height pixmap that caused scroll jitter.
    // Method kept for API compatibility; just triggers a repaint.
    update();
}

void DashboardCanvas::connect_tile(WidgetTile* tile) {
    connect(tile, &WidgetTile::drag_started, this, &DashboardCanvas::on_drag_started);
    connect(tile, &WidgetTile::drag_moved, this, &DashboardCanvas::on_drag_moved);
    connect(tile, &WidgetTile::drag_released, this, &DashboardCanvas::on_drag_released);
    connect(tile, &WidgetTile::resize_started, this, &DashboardCanvas::on_resize_started);
    connect(tile, &WidgetTile::resize_moved, this, &DashboardCanvas::on_resize_moved);
    connect(tile, &WidgetTile::resize_released, this, &DashboardCanvas::on_resize_released);
    connect(tile, &WidgetTile::close_requested, this, &DashboardCanvas::on_tile_close);
}

WidgetTile* DashboardCanvas::tile_for_id(const QString& instance_id) const {
    for (auto* t : tiles_)
        if (t->instance_id() == instance_id)
            return t;
    return nullptr;
}

GridItem* DashboardCanvas::item_for_id(const QString& instance_id) {
    for (auto& i : layout_.items)
        if (i.instance_id == instance_id)
            return &i;
    return nullptr;
}

} // namespace fincept::screens
