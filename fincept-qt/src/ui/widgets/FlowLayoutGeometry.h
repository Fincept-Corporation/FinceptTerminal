// src/ui/widgets/FlowLayoutGeometry.h
#pragma once
// The pure geometry of ui::FlowLayout (QtCore only, header-inline): where each item of a
// wrapping left-to-right row goes for a given area. FlowLayout feeds it the items' size
// hints; tests/tst_flow_layout.cpp checks it without a widget.
//
// Rules (the superset of the per-screen copies it replaces):
//   * Items keep their order and wrap to a new line when the next one would cross the
//     right edge; the first item of a line never wraps (an item wider than the whole area
//     gets a line of its own and is clamped to the area width).
//   * Skipped items (hidden widgets) take no space.
//   * Each line can be centred in the area (centered); otherwise it starts at the left.
//   * Inside a line an item is placed at the top, or by its own alignment
//     (Qt::AlignVCenter / Qt::AlignBottom) against the tallest item of that line.
//   * Right-to-left: every line is mirrored inside the area (the first item at the right).
//   * height = from area.top() to the bottom of the last line (0 with nothing to place).

#include <QRect>
#include <QSize>
#include <QVector>
#include <Qt>

#include <algorithm>

namespace fincept::ui::flow_layout {

struct Item {
    QSize hint;                 ///< the item's size hint
    Qt::Alignment align = {};   ///< vertical alignment inside its line (AlignVCenter / AlignBottom; default top)
    bool skip = false;          ///< hidden / empty: takes no space, gets a null rect
};

struct Params {
    int h_spacing = 6;     ///< between items of a line
    int v_spacing = 4;     ///< between lines
    bool centered = false; ///< centre each line in the area
    bool rtl = false;      ///< right-to-left: mirror each line inside the area
};

struct Result {
    QVector<QRect> rects; ///< one per input item (null QRect for skipped ones)
    int height = 0;       ///< used height from area.top()
    int lines = 0;        ///< number of lines
};

/// Places `items` into `area` (only area.x(), area.y() and area.width() matter).
inline Result arrange(const QRect& area, const QVector<Item>& items, const Params& p) {
    Result r;
    r.rects.resize(items.size());
    const int left = area.x();
    const int width = std::max(1, area.width());
    const int right_edge = left + width; // one past the last usable column
    const int hs = std::max(0, p.h_spacing);
    const int vs = std::max(0, p.v_spacing);

    int x = left;
    int y = area.y();
    int line_h = 0;
    QVector<int> line; // indexes of the line being filled

    const auto finish_line = [&]() {
        if (line.isEmpty())
            return;
        const int used = r.rects[line.last()].right() + 1 - left;
        const int shift = p.centered ? std::max(0, (width - used) / 2) : 0;
        for (int i : std::as_const(line)) {
            QRect& rc = r.rects[i];
            const Qt::Alignment va = items[i].align & Qt::AlignVertical_Mask;
            int dy = 0;
            if (va & Qt::AlignVCenter)
                dy = (line_h - rc.height()) / 2;
            else if (va & Qt::AlignBottom)
                dy = line_h - rc.height();
            rc.translate(shift, dy);
            if (p.rtl)
                rc.moveLeft(left + (right_edge - 1 - rc.right()));
        }
        ++r.lines;
        line.clear();
    };

    for (int i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        if (it.skip)
            continue;
        const int w = std::clamp(it.hint.width(), 0, width);
        const int h = std::max(0, it.hint.height());
        if (!line.isEmpty() && x + w > right_edge) {
            finish_line();
            x = left;
            y += line_h + vs;
            line_h = 0;
        }
        r.rects[i] = QRect(x, y, w, h);
        line.append(i);
        x += w + hs;
        line_h = std::max(line_h, h);
    }
    const bool any = !line.isEmpty() || r.lines > 0;
    finish_line();
    r.height = any ? y + line_h - area.y() : 0;
    return r;
}

/// Size of all non-skipped items on one line (the layout's preferred size).
inline QSize one_line_size(const QVector<Item>& items, int h_spacing) {
    int w = 0;
    int h = 0;
    bool first = true;
    for (const Item& it : items) {
        if (it.skip)
            continue;
        w += it.hint.width() + (first ? 0 : std::max(0, h_spacing));
        h = std::max(h, it.hint.height());
        first = false;
    }
    return {w, h};
}

/// Default spacing from the font: horizontal ~0.8 average character, vertical a quarter
/// line (6 / 4 px at a 14 px UI font, growing with the base size).
inline int default_h_spacing(int average_char_width) {
    return std::max(4, (average_char_width * 4 + 2) / 5);
}
inline int default_v_spacing(int line_height) {
    return std::max(3, line_height / 4);
}

} // namespace fincept::ui::flow_layout
