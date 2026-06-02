#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for AI agent configs — mirrors local `agent_configs` to
/// `/v1/agent-configs`. Local id is a client UUID ↔ `agt_`. `config_json` is a
/// string locally but a JSON object on the wire. Activation uses the dedicated
/// `/activate` endpoint (server enforces one active per user). Singleton.
class AgentConfigCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static AgentConfigCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("agent_config"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    AgentConfigCloudAdapter() = default;
    Q_DISABLE_COPY(AgentConfigCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
    void push_activate(const OutboxRow& row, PushDone done);
    void apply_remote_agent(const QJsonObject& o);
    void reconcile_deletes(const QSet<QString>& remote_ids);
};

} // namespace fincept::services::cloud
