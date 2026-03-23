#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct Report {
    int id = 0;
    QString title;
    QString content_json;
    QString created_at;
    QString updated_at;
};

struct ReportTemplate {
    QString id;
    QString name;
    QString description;
    QString template_data;
    QString created_at;
    QString updated_at;
};

class ReportRepository : public BaseRepository<Report> {
  public:
    static ReportRepository& instance();

    Result<qint64> create(const QString& title, const QString& content_json);
    Result<Report> get(int id);
    Result<QVector<Report>> list_all();
    Result<void> update(int id, const QString& title, const QString& content_json);
    Result<void> remove(int id);

    // Templates
    Result<QVector<ReportTemplate>> list_templates();
    Result<void> save_template(const ReportTemplate& t);
    Result<void> remove_template(const QString& id);

  private:
    ReportRepository() = default;
    static Report map_report(QSqlQuery& q);
    static ReportTemplate map_template(QSqlQuery& q);
};

} // namespace fincept
