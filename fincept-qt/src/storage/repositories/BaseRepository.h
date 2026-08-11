#pragma once
#include "core/logging/Logger.h"
#include "core/result/Result.h"
#include "storage/sqlite/Database.h"

#include <QSqlQuery>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept {

/// Header-only base class for all repositories.
/// Provides common query helpers that wrap Database::instance().
template <typename Entity>
class BaseRepository {
  protected:
    using RowMapper = std::function<Entity(QSqlQuery&)>;

    static Database& db() { return Database::instance(); }

    /// One-line summary of a failing statement, for the error log.
    ///
    /// These helpers used to log only the driver's message — "Parameter count
    /// mismatch" or "No query Unable to fetch row" — with no indication of WHICH
    /// statement failed, across ~16 repositories. That is unactionable: the
    /// driver text alone cannot be traced back to a table, and both of those
    /// strings are actually symptoms of `prepare()` having failed (Qt reports a
    /// failed prepare as a bind/param error at exec time, not at prepare time).
    /// Including the SQL and the bound-parameter count turns a mystery into a
    /// one-line diagnosis.
    ///
    /// Truncated because some statements are long column lists, and the leading
    /// clause is what identifies the table.
    static QString sql_context(const QString& sql, const QVariantList& params) {
        QString flat = sql.simplified();
        constexpr int kMaxSqlChars = 160;
        if (flat.size() > kMaxSqlChars)
            flat = flat.left(kMaxSqlChars) + QStringLiteral("…");
        return QStringLiteral("[params=%1] %2").arg(params.size()).arg(flat);
    }

    /// Execute a SELECT and return all matching rows mapped to a different type.
    template <typename T>
    Result<QVector<T>> query_list_as(const QString& sql, const QVariantList& params,
                                     std::function<T(QSqlQuery&)> mapper) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("query_list_as failed: %1 %2")
                                  .arg(QString::fromStdString(r.error()), sql_context(sql, params)));
            return Result<QVector<T>>::err(r.error());
        }
        QVector<T> result;
        auto& q = r.value();
        while (q.next()) {
            result.append(mapper(q));
        }
        return Result<QVector<T>>::ok(std::move(result));
    }

    /// Execute a SELECT and return all matching rows mapped to entities.
    Result<QVector<Entity>> query_list(const QString& sql, const QVariantList& params, RowMapper mapper) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("query_list failed: %1 %2")
                                  .arg(QString::fromStdString(r.error()), sql_context(sql, params)));
            return Result<QVector<Entity>>::err(r.error());
        }
        QVector<Entity> result;
        auto& q = r.value();
        while (q.next()) {
            result.append(mapper(q));
        }
        return Result<QVector<Entity>>::ok(std::move(result));
    }

    /// Execute a SELECT and return the first row, or error if not found.
    Result<Entity> query_one(const QString& sql, const QVariantList& params, RowMapper mapper) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            return Result<Entity>::err(r.error());
        }
        auto& q = r.value();
        if (!q.next()) {
            return Result<Entity>::err("Not found");
        }
        return Result<Entity>::ok(mapper(q));
    }

    /// Execute a SELECT and return the first row, or std::nullopt if not found.
    std::optional<Entity> query_optional(const QString& sql, const QVariantList& params, RowMapper mapper) const {
        auto r = db().execute(sql, params);
        if (r.is_err())
            return std::nullopt;
        auto& q = r.value();
        if (!q.next())
            return std::nullopt;
        return mapper(q);
    }

    /// Execute an INSERT/UPDATE/DELETE.
    Result<void> exec_write(const QString& sql, const QVariantList& params = {}) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("exec_write failed: %1 %2")
                                  .arg(QString::fromStdString(r.error()), sql_context(sql, params)));
            return Result<void>::err(r.error());
        }
        return Result<void>::ok();
    }

    /// Execute an INSERT and return the last inserted row id.
    Result<qint64> exec_insert(const QString& sql, const QVariantList& params) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("exec_insert failed: %1 %2")
                                  .arg(QString::fromStdString(r.error()), sql_context(sql, params)));
            return Result<qint64>::err(r.error());
        }
        return Result<qint64>::ok(r.value().lastInsertId().toLongLong());
    }

    /// Execute a raw SQL statement (no params, e.g. PRAGMA or DDL).
    Result<void> exec_raw(const QString& sql) const { return db().exec(sql); }
};

} // namespace fincept
