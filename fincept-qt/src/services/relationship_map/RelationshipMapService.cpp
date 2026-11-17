// src/services/relationship_map/RelationshipMapService.cpp
#include "services/relationship_map/RelationshipMapService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "python/PythonRunner.h"
#include "services/relationship_map/RelationshipMapParse.h"
#include "storage/cache/CacheManager.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace fincept::services {

using namespace fincept::relmap;

namespace {

constexpr int kRmsCompleteTtlSec = 36 * 3600; // the screen refreshes quietly after 24 h
constexpr int kRmsPartialTtlSec = 30 * 60;    // a partial map is retried soon, not kept for a day
constexpr int kRmsScriptTimeoutMs = 150 * 1000;

QString rms_cache_key(const QString& ticker) {
    return QStringLiteral("relmap:v2:") + ticker;
}

QString rms_norm(const QString& ticker) {
    return ticker.trimmed().toUpper();
}

QString rms_run_key(const QString& ticker, QStringList sections) {
    std::sort(sections.begin(), sections.end());
    return ticker + QLatin1Char('|') + sections.join(QLatin1Char(','));
}

/// A remembered payload, read and parsed on a worker thread.
struct RmsCached {
    bool found = false;
    RelationshipData data;
    QDateTime as_of;
};

RmsCached rms_read_cache(const QString& ticker) {
    RmsCached out;
    const QVariant v = fincept::CacheManager::instance().get(rms_cache_key(ticker));
    if (v.isNull())
        return out;
    RelParseOutcome p = parse_payload(v.toString().toUtf8(), ticker);
    if (!p.ok) {
        fincept::CacheManager::instance().remove(rms_cache_key(ticker)); // an unreadable entry is a miss
        return out;
    }
    out.found = true;
    out.as_of = p.data.fetched_at.isValid() ? p.data.fetched_at : QDateTime::currentDateTimeUtc();
    out.data = std::move(p.data);
    return out;
}

/// What a finished run produced, prepared on a worker thread (merge, parse, cache write).
struct RmsDone {
    bool ok = false;
    QString error;
    RelationshipData data;
    QDateTime as_of;
};

RmsDone rms_prepare(const QString& ticker, const QStringList& sections, const QString& output) {
    RmsDone out;
    const QJsonDocument doc = QJsonDocument::fromJson(output.trimmed().toUtf8());
    if (!doc.isObject()) {
        out.error = QStringLiteral("The relationship data could not be read (invalid reply).");
        return out;
    }
    QJsonObject root = doc.object();
    if (root.contains(QLatin1String("error"))) {
        out.error = root.value(QLatin1String("error")).toString().simplified();
        if (out.error.isEmpty())
            out.error = QStringLiteral("The relationship data could not be loaded.");
        return out;
    }
    if (!sections.isEmpty()) {
        // A retry of some sections: fold it into the remembered full payload.
        const QVariant base = fincept::CacheManager::instance().get(rms_cache_key(ticker));
        const QJsonDocument bdoc = QJsonDocument::fromJson(base.toString().toUtf8());
        if (bdoc.isObject())
            root = merge_retry_json(bdoc.object(), root, sections);
    }
    const QByteArray text = QJsonDocument(root).toJson(QJsonDocument::Compact);
    RelParseOutcome p = parse_payload(text, ticker);
    if (!p.ok) {
        out.error = p.error;
        return out;
    }
    // A complete map is remembered for a day and a half, a partial one briefly.
    fincept::CacheManager::instance().put(rms_cache_key(ticker), QVariant(QString::fromUtf8(text)),
                                          p.data.partial ? kRmsPartialTtlSec : kRmsCompleteTtlSec, "relmap");
    out.ok = true;
    out.as_of = p.data.fetched_at.isValid() ? p.data.fetched_at : QDateTime::currentDateTimeUtc();
    out.data = std::move(p.data);
    return out;
}

QString rms_section_counts(const RelationshipData& d) {
    QStringList parts;
    QStringList ids = d.sections.keys();
    std::sort(ids.begin(), ids.end());
    for (const QString& id : ids) {
        const RelSectionStatus& s = d.sections[id];
        parts << QStringLiteral("%1=%2:%3").arg(id, section_state_text(s.state)).arg(s.count);
    }
    return parts.join(QLatin1Char(' '));
}

} // namespace

RelationshipMapService& RelationshipMapService::instance() {
    static RelationshipMapService s;
    return s;
}

// ── Requests ────────────────────────────────────────────────────────────────

