// ReportTool.cpp — the `report` tool's logic over ReportToolBackend (see ReportTool.h).
// No service includes here: the real backend is in ReportToolBackend.cpp, tests use a fake.

#include "mcp/core/ReportTool.h"

#include "core/report/ReportBinding.h"
#include "core/report/ReportBlocks.h"
#include "core/report/ReportTableCodec.h"
#include "core/report/ReportVariables.h"

#include "mcp/server/CallApproval.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>

namespace fincept::mcp::core {

namespace rep = ::fincept::report;

namespace {

constexpr int kRtMaxListed = 50;
constexpr int kRtDefaultBlocks = 120;
constexpr int kRtMaxBlocks = 300;
constexpr int kRtMaxMarkdown = 60'000;
constexpr int kRtMaxText = 20'000;

QString rt_clip(const QString& text, int max_chars) {
    const QString flat = text.simplified();
    if (flat.size() <= max_chars)
        return flat;
    return flat.left(std::max(0, max_chars - 1)) + QChar(0x2026);
}

QString rt_quote(const QString& text) {
    return QStringLiteral("“%1”").arg(text);
}

QString rt_report_name(const QString& title, qint64 id, bool is_open) {
    QString n = rt_quote(title.isEmpty() ? QStringLiteral("Untitled Report") : title);
    if (is_open)
        n += QStringLiteral(" (open in the Report Builder)");
    else if (id > 0)
        n += QStringLiteral(" (report %1)").arg(id);
    return n;
}

ToolResult rt_refused(ToolApprovalVerdict v, const QString& what) {
    QString text;
    switch (v) {
        case ToolApprovalVerdict::Approved:
            break;
        case ToolApprovalVerdict::Denied:
            text = QStringLiteral("The user declined: %1. Do not retry it; tell the user what you wanted to do.").arg(what);
            break;
        case ToolApprovalVerdict::TimedOut:
            text = QStringLiteral("Nobody approved \"%1\" in Fincept Terminal in time, so it was not done.").arg(what);
            break;
        case ToolApprovalVerdict::Unavailable:
            text = QStringLiteral("\"%1\" needs the user's approval, but no Fincept Terminal window is open to ask in.")
                       .arg(what);
            break;
    }
    return ToolResult::fail(text);
}

} // namespace

// ── Pure helpers ─────────────────────────────────────────────────────────────

bool ReportToolHandler::is_text_block(const QString& type) {
    return type == QLatin1String("text") || type == QLatin1String("heading") || type == QLatin1String("quote") ||
           type == QLatin1String("callout") || type == QLatin1String("code") || type == QLatin1String("list");
}

QString ReportToolHandler::block_summary(const rep::ReportComponent& c, int max_chars) {
    const QString& t = c.type;
    if (is_text_block(t)) {
        if (!c.content.trimmed().isEmpty())
            return rt_clip(c.content, max_chars);
        return c.config.value(QStringLiteral("placeholder")).isEmpty()
                   ? QStringLiteral("(empty)")
                   : QStringLiteral("(empty — to write: %1)").arg(rt_clip(c.config.value(QStringLiteral("placeholder")), max_chars));
    }
    if (t == QLatin1String("table")) {
        const QVector<QStringList> grid = rep::table::parse(c.config.value(QStringLiteral("csv")));
        int cols = 0;
        for (const QStringList& r : grid)
            cols = std::max(cols, static_cast<int>(r.size()));
        QString s = c.config.value(QStringLiteral("title"));
        s += (s.isEmpty() ? QString() : QStringLiteral(" — ")) +
             QStringLiteral("%1 rows × %2 columns").arg(grid.size()).arg(cols);
        if (!grid.isEmpty())
            s += QStringLiteral("; header: ") + grid.first().join(QStringLiteral(" | "));
        return rt_clip(s, max_chars);
    }
    if (t == QLatin1String("chart"))
        return rt_clip(QStringLiteral("%1 chart: %2")
                           .arg(c.config.value(QStringLiteral("chart_type"), QStringLiteral("line")),
                                c.config.value(QStringLiteral("title"))),
                       max_chars);
    if (t == QLatin1String("stats_block"))
        return rt_clip(c.config.value(QStringLiteral("title")) + QStringLiteral(": ") +
                           c.config.value(QStringLiteral("data")).replace(QLatin1Char('\n'), QStringLiteral("; ")),
                       max_chars);
    if (t == QLatin1String("market_data"))
        return rt_clip(QStringLiteral("%1 %2 (%3%)")
                           .arg(c.config.value(QStringLiteral("symbol")), c.config.value(QStringLiteral("price")),
                                c.config.value(QStringLiteral("change_pct"))),
                       max_chars);
    if (t == QLatin1String("image") || t == QLatin1String("cover"))
        return rt_clip(c.config.value(QStringLiteral("caption"), c.config.value(QStringLiteral("title"))), max_chars);
    return {};
}

QJsonObject ReportToolHandler::outline_json(const rep::ReportDocument& doc, qint64 id, bool is_open, int max_blocks) {
    QJsonObject out;
    out.insert(QStringLiteral("report_id"), static_cast<double>(id));
    out.insert(QStringLiteral("open_in_report_builder"), is_open);
    const rep::ReportMetadata& m = doc.metadata;
    out.insert(QStringLiteral("title"), m.title);
    out.insert(QStringLiteral("author"), m.author);
    out.insert(QStringLiteral("company"), m.company);
    out.insert(QStringLiteral("date"), m.date);
    QJsonObject vars;
    for (auto it = m.variables.cbegin(); it != m.variables.cend(); ++it)
        vars.insert(it.key(), it.value());
    if (!vars.isEmpty())
        out.insert(QStringLiteral("variables"), vars);
    const QStringList unresolved = rep::vars::unresolved_in(doc);
    if (!unresolved.isEmpty())
        out.insert(QStringLiteral("unresolved_variables"), QJsonArray::fromStringList(unresolved));
    const QHash<int, QString> numbers = rep::outline::heading_numbers(doc);
    QJsonArray blocks;
    const int cap = std::clamp(max_blocks, 1, kRtMaxBlocks);
    for (const rep::ReportComponent& c : doc.components) {
        if (blocks.size() >= cap)
            break;
        QJsonObject b{{QStringLiteral("id"), c.id}, {QStringLiteral("type"), c.type}};
        if (c.type == QLatin1String("heading")) {
            b.insert(QStringLiteral("level"), c.config.value(QStringLiteral("level"), QStringLiteral("1")).toInt());
            if (numbers.contains(c.id))
                b.insert(QStringLiteral("number"), numbers.value(c.id));
        }
        const QString s = block_summary(c, c.type == QLatin1String("heading") ? 120 : 160);
        if (!s.isEmpty())
            b.insert(QStringLiteral("text"), s);
        const rep::Binding bind = rep::binding_of(c);
        if (bind.is_valid())
            b.insert(QStringLiteral("data"), rep::describe(bind));
        const QDateTime as_of = rep::as_of_of(c);
        if (as_of.isValid())
            b.insert(QStringLiteral("as_of"), as_of.toUTC().toString(Qt::ISODate));
        blocks.append(b);
    }
    out.insert(QStringLiteral("blocks"), blocks);
    out.insert(QStringLiteral("block_count"), static_cast<int>(doc.components.size()));
    if (doc.components.size() > blocks.size())
        out.insert(QStringLiteral("blocks_truncated"),
                   QStringLiteral("Showing %1 of %2 blocks; pass max_blocks (up to %3) for more.")
                       .arg(blocks.size())
                       .arg(doc.components.size())
                       .arg(kRtMaxBlocks));
    return out;
}

QString ReportToolHandler::describe_blocks(const QVector<rep::ReportComponent>& blocks) {
    QMap<QString, int> counts;
    QStringList parts;
    for (const rep::ReportComponent& c : blocks) {
        if (c.type == QLatin1String("heading") && parts.size() < 3) {
            parts << QStringLiteral("heading %1").arg(rt_quote(rt_clip(c.content, 60)));
            continue;
        }
        if (c.type == QLatin1String("table")) {
            const QVector<QStringList> grid = rep::table::parse(c.config.value(QStringLiteral("csv")));
            int cols = 0;
            for (const QStringList& r : grid)
                cols = std::max(cols, static_cast<int>(r.size()));
            parts << QStringLiteral("a table (%1 rows × %2 columns)").arg(grid.size()).arg(cols);
            continue;
        }
        ++counts[c.type];
    }
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        parts << QStringLiteral("%1 %2 block%3").arg(it.value()).arg(it.key()).arg(it.value() == 1 ? QString() : QStringLiteral("s"));
    return parts.join(QStringLiteral(", "));
}

QString ReportToolHandler::normalise_export_path(const QString& requested, const QString& default_dir,
                                                 const QString& title, const QString& ext, QString* error) {
    QString path = requested.trimmed();
    if (path.isEmpty()) {
        QString stem;
        for (QChar ch : title.trimmed())
            if (ch.isLetterOrNumber() || ch == QLatin1Char(' ') || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
                stem += ch;
        stem = stem.simplified().left(80);
        if (stem.isEmpty())
            stem = QStringLiteral("Report");
        path = QDir(default_dir).filePath(stem + QLatin1Char('.') + ext);
    }
    if (!QFileInfo(path).isAbsolute()) {
        if (error)
            *error = QStringLiteral("path must be absolute (or omit it to use %1)").arg(QDir::toNativeSeparators(default_dir));
        return {};
    }
    if (QFileInfo(path).suffix().compare(ext, Qt::CaseInsensitive) != 0)
        path += QLatin1Char('.') + ext;
    return QDir::cleanPath(path);
}

// ── Actions ──────────────────────────────────────────────────────────────────

namespace {

/// The caller's stable ids (ScopedCallClient, set around call() on the GUI
/// thread), kept for approvals asked later from asynchronous loads.
struct ReportCaller {
    QString client_id;
    QString run_id;
};

/// What an approval request is about: `kind` (file for an export, generic for
/// edits — those stay grantable) and who asks.
void stamp(ToolApprovalRequest& req, const ReportCaller& caller, const char* kind) {
    req.kind = QString::fromLatin1(kind);
    req.client_id = caller.client_id;
    req.run_id = caller.run_id;
}

} // namespace

void ReportToolHandler::call(const QJsonObject& args, const QString& origin, std::function<void(ToolResult)> reply) {
    const ReportCaller caller{ScopedCallClient::client_id(), ScopedCallClient::run_id()};
    const QString action = args.value(QStringLiteral("action")).toString().trimmed();
    const qint64 id = static_cast<qint64>(args.value(QStringLiteral("report_id")).toDouble(0));
    auto backend = backend_;

    if (action == QLatin1String("list_reports")) {
        const int limit = std::clamp(args.value(QStringLiteral("limit")).toInt(20), 1, kRtMaxListed);
        backend->list_reports(args.value(QStringLiteral("query")).toString(), limit,
                              [reply](const QVector<ReportToolBackend::ListEntry>& entries, const QString& error) {
                                  if (!error.isEmpty() && entries.isEmpty()) {
                                      reply(ToolResult::fail(QStringLiteral("The report library could not be read: ") + error));
                                      return;
                                  }
                                  QJsonArray arr;
                                  for (const auto& e : entries)
                                      arr.append(QJsonObject{{QStringLiteral("report_id"), static_cast<double>(e.id)},
                                                             {QStringLiteral("title"), e.title},
                                                             {QStringLiteral("blocks"), e.blocks},
                                                             {QStringLiteral("updated"), e.updated},
                                                             {QStringLiteral("open"), e.open}});
                                  reply(ToolResult::ok(QStringLiteral("%1 report(s). report_id 0 = the one open in the "
                                                                      "Report Builder.")
                                                           .arg(arr.size()),
                                                       QJsonObject{{QStringLiteral("reports"), arr}}));
                              });
        return;
    }

    if (action == QLatin1String("get_report")) {
        const int max_blocks = args.value(QStringLiteral("max_blocks")).toInt(kRtDefaultBlocks);
        backend->load(id, [reply, max_blocks](bool ok, const rep::ReportDocument& doc, qint64 rid, bool is_open,
                                              const QString& error) {
            if (!ok) {
                reply(ToolResult::fail(error.isEmpty() ? QStringLiteral("No such report.") : error));
                return;
            }
            reply(ToolResult::ok(QStringLiteral("Outline of %1.").arg(rt_report_name(doc.metadata.title, rid, is_open)),
                                 outline_json(doc, rid, is_open, max_blocks)));
        });
        return;
    }

    if (action == QLatin1String("append_blocks")) {
        const QString markdown = args.value(QStringLiteral("markdown")).toString();
        const QString heading = args.value(QStringLiteral("title")).toString().trimmed();
        if (markdown.trimmed().isEmpty() && heading.isEmpty()) {
            reply(ToolResult::fail(QStringLiteral("append_blocks needs markdown (and optionally title).")));
            return;
        }
        if (markdown.size() > kRtMaxMarkdown) {
            reply(ToolResult::fail(QStringLiteral("markdown is too long (%1 characters; at most %2 per call).")
                                       .arg(markdown.size())
                                       .arg(kRtMaxMarkdown)));
            return;
        }
        QVector<rep::ReportComponent> blocks;
        if (!heading.isEmpty())
            blocks.append(rep::blocks::heading(heading, 2));
        blocks += rep::blocks::from_markdown(markdown);
        if (blocks.isEmpty()) {
            reply(ToolResult::fail(QStringLiteral("The markdown produced no blocks.")));
            return;
        }
        backend->load(id, [backend, reply, origin, caller, blocks, id](bool ok, const rep::ReportDocument& doc, qint64 rid,
                                                               bool is_open, const QString& error) {
            if (!ok) {
                reply(ToolResult::fail(error.isEmpty() ? QStringLiteral("No such report.") : error));
                return;
            }
            const QString what = QStringLiteral("add %1 to %2")
                                     .arg(describe_blocks(blocks), rt_report_name(doc.metadata.title, rid, is_open));
            ToolApprovalRequest req;
            req.client = origin;
            req.tool = QStringLiteral("report: append blocks");
            req.description = QStringLiteral("Add %1 block(s) at the end of %2: %3. One undo step in the Report Builder.")
                                  .arg(blocks.size())
                                  .arg(rt_report_name(doc.metadata.title, rid, is_open), describe_blocks(blocks));
            req.args = QJsonObject{{QStringLiteral("report"), doc.metadata.title}, {QStringLiteral("blocks"), static_cast<int>(blocks.size())}};
            stamp(req, caller, "generic");
            backend->ask_approval(req, [backend, reply, blocks, id, what](ToolApprovalVerdict v) {
                if (v != ToolApprovalVerdict::Approved) {
                    reply(rt_refused(v, what));
                    return;
                }
                backend->append(id, blocks, QStringLiteral("AI agent"),
                                [reply](bool ok2, const QVector<int>& ids, const QString& title, const QString& err) {
                                    if (!ok2) {
                                        reply(ToolResult::fail(QStringLiteral("The blocks were not added: ") + err));
                                        return;
                                    }
                                    QJsonArray arr;
                                    for (int b : ids)
                                        arr.append(b);
                                    reply(ToolResult::ok(QStringLiteral("Added %1 block(s) to %2.").arg(ids.size()).arg(rt_quote(title)),
                                                         QJsonObject{{QStringLiteral("block_ids"), arr}}));
                                });
            });
        });
        return;
    }

    if (action == QLatin1String("update_block_text")) {
        const int block_id = args.value(QStringLiteral("block_id")).toInt(0);
        const QString text = args.value(QStringLiteral("text")).toString();
        if (block_id <= 0) {
            reply(ToolResult::fail(QStringLiteral("update_block_text needs block_id (from get_report).")));
            return;
        }
        if (text.size() > kRtMaxText) {
            reply(ToolResult::fail(QStringLiteral("text is too long (at most %1 characters).").arg(kRtMaxText)));
            return;
        }
        backend->load(id, [backend, reply, origin, caller, id, block_id, text](bool ok, const rep::ReportDocument& doc, qint64 rid,
                                                                       bool is_open, const QString& error) {
            if (!ok) {
                reply(ToolResult::fail(error.isEmpty() ? QStringLiteral("No such report.") : error));
                return;
            }
            const int idx = doc.index_of(block_id);
            if (idx < 0) {
                reply(ToolResult::fail(QStringLiteral("Block %1 is not in that report (get_report lists the ids).").arg(block_id)));
                return;
            }
            const rep::ReportComponent& c = doc.components[idx];
            if (!is_text_block(c.type)) {
                reply(ToolResult::fail(QStringLiteral("Block %1 is a %2 block; only text, heading, quote, callout, code "
                                                      "and list blocks take text.")
                                           .arg(block_id)
                                           .arg(c.type)));
                return;
            }
            if (rep::is_bound(c)) {
                reply(ToolResult::fail(QStringLiteral("Block %1 is filled by a live data source; its text cannot be replaced.")
                                           .arg(block_id)));
                return;
            }
            const QString what = QStringLiteral("replace the text of block %1 in %2")
                                     .arg(block_id)
                                     .arg(rt_report_name(doc.metadata.title, rid, is_open));
            ToolApprovalRequest req;
            req.client = origin;
            req.tool = QStringLiteral("report: update block text");
            // One .arg() call: chained calls re-scan the text, so a "%6" / "%7" inside the report title or
            // the old block text would receive the new text and garble what the user is approving.
            req.description = QStringLiteral("Replace the text of %1 block %2 in %3 (%4 characters now: %5) with %6 "
                                             "characters: %7. Undo restores it.")
                                  .arg(c.type, QString::number(block_id),
                                       rt_report_name(doc.metadata.title, rid, is_open),
                                       QString::number(c.content.size()), rt_quote(rt_clip(c.content, 80)),
                                       QString::number(text.size()), rt_quote(rt_clip(text, 160)));
            req.args = QJsonObject{{QStringLiteral("block_id"), block_id}, {QStringLiteral("text"), rt_clip(text, 400)}};
            stamp(req, caller, "generic");
            backend->ask_approval(req, [backend, reply, id, block_id, text, what](ToolApprovalVerdict v) {
                if (v != ToolApprovalVerdict::Approved) {
                    reply(rt_refused(v, what));
                    return;
                }
                backend->update_text(id, block_id, text, [reply, block_id](bool ok2, const QString& err) {
                    reply(ok2 ? ToolResult::ok(QStringLiteral("Block %1 updated.").arg(block_id))
                              : ToolResult::fail(QStringLiteral("Block %1 was not updated: %2").arg(block_id).arg(err)));
                });
            });
        });
        return;
    }

    if (action == QLatin1String("refresh_report_data")) {
        backend->load(id, [backend, reply, origin, caller, id](bool ok, const rep::ReportDocument& doc, qint64 rid, bool is_open,
                                                       const QString& error) {
            if (!ok) {
                reply(ToolResult::fail(error.isEmpty() ? QStringLiteral("No such report.") : error));
                return;
            }
            QStringList kinds;
            int bound = 0;
            for (const rep::ReportComponent& c : doc.components) {
                const rep::Binding b = rep::binding_of(c);
                if (!b.is_refreshable())
                    continue;
                ++bound;
                if (!kinds.contains(b.source))
                    kinds << b.source;
            }
            if (bound == 0) {
                reply(ToolResult::ok(QStringLiteral("%1 has no live-data blocks; nothing to refresh.")
                                         .arg(rt_report_name(doc.metadata.title, rid, is_open))));
                return;
            }
            const QString what = QStringLiteral("refresh the data of %1").arg(rt_report_name(doc.metadata.title, rid, is_open));
            ToolApprovalRequest req;
            req.client = origin;
            req.tool = QStringLiteral("report: refresh data");
            req.description = QStringLiteral("Refresh %1 live-data block(s) of %2 (%3) from the terminal's free data "
                                             "sources. The numbers in those blocks are replaced; a block whose source "
                                             "fails keeps its last numbers.")
                                  .arg(bound)
                                  .arg(rt_report_name(doc.metadata.title, rid, is_open), kinds.join(QStringLiteral(", ")));
            req.args = QJsonObject{{QStringLiteral("report"), doc.metadata.title}, {QStringLiteral("blocks"), bound}};
            stamp(req, caller, "generic");
            backend->ask_approval(req, [backend, reply, id, what](ToolApprovalVerdict v) {
                if (v != ToolApprovalVerdict::Approved) {
                    reply(rt_refused(v, what));
                    return;
                }
                backend->refresh(id, [reply](bool ok2, int refreshed, int failed, const QString& err) {
                    if (!ok2) {
                        reply(ToolResult::fail(QStringLiteral("The refresh did not run: ") + err));
                        return;
                    }
                    reply(ToolResult::ok(QStringLiteral("Refreshed %1 block(s); %2 could not be refreshed and kept "
                                                        "their last numbers.")
                                             .arg(refreshed)
                                             .arg(failed),
                                         QJsonObject{{QStringLiteral("refreshed"), refreshed}, {QStringLiteral("failed"), failed}}));
                });
            });
        });
        return;
    }

    if (action == QLatin1String("export_report")) {
        QString format = args.value(QStringLiteral("format")).toString(QStringLiteral("pdf")).trimmed().toLower();
        if (format == QLatin1String("markdown"))
            format = QStringLiteral("md");
        if (format == QLatin1String("docx") || format == QLatin1String("word"))
            format = QStringLiteral("odt");
        if (format != QLatin1String("pdf") && format != QLatin1String("odt") && format != QLatin1String("md") &&
            format != QLatin1String("html")) {
            reply(ToolResult::fail(QStringLiteral("format must be pdf, odt, md or html.")));
            return;
        }
        const QString requested = args.value(QStringLiteral("path")).toString();
        backend->load(id, [backend, reply, origin, caller, id, format, requested](bool ok, const rep::ReportDocument& doc, qint64 rid,
                                                                          bool is_open, const QString& error) {
            if (!ok) {
                reply(ToolResult::fail(error.isEmpty() ? QStringLiteral("No such report.") : error));
                return;
            }
            QString err;
            const QString path = normalise_export_path(requested, backend->default_export_dir(), doc.metadata.title,
                                                       format, &err);
            if (path.isEmpty()) {
                reply(ToolResult::fail(err));
                return;
            }
            const bool exists = backend->file_exists(path);
            const QString what = QStringLiteral("export %1 to %2")
                                     .arg(rt_report_name(doc.metadata.title, rid, is_open), QDir::toNativeSeparators(path));
            ToolApprovalRequest req;
            req.client = origin;
            req.tool = QStringLiteral("report: export");
            req.description = QStringLiteral("Write %1 as %2 to %3%4.")
                                  .arg(rt_report_name(doc.metadata.title, rid, is_open), format.toUpper(),
                                       QDir::toNativeSeparators(path),
                                       exists ? QStringLiteral(" — this REPLACES the existing file") : QString());
            req.args = QJsonObject{{QStringLiteral("format"), format}, {QStringLiteral("path"), QDir::toNativeSeparators(path)}};
            req.destructive = exists;
            stamp(req, caller, "file"); // writes a file wherever the path points
            backend->ask_approval(req, [backend, reply, id, format, path, what](ToolApprovalVerdict v) {
                if (v != ToolApprovalVerdict::Approved) {
                    reply(rt_refused(v, what));
                    return;
                }
                backend->export_report(id, format, path,
                                       [reply](bool ok2, const QString& written, const QStringList& warnings, const QString& e) {
                                           if (!ok2) {
                                               reply(ToolResult::fail(QStringLiteral("The export failed: ") + e));
                                               return;
                                           }
                                           QJsonObject data{{QStringLiteral("path"), QDir::toNativeSeparators(written)}};
                                           if (!warnings.isEmpty())
                                               data.insert(QStringLiteral("warnings"), QJsonArray::fromStringList(warnings));
                                           reply(ToolResult::ok(QStringLiteral("Written: %1").arg(QDir::toNativeSeparators(written)),
                                                                data));
                                       });
            });
        });
        return;
    }

    reply(ToolResult::fail(QStringLiteral("Unknown action \"%1\". Use list_reports, get_report, append_blocks, "
                                          "update_block_text, refresh_report_data or export_report.")
                               .arg(action)));
}

} // namespace fincept::mcp::core
