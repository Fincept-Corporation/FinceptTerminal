#pragma once
// CoreApproval.h — asks the user before a core tool does something risky.
//
// ToolApprovalDialog::request_approval() blocks its calling thread until the
// user answers, and refuses to run on the GUI thread. Core tools decide that a
// step is risky ON the GUI thread (that is where the widget being clicked can be
// inspected), so this helper parks the blocking wait on a small dedicated pool
// and hands the verdict back to the GUI thread. Neither the GUI thread nor the
// caller's thread (an MCP worker, the chat tool loop) is ever blocked by it.

#include "mcp/server/ToolApprovalDialog.h"

#include <QString>

#include <functional>

namespace fincept::mcp::core {

/// Ask the user to approve `request`. `done` runs exactly once, on the GUI
/// thread, with the verdict. Callable from any thread. `timeout_ms` bounds the
/// wait; keep it inside the calling tool's own budget so a late "approve" can
/// never act after the call already answered.
void ask_user_approval(const ToolApprovalRequest& request, std::function<void(ToolApprovalVerdict)> done,
                       int timeout_ms = ToolApprovalDialog::kDefaultTimeoutMs);

/// A one-line refusal for a verdict other than Approved, written for the model.
QString approval_refusal_text(ToolApprovalVerdict verdict, const QString& what);

} // namespace fincept::mcp::core
