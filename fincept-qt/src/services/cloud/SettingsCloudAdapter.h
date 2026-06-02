#pragma once
#include "services/cloud/CloudDomainAdapter.h"

#include <QObject>
#include <QString>

namespace fincept::services::cloud {

/// Cloud adapter for user settings — mirrors a SAFE SUBSET of local settings to
/// `/v1/settings`. Only cross-device-safe key prefixes sync (appearance/general/
/// notifications/keybindings/voice — see CloudSyncSettings::is_syncable_setting_key);
/// device-local & sensitive keys (cloud.* sync flags, credentials, security, python,
/// storage, llm, mcp, data sources) never leave the device. The setting KEY is the
/// shared identity on both sides, so no sync_map is needed. Singleton.
class SettingsCloudAdapter : public QObject, public CloudDomainAdapter {
    Q_OBJECT
  public:
    static SettingsCloudAdapter& instance();

    QString entity() const override { return QStringLiteral("setting"); }
    void push(const OutboxRow& row, PushDone done) override;
    void pull(PullDone done) override;
    int local_count() const override;
    void remote_count(CountDone done) override;
    void enqueue_all_local() override;

  private:
    SettingsCloudAdapter() = default;
    Q_DISABLE_COPY(SettingsCloudAdapter)

    void push_upsert(const OutboxRow& row, PushDone done);
    void push_delete(const OutboxRow& row, PushDone done);
};

} // namespace fincept::services::cloud
