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

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace fincept::mcp::tools {

namespace {
static constexpr const char* TAG = "GeopoliticsTools";
static constexpr int kDefaultTimeoutMs = 120000;

QJsonObject event_to_json(const services::geo::NewsEvent& e) {
    return QJsonObject{
        {"url", e.url}, {"source", e.source}, {"event_category", e.event_category},
        {"title", e.title}, {"city", e.city}, {"country", e.country},
        {"latitude", e.latitude}, {"longitude", e.longitude}, {"has_coords", e.has_coords},
        {"extracted_date", e.extracted_date}, {"created_at", e.created_at},
    };
}

QJsonObject events_page_to_json(const services::geo::EventsPage& p) {
    QJsonArray evs;
    for (const auto& e : p.events) evs.append(event_to_json(e));
    return QJsonObject{
        {"events", evs}, {"total_events", p.total_events},
        {"current_page", p.current_page}, {"total_pages", p.total_pages},
        {"events_per_page", p.events_per_page},
        {"has_next", p.has_next}, {"has_prev", p.has_prev},
        {"credits_used", p.credits_used}, {"remaining_credits", p.remaining_credits},
    };
}

QJsonArray hdx_to_json(const QVector<services::geo::HDXDataset>& xs) {
    QJsonArray arr;
    for (const auto& d : xs) {
        QJsonArray tags;
        for (const auto& t : d.tags) tags.append(t);
        arr.append(QJsonObject{
            {"id", d.id}, {"title", d.title}, {"organization", d.organization},
            {"notes", d.notes}, {"date", d.date}, {"num_resources", d.num_resources},
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
            for (const auto& r : services::geo::critical_regions()) arr.append(r);
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // 2. fetch_geopolitics_events
    {
        ToolDef t;
        t.name = "fetch_geopolitics_events";
        t.description = "Fetch geopolitical news events from the conflict monitor with optional filters (country, city, category, source, date range) and pagination.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("country", "Country filter").default_str("").length(0, 64)
            .string("city", "City filter").default_str("").length(0, 64)
            .string("category", "Event category filter").default_str("").length(0, 64)
            .integer("limit", "Events per page").default_int(100).between(1, 500)
            .integer("page", "Page (1-indexed)").default_int(1).min(1)
            .string("source", "Source filter").default_str("").length(0, 128)
            .string("date_from", "ISO date from").default_str("").length(0, 32)
            .string("date_to", "ISO date to").default_str("").length(0, 32)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, args](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::events_loaded, h,
                                  [resolve, h](services::geo::EventsPage p) {
                                      resolve(ToolResult::ok_data(events_page_to_json(p)));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
                svc->fetch_events(args["country"].toString(), args["city"].toString(),
                                   args["category"].toString(), args["limit"].toInt(100),
                                   args["page"].toInt(1), args["source"].toString(),
                                   args["date_from"].toString(), args["date_to"].toString());
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
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::countries_loaded, h,
                                  [resolve, h](QVector<services::geo::UniqueCountry> xs) {
                                      QJsonArray arr;
                                      for (const auto& c : xs)
                                          arr.append(QJsonObject{{"country", c.country}, {"event_count", c.event_count}});
                                      resolve(ToolResult::ok_data(arr));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
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
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::categories_loaded, h,
                                  [resolve, h](QVector<services::geo::UniqueCategory> xs) {
                                      QJsonArray arr;
                                      for (const auto& c : xs)
                                          arr.append(QJsonObject{{"category", c.category},
                                                                  {"event_count", c.event_count}});
                                      resolve(ToolResult::ok_data(arr));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
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
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::cities_loaded, h,
                                  [resolve, h](QStringList xs) {
                                      QJsonArray arr;
                                      for (const auto& s : xs) arr.append(s);
                                      resolve(ToolResult::ok_data(arr));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
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
            t.input_schema = ToolSchemaBuilder()
                .string(arg_name, arg_desc).required().length(1, 256)
                .build();
        }
        t.async_handler = [ctx_filter, kick](const QJsonObject& args, ToolContext ctx,
                                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, ctx_filter, kick, args](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::hdx_results_loaded, h,
                                  [ctx_filter, resolve, h](QString context, QVector<services::geo::HDXDataset> xs) {
                                      if (!ctx_filter.isEmpty() && context != ctx_filter) return;
                                      resolve(ToolResult::ok_data(hdx_to_json(xs)));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
                kick(svc, args);
            });
        };
        return t;
    };

    tools.push_back(make_hdx_tool("search_hdx_conflicts",
        "Search HDX humanitarian datasets tagged as conflicts.", "conflicts",
        [](auto svc, const QJsonObject&) { svc->search_hdx_conflicts(); }, false, {}, {}));
    tools.push_back(make_hdx_tool("search_hdx_humanitarian",
        "Search HDX humanitarian datasets (broad).", "humanitarian",
        [](auto svc, const QJsonObject&) { svc->search_hdx_humanitarian(); }, false, {}, {}));
    tools.push_back(make_hdx_tool("search_hdx_by_country",
        "Search HDX datasets for a specific country.", "",
        [](auto svc, const QJsonObject& a) { svc->search_hdx_by_country(a["country"].toString()); },
        true, "country", "Country name"));
    tools.push_back(make_hdx_tool("search_hdx_by_topic",
        "Search HDX datasets for a topic / theme.", "",
        [](auto svc, const QJsonObject& a) { svc->search_hdx_by_topic(a["topic"].toString()); },
        true, "topic", "Topic / theme keyword"));
    tools.push_back(make_hdx_tool("search_hdx_advanced",
        "Run an advanced HDX dataset query (free-form).", "",
        [](auto svc, const QJsonObject& a) { svc->search_hdx_advanced(a["query"].toString()); },
        true, "query", "Search query"));

    // 11. analyze_trade_benefits
    {
        ToolDef t;
        t.name = "analyze_trade_benefits";
        t.description = "Run trade-benefits analysis (welfare gains, consumer surplus, integration impact).";
        t.category = "geopolitics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .object("params", "Analysis params (trade_volume_gdp, price_reduction_percent, traded_goods_consumption, "
                              "integration_type, trade_creation, trade_diversion)").required()
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, args](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::trade_result_ready, h,
                                  [resolve, h](QString, QJsonObject data) {
                                      resolve(ToolResult::ok_data(data));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
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
        t.input_schema = ToolSchemaBuilder()
            .object("params", "Analysis params (tariff_rate, quota_volume, subsidy_rate, development_level, "
                              "industry_maturity, liberalization_type, tariff_reduction, gdp_size)").required()
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, args](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::trade_result_ready, h,
                                  [resolve, h](QString, QJsonObject data) {
                                      resolve(ToolResult::ok_data(data));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
                svc->analyze_trade_restrictions(args["params"].toObject());
            });
        };
        tools.push_back(std::move(t));
    }

    // 13. extract_geolocations_from_headlines
    {
        ToolDef t;
        t.name = "extract_geolocations_from_headlines";
        t.description = "Extract geolocation hints (country/city/coords) from a batch of news headlines via Python NLP.";
        t.category = "geopolitics";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .array("headlines", "Headlines to analyse", QJsonObject{{"type", "string"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList hs;
            for (const auto& v : args["headlines"].toArray()) hs.append(v.toString());
            auto* svc = &services::geo::GeopoliticsService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, hs](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &services::geo::GeopoliticsService::geolocation_ready, h,
                                  [resolve, h](QJsonObject data) {
                                      resolve(ToolResult::ok_data(data));
                                      h->deleteLater();
                                  });
                QObject::connect(svc, &services::geo::GeopoliticsService::error_occurred, h,
                                  [resolve, h](QString, QString m) { resolve(ToolResult::fail(m)); h->deleteLater(); });
                svc->extract_geolocations(hs);
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 geopolitics tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
