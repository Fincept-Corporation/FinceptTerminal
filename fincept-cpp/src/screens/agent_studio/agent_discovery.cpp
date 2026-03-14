// Agent Discovery — Built-in agent definitions + custom agents from SQLite

#include "agent_discovery.h"
#include "storage/database.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace fincept::agent_studio {

using json = nlohmann::json;

AgentDiscovery& AgentDiscovery::instance() {
    static AgentDiscovery disc;
    return disc;
}

// ============================================================================
// Public API
// ============================================================================
std::vector<AgentCard> AgentDiscovery::get_all_agents() {
    std::lock_guard<std::mutex> lock(mutex_);
    double now = ImGui::GetTime();
    if (cache_.empty() || (now - last_refresh_) > CACHE_TTL) {
        cache_.clear();
        auto builtins = get_builtin_agents();
        auto customs = load_custom_agents();
        cache_.insert(cache_.end(), builtins.begin(), builtins.end());
        cache_.insert(cache_.end(), customs.begin(), customs.end());
        last_refresh_ = now;
        LOG_INFO("AgentDiscovery", "Discovered %zu agents (%zu builtin, %zu custom)",
                 cache_.size(), builtins.size(), customs.size());
    }
    return cache_;
}

void AgentDiscovery::refresh() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_refresh_ = 0; // Force refresh on next get_all_agents()
    cache_.clear();
}

std::vector<AgentCard> AgentDiscovery::get_by_category(const std::string& category) {
    auto all = get_all_agents();
    if (category == "all" || category.empty()) return all;
    std::vector<AgentCard> result;
    for (auto& a : all) {
        if (a.category == category) result.push_back(a);
    }
    return result;
}

