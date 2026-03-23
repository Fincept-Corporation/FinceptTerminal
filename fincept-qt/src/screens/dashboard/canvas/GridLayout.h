#pragma once
#include <QRect>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// ── Grid coordinate types ─────────────────────────────────────────────────────

struct GridCell {
    int x = 0; // column index [0, cols)
    int y = 0; // row index    [0, inf)
    int w = 4; // column span  [1, cols]
    int h = 4; // row span     [1, inf)
    int min_w = 2;
    int min_h = 3;

    bool operator==(const GridCell& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

struct GridItem {
    QString id;          // widget type id, e.g. "indices"
    QString instance_id; // unique UUID per tile
    GridCell cell;
    bool is_static = false;
};

struct GridLayout {
    QVector<GridItem> items;
    int cols = 12;
    int row_h = 60; // px per row
    int margin = 4; // px gap between tiles
};

// ── Pure math helpers ─────────────────────────────────────────────────────────

/// Convert grid coords -> pixel rect inside a container of given width.
inline QRect grid_to_rect(const GridCell& c, int cols, int container_w, int row_h, int margin) {
    if (cols <= 0 || container_w <= 0)
        return {};
    const int total_margin = margin * (cols + 1);
    const int col_w = (container_w - total_margin) / cols;
    const int px_x = margin + c.x * (col_w + margin);
    const int px_y = margin + c.y * (row_h + margin);
    const int px_w = c.w * col_w + (c.w - 1) * margin;
    const int px_h = c.h * row_h + (c.h - 1) * margin;
    return QRect(px_x, px_y, std::max(px_w, 1), std::max(px_h, 1));
}

/// Convert pixel rect -> grid coords. Clamps to valid range.
inline GridCell rect_to_grid(const QRect& r, int cols, int container_w, int row_h, int margin, int min_w = 1,
                             int min_h = 1) {
    if (cols <= 0 || container_w <= 0)
        return {};
    const int total_margin = margin * (cols + 1);
    const int col_w = (container_w - total_margin) / cols;
    if (col_w <= 0)
        return {};

    GridCell c;
    c.x = std::max(0, (r.x() - margin + col_w / 2) / (col_w + margin));
    c.y = std::max(0, (r.y() - margin + row_h / 2) / (row_h + margin));
    c.w = std::max(1, (r.width() + margin / 2) / (col_w + margin));
    c.h = std::max(1, (r.height() + margin / 2) / (row_h + margin));

    c.x = std::min(c.x, cols - 1);
    c.w = std::max(min_w, std::min(c.w, cols - c.x));
    c.h = std::max(min_h, c.h);
    return c;
}

/// True if two grid cells overlap.
inline bool cells_overlap(const GridCell& a, const GridCell& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

/// Vertical compaction — pack all items upward. Takes FIRST (lowest) valid y.
inline QVector<GridItem> compact_vertical(QVector<GridItem> items) {
    std::stable_sort(items.begin(), items.end(), [](const GridItem& a, const GridItem& b) {
        return a.cell.y != b.cell.y ? a.cell.y < b.cell.y : a.cell.x < b.cell.x;
    });

    QVector<GridItem> placed;
    placed.reserve(items.size());

    for (auto item : items) {
        if (item.is_static) {
            placed.append(item);
            continue;
        }
        int best_y = item.cell.y;
        for (int try_y = 0; try_y <= item.cell.y; ++try_y) {
            GridCell candidate = item.cell;
            candidate.y = try_y;
            bool fits = true;
            for (const auto& p : placed) {
                if (p.instance_id != item.instance_id && cells_overlap(candidate, p.cell)) {
                    fits = false;
                    break;
                }
            }
            if (fits) {
                best_y = try_y;
                break;
            }
        }
        item.cell.y = best_y;
        placed.append(item);
    }
    return placed;
}

/// Find first available position for a widget of size (w, h).
inline GridCell find_first_fit(const QVector<GridItem>& items, int w, int h, int cols) {
    for (int row = 0; row < 1000; ++row) {
        for (int col = 0; col <= cols - w; ++col) {
            GridCell candidate{col, row, w, h};
            bool ok = true;
            for (const auto& item : items) {
                if (cells_overlap(candidate, item.cell)) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return candidate;
        }
    }
    return {0, 0, w, h};
}

/// Swap positions: when dragged tile lands on another, swap their positions.
/// For tiles that don't directly collide with the moving tile, push down.
inline QVector<GridItem> resolve_collisions_with_swap(QVector<GridItem> items, const GridItem& moving) {
    // Find the item most overlapped by the moving item
    QString swap_id;
    int max_overlap = 0;
    for (const auto& item : items) {
        if (item.instance_id == moving.instance_id || item.is_static)
            continue;
        if (cells_overlap(item.cell, moving.cell)) {
            int ox = std::min(item.cell.x + item.cell.w, moving.cell.x + moving.cell.w) -
                     std::max(item.cell.x, moving.cell.x);
            int oy = std::min(item.cell.y + item.cell.h, moving.cell.y + moving.cell.h) -
                     std::max(item.cell.y, moving.cell.y);
            int area = ox * oy;
            if (area > max_overlap) {
                max_overlap = area;
                swap_id = item.instance_id;
            }
        }
    }

    if (!swap_id.isEmpty()) {
        // Find the original position of the moving item to swap
        GridCell moving_original;
        for (const auto& item : items) {
            if (item.instance_id == moving.instance_id) {
                moving_original = item.cell;
                break;
            }
        }

        // Swap: the collided item takes the original position of the moving item
        for (auto& item : items) {
            if (item.instance_id == swap_id) {
                item.cell.x = moving_original.x;
                item.cell.y = moving_original.y;
                break;
            }
        }
    }

    // Push down any remaining collisions (non-swap items)
    bool changed = true;
    int passes = 0;
    while (changed && passes++ < static_cast<int>(items.size()) * 2 + 2) {
        changed = false;
        for (auto& item : items) {
            if (item.instance_id == moving.instance_id || item.is_static)
                continue;
            if (cells_overlap(item.cell, moving.cell)) {
                item.cell.y = moving.cell.y + moving.cell.h;
                changed = true;
            }
            for (const auto& other : items) {
                if (other.instance_id == item.instance_id || other.instance_id == moving.instance_id)
                    continue;
                if (cells_overlap(item.cell, other.cell)) {
                    int new_y = std::max(other.cell.y + other.cell.h, item.cell.y);
                    if (new_y != item.cell.y) {
                        item.cell.y = new_y;
                        changed = true;
                    }
                }
            }
        }
    }
    return items;
}

/// Push-down collision resolution (for resize — no swapping).
inline QVector<GridItem> resolve_collisions(QVector<GridItem> items, const GridItem& moving) {
    bool changed = true;
    int passes = 0;
    while (changed && passes++ < static_cast<int>(items.size()) * 2 + 2) {
        changed = false;
        for (auto& item : items) {
            if (item.instance_id == moving.instance_id || item.is_static)
                continue;
            if (cells_overlap(item.cell, moving.cell)) {
                item.cell.y = moving.cell.y + moving.cell.h;
                changed = true;
            }
            for (const auto& other : items) {
                if (other.instance_id == item.instance_id || other.instance_id == moving.instance_id)
                    continue;
                if (cells_overlap(item.cell, other.cell)) {
                    int new_y = std::max(other.cell.y + other.cell.h, item.cell.y);
                    if (new_y != item.cell.y) {
                        item.cell.y = new_y;
                        changed = true;
                    }
                }
            }
        }
    }
    return items;
}

/// Compute responsive column count based on container width.
inline int responsive_cols(int container_w) {
    if (container_w < 600)
        return 6;  // small: 2 widgets per row (each w=3 in 6-col grid)
    if (container_w < 1000)
        return 9;  // medium: 3 widgets
    return 12;     // large: 3-4 widgets (standard)
}

/// Maximum row bottom edge across all items.
inline int max_row(const QVector<GridItem>& items) {
    int m = 0;
    for (const auto& i : items)
        m = std::max(m, i.cell.y + i.cell.h);
    return m;
}

} // namespace fincept::screens
