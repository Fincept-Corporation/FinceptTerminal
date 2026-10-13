#include "screens/dashboard/canvas/DashboardCanvas.h"

#include "core/logging/Logger.h"
#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QElapsedTimer>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QUuid>

#include <algorithm>

namespace fincept::screens {

DashboardCanvas::DashboardCanvas(QWidget* parent) : QWidget(parent) {
    tokens_ = ui::ThemeManager::instance().tokens();
    // Set both autoFillBackground (Qt paints bg_base before paintEvent) AND
    // a stylesheet so the background survives hide/show cycles from tab switching.
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    apply_bg();
    setMinimumHeight(200);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens& t) {
        tokens_ = t;
        apply_bg();
        update();
    });

    placeholder_ = new PlaceholderOverlay(this);

    scroll_timer_ = new QTimer(this);
    scroll_timer_->setInterval(40);
    connect(scroll_timer_, &QTimer::timeout, this, &DashboardCanvas::on_scroll_tick);

    // Debounce resize events so ADS splitter drags don't trigger repeated
    // column recalculations and layout compaction on every pixel of movement.
    resize_timer_ = new QTimer(this);
    resize_timer_->setSingleShot(true);
    resize_timer_->setInterval(150);
    connect(resize_timer_, &QTimer::timeout, this, [this]() {
        const int w = pending_resize_w_;
        if (w <= 0)
            return;

        // Restore to canonical columns if width now permits it — user's layout
        // should snap back when they expand the panel again.
        const int target_cols = (w >= 1000)  ? canonical_cols_
                                : (w >= 600) ? std::min(canonical_cols_, 9)
                                             : std::min(canonical_cols_, 6);

        if (target_cols != layout_.cols) {
            // Leaving the canonical column count: the items are still in the
            // user's own geometry (and may have been edited since the last
            // snapshot), so capture them before folding.
            if (layout_.cols == canonical_cols_) {
                canonical_items_ = layout_.items;
                canonical_valid_ = true;
            }
            layout_.cols = target_cols;
            // Fold from the layout as designed (when the user has not edited
            // since), so widening the panel restores it rather than keeping an
            // earlier narrow fold.
            if (canonical_valid_)
                layout_.items = canonical_items_;
            fit_items_to_cols();
        }

        if (!dragging_tile_ && !resizing_tile_)
            reflow_tiles();
        update_canvas_height();
    });
}

DashboardCanvas::~DashboardCanvas() {
    cancel_pending_build();
    for (auto* t : tiles_)
        t->deleteLater();
    tiles_.clear();
}

// ── Layout management ─────────────────────────────────────────────────────────

void DashboardCanvas::fit_items_to_cols() {
    for (auto& item : layout_.items) {
        item.cell.w = std::min(item.cell.w, layout_.cols);
        item.cell.x = std::max(0, std::min(item.cell.x, layout_.cols - item.cell.w));
    }
    layout_.items = compact_vertical(layout_.items);
}

GridLayout DashboardCanvas::current_layout() const {
    GridLayout out = layout_;
    // While the visible grid is only a narrower fold of the user's layout (and
    // they have not edited since), report/persist the layout as designed —
    // otherwise merely having the dashboard open in a narrow panel and pressing
    // SAVE (or changing a tile's settings) would permanently squash it.
    if (canonical_valid_ && layout_.cols != canonical_cols_) {
        out.items = canonical_items_;
        out.cols = canonical_cols_;
    }
    return out;
}

