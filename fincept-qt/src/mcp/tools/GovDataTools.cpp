// GovDataTools.cpp — Tools that drive the Government Data screen.
//
// GovDataService is a single generic dispatcher:
//   execute(script, command, args, request_id) → emits result_ready(request_id, GovDataResult)
//
// So all 10 providers + their per-panel sub-commands collapse into one
// pattern. We expose:
//
//   2 generic tools
//     • list_gov_data_providers     — static provider catalog (10 entries)
//     • run_gov_data_command        — generic dispatcher
//
//   23 provider-specific shortcut tools, derived from actual UI dispatch
//   sites (see grep of GovDataService::instance().execute in panels):
//     • US Treasury (3): prices / auctions / summary
//     • US Congress (3): summary / bills / bill_info
//     • France     (4): publishers / search_datasets / dataset_resources / municipalities
//     • Hong Kong  (4): publishers / datasets_list / datasets_by_category / dataset_resources
//     • UK         (5): publishers / popular_publishers / datasets_by_publisher / dataset_resources / search
//     • Australia  (4): organizations / org_datasets / search / recent
//
// Providers using a generic CKAN pattern (Canada, Swiss, openAFRICA, Spain,
// universal CKAN) stay accessible through `run_gov_data_command` — adding
// 4×5 = 20 thin tool wrappers would just bloat the catalogue.

#include "mcp/tools/GovDataTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "services/gov_data/GovDataService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QUuid>

