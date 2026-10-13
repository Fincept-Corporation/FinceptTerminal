// AgentWorkbenchCommon.cpp — shared Agent Workbench UI helpers.

#include "screens/agent_workbench/AgentWorkbenchCommon.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStyle>
#include <QWidget>

namespace fincept::screens {

namespace ca = services::coding_agents;

QString awb_status_text(ca::RunStatus status) {
    switch (status) {
        case ca::RunStatus::Queued:
            return QCoreApplication::translate("AgentWorkbench", "QUEUED");
        case ca::RunStatus::Running:
            return QCoreApplication::translate("AgentWorkbench", "RUNNING");
        case ca::RunStatus::Succeeded:
            return QCoreApplication::translate("AgentWorkbench", "SUCCEEDED");
        case ca::RunStatus::Failed:
            return QCoreApplication::translate("AgentWorkbench", "FAILED");
        case ca::RunStatus::Cancelled:
            return QCoreApplication::translate("AgentWorkbench", "CANCELLED");
        case ca::RunStatus::Interrupted:
            return QCoreApplication::translate("AgentWorkbench", "INTERRUPTED");
    }
    return QCoreApplication::translate("AgentWorkbench", "FAILED");
}

QString awb_agent_text(ca::AgentKind kind) {
    return kind == ca::AgentKind::Codex ? QCoreApplication::translate("AgentWorkbench", "CODEX")
                                        : QCoreApplication::translate("AgentWorkbench", "CLAUDE CODE");
}

QString awb_duration(qint64 ms) {
    const qint64 s = qMax<qint64>(0, ms) / 1000;
    if (s < 60)
        return QStringLiteral("%1s").arg(s);
    if (s < 3600)
        return QStringLiteral("%1m %2s").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1h %2m").arg(s / 3600).arg((s % 3600) / 60, 2, 10, QLatin1Char('0'));
}

QString awb_tokens(qint64 n) {
    if (n < 1000)
        return QString::number(n);
    if (n < 1000000)
        return QString::number(static_cast<double>(n) / 1000.0, 'f', 1) + QLatin1Char('k');
    return QString::number(static_cast<double>(n) / 1000000.0, 'f', 2) + QLatin1Char('M');
}

qint64 awb_run_duration_ms(const ca::CodingAgentRun& run) {
    if (run.started_at_ms <= 0)
        return 0;
    const qint64 end = run.finished_at_ms > 0 ? run.finished_at_ms : QDateTime::currentMSecsSinceEpoch();
    return qMax<qint64>(0, end - run.started_at_ms);
}

void awb_repolish(QWidget* w) {
    if (!w)
        return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

} // namespace fincept::screens
