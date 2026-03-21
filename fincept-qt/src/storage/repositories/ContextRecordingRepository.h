#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct RecordedContext {
    QString id;
    QString tab_name;
    QString data_type;
    QString label;
    QString raw_data;
    QString metadata;   // JSON
    int     data_size = 0;
    QString tags;
    QString created_at;
};

class ContextRecordingRepository : public BaseRepository<RecordedContext> {
public:
    static ContextRecordingRepository& instance();

    Result<QVector<RecordedContext>> list_all(int limit = 100);
    Result<QVector<RecordedContext>> list_by_tab(const QString& tab_name, int limit = 50);
    Result<RecordedContext>          get(const QString& id);
    Result<void>                     save(const RecordedContext& ctx);
    Result<void>                     remove(const QString& id);
    Result<void>                     clear_all();

private:
    ContextRecordingRepository() = default;
    static RecordedContext map_row(QSqlQuery& q);
};

} // namespace fincept