void DashboardCanvas::load_layout(const GridLayout& layout, TileBuild mode) {
    QElapsedTimer timer;
    timer.start();

    cancel_pending_build();
    for (auto* t : tiles_)
        t->deleteLater();
    tiles_.clear();
    layout_ = layout;

    // Record the canonical column count from the saved layout — this is what
    // the user designed for. Responsive shrink is allowed on narrow viewports
    // but we restore to this when the panel expands back to full width.
    canonical_cols_ = layout_.cols > 0 ? layout_.cols : 12;

    // Apply responsive columns for current width without clamping canonical_cols_
    if (width() > 0)
        layout_.cols = responsive_cols(width());

    // Unknown widget types are dropped up front (a registry lookup is cheap),
    // so the grid is compacted once, before any tile is placed: tiles that a
    // progressive build has already put on screen never shift afterwards.
    // (They used to be skipped while the GridItem stayed in layout_.items —
    // a permanent invisible hole re-serialised on every save.)
    QVector<GridItem> kept;
    kept.reserve(layout_.items.size());
    for (const auto& item : layout_.items) {
        if (!WidgetRegistry::instance().find(item.id)) {
            LOG_WARN("Canvas", QString("Dropping tile with unknown widget type: %1").arg(item.id));
            continue;
        }
        kept.append(item);
    }
    const bool dropped_unknown = kept.size() != layout_.items.size();
    if (dropped_unknown)
        layout_.items = compact_vertical(kept);

    // Remember the layout exactly as designed, then fold it into the grid this
    // width affords. A layout built for more columns than the panel has (a
    // 12-column template in a panel under 1000 px, say) used to be left with its
    // right-most tiles positioned past the canvas edge and clipped: only the
    // debounced resize path folded items, and it does nothing when the column
    // count already matches. Keeping the original lets the resize path snap back
    // to it when the panel widens, instead of staying squashed.
    canonical_items_ = layout_.items;
    canonical_valid_ = true;
    if (layout_.cols < canonical_cols_)
        fit_items_to_cols();

    // Final geometry is known now — size the canvas before any tile exists.
    update_canvas_height();

    // Build order: tiles intersecting the visible part of the canvas first,
    // then the rest, each group top-to-bottom / left-to-right.
    QRect visible = visibleRegion().boundingRect();
    if (visible.isEmpty())
        visible = QRect(0, 0, std::max(width(), 1), 900);
    const int container_w = std::max(width(), 1);
    QVector<const GridItem*> order;
    order.reserve(layout_.items.size());
    for (const auto& item : layout_.items)
        order.append(&item);
    std::stable_sort(order.begin(), order.end(), [&](const GridItem* a, const GridItem* b) {
        const bool va =
            grid_to_rect(a->cell, layout_.cols, container_w, layout_.row_h, layout_.margin).intersects(visible);
        const bool vb =
            grid_to_rect(b->cell, layout_.cols, container_w, layout_.row_h, layout_.margin).intersects(visible);
        if (va != vb)
            return va;
        if (a->cell.y != b->cell.y)
            return a->cell.y < b->cell.y;
        return a->cell.x < b->cell.x;
    });
    QStringList build_order;
    build_order.reserve(order.size());
    for (const auto* item : order)
        build_order.append(item->instance_id);

    if (mode == TileBuild::Immediate) {
        for (const QString& id : build_order) {
            const GridItem* item = item_for_id(id);
            if (item && !build_tile(*item))
                drop_unbuildable_item(id);
        }
        reflow_tiles();
        update_canvas_height();
        emit widget_count_changed(tiles_.size());
        LOG_INFO("Canvas", QString("Loaded layout with %1 tiles, %2 cols in %3 ms")
                               .arg(tiles_.size())
                               .arg(layout_.cols)
                               .arg(timer.elapsed()));
    } else {
        pending_build_ = build_order;
        pending_ids_ = QSet<QString>(build_order.cbegin(), build_order.cend());
        // Report the planned count now so the toolbar / status bar don't read
        // EMPTY while the tiles stream in; corrected at the end if any fail.
        emit widget_count_changed(static_cast<int>(pending_build_.size()));
        update(); // paint the skeleton frames for the queued cells
        const quint64 gen = build_generation_;
        QTimer::singleShot(0, this, [this, gen]() { build_next_pending_tile(gen); });
        LOG_INFO("Canvas", QString("Loading layout with %1 tiles, %2 cols (progressive)")
                               .arg(pending_build_.size())
                               .arg(layout_.cols));
    }

    if (dropped_unknown)
        emit layout_changed(layout_);
}

bool DashboardCanvas::build_tile(const GridItem& item) {
    const WidgetMeta* meta = WidgetRegistry::instance().find(item.id);
    if (!meta)
        return false;
    auto* widget = meta->factory(item.config);
    if (!widget) {
        LOG_WARN("Canvas", QString("Factory returned null for widget type: %1").arg(item.id));
        return false;
    }
    auto* tile = new WidgetTile(item.instance_id, widget, this);
    connect_tile(tile);
    tiles_.append(tile);
    if (width() > 0)
        tile->setGeometry(grid_to_rect(item.cell, layout_.cols, width(), layout_.row_h, layout_.margin));
    tile->show();
    return true;
}

void DashboardCanvas::build_next_pending_tile(quint64 generation) {
    if (generation != build_generation_ || pending_build_.isEmpty())
        return;

    const QString id = pending_build_.takeFirst();
    pending_ids_.remove(id);
    // The item can be gone (removed meanwhile) or already built (add path).
    if (const GridItem* item = item_for_id(id); item && !tile_for_id(id)) {
        if (!build_tile(*item))
            drop_unbuildable_item(id);
    }
    update(); // the skeleton this tile replaced

    if (!pending_build_.isEmpty()) {
        QTimer::singleShot(0, this, [this, generation]() { build_next_pending_tile(generation); });
        return;
    }
    emit widget_count_changed(tiles_.size());
    LOG_INFO("Canvas", QString("Progressive layout build finished: %1 tiles").arg(tiles_.size()));
}

