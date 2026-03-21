#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct AgentConfig {
    QString id;
    QString name;
    QString description;
    QString config_json;
    QString category;
    bool    is_default = false;
    bool    is_active  = false;
    QString created_at;
    QString updated_at;
};

class AgentConfigRepository : public BaseRepository<AgentConfig> {
public:
    static AgentConfigRepository& instance();

    Result<QVector<AgentConfig>> list_all();
    Result<QVector<AgentConfig>> list_by_category(const QString& category);
    Result<AgentConfig>          get(const QString& id);
    Result<AgentConfig>          get_active();
    Result<void>                 save(const AgentConfig& c);
    Result<void>                 remove(const QString& id);
    Result<void>                 set_active(const QString& id);

private:
    AgentConfigRepository() = default;
    static AgentConfig map_row(QSqlQuery& q);
};

} // namespace fincept
