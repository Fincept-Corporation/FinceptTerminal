// src/services/agents/AgentService.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/AgentConfigRepository.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>

namespace fincept::services {

/// Singleton service for all agent operations.
/// Delegates to Python finagent_core/main.py via PythonRunner (lightweight calls)
/// or custom QProcess + stdin (large payloads like agent execution).
/// All results emitted as signals on the Qt event loop.
class AgentService : public QObject {
    Q_OBJECT
  public:
    static AgentService& instance();

    // ── Agent discovery & listing ────────────────────────────────────────────
    void discover_agents();
    void list_tools();
    void list_models();
    void get_system_info();

    // ── Core agent execution ─────────────────────────────────────────────────
    // Returns a request_id that will be echoed in the emitted result signals,
    // so callers can guard on their own request and ignore results from other panels.
    QString run_agent(const QString& query, const QJsonObject& config);
    QString run_agent_streaming(const QString& query, const QJsonObject& config);
    QString run_agent_structured(const QString& query, const QJsonObject& config, const QString& output_model);
    QString route_query(const QString& query);
    void execute_routed_query(const QString& query, const QJsonObject& config = {}, const QString& session_id = {});

    // ── Multi-query & parallel ───────────────────────────────────────────────
    void execute_multi_query(const QString& query, bool aggregate = true);

    // ── Financial workflows ──────────────────────────────────────────────────
    void run_stock_analysis(const QString& symbol, const QJsonObject& config = {});
    void run_portfolio_rebalancing(const QJsonObject& portfolio_data = {});
    void run_risk_assessment(const QJsonObject& portfolio_data = {});
    void run_portfolio_analysis(const QString& analysis_type, const QJsonObject& portfolio_summary = {});

    // ── Team execution ───────────────────────────────────────────────────────
    QString run_team(const QString& query, const QJsonObject& team_config);

    // ── Workflow execution ───────────────────────────────────────────────────
    QString run_workflow(const QString& workflow_type, const QJsonObject& params = {});

    // ── Planner ──────────────────────────────────────────────────────────────
    QString create_plan(const QString& query, const QJsonObject& config = {});
    QString create_stock_analysis_plan(const QString& symbol, const QJsonObject& config = {});
    QString create_portfolio_plan(const QJsonObject& goals = {}, const QJsonObject& config = {});
    QString execute_plan(const QJsonObject& plan, const QJsonObject& config = {});

    // ── Memory & Knowledge ───────────────────────────────────────────────────
    void store_memory(const QString& content, const QString& memory_type = "general", const QJsonObject& metadata = {});
    void recall_memories(const QString& query, const QString& memory_type = {}, int limit = 10);
    void search_knowledge(const QString& query, int limit = 10);
    void save_memory_repo(const QString& content, const QString& agent_id = {}, const QJsonObject& options = {});
    void search_memories_repo(const QString& query, const QString& agent_id = {}, int limit = 10);

    // ── Session management ───────────────────────────────────────────────────
    void save_session(const QJsonObject& session_data);
    void get_session(const QString& session_id);
    void add_session_message(const QString& session_id, const QString& role, const QString& content);

    // ── Paper trading ────────────────────────────────────────────────────────
    void paper_execute_trade(const QString& portfolio_id, const QString& symbol, const QString& action, double quantity,
                             double price);
    void paper_get_portfolio(const QString& portfolio_id);
    void paper_get_positions(const QString& portfolio_id);

    // ── Trade decisions ──────────────────────────────────────────────────────
    void save_trade_decision(const QJsonObject& decision);
    void get_trade_decisions(const QString& competition_id = {}, const QString& model_name = {});

    // ── Config CRUD (delegates to AgentConfigRepository) ─────────────────────
    void load_configs(const QString& category = {});
    void save_config(const AgentConfig& config);
    void delete_config(const QString& id);
    void set_active_config(const QString& id);

    // ── Cache management ─────────────────────────────────────────────────────
    void clear_cache();
    void clear_response_cache();
    int cached_agent_count() const;
    QVector<AgentInfo> cached_agents() const;
    QJsonObject get_cache_stats() const;

  signals:
    void agents_discovered(QVector<AgentInfo> agents, QVector<AgentCategory> categories);
    void agent_result(AgentExecutionResult result);
    void agent_stream_token(const QString& request_id, const QString& token);
    void agent_stream_thinking(const QString& request_id, const QString& status);
    void agent_stream_done(AgentExecutionResult result);
    void routing_result(RoutingResult result);
    void tools_loaded(AgentToolsInfo info);
    void models_loaded(AgentModelsInfo info);
    void system_info_loaded(AgentSystemInfo info);
    void plan_created(ExecutionPlan plan);
    void plan_executed(ExecutionPlan plan);
    void configs_loaded(QVector<AgentConfig> configs);
    void config_saved();
    void config_deleted();
    void memory_stored(bool success, QString message);
    void memories_recalled(QJsonArray memories);
    void knowledge_results(QJsonArray results);
    void session_saved(bool success);
    void session_loaded(QJsonObject session);
    void trade_executed(QJsonObject result);
    void trade_decisions_loaded(QJsonArray decisions);
    void multi_query_result(QJsonObject result);
    void error_occurred(const QString& context, const QString& message);

  private:
    explicit AgentService(QObject* parent = nullptr);
    Q_DISABLE_COPY(AgentService)

    // ── Python execution helpers ─────────────────────────────────────────────
    void run_python_light(const QString& action, const QJsonObject& params,
                          std::function<void(bool, QJsonObject)> on_result);
    void run_python_stdin(const QString& action, const QJsonObject& params, const QJsonObject& config,
                          std::function<void(bool, QJsonObject)> on_result);
    QJsonObject build_api_keys() const;
    QJsonObject build_payload(const QString& action, const QJsonObject& params = {},
                              const QJsonObject& config = {}) const;

    // ── Data cache (agent/tools/models/sysinfo) ──────────────────────────────
    static constexpr int kAgentCacheTtlMs = 5 * 60 * 1000;
    static constexpr int kToolsCacheTtlMs = 10 * 60 * 1000;
    static constexpr int kModelsCacheTtlMs = 10 * 60 * 1000;
    static constexpr int kSysInfoCacheTtlMs = 10 * 60 * 1000;

    struct CachedAgents {
        QVector<AgentInfo> data;
        QVector<AgentCategory> categories;
        qint64 fetched_at = 0;
    } agents_cache_;

    struct CachedTools {
        AgentToolsInfo data;
        qint64 fetched_at = 0;
    } tools_cache_;

    struct CachedModels {
        AgentModelsInfo data;
        qint64 fetched_at = 0;
    } models_cache_;

    struct CachedSysInfo {
        AgentSystemInfo data;
        qint64 fetched_at = 0;
    } sysinfo_cache_;

    // ── LRU response cache ───────────────────────────────────────────────────
    static constexpr int kResponseCacheMaxSize = 100;
    static constexpr int kResponseCacheTtlMs = 2 * 60 * 1000; // 2 min

    struct CachedResponse {
        QJsonObject data;
        qint64 fetched_at = 0;
    };
    QHash<QString, CachedResponse> response_cache_;

    QString make_cache_key(const QString& action, const QJsonObject& params) const;
    bool get_cached_response(const QString& key, QJsonObject& out) const;
    void set_cached_response(const QString& key, const QJsonObject& data);

    mutable QMutex cache_mutex_;
    bool is_cache_fresh(qint64 fetched_at, int ttl_ms) const;
};

} // namespace fincept::services
