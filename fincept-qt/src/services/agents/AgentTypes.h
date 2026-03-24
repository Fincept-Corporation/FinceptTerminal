// src/services/agents/AgentTypes.h
#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services {

// ── View modes for the Agent Config screen ──────────────────────────────────

enum class AgentViewMode { Agents, Create, Teams, Workflows, Planner, Tools, Chat, System };

// ── Agent discovery ─────────────────────────────────────────────────────────

struct AgentInfo {
    QString id;
    QString name;
    QString description;
    QString category;
    QStringList capabilities;
    QString provider;
    QString version;
    QJsonObject config; // full config payload
};

struct AgentCategory {
    QString name;
    int count = 0;
};

// ── Execution results ───────────────────────────────────────────────────────

struct AgentExecutionResult {
    bool success = false;
    QString response;
    QString error;
    int execution_time_ms = 0;
};

// ── Routing ─────────────────────────────────────────────────────────────────

struct RoutingResult {
    bool success = false;
    QString agent_id;
    QString intent;
    double confidence = 0.0;
    QStringList matched_keywords;
    QJsonObject config;
};

// ── System info ─────────────────────────────────────────────────────────────

struct AgentSystemInfo {
    QString version;
    QString framework;
    QJsonObject capabilities; // providers, tools, vectordbs, embedders, output_models
    QStringList features;
};

// ── Tools ───────────────────────────────────────────────────────────────────

struct AgentToolsInfo {
    QJsonObject tools; // category -> [tool names]
    QStringList categories;
    int total_count = 0;
};

// ── Models ──────────────────────────────────────────────────────────────────

struct AgentModelsInfo {
    QStringList providers;
    int count = 0;
};

// ── Team configuration ──────────────────────────────────────────────────────

struct TeamMember {
    QString agent_id;
    QString name;
    QString role;
    QJsonObject model_config;
    QStringList tools;
    QString instructions;
};

struct TeamConfig {
    QString name;
    QString mode; // "coordinate", "route", "collaborate"
    QVector<TeamMember> members;
    QJsonObject coordinator_model;
    bool show_member_responses = false;
    int leader_index = 0;
};

// ── Execution plan ──────────────────────────────────────────────────────────

struct PlanStep {
    QString id;
    QString name;
    QString step_type;
    QJsonObject config;
    QStringList dependencies;
    QString status; // "pending", "running", "completed", "failed"
    QString result;
    QString error;
};

struct ExecutionPlan {
    QString id;
    QString name;
    QString description;
    QVector<PlanStep> steps;
    QString status;
    bool is_complete = false;
    bool has_failed = false;
};

} // namespace fincept::services
