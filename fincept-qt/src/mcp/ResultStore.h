#pragma once
// ResultStore.h — Overflow store + size shaper for oversized MCP tool results.
//
// Problem this solves
// -------------------
// The structured tool loop used to splice every tool result into the
// conversation verbatim:
//
//     loop_messages.append({role:"tool", content: QJsonDocument(tr.to_json())});
//
// with no size cap at all. `edgar_get_filing` returns a whole SEC filing;
// several M&A / news / portfolio tools return unbounded arrays. Worse, the loop
// re-posts the entire message array on every subsequent round, so one 60 KB
// result costs 60 KB of input tokens per round for the rest of the turn — up to
// 200 rounds (`max_tool_rounds` clamp).
//
// The fix has two halves, both here:
//
//   1. `shrink_json` — structure-aware shaping. Rather than clipping the JSON
//      text mid-token (which yields invalid JSON the model then has to guess
//      at), it walks the value and trims arrays / long strings until the
//      serialised form fits a byte budget, leaving valid JSON that keeps its
//      shape. The model can still see what the result *looks* like.
//
//   2. `ResultStore` — the trimmed-away remainder isn't lost. The full payload
//      is parked here under a short id and the envelope tells the model it can
//      page through it with `result_fetch(result_id, offset, limit)`. Nothing
//      is silently dropped; the model pays for detail only when it asks.
//
// Retention is deliberately short and bounded (see kTtlMs / kMaxEntries /
// kMaxTotalBytes): this is a within-turn scratch buffer, not a cache.
//
// Thread safety: all methods callable from any thread; one QMutex guards the map.

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QMutex>
#include <QString>

namespace fincept::mcp {

/// What `shrink_json` had to do to fit the budget. `truncated == false` means
/// the value was already small enough and is untouched.
struct ShrinkStats {
    bool truncated = false;
    int original_bytes = 0;
    int final_bytes = 0;
    int arrays_trimmed = 0;
    int strings_clipped = 0;
};

/// Shrink `value` in place until its compact JSON serialisation fits
/// `budget_bytes`. Arrays lose their tail (a trailing marker string records how
/// many items went); long strings are clipped with a "+N chars" suffix. The
/// result is always valid JSON.
///
/// If even the harshest pass can't fit the budget (e.g. one enormous object with
/// thousands of scalar keys), the value is replaced with a stub object noting
/// the original size — the caller is expected to have stashed the full payload
/// in a ResultStore first.
ShrinkStats shrink_json(QJsonValue& value, int budget_bytes);

class ResultStore {
  public:
    static ResultStore& instance();

    /// Park a full payload. Returns a short id ("res_0001a3") for the model to
    /// quote back to `result_fetch`.
    QString put(const QString& tool, const QJsonValue& payload);

    struct Page {
        bool found = false;
        QString tool;
        QString text;         // compact-JSON slice [offset, offset+limit)
        int offset = 0;
        int returned = 0;
        int total_bytes = 0;
        bool has_more = false;
    };

    /// Page through a stored payload's compact JSON text. Slicing text rather
    /// than re-walking the structure keeps paging O(limit) and lets the model
    /// resume exactly where it stopped.
    Page page(const QString& id, int offset, int limit) const;

    void clear();

    ResultStore(const ResultStore&) = delete;
    ResultStore& operator=(const ResultStore&) = delete;

  private:
    ResultStore() = default;

    struct Entry {
        QString tool;
        QByteArray text; // compact JSON, serialised once at put()
        qint64 stored_ms = 0;
    };

    void sweep_locked();

    mutable QMutex mutex_;
    QHash<QString, Entry> entries_;
    quint64 next_id_ = 0;

    static constexpr qint64 kTtlMs = 15 * 60 * 1000;
    static constexpr int kMaxEntries = 24;
    static constexpr qint64 kMaxTotalBytes = 32 * 1024 * 1024;
};

} // namespace fincept::mcp
