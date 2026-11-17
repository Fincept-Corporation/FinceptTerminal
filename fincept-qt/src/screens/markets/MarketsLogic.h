#pragma once
// MarketsLogic — the pure rules of the Markets screen, kept free of widgets, the
// DataHub and settings so they can be unit-tested on their own (Qt Core +
// core::instrument only): symbol canonicalisation of saved panels, the
// row-state / staleness rules, retry wording classes, grid re-flow order, the
// drop slot of a panel dragged by its header, first-run panel heights, the
// pulse-ribbon tile count and column auto-hide.
//
// Header-only (inline): nothing here needs a translation unit of its own.

#include "core/instrument/SymbolInput.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::screens::markets_logic {

// ── Limits (spec 3.9) ────────────────────────────────────────────────────────
inline constexpr int kMaxSymbolsPerPanel = 60;
inline constexpr int kMaxSymbolsOnScreen = 300;
/// Quote / sparkline topics per hub request.
inline constexpr int kTopicsPerRequest = 40;
/// A row still loading this long after its stage started is reported as failed.
inline constexpr qint64 kWatchdogMs = 35'000;
/// A quote older than max(this, 2.5 x interval) is stale.
inline constexpr qint64 kStaleFloorMs = 150'000;

// ── Symbols ──────────────────────────────────────────────────────────────────

/// The market-data key a stored / typed symbol denotes: core::instrument::canonical_symbol,
/// or the trimmed upper-case text when that returns nothing (a bare name, an unknown
/// venue) — the original is kept rather than dropped.
inline QString canonical_or_kept(const QString& symbol) {
    const QString trimmed = symbol.trimmed();
    if (trimmed.isEmpty())
        return {};
    const QString c = core::instrument::canonical_symbol(trimmed);
    return c.isEmpty() ? trimmed.toUpper() : c;
}

/// Canonicalise a stored panel's symbols — the saved-layout migration (spec F5): a
/// symbol changes only when canonical_symbol() returns a different non-empty form;
/// anything it cannot place is kept exactly as stored. Empty entries are dropped and the
/// first occurrence of each symbol wins. `changed` (optional) reports whether the list differs.
inline QStringList canonical_panel_symbols(const QStringList& in, bool* changed = nullptr) {
    QStringList out;
    out.reserve(in.size());
    QSet<QString> seen;
    for (const QString& s : in) {
        if (s.trimmed().isEmpty())
            continue;
        const QString canon = core::instrument::canonical_symbol(s);
        const QString c = canon.isEmpty() ? s : canon;
        if (seen.contains(c))
            continue;
        seen.insert(c);
        out.append(c);
    }
    if (changed)
        *changed = (out != in);
    return out;
}

// ── Row state (spec 3.6) ─────────────────────────────────────────────────────

/// What one symbol's row shows.
enum class RowState {
    Loading,      ///< no quote yet, request in flight: "..."
    Ok,           ///< fresh quote, regular session
    Closed,       ///< fresh quote, market closed / pre / post: secondary colour, no flash
    Stale,        ///< quote held but too old: tertiary colour
    FailedOld,    ///< last refresh failed, the previous quote is still shown (as Stale + "!")
    FailedNoData, ///< failed and nothing to show: "-" + reason
};

