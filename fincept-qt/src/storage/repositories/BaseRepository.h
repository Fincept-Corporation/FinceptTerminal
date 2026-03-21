#pragma once
#include "storage/sqlite/Database.h"
#include "core/logging/Logger.h"
#include "core/result/Result.h"
#include <QSqlQuery>
#include <QVariantList>
#include <QString>
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

    /// Execute a SELECT and return all matching rows mapped to entities.
    Result<QVector<Entity>> query_list(const QString& sql,
                                       const QVariantList& params,
                                       RowMapper mapper) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("query_list failed: %1")
                .arg(QString::fromStdString(r.error())));
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
    Result<Entity> query_one(const QString& sql,
                              const QVariantList& params,
                              RowMapper mapper) const {
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
    std::optional<Entity> query_optional(const QString& sql,
                                          const QVariantList& params,
                                          RowMapper mapper) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) return std::nullopt;
        auto& q = r.value();
        if (!q.next()) return std::nullopt;
        return mapper(q);
    }

    /// Execute an INSERT/UPDATE/DELETE.
    Result<void> exec_write(const QString& sql,
                             const QVariantList& params = {}) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("exec_write failed: %1")
                .arg(QString::fromStdString(r.error())));
            return Result<void>::err(r.error());
        }
        return Result<void>::ok();
    }

    /// Execute an INSERT and return the last inserted row id.
    Result<qint64> exec_insert(const QString& sql,
                                const QVariantList& params) const {
        auto r = db().execute(sql, params);
        if (r.is_err()) {
            LOG_ERROR("Repo", QString("exec_insert failed: %1")
                .arg(QString::fromStdString(r.error())));
            return Result<qint64>::err(r.error());
        }
        return Result<qint64>::ok(r.value().lastInsertId().toLongLong());
    }

    /// Execute a raw SQL statement (no params, e.g. PRAGMA or DDL).
    Result<void> exec_raw(const QString& sql) const {
        return db().exec(sql);
    }
};

} // namespace fincept
