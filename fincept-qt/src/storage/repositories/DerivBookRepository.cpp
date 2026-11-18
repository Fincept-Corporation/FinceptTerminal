#include "storage/repositories/DerivBookRepository.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace fincept {

namespace bk = services::options::book;

namespace {

const char* const kDbrPositionCols = "id, book, kind, name, underlying, currency, multiplier, entry_date, legs_json, "
                                     "strategy_id, notes, created_at, updated_at";
const char* const kDbrStrategyCols =
    "id, name, underlying, template_id, currency, multiplier, legs_json, notes, created_at, updated_at";

std::string dbr_error(const QSqlQuery& q, const char* what) {
    return QStringLiteral("%1: %2").arg(QLatin1String(what), q.lastError().text()).toStdString();
}

bool dbr_exec(QSqlQuery& q, const QString& sql, const QVariantList& params) {
    if (!q.prepare(sql))
        return false;
    for (const QVariant& v : params) {
        // A null QString binds as SQL NULL; every text column here is NOT NULL DEFAULT ''.
        if (v.typeId() == QMetaType::QString && v.toString().isNull())
            q.addBindValue(QVariant(QStringLiteral("")));
        else
            q.addBindValue(v);
    }
    return q.exec();
}

bk::BookPosition dbr_position(const QSqlQuery& q) {
    bk::BookPosition p;
    p.id = q.value(0).toString();
    p.book = q.value(1).toString();
    p.kind = q.value(2).toString();
    p.name = q.value(3).toString();
    p.underlying = q.value(4).toString();
    p.currency = q.value(5).toString();
    p.multiplier = q.value(6).toDouble();
    p.entry_date = QDate::fromString(q.value(7).toString(), Qt::ISODate);
    p.legs = bk::legs_from_json(q.value(8).toString());
    p.strategy_id = q.value(9).toString();
    p.notes = q.value(10).toString();
    p.created_at = q.value(11).toLongLong();
    p.updated_at = q.value(12).toLongLong();
    return p;
}

bk::SavedStrategy dbr_strategy(const QSqlQuery& q) {
    bk::SavedStrategy s;
    s.id = q.value(0).toString();
    s.name = q.value(1).toString();
    s.underlying = q.value(2).toString();
    s.template_id = q.value(3).toString();
    s.currency = q.value(4).toString();
    s.multiplier = q.value(5).toDouble();
    s.legs = bk::legs_from_json(q.value(6).toString());
    s.notes = q.value(7).toString();
    s.created_at = q.value(8).toLongLong();
    s.updated_at = q.value(9).toLongLong();
    return s;
}

QString dbr_currency(const QString& c) {
    const QString u = c.trimmed().toUpper();
    return u.isEmpty() ? QStringLiteral("USD") : u;
}

} // namespace

Result<QVector<bk::BookPosition>> DerivBookRepository::positions(const QString& book) const {
    QSqlQuery q(db_);
    const bool all = book.trimmed().isEmpty();
    // Ties on created_at (two saves in one millisecond) keep insertion order: `id` is a random
    // UUID, which made the order of such rows arbitrary.
    const QString sql = QStringLiteral("SELECT %1 FROM deriv_positions%2 ORDER BY created_at, rowid")
                            .arg(QLatin1String(kDbrPositionCols), all ? QString() : QStringLiteral(" WHERE book = ?"));
    if (!dbr_exec(q, sql, all ? QVariantList{} : QVariantList{book.trimmed()}))
        return Result<QVector<bk::BookPosition>>::err(dbr_error(q, "deriv_positions select"));
    QVector<bk::BookPosition> out;
    while (q.next())
        out.append(dbr_position(q));
    return Result<QVector<bk::BookPosition>>::ok(std::move(out));
}

Result<QStringList> DerivBookRepository::books() const {
    QSqlQuery q(db_);
    if (!dbr_exec(q, QStringLiteral("SELECT DISTINCT book FROM deriv_positions ORDER BY book COLLATE NOCASE"), {}))
        return Result<QStringList>::err(dbr_error(q, "deriv_positions books"));
    QStringList out;
    while (q.next())
        out << q.value(0).toString();
    return Result<QStringList>::ok(std::move(out));
}

Result<int> DerivBookRepository::count_positions() const {
    QSqlQuery q(db_);
    if (!dbr_exec(q, QStringLiteral("SELECT COUNT(*) FROM deriv_positions"), {}) || !q.next())
        return Result<int>::err(dbr_error(q, "deriv_positions count"));
    return Result<int>::ok(q.value(0).toInt());
}

