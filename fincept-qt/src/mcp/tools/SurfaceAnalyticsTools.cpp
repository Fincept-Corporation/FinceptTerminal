// SurfaceAnalyticsTools.cpp — Tools for the Surface Analytics screen.
//
// 15 tools in category "surface-analytics":
//   • Discovery (3) — categories, types, capability
//   • Databento  (12) — connection, fetches (options/ohlcv/futures/local_vol/
//                       implied_dividend/liquidity/commodity_vol/crack_spread/
//                       stress_test/yield_curve), cache access
// All Databento fetches are signal-bridged via DatabentoService::*_ready.

#include "mcp/tools/SurfaceAnalyticsTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "screens/surface_analytics/SurfaceCapabilities.h"
#include "screens/surface_analytics/SurfaceTypes.h"
#include "services/databento/DatabentoService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace fincept::mcp::tools {

namespace {
static constexpr const char* TAG = "SurfaceAnalyticsTools";
static constexpr int kDefaultTimeoutMs = 120000;

// All 35 ChartType values in declaration order — maps int <-> name.
static const std::array<std::pair<surface::ChartType, const char*>, 35> kChartTypes = {{
    {surface::ChartType::Volatility, "Volatility"},
    {surface::ChartType::DeltaSurface, "DeltaSurface"},
    {surface::ChartType::GammaSurface, "GammaSurface"},
    {surface::ChartType::VegaSurface, "VegaSurface"},
    {surface::ChartType::ThetaSurface, "ThetaSurface"},
    {surface::ChartType::SkewSurface, "SkewSurface"},
    {surface::ChartType::LocalVolSurface, "LocalVolSurface"},
    {surface::ChartType::YieldCurve, "YieldCurve"},
    {surface::ChartType::SwaptionVol, "SwaptionVol"},
    {surface::ChartType::CapFloorVol, "CapFloorVol"},
    {surface::ChartType::BondSpread, "BondSpread"},
    {surface::ChartType::OISBasis, "OISBasis"},
    {surface::ChartType::RealYield, "RealYield"},
    {surface::ChartType::ForwardRate, "ForwardRate"},
    {surface::ChartType::FXVol, "FXVol"},
    {surface::ChartType::FXForwardPoints, "FXForwardPoints"},
    {surface::ChartType::CrossCurrencyBasis, "CrossCurrencyBasis"},
    {surface::ChartType::CDSSpread, "CDSSpread"},
    {surface::ChartType::CreditTransition, "CreditTransition"},
    {surface::ChartType::RecoveryRate, "RecoveryRate"},
    {surface::ChartType::CommodityForward, "CommodityForward"},
    {surface::ChartType::CommodityVol, "CommodityVol"},
    {surface::ChartType::CrackSpread, "CrackSpread"},
    {surface::ChartType::ContangoBackwardation, "ContangoBackwardation"},
    {surface::ChartType::Correlation, "Correlation"},
    {surface::ChartType::PCA, "PCA"},
    {surface::ChartType::VaR, "VaR"},
    {surface::ChartType::StressTestPnL, "StressTestPnL"},
    {surface::ChartType::FactorExposure, "FactorExposure"},
    {surface::ChartType::LiquidityHeatmap, "LiquidityHeatmap"},
    {surface::ChartType::Drawdown, "Drawdown"},
    {surface::ChartType::BetaSurface, "BetaSurface"},
    {surface::ChartType::ImpliedDividend, "ImpliedDividend"},
    {surface::ChartType::InflationExpectations, "InflationExpectations"},
    {surface::ChartType::MonetaryPolicyPath, "MonetaryPolicyPath"},
}};

surface::ChartType parse_chart_type(const QString& name) {
    for (const auto& p : kChartTypes)
        if (name.compare(p.second, Qt::CaseInsensitive) == 0) return p.first;
    return surface::ChartType::Volatility;
}

QJsonObject capability_to_json(const surface::SurfaceCapability& c) {
    return QJsonObject{
        {"chart_type", chart_type_name(c.type)},
        {"display_name", chart_type_name(c.type)},
        {"tier", surface::tier_name(c.tier)},
        {"dataset", QString::fromUtf8(c.dataset)},
        {"schema", QString::fromUtf8(c.schema)},
        {"symbology", QString::fromUtf8(c.symbology)},
        {"description", QString::fromUtf8(c.description)},
        {"needs_underlying", c.needs_underlying},
        {"needs_date_range", c.needs_date_range},
        {"needs_strike_window", c.needs_strike_window},
        {"needs_expiry_filter", c.needs_expiry_filter},
        {"needs_correlation_basket", c.needs_correlation_basket},
        {"supports_3d", c.supports_3d},
        {"supports_table", c.supports_table},
        {"supports_line", c.supports_line},
    };
}

QJsonArray z_to_json(const std::vector<std::vector<float>>& z) {
    QJsonArray rows;
    for (const auto& row : z) {
        QJsonArray r;
        for (float v : row) r.append(v);
        rows.append(r);
    }
    return rows;
}

QJsonObject vol_surface_to_json(const surface::VolatilitySurfaceData& v) {
    QJsonArray strikes, exps;
    for (float s : v.strikes) strikes.append(s);
    for (int e : v.expirations) exps.append(e);
    return QJsonObject{
        {"underlying", QString::fromStdString(v.underlying)},
        {"spot_price", v.spot_price},
        {"strikes", strikes},
        {"expirations", exps},
        {"z", z_to_json(v.z)},
    };
}

QJsonObject greeks_to_json(const surface::GreeksSurfaceData& g) {
    QJsonArray strikes, exps;
    for (float s : g.strikes) strikes.append(s);
    for (int e : g.expirations) exps.append(e);
    return QJsonObject{
        {"underlying", QString::fromStdString(g.underlying)},
        {"greek", QString::fromStdString(g.greek_name)},
        {"spot_price", g.spot_price},
        {"strikes", strikes},
        {"expirations", exps},
        {"z", z_to_json(g.z)},
    };
}

QJsonObject skew_to_json(const surface::SkewSurfaceData& s) {
    QJsonArray deltas, exps;
    for (float d : s.deltas) deltas.append(d);
    for (int e : s.expirations) exps.append(e);
    return QJsonObject{
        {"underlying", QString::fromStdString(s.underlying)},
        {"deltas", deltas}, {"expirations", exps}, {"z", z_to_json(s.z)},
    };
}

QJsonObject db_surface_to_json(const DatabentoSurfaceResult& r) {
    QJsonArray xs, ys, xls, yls;
    for (float v : r.x_axis) xs.append(v);
    for (int v : r.y_axis) ys.append(v);
    for (const auto& s : r.x_labels) xls.append(QString::fromStdString(s));
    for (const auto& s : r.y_labels) yls.append(QString::fromStdString(s));
    return QJsonObject{
        {"success", r.success}, {"error", r.error}, {"type", r.type},
        {"x_axis", xs}, {"y_axis", ys},
        {"x_labels", xls}, {"y_labels", yls},
        {"z", z_to_json(r.z)},
    };
}

QJsonObject vol_result_to_json(const DatabentoVolSurfaceResult& r) {
    return QJsonObject{
        {"success", r.success}, {"error", r.error},
        {"vol", vol_surface_to_json(r.vol)},
        {"delta", greeks_to_json(r.delta)},
        {"gamma", greeks_to_json(r.gamma)},
        {"vega", greeks_to_json(r.vega)},
        {"theta", greeks_to_json(r.theta)},
        {"skew", skew_to_json(r.skew)},
    };
}

QJsonObject ohlcv_result_to_json(const DatabentoOhlcvResult& r) {
    QJsonObject data;
    for (auto it = r.data.constBegin(); it != r.data.constEnd(); ++it) {
        QJsonArray bars;
        for (const auto& b : it.value()) bars.append(b);
        data[it.key()] = bars;
    }
    return QJsonObject{{"success", r.success}, {"error", r.error}, {"data", data}};
}

QJsonObject futures_result_to_json(const DatabentoFuturesResult& /*r*/) {
    // CommodityForwardData / ContangoData are large internal structs; expose
    // a summary rather than full grids to keep the tool result manageable.
    return QJsonObject{{"note", "Use surface_ready / cached_futures payload (large nested grids)"}};
}

// Bridge helper for *_ready signals. Each DB fetch kicks the call and
// listens for one of (vol_surface_ready, surface_ready, ohlcv_ready,
// futures_ready, fetch_failed); the first match resolves the promise.
template <typename SignalSig, typename KickFn, typename PayloadFn>
void bridge_db_fetch(QPointer<QObject> holder_parent, ToolContext ctx,
                     std::shared_ptr<QPromise<ToolResult>> promise,
                     KickFn&& kick, SignalSig signal_member, PayloadFn&& payload_fn) {
    auto* svc = &DatabentoService::instance();
    AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise,
        [svc, kick = std::forward<KickFn>(kick), signal_member,
         payload_fn = std::forward<PayloadFn>(payload_fn)](auto resolve) {
            auto* h = new QObject(svc);
            QObject::connect(svc, signal_member, h,
                              [resolve, h, payload_fn](auto result) {
                                  resolve(ToolResult::ok_data(payload_fn(result)));
                                  h->deleteLater();
                              });
            QObject::connect(svc, &DatabentoService::fetch_failed, h,
                              [resolve, h](QString err) {
                                  resolve(ToolResult::fail(err)); h->deleteLater();
                              });
            kick();
        });
}
} // namespace

