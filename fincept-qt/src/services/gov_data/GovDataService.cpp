// src/services/gov_data/GovDataService.cpp
#include "services/gov_data/GovDataService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "python/PythonRunner.h"
#include "services/gov_data/GovDataDecode.h"
#include "storage/cache/CacheManager.h"

#include <QFuture>
#include <QPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <optional>
#include <utility>

namespace fincept::services {

namespace {

/// The service's decode thread: JSON parse of a finished run, the cache read / parse of a hit and
/// the serialise + write of a new answer (CacheManager opens a connection per thread). One thread
/// keeps completions in order; a child of the process-lifetime singleton, so its destructor waits
/// for a decode in flight.
QThreadPool* gov_svc_decode_pool() {
    static QThreadPool* const pool = [] {
        auto* p = new QThreadPool(&GovDataService::instance());
        p->setObjectName(QStringLiteral("GovDataDecode"));
        p->setMaxThreadCount(1);
        return p;
    }();
    return pool;
}

/// Outcome of the cache lookup on the decode thread.
struct GovSvcLookup {
    bool hit = false;
    GovDataResult result;
};

bool gov_svc_cancelled(const GovDataService::CancelToken& token) {
    return token && token->load();
}

} // namespace

// ── Static provider list ─────────────────────────────────────────────────────
// `color` is each provider's identity colour, written for the dark theme;
// screens draw it through ui::colors::legible() / tint().

static const QVector<GovProviderInfo> kProviders = {
    {"us-treasury", "US Treasury", "U.S. Department of the Treasury",
     "Treasury securities prices, auctions & market data", "#3B82F6", "United States", "US", "government_us_data.py"},

    {"us-congress", "US Congress", "United States Congress", "Congressional bills, resolutions & legislative activity",
     "#8B5CF6", "United States", "US", "congress_gov_data.py"},

    {"canada-gov", "Canada Open Gov", "Government of Canada Open Data",
     "Open data from Canadian federal departments & agencies", "#EF4444", "Canada", "CA", "canada_gov_api.py"},

    {"swiss", "Swiss Open Data", "opendata.swiss", "Swiss federal open data portal (DE/FR/IT/EN)", "#E11D48",
     "Switzerland", "CH", "swiss_gov_api.py"},

    {"france", "France Open Data", "data.gouv.fr", "French government APIs: geographic, datasets, company registry",
     "#2563EB", "France", "FR", "french_gov_api.py"},

    {"hk", "Hong Kong Gov", "Data.gov.hk", "Hong Kong government open data portal", "#F43F5E", "Hong Kong", "HK",
     "data_gov_hk_api.py"},

    {"openafrica", "openAFRICA", "openAFRICA Open Data Portal",
     "African open data from organizations across the continent", "#F59E0B", "Africa", "AF",
     "openafrica_api.py"}, // native openAFRICA CKAN client (open.africa) — was wrongly canada_gov_api.py

    // NOTE: Spain (datos.gob.es) was REMOVED — it was mis-wired to canada_gov_api.py and
    // served CANADIAN datasets under the Spanish flag (a data-provenance bug). datos.gob.es
    // is not a CKAN portal; re-add once scripts/spain_data.py is aligned to the
    // GovDataProviderPanel CLI/response contract and validated against the live portal.

    // Multi-portal CKAN client: the panel lists the portals the script serves (`portals` command)
    // and passes the chosen one to every request. This entry used to run datagovuk_api.py, i.e.
    // data.gov.uk only, under an "8 portals" label.
    {"universal-ckan", "CKAN Portals", "Universal CKAN Open Data Portals",
     "11 CKAN portals: UK, Ireland, Italy, Netherlands, Australia, Latin America & more", "#14B8A6",
     "Multi-Country", "CKAN", "universal_ckan_api.py"},

    {"australia", "Australia Gov", "data.gov.au", "Australian government open data portal", "#0EA5E9", "Australia",
     "AU", "datagov_au_api.py"},

    // Appended (not inserted) so provider positions saved by older builds keep their meaning.
    {"uk", "UK Open Data", "data.gov.uk", "UK government open data: publishers, popular publishers, datasets",
     "#10B981", "United Kingdom", "UK", "datagovuk_api.py"},
};

const QVector<GovProviderInfo>& GovDataService::providers() {
    return kProviders;
}

const GovProviderInfo* GovDataService::provider_by_id(const QString& id) {
    for (const auto& p : kProviders) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

// ── Singleton ────────────────────────────────────────────────────────────────

GovDataService& GovDataService::instance() {
    static GovDataService inst;
    return inst;
}

GovDataService::GovDataService(QObject* parent) : QObject(parent) {}

// ── Cache ────────────────────────────────────────────────────────────────────

QString GovDataService::cache_key(const QString& script, const QString& command, const QStringList& args) const {
    return "govdata:" + script + ":" + command + ":" + args.join(",");
}

// ── Execute ──────────────────────────────────────────────────────────────────

void GovDataService::execute(const QString& script, const QString& command, const QStringList& args,
                             const QString& request_id, const CancelToken& cancel) {
    const QString key = cache_key(script, command, args);

    const QString topic = hub_topic(script, request_id);
    dispatch_records_.insert(topic, DispatchRecord{script, command, args, request_id});

    // Nothing here parses or reads the cache: the GUI thread only dispatches and reports. The cache
    // read + parse of a hit, the JSON parse of a finished run and the serialise + write of a new answer
    // all run on the decode thread (a data.gov.hk category answer is 2.5 MB). A reply therefore always
    // arrives asynchronously, cache hits included; callers match it by request id.
    QPointer<GovDataService> self = this;

    // GUI thread: report a result — log, signal, hub. A request whose cancel token is raised by now
    // reports what a cancelled script reports (its data is discarded, never delivered). Every
    // failure path also clears the hub topic's in_flight flag (publish_error); otherwise a failed
    // refresh leaves the topic "busy" until the scheduler's refresh timeout and retries are refused.
    const auto finish = [self, script, command, request_id, topic, cancel](GovDataResult result) {
        if (!self)
            return;
        if (gov_svc_cancelled(cancel) && result.error_code != QLatin1String("cancelled"))
            result = gov_decode::cancelled_result();
        if (result.success) {
            LOG_DEBUG("GovDataService", QString("Result ready: %1").arg(request_id));
            emit self->result_ready(request_id, result);
            if (self->hub_registered_)
                fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(result));
            return;
        }
        if (result.error_code == QLatin1String("cancelled")) {
            LOG_DEBUG("GovDataService", logf("Gov-data script cancelled", {{"script", script}, {"command", command}}));
        } else {
            const QString line = logf("Gov-data script failed", {{"script", script},
                                                                  {"command", command},
                                                                  {"code", result.error_code},
                                                                  {"error", result.error.left(300)}});
            // A missing key or an empty answer is the user's situation, not a fault.
            if (result.error_code == QLatin1String("missing_key") || result.error_code == QLatin1String("no_data"))
                LOG_WARN("GovDataService", line);
            else
                LOG_ERROR("GovDataService", line);
        }
        emit self->result_ready(request_id, result);
        if (self->hub_registered_)
            fincept::datahub::DataHub::instance().publish_error(topic, result.error);
    };

    // GUI thread: no usable cached answer — run the script; its output is decoded (and a good answer
    // cached) on the decode thread, then reported by `finish`.
    const auto start_script = [self, script, command, args, key, cancel, finish]() {
        if (!self)
            return;
        QStringList full_args;
        full_args << command;
        full_args << args;

        LOG_DEBUG("GovDataService",
                  logf("Gov-data script started", {{"script", script}, {"command", command}, {"args", args.size()}}));

        python::PythonRunner::RunOptions opts;
        opts.cancel_token = cancel;
        python::PythonRunner::instance().run_with_options(
            script, full_args, opts, [self, key, cancel, finish](python::PythonResult py_result) {
                if (!self)
                    return;
                // Only a hand-over here: the output (often megabytes) moves to the decode thread.
                QtConcurrent::run(gov_svc_decode_pool(),
                                  [py = std::move(py_result), key, cancel]() -> GovDataResult {
                                      if (gov_svc_cancelled(cancel))
                                          return gov_decode::cancelled_result(); // nobody wants it: skip the parse
                                      GovDataResult result = gov_decode::decode_reply(py);
                                      if (result.success)
                                          fincept::CacheManager::instance().put(
                                              key, QVariant(gov_decode::cache_text(result)), kCacheTtlSec,
                                              QStringLiteral("govdata"));
                                      return result;
                                  })
                    .then(self.data(), finish);
            });
    };

    // Serve from the cache if fresh — looked up and parsed on the decode thread.
    QtConcurrent::run(gov_svc_decode_pool(),
                      [key, cancel]() -> GovSvcLookup {
                          GovSvcLookup out;
                          if (gov_svc_cancelled(cancel))
                              return out; // reported as cancelled by the continuation
                          const std::optional<QString> text = fincept::CacheManager::instance().try_get(key);
                          if (!text)
                              return out;
                          out.result = gov_decode::decode_cached(*text);
                          out.hit = out.result.success;
                          if (!out.hit)
                              fincept::CacheManager::instance().remove(key); // unreadable copy: run the script
                          return out;
                      })
        .then(this, [self, key, cancel, finish, start_script](GovSvcLookup found) {
            if (!self)
                return;
            if (gov_svc_cancelled(cancel)) {
                finish(gov_decode::cancelled_result());
                return;
            }
            if (found.hit) {
                LOG_DEBUG("GovDataService", QString("Cache hit: %1").arg(key));
                finish(std::move(found.result));
                return;
            }
            start_script();
        });
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

QString GovDataService::hub_topic(const QString& script, const QString& request_id) {
    // Map script filename → provider id when possible; fall back to the
    // script name stripped of extension.
    QString provider_key;
    for (const auto& p : kProviders) {
        if (p.script == script) {
            provider_key = p.id;
            break;
        }
    }
    if (provider_key.isEmpty()) {
        provider_key = script;
        if (provider_key.endsWith(QLatin1String(".py")))
            provider_key.chop(3);
    }
    // One topic per caller: a per-run suffix ("gov.treasury#17") would mint a new topic (and a
    // dispatch record) for every run.
    return QStringLiteral("govdata:") + provider_key + QLatin1Char(':') + request_id.section(QLatin1Char('#'), 0, 0);
}

QStringList GovDataService::topic_patterns() const {
    return {QStringLiteral("govdata:*")};
}

void GovDataService::refresh(const QStringList& topics) {
    for (const auto& topic : topics) {
        auto it = dispatch_records_.constFind(topic);
        if (it == dispatch_records_.constEnd()) {
            LOG_DEBUG("GovDataService", "refresh() for unknown topic (no prior execute): " + topic);
            continue;
        }
        const DispatchRecord rec = it.value();
        // Drop the cached answer on the decode thread (no database write on the GUI thread). The pool
        // has one thread, so the removal lands before execute()'s cache lookup.
        gov_svc_decode_pool()->start([key = cache_key(rec.script, rec.command, rec.args)]() {
            fincept::CacheManager::instance().remove(key);
        });
        execute(rec.script, rec.command, rec.args, rec.request_id);
    }
}

int GovDataService::max_requests_per_sec() const {
    return 2;
}

void GovDataService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // 1-hour TTL, 60s min_interval — same as economics (govdata updates slowly).
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 60 * 60 * 1000;
    policy.min_interval_ms = 60 * 1000;
    policy.push_only = false;
    hub.set_policy_pattern(QStringLiteral("govdata:*"), policy);

    hub_registered_ = true;
    LOG_DEBUG("GovDataService", "Registered with DataHub (govdata:*)");
}

} // namespace fincept::services
