// LocalAgentEngine.cpp — the app's ILocalAgentEngine (LocalAgentPort.h): the
// local agentic engine as AgentService exposes it, plus LocalAgenticExecutor's
// default constructor, which creates it. Uses src/services/agents read-only.
//
// The LLM is the user's own: AgentService::build_payload hands the Python agent
// the active LLM configuration (Settings › LLM Config) when the task config names
// no model — and resume_task / reply_to_question always re-resolve it that way —
// so readiness() requires that configuration to exist.

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "services/agent_tasks/LocalAgentPort.h"
#include "services/agent_tasks/LocalAgenticExecutor.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QObject>

#include <memory>

namespace fincept::agent_tasks {

namespace local_agent_engine_detail {

QString lag_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::LocalAgenticExecutor", text);
}

class LagAgentServiceEngine final : public QObject, public ILocalAgentEngine {
  public:
    ~LagAgentServiceEngine() override = default;

    void set_listener(ILocalAgentListener* listener) override { listener_ = listener; }

    LocalAgentReadiness readiness() const override {
        LocalAgentReadiness r;
        if (!services::AgentService::agentic_mode_enabled()) {
            r.ready = false;
            r.code = QStringLiteral("agentic_mode_disabled");
            r.message = lag_tr("Agentic Mode is switched off, so the local agent cannot take tasks. Turn it on in "
                               "Settings › Developer, or run this task on the cloud engine.");
            return r;
        }
        // A system Python alone makes PythonRunner "available", but the agent runs in
        // the terminal's own environment: that one has to be set up.
        if (!python::PythonRunner::instance().is_available() ||
            !python::PythonSetupManager::instance().is_core_ready()) {
            r.ready = false;
            r.code = QStringLiteral("python_not_ready");
            r.message = lag_tr("The local agent needs the terminal's Python environment, which is not set up yet. "
                               "Finish the setup in Settings › Python Env (SET UP PYTHON), or run this task on the "
                               "cloud engine.");
            return r;
        }
        auto& llm = ai_chat::LlmService::instance();
        if (!llm.is_configured()) {
            r.ready = false;
            r.code = QStringLiteral("llm_not_configured");
            r.message = lag_tr("No LLM is configured for the local agent. Add a provider and its API key in "
                               "Settings › LLM Config, or run this task on the cloud engine.");
            return r;
        }
        const QString provider = llm.active_provider();
        const QString model = llm.active_model();
        r.llm_label = model.isEmpty() ? provider : QStringLiteral("%1 · %2").arg(provider, model);
        return r;
    }

    QString start_task(const QString& query, const QJsonObject& config) override {
        ensure_connected();
        return services::AgentService::instance().start_task(query, config);
    }

    QString resume_task(const QString& task_id, const QJsonObject& config) override {
        ensure_connected();
        return services::AgentService::instance().resume_task(task_id, config);
    }

    QString reply_to_question(const QString& task_id, const QString& answer, const QJsonObject& config) override {
        ensure_connected();
        return services::AgentService::instance().reply_to_question(task_id, answer, config);
    }

    void pause_task(const QString& task_id) override {
        ensure_connected();
        services::AgentService::instance().pause_task(task_id);
    }

    void cancel_task(const QString& task_id) override {
        ensure_connected();
        services::AgentService::instance().cancel_task(task_id);
    }

    const TaskRun* find_run(const QString& run_id) const override {
        return TaskRunService::instance().find_run(run_id);
    }

    QVector<RunStep> run_steps(const QString& run_id) const override { return TaskRunService::instance().steps(run_id); }

    void log_info(const QString& message) override { LOG_INFO("LocalAgentRun", message); }

  private:
    /// AgentService is subscribed to on first use (not at app start-up).
    void ensure_connected() {
        if (connected_)
            return;
        connected_ = true;
        connect(&services::AgentService::instance(), &services::AgentService::task_event, this,
                [this](const QString& task_id, const QJsonObject& event) {
                    if (!listener_)
                        return;
                    // A missing Python library (the agent needs the optional full set) is
                    // reported as what to install, not as the interpreter's ImportError.
                    if (event.value(QStringLiteral("kind")).toString() == QLatin1String("error")) {
                        const QString raw = event.value(QStringLiteral("error")).toString();
                        if (!python::PythonSetupManager::missing_module_name(raw).isEmpty()) {
                            const QString explained = python::PythonSetupManager::instance().explain_missing_module(
                                raw, QStringLiteral("local agent"));
                            if (explained != raw) {
                                QJsonObject copy = event;
                                copy.insert(QStringLiteral("error"), explained);
                                copy.insert(QStringLiteral("explained"), true);
                                listener_->local_agent_event(task_id, copy);
                                return;
                            }
                        }
                    }
                    listener_->local_agent_event(task_id, event);
                });
    }

    ILocalAgentListener* listener_ = nullptr;
    bool connected_ = false;
};

} // namespace local_agent_engine_detail

// ── LocalAgenticExecutor's default constructor (the app's) ───────────────────

LocalAgenticExecutor::LocalAgenticExecutor(QObject* parent)
    : LocalAgenticExecutor(std::make_unique<local_agent_engine_detail::LagAgentServiceEngine>(), parent) {}

} // namespace fincept::agent_tasks