void DashboardCanvas::flush_pending_tiles() {
    if (pending_build_.isEmpty())
        return;
    const QStringList ids = pending_build_;
    cancel_pending_build();
    for (const QString& id : ids) {
        const GridItem* item = item_for_id(id);
        if (item && !tile_for_id(id) && !build_tile(*item))
            drop_unbuildable_item(id);
    }
    reflow_tiles();
    update_canvas_height();
    update();
    emit widget_count_changed(tiles_.size());
}

void DashboardCanvas::cancel_pending_build() {
    ++build_generation_;
    pending_build_.clear();
    pending_ids_.clear();
}

void DashboardCanvas::drop_unbuildable_item(const QString& instance_id) {
    auto it = std::find_if(layout_.items.begin(), layout_.items.end(),
                           [&](const GridItem& i) { return i.instance_id == instance_id; });
    if (it == layout_.items.end())
        return;
    layout_.items.erase(it);
    layout_.items = compact_vertical(layout_.items);
    canonical_valid_ = false;
    reflow_tiles();
    update_canvas_height();
    emit layout_changed(layout_);
}

void DashboardCanvas::apply_template(const QString& template_id, TileBuild mode) {
    const auto tmpls = all_dashboard_templates();
    for (const auto& t : tmpls) {
        if (t.id == template_id) {
            GridLayout layout;
            layout.cols = 12; // templates are designed for 12 columns
            canonical_cols_ = 12;
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
            load_layout(layout, mode);
            emit layout_changed(layout_);
            return;
        }
    }
    LOG_WARN("Canvas", QString("Template not found: %1").arg(template_id));
}

void DashboardCanvas::add_widget(const QString& widget_type_id) {
    add_widget_with(widget_type_id, QJsonObject{});
}

QString DashboardCanvas::add_widget_with(const QString& widget_type_id, const QJsonObject& config, int x, int y,
                                         int w, int h) {
    const WidgetMeta* meta = WidgetRegistry::instance().find(widget_type_id);
    if (!meta)
        return {};

    GridItem item;
    item.id = widget_type_id;
    item.instance_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.config = config;
    const int cols = std::max(1, layout_.cols);
    const int iw = std::clamp(w > 0 ? std::max(w, meta->min_w) : meta->default_w, 1, cols);
    const int ih = std::max(1, h > 0 ? std::max(h, meta->min_h) : meta->default_h);
    if (x < 0 && y < 0) {
        item.cell = find_first_fit(layout_.items, iw, ih, cols);
    } else {
        item.cell.w = iw;
        item.cell.h = ih;
        item.cell.x = std::clamp(x < 0 ? 0 : x, 0, cols - iw);
        item.cell.y = std::max(0, y < 0 ? max_row(layout_.items) : y);
    }
    item.cell.min_w = std::min(meta->min_w, cols);
    item.cell.min_h = meta->min_h;

    // Construct first: never leave a GridItem behind for a widget that failed
    // to construct — that produced an invisible tile that still reserved space.
    if (!build_tile(item))
        return {};

    layout_.items.append(item);
    // An explicit position pushes whatever it lands on down; auto-placement
    // already found a free spot.
    if (x >= 0 || y >= 0)
        layout_.items = resolve_collisions(layout_.items, item);
    layout_.items = compact_vertical(layout_.items);
    canonical_valid_ = false; // layout edited at the current column count

    reflow_tiles();
    update_canvas_height();
    emit widget_count_changed(tiles_.size());
    emit layout_changed(layout_);
    return item.instance_id;
}

