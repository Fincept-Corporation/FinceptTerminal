#pragma once
// FinceptDataParse.h — free functions that turn /data API answers into the
// models of FinceptDataModels.h, and classify failures.
//
// No QObject, no network, no widgets: FinceptDataService calls them on replies,
// and a stand-alone test feeds them the backend's captured fixtures.

#include "services/fincept_data/FinceptDataModels.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QVector>

namespace fincept::services::fincept_data {

/// Query parameters of one request, in the order given.
using QueryParams = QVector<QPair<QString, QString>>;

// ── Envelope + errors ────────────────────────────────────────────────────────

/// A 2xx body: `{"success": true, "data": {...}, "meta": {...}}` → data + meta.
/// `{"success": false, "message"}` and anything that is not an envelope → a
/// BadResponse error carrying `status` (the reply's HTTP status) and the message.
DataResult parse_envelope(const QJsonDocument& doc, int status = 200);

/// `meta` → DataMeta (as_of parsed as UTC).
DataMeta parse_meta(const QJsonObject& meta);

/// A failed answer → DataError. `status` 0 = no HTTP status (transport failure).
/// `message` is the server's sentence (HttpClient::message_from_error). The three
/// header values are optional: the server's Retry-After seconds, X-Symbol-Suggestions
/// and X-Data-Error. When they are not available the kind is inferred from the status
/// and the server's wording (data_api/core.py DataError.PUBLIC), suggestions are read
/// from "Did you mean: A, B?", and Retry-After falls back to a per-kind default.
DataError classify_error(int status, const QString& message, int retry_after_s = 0,
                         const QString& suggestions_header = {}, const QString& kind_header = {});

/// "Ticker 'APPL' not found. Did you mean: AAPL, APP?" → {"AAPL", "APP"}.
QStringList suggestions_from_message(const QString& message);

/// A 404 that is the server's "no such route" (FastAPI's "Not Found"), not a
/// data-level "not found" — i.e. this server has no /data API (yet).
bool is_missing_route(int status, const QString& message);

/// Seconds to hold off after a refusal of this kind when the server sent no Retry-After.
int default_retry_after_s(const DataError& e);

/// Request key: path + query sorted by name (and value), percent-encoded — the
/// client cache and in-flight de-duplication key, and the URL that is sent.
QString request_key(const QString& path, const QueryParams& query);

/// Percent-encode one path segment ("BRK.B" stays, "S&P" → "S%26P").
QString path_segment(const QString& value);

// ── Payloads ─────────────────────────────────────────────────────────────────

DatasetCatalog parse_catalog(const QJsonObject& data);
OptionChain parse_option_chain(const QJsonObject& data);
/// Calls and puts paired by strike, ascending.
QVector<StraddleRow> straddle_rows(const QVector<OptionContract>& contracts);
/// Index of the row whose strike is closest to `spot` (-1 when none / no spot).
int at_the_money_row(const QVector<StraddleRow>& rows, double spot);
AnalystOverview parse_analyst(const QJsonObject& data);
OwnershipOverview parse_ownership(const QJsonObject& data);
CalendarPage parse_calendar(CalendarKind kind, const QJsonObject& data);

/// ISO-8601 instant ("2026-11-17T21:00:00.000Z", "2026-11-13T15:59:59") → UTC QDateTime.
QDateTime parse_instant(const QString& text);

// ── Calendar routes (data_api/calendars.py) ──────────────────────────────────

QString calendar_name(CalendarKind kind);       ///< "earnings", "ipos", "splits", "economic"
QString calendar_dataset_id(CalendarKind kind); ///< "calendar.earnings", ...
QString calendar_path(CalendarKind kind);       ///< "/data/calendar/earnings", ...
bool calendar_supports_symbol(CalendarKind kind);
/// Longest start..end span the server accepts (a symbol filter allows a year).
int calendar_max_days(CalendarKind kind, bool with_symbol);

} // namespace fincept::services::fincept_data
