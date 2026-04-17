#include "trading/brokers/zerodha/ZerodhaAutoLogin.h"

#include "core/logging/Logger.h"
#include "trading/brokers/zerodha/Totp.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::trading::zerodha {

namespace {

struct HttpResult {
    int status = 0;
    QByteArray body;
    QUrl final_url;
    QString network_error;
};

HttpResult do_request(QNetworkAccessManager& nam, QNetworkRequest req, const QByteArray& body,
                      bool is_post, bool follow_redirects) {
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     follow_redirects ? QNetworkRequest::NoLessSafeRedirectPolicy
                                      : QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = is_post ? nam.post(req, body) : nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    HttpResult r;
    r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.body = reply->readAll();
    r.final_url = reply->url();
    if (reply->error() != QNetworkReply::NoError && r.status == 0)
        r.network_error = reply->errorString();
    reply->deleteLater();
    return r;
}

QString extract_error(const QByteArray& body, const QString& fallback) {
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject())
        return fallback;
    const QString message = doc.object().value("message").toString();
    return message.isEmpty() ? fallback : message;
}

} // namespace

AutoLoginResult run_auto_login(const QString& user_id, const QString& password,
                               const QString& api_key, const QString& api_secret,
                               const QString& totp_secret,
                               const ProgressCallback& progress) {
    AutoLoginResult out;
    out.token.success = false;

    const auto report = [&](const QString& stage) {
        if (progress) progress(stage);
        LOG_INFO("ZerodhaAutoLogin", stage);
    };

    if (user_id.isEmpty() || password.isEmpty() || api_key.isEmpty() ||
        api_secret.isEmpty() || totp_secret.isEmpty()) {
        out.token.error = "Missing one or more required fields";
        return out;
    }

    QNetworkAccessManager nam;
    nam.setCookieJar(new QNetworkCookieJar(&nam));

    QString request_id;

    // Step 1: /api/login
    report("Logging in...");
    {
        QNetworkRequest req(QUrl("https://kite.zerodha.com/api/login"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery form;
        form.addQueryItem("user_id", user_id);
        form.addQueryItem("password", password);
        const auto body = form.toString(QUrl::FullyEncoded).toUtf8();

        HttpResult r = do_request(nam, req, body, /*is_post=*/true, /*follow=*/false);
        if (r.status != 200) {
            out.token.error = extract_error(r.body, r.network_error.isEmpty()
                                                       ? QStringLiteral("Invalid Kite user ID or password")
                                                       : r.network_error);
            return out;
        }
        const auto doc = QJsonDocument::fromJson(r.body);
        request_id = doc.object().value("data").toObject().value("request_id").toString();
        if (request_id.isEmpty()) {
            out.token.error = "Login response missing request_id";
            return out;
        }
    }

    // Step 2: /api/twofa
    report("Submitting 2FA...");
    {
        const QString totp = generate_totp(totp_secret, QDateTime::currentSecsSinceEpoch());
        if (totp.isEmpty()) {
            out.token.error = "TOTP secret is not valid Base32";
            return out;
        }

        QNetworkRequest req(QUrl("https://kite.zerodha.com/api/twofa"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QUrlQuery form;
        form.addQueryItem("user_id", user_id);
        form.addQueryItem("request_id", request_id);
        form.addQueryItem("twofa_value", totp);
        form.addQueryItem("twofa_type", "totp");
        form.addQueryItem("skip_session", "");
        const auto body = form.toString(QUrl::FullyEncoded).toUtf8();

        HttpResult r = do_request(nam, req, body, /*is_post=*/true, /*follow=*/false);
        if (r.status != 200) {
            QString msg = extract_error(r.body, "Invalid TOTP");
            if (msg.contains("TwoFAException", Qt::CaseInsensitive) ||
                msg.contains("TOTP", Qt::CaseInsensitive))
                msg = "Invalid TOTP - check your device clock";
            out.token.error = msg;
            LOG_WARN("ZerodhaAutoLogin",
                     QString("2FA failed (system_time=%1, generated_code_len=%2)")
                         .arg(QDateTime::currentSecsSinceEpoch())
                         .arg(totp.size()));
            return out;
        }
    }

    // Step 3: /connect/finish (capture redirect)
    report("Capturing request_token...");
    QString request_token;
    {
        QUrl url("https://kite.zerodha.com/connect/finish");
        QUrlQuery q;
        q.addQueryItem("api_key", api_key);
        q.addQueryItem("v", "3");
        url.setQuery(q);
        QNetworkRequest req(url);

        HttpResult r = do_request(nam, req, {}, /*is_post=*/false, /*follow=*/true);

        // Parse request_token from final URL query (set during redirect).
        const QUrlQuery final_q(r.final_url);
        request_token = final_q.queryItemValue("request_token");
        if (request_token.isEmpty()) {
            // Fallback: scan response body text.
            const QString body_str = QString::fromUtf8(r.body);
            const int idx = body_str.indexOf("request_token=");
            if (idx >= 0) {
                const int end = body_str.indexOf('&', idx);
                const int start = idx + static_cast<int>(qstrlen("request_token="));
                request_token = body_str.mid(start, (end >= 0 ? end : body_str.size()) - start);
            }
        }
        if (request_token.isEmpty()) {
            out.token.error = "Could not capture request_token from redirect chain";
            return out;
        }
    }

    // Step 4: Exchange for access_token
    report("Exchanging token...");
    {
        const QByteArray checksum =
            QCryptographicHash::hash((api_key + request_token + api_secret).toUtf8(),
                                     QCryptographicHash::Sha256)
                .toHex();

        QNetworkRequest req(QUrl("https://api.kite.trade/session/token"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        req.setRawHeader("X-Kite-Version", "3");

        QUrlQuery form;
        form.addQueryItem("api_key", api_key);
        form.addQueryItem("request_token", request_token);
        form.addQueryItem("checksum", QString::fromLatin1(checksum));
        const auto body = form.toString(QUrl::FullyEncoded).toUtf8();

        HttpResult r = do_request(nam, req, body, /*is_post=*/true, /*follow=*/false);
        if (r.status != 200) {
            QString msg = extract_error(r.body, "Token exchange failed");
            if (msg.contains("TokenException", Qt::CaseInsensitive))
                msg = "Invalid api_secret for this api_key";
            out.token.error = msg;
            return out;
        }

        const auto doc = QJsonDocument::fromJson(r.body);
        const auto data = doc.object().value("data").toObject();
        out.token.success = true;
        out.token.access_token = data.value("access_token").toString();
        out.token.user_id = data.value("user_id").toString();
        QJsonObject extra;
        for (auto it = data.begin(); it != data.end(); ++it) {
            const QString k = it.key();
            if (k == "access_token" || k == "user_id")
                continue;
            extra.insert(k, it.value());
        }
        out.token.additional_data =
            QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
    }

    report(QString("Connected as %1").arg(out.token.user_id));
    return out;
}

} // namespace fincept::trading::zerodha
