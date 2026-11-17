// RequestSigning.cpp — see RequestSigning.h.

#include "services/data_sources/RequestSigning.h"

#include <QCryptographicHash>
#include <QLocale>
#include <QMap>
#include <QMessageAuthenticationCode>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace fincept::services::datasources {

namespace {

QByteArray rs_hmac(const QByteArray& key, const QByteArray& data) {
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

QByteArray rs_sha256_hex(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

void rs_set_header(HeaderList& headers, const QByteArray& name, const QByteArray& value) {
    for (auto& h : headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0) {
            h.second = value;
            return;
        }
    }
    headers.emplaceBack(name, value);
}

QByteArray rs_host(const QUrl& url) {
    QByteArray host = url.host(QUrl::FullyEncoded).toUtf8();
    const int port = url.port(-1);
    const bool default_port = port < 0 || (url.scheme() == QLatin1String("https") && port == 443) ||
                              (url.scheme() == QLatin1String("http") && port == 80);
    if (!default_port)
        host += ':' + QByteArray::number(port);
    return host;
}

} // namespace

QByteArray header_value(const HeaderList& headers, const QByteArray& name) {
    for (const auto& h : headers)
        if (h.first.compare(name, Qt::CaseInsensitive) == 0)
            return h.second;
    return QByteArray();
}

QByteArray aws_uri_encode(const QByteArray& utf8, bool keep_slash) {
    static const char kHex[] = "0123456789ABCDEF";
    QByteArray out;
    out.reserve(utf8.size() * 3);
    for (const char c : utf8) {
        const unsigned char u = static_cast<unsigned char>(c);
        const bool unreserved = (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u == '-' ||
                                u == '_' || u == '.' || u == '~';
        if (unreserved || (keep_slash && u == '/')) {
            out.append(c);
        } else {
            out.append('%');
            out.append(kHex[u >> 4]);
            out.append(kHex[u & 0x0F]);
        }
    }
    return out;
}

void sign_aws_v4(const QByteArray& method, const QUrl& url, HeaderList& headers, const QByteArray& body,
                 const QString& access_key, const QString& secret_key, const QString& region, const QString& service,
                 const QDateTime& now_utc) {
    const QDateTime t = now_utc.toUTC();
    const QByteArray amz_date = QLocale::c().toString(t, QStringLiteral("yyyyMMdd'T'HHmmss'Z'")).toLatin1();
    const QByteArray date_stamp = amz_date.left(8);
    const QByteArray payload_hash = rs_sha256_hex(body);
    rs_set_header(headers, "x-amz-date", amz_date);
    rs_set_header(headers, "x-amz-content-sha256", payload_hash);

    // Canonical URI: the decoded path, encoded once ('/' kept). Empty path → "/".
    QByteArray path = url.path(QUrl::FullyDecoded).toUtf8();
    if (path.isEmpty())
        path = "/";
    const QByteArray canonical_uri = aws_uri_encode(path, true);

    // Canonical query: every key=value encoded, sorted by key then value.
    QList<QPair<QByteArray, QByteArray>> q;
    for (const auto& item : QUrlQuery(url).queryItems(QUrl::FullyDecoded))
        q.append({aws_uri_encode(item.first.toUtf8(), false), aws_uri_encode(item.second.toUtf8(), false)});
    std::sort(q.begin(), q.end());
    QByteArray canonical_query;
    for (int i = 0; i < q.size(); ++i) {
        if (i)
            canonical_query += '&';
        canonical_query += q[i].first + '=' + q[i].second;
    }

    // Canonical headers: host + every header present, lower-case, trimmed, sorted.
    QMap<QByteArray, QByteArray> canon;
    canon.insert("host", rs_host(url));
    for (const auto& h : std::as_const(headers)) {
        const QByteArray name = h.first.toLower().trimmed();
        if (name == "authorization")
            continue;
        canon.insert(name, h.second.simplified());
    }
    QByteArray canonical_headers;
    QByteArray signed_headers;
    for (auto it = canon.constBegin(); it != canon.constEnd(); ++it) {
        canonical_headers += it.key() + ':' + it.value() + '\n';
        if (!signed_headers.isEmpty())
            signed_headers += ';';
        signed_headers += it.key();
    }

    const QByteArray canonical_request = method.toUpper() + '\n' + canonical_uri + '\n' + canonical_query + '\n' +
                                         canonical_headers + '\n' + signed_headers + '\n' + payload_hash;
    const QByteArray scope = date_stamp + '/' + region.toUtf8() + '/' + service.toUtf8() + "/aws4_request";
    const QByteArray string_to_sign =
        "AWS4-HMAC-SHA256\n" + amz_date + '\n' + scope + '\n' + rs_sha256_hex(canonical_request);

    const QByteArray k_date = rs_hmac("AWS4" + secret_key.toUtf8(), date_stamp);
    const QByteArray k_region = rs_hmac(k_date, region.toUtf8());
    const QByteArray k_service = rs_hmac(k_region, service.toUtf8());
    const QByteArray k_signing = rs_hmac(k_service, "aws4_request");
    const QByteArray signature = rs_hmac(k_signing, string_to_sign).toHex();

    rs_set_header(headers, "Authorization",
                  "AWS4-HMAC-SHA256 Credential=" + access_key.toUtf8() + '/' + scope + ", SignedHeaders=" +
                      signed_headers + ", Signature=" + signature);
}

void sign_azure_shared_key(const QByteArray& method, const QUrl& url, HeaderList& headers, const QByteArray& body,
                           const QString& account, const QString& base64_key, const QDateTime& now_utc) {
    const QByteArray date =
        QLocale::c().toString(now_utc.toUTC(), QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'")).toLatin1();
    rs_set_header(headers, "x-ms-date", date);
    if (header_value(headers, "x-ms-version").isEmpty())
        rs_set_header(headers, "x-ms-version", "2021-08-06");

    QMap<QByteArray, QByteArray> ms;
    for (const auto& h : std::as_const(headers)) {
        const QByteArray name = h.first.toLower().trimmed();
        if (name.startsWith("x-ms-"))
            ms.insert(name, h.second.trimmed());
    }
    QByteArray canonical_headers;
    for (auto it = ms.constBegin(); it != ms.constEnd(); ++it)
        canonical_headers += it.key() + ':' + it.value() + '\n';

    QByteArray resource = '/' + account.toUtf8() + url.path(QUrl::FullyEncoded).toUtf8();
    if (url.path().isEmpty())
        resource += '/';
    QMap<QByteArray, QList<QByteArray>> params;
    for (const auto& item : QUrlQuery(url).queryItems(QUrl::FullyDecoded))
        params[item.first.toLower().toUtf8()].append(item.second.toUtf8());
    for (auto it = params.begin(); it != params.end(); ++it) {
        QList<QByteArray> values = it.value();
        std::sort(values.begin(), values.end());
        resource += '\n' + it.key() + ':' + values.join(',');
    }

    const QByteArray content_length = body.isEmpty() ? QByteArray() : QByteArray::number(body.size());
    const QByteArray string_to_sign = method.toUpper() + '\n' +            // VERB
                                      header_value(headers, "Content-Encoding") + '\n' +
                                      header_value(headers, "Content-Language") + '\n' + content_length + '\n' +
                                      header_value(headers, "Content-MD5") + '\n' +
                                      header_value(headers, "Content-Type") + '\n' +
                                      QByteArray() + '\n' +                  // Date (x-ms-date is used)
                                      header_value(headers, "If-Modified-Since") + '\n' +
                                      header_value(headers, "If-Match") + '\n' +
                                      header_value(headers, "If-None-Match") + '\n' +
                                      header_value(headers, "If-Unmodified-Since") + '\n' +
                                      header_value(headers, "Range") + '\n' + canonical_headers + resource;
    const QByteArray key = QByteArray::fromBase64(base64_key.toLatin1());
    const QByteArray signature = rs_hmac(key, string_to_sign).toBase64();
    rs_set_header(headers, "Authorization", "SharedKey " + account.toUtf8() + ':' + signature);
}

} // namespace fincept::services::datasources