quint64 RelationshipMapService::request(const QString& ticker, const RelMapRequestOptions& opts) {
    const quint64 id = ++next_request_;
    const QString t = rms_norm(ticker);
    if (t.isEmpty()) {
        QMetaObject::invokeMethod(
            this, [this, id]() { emit request_failed(id, QStringLiteral("No symbol to analyse.")); },
            Qt::QueuedConnection);
        return id;
    }
    request_run_.insert(id, QString()); // waiting for the remembered-payload look-up
    const bool force = opts.force;
    const QStringList sections = opts.sections;
    QPointer<RelationshipMapService> self = this;
    auto* watcher = new QFutureWatcher<RmsCached>(this);
    connect(watcher, &QFutureWatcher<RmsCached>::finished, this, [this, self, watcher, id, t, force, sections]() {
        watcher->deleteLater();
        if (!self || !request_run_.contains(id))
            return; // cancelled meanwhile
        const RmsCached c = watcher->result();
        if (c.found && !force && sections.isEmpty()) {
            request_run_.remove(id);
            LOG_INFO("RelMapService", logf("Map from the remembered payload",
                                           {{"ticker", t}, {"as_of", c.as_of.toString(Qt::ISODate)}}));
            emit request_finished(id, c.data, c.as_of, true);
            return;
        }
        // A section retry needs the remembered map to merge into; without it, the whole map.
        const QStringList secs = (c.found && !sections.isEmpty()) ? sections : QStringList();
        const QString key = rms_run_key(t, secs);
        if (!runs_.contains(key))
            start_run(key, t, secs);
        runs_[key].requests.insert(id);
        request_run_.insert(id, key);
    });
    watcher->setFuture(QtConcurrent::run([t]() { return rms_read_cache(t); }));
    return id;
}

void RelationshipMapService::cancel(quint64 request_id) {
    if (!request_run_.contains(request_id))
        return;
    const QString key = request_run_.take(request_id);
    if (key.isEmpty())
        return; // still in the cache look-up: its answer is dropped
    auto it = runs_.find(key);
    if (it == runs_.end())
        return;
    it->requests.remove(request_id);
    if (it->requests.isEmpty() && !it->for_hub) {
        if (it->cancel)
            it->cancel->store(true); // PythonRunner kills the script within ~250 ms
        LOG_DEBUG("RelMapService", logf("Run abandoned", {{"ticker", it->ticker}}));
        runs_.erase(it); // its callback no longer finds it: nothing is cached or emitted
    }
}

bool RelationshipMapService::is_pending(quint64 request_id) const {
    return request_run_.contains(request_id);
}

void RelationshipMapService::load_remembered(
    const QString& ticker, QObject* ctx,
    std::function<void(bool, const fincept::relmap::RelationshipData&, const QDateTime&)> done) {
    const QString t = rms_norm(ticker);
    QPointer<QObject> guard = ctx;
    auto* watcher = new QFutureWatcher<RmsCached>(this);
    connect(watcher, &QFutureWatcher<RmsCached>::finished, this, [watcher, guard, done]() {
        watcher->deleteLater();
        if (!guard || !done)
            return;
        const RmsCached c = watcher->result();
        done(c.found, c.data, c.as_of);
    });
    watcher->setFuture(QtConcurrent::run([t]() { return t.isEmpty() ? RmsCached{} : rms_read_cache(t); }));
}

// ── Runs ────────────────────────────────────────────────────────────────────

void RelationshipMapService::start_run(const QString& key, const QString& ticker, const QStringList& sections) {
    Run run;
    run.key = key;
    run.ticker = ticker;
    run.sections = sections;
    run.cancel = std::make_shared<std::atomic<bool>>(false);
    run.seq = ++next_seq_;
    run.started_ms = QDateTime::currentMSecsSinceEpoch();
    run.snapshot.company.ticker = ticker;
    run.snapshot.schema = 2;
    const quint64 seq = run.seq;
    runs_.insert(key, run);

    QStringList args{ticker};
    if (!sections.isEmpty())
        args << QStringLiteral("--sections") << sections.join(QLatin1Char(','));
    LOG_INFO("RelMapService", logf("Load started", {{"ticker", ticker},
                                                    {"sections", sections.isEmpty() ? QStringLiteral("all")
                                                                                    : sections.join(QLatin1Char(','))}}));

    python::PythonRunner::RunOptions opts;
    opts.cancel_token = run.cancel;
    opts.timeout_ms = kRmsScriptTimeoutMs;
    opts.stream_stdout = false; // progress on stderr; the one big JSON on stdout is never scanned per line
    QPointer<RelationshipMapService> self = this;
    python::PythonRunner::instance().run_with_options(
        QStringLiteral("relationship_map.py"), args, opts,
        [self, key, seq](python::PythonResult result) {
            if (self)
                self->on_run_done(key, seq, result.success, result.output, result.error);
        },
        [self, key, seq](QString line, bool is_stderr) {
            if (self && is_stderr)
                self->on_run_line(key, seq, line);
        });
}

void RelationshipMapService::on_run_line(const QString& key, quint64 seq, const QString& line) {
    auto it = runs_.find(key);
    if (it == runs_.end() || it->seq != seq)
        return;
    RelStreamEvent ev;
    if (!parse_stream_line(line, &ev))
        return; // library chatter
    apply_stream_event(it->snapshot, ev);
    const QList<quint64> ids = it->requests.values();
    if (ev.kind == QLatin1String("plan")) {
        for (quint64 id : ids)
            emit request_planned(id, it->ticker, ev.planned);
        return;
    }
    const RelationshipData snap = it->snapshot;
    for (quint64 id : ids)
        emit request_section(id, snap, ev.id);
}

