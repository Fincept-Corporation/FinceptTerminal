// HarnessArenaClient.cpp — Alpha Arena decisions answered by an agent harness
// (see the header). The pure parts are in HarnessArenaLogic.h.

#include "services/alpha_arena/HarnessArenaClient.h"

#include "core/logging/Logger.h"
#include "services/alpha_arena/HarnessArenaLogic.h"
#include "services/harness/HarnessChatBackend.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/ProviderCatalog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QPointer>
#include <QThreadPool>

namespace fincept::arena {

namespace harness_arena_client_detail {

constexpr const char* kHacTag = "ArenaHarness";

/// Shared by every client; deliberately leaked so process exit never waits on
/// a decision still running (the call is cancelled through `alive` instead).
QThreadPool& hac_pool() {
    static QThreadPool* const pool = []() {
        auto* p = new QThreadPool();
        p->setObjectName(QStringLiteral("fincept-arena-harness"));
        p->setMaxThreadCount(HarnessArenaClient::kMaxConcurrentCalls);
        p->setExpiryTimeout(60 * 1000);
        return p;
    }();
    return *pool;
}

ArenaLlmResult hac_to_result(const harness_arena::ArenaHarnessOutcome& o) {
    ArenaLlmResult r;
    r.content = o.content;
    r.error = o.error;
    r.prompt_tokens = o.prompt_tokens;
    r.completion_tokens = o.completion_tokens;
    r.latency_ms = o.latency_ms;
    r.success = o.success;
    return r;
}

} // namespace harness_arena_client_detail

using namespace harness_arena_client_detail;

HarnessArenaClient::HarnessArenaClient(QObject* parent) : IArenaLlmClient(parent) {}

HarnessArenaClient::~HarnessArenaClient() {
    alive_->store(false); // a decision still running stops (process tree killed); its callback is dropped
}

void HarnessArenaClient::complete(const ArenaLlmRequest& req, std::function<void(ArenaLlmResult)> cb) {
    const QString harness_id = ai_chat::ProviderCatalog::harness_id(req.provider);
    auto& reg = harness::HarnessRegistry::instance();
    const harness::IHarnessAdapter* adapter = reg.adapter(harness_id);
    if (!adapter) {
        ArenaLlmResult r;
        r.error = QStringLiteral("no harness \"%1\" in this build").arg(harness_id.left(40));
        cb(r);
        return;
    }
    // Registry reads stay on this (GUI) thread; the worker gets plain values.
    const harness::Capabilities& caps = adapter->descriptor().caps;
    const QString effort = caps.effort ? reg.default_effort(harness_id) : QString();
    harness::HarnessChatRequest hreq = harness_arena::build_request(harness_id, req.model_id, effort, req.system_prompt,
                                                                    req.user_prompt, req.timeout_ms, caps.json_schema);
    const qint64 enqueued_ms = QDateTime::currentMSecsSinceEpoch();
    const int budget_ms = hreq.timeout_ms;
    QPointer<HarnessArenaClient> self(this);
    const std::shared_ptr<std::atomic<bool>> alive = alive_;

    hac_pool().start([self, alive, hreq, cb, enqueued_ms, budget_ms]() mutable {
        if (!alive->load())
            return;
        harness_arena::ArenaHarnessOutcome out;
        // A decision that waited behind others for its whole budget is late
        // already: answer "timeout" without starting a process.
        const qint64 waited = QDateTime::currentMSecsSinceEpoch() - enqueued_ms;
        if (waited >= budget_ms) {
            out.error = QStringLiteral("timeout");
            out.latency_ms = waited;
        } else {
            const int remaining = static_cast<int>(budget_ms - waited);
            hreq.timeout_ms = remaining;
            hreq.first_event_timeout_ms = qMin(hreq.first_event_timeout_ms, remaining);
            QString structured;
            harness::HarnessChatSink sink;
            sink.cancelled = [alive]() { return !alive->load(); };
            sink.event = [&structured](const services::coding_agents::AgentEvent& ev) {
                if (ev.kind != services::coding_agents::EventKind::Usage)
                    return;
                const QString so = harness_arena::structured_output_from_result_raw(ev.raw_json);
                if (!so.isEmpty())
                    structured = so;
            };
            const harness::HarnessChatResult r = harness::HarnessChatBackend::complete(hreq, sink);
            out = harness_arena::map_result(r, structured);
            out.latency_ms = waited + r.duration_ms;
        }
        LOG_DEBUG(kHacTag, QStringLiteral("Arena decision via %1: %2 in %3 ms")
                               .arg(hreq.harness_id, out.success ? QStringLiteral("answered") : out.error.left(60))
                               .arg(out.latency_ms));
        const ArenaLlmResult result = hac_to_result(out);
        QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return;
        QMetaObject::invokeMethod(
            app,
            [self, alive, cb, result]() {
                if (!self || !alive->load())
                    return; // the client (and its engine) went away mid-call
                cb(result);
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::arena
