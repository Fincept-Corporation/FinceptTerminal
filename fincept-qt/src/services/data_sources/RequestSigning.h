#pragma once
// RequestSigning — request authentication schemes the object stores need, computed in C++ with
// QtCore's HMAC (no SDK): AWS Signature Version 4 (S3, MinIO, DigitalOcean Spaces, Wasabi,
// Cloudflare R2, Oracle's S3-compatible API, Backblaze B2's S3 endpoint) and Azure Storage
// Shared Key. Pure functions; unit-tested against the providers' published examples.

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

namespace fincept::services::datasources {

using HeaderList = QList<QPair<QByteArray, QByteArray>>;

/// RFC 3986 percent-encoding as SigV4 wants it (unreserved kept; '/' kept when `keep_slash`).
QByteArray aws_uri_encode(const QByteArray& utf8, bool keep_slash);

/// Adds x-amz-date, x-amz-content-sha256 and Authorization to `headers` for a request to `url`.
/// Every header already in `headers` (plus host) is signed.
void sign_aws_v4(const QByteArray& method, const QUrl& url, HeaderList& headers, const QByteArray& body,
                 const QString& access_key, const QString& secret_key, const QString& region, const QString& service,
                 const QDateTime& now_utc);

/// Adds x-ms-date, x-ms-version and Authorization: SharedKey to `headers` (Azure Blob Storage).
void sign_azure_shared_key(const QByteArray& method, const QUrl& url, HeaderList& headers, const QByteArray& body,
                           const QString& account, const QString& base64_key, const QDateTime& now_utc);

/// The value of a header (case-insensitive name), empty when absent.
QByteArray header_value(const HeaderList& headers, const QByteArray& name);

} // namespace fincept::services::datasources
