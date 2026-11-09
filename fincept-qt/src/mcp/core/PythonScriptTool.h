#pragma once
// PythonScriptTool.h — the `python` core tool: find, describe and run the
// terminal's bundled analytics scripts (scripts/**, ~490 runnable entry points:
// market & macro data connectors, Analytics/*, AI Quant Lab, technicals …).
//
//   python(action="search", query="fred gdp")         ranked catalogue matches
//   python(action="info",   script="fred_data")        docstring, commands, options, API keys, main()
//   python(action="run",    script="fred_data", args=["series","GDP"])
//        → the result inline when it finishes within wait_s (≤ 25 s), otherwise a job id
//   python(action="status", job_id="job_000012", wait_s=25)   → long-poll; returns the result when done
//   python(action="cancel", job_id="job_000012")       → kills the process
//
// Policy (per script, never per tool):
//   blocked    — never runnable here: arbitrary-code runners, maintenance tools,
//                tests, packaging helpers
//   sensitive  — asks the user first (ToolApprovalDialog): anything under
//                exchange/ (exchange credentials, orders), LLM agent runners,
//                live strategy runners, voice I/O, and any script called with an
//                order / transfer / delete style command
//   normal     — runs straight away
// Scripts run through PythonRunner (P4: concurrency cap, venv routing, per-tree
// watchdog), never a raw QProcess.

#include "mcp/McpTypes.h"

#include <QString>
#include <QStringList>

namespace fincept::mcp::core {

/// The `python` ToolDef.
ToolDef make_python_tool();

namespace python_scripts {

enum class ScriptRisk { Normal, Sensitive, Blocked };

/// Canonical catalogue key for a user-supplied script reference: forward
/// slashes, no "scripts/" prefix, no ".py" suffix ("Analytics/corporateFinance/
/// lbo/lbo_model"). Empty + `*error` set for anything that is not a plain
/// relative path inside the scripts tree (absolute paths, "..", drive letters,
/// odd characters).
QString normalize_script_key(const QString& raw, QString* error);

/// Risk of running `key` with `args`. The command is read from the first
/// argument, an --action / --command / --operation flag, or the action /
/// command / operation key of a JSON-object argument. `runs_code`: the script
/// exec()s / eval()s its input (Sensitive). `moves_money` (optional) is set when
/// the call places / changes / cancels orders or moves money — the approval
/// dialog then offers no blanket grant.
ScriptRisk classify(const QString& key, const QStringList& args, bool runs_code = false, bool* moves_money = nullptr);

} // namespace python_scripts

} // namespace fincept::mcp::core
