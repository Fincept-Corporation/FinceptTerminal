// EquityResearchTools.cpp — Tools that drive the Equity Research screen.
//
// 12 tools in category "equity-research":
//   1. search_equity_symbols
//   2. load_equity_symbol            — combined quote + info + historical
//   3. get_equity_quote              — quote only (price/change/vol)
//   4. get_equity_info               — full company / valuation profile
//   5. get_equity_historical         — OHLCV candles for a period
//   6. get_equity_financials         — income / balance / cashflow
//   7. get_equity_technicals         — indicators + overall signal
//   8. get_equity_peers              — peer-group comparison
//   9. get_equity_news               — recent news articles for a symbol
//  10. compute_equity_talipp         — run a talipp indicator (generic)
//  11. list_equity_talipp_indicators — talipp indicator catalog (sync)
//  12. get_equity_sentiment          — MarketSentimentService snapshot
//
// EquityResearchService signals do NOT carry a per-call request_id; most
// carry the symbol (or indicator) so we filter by that. Concurrent calls
// for the SAME symbol can race — caller must serialise per-symbol if it
// matters.

#include "mcp/tools/EquityResearchTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/MarketSentimentService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace fincept::mcp::tools {

namespace {

static constexpr const char* TAG = "EquityResearchTools";

// Most calls hit yfinance via Python; 90s default covers slow paths.
static constexpr int kDefaultTimeoutMs = 90000;

QJsonObject quote_to_json(const services::equity::QuoteData& q) {
    return QJsonObject{
        {"symbol", q.symbol},
        {"price", q.price},
        {"change", q.change},
        {"change_pct", q.change_pct},
        {"open", q.open},
        {"high", q.high},
        {"low", q.low},
        {"prev_close", q.prev_close},
        {"volume", q.volume},
        {"exchange", q.exchange},
        {"timestamp", q.timestamp},
    };
}

QJsonObject info_to_json(const services::equity::StockInfo& i) {
    return QJsonObject{
        {"symbol", i.symbol},
        {"company_name", i.company_name},
        {"sector", i.sector},
        {"industry", i.industry},
        {"description", i.description},
        {"website", i.website},
        {"country", i.country},
        {"currency", i.currency},
        {"exchange", i.exchange},
        {"employees", i.employees},
        {"market_cap", i.market_cap},
        {"enterprise_value", i.enterprise_value},
        {"pe_ratio", i.pe_ratio},
        {"forward_pe", i.forward_pe},
        {"peg_ratio", i.peg_ratio},
        {"price_to_book", i.price_to_book},
        {"ev_to_revenue", i.ev_to_revenue},
        {"ev_to_ebitda", i.ev_to_ebitda},
        {"gross_margins", i.gross_margins},
        {"operating_margins", i.operating_margins},
        {"ebitda_margins", i.ebitda_margins},
        {"profit_margins", i.profit_margins},
        {"roe", i.roe},
        {"roa", i.roa},
        {"gross_profits", i.gross_profits},
        {"book_value", i.book_value},
        {"revenue_per_share", i.revenue_per_share},
        {"free_cashflow", i.free_cashflow},
        {"operating_cashflow", i.operating_cashflow},
        {"total_cash", i.total_cash},
        {"total_debt", i.total_debt},
        {"total_revenue", i.total_revenue},
        {"earnings_growth", i.earnings_growth},
        {"revenue_growth", i.revenue_growth},
        {"shares_outstanding", i.shares_outstanding},
        {"float_shares", i.float_shares},
        {"held_insiders_pct", i.held_insiders_pct},
        {"held_institutions_pct", i.held_institutions_pct},
        {"short_ratio", i.short_ratio},
        {"short_pct_of_float", i.short_pct_of_float},
        {"week52_high", i.week52_high},
        {"week52_low", i.week52_low},
        {"avg_volume", i.avg_volume},
        {"beta", i.beta},
        {"dividend_yield", i.dividend_yield},
        {"current_price", i.current_price},
        {"target_high", i.target_high},
        {"target_low", i.target_low},
        {"target_mean", i.target_mean},
        {"recommendation_mean", i.recommendation_mean},
        {"recommendation_key", i.recommendation_key},
        {"analyst_count", i.analyst_count},
    };
}

QJsonArray candles_to_json(const QVector<services::equity::Candle>& cs) {
    QJsonArray arr;
    for (const auto& c : cs) {
        arr.append(QJsonObject{
            {"timestamp", c.timestamp},
            {"open", c.open},
            {"high", c.high},
            {"low", c.low},
            {"close", c.close},
            {"volume", static_cast<double>(c.volume)},
        });
    }
    return arr;
}

QJsonArray financials_section_to_json(const QVector<QPair<QString, QJsonObject>>& xs) {
    QJsonArray arr;
    for (const auto& kv : xs)
        arr.append(QJsonObject{{"period", kv.first}, {"items", kv.second}});
    return arr;
}

const char* tech_signal_str(services::equity::TechSignal s) {
    using TS = services::equity::TechSignal;
    switch (s) {
        case TS::StrongBuy:  return "strong_buy";
        case TS::Buy:        return "buy";
        case TS::Neutral:    return "neutral";
        case TS::Sell:       return "sell";
        case TS::StrongSell: return "strong_sell";
    }
    return "neutral";
}

QJsonArray indicators_to_json(const QVector<services::equity::TechIndicator>& xs) {
    QJsonArray arr;
    for (const auto& x : xs) {
        arr.append(QJsonObject{
            {"name", x.name},
            {"value", x.value},
            {"signal", tech_signal_str(x.signal)},
            {"category", x.category},
        });
    }
    return arr;
}

QJsonObject technicals_to_json(const services::equity::TechnicalsData& t) {
    return QJsonObject{
        {"symbol", t.symbol},
        {"trend", indicators_to_json(t.trend)},
        {"momentum", indicators_to_json(t.momentum)},
        {"volatility", indicators_to_json(t.volatility)},
        {"volume", indicators_to_json(t.volume)},
        {"overall_signal", tech_signal_str(t.overall_signal)},
        {"strong_buy", t.strong_buy},
        {"buy", t.buy},
        {"neutral", t.neutral},
        {"sell", t.sell},
        {"strong_sell", t.strong_sell},
    };
}

QJsonArray peers_to_json(const QVector<services::equity::PeerData>& ps) {
    QJsonArray arr;
    for (const auto& p : ps) {
        arr.append(QJsonObject{
            {"symbol", p.symbol},
            {"name", p.name},
            {"sector", p.sector},
            {"market_cap", p.market_cap},
            {"pe_ratio", p.pe_ratio},
            {"forward_pe", p.forward_pe},
            {"price_to_book", p.price_to_book},
            {"price_to_sales", p.price_to_sales},
            {"peg_ratio", p.peg_ratio},
            {"roe", p.roe},
            {"roa", p.roa},
            {"profit_margin", p.profit_margin},
            {"operating_margin", p.operating_margin},
            {"gross_margin", p.gross_margin},
            {"revenue_growth", p.revenue_growth},
            {"earnings_growth", p.earnings_growth},
            {"debt_to_equity", p.debt_to_equity},
            {"current_ratio", p.current_ratio},
            {"quick_ratio", p.quick_ratio},
            {"dividend_yield", p.dividend_yield},
            {"beta", p.beta},
            {"price", p.price},
            {"change_pct", p.change_pct},
        });
    }
    return arr;
}

QJsonArray news_to_json(const QVector<services::equity::NewsArticle>& xs) {
    QJsonArray arr;
    for (const auto& a : xs) {
        arr.append(QJsonObject{
            {"title", a.title},
            {"description", a.description},
            {"url", a.url},
            {"publisher", a.publisher},
            {"published_date", a.published_date},
        });
    }
    return arr;
}

QJsonObject sentiment_to_json(const services::equity::MarketSentimentSnapshot& s) {
    QJsonArray sources;
    for (const auto& src : s.sources) {
        sources.append(QJsonObject{
            {"source_id", src.source_id},
            {"label", src.label},
            {"available", src.available},
            {"buzz_score", src.buzz_score},
            {"bullish_pct", src.bullish_pct},
            {"sentiment_score", src.sentiment_score},
            {"activity_count", src.activity_count},
        });
    }
    return QJsonObject{
        {"symbol", s.symbol},
        {"configured", s.configured},
        {"available", s.available},
        {"status", s.status},
        {"message", s.message},
        {"average_buzz", s.average_buzz},
        {"average_bullish_pct", s.average_bullish_pct},
        {"coverage", s.coverage},
        {"source_alignment", s.source_alignment},
        {"fetched_at", s.fetched_at},
        {"sources", sources},
    };
}

// Hand-curated talipp catalogue mirrored from EquityTalippTab::categories().
// Kept in this TU to avoid pulling the UI header into the tools layer.
struct TalippEntry { const char* id; const char* label; const char* data_type; const char* category; };
static const TalippEntry kTalipp[] = {
    // trend
    {"sma","SMA","prices","trend"}, {"ema","EMA","prices","trend"}, {"wma","WMA","prices","trend"},
    {"dema","DEMA","prices","trend"}, {"tema","TEMA","prices","trend"}, {"hma","HMA","prices","trend"},
    {"kama","KAMA","prices","trend"}, {"alma","ALMA","prices","trend"}, {"t3","T3","prices","trend"},
    {"zlema","ZLEMA","prices","trend"},
    // trend advanced
    {"adx","ADX","ohlcv","trend_advanced"}, {"aroon","Aroon","ohlcv","trend_advanced"},
    {"ichimoku","Ichimoku","ohlcv","trend_advanced"}, {"parabolic_sar","Parabolic SAR","ohlcv","trend_advanced"},
    {"supertrend","SuperTrend","ohlcv","trend_advanced"},
    // momentum
    {"rsi","RSI","prices","momentum"}, {"macd","MACD","prices","momentum"},
    {"stoch","Stochastic","ohlcv","momentum"}, {"stoch_rsi","StochRSI","prices","momentum"},
    {"cci","CCI","ohlcv","momentum"}, {"roc","ROC","prices","momentum"},
    {"tsi","TSI","prices","momentum"}, {"williams","Williams %R","ohlcv","momentum"},
    // volatility
    {"atr","ATR","ohlcv","volatility"}, {"bb","Bollinger Bands","prices","volatility"},
    {"keltner","Keltner Channels","ohlcv","volatility"}, {"donchian","Donchian Channels","ohlcv","volatility"},
    {"chandelier_stop","Chandelier Stop","ohlcv","volatility"}, {"natr","NATR","ohlcv","volatility"},
    // volume
    {"obv","OBV","ohlcv","volume"}, {"vwap","VWAP","ohlcv","volume"},
    {"vwma","VWMA","ohlcv","volume"}, {"mfi","MFI","ohlcv","volume"},
    {"chaikin_osc","Chaikin Osc","ohlcv","volume"}, {"force_index","Force Index","ohlcv","volume"},
    // specialized
    {"ao","Awesome Osc","ohlcv","specialized"}, {"accu_dist","Accum/Dist","ohlcv","specialized"},
    {"bop","Balance of Pwr","ohlcv","specialized"}, {"chop","CHOP","ohlcv","specialized"},
    {"coppock_curve","Coppock Curve","prices","specialized"}, {"dpo","DPO","prices","specialized"},
    {"emv","EMV","ohlcv","specialized"}, {"ibs","IBS","ohlcv","specialized"},
    {"kst","KST","prices","specialized"}, {"kvo","KVO","ohlcv","specialized"},
    {"mass_index","Mass Index","ohlcv","specialized"}, {"mcginley","McGinley","prices","specialized"},
    {"mean_dev","Mean Dev","prices","specialized"}, {"smma","SMMA","prices","specialized"},
    {"sobv","Smoothed OBV","ohlcv","specialized"}, {"stc","STC","prices","specialized"},
    {"std_dev","Std Dev","prices","specialized"}, {"trix","TRIX","prices","specialized"},
    {"ttm","TTM Squeeze","ohlcv","specialized"}, {"uo","Ultimate Osc","ohlcv","specialized"},
    {"vtx","Vortex","ohlcv","specialized"}, {"zigzag","ZigZag","ohlcv","specialized"},
};

} // namespace

