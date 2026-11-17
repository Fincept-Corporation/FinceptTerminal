// CommandUsage.cpp — see CommandUsage.h.
#include "core/command/CommandUsage.h"

#include <QJsonArray>

#include <algorithm>
#include <functional>

namespace fincept::command {

int CommandUsage::index_of(const QString& id) const {
    for (int i = 0; i < entries_.size(); ++i)
        if (entries_[i].id == id)
            return i;
    return -1;
}

bool CommandUsage::contains(const QString& id) const {
    return index_of(id) >= 0;
}

void CommandUsage::record(const Entry& row, qint64 now_ms) {
    if (row.id.isEmpty())
        return;
    const int i = index_of(row.id);
    if (i < 0) {
        Entry e = row;
        e.count = 1;
        e.last_used_ms = now_ms;
        e.pinned = false;
        e.pinned_ms = 0;
        entries_.append(e);
    } else {
        Entry& e = entries_[i];
        e.count = std::min(e.count + 1, 1000000);
        e.last_used_ms = now_ms;
        if (!row.title.isEmpty())
            e.title = row.title;
        if (!row.subtitle.isEmpty())
            e.subtitle = row.subtitle;
        if (!row.group.isEmpty())
            e.group = row.group;
        if (!row.payload.isEmpty())
            e.payload = row.payload;
    }
    prune();
}

bool CommandUsage::set_pinned(const Entry& row, bool pinned, qint64 now_ms) {
    if (row.id.isEmpty())
        return false;
    int i = index_of(row.id);
    if (pinned) {
        if (i >= 0 && entries_[i].pinned)
            return true;
        const int pins = static_cast<int>(std::count_if(entries_.begin(), entries_.end(), [](const Entry& e) { return e.pinned; }));
        if (pins >= kMaxPins)
            return false;
        if (i < 0) {
            Entry e = row;
            e.count = 0;
            e.last_used_ms = 0;
            entries_.append(e);
            i = static_cast<int>(entries_.size()) - 1;
        }
        entries_[i].pinned = true;
        entries_[i].pinned_ms = now_ms;
        if (!row.title.isEmpty())
            entries_[i].title = row.title;
        if (!row.payload.isEmpty())
            entries_[i].payload = row.payload;
        return true;
    }
    if (i < 0)
        return true;
    entries_[i].pinned = false;
    entries_[i].pinned_ms = 0;
    if (entries_[i].count == 0)
        entries_.removeAt(i);
    prune();
    return true;
}

bool CommandUsage::is_pinned(const QString& id) const {
    const int i = index_of(id);
    return i >= 0 && entries_[i].pinned;
}

void CommandUsage::forget(const QString& id) {
    const int i = index_of(id);
    if (i >= 0)
        entries_.removeAt(i);
}

void CommandUsage::clear_history() {
    QList<Entry> kept;
    for (Entry e : entries_) {
        if (!e.pinned)
            continue;
        e.count = 0;
        e.last_used_ms = 0;
        kept.append(e);
    }
    entries_ = kept;
}

rank::Usage CommandUsage::usage(const QString& id) const {
    const int i = index_of(id);
    if (i < 0)
        return {};
    return {entries_[i].count, entries_[i].last_used_ms, entries_[i].pinned};
}

QList<CommandUsage::Entry> CommandUsage::recent(int limit) const {
    QList<Entry> out;
    for (const Entry& e : entries_)
        if (!e.pinned && e.count > 0)
            out.append(e);
    std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.last_used_ms > b.last_used_ms; });
    if (limit >= 0 && out.size() > limit)
        out.resize(limit);
    return out;
}

QList<CommandUsage::Entry> CommandUsage::pinned() const {
    QList<Entry> out;
    for (const Entry& e : entries_)
        if (e.pinned)
            out.append(e);
    std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.pinned_ms < b.pinned_ms; });
    return out;
}

void CommandUsage::prune() {
    if (entries_.size() <= kMaxEntries)
        return;
    // Least recently used unpinned rows go first.
    QList<int> unpinned;
    for (int i = 0; i < entries_.size(); ++i)
        if (!entries_[i].pinned)
            unpinned.append(i);
    std::stable_sort(unpinned.begin(), unpinned.end(),
                     [this](int a, int b) { return entries_[a].last_used_ms < entries_[b].last_used_ms; });
    int excess = static_cast<int>(entries_.size()) - kMaxEntries;
    QList<int> drop;
    for (int i = 0; i < unpinned.size() && excess > 0; ++i, --excess)
        drop.append(unpinned[i]);
    std::sort(drop.begin(), drop.end(), std::greater<int>());
    for (int i : drop)
        entries_.removeAt(i);
}

QJsonObject CommandUsage::to_json() const {
    QJsonArray rows;
    for (const Entry& e : entries_) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("t"), e.title);
        o.insert(QStringLiteral("g"), e.group);
        if (!e.subtitle.isEmpty())
            o.insert(QStringLiteral("s"), e.subtitle);
        if (!e.payload.isEmpty())
            o.insert(QStringLiteral("p"), QJsonObject::fromVariantMap(e.payload));
        o.insert(QStringLiteral("n"), e.count);
        o.insert(QStringLiteral("at"), static_cast<double>(e.last_used_ms));
        if (e.pinned) {
            o.insert(QStringLiteral("pin"), true);
            o.insert(QStringLiteral("pin_at"), static_cast<double>(e.pinned_ms));
        }
        rows.append(o);
    }
    QJsonObject doc;
    doc.insert(QStringLiteral("v"), kFormatVersion);
    doc.insert(QStringLiteral("rows"), rows);
    return doc;
}

CommandUsage CommandUsage::from_json(const QJsonObject& doc) {
    CommandUsage u;
    if (doc.value(QStringLiteral("v")).toInt() != kFormatVersion)
        return u;
    for (const QJsonValue& v : doc.value(QStringLiteral("rows")).toArray()) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.id = o.value(QStringLiteral("id")).toString();
        if (e.id.isEmpty() || u.index_of(e.id) >= 0)
            continue;
        e.title = o.value(QStringLiteral("t")).toString();
        e.group = o.value(QStringLiteral("g")).toString();
        e.subtitle = o.value(QStringLiteral("s")).toString();
        e.payload = o.value(QStringLiteral("p")).toObject().toVariantMap();
        e.count = std::max(0, o.value(QStringLiteral("n")).toInt());
        e.last_used_ms = static_cast<qint64>(o.value(QStringLiteral("at")).toDouble());
        e.pinned = o.value(QStringLiteral("pin")).toBool();
        e.pinned_ms = static_cast<qint64>(o.value(QStringLiteral("pin_at")).toDouble());
        if (e.title.isEmpty() || (!e.pinned && e.count == 0))
            continue;
        u.entries_.append(e);
    }
    u.prune();
    return u;
}

} // namespace fincept::command