/// Staleness threshold for an interval (0 = live, quotes refresh at the 30 s TTL).
inline qint64 stale_after_ms(int interval_ms) {
    return std::max<qint64>(kStaleFloorMs, static_cast<qint64>(2.5 * std::max(interval_ms, 30'000)));
}

/// Session class of a quote's market_state ("PRE", "REGULAR", "POST", "CLOSED", empty).
enum class Session { Unknown, Regular, Extended, Closed, Holiday };

inline Session session_from_state(const QString& market_state) {
    if (market_state == QLatin1String("REGULAR"))
        return Session::Regular;
    if (market_state == QLatin1String("PRE") || market_state == QLatin1String("POST"))
        return Session::Extended;
    if (market_state == QLatin1String("CLOSED"))
        return Session::Closed;
    return Session::Unknown;
}

/// The row state from what the store holds. `has_quote`: a priced quote is held;
/// `failed`: the last request for the symbol ended in an error and nothing newer arrived.
/// A symbol with neither is loading (every listed symbol is requested).
inline RowState row_state(bool has_quote, bool failed, Session session, qint64 now_ms, qint64 quote_ms,
                          int interval_ms) {
    if (!has_quote)
        return failed ? RowState::FailedNoData : RowState::Loading;
    if (failed)
        return RowState::FailedOld;
    // A closed market's last price stays current until the venue reopens: its quote is
    // not re-published every 30 s any more (MarketDataService keeps it until the next
    // session), so its age says nothing about staleness.
    if (session == Session::Closed || session == Session::Holiday)
        return RowState::Closed;
    if (quote_ms > 0 && now_ms - quote_ms > stale_after_ms(interval_ms))
        return RowState::Stale;
    if (session == Session::Extended)
        return RowState::Closed;
    return RowState::Ok;
}

/// How the hub will retry a failed quote topic, judged from the published reason text
/// (MarketDataService words them: rate limiting -> 1 minute; "not found" -> 15 minutes).
enum class RetryKind { Ladder, RateLimited, NotFound };

inline RetryKind retry_kind_for(const QString& reason) {
    const QString r = reason.toLower();
    if (r.contains(QLatin1String("rate limit")) || r.contains(QLatin1String("too many requests")))
        return RetryKind::RateLimited;
    if (r.contains(QLatin1String("not found")) || r.contains(QLatin1String("delisted")))
        return RetryKind::NotFound;
    return RetryKind::Ladder;
}

/// Minutes until the hub retries for `kind` (0 = the normal 2 s .. 5 min ladder).
inline int retry_minutes(RetryKind kind) {
    switch (kind) {
        case RetryKind::RateLimited:
            return 1;
        case RetryKind::NotFound:
            return 15;
        case RetryKind::Ladder:
            break;
    }
    return 0;
}

// ── Grid ─────────────────────────────────────────────────────────────────────

/// Panel ids in reading order (row by row, left to right) of a column layout given
/// as one id list per column, top to bottom.
inline QStringList reading_order(const QVector<QStringList>& columns) {
    QStringList out;
    int rows = 0;
    for (const auto& c : columns)
        rows = std::max(rows, static_cast<int>(c.size()));
    for (int r = 0; r < rows; ++r)
        for (const auto& c : columns)
            if (r < c.size())
                out.append(c.at(r));
    return out;
}

/// Re-flow ids (reading order) into `n` columns, filled row by row — the 2- and
/// 1-column layouts of narrow windows. The saved 3-column layout is not touched.
inline QVector<QStringList> reflow(const QStringList& reading, int n) {
    n = std::max(1, n);
    QVector<QStringList> cols(n);
    for (int i = 0; i < reading.size(); ++i)
        cols[i % n].append(reading.at(i));
    return cols;
}

/// Columns of panels the grid shows at `grid_width` px: 3 from 960, 2 from 640, else 1.
inline int grid_columns_for_width(int grid_width, int saved_columns = 3) {
    const int want = grid_width >= 960 ? 3 : (grid_width >= 640 ? 2 : 1);
    return std::max(1, std::min(want, std::max(1, saved_columns)));
}

// ── Panel drag-to-reorder (spec 3.4 NICE: drag a panel by its header) ───────

/// The saved layout after moving panel `id` to slot (`column`, `index`). `index` is
/// the insertion slot as the grid shows it BEFORE the move — counted with `id` still in
/// place (0 = top of the column, size = bottom) — so a same-column move down lands
/// where the drop bar was drawn. `column` / `index` are clamped; an unknown id returns
/// `columns` unchanged. `changed` (optional) reports whether the order differs.
inline QVector<QStringList> move_panel_to_slot(const QVector<QStringList>& columns, const QString& id, int column,
                                               int index, bool* changed = nullptr) {
    if (changed)
        *changed = false;
    QVector<QStringList> out = columns;
    int from_c = -1;
    int from_i = -1;
    for (int c = 0; c < out.size() && from_c < 0; ++c) {
        const int k = static_cast<int>(out[c].indexOf(id));
        if (k >= 0) {
            from_c = c;
            from_i = k;
        }
    }
    if (from_c < 0 || out.isEmpty())
        return out;
    column = std::clamp(column, 0, static_cast<int>(out.size()) - 1);
    index = std::clamp(index, 0, static_cast<int>(out[column].size()));
    out[from_c].removeAt(from_i);
    if (column == from_c && index > from_i)
        --index; // the slot was counted with the panel still above it
    out[column].insert(index, id);
    if (changed)
        *changed = (out != columns);
    return out;
}

/// One column of the grid as it is shown (3 saved columns, or the 2 / 1 re-flowed ones):
/// its horizontal extent and its panels top to bottom (grid coordinates).
struct PanelDropColumn {
    struct Panel {
        QString id;
        int top = 0;
        int bottom = 0;
    };
    int left = 0;
    int right = 0;
    /// An EMPTY column of the saved 3-column layout: its saved index (a drop target of
    /// its own); -1 otherwise (an empty re-flowed column is not a target).
    int saved_column = -1;
    QVector<Panel> panels;
};

/// Where a dragged panel would land.
struct PanelDropTarget {
    bool valid = false;
    int column = -1;       ///< index into the shown columns
    QString anchor;        ///< next to this panel …
    bool after = false;    ///< … below it (else above it)
    int empty_column = -1; ///< or into this empty saved column (anchor empty)
};

/// The drop target at grid point (`x`, `y`): the shown column under x — or the nearest
/// one when x is on a handle or beside the grid — then above the first panel whose
/// vertical middle is below y, else below the column's last panel; an empty saved
/// column takes the drop as a whole. Invalid when no column can take it.
inline PanelDropTarget panel_drop_target_at(const QVector<PanelDropColumn>& columns, int x, int y) {
    PanelDropTarget t;
    int best = -1;
    int best_d = 0;
    for (int c = 0; c < columns.size(); ++c) {
        const PanelDropColumn& col = columns[c];
        if (col.panels.isEmpty() && col.saved_column < 0)
            continue;
        const int d = x < col.left ? col.left - x : (x > col.right ? x - col.right : 0);
        if (best < 0 || d < best_d) {
            best = c;
            best_d = d;
        }
    }
    if (best < 0)
        return t;
    const PanelDropColumn& col = columns[best];
    t.valid = true;
    t.column = best;
    if (col.panels.isEmpty()) {
        t.empty_column = col.saved_column;
        return t;
    }
    for (const auto& p : col.panels) {
        if (y < (p.top + p.bottom) / 2) {
            t.anchor = p.id;
            return t;
        }
    }
    t.anchor = col.panels.last().id;
    t.after = true;
    return t;
}

/// The saved-layout slot (`column`, `index`, counted before the move — the input of
/// move_panel_to_slot) of a drop target. Next to a panel it is that panel's own saved
/// slot (+1 below it): in the 2 / 1-column re-flow a drop beside a panel therefore lands
/// beside it in the saved 3-column layout too. An empty saved column: its end. False
/// when the target names nothing in `saved`.
inline bool saved_drop_slot(const QVector<QStringList>& saved, const PanelDropTarget& target, int* column, int* index) {
    if (!target.valid || !column || !index)
        return false;
    if (!target.anchor.isEmpty()) {
        for (int c = 0; c < saved.size(); ++c) {
            const int k = static_cast<int>(saved[c].indexOf(target.anchor));
            if (k >= 0) {
                *column = c;
                *index = k + (target.after ? 1 : 0);
                return true;
            }
        }
        return false;
    }
    if (target.empty_column >= 0 && target.empty_column < saved.size()) {
        *column = target.empty_column;
        *index = static_cast<int>(saved[target.empty_column].size());
        return true;
    }
    return false;
}

/// First-run vertical splitter sizes of one column: proportional to
/// min(symbol_count, 14) + 2, so 18-row panels get more height than 12-row ones.
inline QList<int> proportional_sizes(const QVector<int>& symbol_counts) {
    QList<int> out;
    for (int n : symbol_counts)
        out.append((std::min(std::max(n, 0), 14) + 2) * 100);
    return out;
}

/// Tiles the pulse ribbon shows at `width` px: as many of at least `min_tile` px
/// (plus `spacing`) as fit in the 8 px margins, clamped to 3..`total`.
inline int ribbon_tile_count(int width, int min_tile, int spacing, int total) {
    if (total <= 0)
        return 0;
    const int fit = (width - 16 + spacing) / std::max(1, min_tile + spacing);
    return std::clamp(fit, std::min(3, total), total);
}

/// Column auto-hide (spec 2.3 d). `visible` are the visible logical columns with their
/// fixed widths (the stretch column excluded), `rank` their auto-hide rank (lower hides
/// first; < 0 = never), `available` the viewport width, `stretch_min` the space the
/// stretch column needs. Returns the columns to hide, view-only.
inline QSet<int> auto_hidden_columns(const QVector<QPair<int, int>>& visible, const QHash<int, int>& rank,
                                     int available, int stretch_min) {
    QSet<int> hidden;
    int total = stretch_min;
    for (const auto& v : visible)
        total += v.second;
    if (total <= available)
        return hidden;
    QVector<QPair<int, int>> order; // (rank, column)
    for (const auto& v : visible) {
        const int r = rank.value(v.first, -1);
        if (r >= 0)
            order.append({r, v.first});
    }
    std::sort(order.begin(), order.end());
    QHash<int, int> width_of;
    for (const auto& v : visible)
        width_of.insert(v.first, v.second);
    for (const auto& o : order) {
        if (total <= available)
            break;
        hidden.insert(o.second);
        total -= width_of.value(o.second);
    }
    return hidden;
}

// ── Sorting ──────────────────────────────────────────────────────────────────

/// Order of two numeric sort keys where NaN means "missing" and `a_na` / `b_na` mark
/// "not applicable": real values first in the requested direction, then missing, then
/// n/a — in BOTH directions. Returns <0, 0, >0 for the ascending comparison the proxy
/// asks for (the proxy flips it for a descending sort, so the caller passes `descending`
/// to pin missing / n/a last either way).
inline int compare_keys(double a, bool a_na, double b, bool b_na, bool descending) {
    const int a_rank = a_na ? 2 : (std::isfinite(a) ? 0 : 1);
    const int b_rank = b_na ? 2 : (std::isfinite(b) ? 0 : 1);
    if (a_rank != b_rank) {
        const int c = a_rank < b_rank ? -1 : 1;
        return descending ? -c : c; // keep them last after the proxy reverses the order
    }
    if (a_rank != 0 || a == b)
        return 0;
    return a < b ? -1 : 1;
}

// ── Breadth ──────────────────────────────────────────────────────────────────

/// Direction of a change for breadth / colour: +1, -1 or 0 (exactly zero or not a number).
inline int direction_of(double change) {
    if (!std::isfinite(change) || change == 0.0)
        return 0;
    return change > 0 ? 1 : -1;
}

} // namespace fincept::screens::markets_logic
