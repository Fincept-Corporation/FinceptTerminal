// CodingAgentTypes.cpp — id <-> enum mapping, schedule maths and text helpers
// for the Agent Workbench. QtCore only.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QTime>

namespace fincept::services::coding_agents {

namespace coding_agent_types_detail {

struct KindName {
    EventKind kind;
    const char* id;
};

constexpr KindName kEventKindNames[] = {
    {EventKind::Text, "text"},         {EventKind::Thinking, "thinking"},       {EventKind::ToolCall, "tool_call"},
    {EventKind::ToolResult, "tool_result"}, {EventKind::Command, "command"}, {EventKind::FileChange, "file_change"},
    {EventKind::Status, "status"},     {EventKind::Usage, "usage"},             {EventKind::Error, "error"},
    {EventKind::Final, "final"},
};

} // namespace coding_agent_types_detail

QString agent_id(AgentKind kind) {
    return kind == AgentKind::Codex ? QStringLiteral("codex") : QStringLiteral("claude");
}

std::optional<AgentKind> agent_from_id(const QString& id) {
    const QString v = id.trimmed().toLower();
    if (v == QLatin1String("claude") || v == QLatin1String("claude_code") || v == QLatin1String("claude-code"))
        return AgentKind::Claude;
    if (v == QLatin1String("codex") || v == QLatin1String("openai_codex") || v == QLatin1String("codex-cli"))
        return AgentKind::Codex;
    return std::nullopt;
}

QString agent_display_name(AgentKind kind) {
    return kind == AgentKind::Codex ? QStringLiteral("Codex") : QStringLiteral("Claude Code");
}

QString mode_id(RunMode mode) {
    return mode == RunMode::WorkspaceEdits ? QStringLiteral("workspace_edits") : QStringLiteral("analysis");
}

RunMode mode_from_id(const QString& id) {
    return id == QLatin1String("workspace_edits") ? RunMode::WorkspaceEdits : RunMode::Analysis;
}

QString status_id(RunStatus status) {
    switch (status) {
        case RunStatus::Queued:
            return QStringLiteral("queued");
        case RunStatus::Running:
            return QStringLiteral("running");
        case RunStatus::Succeeded:
            return QStringLiteral("succeeded");
        case RunStatus::Failed:
            return QStringLiteral("failed");
        case RunStatus::Cancelled:
            return QStringLiteral("cancelled");
        case RunStatus::Interrupted:
            return QStringLiteral("interrupted");
    }
    return QStringLiteral("failed");
}

RunStatus status_from_id(const QString& id) {
    if (id == QLatin1String("queued"))
        return RunStatus::Queued;
    if (id == QLatin1String("running"))
        return RunStatus::Running;
    if (id == QLatin1String("succeeded"))
        return RunStatus::Succeeded;
    if (id == QLatin1String("cancelled"))
        return RunStatus::Cancelled;
    if (id == QLatin1String("interrupted"))
        return RunStatus::Interrupted;
    return RunStatus::Failed;
}

bool is_terminal_status(RunStatus status) {
    return status != RunStatus::Queued && status != RunStatus::Running;
}

QString event_kind_id(EventKind kind) {
    for (const auto& kn : coding_agent_types_detail::kEventKindNames) {
        if (kn.kind == kind)
            return QString::fromLatin1(kn.id);
    }
    return QStringLiteral("status");
}

EventKind event_kind_from_id(const QString& id) {
    for (const auto& kn : coding_agent_types_detail::kEventKindNames) {
        if (id == QLatin1String(kn.id))
            return kn.kind;
    }
    return EventKind::Status;
}

QString schedule_kind_id(ScheduleKind kind) {
    return kind == ScheduleKind::Interval ? QStringLiteral("interval") : QStringLiteral("daily");
}

ScheduleKind schedule_kind_from_id(const QString& id) {
    return id == QLatin1String("interval") ? ScheduleKind::Interval : ScheduleKind::Daily;
}

QString run_title_from_prompt(const QString& prompt) {
    const QStringList lines = prompt.split(QLatin1Char('\n'));
    QString first;
    for (const auto& line : lines) {
        const QString t = line.simplified();
        if (!t.isEmpty()) {
            first = t;
            break;
        }
    }
    // Strip a leading Markdown heading marker — "# Morning brief" reads better as a title.
    while (first.startsWith(QLatin1Char('#')))
        first.remove(0, 1);
    first = first.trimmed();
    if (first.isEmpty())
        return QStringLiteral("Untitled run");
    if (first.size() > kAgentRunTitleMaxChars)
        first = first.left(kAgentRunTitleMaxChars - 1).trimmed() + QChar(0x2026);
    return first;
}

QString parse_cli_version(const QString& line) {
    static const QRegularExpression rx(QStringLiteral(R"((\d+)\.(\d+)\.(\d+))"));
    const auto m = rx.match(line);
    return m.hasMatch() ? m.captured(0) : QString();
}

bool cli_version_at_least(const QString& version, int major, int minor, int patch) {
    const QStringList parts = version.split(QLatin1Char('.'));
    if (parts.size() < 3)
        return false;
    bool ok1 = false, ok2 = false, ok3 = false;
    const int a = parts[0].toInt(&ok1);
    const int b = parts[1].toInt(&ok2);
    const int c = parts[2].toInt(&ok3);
    if (!ok1 || !ok2 || !ok3)
        return false;
    if (a != major)
        return a > major;
    if (b != minor)
        return b > minor;
    return c >= patch;
}

qint64 compute_next_run_ms(const CodingAgentSchedule& schedule, qint64 after_ms) {
    if (schedule.kind == ScheduleKind::Interval) {
        const int minutes = qMax(schedule.interval_minutes, kScheduleMinIntervalMinutes);
        return after_ms + static_cast<qint64>(minutes) * 60 * 1000;
    }
    const QTime at = QTime::fromString(schedule.daily_time, QStringLiteral("HH:mm"));
    if (!at.isValid() || (schedule.weekdays_mask & 0x7F) == 0)
        return 0;
    const QDateTime from = QDateTime::fromMSecsSinceEpoch(after_ms); // local time
    // Look one extra day ahead so "today at HH:mm, already passed" rolls over a
    // full week even when only today's weekday is enabled.
    for (int d = 0; d <= 7; ++d) {
        const QDate date = from.date().addDays(d);
        const int bit = date.dayOfWeek() - 1; // 0 = Monday
        if ((schedule.weekdays_mask & (1 << bit)) == 0)
            continue;
        const QDateTime candidate(date, at);
        if (!candidate.isValid())
            continue; // a DST gap swallowed this wall-clock time — try the next day
        const qint64 ms = candidate.toMSecsSinceEpoch();
        if (ms > after_ms)
            return ms;
    }
    return 0;
}

QString validate_schedule(const CodingAgentSchedule& schedule) {
    if (schedule.name.trimmed().isEmpty())
        return QStringLiteral("Schedule name is required.");
    if (schedule.prompt.trimmed().isEmpty())
        return QStringLiteral("Schedule prompt is required.");
    if (schedule.prompt.size() > kAgentPromptMaxChars)
        return QStringLiteral("Prompt is too long.");
    if (schedule.kind == ScheduleKind::Interval) {
        if (schedule.interval_minutes < kScheduleMinIntervalMinutes)
            return QStringLiteral("Interval must be at least %1 minutes.").arg(kScheduleMinIntervalMinutes);
        if (schedule.interval_minutes > 7 * 24 * 60)
            return QStringLiteral("Interval must be at most 7 days.");
    } else {
        if (!QTime::fromString(schedule.daily_time, QStringLiteral("HH:mm")).isValid())
            return QStringLiteral("Daily time must be HH:mm.");
        if ((schedule.weekdays_mask & 0x7F) == 0)
            return QStringLiteral("Pick at least one weekday.");
    }
    if (schedule.max_budget_usd < 0.0)
        return QStringLiteral("Budget cannot be negative.");
    return {};
}

QString clip_utf8(const QString& text, int max_bytes) {
    // Fast path: a UTF-16 code unit is at most 3 UTF-8 bytes.
    if (text.size() * 3 <= max_bytes)
        return text;
    const QByteArray utf8 = text.toUtf8();
    if (utf8.size() <= max_bytes)
        return text;
    const int keep_budget = qMax(0, max_bytes - 64); // leave room for the marker
    int cut = keep_budget;
    // Never split a multi-byte sequence: back up over continuation bytes.
    while (cut > 0 && (static_cast<unsigned char>(utf8.at(cut)) & 0xC0) == 0x80)
        --cut;
    const qsizetype dropped = utf8.size() - cut;
    return QString::fromUtf8(utf8.constData(), cut) +
           QStringLiteral("\n… [truncated %1 bytes]").arg(static_cast<qlonglong>(dropped));
}

} // namespace fincept::services::coding_agents
