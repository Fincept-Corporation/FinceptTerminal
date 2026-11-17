// HarnessChatBackend.cpp — terminal AI through an agent harness (see the header).

#include "services/harness/HarnessChatBackend.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "core/process/HostExec.h"
#include "mcp/server/McpHttpServer.h"
#include "services/harness/HarnessChatRun.h"
#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessLaunch.h"
#include "services/harness/HarnessProcessPool.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/ModelRouter.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QSemaphore>
#include <QThread>

#include <atomic>
#include <memory>
#include <optional>

namespace fincept::harness {

namespace harness_chat_backend_detail {

constexpr const char* kHcbTag = "HarnessChat";
constexpr int kHcbPrepareTimeoutMs = 30000;
constexpr qint64 kHcbTokenSlackMs = 2LL * 60 * 1000;

bool hcb_on_gui_thread() {
    QCoreApplication* app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

/// Everything a call needs from GUI-thread objects (registry, MCP server).
struct HcbPrepared {
    bool ok = false;
    FailureKind kind = FailureKind::Other;
    QString error;
    const IHarnessAdapter* adapter = nullptr; // owned by the registry for the app's lifetime
    Installation inst;
    ChildEnvOptions env_opts; // strip list + the run token
    std::optional<McpEndpoint> mcp;
    QString run_token; // retired after the call; never logged
    QString scratch_root;
    bool require_key_source = true;
    QString display_name;
};

/// What a harness state means for a call that was routed to it a moment ago.
FailureKind hcb_kind_for(TerminalState st) {
    switch (st) {
        case TerminalState::CoolingDown:
            return FailureKind::RateLimit;
        case TerminalState::SignedOut:
        case TerminalState::SubscriptionOnly:
        case TerminalState::KeyStripped:
        case TerminalState::ConsentNeeded:
            return FailureKind::Auth;
        default:
            break;
    }
    return FailureKind::NotInstalled;
}

/// GUI thread.
HcbPrepared hcb_prepare(const HarnessChatRequest& req) {
    HcbPrepared p;
    auto& reg = HarnessRegistry::instance();
    const IHarnessAdapter* adapter = reg.adapter(req.harness_id);
    if (!adapter) {
        p.kind = FailureKind::NotInstalled;
        p.error = QStringLiteral("This build has no harness “%1”.").arg(req.harness_id.left(40));
        return p;
    }
    const HarnessDescriptor& d = adapter->descriptor();
    p.display_name = d.display_name;
    const HarnessStatus st = reg.status(req.harness_id);
    if (!st.installation) {
        p.kind = FailureKind::NotInstalled;
        p.error = st.host_blocked ? st.detect_error : QStringLiteral("%1 is not installed.").arg(d.display_name);
        return p;
    }
    // The route was chosen a moment ago; the user may have changed something
    // since: the eligibility rule again, from the registry as it is now.
    const std::optional<ai_chat::RouterHarness> now = ai_chat::ModelRouter::instance().current_harness(req.harness_id);
    if (!now) {
        p.kind = FailureKind::NotInstalled;
        p.error = QStringLiteral("%1 is not available for terminal AI.").arg(d.display_name);
        return p;
    }
    if (!now->eligibility.usable) {
        p.kind = hcb_kind_for(now->eligibility.state);
        p.error = now->eligibility.reason;
        return p;
    }
    p.require_key_source = now->eligibility.require_api_key_source;

    p.adapter = adapter;
    p.inst = *st.installation;
    if (reg.env_policy(req.harness_id) == QLatin1String(HarnessRegistry::kEnvPolicySubscriptionOnly) && !d.api_key_only)
        p.env_opts.strip = d.api_key_env_vars;

    if (req.tools != ChatTools::None) {
        auto& srv = mcp::McpHttpServer::instance();
        if (!srv.is_running() && !srv.start())
            LOG_WARN(kHcbTag, QStringLiteral("Terminal tools unavailable for this call: %1").arg(srv.last_error()));
        if (srv.is_running()) {
            mcp::RunTokenPolicy policy;
            policy.scope = req.tools == ChatTools::NoNavigation ? mcp::RunToolScope::NoNavigation : mcp::RunToolScope::All;
            policy.paper_only = req.paper_only;
            policy.label = (req.feature.isEmpty() ? QStringLiteral("terminal AI") : req.feature) + QStringLiteral(" · ") +
                           req.harness_id;
            policy.harness_id = req.harness_id; // the server shapes its answers for this client (McpClientProfile.h)
            p.run_token = srv.begin_run(policy, static_cast<qint64>(req.timeout_ms) + kHcbTokenSlackMs);
            McpEndpoint ep;
            // Never the other edition's entry name ("fincept"): this edition's constant.
            ep.name = d.run_mcp_server_name.isEmpty() ? QString::fromLatin1(kMcpServerName) : d.run_mcp_server_name;
            ep.url = srv.url();
            ep.token_env_var = QString::fromLatin1(kMcpTokenEnvVar);
            ep.tool_timeout_s = HarnessChatBackend::kMcpToolTimeoutS;
            p.mcp = ep;
            p.env_opts.mcp_token = p.run_token;
        }
    }
    // Flatpak: spelled so the CLI on the host finds the call's folder and files.
    p.scratch_root = process::host::shared_path(AppPaths::cache() + QStringLiteral("/harness_chat"));
    p.ok = true;
    return p;
}

/// Run `fn` on the GUI thread and wait (bounded, cancellable) for its result.
/// nullopt = cancelled / timed out / no application.
std::optional<HcbPrepared> hcb_prepare_on_gui(const HarnessChatRequest& req, const std::function<bool()>& cancelled) {
    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return std::nullopt;
    struct State {
        QSemaphore done;
        HcbPrepared value;
        std::atomic<bool> abandoned{false};
    };
    auto state = std::make_shared<State>();
    QMetaObject::invokeMethod(
        app,
        [state, req]() {
            if (state->abandoned.load())
                return;
            state->value = hcb_prepare(req);
            if (state->abandoned.load() && !state->value.run_token.isEmpty())
                mcp::McpHttpServer::instance().end_run(state->value.run_token); // nobody will use it
            state->done.release();
        },
        Qt::QueuedConnection);
    QElapsedTimer waited;
    waited.start();
    while (!state->done.tryAcquire(1, 100)) {
        if ((cancelled && cancelled()) || waited.elapsed() > kHcbPrepareTimeoutMs) {
            state->abandoned.store(true);
            // The GUI thread may have finished in between: then its token is ours to retire.
            if (state->done.tryAcquire(1, 0) && !state->value.run_token.isEmpty())
                mcp::McpHttpServer::instance().end_run(state->value.run_token);
            return std::nullopt;
        }
    }
    return state->value;
}

QString hcb_outcome(const HarnessChatResult& r) {
    if (r.success)
        return QStringLiteral("answered");
    if (r.cancelled)
        return QStringLiteral("cancelled");
    return QStringLiteral("failed (%1%2)")
        .arg(failure_id(r.failure).isEmpty() ? QStringLiteral("other") : failure_id(r.failure),
             r.output_started ? QStringLiteral(", after output") : QString());
}

} // namespace harness_chat_backend_detail

using namespace harness_chat_backend_detail;

HarnessChatResult HarnessChatBackend::complete(const HarnessChatRequest& req, const HarnessChatSink& sink) {
    HarnessChatResult r;
    if (hcb_on_gui_thread()) {
        // Blocking the GUI thread for a whole model answer is never acceptable.
        r.failure = FailureKind::Other;
        r.error = QStringLiteral("Internal error: a harness call was made on the GUI thread.");
        LOG_ERROR(kHcbTag, "complete() called on the GUI thread — refused");
        return r;
    }
    QElapsedTimer clock;
    clock.start();
    const std::optional<HcbPrepared> prepared = hcb_prepare_on_gui(req, sink.cancelled);
    if (!prepared) {
        if (sink.cancelled && sink.cancelled()) {
            r.cancelled = true;
            r.error = QStringLiteral("Request cancelled");
        } else {
            r.failure = FailureKind::Timeout;
            r.error = QStringLiteral("The terminal did not get the harness ready in time.");
        }
        return r;
    }
    if (!prepared->ok) {
        r.failure = prepared->kind;
        r.error = prepared->error;
        LOG_INFO(kHcbTag, QStringLiteral("Terminal AI via %1 not possible for %2: %3")
                              .arg(req.harness_id, req.feature, failure_id(r.failure)));
        return r;
    }

    HarnessChatEnvironment env;
    env.adapter = prepared->adapter;
    env.installation = prepared->inst;
    const ChildEnvOptions base_opts = prepared->env_opts;
    const Installation inst = prepared->inst;
    env.environment = [base_opts, inst](const LaunchPlan& plan) {
        ChildEnvOptions o = base_opts;
        o.set = plan.env_set;
        o.unset = plan.env_unset;
        return child_environment(inst, o);
    };
    env.mcp = prepared->mcp;
    env.scratch_root = prepared->scratch_root;
    env.pool = &HarnessProcessPool::chat();
    const QString harness_id = req.harness_id;
    if (req.require_api_key_source && prepared->require_key_source)
        env.line_guard = [harness_id](const QString& line) { return subscription_session_refusal(harness_id, line); };
    env.log = [](const QString& m) { LOG_INFO(kHcbTag, m); };

    LOG_INFO(kHcbTag, QStringLiteral("Terminal AI via %1 for %2 (model %3, tools %4)")
                          .arg(req.harness_id, req.feature.isEmpty() ? QStringLiteral("?") : req.feature,
                               req.model.isEmpty() ? QStringLiteral("default") : req.model,
                               req.tools == ChatTools::None           ? QStringLiteral("off")
                               : req.tools == ChatTools::NoNavigation ? QStringLiteral("no navigation")
                                                                      : QStringLiteral("on")));
    r = run_harness_chat(env, req, sink);

    if (!prepared->run_token.isEmpty())
        mcp::McpHttpServer::instance().end_run(prepared->run_token); // the token dies with the call

    // The registry and the router learn the outcome (GUI thread).
    if (QCoreApplication* app = QCoreApplication::instance()) {
        const FailureKind kind = r.failure;
        const bool success = r.success;
        const bool subscription = r.subscription_detected;
        const qint64 retry_at = r.retry_at_ms;
        QMetaObject::invokeMethod(
            app,
            [harness_id, kind, success, subscription, retry_at]() {
                auto& reg = HarnessRegistry::instance();
                if (success)
                    reg.report_success(harness_id);
                else if (kind == FailureKind::Auth || kind == FailureKind::RateLimit ||
                         kind == FailureKind::NotInstalled || kind == FailureKind::Model)
                    reg.report_failure(harness_id, kind, retry_at);
                if (subscription)
                    ai_chat::ModelRouter::instance().refuse_harness_for(harness_id, kSubscriptionRefusalMs);
            },
            Qt::QueuedConnection);
    }
    LOG_INFO(kHcbTag, QStringLiteral("Terminal AI via %1 for %2: %3 in %4 ms (in %5 / out %6 tok)")
                          .arg(req.harness_id, req.feature.isEmpty() ? QStringLiteral("?") : req.feature, hcb_outcome(r))
                          .arg(clock.elapsed())
                          .arg(r.input_tokens)
                          .arg(r.output_tokens));
    return r;
}

} // namespace fincept::harness
