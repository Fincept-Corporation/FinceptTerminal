// Workflow Templates — Pre-built workflow configurations

#include "workflow_templates.h"
#include "node_registry.h"
#include <algorithm>
#include <cctype>

namespace fincept::node_editor {

std::vector<WorkflowTemplate> WorkflowTemplates::templates_;
bool WorkflowTemplates::initialized_ = false;

// Helper: create a node instance from a type with given id, position, and param overrides
static NodeInstance make_node(int id, const std::string& type, ImVec2 pos,
                               const std::map<std::string, ParamValue>& param_overrides = {}) {
    const auto* def = NodeRegistry::instance().get_node(type);
    NodeInstance node;
    node.id = id;
    node.type = type;
    node.label = def ? def->label : type;
    node.position = pos;

    if (def) {
        // Create pins
        int pin_id = id * 100 + 1;
        for (int i = 0; i < def->num_inputs; i++) {
            Pin pin;
            pin.id = pin_id++;
            pin.conn_type = def->input_type(i);
            pin.name = (i < (int)def->input_defs.size()) ? def->input_defs[i].name : "in_" + std::to_string(i);
            pin.color = connection_type_color(pin.conn_type);
            node.inputs.push_back(pin);
        }
        for (int i = 0; i < def->num_outputs; i++) {
            Pin pin;
            pin.id = pin_id++;
            pin.conn_type = def->output_type(i);
            pin.name = (i < (int)def->output_defs.size()) ? def->output_defs[i].name : "out_" + std::to_string(i);
            pin.color = connection_type_color(pin.conn_type);
            node.outputs.push_back(pin);
        }

        // Set default params then override
        for (auto& p : def->default_params) {
            node.params[p.name] = p.default_value;
        }
    }

    for (auto& [k, v] : param_overrides) {
        node.params[k] = v;
    }

    return node;
}

static Link make_link(int id, int start_pin, int end_pin) {
    Link l;
    l.id = id;
    l.start_pin = start_pin;
    l.end_pin = end_pin;
    return l;
}

void WorkflowTemplates::init() {
    if (initialized_) return;

    // Ensure registry is initialized
    if (!NodeRegistry::instance().is_initialized()) {
        NodeRegistry::instance().init();
    }

    // =========================================================================
    // BEGINNER TEMPLATES
    // =========================================================================

    // 1. Hello World — Manual Trigger → Results Display
    {
        WorkflowTemplate t;
        t.id = "hello-world";
        t.name = "Hello World";
        t.description = "Simple trigger to display. Learn the basics of connecting nodes.";
        t.category = "Beginner";
        t.tags = {"starter", "learning", "basic"};

        auto n1 = make_node(1, "manual-trigger", {100, 200});
        auto n2 = make_node(2, "results-display", {400, 200}, {{"title", std::string("Hello World")}});
        t.nodes = {n1, n2};

        // Connect trigger output -> display input
        t.links = {make_link(1, 102, 201)};  // n1.out_0 -> n2.in_0
        t.node_count = 2;
        templates_.push_back(std::move(t));
    }

    // 2. Stock Quote Fetch
    {
        WorkflowTemplate t;
        t.id = "stock-quote";
        t.name = "Stock Quote Fetch";
        t.description = "Fetch real-time stock data and display it.";
        t.category = "Beginner";
        t.tags = {"stocks", "data", "quote"};

        auto n1 = make_node(1, "data-source", {100, 200}, {{"symbol", std::string("AAPL")}});
        auto n2 = make_node(2, "results-display", {450, 200}, {{"format", std::string("table")}});
        t.nodes = {n1, n2};
        t.links = {make_link(1, 102, 201)};
        t.node_count = 2;
        templates_.push_back(std::move(t));
    }

    // 3. Simple Data Transform
    {
        WorkflowTemplate t;
        t.id = "data-transform";
        t.name = "Simple Data Transform";
        t.description = "Fetch data, filter it, and display results.";
        t.category = "Beginner";
        t.tags = {"transform", "filter", "data"};

        auto n1 = make_node(1, "data-source", {100, 200}, {{"symbol", std::string("MSFT")}});
        auto n2 = make_node(2, "data-filter", {400, 200}, {{"filter_type", std::string("dropna")}});
        auto n3 = make_node(3, "results-display", {700, 200});
        t.nodes = {n1, n2, n3};
        t.links = {make_link(1, 102, 201), make_link(2, 202, 301)};
        t.node_count = 3;
        templates_.push_back(std::move(t));
    }

    // =========================================================================
    // INTERMEDIATE TEMPLATES
    // =========================================================================

    // 4. Technical Analysis Pipeline
    {
        WorkflowTemplate t;
        t.id = "tech-analysis";
        t.name = "Technical Analysis Pipeline";
        t.description = "Fetch data → Compute SMA & RSI → Display results.";
        t.category = "Intermediate";
        t.tags = {"technical", "indicators", "SMA", "RSI"};

        auto n1 = make_node(1, "data-source", {50, 200}, {{"symbol", std::string("AAPL")}, {"timeframe", std::string("1d")}});
        auto n2 = make_node(2, "technical-indicator", {350, 100}, {{"indicator", std::string("SMA")}, {"period", 20}});
        auto n3 = make_node(3, "technical-indicator", {350, 300}, {{"indicator", std::string("RSI")}, {"period", 14}});
        n3.label = "RSI Indicator";
        auto n4 = make_node(4, "data-merger", {650, 200});
        auto n5 = make_node(5, "results-display", {950, 200}, {{"format", std::string("chart")}});
        t.nodes = {n1, n2, n3, n4, n5};
        t.links = {
            make_link(1, 102, 201),  // data -> SMA
            make_link(2, 102, 301),  // data -> RSI
            make_link(3, 202, 401),  // SMA -> merger in_0
            make_link(4, 302, 402),  // RSI -> merger in_1
            make_link(5, 402, 501),  // merger -> display
        };
        t.node_count = 5;
        templates_.push_back(std::move(t));
    }

    // 5. Backtest Strategy
    {
        WorkflowTemplate t;
        t.id = "backtest-strategy";
        t.name = "Backtest Strategy";
        t.description = "Data → Indicator → Signal → Backtest → Results.";
        t.category = "Intermediate";
        t.tags = {"backtest", "strategy", "signals"};

        auto n1 = make_node(1, "data-source", {50, 200}, {{"symbol", std::string("SPY")}});
        auto n2 = make_node(2, "technical-indicator", {300, 200}, {{"indicator", std::string("EMA")}, {"period", 21}});
        auto n3 = make_node(3, "signal-generator", {550, 200}, {{"strategy", std::string("crossover")}});
        auto n4 = make_node(4, "backtest", {800, 200}, {{"initial_capital", 100000.0}});
        auto n5 = make_node(5, "results-display", {1050, 200}, {{"format", std::string("table")}});
        t.nodes = {n1, n2, n3, n4, n5};
        t.links = {
            make_link(1, 102, 201), make_link(2, 202, 301),
            make_link(3, 302, 401), make_link(4, 402, 501),
        };
        t.node_count = 5;
        templates_.push_back(std::move(t));
    }

    // 6. AI Analysis
    {
        WorkflowTemplate t;
        t.id = "ai-analysis";
        t.name = "AI Analysis";
        t.description = "Fetch data → AI agent analysis → Display insights.";
        t.category = "Intermediate";
        t.tags = {"AI", "LLM", "analysis"};

        auto n1 = make_node(1, "data-source", {100, 200}, {{"symbol", std::string("NVDA")}});
        auto n2 = make_node(2, "agent-mediator", {400, 200}, {{"prompt", std::string("Analyze this stock data and provide buy/sell recommendation")}});
        auto n3 = make_node(3, "results-display", {700, 200});
        t.nodes = {n1, n2, n3};
        t.links = {make_link(1, 102, 201), make_link(2, 202, 301)};
        t.node_count = 3;
        templates_.push_back(std::move(t));
    }

    // 7. News Sentiment
    {
        WorkflowTemplate t;
        t.id = "news-sentiment";
        t.name = "News Sentiment Analysis";
        t.description = "Fetch news → Analyze sentiment → Display results.";
        t.category = "Intermediate";
        t.tags = {"news", "sentiment", "NLP"};

        auto n1 = make_node(1, "news-feed", {100, 200}, {{"query", std::string("AAPL")}});
        auto n2 = make_node(2, "sentiment-analyzer", {400, 200});
        auto n3 = make_node(3, "results-display", {700, 200});
        t.nodes = {n1, n2, n3};
        t.links = {make_link(1, 102, 201), make_link(2, 202, 301)};
        t.node_count = 3;
        templates_.push_back(std::move(t));
    }

    // =========================================================================
    // ADVANCED TEMPLATES
    // =========================================================================

    // 8. Portfolio Risk System
    {
        WorkflowTemplate t;
        t.id = "portfolio-risk";
        t.name = "Portfolio Risk System";
        t.description = "Multi-asset data → Portfolio allocation → Risk metrics → Monte Carlo → Report.";
        t.category = "Advanced";
        t.tags = {"portfolio", "risk", "VaR", "Monte Carlo"};

        auto n1 = make_node(1, "data-source", {50, 100}, {{"symbol", std::string("AAPL")}});
        auto n2 = make_node(2, "data-source", {50, 300}, {{"symbol", std::string("MSFT")}});
        n2.label = "Data Source (MSFT)";
        auto n3 = make_node(3, "data-merger", {300, 200});
        auto n4 = make_node(4, "portfolio-allocator", {550, 200}, {{"method", std::string("mean_variance")}});
        auto n5 = make_node(5, "risk-metrics", {800, 100}, {{"metric", std::string("VaR")}});
        auto n6 = make_node(6, "monte-carlo", {800, 300}, {{"simulations", 10000}});
        auto n7 = make_node(7, "results-display", {1100, 200});
        t.nodes = {n1, n2, n3, n4, n5, n6, n7};
        t.links = {
            make_link(1, 102, 301), make_link(2, 202, 302),
            make_link(3, 302, 401), make_link(4, 402, 501),
            make_link(5, 402, 601), make_link(6, 502, 701),
        };
        t.node_count = 7;
        templates_.push_back(std::move(t));
    }

    // 9. Multi-Agent Trading
    {
        WorkflowTemplate t;
        t.id = "multi-agent-trading";
        t.name = "Multi-Agent Trading";
        t.description = "Data → Multiple AI agents → Consensus → Safety check → Paper trade.";
        t.category = "Advanced";
        t.tags = {"multi-agent", "AI", "trading", "consensus"};

        auto n1 = make_node(1, "data-source", {50, 200}, {{"symbol", std::string("TSLA")}});
        auto n2 = make_node(2, "technical-indicator", {300, 200}, {{"indicator", std::string("MACD")}});
        auto n3 = make_node(3, "multi-agent", {550, 200});
        auto n4 = make_node(4, "risk-check", {800, 200}, {{"max_loss_pct", 2.0}});
        auto n5 = make_node(5, "order-executor", {1050, 200}, {{"mode", std::string("paper")}});
        auto n6 = make_node(6, "results-display", {1300, 200});
        t.nodes = {n1, n2, n3, n4, n5, n6};
        t.links = {
            make_link(1, 102, 201), make_link(2, 202, 301),
            make_link(3, 302, 401), make_link(4, 402, 501),
            make_link(5, 502, 601),
        };
        t.node_count = 6;
        templates_.push_back(std::move(t));
    }

    // 10. Full Analysis Pipeline
    {
        WorkflowTemplate t;
        t.id = "full-pipeline";
        t.name = "Full Analysis Pipeline";
        t.description = "Comprehensive: Data → Indicators → AI → Backtest → Risk → Optimize → Export.";
        t.category = "Advanced";
        t.tags = {"complete", "pipeline", "analysis", "backtest", "optimization"};

        auto n1 = make_node(1, "data-source", {50, 250}, {{"symbol", std::string("AAPL")}});
        auto n2 = make_node(2, "technical-indicator", {300, 150}, {{"indicator", std::string("SMA")}, {"period", 20}});
        auto n3 = make_node(3, "technical-indicator", {300, 350}, {{"indicator", std::string("RSI")}, {"period", 14}});
        n3.label = "RSI";
        auto n4 = make_node(4, "agent-mediator", {550, 150});
        auto n5 = make_node(5, "signal-generator", {550, 350}, {{"strategy", std::string("crossover")}});
        auto n6 = make_node(6, "backtest", {800, 250}, {{"initial_capital", 100000.0}});
        auto n7 = make_node(7, "risk-metrics", {1050, 150}, {{"metric", std::string("sharpe")}});
        auto n8 = make_node(8, "optimization", {1050, 350}, {{"method", std::string("bayesian")}});
        auto n9 = make_node(9, "results-display", {1300, 150});
        auto n10 = make_node(10, "csv-export", {1300, 350}, {{"filename", std::string("analysis.csv")}});
        t.nodes = {n1, n2, n3, n4, n5, n6, n7, n8, n9, n10};
        t.links = {
            make_link(1, 102, 201), make_link(2, 102, 301),
            make_link(3, 202, 401), make_link(4, 302, 501),
            make_link(5, 402, 601), make_link(6, 502, 601),
            make_link(7, 602, 701), make_link(8, 602, 801),
            make_link(9, 702, 901), make_link(10, 802, 1001),
        };
        t.node_count = 10;
        templates_.push_back(std::move(t));
    }

    initialized_ = true;
}

const std::vector<WorkflowTemplate>& WorkflowTemplates::get_all() {
    init();
    return templates_;
}

const WorkflowTemplate* WorkflowTemplates::find(const std::string& id) {
    init();
    for (auto& t : templates_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

std::vector<const WorkflowTemplate*> WorkflowTemplates::get_by_category(const std::string& category) {
    init();
    std::vector<const WorkflowTemplate*> result;
    for (auto& t : templates_) {
        if (t.category == category) result.push_back(&t);
    }
    return result;
}

std::vector<const WorkflowTemplate*> WorkflowTemplates::search(const std::string& query) {
    init();
    std::string q;
    for (char c : query) q += (char)std::tolower((unsigned char)c);

    std::vector<const WorkflowTemplate*> result;
    for (auto& t : templates_) {
        std::string name_lower;
        for (char c : t.name) name_lower += (char)std::tolower((unsigned char)c);
        std::string desc_lower;
        for (char c : t.description) desc_lower += (char)std::tolower((unsigned char)c);

        if (name_lower.find(q) != std::string::npos || desc_lower.find(q) != std::string::npos) {
            result.push_back(&t);
            continue;
        }
        for (auto& tag : t.tags) {
            std::string tag_lower;
            for (char c : tag) tag_lower += (char)std::tolower((unsigned char)c);
            if (tag_lower.find(q) != std::string::npos) {
                result.push_back(&t);
                break;
            }
        }
    }
    return result;
}

} // namespace fincept::node_editor
