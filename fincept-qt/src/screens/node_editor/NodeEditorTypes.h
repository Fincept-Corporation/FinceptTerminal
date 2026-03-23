#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace fincept::workflow {

// ── Enums ──────────────────────────────────────────────────────────────

enum class PortDirection { Input, Output };

enum class ConnectionType {
    Main,
    AiLanguageModel,
    AiMemory,
    AiTool,
    MarketData,
    PortfolioData,
    PriceData,
    SignalData,
    RiskData,
    BacktestData,
    TechnicalData,
    FundamentalData,
    NewsData,
    EconomicData,
    OptionsData,
};

enum class WorkflowStatus { Draft, Idle, Running, Completed, Error };

// ── Port definition (on a node type) ───────────────────────────────────

struct PortDef {
    QString id;
    QString label;
    PortDirection direction = PortDirection::Input;
    ConnectionType connection_type = ConnectionType::Main;
    bool required = false;
    bool multi = false;
};

// ── Parameter definition (describes a properties-panel field) ──────────

struct ParamDef {
    QString key;
    QString label;
    QString type;           // "string", "number", "boolean", "select", "code", "json", "expression"
    QJsonValue default_value;
    QStringList options;    // for "select" type
    QString placeholder;
    bool required = false;
};

// ── Node type definition (registered in NodeRegistry) ──────────────────

struct NodeTypeDef {
    QString type_id;        // e.g. "trigger.manual"
    QString display_name;   // e.g. "Manual Trigger"
    QString category;       // e.g. "Triggers"
    QString description;
    QString icon_text;      // 1-2 char icon, e.g. ">>"
    QString accent_color;   // category color for header
    int version = 1;
    QVector<PortDef> inputs;
    QVector<PortDef> outputs;
    QVector<ParamDef> parameters;

    /// Async executor: receives params + inputs, calls callback with result.
    using ExecuteFn = std::function<void(
        const QJsonObject& params,
        const QVector<QJsonValue>& inputs,
        std::function<void(bool success, QJsonValue output, QString error)> callback
    )>;
    ExecuteFn execute;
};

// ── Node instance (placed on canvas) ───────────────────────────────────

struct NodeDef {
    QString id;
    QString type;           // registry key
    QString name;           // user label
    int type_version = 1;
    double x = 0.0;
    double y = 0.0;
    QJsonObject parameters;
    QJsonObject credentials;
    bool disabled = false;
    bool continue_on_fail = false;
    bool retry_on_fail = false;
    int max_tries = 1;
};

// ── Edge instance (connection between two ports) ───────────────────────

struct EdgeDef {
    QString id;
    QString source_node;
    QString target_node;
    QString source_port;
    QString target_port;
    bool animated = false;
};

// ── Workflow definition ────────────────────────────────────────────────

struct WorkflowDef {
    QString id;
    QString name = "Untitled Workflow";
    QString description;
    QVector<NodeDef> nodes;
    QVector<EdgeDef> edges;
    WorkflowStatus status = WorkflowStatus::Draft;
    QJsonObject static_data;
    QString created_at;
    QString updated_at;
};

// ── Execution results ──────────────────────────────────────────────────

struct NodeExecutionResult {
    QString node_id;
    bool success = false;
    QJsonValue output;
    QString error;
    int duration_ms = 0;
};

struct WorkflowExecutionResult {
    QString workflow_id;
    bool success = false;
    QVector<NodeExecutionResult> node_results;
    int total_duration_ms = 0;
    QString error;
};

} // namespace fincept::workflow
