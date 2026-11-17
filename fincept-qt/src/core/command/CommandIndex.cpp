// CommandIndex.cpp — see CommandIndex.h.
#include "core/command/CommandIndex.h"

#include <algorithm>

namespace fincept::command {

QStringList CommandIndex::codes_of(const CommandItem& item) {
    QStringList codes = item.aliases;
    if (item.group == QLatin1String(groups::kScreen) && item.section.isEmpty() && !item.screen_id.isEmpty())
        codes.append(item.screen_id);
    return codes;
}

void CommandIndex::rebuild(const QList<CommandItem>& items) {
    entries_.clear();
    by_id_.clear();
    entries_.reserve(items.size());
    for (const CommandItem& item : items) {
        if (by_id_.contains(item.id))
            continue;
        rank::Fields f;
        f.title = item.title;
        f.codes = codes_of(item);
        f.keywords = item.keywords;
        if (!item.context.isEmpty())
            f.keywords.append(item.context); // "backtesting runs" finds the Runs tab of Backtesting
        f.description = item.subtitle;
        by_id_.insert(item.id, static_cast<int>(entries_.size()));
        entries_.append({item, rank::prepare(f)});
    }
}

QList<CommandIndex::Hit> CommandIndex::query(const rank::Query& q, const CommandUsage& usage, qint64 now_ms,
                                             const QSet<QString>& groups, int limit) const {
    QList<Hit> hits;
    if (q.empty())
        return hits;
    for (int i = 0; i < entries_.size(); ++i) {
        const Entry& e = entries_[i];
        if (!groups.isEmpty() && !groups.contains(e.item.group))
            continue;
        rank::Match m = rank::match(q, e.prepared);
        if (!m.matched()) {
            // A sub-tab is also found by "<screen> <tab>" ("backtesting runs"): the context word plus the title.
            if (e.item.context.isEmpty() || q.words.size() < 2)
                continue;
            rank::Fields combined;
            combined.title = e.item.context + QLatin1Char(' ') + e.item.title;
            const rank::Match cm = rank::match(q, rank::prepare(combined));
            if (cm.tier < rank::Tier::WordsInOrder)
                continue;
            m.tier = rank::Tier::WordsInOrder;
            m.quality = cm.quality;
        }
        const int boost = rank::usage_boost(usage.usage(e.item.id), now_ms);
        hits.append({i, m, rank::score(m, boost, e.item.priority)});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
    if (limit >= 0 && hits.size() > limit)
        hits.resize(limit);
    return hits;
}

QString CommandIndex::screen_for(const QString& token) const {
    const QString t = token.trimmed();
    if (t.isEmpty())
        return {};
    const rank::Query q = rank::prepare_query(t);
    if (q.empty())
        return {};
    QString best;
    qint64 best_score = 0;
    for (const Entry& e : entries_) {
        if (e.item.group != QLatin1String(groups::kScreen) || !e.item.section.isEmpty() || e.item.screen_id.isEmpty())
            continue;
        if (e.item.screen_id.compare(t, Qt::CaseInsensitive) == 0)
            return e.item.screen_id;
        const rank::Match m = rank::match(q, e.prepared);
        if (m.tier < rank::Tier::WordsAnyOrder)
            continue; // never a keyword / typo guess for a dock command
        const qint64 s = rank::score(m, 0, e.item.priority);
        if (s > best_score) {
            best_score = s;
            best = e.item.screen_id;
        }
    }
    return best;
}

namespace layout {

namespace {

qint64 rl_best(const GroupInput& g) {
    qint64 best = 0;
    for (const Candidate& c : g.rows)
        best = std::max(best, c.score);
    if (g.rows.isEmpty() && g.loading)
        best = g.predicted;
    return best;
}

int rl_order_index(const QStringList& order, const QString& group) {
    const int i = static_cast<int>(order.indexOf(group));
    return i < 0 ? static_cast<int>(order.size()) : i;
}

} // namespace

QList<Section> arrange(const QList<GroupInput>& groups, const QStringList& default_order, const Previous& previous) {
    struct Pending {
        const GroupInput* in = nullptr;
        qint64 best = 0;
        int prev_slot = -1;
    };
    QList<Pending> shown;
    for (const GroupInput& g : groups) {
        if (g.rows.isEmpty() && !g.loading)
            continue;
        Pending p;
        p.in = &g;
        p.best = rl_best(g);
        if (previous.same_query)
            p.prev_slot = static_cast<int>(previous.group_order.indexOf(g.group));
        shown.append(p);
    }
    std::stable_sort(shown.begin(), shown.end(), [&](const Pending& a, const Pending& b) {
        if (previous.same_query) {
            const bool a_prev = a.prev_slot >= 0;
            const bool b_prev = b.prev_slot >= 0;
            if (a_prev != b_prev)
                return a_prev; // groups already on screen first, in their order
            if (a_prev && b_prev)
                return a.prev_slot < b.prev_slot;
        }
        if (a.best != b.best)
            return a.best > b.best;
        return rl_order_index(default_order, a.in->group) < rl_order_index(default_order, b.in->group);
    });

    QList<Section> out;
    for (const Pending& p : shown) {
        const GroupInput& g = *p.in;
        QList<Candidate> rows = g.rows;
        std::stable_sort(rows.begin(), rows.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        QStringList ordered;
        ordered.reserve(rows.size());
        if (previous.same_query && previous.keys.contains(g.group)) {
            // Rows already shown keep their places; newcomers follow, best first.
            QSet<QString> present;
            for (const Candidate& c : rows)
                present.insert(c.key);
            for (const QString& k : previous.keys.value(g.group))
                if (present.contains(k) && !ordered.contains(k))
                    ordered.append(k);
            for (const Candidate& c : rows)
                if (!ordered.contains(c.key))
                    ordered.append(c.key);
        } else {
            for (const Candidate& c : rows)
                if (!ordered.contains(c.key))
                    ordered.append(c.key);
        }
        Section s;
        s.group = g.group;
        s.total = static_cast<int>(ordered.size());
        s.loading = g.loading;
        const int cap = g.expanded ? static_cast<int>(ordered.size()) : std::max(1, g.cap);
        s.keys = ordered.mid(0, cap);
        s.more = s.total > s.keys.size();
        out.append(s);
    }
    return out;
}

} // namespace layout

} // namespace fincept::command
