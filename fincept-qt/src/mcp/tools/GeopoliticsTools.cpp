// GeopoliticsTools.cpp — Tools for the Geopolitics screen.
//
// 13 tools in category "geopolitics":
//   • Events / reference data (5)
//   • HDX humanitarian search (5)
//   • Trade analysis (2)
//   • Geolocations + critical regions (1 each, 2 total)
// All async, bridged from GeopoliticsService signals.

#include "mcp/tools/GeopoliticsTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "services/geopolitics/GeopoliticsService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <atomic>
#include <memory>

namespace fincept::mcp::tools {

namespace {
static constexpr const char* TAG = "GeopoliticsTools";
static constexpr int kDefaultTimeoutMs = 120000;

// HDX, trade-analysis and geolocation results come back keyed by CONTEXT only ("country",
// "trade_benefits", ...), never by a request id, so two in-flight calls on the same context
// cannot be told apart: whichever result arrives first would resolve both, silently attaching
// Ukraine's datasets to the Sudan query. The tool loop makes that a realistic call shape --
// consecutive read-only calls in one round fan out concurrently. Until the service returns an
// id, admit one call per context at a time and tell the caller to retry (honest, where the
// alternative is a confidently wrong answer). The claim is a deadline in ms since epoch (0 =
// free): a call whose result never arrives frees the slot when its own budget runs out.
using GeoClaimSlot = std::atomic<qint64>;

qint64 geo_claim_acquire(GeoClaimSlot& slot, int budget_ms) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 cur = slot.load();
    while (cur <= now) {
        const qint64 token = now + budget_ms;
        if (slot.compare_exchange_weak(cur, token))
            return token;
    }
    return 0;
}

void geo_claim_release(GeoClaimSlot& slot, qint64 token) {
    qint64 expected = token; // only the claim's owner releases; a later claimant's deadline differs
    slot.compare_exchange_strong(expected, 0);
}

// callback_to_promise with an optional exclusivity claim (null slot = plain pass-through).
// `body` receives a `resolve` that also releases the claim.
template <typename Body>
void geo_run_exclusive(const std::shared_ptr<GeoClaimSlot>& slot, QObject* svc, const QString& what, ToolContext ctx,
                       std::shared_ptr<QPromise<ToolResult>> promise, Body body) {
    qint64 token = 0;
    if (slot) {
        token = geo_claim_acquire(*slot, kDefaultTimeoutMs);
        if (token == 0) {
            AsyncDispatch::callback_to_promise(nullptr, std::move(ctx), promise, [what](auto resolve) {
                resolve(ToolResult::fail(
                    what + " is already in flight. This service reports results by context only, so two concurrent "
                           "calls could swap results - retry once the first call has returned."));
            });
            return;
        }
    }
    AsyncDispatch::callback_to_promise(
        svc, std::move(ctx), promise, [slot, token, body = std::move(body)](auto resolve) mutable {
            auto done = [resolve, slot, token](ToolResult r) {
                if (slot)
                    geo_claim_release(*slot, token);
                resolve(std::move(r));
            };
            body(done);
        });
}

QJsonObject event_to_json(const services::geo::NewsEvent& e) {
    return QJsonObject{
        {"url", e.url},
        {"source", e.source},
        {"event_category", e.event_category},
        {"title", e.title},
        {"city", e.city},
        {"country", e.country},
        {"latitude", e.latitude},
        {"longitude", e.longitude},
        {"has_coords", e.has_coords},
        {"extracted_date", e.extracted_date},
        {"created_at", e.created_at},
    };
}

QJsonObject events_page_to_json(const services::geo::EventsPage& p) {
    QJsonArray evs;
    for (const auto& e : p.events)
        evs.append(event_to_json(e));
    return QJsonObject{
        {"events", evs},
        {"total_events", p.total_events},
        {"current_page", p.current_page},
        {"total_pages", p.total_pages},
        {"events_per_page", p.events_per_page},
        {"has_next", p.has_next},
        {"has_prev", p.has_prev},
        {"credits_used", p.credits_used},
        {"remaining_credits", p.remaining_credits},
    };
}

QJsonArray hdx_to_json(const QVector<services::geo::HDXDataset>& xs) {
    QJsonArray arr;
    for (const auto& d : xs) {
        QJsonArray tags;
        for (const auto& t : d.tags)
            tags.append(t);
        arr.append(QJsonObject{
            {"id", d.id},
            {"title", d.title},
            {"organization", d.organization},
            {"notes", d.notes},
            {"date", d.date},
            {"num_resources", d.num_resources},
            {"tags", tags},
        });
    }
    return arr;
}
} // namespace

