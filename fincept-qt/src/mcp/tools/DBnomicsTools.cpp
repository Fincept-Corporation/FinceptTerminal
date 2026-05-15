// DBnomicsTools.cpp — Tools that drive the DBnomics screen.
//
// 5 tools in category "dbnomics":
//   • list_dbnomics_providers
//   • list_dbnomics_datasets
//   • list_dbnomics_series
//   • get_dbnomics_observations  (the hot path: actual time-series data)
//   • search_dbnomics            (global search across providers/datasets)
//
// All async, bridged from DBnomicsService signals via AsyncDispatch.

#include "mcp/tools/DBnomicsTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "services/dbnomics/DBnomicsService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace fincept::mcp::tools {

namespace {

static constexpr const char* TAG = "DBnomicsTools";

// DBnomics rate-limits at 3 req/s. Allow generous timeout for paginated fetches.
static constexpr int kDefaultTimeoutMs = 60000;

QJsonObject pagination_to_json(const services::DbnPagination& p) {
    return QJsonObject{
        {"offset", p.offset},
        {"limit", p.limit},
        {"total", p.total},
        {"has_more", p.has_more()},
    };
}

QJsonArray observations_to_json(const QVector<services::DbnObservation>& obs) {
    QJsonArray arr;
    for (const auto& o : obs) {
        arr.append(QJsonObject{
            {"period", o.period},
            {"value", o.value},
            {"valid", o.valid},
        });
    }
    return arr;
}

} // namespace

std::vector<ToolDef> get_dbnomics_tools() {
    std::vector<ToolDef> tools;

    // ── list_dbnomics_providers ─────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_dbnomics_providers";
        t.description = "List all DBnomics data providers (code + name). e.g. BLS, IMF, ECB, OECD.";
        t.category = "dbnomics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::DBnomicsService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::DBnomicsService::providers_loaded, holder,
                                      [resolve, holder](QVector<services::DbnProvider> ps) {
                                          QJsonArray arr;
                                          for (const auto& p : ps)
                                              arr.append(QJsonObject{{"code", p.code}, {"name", p.name}});
                                          resolve(ToolResult::ok_data(arr));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::DBnomicsService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_providers();
                });
        };
        tools.push_back(std::move(t));
    }

    // ── list_dbnomics_datasets ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_dbnomics_datasets";
        t.description = "List datasets within a DBnomics provider (paginated). Returns code + name + pagination info.";
        t.category = "dbnomics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("provider_code", "Provider code (e.g. BLS, IMF, ECB)").required().length(1, 32)
            .integer("offset", "Pagination offset").default_int(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString prov = args["provider_code"].toString();
            const int offset = args["offset"].toInt(0);
            auto* svc = &services::DBnomicsService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, prov, offset](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::DBnomicsService::datasets_loaded, holder,
                                      [resolve, holder](QVector<services::DbnDataset> ds,
                                                        services::DbnPagination page, bool /*append*/) {
                                          QJsonArray arr;
                                          for (const auto& d : ds)
                                              arr.append(QJsonObject{{"code", d.code}, {"name", d.name}});
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"datasets", arr},
                                              {"pagination", pagination_to_json(page)},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::DBnomicsService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_datasets(prov, offset);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── list_dbnomics_series ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_dbnomics_series";
        t.description = "List series within a dataset; optional in-dataset query filter (paginated).";
        t.category = "dbnomics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("provider_code", "Provider code").required().length(1, 32)
            .string("dataset_code", "Dataset code").required().length(1, 64)
            .string("query", "Optional filter (text search within dataset)").default_str("").length(0, 256)
            .integer("offset", "Pagination offset").default_int(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString prov = args["provider_code"].toString();
            const QString ds = args["dataset_code"].toString();
            const QString q = args["query"].toString();
            const int offset = args["offset"].toInt(0);
            auto* svc = &services::DBnomicsService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, prov, ds, q, offset](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::DBnomicsService::series_loaded, holder,
                                      [resolve, holder](QVector<services::DbnSeriesInfo> ss,
                                                        services::DbnPagination page, bool /*append*/) {
                                          QJsonArray arr;
                                          for (const auto& s : ss)
                                              arr.append(QJsonObject{{"code", s.code}, {"name", s.name}});
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"series", arr},
                                              {"pagination", pagination_to_json(page)},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::DBnomicsService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_series(prov, ds, q, offset);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── get_dbnomics_observations ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_dbnomics_observations";
        t.description = "Fetch full time-series observations (period + value) for a specific DBnomics series.";
        t.category = "dbnomics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("provider_code", "Provider code (e.g. BLS)").required().length(1, 32)
            .string("dataset_code", "Dataset code (e.g. ln)").required().length(1, 64)
            .string("series_code", "Series code (e.g. LNS14000000)").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString prov = args["provider_code"].toString();
            const QString ds = args["dataset_code"].toString();
            const QString sc = args["series_code"].toString();
            const QString series_id = prov + "/" + ds + "/" + sc;
            auto* svc = &services::DBnomicsService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, prov, ds, sc, series_id](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::DBnomicsService::observations_loaded, holder,
                                      [series_id, resolve, holder](services::DbnDataPoint dp) {
                                          // Filter by the series we requested — observations_loaded
                                          // is broadcast.
                                          if (dp.series_id != series_id)
                                              return;
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"series_id", dp.series_id},
                                              {"series_name", dp.series_name},
                                              {"observations", observations_to_json(dp.observations)},
                                              {"count", static_cast<int>(dp.observations.size())},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::DBnomicsService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_observations(prov, ds, sc);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── search_dbnomics ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_dbnomics";
        t.description = "Global search across all DBnomics providers and datasets (paginated).";
        t.category = "dbnomics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query (e.g. 'unemployment')").required().length(1, 256)
            .integer("offset", "Pagination offset").default_int(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString q = args["query"].toString();
            const int offset = args["offset"].toInt(0);
            auto* svc = &services::DBnomicsService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, q, offset](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::DBnomicsService::search_results_loaded, holder,
                                      [resolve, holder](QVector<services::DbnSearchResult> rs,
                                                        services::DbnPagination page, bool /*append*/) {
                                          QJsonArray arr;
                                          for (const auto& r : rs) {
                                              arr.append(QJsonObject{
                                                  {"provider_code", r.provider_code},
                                                  {"provider_name", r.provider_name},
                                                  {"dataset_code", r.dataset_code},
                                                  {"dataset_name", r.dataset_name},
                                                  {"nb_series", r.nb_series},
                                              });
                                          }
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"results", arr},
                                              {"pagination", pagination_to_json(page)},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::DBnomicsService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->global_search(q, offset);
                });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 dbnomics tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