std::vector<ToolDef> get_surface_analytics_tools() {
    std::vector<ToolDef> tools;

    // 1. list_surface_categories
    {
        ToolDef t;
        t.name = "list_surface_categories";
        t.description = "List the 7 surface categories (Equity Deriv, Fixed Income, FX, Credit, Commodities, Risk, Macro).";
        t.category = "surface-analytics";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& cat : surface::get_surface_categories()) {
                QJsonArray types;
                for (auto type : cat.types) types.append(surface::chart_type_name(type));
                arr.append(QJsonObject{
                    {"name", QString::fromUtf8(cat.name)},
                    {"surface_types", types},
                    {"count", static_cast<int>(cat.types.size())},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // 2. list_surface_types
    {
        ToolDef t;
        t.name = "list_surface_types";
        t.description = "List all 35 surface types with capability metadata (tier, dataset, schema, required inputs, supported view modes).";
        t.category = "surface-analytics";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray arr;
            for (const auto& cap : surface::SURFACE_CAPABILITIES) arr.append(capability_to_json(cap));
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // 3. get_surface_capability
    {
        ToolDef t;
        t.name = "get_surface_capability";
        t.description = "Get capability metadata for one ChartType (tier, dataset/schema lineage, required inputs).";
        t.category = "surface-analytics";
        t.input_schema = ToolSchemaBuilder()
            .string("chart_type", "ChartType name (e.g. 'Volatility', 'DeltaSurface', 'YieldCurve')").required().length(1, 64)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const auto ct = parse_chart_type(args["chart_type"].toString());
            return ToolResult::ok_data(capability_to_json(surface::capability_for(ct)));
        };
        tools.push_back(std::move(t));
    }

    // 4. test_databento_connection
    {
        ToolDef t;
        t.name = "test_databento_connection";
        t.description = "Verify that the Databento API key works.";
        t.category = "surface-analytics";
        t.default_timeout_ms = 30000;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            auto* svc = &DatabentoService::instance();
            AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc](auto resolve) {
                auto* h = new QObject(svc);
                QObject::connect(svc, &DatabentoService::connection_tested, h,
                                  [resolve, h](bool ok, const QString& msg) {
                                      resolve(ok ? ToolResult::ok(msg) : ToolResult::fail(msg));
                                      h->deleteLater();
                                  });
                svc->test_connection();
            });
        };
        tools.push_back(std::move(t));
    }

    // 5. fetch_databento_options_surface
    {
        ToolDef t;
        t.name = "fetch_databento_options_surface";
        t.description = "Fetch the full options surface (vol + delta + gamma + vega + theta + skew) for an underlying.";
        t.category = "surface-analytics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Underlying ticker (e.g. SPY, AAPL)").required().length(1, 16)
            .number("spot", "Spot price (0 = look up from DataHub)").default_num(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString();
            const float spot = static_cast<float>(args["spot"].toDouble(0));
            bridge_db_fetch({}, std::move(ctx), promise,
                [sym, spot]() { DatabentoService::instance().fetch_options_surface(sym, spot); },
                &DatabentoService::vol_surface_ready,
                [](const DatabentoVolSurfaceResult& r) { return vol_result_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 6. fetch_databento_ohlcv
    {
        ToolDef t;
        t.name = "fetch_databento_ohlcv";
        t.description = "Fetch historical OHLCV bars for one or more symbols (used for correlation, PCA, drawdown, beta).";
        t.category = "surface-analytics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .array("symbols", "Symbols to fetch", QJsonObject{{"type", "string"}})
            .integer("days", "History window in days").default_int(60).between(1, 5000)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList syms;
            for (const auto& v : args["symbols"].toArray()) syms.append(v.toString());
            const int days = args["days"].toInt(60);
            bridge_db_fetch({}, std::move(ctx), promise,
                [syms, days]() { DatabentoService::instance().fetch_ohlcv(syms, days); },
                &DatabentoService::ohlcv_ready,
                [](const DatabentoOhlcvResult& r) { return ohlcv_result_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 7. fetch_databento_futures_term_structure
    {
        ToolDef t;
        t.name = "fetch_databento_futures_term_structure";
        t.description = "Fetch futures term structure for commodities (forward curve + contango/backwardation).";
        t.category = "surface-analytics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .array("commodities", "Commodity root symbols (e.g. CL, NG, GC)",
                   QJsonObject{{"type", "string"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList cs;
            for (const auto& v : args["commodities"].toArray()) cs.append(v.toString());
            bridge_db_fetch({}, std::move(ctx), promise,
                [cs]() { DatabentoService::instance().fetch_futures_term_structure(cs); },
                &DatabentoService::futures_ready,
                [](const DatabentoFuturesResult& r) { return futures_result_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // Generic single-arg fetch builder for surface_ready endpoints.
    auto make_db_surface_tool = [](const QString& name, const QString& desc,
                                    std::function<void(const QJsonObject&)> kick) {
        ToolDef t;
        t.name = name;
        t.description = desc;
        t.category = "surface-analytics";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        return t;
    };

    // 8. fetch_databento_local_vol
    {
        ToolDef t = make_db_surface_tool("fetch_databento_local_vol",
            "Fetch local volatility surface for an underlying.", {});
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Underlying ticker").required().length(1, 16)
            .number("spot", "Spot price (0 = auto)").default_num(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString();
            const float spot = static_cast<float>(args["spot"].toDouble(0));
            bridge_db_fetch({}, std::move(ctx), promise,
                [sym, spot]() { DatabentoService::instance().fetch_local_vol(sym, spot); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 9. fetch_databento_implied_dividend
    {
        ToolDef t = make_db_surface_tool("fetch_databento_implied_dividend",
            "Fetch implied dividend surface.", {});
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Underlying ticker").required().length(1, 16)
            .number("spot", "Spot price (0 = auto)").default_num(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString();
            const float spot = static_cast<float>(args["spot"].toDouble(0));
            bridge_db_fetch({}, std::move(ctx), promise,
                [sym, spot]() { DatabentoService::instance().fetch_implied_dividend(sym, spot); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 10. fetch_databento_liquidity
    {
        ToolDef t = make_db_surface_tool("fetch_databento_liquidity",
            "Fetch options liquidity heatmap for an underlying.", {});
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Underlying ticker").required().length(1, 16)
            .number("spot", "Spot price (0 = auto)").default_num(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString();
            const float spot = static_cast<float>(args["spot"].toDouble(0));
            bridge_db_fetch({}, std::move(ctx), promise,
                [sym, spot]() { DatabentoService::instance().fetch_liquidity(sym, spot); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 11. fetch_databento_commodity_vol
    {
        ToolDef t = make_db_surface_tool("fetch_databento_commodity_vol",
            "Fetch commodity volatility surface for a root symbol.", {});
        t.input_schema = ToolSchemaBuilder()
            .string("root_symbol", "Commodity root (e.g. CL, NG)").required().length(1, 16)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString s = args["root_symbol"].toString();
            bridge_db_fetch({}, std::move(ctx), promise,
                [s]() { DatabentoService::instance().fetch_commodity_vol(s); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 12. fetch_databento_crack_spread
    {
        ToolDef t = make_db_surface_tool("fetch_databento_crack_spread",
            "Fetch crack-spread / crush-spread surface (energy/grains).", {});
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            bridge_db_fetch({}, std::move(ctx), promise,
                []() { DatabentoService::instance().fetch_crack_spread(); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 13. fetch_databento_stress_test
    {
        ToolDef t = make_db_surface_tool("fetch_databento_stress_test",
            "Run stress-test P&L surface for a basket of symbols.", {});
        t.input_schema = ToolSchemaBuilder()
            .array("symbols", "Basket symbols", QJsonObject{{"type", "string"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            QStringList syms;
            for (const auto& v : args["symbols"].toArray()) syms.append(v.toString());
            bridge_db_fetch({}, std::move(ctx), promise,
                [syms]() { DatabentoService::instance().fetch_stress_test(syms); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 14. fetch_databento_yield_curve
    {
        ToolDef t = make_db_surface_tool("fetch_databento_yield_curve",
            "Fetch a yield-curve surface (Treasuries / sovereigns).", {});
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            bridge_db_fetch({}, std::move(ctx), promise,
                []() { DatabentoService::instance().fetch_yield_curve(); },
                &DatabentoService::surface_ready,
                [](const DatabentoSurfaceResult& r) { return db_surface_to_json(r); });
        };
        tools.push_back(std::move(t));
    }

    // 15. get_databento_cached_data
    {
        ToolDef t;
        t.name = "get_databento_cached_data";
        t.description = "Get the last-fetched cached Databento data (vol, ohlcv, futures) without triggering a network call.";
        t.category = "surface-analytics";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& svc = DatabentoService::instance();
            return ToolResult::ok_data(QJsonObject{
                {"has_api_key", svc.has_api_key()},
                {"last_fetch_time", svc.last_fetch_time().toString(Qt::ISODate)},
                {"vol", vol_result_to_json(svc.last_vol())},
                {"ohlcv", ohlcv_result_to_json(svc.last_ohlcv())},
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 surface-analytics tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
