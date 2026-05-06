#pragma once
// StrategiesRepository — saved F&O strategies (Builder "Saved Strategies"
// tab). One row per Strategy; legs serialised as a JSON array.

#include "services/options/OptionChainTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept {

/// Lightweight DB row — the in-memory `services::options::Strategy` plus
/// the row id assigned by SQLite at insert time.
struct SavedStrategyRow {
    qint64 id = 0;
    fincept::services::options::Strategy strategy;
};

class StrategiesRepository : public BaseRepository<SavedStrategyRow> {
  public:
    static StrategiesRepository& instance();

    /// Insert a new row; returns the generated id. Stamps created_at /
    /// modified_at to "now" on insert.
    Result<qint64> save(const fincept::services::options::Strategy& s);

    /// Update row `id` in place. Bumps modified_at.
    Result<void> update(qint64 id, const fincept::services::options::Strategy& s);

    Result<SavedStrategyRow> get(qint64 id);
    Result<QVector<SavedStrategyRow>> list_all();
    Result<QVector<SavedStrategyRow>> list_by_underlying(const QString& underlying);
    Result<void> remove(qint64 id);

  private:
    StrategiesRepository() = default;
    static SavedStrategyRow map_row(QSqlQuery& q);

    /// JSON encode/decode helpers — `legs_json` carries the QVector<StrategyLeg>.
    static QString legs_to_json(const QVector<fincept::services::options::StrategyLeg>& legs);
    static QVector<fincept::services::options::StrategyLeg> legs_from_json(const QString& json);
};

} // namespace fincept
