#pragma once
// ThemeHelpers.h — small shared helpers for widgets that keep appearance state
// the central re-style pass (ThemeRestyle.cpp) cannot reach:
//
//   * ui::metrics — text-bearing sizes (row heights, button heights, widths
//     sized for a label) that are N px at the default font size and grow with
//     the user's font, optionally kept in step with later font changes;
//   * ui::ThemeRecolor — re-colours cached token colours (QColor members,
//     QTableWidgetItem / QTreeWidgetItem brushes) from the token set they were
//     built with to the active one.
//
// Everything reads the active appearance from ThemeManager (ui/theme/Theme.h).

#include "ui/theme/Theme.h"

#include <QBrush>
#include <QColor>
#include <QFontMetrics>
#include <QHash>
#include <QHeaderView>
#include <QTableView>
#include <QTableWidget>
#include <QTreeWidget>
#include <QWidget>

#include <algorithm>

namespace fincept::ui {

namespace metrics {

/// A text-bearing height (row, strip, button) designed as `px_at_default` at
/// the default font size: grows 1:1 with a larger base font size so its text is
/// not clipped, and stays at the design value for smaller ones.
inline int height(int px_at_default) {
    return std::max(px_at_default, px_at_default + fonts::font_px() - ThemeManager::kDefaultFontPx);
}

/// A width designed for its text as `px_at_default` at the default font size:
/// grows in proportion to a larger base font size, never shrinks below the design.
inline int width(int px_at_default) {
    return std::max(px_at_default, px_at_default * fonts::font_px() / ThemeManager::kDefaultFontPx);
}

/// Height of one line of text at `font_offset` px from the base size plus
/// `padding`, but at least `min_px` — for table rows and list entries.
inline int row_height(int font_offset, int min_px, int padding = 8) {
    return std::max(min_px, QFontMetrics(fonts::font(font_offset)).height() + padding);
}

/// setFixedHeight(height(px_at_default)), re-applied whenever the appearance
/// changes, for widgets whose fixed height was chosen for their text.
inline void follow_fixed_height(QWidget* w, int px_at_default) {
    if (!w)
        return;
    w->setFixedHeight(height(px_at_default));
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, w,
                     [w, px_at_default](const ThemeTokens&) { w->setFixedHeight(height(px_at_default)); });
}

/// setFixedWidth(width(px_at_default)), re-applied whenever the appearance changes.
inline void follow_fixed_width(QWidget* w, int px_at_default) {
    if (!w)
        return;
    w->setFixedWidth(width(px_at_default));
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, w,
                     [w, px_at_default](const ThemeTokens&) { w->setFixedWidth(width(px_at_default)); });
}

/// setFixedSize(width(w_px), height(h_px)), re-applied whenever the appearance changes.
inline void follow_fixed_size(QWidget* w, int w_px_at_default, int h_px_at_default) {
    if (!w)
        return;
    w->setFixedSize(width(w_px_at_default), height(h_px_at_default));
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, w,
                     [w, w_px_at_default, h_px_at_default](const ThemeTokens&) {
                         w->setFixedSize(width(w_px_at_default), height(h_px_at_default));
                     });
}

/// Every row of `view` height(px_at_default) tall — set as the vertical
/// header's default section size (rows inserted later get it too) and
/// re-applied to all rows whenever the appearance changes. Do not also call
/// setRowHeight() on the rows.
inline void follow_row_height(QTableView* view, int px_at_default) {
    if (!view)
        return;
    view->verticalHeader()->setDefaultSectionSize(height(px_at_default));
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, view,
                     [view, px_at_default](const ThemeTokens&) {
                         view->verticalHeader()->setDefaultSectionSize(height(px_at_default));
                     });
}

} // namespace metrics

/// Re-colours cached token colours after a theme or direction-palette change.
///
/// Holds the token set the cached colours were built with. On
/// ThemeManager::theme_changed call advance() once, then map() each cached
/// colour (or apply() to a table / tree): a colour that is a token of the old
/// set becomes the same token of the active set, keeping its alpha; any other
/// colour (a legible() / tint() result, a user colour) is returned unchanged —
/// rebuild those from their source instead.
class ThemeRecolor {
  public:
    ThemeRecolor() : from_(ThemeManager::instance().tokens()) {}

