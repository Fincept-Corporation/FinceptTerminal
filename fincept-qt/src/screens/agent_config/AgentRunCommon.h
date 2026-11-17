#pragma once
// AgentRunCommon.h — small UI helpers shared by the agents screen's run panels
// and the harness widgets (also used by Settings › AI Agents). The awb_ prefix
// and the "AgentWorkbench" translation context come from the Agent Workbench
// these helpers were moved from.

#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/HarnessTypes.h"

#include <QString>

class QWidget;

namespace fincept::screens {

/// "RUNNING", "SUCCEEDED", … (translated)
QString awb_status_text(services::coding_agents::RunStatus status);
/// Status pill text for a run: the failure class when there is one ("SIGNED OUT",
/// "RATE LIMITED", "TIMED OUT", "NOT INSTALLED", "MODEL REJECTED"), else the status.
QString awb_run_pill_text(const services::coding_agents::CodingAgentRun& run);
/// The harness's display name ("Claude Code"), or "unknown harness (<id>)".
QString awb_harness_name(const QString& harness_id);
/// "Read-only analysis" | "Workspace edits" | "Full access"
QString awb_permission_text(harness::PermissionMode permission);
/// "Signed in · subscription · max", "Signed out", "Sign-in state unknown", …
QString awb_auth_text(const harness::AuthStatus& auth);
/// 2m 13s / 1h 04m
QString awb_duration(qint64 ms);
/// 950 / 12.3k / 1.2M
QString awb_tokens(qint64 n);
/// Wall-clock duration of a run so far (or in total once finished); 0 if never started.
qint64 awb_run_duration_ms(const services::coding_agents::CodingAgentRun& run);
/// Re-evaluate QSS after a dynamic property change (e.g. [status="running"]).
void awb_repolish(QWidget* w);
/// HTML-escaped `text` that a word-wrapped label may break after / \ @ - . :
/// (a long path or command is one "word" otherwise, and sets the label's — and
/// so the whole column's — minimum width).
QString awb_breakable_html(const QString& text);

/// The overall state of one harness, for the coloured dot and the one-word state.
enum class AwbHarnessTone { Ready, Warning, Danger, Busy, Absent };
struct AwbHarnessState {
    AwbHarnessTone tone = AwbHarnessTone::Absent;
    QString word;     // "READY", "SIGNED OUT", "NOT INSTALLED", "DETECTING", …
    QString sentence; // one plain sentence for tooltips and warning lines
};
AwbHarnessState awb_harness_state(const QString& harness_id);
/// SUCCESS / WARNING / DANGER / INFO / TEXT_TERTIARY for a tone (theme token value).
QString awb_tone_color(AwbHarnessTone tone);

} // namespace fincept::screens
