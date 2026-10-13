#pragma once
// AgentWorkbenchCommon.h — small UI helpers shared by the Agent Workbench panels.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QString>

class QWidget;

namespace fincept::screens {

/// "RUNNING", "SUCCEEDED", … (translated)
QString awb_status_text(services::coding_agents::RunStatus status);
/// "CLAUDE CODE" / "CODEX"
QString awb_agent_text(services::coding_agents::AgentKind kind);
/// 2m 13s / 1h 04m
QString awb_duration(qint64 ms);
/// 950 / 12.3k / 1.2M
QString awb_tokens(qint64 n);
/// Wall-clock duration of a run so far (or in total once finished); 0 if never started.
qint64 awb_run_duration_ms(const services::coding_agents::CodingAgentRun& run);
/// Re-evaluate QSS after a dynamic property change (e.g. [status="running"]).
void awb_repolish(QWidget* w);

} // namespace fincept::screens