    /// Switch the mapping to "token set captured last → active token set" and
    /// capture the active set for the next change.
    void advance() {
        const ThemeTokens& to = ThemeManager::instance().tokens();
        map_.clear();
        using Field = const char* ThemeTokens::*;
        static constexpr Field kFields[] = {
            &ThemeTokens::bg_base,        &ThemeTokens::bg_surface,     &ThemeTokens::bg_raised,
            &ThemeTokens::bg_hover,       &ThemeTokens::border_dim,     &ThemeTokens::border_med,
            &ThemeTokens::border_bright,  &ThemeTokens::text_primary,   &ThemeTokens::text_secondary,
            &ThemeTokens::text_tertiary,  &ThemeTokens::text_dim,       &ThemeTokens::accent,
            &ThemeTokens::accent_dim,     &ThemeTokens::text_on_accent, &ThemeTokens::icon_dim,
            &ThemeTokens::icon_hover,     &ThemeTokens::positive,       &ThemeTokens::positive_dim,
            &ThemeTokens::negative,       &ThemeTokens::negative_dim,   &ThemeTokens::warning,
            &ThemeTokens::info,           &ThemeTokens::cyan,           &ThemeTokens::success,
            &ThemeTokens::danger,         &ThemeTokens::accent_bg,      &ThemeTokens::positive_bg,
            &ThemeTokens::negative_bg,    &ThemeTokens::row_alt,        &ThemeTokens::chart_neutral,
            &ThemeTokens::seq_low,        &ThemeTokens::seq_high,
        };
        for (Field f : kFields)
            add(from_.*f, to.*f);
        for (size_t i = 0; i < from_.chart_palette.size(); ++i)
            add(from_.chart_palette[i], to.chart_palette[i]);
        from_ = to;
    }

    /// `c` translated to the active token set (see class comment).
    QColor map(const QColor& c) const {
        if (!c.isValid() || map_.isEmpty())
            return c;
        const auto it = map_.constFind(c.rgb() & 0x00ffffffu);
        if (it == map_.constEnd())
            return c;
        QColor out = QColor::fromRgb(*it);
        out.setAlpha(c.alpha());
        return out;
    }

    /// `b` with its colour translated; a brush without a fill stays as it is.
    QBrush map(const QBrush& b) const {
        if (b.style() == Qt::NoBrush)
            return b;
        QBrush out = b;
        out.setColor(map(b.color()));
        return out;
    }

    /// Translate every item's foreground and background brush.
    void apply(QTableWidget* table) const {
        if (!table || map_.isEmpty())
            return;
        const bool sorting = table->isSortingEnabled(); // rows must not move while items change
        if (sorting)
            table->setSortingEnabled(false);
        for (int r = 0; r < table->rowCount(); ++r) {
            for (int c = 0; c < table->columnCount(); ++c) {
                if (QTableWidgetItem* item = table->item(r, c))
                    apply_item(item);
            }
        }
        if (sorting)
            table->setSortingEnabled(true);
    }

    /// Translate every item's foreground and background brush, all columns.
    void apply(QTreeWidget* tree) const {
        if (!tree || map_.isEmpty())
            return;
        const int cols = tree->columnCount();
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            QTreeWidgetItem* item = *it;
            for (int c = 0; c < cols; ++c) {
                const QBrush fg = item->foreground(c);
                const QBrush bg = item->background(c);
                const QBrush fg2 = map(fg);
                const QBrush bg2 = map(bg);
                if (fg2 != fg)
                    item->setForeground(c, fg2);
                if (bg2 != bg)
                    item->setBackground(c, bg2);
            }
        }
    }

  private:
    void add(const char* from, const char* to) {
        if (!from || !to)
            return;
        const QRgb key = QColor(from).rgb() & 0x00ffffffu;
        if (!map_.contains(key))
            map_.insert(key, QColor(to).rgb() & 0x00ffffffu);
    }

    template <typename Item>
    void apply_item(Item* item) const {
        const QBrush fg = item->foreground();
        const QBrush bg = item->background();
        const QBrush fg2 = map(fg);
        const QBrush bg2 = map(bg);
        if (fg2 != fg)
            item->setForeground(fg2);
        if (bg2 != bg)
            item->setBackground(bg2);
    }

    ThemeTokens from_;
    QHash<QRgb, QRgb> map_;
};

} // namespace fincept::ui
