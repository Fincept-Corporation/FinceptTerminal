#include "storage/repositories/FiiDiiRepository.h"

#include "core/logging/Logger.h"

#include <QSqlError>

namespace fincept {

using fincept::services::options::FiiDiiDay;

FiiDiiRepository& FiiDiiRepository::instance() {
    static FiiDiiRepository s;
    return s;
}

FiiDiiDay FiiDiiRepository::map_row(QSqlQuery& q) {
    FiiDiiDay d;
    d.date_iso = q.value(0).toString();
    d.fii_buy = q.value(1).toDouble();
    d.fii_sell = q.value(2).toDouble();
    d.fii_net = q.value(3).toDouble();
    d.dii_buy = q.value(4).toDouble();
    d.dii_sell = q.value(5).toDouble();
    d.dii_net = q.value(6).toDouble();
    return d;
}

Result<void> FiiDiiRepository::upsert(const FiiDiiDay& d) {
    return exec_write(
        "INSERT OR REPLACE INTO fii_dii_daily "
        "(date_iso, fii_buy, fii_sell, fii_net, dii_buy, dii_sell, dii_net, fetched_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, datetime('now'))",
        {d.date_iso, d.fii_buy, d.fii_sell, d.fii_net, d.dii_buy, d.dii_sell, d.dii_net});
}

Result<void> FiiDiiRepository::upsert_batch(const QVector<FiiDiiDay>& days) {
    if (days.isEmpty())
        return Result<void>::ok();
    auto begin = db().begin_transaction();
    const bool in_tx = begin.is_ok();
    for (const auto& d : days) {
        auto r = upsert(d);
        if (r.is_err()) {
            if (in_tx)
                db().rollback();
            return r;
        }
    }
    if (in_tx) {
        auto c = db().commit();
        if (c.is_err())
            return c;
    }
    return Result<void>::ok();
}

Result<QVector<FiiDiiDay>> FiiDiiRepository::get_recent(int limit) {
    return query_list(
        "SELECT date_iso, fii_buy, fii_sell, fii_net, dii_buy, dii_sell, dii_net "
        "FROM fii_dii_daily ORDER BY date_iso DESC LIMIT ?",
        {limit}, &FiiDiiRepository::map_row);
}

Result<QVector<FiiDiiDay>> FiiDiiRepository::get_window(const QString& since_iso,
                                                         const QString& until_iso) {
    if (until_iso.isEmpty()) {
        return query_list(
            "SELECT date_iso, fii_buy, fii_sell, fii_net, dii_buy, dii_sell, dii_net "
            "FROM fii_dii_daily WHERE date_iso >= ? ORDER BY date_iso ASC",
            {since_iso}, &FiiDiiRepository::map_row);
    }
    return query_list(
        "SELECT date_iso, fii_buy, fii_sell, fii_net, dii_buy, dii_sell, dii_net "
        "FROM fii_dii_daily WHERE date_iso >= ? AND date_iso <= ? ORDER BY date_iso ASC",
        {since_iso, until_iso}, &FiiDiiRepository::map_row);
}

} // namespace fincept