void DashboardCanvas::remove_widget(const QString& instance_id) {
    auto it = std::find_if(layout_.items.begin(), layout_.items.end(),
                           [&](const GridItem& i) { return i.instance_id == instance_id; });
    if (it != layout_.items.end())
        layout_.items.erase(it);
    pending_build_.removeAll(instance_id);
    pending_ids_.remove(instance_id);

    auto* tile = tile_for_id(instance_id);
    if (tile) {
        tiles_.removeOne(tile);
        tile->deleteLater();
    }

    layout_.items = compact_vertical(layout_.items);
    canonical_valid_ = false; // layout edited at the current column count
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

bool DashboardCanvas::set_item_cell(const QString& instance_id, int x, int y, int w, int h) {
    GridItem* item = item_for_id(instance_id);
    if (!item)
        return false;
    const int cols = std::max(1, layout_.cols);
    GridCell cell = item->cell;
    if (w > 0)
        cell.w = std::clamp(std::max(w, cell.min_w), 1, cols);
    if (h > 0)
        cell.h = std::max(h, cell.min_h);
    if (x >= 0)
        cell.x = x;
    if (y >= 0)
        cell.y = y;
    cell.x = std::clamp(cell.x, 0, cols - cell.w);
    item->cell = cell;

    GridItem moving;
    moving.instance_id = instance_id;
    moving.cell = cell;
    layout_.items = resolve_collisions(layout_.items, moving);
    layout_.items = compact_vertical(layout_.items);
    canonical_valid_ = false; // layout edited at the current column count

    reflow_tiles(true);
    update_canvas_height();
    emit layout_changed(layout_);
    return true;
}

bool DashboardCanvas::set_item_config(const QString& instance_id, const QJsonObject& config) {
    GridItem* item = item_for_id(instance_id);
    if (!item)
        return false;
    item->config = config;
    for (auto& ci : canonical_items_) {
        if (ci.instance_id == instance_id) {
            ci.config = config;
            break;
        }
    }
    if (auto* tile = tile_for_id(instance_id); tile && tile->content_widget())
        tile->content_widget()->apply_config(config);
    // A still-queued tile is built from item->config, which is already updated.
    emit layout_changed(layout_);
    return true;
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
    canonical_valid_ = false; // layout edited at the current column count

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
    canonical_valid_ = false; // layout edited at the current column count

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

void DashboardCanvas::apply_bg() {
    // Set both stylesheet AND QPalette so Qt always knows the correct background,
    // even during hide/show cycles from tab switching in ADS docking.
    QString bg = QString(tokens_.bg_base);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString("background:%1;").arg(bg));
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(bg));
    pal.setColor(QPalette::Base, QColor(bg));
    pal.setColor(QPalette::Button, QColor(bg));
    setPalette(pal);
}

void DashboardCanvas::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Re-apply background and force immediate repaint when tab is re-selected.
    // Without this, the canvas shows Qt's default grey for one frame.
    apply_bg();
    update();
}

void DashboardCanvas::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);

    // Immediately reflow tiles to the new pixel width without changing column
    // count. This keeps widget positions visually correct as the panel is dragged.
    // Column recalculation (and potentially layout compaction) is deferred to
    // resize_timer_ so transient ADS splitter movements don't destroy the layout.
    if (!dragging_tile_ && !resizing_tile_)
        reflow_tiles();

    update_canvas_height();

    // Kick the debounce timer — column recalc fires after 150ms of stable width.
    pending_resize_w_ = width();
    resize_timer_->start();
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

    // Skeleton frames for tiles a progressive build has not constructed yet,
    // so the grid reads as "loading", not as holes.
    if (!pending_ids_.isEmpty()) {
        p.setPen(QPen(QColor(tokens_.border_dim), 1));
        const QColor fill(tokens_.bg_surface);
        for (const auto& item : layout_.items) {
            if (!pending_ids_.contains(item.instance_id))
                continue;
            const QRect r = grid_to_rect(item.cell, layout_.cols, width(), layout_.row_h, layout_.margin);
            if (!r.intersects(clip))
                continue;
            p.fillRect(r, fill);
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DashboardCanvas::reflow_tiles(bool animate) {
    if (width() <= 0)
        return;
    if (!pending_ids_.isEmpty())
        update(); // skeleton frames follow their (possibly moved) cells

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

void DashboardCanvas::connect_tile(WidgetTile* tile) {
    connect(tile, &WidgetTile::drag_started, this, &DashboardCanvas::on_drag_started);
    connect(tile, &WidgetTile::drag_moved, this, &DashboardCanvas::on_drag_moved);
    connect(tile, &WidgetTile::drag_released, this, &DashboardCanvas::on_drag_released);
    connect(tile, &WidgetTile::resize_started, this, &DashboardCanvas::on_resize_started);
    connect(tile, &WidgetTile::resize_moved, this, &DashboardCanvas::on_resize_moved);
    connect(tile, &WidgetTile::resize_released, this, &DashboardCanvas::on_resize_released);
    connect(tile, &WidgetTile::close_requested, this, &DashboardCanvas::on_tile_close);

    // Per-instance widget config changes (gear-icon dialog) — persist into GridItem
    // and emit layout_changed so the DashboardScreen writes it through to sqlite.
    if (auto* content = tile->content_widget()) {
        const QString instance_id = tile->instance_id();
        connect(content, &widgets::BaseWidget::config_changed, this, [this, instance_id](const QJsonObject& cfg) {
            if (auto* item = item_for_id(instance_id)) {
                item->config = cfg;
                // Geometry is untouched, so the as-designed snapshot stays valid
                // - just keep its copy of the config in step.
                for (auto& ci : canonical_items_) {
                    if (ci.instance_id == instance_id) {
                        ci.config = cfg;
                        break;
                    }
                }
                emit layout_changed(layout_);
            }
        });
    }
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
