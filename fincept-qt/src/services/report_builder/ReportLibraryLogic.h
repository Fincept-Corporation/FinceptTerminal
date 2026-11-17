#pragma once
// ReportLibraryLogic.h — what the report library does to a document, without the
// database: the list summary, rename, duplicate and "append blocks to a stored report"
// (the send-to-report queue for a report that is not open). ReportLibrary runs these on
// its worker around the SQL; tests/tst_report_builder_core.cpp tests them directly.
// Header-only, Qt Core only.

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"

#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace fincept::services {

/// One library row, summarised on the worker (the GUI never parses report JSON).
struct LibraryEntry {
    qint64 id = 0;
    QString title;
    QString created_at; ///< SQLite UTC text
    QString updated_at;
    int blocks = 0;
    int bytes = 0;
    int unseen = 0;      ///< blocks added by other tabs, not looked at yet
    QString theme;       ///< report theme name (paper / ink for the preview)
    QStringList outline; ///< the first headings
    QStringList kinds;   ///< component types in order (capped) — the wireframe preview
    bool bound = false;  ///< has data blocks that refresh
};

/// A template, built-in or the user's own.
struct TemplateEntry {
    QString id;          ///< built-in: its name; user: "user:<row id>"
    QString name;
    QString description;
    QString category;
    QString needs;       ///< "symbol" | "portfolio" | "" — what the template asks for
    bool user = false;
    bool live = false;   ///< made of data blocks that fill themselves
    QString theme;
    QStringList kinds;
};

namespace library_logic {

inline constexpr int kKindsCap = 60;

inline LibraryEntry summarise(qint64 id, const QString& title, const report::ReportDocument& doc) {
    LibraryEntry e;
    e.id = id;
    e.title = title.isEmpty() ? doc.metadata.title : title;
    e.blocks = static_cast<int>(doc.components.size());
    e.theme = doc.theme.name;
    QSet<int> present;
    for (const auto& c : doc.components) {
        present.insert(c.id);
        if (e.kinds.size() < kKindsCap)
            e.kinds << c.type;
        if (c.type == QLatin1String("heading") && e.outline.size() < 4 && !c.content.trimmed().isEmpty())
            e.outline << c.content.trimmed();
        if (!e.bound && report::is_bound(c))
            e.bound = true;
    }
    for (int uid : doc.unseen_ids)
        if (present.contains(uid))
            ++e.unseen;
    return e;
}

/// Rename: the row title AND the document's own title stay the same.
inline void rename(report::ReportDocument& doc, const QString& title) {
    doc.metadata.title = title;
}

/// Duplicate: a copy under `title`, without the original's "new blocks" notice.
inline report::ReportDocument duplicate(const report::ReportDocument& doc, const QString& title) {
    report::ReportDocument d = doc;
    d.metadata.title = title;
    d.unseen_ids.clear();
    d.unseen_sources.clear();
    return d;
}

// ── Revisions (stale-save detection) ─────────────────────────────────────────

/// A write based on `expected` must not overwrite a row now at `stored` (unless forced).
inline bool revision_conflicts(const report::ReportRevision& stored, const report::ReportRevision& expected,
                               bool force) {
    return !force && stored != expected;
}

/// The revision a write stamps: one past the newer of what it was based on and what is
/// stored (a forced write after a conflict still moves forward).
inline report::ReportRevision next_revision(const report::ReportRevision& expected,
                                            const report::ReportRevision& stored, const QString& by) {
    report::ReportRevision r;
    r.n = std::max(expected.n, stored.n) + 1;
    r.by = by;
    return r;
}

inline void stamp_revision(QJsonObject& root, const report::ReportRevision& r) {
    root.insert(QStringLiteral("revision"), QJsonObject{{QStringLiteral("n"), r.n}, {QStringLiteral("by"), r.by}});
}

/// Append `blocks` to a stored report: fresh ids from its own counter, listed as unseen
/// (the "N new blocks" notice when it is next opened). Returns the new ids in order.
inline QVector<int> append_blocks(report::ReportDocument& doc, const QVector<report::ReportComponent>& blocks,
                                  const QString& source_label) {
    QVector<int> ids;
    for (report::ReportComponent c : blocks) {
        c.id = doc.allocate_id();
        doc.components.append(c);
        ids.append(c.id);
        doc.unseen_ids.append(c.id);
    }
    if (!ids.isEmpty() && !source_label.isEmpty() && !doc.unseen_sources.contains(source_label))
        doc.unseen_sources.append(source_label);
    return ids;
}

} // namespace library_logic

} // namespace fincept::services