Result<bk::BookPosition> DerivBookRepository::save_position(const bk::BookPosition& in) const {
    bk::BookPosition p = in;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    p.book = p.book.trimmed().isEmpty() ? QStringLiteral("Main") : p.book.trimmed();
    p.name = p.name.trimmed();
    p.underlying = p.underlying.trimmed();
    p.currency = dbr_currency(p.currency);
    p.updated_at = now;
    const QString legs = bk::legs_to_json(p.legs);
    const QString entry = p.entry_date.isValid() ? p.entry_date.toString(Qt::ISODate) : QString();
    QSqlQuery q(db_);
    if (p.id.isEmpty()) {
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.created_at = now;
        const QString sql = QStringLiteral("INSERT INTO deriv_positions (%1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                                .arg(QLatin1String(kDbrPositionCols));
        if (!dbr_exec(q, sql,
                      {p.id, p.book, p.kind, p.name, p.underlying, p.currency, p.multiplier, entry, legs, p.strategy_id,
                       p.notes, p.created_at, p.updated_at}))
            return Result<bk::BookPosition>::err(dbr_error(q, "deriv_positions insert"));
        return Result<bk::BookPosition>::ok(std::move(p));
    }
    if (!dbr_exec(q,
                  QStringLiteral("UPDATE deriv_positions SET book = ?, kind = ?, name = ?, underlying = ?, currency = ?, "
                                 "multiplier = ?, entry_date = ?, legs_json = ?, strategy_id = ?, notes = ?, updated_at = ? "
                                 "WHERE id = ?"),
                  {p.book, p.kind, p.name, p.underlying, p.currency, p.multiplier, entry, legs, p.strategy_id, p.notes,
                   p.updated_at, p.id}))
        return Result<bk::BookPosition>::err(dbr_error(q, "deriv_positions update"));
    if (q.numRowsAffected() == 0) {
        // The row was deleted meanwhile: store it again under the same id.
        QSqlQuery ins(db_);
        if (p.created_at <= 0)
            p.created_at = now;
        const QString sql = QStringLiteral("INSERT INTO deriv_positions (%1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                                .arg(QLatin1String(kDbrPositionCols));
        if (!dbr_exec(ins, sql,
                      {p.id, p.book, p.kind, p.name, p.underlying, p.currency, p.multiplier, entry, legs, p.strategy_id,
                       p.notes, p.created_at, p.updated_at}))
            return Result<bk::BookPosition>::err(dbr_error(ins, "deriv_positions reinsert"));
    }
    return Result<bk::BookPosition>::ok(std::move(p));
}

Result<void> DerivBookRepository::remove_position(const QString& id) const {
    QSqlQuery q(db_);
    if (!dbr_exec(q, QStringLiteral("DELETE FROM deriv_positions WHERE id = ?"), {id}))
        return Result<void>::err(dbr_error(q, "deriv_positions delete"));
    return Result<void>::ok();
}

Result<QVector<bk::SavedStrategy>> DerivBookRepository::strategies() const {
    QSqlQuery q(db_);
    if (!dbr_exec(q,
                  QStringLiteral("SELECT %1 FROM deriv_strategies ORDER BY name COLLATE NOCASE")
                      .arg(QLatin1String(kDbrStrategyCols)),
                  {}))
        return Result<QVector<bk::SavedStrategy>>::err(dbr_error(q, "deriv_strategies select"));
    QVector<bk::SavedStrategy> out;
    while (q.next())
        out.append(dbr_strategy(q));
    return Result<QVector<bk::SavedStrategy>>::ok(std::move(out));
}

Result<bk::SavedStrategy> DerivBookRepository::save_strategy(const bk::SavedStrategy& in) const {
    bk::SavedStrategy s = in;
    s.name = s.name.trimmed();
    if (s.name.isEmpty())
        return Result<bk::SavedStrategy>::err("deriv_strategies: empty name");
    s.currency = dbr_currency(s.currency);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    s.updated_at = now;
    const QString legs = bk::legs_to_json(s.legs);
    // Same name (any case): replace that strategy, keeping its id and creation time.
    QSqlQuery find(db_);
    if (!dbr_exec(find, QStringLiteral("SELECT id, created_at FROM deriv_strategies WHERE name = ? COLLATE NOCASE"),
                  {s.name}))
        return Result<bk::SavedStrategy>::err(dbr_error(find, "deriv_strategies find"));
    QSqlQuery q(db_);
    if (find.next()) {
        s.id = find.value(0).toString();
        s.created_at = find.value(1).toLongLong();
        if (!dbr_exec(q,
                      QStringLiteral("UPDATE deriv_strategies SET name = ?, underlying = ?, template_id = ?, currency = ?, "
                                     "multiplier = ?, legs_json = ?, notes = ?, updated_at = ? WHERE id = ?"),
                      {s.name, s.underlying, s.template_id, s.currency, s.multiplier, legs, s.notes, s.updated_at, s.id}))
            return Result<bk::SavedStrategy>::err(dbr_error(q, "deriv_strategies update"));
        return Result<bk::SavedStrategy>::ok(std::move(s));
    }
    s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.created_at = now;
    if (!dbr_exec(q,
                  QStringLiteral("INSERT INTO deriv_strategies (%1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                      .arg(QLatin1String(kDbrStrategyCols)),
                  {s.id, s.name, s.underlying, s.template_id, s.currency, s.multiplier, legs, s.notes, s.created_at,
                   s.updated_at}))
        return Result<bk::SavedStrategy>::err(dbr_error(q, "deriv_strategies insert"));
    return Result<bk::SavedStrategy>::ok(std::move(s));
}

Result<void> DerivBookRepository::remove_strategy(const QString& id) const {
    QSqlQuery q(db_);
    if (!dbr_exec(q, QStringLiteral("DELETE FROM deriv_strategies WHERE id = ?"), {id}))
        return Result<void>::err(dbr_error(q, "deriv_strategies delete"));
    return Result<void>::ok();
}

} // namespace fincept
