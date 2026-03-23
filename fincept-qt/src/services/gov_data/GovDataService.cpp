// src/services/gov_data/GovDataService.cpp
#include "services/gov_data/GovDataService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::services {

// ── Static provider list ─────────────────────────────────────────────────────

static const QVector<GovProviderInfo> kProviders = {
    {"us-treasury",    "US Treasury",        "U.S. Department of the Treasury",
     "Treasury securities prices, auctions & market data", "#3B82F6",
     "United States", "US", "government_us_data.py"},

    {"us-congress",    "US Congress",         "United States Congress",
     "Congressional bills, resolutions & legislative activity", "#8B5CF6",
     "United States", "US", "congress_gov_data.py"},

    {"canada-gov",     "Canada Open Gov",     "Government of Canada Open Data",
     "Open data from Canadian federal departments & agencies", "#EF4444",
     "Canada", "CA", "canada_gov_api.py"},

    {"swiss",          "Swiss Open Data",     "opendata.swiss",
     "Swiss federal open data portal (DE/FR/IT/EN)", "#E11D48",
     "Switzerland", "CH", "swiss_gov_api.py"},

    {"france",         "France Open Data",    "data.gouv.fr",
     "French government APIs: geographic, datasets, company registry", "#2563EB",
     "France", "FR", "french_gov_api.py"},

    {"hk",             "Hong Kong Gov",       "Data.gov.hk",
     "Hong Kong government open data portal", "#F43F5E",
     "Hong Kong", "HK", "data_gov_hk_api.py"},

    {"openafrica",     "openAFRICA",          "openAFRICA Open Data Portal",
     "African open data from organizations across the continent", "#F59E0B",
     "Africa", "AF", "canada_gov_api.py"}, // Uses CKAN pattern

    {"spain",          "Spain Open Data",     "datos.gob.es",
     "Spanish government open data portal", "#DC2626",
     "Spain", "ES", "canada_gov_api.py"}, // placeholder

    {"universal-ckan", "CKAN Portals",        "Universal CKAN Open Data Portals",
     "8 CKAN portals: US, UK, Australia, Italy, Brazil & more", "#10B981",
     "Multi-Country", "CKAN", "datagovuk_api.py"},

    {"australia",      "Australia Gov",       "data.gov.au",
     "Australian government open data portal", "#0EA5E9",
     "Australia", "AU", "datagov_au_api.py"},
};

const QVector<GovProviderInfo>& GovDataService::providers() {
    return kProviders;
}

const GovProviderInfo* GovDataService::provider_by_id(const QString& id) {
    for (const auto& p : kProviders) {
        if (p.id == id) return &p;
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

QString GovDataService::cache_key(const QString& script, const QString& command,
                                   const QStringList& args) const {
    return script + ":" + command + ":" + args.join(",");
}

bool GovDataService::is_cache_fresh(const QString& key) const {
    if (!cache_.contains(key)) return false;
    auto elapsed = QDateTime::currentMSecsSinceEpoch() - cache_[key].fetched_at;
    return elapsed < kCacheTtlMs;
}

// ── Execute ──────────────────────────────────────────────────────────────────

void GovDataService::execute(const QString& script, const QString& command,
                              const QStringList& args, const QString& request_id) {
    const QString key = cache_key(script, command, args);

    // Serve from cache if fresh
    if (is_cache_fresh(key)) {
        LOG_DEBUG("GovDataService", QString("Cache hit: %1").arg(key));
        GovDataResult result;
        result.success = true;
        result.data = cache_[key].data;
        emit result_ready(request_id, result);
        return;
    }

    // Build args: command first, then extra args
    QStringList full_args;
    full_args << command;
    full_args << args;

    LOG_INFO("GovDataService",
             QString("Executing %1 %2 [%3]").arg(script, command, args.join(", ")));

    QPointer<GovDataService> self = this;
    python::PythonRunner::instance().run(script, full_args,
        [self, request_id, key](python::PythonResult py_result) {
            if (!self) return;

            GovDataResult result;

            if (!py_result.success) {
                result.success = false;
                result.error = py_result.error.isEmpty()
                    ? QString("Script exited with code %1").arg(py_result.exit_code)
                    : py_result.error;
                LOG_ERROR("GovDataService", QString("Script failed: %1").arg(result.error));
                emit self->result_ready(request_id, result);
                return;
            }

            // Extract JSON from output
            QString json_str = python::extract_json(py_result.output);
            if (json_str.isEmpty()) {
                result.success = false;
                result.error = "No JSON output from script";
                LOG_ERROR("GovDataService", "No JSON in script output");
                emit self->result_ready(request_id, result);
                return;
            }

            QJsonParseError parse_err;
            QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8(), &parse_err);
            if (doc.isNull()) {
                result.success = false;
                result.error = QString("JSON parse error: %1").arg(parse_err.errorString());
                LOG_ERROR("GovDataService", result.error);
                emit self->result_ready(request_id, result);
                return;
            }

            result.success = true;
            result.data = doc.object();

            // Cache the result
            self->cache_[key] = {result.data, QDateTime::currentMSecsSinceEpoch()};

            LOG_INFO("GovDataService", QString("Result ready: %1").arg(request_id));
            emit self->result_ready(request_id, result);
        });
}

} // namespace fincept::services
