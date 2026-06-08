// src/storage/repositories/ScanWatchRepository.h
#pragma once
#include "storage/repositories/BaseRepository.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept {

struct ScanWatch {
    QString     id;
    QString     name;
    QJsonArray  conditions;
    QString     logic = QStringLiteral("AND");
    QStringList symbols;
    QString     universe; // '' / 'CUSTOM' use symbols; 'NSE_EQ'/'BSE_EQ'/'NIFTY50' resolve live
    QString     timeframe = QStringLiteral("1m");
    int         lookback_days = 5;
    QString     data_source = QStringLiteral("Broker");
    QString     broker_id;
    QString     account_id;
    QString     mode = QStringLiteral("poll");
    int         interval_sec = 60;
    int         cooldown_min = 15;
    QJsonObject actions;
    bool        active = true;
    QString     status = QStringLiteral("idle");
    qint64      last_eval_at = 0;
    qint64      last_fired_at = 0;
};

class ScanWatchRepository : public BaseRepository<ScanWatch> {
  public:
    static ScanWatchRepository& instance();

    Result<ScanWatch>          create(const ScanWatch& in);  // generates id if empty
    Result<void>               update(const ScanWatch& w);
    Result<void>               remove(const QString& id);
    Result<void>               set_active(const QString& id, bool active);
    Result<void>               touch_status(const QString& id, const QString& status, qint64 last_eval_at);
    Result<void>               touch_fired(const QString& id, qint64 last_fired_at);
    Result<QVector<ScanWatch>> list_all();
    Result<QVector<ScanWatch>> list_active();
    Result<ScanWatch>          get(const QString& id);

  private:
    ScanWatchRepository() = default;
    static ScanWatch map_row(QSqlQuery& q);
};

} // namespace fincept
