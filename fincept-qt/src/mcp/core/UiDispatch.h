#pragma once
// UiDispatch.h — how a core UI tool call reaches the GUI-thread engine: posted
// to the GUI thread, answered once through the provider's single-winner guard,
// every failure text starting with an error code (UiErrors.h), and nothing the
// engine throws escaping into the event loop. Header-only so the unit tests
// drive the very wrapper the terminal uses.

#include "core/logging/Logger.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"
#include "mcp/core/UiErrors.h"
#include "mcp/server/CallApproval.h"

#include <QCoreApplication>

#include <exception>
#include <functional>

namespace fincept::mcp::core {

using UiEngineFn =
    std::function<void(const QJsonObject& args, const QString& origin, ui::CancelCheck cancelled, ui::Reply reply)>;

/// An async handler that runs `fn` on the GUI thread. The caller's thread is
/// never blocked; McpProvider's watchdog bounds the whole call by the tool's
/// timeout.
inline AsyncToolHandler ui_gui_handler(const QString& tool, UiEngineFn fn) {
    return [tool, fn](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
        const common::Resolver resolve = common::make_resolver(ctx, promise);
        if (!QCoreApplication::instance() || !fn) {
            resolve(ui::ui_fail(ui::Err::Busy, QStringLiteral("The terminal UI is not running.")));
            return;
        }
        QString origin = ScopedCallOrigin::current(); // read on the calling thread
        if (ScopedPaperRun::active())
            origin += QLatin1String(kPaperRunOriginSuffix); // the GUI thread cannot see the flag
        // The stable caller ids (the transport's ScopedCallClient) travel with the
        // label and are re-established on the GUI thread for the engine's
        // synchronous entry (agent_actor_for_call reads them there).
        const QString client_id = ScopedCallClient::client_id();
        const QString run_id = ScopedCallClient::run_id();
        const auto is_cancelled = ctx.is_cancelled;
        common::on_gui_thread([tool, fn, args, origin, client_id, run_id, is_cancelled, resolve]() {
            ui::Reply reply = [resolve](ToolResult r) {
                if (!r.success)
                    r.error = ui::with_error_code(r.error);
                resolve(std::move(r));
            };
            ScopedCallClient caller(client_id, run_id);
            try {
                fn(args, origin, is_cancelled, reply);
            } catch (const std::exception& e) {
                LOG_ERROR("CoreTools", QStringLiteral("%1 threw: %2").arg(tool, QString::fromUtf8(e.what())));
                reply(ui::ui_fail(ui::Err::Internal, QStringLiteral("%1 failed inside the terminal: %2")
                                                         .arg(tool, QString::fromUtf8(e.what()))));
            } catch (...) {
                LOG_ERROR("CoreTools", QStringLiteral("%1 threw an unknown exception").arg(tool));
                reply(ui::ui_fail(ui::Err::Internal, QStringLiteral("%1 failed inside the terminal.").arg(tool)));
            }
        });
    };
}

} // namespace fincept::mcp::core
