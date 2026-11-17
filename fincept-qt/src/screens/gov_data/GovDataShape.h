// src/screens/gov_data/GovDataShape.h
// The pure JSON helpers of the Gov Data panels: request ids, field reading and the shaping of a
// script answer into the compact rows a table shows. QtCore only — no widgets, theme, tr() or
// logging — so they run on the panels' shaping thread (gov::shape_async) and a timing harness can
// link GovDataShape.cpp on its own.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <initializer_list>

namespace fincept::screens::gov {

/// Request id of run `run` of a panel: "<prefix>#<run>".
QString request_id(const QString& prefix, quint64 run);
/// The run of `request_id` when it belongs to `prefix`, else 0.
quint64 run_of(const QString& request_id, const QString& prefix);

/// First line of a script / service error: no traceback, at most 300 characters.
QString plain_error(const QString& raw);

/// A JSON field as text whether the script sent a string or a number ("75056" or 75056 -> "75056";
/// connectors emit numbers for numeric columns). Empty for null / missing / objects.
QString text(const QJsonValue& v);
/// A JSON field as an integer from a number or a numeric string; `fallback` otherwise.
int integer(const QJsonValue& v, int fallback = -1);

/// The rows of a list answer — `data` as an array, or {data: {datasets|results|result: [...]}}.
/// `total` (optional) = metadata.total_count, else data.count, else the row count.
QJsonArray list_rows(const QJsonObject& data, int* total = nullptr);

/// Only what the dataset tables show (title, id, files, modified, tags, organisation,
/// licence, author; resources with `keep_resources`): small payloads to remember.
QJsonArray compact_datasets(const QJsonArray& rows, bool keep_resources = false);
/// Only what the resource tables show (name, format, size, modified, url).
QJsonArray compact_resources(const QJsonArray& rows);

/// Payload of a list view: {"kind", "rows", "total"}.
QJsonObject list_payload(const QString& kind, const QJsonArray& rows, int total = -1);

/// The first of `keys` whose value reads as non-empty text (gov::text), else "".
QString first_string(const QJsonObject& o, std::initializer_list<const char*> keys);

} // namespace fincept::screens::gov
