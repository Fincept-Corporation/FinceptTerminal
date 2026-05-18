// src/services/agents/AgentService.h
#pragma once
#include "services/agents/AgentTypes.h"
#include "storage/repositories/AgentConfigRepository.h"

#    include "datahub/Producer.h"

#include <QJsonObject>
#include <QObject>

namespace fincept::services {

/// Singleton service for all agent operations.
/// Delegates to Python finagent_core/main.py via PythonRunner (lightweight calls)
/// or custom QProcess + stdin (large payloads like agent execution).
/// All results emitted as signals on the Qt event loop.
class AgentService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static AgentService& instance();

    // ── Producer interface ──────────────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;
    void ensure_registered_with_hub();

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
    void execute_multi_query(const QString& query, bool aggregate = true, const QJsonObject& config = {});

    // ── Financial workflows ──────────────────────────────────────────────────
    void run_stock_analysis(const QString& symbol, const QJsonObject& config = {});
    void run_portfolio_rebalancing(const QJsonObject& portfolio_data = {});
    void run_risk_assessment(const QJsonObject& portfolio_data = {});
    void run_portfolio_analysis(const QString& analysis_type, const QJsonObject& portfolio_summary = {});

    // ── Team execution ───────────────────────────────────────────────────────
    QString run_team(const QString& query, const QJsonObject& team_config);

    // ── Workflow execution ───────────────────────────────────────────────────
    QString run_workflow(const QString& workflow_type, const QJsonObject& params = {});

    // ── Agentic Mode (gated by `agentic_mode_enabled` setting) ───────────────
    // Spawns a streaming Python subprocess that runs an AgenticRunner against
    // the existing TaskStateManager / ResumableTaskRunner. Each per-step event
    // is emitted on the `task_event` signal AND published on DataHub topic
    // `task:event:<task_id>`. Crash-resume works because state is persisted
    // in the existing agent_tasks SQLite table.
    //
    // For start_task: returns the C++ request_id; the durable Python task_id
    //   arrives in the first `task_started` event payload.
    // For resume/pause/cancel/get/list: returns the request_id correlator;
    //   the task_id is the input.
    QString start_task(const QString& query, const QJsonObject& config);
    QString resume_task(const QString& task_id);
    QString pause_task(const QString& task_id);
    QString cancel_task(const QString& task_id);
    QString list_tasks(const QString& status_filter = {}, int limit = 50);
    QString get_task(const QString& task_id);
    /// HITL: user replies to a clarifying question persisted on the task row.
    /// Returns the streaming request_id; events arrive on task:event:<task_id>.
    QString reply_to_question(const QString& task_id, const QString& answer);

    // ── Scheduled recurring tasks (Phase 3D) ─────────────────────────────────
    /// Register a recurring agentic task. `schedule_expr` uses the small DSL
    /// in scheduler.py: "every 30m", "hourly", "daily 09:30", "weekday 16:00".
    /// `start_now=true` fires once on the next tick instead of waiting for the
    /// first cadence boundary.
    QString schedule_create_task(const QString& name, const QString& query,
                                 const QString& schedule_expr,
                                 const QJsonObject& config = {},
                                 bool start_now = false);
    QString schedule_list();
    QString schedule_delete(const QString& schedule_id);
    QString schedule_set_enabled(const QString& schedule_id, bool enabled);

    // ── Phase 3 library inspection (read-only UI surface) ───────────────────
    QString skills_list();
    QString skill_delete(const QString& skill_id);
    QString archival_list(const QString& user_id = {}, const QString& type = {});
    QString archival_delete(const QString& memory_id);
    QString reflexion_list();

    // ── Planner ──────────────────────────────────────────────────────────────
    QString create_plan(const QString& query, const QJsonObject& config = {});
    QString create_stock_analysis_plan(const QString& symbol, const QJsonObject& config = {});
    QString create_portfolio_plan(const QJsonObject& goals = {}, const QJsonObject& config = {});
    QString execute_plan(const QJsonObject& plan, const QJsonObject& config = {});

