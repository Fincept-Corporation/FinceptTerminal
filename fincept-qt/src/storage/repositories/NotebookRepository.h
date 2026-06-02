#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct Notebook {
    QString id;
    QString name;
    QString description;
    QString cells;    // JSON array string (Jupyter cells)
    QString metadata; // JSON object string
    int execution_counter = 0;
    QString created_at;
    QString updated_at;
};

/// Local store for notebooks (table created in v038). No local editor UI yet —
/// this exists so /v1/notebooks can sync; a future notebook editor will use it.
class NotebookRepository : public BaseRepository<Notebook> {
  public:
    static NotebookRepository& instance();

    Result<QVector<Notebook>> list_all();
    Result<Notebook> get(const QString& id);
    Result<void> save(const Notebook& n); // upsert
    Result<void> remove(const QString& id);

  private:
    NotebookRepository() = default;
    static Notebook map_row(QSqlQuery& q);
};

} // namespace fincept
