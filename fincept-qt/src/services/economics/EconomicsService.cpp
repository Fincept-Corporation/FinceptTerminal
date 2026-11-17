// src/services/economics/EconomicsService.cpp
#include "services/economics/EconomicsService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "python/PythonRunner.h"
#include "services/economics/EconomicsDecode.h"
#include "storage/cache/CacheManager.h"

#include <QFuture>
#include <QPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

namespace fincept::services {

namespace {

/// The service's decode pool: cache reads, JSON parses of script answers (the EIA petroleum default
/// was 7.6 MB / 23,514 rows) and cache writes run here, never on the GUI thread. Two threads, so one
/// multi-MB answer does not hold up a small one behind it. A child of the process-lifetime service,
/// so its destructor waits for a decode still in flight; CacheManager opens a per-thread connection.
QThreadPool* econ_svc_decode_pool() {
    static QThreadPool* const pool = []() {
        auto* p = new QThreadPool(&EconomicsService::instance());
        p->setObjectName(QStringLiteral("EconomicsDecode"));
        p->setMaxThreadCount(2);
        return p;
    }();
    return pool;
}

} // namespace

EconomicsService& EconomicsService::instance() {
    static EconomicsService inst;
    return inst;
}

EconomicsService::EconomicsService(QObject* parent) : QObject(parent) {}

// ── Cache ─────────────────────────────────────────────────────────────────────

// "v2" entries are {"fetched_at": <ISO UTC>, "data": {...}} so a cache hit can report when the data
// was actually retrieved. The prefix keeps pre-v2 entries (bare data objects) from being misread.
QString EconomicsService::cache_key(const QString& script, const QString& command, const QStringList& args) const {
    return "economics:v2:" + script + ":" + command + ":" + args.join(",");
}

// ── Execute ───────────────────────────────────────────────────────────────────

void EconomicsService::execute(const QString& source_id, const QString& script, const QString& command,
                               const QStringList& args, const QString& request_id) {
    start(source_id, script, command, args, request_id, false);
}

void EconomicsService::start(const QString& source_id, const QString& script, const QString& command,
                             const QStringList& args, const QString& request_id, bool fresh) {
    const QString key = cache_key(script, command, args);

    // Remember the dispatch so hub-driven refresh() can replay it later.
    const QString topic = hub_topic(source_id, request_id);
    dispatch_records_.insert(topic, DispatchRecord{source_id, script, command, args, request_id});

    // The identical call is already in flight for this id: wait for it instead of a second process.
    // Another call under the same id replaces it (its answer would be for the old arguments).
    if (auto running = inflight_.find(request_id); running != inflight_.end()) {
        if (running->key == key) {
            ++running->waiters;
            if (fresh)
                running->fresh = true; // a cached answer still being read must not satisfy a refresh
            LOG_DEBUG("EconomicsService", logf("Joined the run in flight", {{"request", request_id}, {"waiters", running->waiters}}));
            return;
        }
        drop_inflight(request_id, "superseded");
    }

    Inflight run;
    run.seq = ++next_seq_;
    run.key = key;
    run.token = std::make_shared<std::atomic<bool>>(false);
    run.fresh = fresh;
    inflight_.insert(request_id, run);
    const quint64 seq = run.seq;

    if (fresh) {
        start_script(source_id, script, command, args, request_id, seq);
        return;
    }

    // The cache lookup reads (and parses) up to several MB: on the decode pool. A miss comes back
    // with success = false and starts the script.
    const auto token = run.token;
    (void)QtConcurrent::run(econ_svc_decode_pool(), [key, source_id, token]() {
        if (token->load())
            return EconomicsResult{}; // cancelled while queued
        const std::optional<QString> cached = fincept::CacheManager::instance().try_get(key);
        if (!cached || cached->isEmpty())
            return EconomicsResult{};
        return econ_decode::decode_cached(*cached, source_id);
    }).then(this, [this, source_id, script, command, args, request_id, seq, key](const EconomicsResult& res) {
        const auto live = inflight_.constFind(request_id);
        if (live == inflight_.constEnd() || live->seq != seq)
            return; // cancelled or superseded during the lookup
        if (res.success && !live->fresh) {
            LOG_DEBUG("EconomicsService", "Cache hit: " + key);
            finish(request_id, seq, res);
            return;
        }
        start_script(source_id, script, command, args, request_id, seq);
    });
}

void EconomicsService::start_script(const QString& source_id, const QString& script, const QString& command,
                                    const QStringList& args, const QString& request_id, quint64 seq) {
    const auto live = inflight_.constFind(request_id);
    if (live == inflight_.constEnd() || live->seq != seq)
        return;
    const QString key = live->key;
    const auto token = live->token;

    QStringList full_args;
    full_args << command << args;

    // Argument lists are user input (and can carry whole JSON payloads): only their count is logged.
    LOG_DEBUG("EconomicsService", logf("Economics script started", {{"script", script}, {"command", command},
                                                                    {"args", static_cast<int>(args.size())}}));

    python::PythonRunner::RunOptions opts;
    opts.cancel_token = token;

    QPointer<EconomicsService> self = this;
    python::PythonRunner::instance().run_with_options(
        script, full_args, opts, [self, source_id, request_id, key, seq, token](python::PythonResult py) {
            if (!self)
                return;
            // Cancelled or superseded meanwhile: nobody waits for this answer any more.
            const auto run = self->inflight_.constFind(request_id);
            if (run == self->inflight_.constEnd() || run->seq != seq) {
                LOG_DEBUG("EconomicsService", logf("Dropped the answer of a cancelled run", {{"request", request_id}}));
                return;
            }
            const QDateTime fetched_at = QDateTime::currentDateTimeUtc();
            // Extraction, parse, checks and the cache write of the answer: off the GUI thread.
            (void)QtConcurrent::run(econ_svc_decode_pool(), [py = std::move(py), source_id, key, fetched_at, token]() {
                if (token->load())
                    return econ_decode::DecodedReply{}; // cancelled before the decode started
                econ_decode::DecodedReply reply = econ_decode::decode_reply(py, source_id, fetched_at);
                if (reply.result.success && !token->load())
                    fincept::CacheManager::instance().put(key, QVariant(econ_decode::cache_text(reply.result)),
                                                          kCacheTtlSec, QStringLiteral("economics"));
                return reply;
            }).then(self.data(), [self, request_id, seq](const econ_decode::DecodedReply& reply) {
                if (!self)
                    return;
                const auto still = self->inflight_.constFind(request_id);
                if (still == self->inflight_.constEnd() || still->seq != seq) {
                    LOG_DEBUG("EconomicsService", logf("Dropped the answer of a cancelled run", {{"request", request_id}}));
                    return;
                }
                if (!reply.log_message.isEmpty()) {
                    if (reply.log_warning)
                        LOG_WARN("EconomicsService", reply.log_message);
                    else
                        LOG_ERROR("EconomicsService", reply.log_message);
                }
                if (reply.result.success)
                    LOG_DEBUG("EconomicsService", "Result ready: " + request_id);
                self->finish(request_id, seq, reply.result);
            });
        });
}

void EconomicsService::finish(const QString& request_id, quint64 seq, const EconomicsResult& result) {
    const auto live = inflight_.constFind(request_id);
    if (live == inflight_.constEnd() || live->seq != seq)
        return;
    inflight_.remove(request_id);
    emit result_ready(request_id, result);
    if (!hub_registered_)
        return;
    // A failed refresh must clear the hub's in_flight flag, or the topic stays "busy" until the
    // scheduler's refresh_timeout fires and a retry is silently refused.
    const QString topic = hub_topic(result.source_id, request_id);
    if (result.success)
        fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(result));
    else
        fincept::datahub::DataHub::instance().publish_error(topic, result.error);
}