    // ── Memory & Knowledge ───────────────────────────────────────────────────
    void store_memory(const QString& content, const QString& memory_type = "general",
                      const QJsonObject& metadata = {}, const QString& agent_id = {});
    void recall_memories(const QString& query, const QString& memory_type = {}, int limit = 10,
                         const QString& agent_id = {});
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

    // ── Agentic Mode signals ─────────────────────────────────────────────────
    /// Per-step event from a running agentic task. `event` includes kind
    /// ∈ {task_started, task_resumed, plan_ready, step_start, step_end,
    /// paused, cancelled, done, error} plus kind-specific fields.
    void task_event(const QString& task_id, const QJsonObject& event);
    /// Result of list_tasks(): full agent_tasks rows as JSON.
    void tasks_listed(const QJsonArray& tasks);
    /// Result of get_task(): single agent_tasks row as JSON.
    void task_loaded(const QJsonObject& task);
    /// Scheduled-task management signals.
    void schedules_listed(const QJsonArray& schedules);
    void schedule_created(const QString& schedule_id);
    void schedule_deleted(const QString& schedule_id);
    /// Emitted when a scheduled task auto-fires; carries the new agentic task_id.
    void scheduled_task_fired(const QString& schedule_id, const QString& schedule_name,
                              const QString& task_id);
    /// Phase 3 library inspection signals.
    void skills_listed(const QJsonArray& skills);
    void archival_listed(const QJsonArray& memories);
    void reflexion_listed(const QJsonArray& reflections);

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

    // ── Cache TTLs — delegated to CacheManager ───────────────────────────────
    static constexpr int kAgentCacheTtlSec = 5 * 60;
    static constexpr int kToolsCacheTtlSec = 10 * 60;
    static constexpr int kModelsCacheTtlSec = 10 * 60;
    static constexpr int kSysInfoCacheTtlSec = 10 * 60;
    static constexpr int kResponseCacheTtlSec = 2 * 60;

    QString make_cache_key(const QString& action, const QJsonObject& params) const;
    bool get_cached_response(const QString& key, QJsonObject& out) const;
    void set_cached_response(const QString& key, const QJsonObject& data);

    // ── Hub publishing helpers ──────────────────────────────────────────────
    // AgentService is a push-only producer — it publishes run outputs, stream
    // tokens, status, routing, and errors but never pulls on `refresh()`.
    // `agent:output:<run_id>` is retired at run completion to avoid unbounded
    // hub memory growth from disposable per-run topics.
    void publish_agent_result(const AgentExecutionResult& r, bool final);
    void publish_agent_token(const QString& run_id, const QString& token);
    void publish_agent_status(const QString& run_id, const QString& status);
    void publish_routing_result(const RoutingResult& r);
    void publish_agent_error(const QString& context, const QString& message);
    /// Publishes a parsed AGENTIC_EVENT to task:event:<task_id> and emits task_event().
    /// Retires the topic when the event is terminal (done/error/cancelled).
    void publish_task_event(const QString& task_id, const QJsonObject& event);
    /// Streaming runner for the `agentic_start_task` / `agentic_resume_task`
    /// Python actions. Parses AGENTIC_EVENT: <json> lines and routes them
    /// through publish_task_event(). Returns the request_id correlator.
    QString run_agentic_streaming(const QString& action, const QJsonObject& params,
                                  const QJsonObject& config, const QString& known_task_id = {});
    /// One tick of the scheduler — pulls due schedules from Python and fires
    /// each as a fresh agentic task. Called on a QTimer (30s) once any
    /// schedule exists.
    void tick_schedules();
    void ensure_schedule_timer();
    QObject* schedule_timer_ = nullptr;
    bool hub_registered_ = false;
};

} // namespace fincept::services
