#pragma once
// InstrumentDownload — blocking HTTP helpers for instrument-master downloads.
//
// Master files are large (5–30 MB) and fetched on the InstrumentService worker
// thread. We use a dedicated QNetworkAccessManager per call (heap-allocated with
// a DeferredDelete drain) rather than the shared BrokerHttp client, so a slow
// multi-second download does not hold BrokerHttp's mutex and starve concurrent
// quote/order calls. Mirrors the pattern in download_angel_master_json().

#include <QByteArray>
#include <QMap>
#include <QString>

namespace fincept::trading {

/// Blocking GET. Returns the raw body, or {} on error/timeout. `timeout_ms`
/// defaults high because masters are large.
QByteArray http_get_blocking(const QString& url, const QMap<QString, QString>& headers = {},
                             int timeout_ms = 60000);

/// Blocking POST with a raw body + content type (used by XTS instrument master
/// and login). Returns the raw body, or {} on error/timeout.
QByteArray http_post_blocking(const QString& url, const QByteArray& body, const QString& content_type,
                              const QMap<QString, QString>& headers = {}, int timeout_ms = 60000);

} // namespace fincept::trading
