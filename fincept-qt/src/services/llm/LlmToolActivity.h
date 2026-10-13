#pragma once
// LlmToolActivity.h — completion records on the chat's tool channel.
//
// The tool channel (tool_stream_prefix(), LlmService.h) used to carry one line
// per call, emitted BEFORE the call ran (detail::emit_tool_progress). The chat
// could show what was being called but never how long it took, whether it
// worked, or that a slow tool had been handed to a background job. Each call
// is now followed by a completion record. Start lines are unchanged, so a
// consumer that ignores records — or a tool loop that does not emit them yet —
// keeps working exactly as before.
//
// Record payload (after the tool prefix), one line:
//   \x1D <outcome> \x1F <elapsed_ms> \x1F <flags> \x1F <label> \x1F <detail>
//   outcome  'o' ok · 'f' failed · 'j' backgrounded (detail = job id)
//   flags    'p' when the call ran inside a multi-call round: elapsed_ms is then
//            the whole round's wall time, not this call's alone
//   label    exactly the start line's label, so a consumer pairs them FIFO
//   detail   short failure reason / job id, control characters stripped

#include "mcp/McpTypes.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace fincept::ai_chat {

struct ToolDoneRecord {
    enum class Outcome { Ok, Failed, Backgrounded };
    Outcome outcome = Outcome::Ok;
    qint64 elapsed_ms = -1; ///< -1 = unknown
    bool parallel = false;  ///< elapsed covers a multi-call round
    QString label;
    QString detail;
};

inline QChar tool_done_marker() {
    return QChar(0x1D);
}

namespace tool_activity_detail {
/// One-line, separator-free, bounded text for a record field.
inline QString clean_field(QString s, int max_chars) {
    for (QChar& c : s)
        if (c.unicode() < 0x20)
            c = QLatin1Char(' ');
    s = s.simplified();
    if (s.size() > max_chars)
        s = s.left(max_chars - 1) + QChar(0x2026);
    return s;
}
} // namespace tool_activity_detail

/// Record payload WITHOUT the tool-channel prefix and trailing newline.
inline QString encode_tool_done(const ToolDoneRecord& r) {
    const QChar sep(0x1F);
    const QChar outcome = r.outcome == ToolDoneRecord::Outcome::Failed         ? QLatin1Char('f')
                          : r.outcome == ToolDoneRecord::Outcome::Backgrounded ? QLatin1Char('j')
                                                                               : QLatin1Char('o');
    return tool_done_marker() + QString(outcome) + sep + QString::number(r.elapsed_ms) + sep +
           (r.parallel ? QStringLiteral("p") : QString()) + sep + tool_activity_detail::clean_field(r.label, 200) +
           sep + tool_activity_detail::clean_field(r.detail, 240);
}

/// Parse one tool-channel line (prefix already stripped). nullopt for a start
/// line or anything malformed.
inline std::optional<ToolDoneRecord> decode_tool_done(const QString& line) {
    if (!line.startsWith(tool_done_marker()))
        return std::nullopt;
    const QStringList f = line.mid(1).split(QChar(0x1F));
    if (f.size() < 5 || f[0].size() != 1)
        return std::nullopt;
    ToolDoneRecord r;
    const QChar o = f[0].at(0);
    r.outcome = o == QLatin1Char('f')   ? ToolDoneRecord::Outcome::Failed
                : o == QLatin1Char('j') ? ToolDoneRecord::Outcome::Backgrounded
                                        : ToolDoneRecord::Outcome::Ok;
    bool ok = false;
    r.elapsed_ms = f[1].toLongLong(&ok);
    if (!ok)
        r.elapsed_ms = -1;
    r.parallel = f[2].contains(QLatin1Char('p'));
    r.label = f[3];
    r.detail = f[4];
    return r;
}

/// Build the record for a finished call. `label` must equal the start line's
/// label (detail::tool_progress_label(display, args)).
inline ToolDoneRecord make_tool_done(const QString& label, const mcp::ToolResult& tr, qint64 elapsed_ms,
                                     bool parallel = false) {
    ToolDoneRecord r;
    r.label = label;
    r.elapsed_ms = elapsed_ms;
    r.parallel = parallel;
    const QJsonObject d = tr.data.toObject();
    const QString job_id = d.value(QStringLiteral("job_id")).toString();
    if (tr.success && !job_id.isEmpty() && d.value(QStringLiteral("status")).toString() == QLatin1String("running")) {
        // A receipt, not a result: McpProvider backgrounded the call (§M1).
        r.outcome = ToolDoneRecord::Outcome::Backgrounded;
        r.detail = job_id;
    } else if (!tr.success) {
        r.outcome = ToolDoneRecord::Outcome::Failed;
        r.detail = !tr.error.isEmpty() ? tr.error : tr.message;
    }
    return r;
}

/// Emit a completion record on the calling thread's progress channel (no-op
/// when no streaming consumer installed one). `display` / `args` are the same
/// values the start line was emitted with. Defined in LlmToolLoop.cpp so this
/// header stays free of the tool-loop internals the chat UI must not include.
void emit_tool_done(const QString& display, const QJsonObject& args, const mcp::ToolResult& tr, qint64 elapsed_ms,
                    bool parallel = false);

} // namespace fincept::ai_chat