std::vector<AgentCard> AgentDiscovery::search(const std::string& query) {
    if (query.empty()) return get_all_agents();

    // Case-insensitive search
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    auto all = get_all_agents();
    std::vector<AgentCard> result;
    for (auto& a : all) {
        std::string name = a.name;
        std::string desc = a.description;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(desc.begin(), desc.end(), desc.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (name.find(q) != std::string::npos || desc.find(q) != std::string::npos) {
            result.push_back(a);
        }
    }
    return result;
}

std::vector<std::string> AgentDiscovery::get_categories() {
    auto all = get_all_agents();
    std::vector<std::string> cats;
    for (auto& a : all) {
        if (std::find(cats.begin(), cats.end(), a.category) == cats.end()) {
            cats.push_back(a.category);
        }
    }
    return cats;
}

int AgentDiscovery::total_count() const {
    return (int)cache_.size();
}

int AgentDiscovery::custom_count() const {
    int count = 0;
    for (auto& a : cache_) {
        if (a.user_created) count++;
    }
    return count;
}

// ============================================================================
// Load custom agents from SQLite
// ============================================================================
std::vector<AgentCard> AgentDiscovery::load_custom_agents() {
    std::vector<AgentCard> result;
    auto rows = db::ops::get_agent_configs();
    for (auto& row : rows) {
        AgentCard card;
        card.id = row.id;
        card.name = row.name;
        card.description = row.description;
        card.category = row.category;
        card.user_created = true;
        card.version = "1.0";
        try {
            card.config = json::parse(row.config_json);
            card.provider = card.config.value("provider",
                card.config.contains("model") ? card.config["model"].value("provider", "openai") : "openai");
        } catch (...) {
            card.config = json::object();
            card.provider = "openai";
        }
        result.push_back(std::move(card));
    }
    return result;
}

// ============================================================================
// Built-in agent definitions
// Hardcoded to match finagent_core/configs/*.json + specialized agent groups
// ============================================================================
static AgentCard make_agent(const std::string& id, const std::string& name,
                             const std::string& desc, const std::string& category,
                             const std::string& provider, const std::string& model,
                             const std::string& instructions,
                             const std::vector<std::string>& capabilities = {}) {
    AgentCard card;
    card.id = id;
    card.name = name;
    card.description = desc;
    card.category = category;
    card.provider = provider;
    card.version = "1.0";
    card.capabilities = capabilities;
    card.config = {
        {"model", {{"provider", provider}, {"model_id", model}}},
        {"instructions", instructions},
        {"temperature", 0.7},
        {"max_tokens", 4096}
    };
    return card;
}

std::vector<AgentCard> AgentDiscovery::get_builtin_agents() {
    std::vector<AgentCard> agents;

    // ── FinAgent Core Configs (16 agents) ──
    agents.push_back(make_agent(
        "trading_agent", "Trading Agent",
        "Trading signals, order execution, position sizing, and risk management",
        "trader", "openai", "gpt-4o",
        "You are an expert trading agent. Analyze market data, generate trading signals, "
        "determine optimal entry/exit points, and manage position sizing with risk controls.",
        {"trading", "signals", "execution"}
    ));

    agents.push_back(make_agent(
        "portfolio_management_agent", "Portfolio Management Agent",
        "Portfolio construction, rebalancing, allocation optimization, and performance tracking",
        "trader", "openai", "gpt-4o",
        "You are a portfolio management agent. Optimize asset allocation, suggest rebalancing "
        "strategies, track performance metrics, and manage diversification.",
        {"portfolio", "allocation", "rebalancing"}
    ));

    agents.push_back(make_agent(
        "analysis_agent", "Analysis Agent",
        "General financial analysis covering fundamentals, technicals, and market conditions",
        "analysis", "openai", "gpt-4o",
        "You are a financial analysis agent. Provide comprehensive analysis including "
        "fundamental metrics, technical indicators, valuation models, and market outlook.",
        {"analysis", "fundamentals", "technicals"}
    ));

    agents.push_back(make_agent(
        "stock_analysis_agent", "Stock Analysis Agent",
        "Deep-dive individual stock analysis with valuation, catalysts, and risk factors",
        "analysis", "openai", "gpt-4o",
        "You are a stock analysis specialist. Analyze individual stocks with DCF valuation, "
        "peer comparison, growth drivers, risk factors, and price targets.",
        {"stocks", "valuation", "research"}
    ));

    agents.push_back(make_agent(
        "macro_cycle_agent", "Macro Cycle Agent",
        "Business cycle analysis, macro indicators, and cycle-aware investment positioning",
        "economic", "openai", "gpt-4o",
        "You are a macro cycle analyst. Identify the current business cycle phase, analyze "
        "leading indicators, and recommend sector rotations and positioning strategies.",
        {"macro", "cycles", "indicators"}
    ));

    agents.push_back(make_agent(
        "central_bank_analyst", "Central Bank Analyst",
        "Federal Reserve policy analysis, rate decisions, and monetary policy implications",
        "economic", "openai", "gpt-4o",
        "You are a central bank policy analyst. Analyze Fed communications, predict rate "
        "decisions, assess quantitative tightening/easing impacts, and track policy shifts.",
        {"fed", "rates", "policy"}
    ));

    agents.push_back(make_agent(
        "behavioral_finance_analyst", "Behavioral Finance Analyst",
        "Market psychology, sentiment analysis, and behavioral bias identification",
        "analysis", "openai", "gpt-4o",
        "You are a behavioral finance specialist. Identify cognitive biases, analyze market "
        "sentiment, detect herding behavior, and assess fear/greed indicators.",
        {"psychology", "sentiment", "biases"}
    ));

    agents.push_back(make_agent(
        "currency_strategist", "Currency Strategist",
        "FX market analysis, currency pair dynamics, and cross-border investment implications",
        "economic", "openai", "gpt-4o",
        "You are a currency strategist. Analyze FX pair dynamics, interest rate differentials, "
        "trade flows, and geopolitical impacts on currencies.",
        {"forex", "currencies", "strategy"}
    ));

    agents.push_back(make_agent(
        "geopolitical_risk_analyst", "Geopolitical Risk Analyst",
        "Geopolitical events, conflict analysis, and market impact assessment",
        "geopolitics", "openai", "gpt-4o",
        "You are a geopolitical risk analyst. Assess global conflicts, trade tensions, "
        "sanctions, and their financial market implications.",
        {"geopolitics", "risk", "conflicts"}
    ));

    agents.push_back(make_agent(
        "innovation_disruption_analyst", "Innovation & Disruption Analyst",
        "Emerging technology trends, disruptive companies, and innovation-driven investment",
        "analysis", "openai", "gpt-4o",
        "You are an innovation analyst. Identify disruptive technologies, assess adoption curves, "
        "evaluate competitive moats, and find investment opportunities in emerging sectors.",
        {"innovation", "technology", "disruption"}
    ));

    agents.push_back(make_agent(
        "institutional_flow_tracker", "Institutional Flow Tracker",
        "Track institutional money flows, 13F filings, and smart money positioning",
        "trader", "openai", "gpt-4o",
        "You are an institutional flow analyst. Track 13F filings, analyze fund positioning, "
        "identify accumulation/distribution patterns, and follow smart money signals.",
        {"institutions", "flows", "13f"}
    ));

    agents.push_back(make_agent(
        "sentiment_news_analyst", "Sentiment & News Analyst",
        "News analysis, social media sentiment, and event-driven trading signals",
        "analysis", "openai", "gpt-4o",
        "You are a sentiment analyst. Analyze financial news, social media trends, "
        "earnings call sentiment, and generate event-driven trading insights.",
        {"sentiment", "news", "social"}
    ));

    agents.push_back(make_agent(
        "supply_chain_analyst", "Supply Chain Analyst",
        "Global supply chain analysis, bottleneck identification, and trade flow mapping",
        "analysis", "openai", "gpt-4o",
        "You are a supply chain analyst. Map global supply networks, identify bottlenecks, "
        "assess shipping/logistics disruptions, and evaluate supply-side investment impacts.",
        {"supply-chain", "logistics", "trade"}
    ));

    agents.push_back(make_agent(
        "regulatory_policy_analyst", "Regulatory & Policy Analyst",
        "Regulatory changes, compliance impacts, and policy-driven market effects",
        "economic", "openai", "gpt-4o",
        "You are a regulatory analyst. Track regulatory changes, assess compliance impacts, "
        "analyze antitrust actions, and evaluate policy-driven market shifts.",
        {"regulation", "compliance", "policy"}
    ));

    agents.push_back(make_agent(
        "renaissance_technologies_agent", "Renaissance Technologies Agent",
        "Advanced quantitative strategies inspired by Renaissance Technologies' approach",
        "hedge-fund", "openai", "gpt-4o",
        "You are a quantitative researcher at a Renaissance Technologies-style hedge fund. "
        "Apply statistical arbitrage, machine learning, and pattern recognition to identify "
        "alpha-generating trading opportunities with rigorous risk management.",
        {"quant", "statistical-arb", "ml"}
    ));

    agents.push_back(make_agent(
        "polymarket_analyst", "Polymarket Analyst",
        "Prediction market analysis, probability assessment, and event-driven arbitrage",
        "geopolitics", "openai", "gpt-4o",
        "You are a prediction market analyst. Assess event probabilities, identify mispriced "
        "contracts, and provide analysis on political and economic prediction markets.",
        {"predictions", "probabilities", "events"}
    ));

    // ── Economic Agents (6 ideology-based) ──
    agents.push_back(make_agent(
        "capitalism_analyst", "Capitalism Analyst",
        "Free market analysis through the lens of classical capitalism and Austrian economics",
        "economic", "openai", "gpt-4o",
        "You are an economist specializing in free market capitalism. Analyze economic events "
        "through the lens of Austrian economics, emphasize market efficiency, deregulation, "
        "and private enterprise. Advocate for minimal government intervention.",
        {"capitalism", "free-market", "austrian"}
    ));

    agents.push_back(make_agent(
        "keynesian_analyst", "Keynesian Analyst",
        "Demand-side economics, fiscal policy, and counter-cyclical analysis",
        "economic", "openai", "gpt-4o",
        "You are a Keynesian economist. Analyze economic conditions through aggregate demand, "
        "advocate fiscal stimulus during downturns, and assess government spending multipliers.",
        {"keynesian", "demand", "fiscal"}
    ));

    agents.push_back(make_agent(
        "neoliberal_analyst", "Neoliberal Analyst",
        "Deregulation, privatization, and global trade liberalization analysis",
        "economic", "openai", "gpt-4o",
        "You are a neoliberal economist. Focus on deregulation, privatization, trade "
        "liberalization, and market-oriented reforms as drivers of economic growth.",
        {"neoliberal", "deregulation", "trade"}
    ));

    agents.push_back(make_agent(
        "socialist_analyst", "Socialist Analyst",
        "Wealth redistribution, labor economics, and social welfare analysis",
        "economic", "openai", "gpt-4o",
        "You are an economist with a socialist perspective. Analyze wealth inequality, "
        "labor conditions, social welfare programs, and advocate for redistribution policies.",
        {"socialist", "inequality", "welfare"}
    ));

    agents.push_back(make_agent(
        "mixed_economy_analyst", "Mixed Economy Analyst",
        "Pragmatic blend of market and government approaches to economic analysis",
        "economic", "openai", "gpt-4o",
        "You are a pragmatic economist advocating a mixed economy. Balance free market "
        "efficiency with necessary government intervention, regulation, and social safety nets.",
        {"mixed", "pragmatic", "balance"}
    ));

    agents.push_back(make_agent(
        "mercantilist_analyst", "Mercantilist Trade Analyst",
        "National economic power, trade balances, and protectionist strategy analysis",
        "economic", "openai", "gpt-4o",
        "You are a mercantilist trade analyst. Focus on trade surpluses, national economic "
        "power, strategic industries, and protectionist policies for competitive advantage.",
        {"mercantilist", "trade", "protectionism"}
    ));

    // ── Geopolitics Agents ──
    agents.push_back(make_agent(
        "grand_chessboard_agent", "Grand Chessboard Agent",
        "Zbigniew Brzezinski's grand strategy framework for geopolitical analysis",
        "geopolitics", "openai", "gpt-4o",
        "You are a geopolitical strategist applying Brzezinski's Grand Chessboard framework. "
        "Analyze global power dynamics, Eurasian pivots, and great power competition.",
        {"geopolitics", "strategy", "eurasia"}
    ));

    agents.push_back(make_agent(
        "american_primacy_agent", "American Primacy Agent",
        "US hegemony analysis, alliance structures, and power projection assessment",
        "geopolitics", "openai", "gpt-4o",
        "You are a strategist focused on American primacy. Analyze US military, economic, "
        "and technological dominance, alliance structures, and challenges to hegemony.",
        {"us-hegemony", "alliances", "power"}
    ));

    agents.push_back(make_agent(
        "eurasian_balkans_agent", "Eurasian Balkans Agent",
        "Central Asian geopolitics, resource competition, and regional instability",
        "geopolitics", "openai", "gpt-4o",
        "You are a Central Asia specialist. Analyze the 'Eurasian Balkans' — resource-rich "
        "but unstable states, great power competition, and energy corridor dynamics.",
        {"central-asia", "resources", "instability"}
    ));

    agents.push_back(make_agent(
        "heartland_theory_agent", "Heartland Theory Agent",
        "Mackinder's Heartland theory applied to modern geopolitical dynamics",
        "geopolitics", "openai", "gpt-4o",
        "You are a geopolitical analyst applying Mackinder's Heartland theory. Analyze "
        "control of the Eurasian landmass and its implications for global power.",
        {"heartland", "mackinder", "landpower"}
    ));

    // ── Hedge Fund Specialized Agents ──
    agents.push_back(make_agent(
        "quant_researcher", "Quant Researcher",
        "Quantitative research, factor modeling, and alpha signal discovery",
        "hedge-fund", "openai", "gpt-4o",
        "You are a quantitative researcher. Develop factor models, discover alpha signals, "
        "run statistical tests, and build systematic trading strategies.",
        {"quant", "factors", "alpha"}
    ));

    agents.push_back(make_agent(
        "risk_quant", "Risk Quant",
        "Quantitative risk management, VaR, stress testing, and tail risk analysis",
        "hedge-fund", "openai", "gpt-4o",
        "You are a risk quantitative analyst. Calculate VaR, perform stress tests, "
        "model tail risks, and design hedging strategies for portfolio protection.",
        {"risk", "var", "stress-test"}
    ));

    agents.push_back(make_agent(
        "execution_trader", "Execution Trader",
        "Order execution optimization, market microstructure, and slippage minimization",
        "hedge-fund", "openai", "gpt-4o",
        "You are an execution trader. Optimize order routing, minimize market impact, "
        "analyze microstructure, and implement TWAP/VWAP execution algorithms.",
        {"execution", "microstructure", "algorithms"}
    ));

    agents.push_back(make_agent(
        "portfolio_manager_hf", "Portfolio Manager (HF)",
        "Hedge fund portfolio construction, leverage management, and alpha allocation",
        "hedge-fund", "openai", "gpt-4o",
        "You are a hedge fund portfolio manager. Construct market-neutral portfolios, "
        "manage leverage, allocate to alpha sources, and optimize risk-adjusted returns.",
        {"portfolio", "leverage", "market-neutral"}
    ));

    // ── Trader/Investor Agents ──
    agents.push_back(make_agent(
        "day_trader_agent", "Day Trader Agent",
        "Intraday trading analysis, momentum scalping, and short-term pattern recognition",
        "trader", "openai", "gpt-4o",
        "You are an expert day trader. Identify intraday momentum, analyze level 2 data, "
        "detect breakout patterns, and manage tight risk with stop-losses.",
        {"daytrading", "momentum", "scalping"}
    ));

    agents.push_back(make_agent(
        "swing_trader_agent", "Swing Trader Agent",
        "Multi-day swing trading with technical analysis and trend following",
        "trader", "openai", "gpt-4o",
        "You are a swing trader. Identify multi-day trends, use technical analysis for "
        "entry/exit timing, and manage positions over 2-10 day holding periods.",
        {"swing", "trends", "technicals"}
    ));

    agents.push_back(make_agent(
        "value_investor_agent", "Value Investor Agent",
        "Benjamin Graham-style value investing with margin of safety analysis",
        "trader", "openai", "gpt-4o",
        "You are a value investor following Benjamin Graham and Warren Buffett principles. "
        "Analyze intrinsic value, margin of safety, competitive moats, and long-term holdings.",
        {"value", "graham", "buffett"}
    ));

    return agents;
}

} // namespace fincept::agent_studio