namespace fincept::mcp::tools {

namespace {

static constexpr const char* TAG = "GovDataTools";

// Government APIs vary wildly; some auctions / large dataset queries take
// a long time. 90s default.
static constexpr int kDefaultTimeoutMs = 90000;

// Script filenames — must match GovDataService::kProviders.
constexpr const char* kTreasury  = "government_us_data.py";
constexpr const char* kCongress  = "congress_gov_data.py";
constexpr const char* kFrance    = "french_gov_api.py";
constexpr const char* kHK        = "data_gov_hk_api.py";
constexpr const char* kUK        = "datagovuk_api.py";
constexpr const char* kAustralia = "datagov_au_api.py";

// Bridge: run a single GovDataService::execute call and resolve when
// result_ready fires with the matching request_id (race-safe).
void dispatch_gov_async(const QString& script, const QString& command,
                        const QStringList& args, ToolContext ctx,
                        std::shared_ptr<QPromise<ToolResult>> promise) {
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* svc = &services::GovDataService::instance();
    AsyncDispatch::callback_to_promise(
        svc, std::move(ctx), promise,
        [svc, script, command, args, req_id](auto resolve) {
            auto* holder = new QObject(svc);
            QObject::connect(svc, &services::GovDataService::result_ready, holder,
                              [req_id, resolve, holder](QString id, services::GovDataResult r) {
                                  if (id != req_id)
                                      return;
                                  if (r.success)
                                      resolve(ToolResult::ok_data(r.data));
                                  else
                                      resolve(ToolResult::fail(
                                          r.error.isEmpty() ? "Government data request failed" : r.error));
                                  holder->deleteLater();
                              });
            svc->execute(script, command, args, req_id);
        });
}

// Macro for the common pattern: one tool per (script, command) with
// declared string params.
#define GOV_TOOL_BEGIN(NAME, DESC)                                                                                     \
    do {                                                                                                               \
        ToolDef t;                                                                                                     \
        t.name = (NAME);                                                                                               \
        t.description = (DESC);                                                                                        \
        t.category = "gov-data";                                                                                       \
        t.default_timeout_ms = kDefaultTimeoutMs;

#define GOV_TOOL_END                                                                                                   \
        tools.push_back(std::move(t));                                                                                 \
    } while (0)

} // namespace

std::vector<ToolDef> get_gov_data_tools() {
    std::vector<ToolDef> tools;

    // ── Generic 1: list_gov_data_providers ──────────────────────────────
    {
        ToolDef t;
        t.name = "list_gov_data_providers";
        t.description = "List all supported government data providers (id, name, country, script).";
        t.category = "gov-data";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& p : services::GovDataService::providers()) {
                arr.append(QJsonObject{
                    {"id", p.id},
                    {"name", p.name},
                    {"full_name", p.full_name},
                    {"description", p.description},
                    {"country", p.country},
                    {"flag", p.flag},
                    {"script", p.script},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── Generic 2: run_gov_data_command ─────────────────────────────────
    {
        ToolDef t;
        t.name = "run_gov_data_command";
        t.description = "Generic dispatcher: run any (script, command, args) on the gov-data Python backend.";
        t.category = "gov-data";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("script", "Python script filename (use list_gov_data_providers)").required().length(1, 128)
            .string("command", "Sub-command for the script").required().length(1, 64)
            .array("args", "Positional string arguments", QJsonObject{{"type", "string"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString script = args["script"].toString();
            const QString command = args["command"].toString();
            QStringList sargs;
            for (const auto& v : args["args"].toArray())
                sargs.append(v.toString());
            dispatch_gov_async(script, command, sargs, std::move(ctx), promise);
        };
        tools.push_back(std::move(t));
    }

    // ── US Treasury (3) ─────────────────────────────────────────────────
    GOV_TOOL_BEGIN("gov_us_treasury_prices",
                   "US Treasury — fetch security prices for a given date and optional security type.")
        t.input_schema = ToolSchemaBuilder()
            .string("date", "Date in YYYY-MM-DD").required().length(10, 10)
            .string("security_type", "Optional security type (e.g. 'Bill', 'Note', 'Bond'). Empty = all.")
                .default_str("").length(0, 32)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["date"].toString();
            const QString sec = args["security_type"].toString();
            if (!sec.isEmpty()) { a << ""; a << sec; }
            dispatch_gov_async(kTreasury, "treasury_prices", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_us_treasury_auctions",
                   "US Treasury — list auction results between two dates with optional security filter.")
        t.input_schema = ToolSchemaBuilder()
            .string("start_date", "Start date YYYY-MM-DD").required().length(10, 10)
            .string("end_date", "End date YYYY-MM-DD").required().length(10, 10)
            .string("security_type", "Optional security type filter").default_str("").length(0, 32)
            .integer("limit", "Max rows").default_int(50).between(1, 500)
            .integer("page", "Page index").default_int(1).min(1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["start_date"].toString()
              << args["end_date"].toString()
              << args["security_type"].toString()
              << QString::number(args["limit"].toInt(50))
              << QString::number(args["page"].toInt(1));
            dispatch_gov_async(kTreasury, "treasury_auctions", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_us_treasury_summary",
                   "US Treasury — daily summary as of a given date.")
        t.input_schema = ToolSchemaBuilder()
            .string("date", "Date YYYY-MM-DD").required().length(10, 10)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kTreasury, "summary", {args["date"].toString()},
                                std::move(ctx), promise);
        };
    GOV_TOOL_END;

    // ── US Congress (3) ─────────────────────────────────────────────────
    GOV_TOOL_BEGIN("gov_us_congress_summary",
                   "US Congress — high-level summary for a Congress number (e.g. 118).")
        t.input_schema = ToolSchemaBuilder()
            .integer("congress", "Congress number (e.g. 118)").required().between(1, 200)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kCongress, "summary",
                                {QString::number(args["congress"].toInt())}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_us_congress_bills",
                   "US Congress — list bills for a Congress with optional bill_type filter and pagination.")
        t.input_schema = ToolSchemaBuilder()
            .integer("congress", "Congress number").required().between(1, 200)
            .string("bill_type", "Optional bill type (hr, s, hjres, sjres, hconres, sconres, hres, sres)")
                .default_str("").length(0, 16)
            .integer("limit", "Max rows").default_int(50).between(1, 250)
            .integer("offset", "Pagination offset").default_int(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << QString::number(args["congress"].toInt());
            a << args["bill_type"].toString();
            a << QString::number(args["limit"].toInt(50));
            a << QString::number(args["offset"].toInt(0));
            dispatch_gov_async(kCongress, "congress_bills", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_us_congress_bill_info",
                   "US Congress — full info for a specific bill (congress + type + number).")
        t.input_schema = ToolSchemaBuilder()
            .integer("congress", "Congress number").required().between(1, 200)
            .string("bill_type", "Bill type (hr, s, hjres, sjres, ...)").required().length(1, 16)
            .integer("bill_number", "Bill number").required().min(1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << QString::number(args["congress"].toInt())
              << args["bill_type"].toString()
              << QString::number(args["bill_number"].toInt());
            dispatch_gov_async(kCongress, "bill_info", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    // ── France (4) ──────────────────────────────────────────────────────
    GOV_TOOL_BEGIN("gov_fr_publishers", "France data.gouv.fr — list publishers (services).")
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kFrance, "publishers", {}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_fr_search_datasets", "France data.gouv.fr — search datasets matching a query.")
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 256)
            .integer("limit", "Max rows").default_int(50).between(1, 200)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["query"].toString() << QString::number(args["limit"].toInt(50));
            dispatch_gov_async(kFrance, "datasets", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_fr_dataset_resources", "France data.gouv.fr — list resources (files/APIs) for a dataset.")
        t.input_schema = ToolSchemaBuilder()
            .string("dataset_id", "Dataset id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kFrance, "resources", {args["dataset_id"].toString()},
                                std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_fr_municipalities", "France — geographic lookup of municipalities matching a query.")
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Municipality name or postal code").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kFrance, "municipalities", {args["query"].toString()},
                                std::move(ctx), promise);
        };
    GOV_TOOL_END;

    // ── Hong Kong (4) ───────────────────────────────────────────────────
    GOV_TOOL_BEGIN("gov_hk_publishers", "Hong Kong Data.gov.hk — list publishers / categories.")
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kHK, "publishers", {}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_hk_datasets_list", "Hong Kong Data.gov.hk — flat list of all datasets.")
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kHK, "datasets_list", {}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_hk_datasets_by_category", "Hong Kong Data.gov.hk — datasets within a category.")
        t.input_schema = ToolSchemaBuilder()
            .string("category_id", "Category id from gov_hk_publishers").required().length(1, 128)
            .integer("limit", "Max rows").default_int(100).between(1, 500)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["category_id"].toString() << QString::number(args["limit"].toInt(100));
            dispatch_gov_async(kHK, "datasets", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_hk_dataset_resources", "Hong Kong Data.gov.hk — resources for a dataset.")
        t.input_schema = ToolSchemaBuilder()
            .string("dataset_id", "Dataset id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kHK, "resources", {args["dataset_id"].toString()},
                                std::move(ctx), promise);
        };
    GOV_TOOL_END;

    // ── UK (5) ──────────────────────────────────────────────────────────
    GOV_TOOL_BEGIN("gov_uk_publishers", "UK data.gov.uk — list publishers.")
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kUK, "publishers", {}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_uk_popular_publishers", "UK data.gov.uk — popular publishers (top by dataset count).")
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kUK, "popular-publishers", {}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_uk_datasets_by_publisher", "UK data.gov.uk — datasets owned by a publisher.")
        t.input_schema = ToolSchemaBuilder()
            .string("publisher_id", "Publisher id").required().length(1, 128)
            .integer("limit", "Max rows").default_int(100).between(1, 500)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["publisher_id"].toString() << QString::number(args["limit"].toInt(100));
            dispatch_gov_async(kUK, "datasets", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_uk_dataset_resources", "UK data.gov.uk — resources for a dataset.")
        t.input_schema = ToolSchemaBuilder()
            .string("dataset_id", "Dataset id").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kUK, "resources", {args["dataset_id"].toString()},
                                std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_uk_search", "UK data.gov.uk — search datasets matching a query.")
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 256)
            .integer("limit", "Max rows").default_int(50).between(1, 200)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["query"].toString() << QString::number(args["limit"].toInt(50));
            dispatch_gov_async(kUK, "search", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    // ── Australia (4) ───────────────────────────────────────────────────
    GOV_TOOL_BEGIN("gov_au_organizations", "Australia data.gov.au — list organizations.")
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kAustralia, "organizations", {}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_au_org_datasets", "Australia data.gov.au — datasets owned by an organization.")
        t.input_schema = ToolSchemaBuilder()
            .string("organization_id", "Organization id").required().length(1, 128)
            .integer("limit", "Max rows").default_int(100).between(1, 500)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["organization_id"].toString() << QString::number(args["limit"].toInt(100));
            dispatch_gov_async(kAustralia, "org-datasets", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_au_search", "Australia data.gov.au — dataset search.")
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 256)
            .integer("limit", "Max rows").default_int(50).between(1, 200)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList a;
            a << args["query"].toString() << QString::number(args["limit"].toInt(50));
            dispatch_gov_async(kAustralia, "search", a, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    GOV_TOOL_BEGIN("gov_au_recent", "Australia data.gov.au — most recently published datasets.")
        t.input_schema = ToolSchemaBuilder()
            .integer("limit", "Max rows").default_int(20).between(1, 100)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            dispatch_gov_async(kAustralia, "recent",
                                {QString::number(args["limit"].toInt(20))}, std::move(ctx), promise);
        };
    GOV_TOOL_END;

    LOG_INFO(TAG, QString("Defined %1 gov-data tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
