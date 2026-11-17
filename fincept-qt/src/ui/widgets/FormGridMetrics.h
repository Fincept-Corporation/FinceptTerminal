// src/ui/widgets/FormGridMetrics.h
#pragma once
// FormGridMetrics — the pure arithmetic of a compact input form that sits beside (or above)
// its results (header-only, std only; tests/tst_form_grid_metrics.cpp checks it without a
// widget):
//
//   * field_px     — width of an input box that shows `chars` characters of the input font,
//                    plus its frame / padding / drop-down ("chrome");
//   * span_px      — a cell spanning `span` columns of equal fields, the gaps between included;
//   * form_px      — a whole form of `columns` fields with its padding on both sides;
//   * columns_for  — how many items of a fixed width fit in a width (between 1 and a maximum);
//   * side_by_side — the responsive breakpoint: does a fixed-width input column plus a results
//                    area of at least `trail_min_px` fit next to each other?
//
// Everything is plain px. Callers feed font metrics (QFontMetrics::horizontalAdvance('0') of
// the input font for char_px) and re-evaluate on resize and on
// ThemeManager::appearance_settled — a font-size change moves every number here.

#include <algorithm>

namespace fincept::ui::form_grid {

/// Width of an input box that shows `chars` characters (at least one) of `char_px` wide text,
/// plus `chrome_px` of frame, padding and drop-down button.
constexpr int field_px(int char_px, int chars, int chrome_px) noexcept {
    return std::max(0, char_px) * std::max(1, chars) + std::max(0, chrome_px);
}

/// A cell spanning `span` columns (at least one) of `field` px wide fields with `gap_px`
/// between neighbouring columns.
constexpr int span_px(int field, int span, int gap_px) noexcept {
    const int s = std::max(1, span);
    return std::max(0, field) * s + std::max(0, gap_px) * (s - 1);
}

/// A form of `columns` fields (at least one) with `pad_px` of padding on each side.
constexpr int form_px(int columns, int field, int gap_px, int pad_px) noexcept {
    return span_px(field, columns, gap_px) + 2 * std::max(0, pad_px);
}

/// How many items of `item_px` (with `gap_px` between neighbours) fit in `available_px`:
/// never fewer than one (a too-narrow area still shows one per row), never more than
/// `max_columns` (when that is positive).
constexpr int columns_for(int available_px, int item_px, int gap_px, int max_columns) noexcept {
    const int cap = max_columns > 0 ? max_columns : 1 << 20;
    if (item_px <= 0)
        return max_columns > 0 ? max_columns : 1;
    const int g = std::max(0, gap_px);
    const int n = available_px <= 0 ? 1 : (available_px + g) / (item_px + g);
    return std::clamp(n, 1, cap);
}

/// The responsive breakpoint: true when a lead column of `lead_px` and a trailing area of at
/// least `trail_min_px` fit side by side in `available_px` with `gap_px` between them.
constexpr bool side_by_side(int available_px, int lead_px, int trail_min_px, int gap_px) noexcept {
    return available_px >= std::max(0, lead_px) + std::max(0, gap_px) + std::max(0, trail_min_px);
}

} // namespace fincept::ui::form_grid
