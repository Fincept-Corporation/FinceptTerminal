#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct McpServer {
    QString id;
    QString name;
    QString description;
    QString command;
    QString args;
    QString env;
    QString category;
    QString icon;
    bool enabled = true;
    bool auto_start = false;
    QString status; // "stopped", "running", "error"
    QString created_at;
    QString updated_at;
};

class McpServerRepository : public BaseRepository<McpServer> {
  public:
    static McpServerRepository& instance();

    Result<QVector<McpServer>> list_all();
    Result<McpServer> get(const QString& id);
    Result<void> save(const McpServer& s);
    Result<void> remove(const QString& id);
    Result<void> set_status(const QString& id, const QString& status);
    Result<void> set_enabled(const QString& id, bool enabled);

  private:
    McpServerRepository() = default;
    static McpServer map_row(QSqlQuery& q);
};

} // namespace fincept