// ── Cancel ───────────────────────────────────────────────────────────────────

void EconomicsService::drop_inflight(const QString& request_id, const char* why) {
    const auto it = inflight_.constFind(request_id);
    if (it == inflight_.constEnd())
        return;
    if (it->token)
        it->token->store(true); // queued: never spawned; running: killed within ~250 ms; decoding: dropped
    inflight_.remove(request_id);
    LOG_DEBUG("EconomicsService", logf("Economics run stopped", {{"request", request_id}, {"reason", why}}));
}

void EconomicsService::cancel(const QString& request_id) {
    const auto it = inflight_.find(request_id);
    if (it == inflight_.end())
        return;
    if (--it->waiters > 0)
        return; // somebody else still waits for this run
    drop_inflight(request_id, "cancelled");
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

QString EconomicsService::hub_topic(const QString& source_id, const QString& request_id) {
    return QStringLiteral("econ:") + source_id + QLatin1Char(':') + request_id;
}

QString EconomicsService::remember_topic(const QString& source_id, const QString& script, const QString& command,
                                         const QStringList& args, const QString& request_id) {
    const QString topic = hub_topic(source_id, request_id);
    dispatch_records_.insert(topic, DispatchRecord{source_id, script, command, args, request_id});
    return topic;
}

QStringList EconomicsService::topic_patterns() const {
    // Single broad pattern — individual source refactor to explicit
    // patterns can happen later once panels migrate. Keeps producer
    // registration simple across 32 upstream sources.
    return {QStringLiteral("econ:*")};
}

void EconomicsService::refresh(const QStringList& topics) {
    // Replay the last-known dispatch for each topic. If we've never
    // seen an execute() for this topic, nothing to do — we don't know
    // which script + args produce it.
    for (const auto& topic : topics) {
        auto it = dispatch_records_.constFind(topic);
        if (it == dispatch_records_.constEnd()) {
            LOG_DEBUG("EconomicsService", "refresh() for unknown topic (no prior execute): " + topic);
            continue;
        }
        const DispatchRecord rec = it.value();
        // Force a re-fetch: the cached entry goes (no other request id reads it back either) and the
        // run skips the lookup.
        fincept::CacheManager::instance().remove(cache_key(rec.script, rec.command, rec.args));
        start(rec.source_id, rec.script, rec.command, rec.args, rec.request_id, true);
    }
}

int EconomicsService::max_requests_per_sec() const {
    return 2; // Python spawn pacing — upstream rate limits are usually higher
}

void EconomicsService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // 1-hour TTL, 60s min interval — economics series move slowly.
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 60 * 60 * 1000;
    policy.min_interval_ms = 60 * 1000;
    policy.push_only = false;
    hub.set_policy_pattern(QStringLiteral("econ:*"), policy);

    hub_registered_ = true;
    LOG_DEBUG("EconomicsService", "Registered with DataHub (econ:*)");
}

} // namespace fincept::services
