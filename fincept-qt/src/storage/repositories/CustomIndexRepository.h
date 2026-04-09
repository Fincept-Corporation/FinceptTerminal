// src/storage/repositories/CustomIndexRepository.h
#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct CustomIndexConstituent {
    QString symbol;
    double weight = 0.0; // 0–100 percentage
    double price_at_create = 0.0;
};

struct CustomIndex {
    QString id;
    QString name;
    QString method;
    double base_value = 1000.0;
    QString portfolio_id;
    QVector<CustomIndexConstituent> constituents;
    QString created_at;
    QString updated_at;
};

struct CustomIndexValue {
    qint64 id = 0;
    QString index_id;
    QString date; // YYYY-MM-DD
    double value = 0.0;
};

class CustomIndexRepository : public BaseRepository<CustomIndex> {
  public:
    static CustomIndexRepository& instance();

    // ── Index CRUD ────────────────────────────────────────────────────────────
    Result<QString> create(const CustomIndex& idx);
    Result<QVector<CustomIndex>> list_all();
    Result<CustomIndex> get(const QString& id);
    Result<void> remove(const QString& id);

    // ── Index values ──────────────────────────────────────────────────────────
    Result<void> save_value(const QString& index_id, const QString& date, double value);
    Result<QVector<CustomIndexValue>> get_values(const QString& index_id, int limit = 365);

    // ── Convenience: latest computed value for an index ───────────────────────
    double latest_value(const QString& index_id);

  private:
    CustomIndexRepository() = default;
    static CustomIndex map_index(QSqlQuery& q);
    static CustomIndexValue map_value(QSqlQuery& q);

    static QString constituents_to_json(const QVector<CustomIndexConstituent>& cs);
    static QVector<CustomIndexConstituent> json_to_constituents(const QString& json);
};

} // namespace fincept
