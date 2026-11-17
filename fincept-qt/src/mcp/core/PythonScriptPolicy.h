#pragma once
// PythonScriptPolicy.h — what the `python` core tool (PythonScriptTool.cpp) may
// do with one script call, decided before anything starts. Pure (Qt Core, no
// I/O): unit-tested by tests/tst_python_script_policy.cpp.
//
//   blocked    — never runnable here (arbitrary-code runners, maintenance, tests)
//   sensitive  — the user approves each run (no timed grant), and a PAPER run
//                is refused outright:
//                  kind order — the call places / changes / cancels an order or
//                    moves money (exchange/ order scripts, prediction-market
//                    order commands, order words on argv or in JSON on stdin).
//                    These scripts talk to the exchange directly: the approval
//                    says LIVE and that the order goes straight to the venue,
//                    not through the terminal's trading engine or its approvals.
//                  kind code — everything else that asks (agents, voice, live
//                    strategy runners, exchange reads with stored keys, scripts
//                    that exec() their input, delete / upload style commands).
//   normal     — runs straight away

#include "mcp/server/ApprovalPolicy.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace fincept::mcp::core::python_scripts {

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
/// the call places / changes / cancels orders or moves money: an order / money
/// command anywhere, or an exchange/ script other than a read (fetch_*, list_*,
/// ws_stream).
ScriptRisk classify(const QString& key, const QStringList& args, bool runs_code = false, bool* moves_money = nullptr);

/// Where a money-moving call sends its order, in words: the exchange id an
/// exchange/ script is given ("binance"), the prediction market of a
/// prediction_* script ("Polymarket"), an "exchange" / "broker" / "venue" key of
/// a JSON argument; empty when the call does not say.
QString order_venue(const QString& key, const QStringList& args);

/// One `run` call as the tool received it.
struct RunCall {
    QString key;     ///< canonical script key
    QString summary; ///< the script's first docstring line ("" if none)
    QStringList argv;
    QByteArray stdin_data;
    bool runs_code = false;
    /// The call belongs to a PAPER run (ScopedPaperRun on the calling thread —
    /// a paper-only run token). A caller label ending in " (paper run)" counts
    /// too, whatever this says.
    bool paper_run = false;
    QString origin;    ///< caller label (ScopedCallOrigin)
    QString client_id; ///< ScopedCallClient::client_id()
    QString run_id;    ///< ScopedCallClient::run_id()
};

/// The decision for one `run` call.
struct RunGate {
    ScriptRisk risk = ScriptRisk::Normal;
    bool moves_money = false;
    /// Set: refuse the call with this text — nothing is asked, nothing starts.
    QString refusal;
    /// risk Sensitive and no refusal: ask the user with this (kind order or code).
    ToolApprovalRequest request;
};

RunGate gate_run(const RunCall& call);

} // namespace fincept::mcp::core::python_scripts