std::vector<ToolDef> get_equity_research_tools() {
    std::vector<ToolDef> tools;

    // ── 1. search_equity_symbols ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_equity_symbols";
        t.description = "Search for tradable equity symbols matching a query (ticker, name, fragment).";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("query", "Search query").required().length(1, 128)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString q = args["query"].toString();
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, q](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::EquityResearchService::search_results_loaded, holder,
                                      [resolve, holder](QVector<services::equity::SearchResult> rs) {
                                          QJsonArray arr;
                                          for (const auto& r : rs) {
                                              arr.append(QJsonObject{
                                                  {"symbol", r.symbol},
                                                  {"name", r.name},
                                                  {"exchange", r.exchange},
                                                  {"type", r.type},
                                                  {"currency", r.currency},
                                                  {"industry", r.industry},
                                              });
                                          }
                                          resolve(ToolResult::ok_data(arr));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->search_symbols(q);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 2. load_equity_symbol (combined quote + info + historical) ─────
    // Kicks load_symbol() which spawns three parallel Python calls
    // emitting three different signals. We collect all three (filtering
    // by symbol where the signal carries it) and resolve when all are in.
    {
        ToolDef t;
        t.name = "load_equity_symbol";
        t.description = "Fetch quote + info + historical OHLCV for a symbol in one call (parallel under the hood).";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol (e.g. AAPL)").required().length(1, 32)
            .string("period", "Historical period (1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, max)")
                .default_str("1y").length(1, 8)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            const QString period = args["period"].toString("1y");
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, period](auto resolve) {
                    struct State {
                        QJsonObject quote;
                        QJsonObject info;
                        QJsonArray candles;
                        bool got_quote = false;
                        bool got_info = false;
                        bool got_hist = false;
                    };
                    auto state = std::make_shared<State>();
                    auto* holder = new QObject(svc);
                    auto try_finish = [resolve, holder, state, sym]() {
                        if (state->got_quote && state->got_info && state->got_hist) {
                            resolve(ToolResult::ok_data(QJsonObject{
                                {"symbol", sym},
                                {"quote", state->quote},
                                {"info", state->info},
                                {"historical", state->candles},
                            }));
                            holder->deleteLater();
                        }
                    };
                    QObject::connect(svc, &services::equity::EquityResearchService::quote_loaded, holder,
                                      [sym, state, try_finish](services::equity::QuoteData q) {
                                          if (q.symbol.toUpper() != sym) return;
                                          state->quote = quote_to_json(q);
                                          state->got_quote = true;
                                          try_finish();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::info_loaded, holder,
                                      [sym, state, try_finish](services::equity::StockInfo i) {
                                          if (i.symbol.toUpper() != sym) return;
                                          state->info = info_to_json(i);
                                          state->got_info = true;
                                          try_finish();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::historical_loaded, holder,
                                      [sym, state, try_finish](QString s, QVector<services::equity::Candle> cs) {
                                          if (s.toUpper() != sym) return;
                                          state->candles = candles_to_json(cs);
                                          state->got_hist = true;
                                          try_finish();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->load_symbol(sym, period);
                });
        };
        tools.push_back(std::move(t));
    }

    // Generator for the three "single-signal" variants (quote / info /
    // historical). They all kick load_symbol() and listen for one signal.
    auto make_single = [](const QString& tool_name, const QString& desc, char which) {
        ToolDef t;
        t.name = tool_name;
        t.description = desc;
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .string("period", "Historical period (only used for historical)").default_str("1y").length(1, 8)
            .build();
        t.async_handler = [which](const QJsonObject& args, ToolContext ctx,
                                   std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            const QString period = args["period"].toString("1y");
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, period, which](auto resolve) {
                    auto* holder = new QObject(svc);
                    if (which == 'q') {
                        QObject::connect(svc, &services::equity::EquityResearchService::quote_loaded, holder,
                                          [sym, resolve, holder](services::equity::QuoteData q) {
                                              if (q.symbol.toUpper() != sym) return;
                                              resolve(ToolResult::ok_data(quote_to_json(q)));
                                              holder->deleteLater();
                                          });
                    } else if (which == 'i') {
                        QObject::connect(svc, &services::equity::EquityResearchService::info_loaded, holder,
                                          [sym, resolve, holder](services::equity::StockInfo i) {
                                              if (i.symbol.toUpper() != sym) return;
                                              resolve(ToolResult::ok_data(info_to_json(i)));
                                              holder->deleteLater();
                                          });
                    } else { // 'h'
                        QObject::connect(svc, &services::equity::EquityResearchService::historical_loaded, holder,
                                          [sym, resolve, holder](QString s, QVector<services::equity::Candle> cs) {
                                              if (s.toUpper() != sym) return;
                                              resolve(ToolResult::ok_data(QJsonObject{
                                                  {"symbol", s},
                                                  {"candles", candles_to_json(cs)},
                                                  {"count", static_cast<int>(cs.size())},
                                              }));
                                              holder->deleteLater();
                                          });
                    }
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->load_symbol(sym, period);
                });
        };
        return t;
    };

    // ── 3-5. get_equity_quote / get_equity_info / get_equity_historical ─
    tools.push_back(make_single("get_equity_quote",
                                  "Get current quote (price/change/volume) for a symbol.", 'q'));
    tools.push_back(make_single("get_equity_info",
                                  "Get full company info + valuation + analyst targets for a symbol.", 'i'));
    tools.push_back(make_single("get_equity_historical",
                                  "Get OHLCV historical candles for a symbol over a period.", 'h'));

    // ── 6. get_equity_financials ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_equity_financials";
        t.description = "Get income statement, balance sheet, and cash flow for a symbol (multi-period).";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::EquityResearchService::financials_loaded, holder,
                                      [sym, resolve, holder](services::equity::FinancialsData f) {
                                          if (f.symbol.toUpper() != sym) return;
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"symbol", f.symbol},
                                              {"income_statement", financials_section_to_json(f.income_statement)},
                                              {"balance_sheet", financials_section_to_json(f.balance_sheet)},
                                              {"cash_flow", financials_section_to_json(f.cash_flow)},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_financials(sym);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 7. get_equity_technicals ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_equity_technicals";
        t.description = "Get technical indicators (trend/momentum/volatility/volume) and overall buy/sell signal.";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .string("period", "Historical period for indicator calc").default_str("1y").length(1, 8)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            const QString period = args["period"].toString("1y");
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, period](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::EquityResearchService::technicals_loaded, holder,
                                      [sym, resolve, holder](services::equity::TechnicalsData td) {
                                          if (td.symbol.toUpper() != sym) return;
                                          resolve(ToolResult::ok_data(technicals_to_json(td)));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_technicals(sym, period);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 8. get_equity_peers ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_equity_peers";
        t.description = "Get peer comparison metrics for a symbol against a caller-supplied list of peer symbols.";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Anchor symbol").required().length(1, 32)
            .array("peer_symbols", "List of peer ticker symbols", QJsonObject{{"type", "string"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            QStringList peers;
            for (const auto& v : args["peer_symbols"].toArray())
                peers.append(v.toString().toUpper());
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, peers](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::EquityResearchService::peers_loaded, holder,
                                      [resolve, holder](QVector<services::equity::PeerData> ps) {
                                          resolve(ToolResult::ok_data(peers_to_json(ps)));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_peers(sym, peers);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 9. get_equity_news ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_equity_news";
        t.description = "Get recent news articles for a symbol.";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .integer("count", "Max articles").default_int(20).between(1, 100)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            const int count = args["count"].toInt(20);
            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, count](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::EquityResearchService::news_loaded, holder,
                                      [sym, resolve, holder](QString s, QVector<services::equity::NewsArticle> as) {
                                          if (s.toUpper() != sym) return;
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"symbol", s},
                                              {"articles", news_to_json(as)},
                                              {"count", static_cast<int>(as.size())},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_news(sym, count);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 10. compute_equity_talipp ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "compute_equity_talipp";
        t.description = "Compute a talipp technical indicator over historical data for a symbol.";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .string("indicator", "Indicator id (use list_equity_talipp_indicators)").required().length(1, 32)
            .object("params", "Indicator-specific params (e.g. {period: 20})")
            .string("period", "Historical period").default_str("2y").length(1, 8)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            const QString ind = args["indicator"].toString();
            const QJsonObject params = args["params"].toObject();
            const QString period = args["period"].toString("2y");

            // Convert JSON params object → QVariantMap for the service API.
            QVariantMap pmap;
            for (auto it = params.constBegin(); it != params.constEnd(); ++it)
                pmap.insert(it.key(), it.value().toVariant());

            auto* svc = &services::equity::EquityResearchService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, ind, pmap, period](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::EquityResearchService::talipp_result, holder,
                                      [ind, resolve, holder](QString got_ind, QVector<double> vs,
                                                              QVector<qint64> ts) {
                                          if (got_ind != ind) return;
                                          QJsonArray values;
                                          for (double v : vs) values.append(v);
                                          QJsonArray timestamps;
                                          for (qint64 t : ts) timestamps.append(t);
                                          resolve(ToolResult::ok_data(QJsonObject{
                                              {"indicator", got_ind},
                                              {"values", values},
                                              {"timestamps", timestamps},
                                              {"count", static_cast<int>(vs.size())},
                                          }));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::EquityResearchService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->compute_talipp(sym, ind, pmap, period);
                });
        };
        tools.push_back(std::move(t));
    }

    // ── 11. list_equity_talipp_indicators ───────────────────────────────
    {
        ToolDef t;
        t.name = "list_equity_talipp_indicators";
        t.description = "List all talipp indicators (id, label, category, data_type: 'prices' or 'ohlcv').";
        t.category = "equity-research";
        t.input_schema = ToolSchemaBuilder()
            .string("category", "Optional category filter (trend, trend_advanced, momentum, volatility, volume, specialized)")
                .default_str("").length(0, 32)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString filter = args["category"].toString();
            QJsonArray arr;
            for (const auto& e : kTalipp) {
                if (!filter.isEmpty() && filter != QString::fromUtf8(e.category))
                    continue;
                arr.append(QJsonObject{
                    {"id", QString::fromUtf8(e.id)},
                    {"label", QString::fromUtf8(e.label)},
                    {"data_type", QString::fromUtf8(e.data_type)},
                    {"category", QString::fromUtf8(e.category)},
                });
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── 12. get_equity_sentiment ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_equity_sentiment";
        t.description = "Get a market-sentiment snapshot for a symbol (buzz, bullish %, multi-source coverage).";
        t.category = "equity-research";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Ticker symbol").required().length(1, 32)
            .integer("days", "Lookback window in days").default_int(7).between(1, 90)
            .boolean("force", "Bypass cache").default_bool(false)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString sym = args["symbol"].toString().toUpper();
            const int days = args["days"].toInt(7);
            const bool force = args["force"].toBool(false);
            auto* svc = &services::equity::MarketSentimentService::instance();
            AsyncDispatch::callback_to_promise(
                svc, std::move(ctx), promise,
                [svc, sym, days, force](auto resolve) {
                    auto* holder = new QObject(svc);
                    QObject::connect(svc, &services::equity::MarketSentimentService::snapshot_loaded, holder,
                                      [sym, resolve, holder](QString s,
                                                              services::equity::MarketSentimentSnapshot snap) {
                                          if (s.toUpper() != sym) return;
                                          resolve(ToolResult::ok_data(sentiment_to_json(snap)));
                                          holder->deleteLater();
                                      });
                    QObject::connect(svc, &services::equity::MarketSentimentService::error_occurred, holder,
                                      [resolve, holder](QString, QString msg) {
                                          resolve(ToolResult::fail(msg));
                                          holder->deleteLater();
                                      });
                    svc->fetch_snapshot(sym, days, force);
                });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 equity-research tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