void RelationshipMapService::on_run_done(const QString& key, quint64 seq, bool success, const QString& output,
                                         const QString& error) {
    auto it = runs_.find(key);
    if (it == runs_.end() || it->seq != seq) {
        LOG_DEBUG("RelMapService", logf("Dropped the output of an abandoned run", {{"run", key}}));
        return;
    }
    if (!success || output.trimmed().isEmpty()) {
        // The script's own sentence when it printed one ("Yahoo Finance did not answer in time…").
        QString msg = QJsonDocument::fromJson(output.trimmed().toUtf8()).object().value(QLatin1String("error")).toString();
        if (msg.trimmed().isEmpty())
            msg = error.trimmed().isEmpty() ? QStringLiteral("Failed to fetch data for %1").arg(it->ticker)
                                            : error.simplified().left(300);
        finish_run(key, seq, false, {}, msg, {});
        return;
    }
    const QString ticker = it->ticker;
    const QStringList sections = it->sections;
    QPointer<RelationshipMapService> self = this;
    auto* watcher = new QFutureWatcher<RmsDone>(this);
    connect(watcher, &QFutureWatcher<RmsDone>::finished, this, [self, watcher, key, seq]() {
        watcher->deleteLater();
        if (!self)
            return;
        const RmsDone r = watcher->result();
        self->finish_run(key, seq, r.ok, r.data, r.error, r.as_of);
    });
    watcher->setFuture(QtConcurrent::run([ticker, sections, output]() { return rms_prepare(ticker, sections, output); }));
}

void RelationshipMapService::finish_run(const QString& key, quint64 seq, bool ok, const RelationshipData& data,
                                        const QString& error, const QDateTime& as_of) {
    auto it = runs_.find(key);
    if (it == runs_.end() || it->seq != seq)
        return; // abandoned while its output was being parsed
    const Run run = *it;
    runs_.erase(it);
    const qint64 ms = QDateTime::currentMSecsSinceEpoch() - run.started_ms;
    if (ok) {
        LOG_INFO("RelMapService", logf("Load finished", {{"ticker", run.ticker},
                                                         {"ms", QString::number(ms)},
                                                         {"partial", data.partial ? "yes" : "no"},
                                                         {"sections", rms_section_counts(data)}}));
    } else {
        LOG_WARN("RelMapService", logf("Load failed", {{"ticker", run.ticker}, {"ms", QString::number(ms)},
                                                       {"error", error.left(200)}}));
    }
    for (quint64 id : run.requests) {
        request_run_.remove(id);
        if (ok)
            emit request_finished(id, data, as_of, false);
        else
            emit request_failed(id, error);
    }
    if (run.for_hub) {
        if (ok)
            publish_to_hub(data);
        else
            hub_error(run.ticker, error);
    }
}

// ── DataHub ─────────────────────────────────────────────────────────────────

void RelationshipMapService::publish_to_hub(const RelationshipData& data) {
    if (hub_registered_ && !data.company.ticker.isEmpty())
        fincept::datahub::DataHub::instance().publish(
            QStringLiteral("geopolitics:relationship_graph:") + data.company.ticker, QVariant::fromValue(data));
}

void RelationshipMapService::hub_error(const QString& ticker, const QString& message) {
    if (hub_registered_ && !ticker.isEmpty())
        fincept::datahub::DataHub::instance().publish_error(QStringLiteral("geopolitics:relationship_graph:") + ticker,
                                                            message.left(200));
}

void RelationshipMapService::fetch(const QString& ticker) {
    const QString t = rms_norm(ticker);
    if (t.isEmpty())
        return;
    // The remembered map first (worker read); a run only when there is none.
    load_remembered(t, this, [this, t](bool found, const RelationshipData& data, const QDateTime&) {
        if (found && !data.partial) {
            publish_to_hub(data);
            return;
        }
        const QString key = rms_run_key(t, {});
        if (!runs_.contains(key))
            start_run(key, t, {});
        runs_[key].for_hub = true;
    });
}

QStringList RelationshipMapService::topic_patterns() const {
    return {QStringLiteral("geopolitics:relationship_graph:*")};
}

void RelationshipMapService::refresh(const QStringList& topics) {
    for (const auto& topic : topics) {
        const QStringList parts = topic.split(QLatin1Char(':'));
        if (parts.size() != 3) // geopolitics:relationship_graph:<ticker>
            continue;
        fetch(parts[2]);
    }
}

int RelationshipMapService::max_requests_per_sec() const {
    return 1; // heavy multi-source aggregation per call
}

void RelationshipMapService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);
    // Consumers are dashboards, not tickers: a map is good for hours.
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 6 * 3600 * 1000;
    policy.min_interval_ms = 2 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("geopolitics:relationship_graph:*"), policy);
    hub_registered_ = true;
    LOG_DEBUG("RelMapService", "Registered with DataHub (geopolitics:relationship_graph:*)");
}

} // namespace fincept::services
