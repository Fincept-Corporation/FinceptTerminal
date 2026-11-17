#pragma once
// SearchRow — one line of the search popup (heading, result, loading, "more", hint,
// Ask AI) and the pure key diff the model uses to update without a reset (no
// flicker, the selection stays on the same row). Qt Core only; tests/tst_search_model.

#include "core/command/CommandRank.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>

namespace fincept::ui::search {

struct SearchRow {
    enum class Kind { Heading, Item, Loading, More, Hint, Ask };
    Kind kind = Kind::Item;
    QString key;        ///< unique in the list ("h:screen", "i:screen:markets", "l:instrument", "m:action")
    QString group;      ///< command::groups id ("recent" / "pinned" / "suggested" for the empty box)
    QString title;      ///< primary text (heading: the group name)
    QString context;    ///< muted text before the title ("Backtesting")
    QString lead;       ///< bold code before the title (ticker)
    QString subtitle;   ///< what Enter does, in plain words
    QString chip;       ///< type chip ("STOCK", "TAB", "ACTION")
    QString hint;       ///< right-aligned hint (key, "Tab ▸")
    QList<command::rank::Span> highlight; ///< in `title` (offset by nothing: lead and context are separate)
    QStringList actions; ///< labels of secondary actions (hover buttons / Tab menu), index = action index
    QString agent_risk; ///< kind for agents (model role kAgentRiskRole)
    bool pinned = false;
    int count = 0;      ///< heading: rows found
    QString symbol;       ///< instrument rows: the market-data symbol (drag to a panel / the pins strip)
    QString symbol_class; ///< its SymbolRef asset class
    bool selectable() const { return kind == Kind::Item || kind == Kind::More || kind == Kind::Ask || kind == Kind::Hint; }

    bool same_content(const SearchRow& o) const {
        return kind == o.kind && key == o.key && group == o.group && title == o.title && context == o.context &&
               lead == o.lead && subtitle == o.subtitle && chip == o.chip && hint == o.hint && highlight == o.highlight &&
               actions == o.actions && agent_risk == o.agent_risk && pinned == o.pinned && count == o.count &&
               symbol == o.symbol && symbol_class == o.symbol_class;
    }
};

/// One step that turns the old key list into the new one (applied in order).
struct DiffOp {
    enum class Type { Remove, Insert };
    Type type = Type::Remove;
    int first = 0;   ///< row index in the list as it is when the op runs
    int count = 0;
    int source = 0;  ///< Insert: index of the first inserted row in the NEW list
};

/// Removes (bottom-up) then inserts (top-down) that keep the longest common subsequence of keys in place.
/// Pure; O(n*m) on lists of a few dozen rows.
inline QList<DiffOp> diff_keys(const QStringList& old_keys, const QStringList& new_keys) {
    const int n = static_cast<int>(old_keys.size());
    const int m = static_cast<int>(new_keys.size());
    // LCS table (lengths of the suffixes).
    QList<QList<int>> lcs(n + 1, QList<int>(m + 1, 0));
    for (int i = n - 1; i >= 0; --i)
        for (int j = m - 1; j >= 0; --j)
            lcs[i][j] = old_keys[i] == new_keys[j] ? lcs[i + 1][j + 1] + 1 : std::max(lcs[i + 1][j], lcs[i][j + 1]);
    QList<bool> keep_old(n, false);
    QList<bool> keep_new(m, false);
    for (int i = 0, j = 0; i < n && j < m;) {
        if (old_keys[i] == new_keys[j]) {
            keep_old[i] = true;
            keep_new[j] = true;
            ++i;
            ++j;
        } else if (lcs[i + 1][j] >= lcs[i][j + 1]) {
            ++i;
        } else {
            ++j;
        }
    }
    QList<DiffOp> ops;
    // Removals, bottom-up so earlier indices stay valid.
    for (int i = n - 1; i >= 0;) {
        if (keep_old[i]) {
            --i;
            continue;
        }
        int last = i;
        while (i >= 0 && !keep_old[i])
            --i;
        ops.append({DiffOp::Type::Remove, i + 1, last - i, 0});
    }
    // Insertions, top-down: after the removals the list holds exactly the kept rows in order, so the
    // insert position of new row j is j itself once every earlier new row is in.
    for (int j = 0; j < m;) {
        if (keep_new[j]) {
            ++j;
            continue;
        }
        const int first = j;
        while (j < m && !keep_new[j])
            ++j;
        ops.append({DiffOp::Type::Insert, first, j - first, first});
    }
    return ops;
}

} // namespace fincept::ui::search
