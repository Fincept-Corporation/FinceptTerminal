// ResultStore.cpp — Overflow store + size shaper for oversized MCP tool results.

#include "mcp/ResultStore.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace fincept::mcp {

static constexpr const char* TAG = "ResultStore";

static int compact_size(const QJsonValue& v) {
    // QJsonDocument only wraps objects/arrays, so scalars are measured inside a
    // throwaway array and the two bracket bytes discounted.
    if (v.isObject())
        return static_cast<int>(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact).size());
    if (v.isArray())
        return static_cast<int>(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact).size());
    QJsonArray wrap;
    wrap.append(v);
    return std::max(0, static_cast<int>(QJsonDocument(wrap).toJson(QJsonDocument::Compact).size()) - 2);
}

static QByteArray to_compact(const QJsonValue& v) {
    if (v.isObject())
        return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
    if (v.isArray())
        return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    QJsonArray wrap;
    wrap.append(v);
    QByteArray b = QJsonDocument(wrap).toJson(QJsonDocument::Compact);
    return b.mid(1, b.size() - 2); // strip the wrapping brackets
}

// ============================================================================
// Shaping
// ============================================================================

// One shaping pass at a fixed aggressiveness. Recurses the whole value; callers
// re-run it with tighter limits until the serialised size fits.
static QJsonValue clamp_value(const QJsonValue& v, int max_items, int max_chars, ShrinkStats& st) {
    if (v.isString()) {
        const QString s = v.toString();
        if (static_cast<int>(s.size()) <= max_chars)
            return v;
        ++st.strings_clipped;
        return QJsonValue(s.left(max_chars) + QStringLiteral("…[+%1 chars]").arg(s.size() - max_chars));
    }

    if (v.isArray()) {
        const QJsonArray in = v.toArray();
        QJsonArray out;
        const int keep = std::min(max_items, static_cast<int>(in.size()));
        for (int i = 0; i < keep; ++i)
            out.append(clamp_value(in.at(i), max_items, max_chars, st));
        if (in.size() > keep) {
            ++st.arrays_trimmed;
            // A trailing marker keeps the omission visible in-place. Without it
            // the model reads a 5-item array as complete and reasons off a
            // truncated dataset — a silent-wrong-answer bug, not a cosmetic one.
            out.append(QJsonValue(QStringLiteral("…[%1 more items omitted — use result_fetch for the full payload]")
                                      .arg(in.size() - keep)));
        }
        return out;
    }

    if (v.isObject()) {
        const QJsonObject in = v.toObject();
        QJsonObject out;
        for (auto it = in.constBegin(); it != in.constEnd(); ++it)
            out.insert(it.key(), clamp_value(it.value(), max_items, max_chars, st));
        return out;
    }

    return v; // scalars pass through
}

ShrinkStats shrink_json(QJsonValue& value, int budget_bytes) {
    ShrinkStats st;
    st.original_bytes = compact_size(value);
    st.final_bytes = st.original_bytes;
    if (budget_bytes <= 0 || st.original_bytes <= budget_bytes)
        return st;

    // Progressively harsher passes. Each is a full re-walk, but this only runs
    // on payloads already known to be over budget, and the early passes cut the
    // bulk of the common cases (a long array of rows) on the first try.
    struct Pass {
        int max_items;
        int max_chars;
    };
    static constexpr Pass kPasses[] = {
        {50, 2000}, {20, 1000}, {10, 400}, {5, 200}, {2, 120}, {1, 80},
    };

    for (const Pass& p : kPasses) {
        ShrinkStats attempt;
        attempt.original_bytes = st.original_bytes;
        QJsonValue candidate = clamp_value(value, p.max_items, p.max_chars, attempt);
        const int size = compact_size(candidate);
        if (size <= budget_bytes) {
            value = candidate;
            attempt.truncated = true;
            attempt.final_bytes = size;
            return attempt;
        }
    }

    // Nothing fit — the payload is wide rather than deep (thousands of scalar
    // keys). Hand back a stub; the full text lives in the ResultStore.
    QJsonObject stub{
        {"__truncated", true},
        {"__original_bytes", st.original_bytes},
        {"__note", QStringLiteral("Payload too large to preview — use result_fetch to page through it.")},
    };
    value = stub;
    st.truncated = true;
    st.final_bytes = compact_size(value);
    return st;
}

// ============================================================================
// Store
// ============================================================================

ResultStore& ResultStore::instance() {
    static ResultStore s;
    return s;
}

QString ResultStore::put(const QString& tool, const QJsonValue& payload) {
    QMutexLocker lock(&mutex_);
    sweep_locked();

    const QString id = QStringLiteral("res_%1").arg(++next_id_, 6, 16, QLatin1Char('0'));
    Entry e;
    e.tool = tool;
    e.text = to_compact(payload);
    e.stored_ms = QDateTime::currentMSecsSinceEpoch();
    entries_.insert(id, std::move(e));
    return id;
}

ResultStore::Page ResultStore::page(const QString& id, int offset, int limit) const {
    QMutexLocker lock(&mutex_);
    auto it = entries_.constFind(id);
    if (it == entries_.constEnd())
        return {};

    Page p;
    p.found = true;
    p.tool = it->tool;
    p.total_bytes = static_cast<int>(it->text.size());
    p.offset = std::clamp(offset, 0, p.total_bytes);
    const int take = std::clamp(limit, 1, p.total_bytes - p.offset);
    p.text = QString::fromUtf8(it->text.mid(p.offset, take));
    p.returned = take;
    p.has_more = p.offset + take < p.total_bytes;
    return p;
}

void ResultStore::clear() {
    QMutexLocker lock(&mutex_);
    entries_.clear();
}

void ResultStore::sweep_locked() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 total = 0;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (now - it->stored_ms > kTtlMs) {
            it = entries_.erase(it);
        } else {
            total += it->text.size();
            ++it;
        }
    }

    if (entries_.size() <= kMaxEntries && total <= kMaxTotalBytes)
        return;

    // Over a cap — drop oldest first until both budgets are satisfied.
    std::vector<QPair<qint64, QString>> by_age;
    by_age.reserve(static_cast<std::size_t>(entries_.size()));
    for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it)
        by_age.push_back({it->stored_ms, it.key()});
    std::sort(by_age.begin(), by_age.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& entry : by_age) {
        if (entries_.size() <= kMaxEntries && total <= kMaxTotalBytes)
            break;
        auto it = entries_.constFind(entry.second);
        if (it == entries_.constEnd())
            continue;
        total -= it->text.size();
        entries_.remove(entry.second);
    }
    LOG_DEBUG(TAG, QString("Swept overflow store — %1 entries, %2 KB retained")
                       .arg(entries_.size())
                       .arg(total / 1024));
}

} // namespace fincept::mcp