std::vector<ToolDef> get_geopolitics_tools() {
    std::vector<ToolDef> tools;

    // 1. list_geopolitics_critical_regions
    {
        ToolDef t;
        t.name = "list_geopolitics_critical_regions";
        t.description = "List the built-in critical-region watchlist (Ukraine, Gaza, Sudan, ...).";
        t.category = "geopolitics";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& r : services::geo::critical_regions())
                arr.append(r);
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // 2. fetch_geopolitics_events
    {
        ToolDef t;
        t.name = "fetch_geopolitics_events";
        t.description = "Fetch geopolitical news events from the conflict monitor with optional filters (country, "
                        "city, category, source, date range) and pagination.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
                             .string("country", "Country filter")
                             .default_str("")
                             .length(0, 64)
                             .string("city", "City filter")
                             .default_str("")
                             .length(0, 64)
                             .string("category", "Event category filter")
                             .default_str("")
                             .length(0, 64)
                             .integer("limit", "Events per page")
                             .default_int(100)
                             .between(1, 500)
                             .integer("page", "Page (1-indexed)")
                             .default_int(1)
                             .min(1)
                             .string("source", "Source filter")
                             .default_str("")
                             .length(0, 128)
                             .string("date_from", "ISO date from")
                             .default_str("")
                             .length(0, 32)
                             .string("date_to", "ISO date to")
                             .default_str("")
                             .length(0, 32)
                             .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, args](auto resolve) {
                const QString country = args["country"].toString();
                const QString city = args["city"].toString();
                const QString category = args["category"].toString();
                const int limit = args["limit"].toInt(100);
                const int page = args["page"].toInt(1);
                const QString source = args["source"].toString();
                const QString date_from = args["date_from"].toString();
                const QString date_to = args["date_to"].toString();
                // events_loaded is shared by the hub refresh, the Geopolitics screen and every
                // MCP caller, so "the next events_loaded" is not necessarily OUR page. Match on the
                // key the service stamps on each page; an identical in-flight request from another
                // consumer carries the same key and the same data, so adopting it is correct.
                const QString want_key =
                    services::geo::events_request_key(country, city, category, limit, page, source, date_from, date_to);

                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::events_loaded, h,
                                 [resolve, h, want_key](services::geo::EventsPage p) {
                                     if (p.request_key != want_key)
                                         return;
                                     resolve(ToolResult::ok_data(events_page_to_json(p)));
                                     h->deleteLater();
                                 });
                // error_occurred carries a context name ("events", "countries", ...) but no request
                // key, so the filter can only narrow to the events fetch family. Another events
                // request failing while ours is in flight can still fail this call; the service would
                // have to put the key on the error to close that (see report).
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("events"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->fetch_events(country, city, category, limit, page, source, date_from, date_to);
            });
        };
        tools.push_back(std::move(t));
    }

    // 3. list_geopolitics_countries
    {
        ToolDef t;
        t.name = "list_geopolitics_countries";
        t.description = "List all countries with event counts known to the conflict monitor.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::countries_loaded, h,
                                 [resolve, h](QVector<services::geo::UniqueCountry> xs) {
                                     QJsonArray arr;
                                     for (const auto& c : xs)
                                         arr.append(
                                             QJsonObject{{"country", c.country}, {"event_count", c.event_count}});
                                     resolve(ToolResult::ok_data(arr));
                                     h->deleteLater();
                                 });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("countries"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->fetch_unique_countries();
            });
        };
        tools.push_back(std::move(t));
    }

    // 4. list_geopolitics_categories
    {
        ToolDef t;
        t.name = "list_geopolitics_categories";
        t.description = "List all event categories with counts.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::categories_loaded, h,
                                 [resolve, h](QVector<services::geo::UniqueCategory> xs) {
                                     QJsonArray arr;
                                     for (const auto& c : xs)
                                         arr.append(
                                             QJsonObject{{"category", c.category}, {"event_count", c.event_count}});
                                     resolve(ToolResult::ok_data(arr));
                                     h->deleteLater();
                                 });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("categories"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->fetch_unique_categories();
            });
        };
        tools.push_back(std::move(t));
    }

    // 5. list_geopolitics_cities
    {
        ToolDef t;
        t.name = "list_geopolitics_cities";
        t.description = "List all cities known to the conflict monitor.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::cities_loaded, h,
                                 [resolve, h](QStringList xs) {
                                     QJsonArray arr;
                                     for (const auto& s : xs)
                                         arr.append(s);
                                     resolve(ToolResult::ok_data(arr));
                                     h->deleteLater();
                                 });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("cities"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->fetch_unique_cities();
            });
        };
        tools.push_back(std::move(t));
    }

    // 6-10. HDX search variants — emit hdx_results_loaded(context, datasets).
    auto make_hdx_tool = [](const QString& name, const QString& desc, const QString& ctx_filter,
                            std::function<void(services::geo::GeopoliticsService*, const QJsonObject&)> kick,
                            bool need_arg, const QString& arg_name, const QString& arg_desc) {
        ToolDef t;
        t.name = name;
        t.description = desc;
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        if (need_arg) {
            t.input_schema = ToolSchemaBuilder().string(arg_name, arg_desc).required().length(1, 256).build();
        }
        // Argument-less searches return the same data for every caller, so only the ones that
        // take an argument need the one-at-a-time claim (see geo_run_exclusive).
        const auto slot = need_arg ? std::make_shared<GeoClaimSlot>(0) : std::shared_ptr<GeoClaimSlot>();
        const QString what = name;
        t.async_handler = [ctx_filter, kick, slot, what](const QJsonObject& args, ToolContext ctx,
                                                         std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            geo_run_exclusive(
                slot, svc, what, std::move(ctx), promise, [svc, ctx_filter, kick, args](auto resolve) {
                    auto* h = new QObject(svc);
                    QObject::connect(svc, &services::geo::GeopoliticsService::hdx_results_loaded, h,
                                     [ctx_filter, resolve, h](QString context, QVector<services::geo::HDXDataset> xs) {
                                         if (context != ctx_filter)
                                             return;
                                         resolve(ToolResult::ok_data(hdx_to_json(xs)));
                                         h->deleteLater();
                                     });
                    // The service reports HDX failures as "hdx_<flavour>" — match our own.
                    QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                     [ctx_filter, resolve, h](QString err_ctx, QString m) {
                                         if (err_ctx != QStringLiteral("hdx_") + ctx_filter)
                                             return;
                                         resolve(ToolResult::fail(m));
                                         h->deleteLater();
                                     });
                    kick(svc, args);
                });
        };
        return t;
    };

    tools.push_back(make_hdx_tool("search_hdx_conflicts", "Search HDX humanitarian datasets tagged as conflicts.",
                                  "conflicts", [](auto svc, const QJsonObject&) { svc->search_hdx_conflicts(); }, false,
                                  {}, {}));
    tools.push_back(make_hdx_tool("search_hdx_humanitarian", "Search HDX humanitarian datasets (broad).",
                                  "humanitarian", [](auto svc, const QJsonObject&) { svc->search_hdx_humanitarian(); },
                                  false, {}, {}));
    tools.push_back(make_hdx_tool(
        "search_hdx_by_country", "Search HDX datasets for a specific country.", "country",
        [](auto svc, const QJsonObject& a) { svc->search_hdx_by_country(a["country"].toString()); }, true, "country",
        "Country name"));
    tools.push_back(make_hdx_tool(
        "search_hdx_by_topic", "Search HDX datasets for a topic / theme.", "topic",
        [](auto svc, const QJsonObject& a) { svc->search_hdx_by_topic(a["topic"].toString()); }, true, "topic",
        "Topic / theme keyword"));
    tools.push_back(make_hdx_tool(
        "search_hdx_advanced", "Run an advanced HDX dataset query (free-form).", "search",
        [](auto svc, const QJsonObject& a) { svc->search_hdx_advanced(a["query"].toString()); }, true, "query",
        "Search query"));

    // 11. analyze_trade_benefits
    {
        ToolDef t;
        t.name = "analyze_trade_benefits";
        t.description = "Run trade-benefits analysis (welfare gains, consumer surplus, integration impact).";
        t.category = "geopolitics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder()
                .object("params",
                        "Analysis params (trade_volume_gdp, price_reduction_percent, traded_goods_consumption, "
                        "integration_type, trade_creation, trade_diversion)")
                .required()
                .build();
        const auto slot = std::make_shared<GeoClaimSlot>(0);
        t.async_handler = [slot](const QJsonObject& args, ToolContext ctx,
                                 std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            geo_run_exclusive(slot, svc, QStringLiteral("analyze_trade_benefits"), std::move(ctx), promise,
                              [svc, args](auto resolve) {
                auto* h = new QObject(svc);
                // Both trade analyses share trade_result_ready/error_occurred and tag them
                // "trade_benefits" / "trade_restrictions"; take only our own.
                QObject::connect(svc, &services::geo::GeopoliticsService::trade_result_ready, h,
                                 [resolve, h](QString res_ctx, QJsonObject data) {
                                     if (res_ctx != QLatin1String("trade_benefits"))
                                         return;
                                     resolve(ToolResult::ok_data(data));
                                     h->deleteLater();
                                 });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("trade_benefits"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->analyze_trade_benefits(args["params"].toObject());
            });
        };
        tools.push_back(std::move(t));
    }

    // 12. analyze_trade_restrictions
    {
        ToolDef t;
        t.name = "analyze_trade_restrictions";
        t.description = "Run trade-restrictions analysis (tariffs, quotas, subsidies, liberalization impact).";
        t.category = "geopolitics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder()
                .object("params", "Analysis params (tariff_rate, quota_volume, subsidy_rate, development_level, "
                                  "industry_maturity, liberalization_type, tariff_reduction, gdp_size)")
                .required()
                .build();
        const auto slot = std::make_shared<GeoClaimSlot>(0);
        t.async_handler = [slot](const QJsonObject& args, ToolContext ctx,
                                 std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            geo_run_exclusive(slot, svc, QStringLiteral("analyze_trade_restrictions"), std::move(ctx), promise,
                              [svc, args](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::trade_result_ready, h,
                                 [resolve, h](QString res_ctx, QJsonObject data) {
                                     if (res_ctx != QLatin1String("trade_restrictions"))
                                         return;
                                     resolve(ToolResult::ok_data(data));
                                     h->deleteLater();
                                 });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("trade_restrictions"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->analyze_trade_restrictions(args["params"].toObject());
            });
        };
        tools.push_back(std::move(t));
    }

    // 13. extract_geolocations_from_headlines
    {
        ToolDef t;
        t.name = "extract_geolocations_from_headlines";
        t.description =
            "Extract geolocation hints (country/city/coords) from a batch of news headlines via Python NLP.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema =
            ToolSchemaBuilder().array("headlines", "Headlines to analyse", QJsonObject{{"type", "string"}}).required().build();
        const auto slot = std::make_shared<GeoClaimSlot>(0);
        t.async_handler = [slot](const QJsonObject& args, ToolContext ctx,
                                 std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList hs;
            for (const auto& v : args["headlines"].toArray()) {
                const QString h = v.toString().trimmed();
                if (!h.isEmpty())
                    hs.append(h);
            }
            if (hs.isEmpty()) {
                AsyncDispatch::callback_to_promise(nullptr, ctx, promise, [](auto resolve) {
                    resolve(ToolResult::fail("'headlines' must be a non-empty array of headline strings"));
                });
                return;
            }
            auto* svc = &services::geo::GeopoliticsService::instance();
            geo_run_exclusive(slot, svc, QStringLiteral("extract_geolocations_from_headlines"), std::move(ctx), promise,
                              [svc, hs](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::geolocation_ready, h,
                                 [resolve, h](QJsonObject data) {
                                     resolve(ToolResult::ok_data(data));
                                     h->deleteLater();
                                 });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                 [resolve, h](QString err_ctx, QString m) {
                                     if (err_ctx != QLatin1String("geolocation"))
                                         return;
                                     resolve(ToolResult::fail(m));
                                     h->deleteLater();
                                 });
                svc->extract_geolocations(hs);
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 geopolitics tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
