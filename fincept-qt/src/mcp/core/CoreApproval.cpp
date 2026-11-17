// CoreApproval.cpp — see CoreApproval.h.

#include "mcp/core/CoreApproval.h"

#include "core/logging/Logger.h"
#include "mcp/server/CallApproval.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QThreadPool>

#include <exception>

namespace fincept::mcp::core {

namespace core_approval_detail {

/// Dedicated pool: every thread here sits in a QWaitCondition for up to the
/// approval deadline, so it must never be QThreadPool::globalInstance() (the LLM
/// fan-out and the agent bridge block on that one). Deliberately leaked — a
/// static QThreadPool would wait for a pending approval at process exit.
QThreadPool* approval_pool() {
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool;
        p->setObjectName(QStringLiteral("mcp-core-approval"));
        p->setMaxThreadCount(4);
        p->setExpiryTimeout(30 * 1000);
        return p;
    }();
    return pool;
}

} // namespace core_approval_detail

void ask_user_approval(const ToolApprovalRequest& request, std::function<void(ToolApprovalVerdict)> done,
                       int timeout_ms) {
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        if (done)
            done(ToolApprovalVerdict::Unavailable);
        return;
    }
    // The caller's stable ids are read here, on the calling thread (the pool cannot see them).
    ToolApprovalRequest asked = request;
    if (asked.client_id.isEmpty()) {
        asked.client_id = ScopedCallClient::client_id();
        if (asked.run_id.isEmpty())
            asked.run_id = ScopedCallClient::run_id();
    }
    core_approval_detail::approval_pool()->start([request = std::move(asked), done = std::move(done), app, timeout_ms]() {
        const ToolApprovalVerdict verdict = ToolApprovalDialog::request_approval(request, timeout_ms);
        LOG_INFO("CoreApproval", QStringLiteral("Approval %2 action=%1 requester=%3 kind=%4")
                                     .arg(request.tool,
                                          verdict == ToolApprovalVerdict::Approved   ? QStringLiteral("approved")
                                          : verdict == ToolApprovalVerdict::Denied   ? QStringLiteral("denied")
                                          : verdict == ToolApprovalVerdict::TimedOut ? QStringLiteral("timed out")
                                                                                     : QStringLiteral("unavailable"),
                                          request.client, approval::kind_of(request)));
        // The continuation re-finds and performs the approved step on the GUI
        // thread; whatever it throws must not reach the event loop.
        QMetaObject::invokeMethod(
            app,
            [done, verdict]() {
                if (!done)
                    return;
                try {
                    done(verdict);
                } catch (const std::exception& e) {
                    LOG_ERROR("CoreApproval", QStringLiteral("approval continuation threw: %1").arg(QString::fromUtf8(e.what())));
                } catch (...) {
                    LOG_ERROR("CoreApproval", QStringLiteral("approval continuation threw an unknown exception"));
                }
            },
            Qt::QueuedConnection);
    });
}

QString approval_refusal_text(ToolApprovalVerdict verdict, const QString& what) {
    switch (verdict) {
        case ToolApprovalVerdict::Approved:
            return {};
        case ToolApprovalVerdict::Denied:
            return QStringLiteral("The user declined: %1. Do not retry it; tell the user what you wanted to do.")
                .arg(what);
        case ToolApprovalVerdict::TimedOut:
            return QStringLiteral(
                       "Nobody approved \"%1\" in Fincept Terminal in time, so it was not done. Ask the user "
                       "to approve it and retry.")
                .arg(what);
        case ToolApprovalVerdict::Unavailable:
            return QStringLiteral("\"%1\" needs the user's approval, but no Fincept Terminal window is open to ask in, "
                                  "so it was not done.")
                .arg(what);
    }
    return {};
}

} // namespace fincept::mcp::core
