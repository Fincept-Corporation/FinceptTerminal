// RssFeedProbeService.cpp — see the header.
#include "services/news/RssFeedProbeService.h"

#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrent>

#include <algorithm>

namespace fincept::services {

namespace rss_probe_detail {

constexpr int kProbeTimeoutMs = 8000;
constexpr qint64 kProbeMaxBytes = 8LL * 1024 * 1024;
constexpr qint64 kStaleFeedSecs = 7LL * 86400;
constexpr const char* kProbeUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

QString tr_probe(const char* text) {
    return QCoreApplication::translate("RssFeedProbeService", text);
}

qint64 parse_feed_date(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return 0;
    QDateTime dt = QDateTime::fromString(s, Qt::RFC2822Date); // RSS pubDate
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODateWithMs); // Atom updated / dc:date
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    return dt.isValid() ? dt.toSecsSinceEpoch() : 0;
}

/// Sniff the payload: root element decides RSS / Atom / RDF / HTML; then count
/// items, pick up the channel title and the newest item date. Pure; worker-safe.
RssProbeResult classify(QByteArray body) {
    RssProbeResult r;
    r.bytes = body.size();
    if (body.startsWith("\xEF\xBB\xBF")) // UTF-8 BOM used to fail the old "<?xml" prefix test
        body.remove(0, 3);

    QXmlStreamReader xml(body);
    bool in_item = false;
    bool have_title = false;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token != QXmlStreamReader::StartElement) {
            if (token == QXmlStreamReader::EndElement &&
                (xml.name() == QLatin1String("item") || xml.name() == QLatin1String("entry")))
                in_item = false;
            continue;
        }
        const auto name = xml.name();
        if (r.root_element.isEmpty()) {
            r.root_element = xml.qualifiedName().toString();
            if (name.compare(QLatin1String("rss"), Qt::CaseInsensitive) == 0) {
                const QString version = xml.attributes().value(QLatin1String("version")).toString();
                r.format = version.isEmpty() ? QStringLiteral("RSS") : QStringLiteral("RSS ") + version;
            } else if (name == QLatin1String("feed")) {
                r.format = QStringLiteral("Atom");
            } else if (name == QLatin1String("RDF")) {
                r.format = QStringLiteral("RSS 1.0 (RDF)");
            } else {
                break; // not a feed root (html, error XML, ...)
            }
            continue;
        }
        if (name == QLatin1String("item") || name == QLatin1String("entry")) {
            ++r.item_count;
            in_item = true;
        } else if (name == QLatin1String("title") && !in_item && !have_title) {
            r.title = xml.readElementText(QXmlStreamReader::SkipChildElements).simplified();
            have_title = true;
        } else if (in_item && (name == QLatin1String("pubDate") || name == QLatin1String("updated") ||
                               name == QLatin1String("published") || name == QLatin1String("date"))) {
            r.newest_item_secs =
                std::max(r.newest_item_secs, parse_feed_date(xml.readElementText(QXmlStreamReader::SkipChildElements)));
        }
    }

    if (!r.format.isEmpty()) {
        // A truncated / slightly malformed feed that still yielded items is usable.
        r.verdict = (!xml.hasError() || r.item_count > 0) ? RssProbeResult::Verdict::Feed
                                                         : RssProbeResult::Verdict::NotFeed;
        if (xml.hasError() && r.item_count == 0)
            r.error = xml.errorString();
        return r;
    }
    const QByteArray head = body.left(2048).toLower();
    r.verdict = (head.contains("<html") || head.contains("<!doctype html")) ? RssProbeResult::Verdict::Html
                                                                            : RssProbeResult::Verdict::NotFeed;
    return r;
}

QString age_text(qint64 secs) {
    if (secs < 3600)
        return QCoreApplication::translate("RssFeedProbeService", "%n min ago", "", int(std::max<qint64>(secs, 0) / 60));
    if (secs < 2 * 86400)
        return QCoreApplication::translate("RssFeedProbeService", "%n h ago", "", int(secs / 3600));
    return QCoreApplication::translate("RssFeedProbeService", "%n day(s) ago", "", int(secs / 86400));
}

} // namespace rss_probe_detail

RssFeedProbeService& RssFeedProbeService::instance() {
    static RssFeedProbeService s;
    return s;
}

RssFeedProbeService::RssFeedProbeService() : nam_(new QNetworkAccessManager(this)) {}

void RssFeedProbeService::probe(const QString& url, QObject* context, ProbeCallback cb) {
    using namespace rss_probe_detail;
    QPointer<QObject> ctx = context;
    const QUrl qurl(url.trimmed());
    const QString scheme = qurl.scheme().toLower();
    if (!qurl.isValid() || qurl.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        RssProbeResult r;
        r.error = tr_probe("Only http:// and https:// feed URLs can be checked.");
        QMetaObject::invokeMethod(
            this, [ctx, cb, r]() {
                if (ctx && cb)
                    cb(r);
            },
            Qt::QueuedConnection);
        return;
    }

    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kProbeUserAgent));
    req.setRawHeader("Accept", "application/rss+xml, application/atom+xml, application/xml, text/xml, */*");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(kProbeTimeoutMs);

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > kProbeMaxBytes && !reply->property("too_large").toBool()) {
            reply->setProperty("too_large", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, ctx, cb]() {
        reply->deleteLater();
        if (!ctx)
            return;
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString final_url = reply->url().toString();
        if (reply->error() != QNetworkReply::NoError) {
            RssProbeResult r;
            r.http_status = http;
            r.final_url = final_url;
            if (reply->property("too_large").toBool())
                r.error = tr_probe("Response exceeds 8 MB — not a normal feed.");
            else if (reply->error() == QNetworkReply::OperationCanceledError)
                r.error = tr_probe("No response within 8 s.");
            else
                r.error = reply->errorString();
            if (cb)
                cb(r);
            return;
        }
        QByteArray body = reply->readAll();
        QPointer<RssFeedProbeService> self = this;
        (void)QtConcurrent::run([self, ctx, cb, body = std::move(body), http, final_url]() mutable {
            RssProbeResult r = classify(std::move(body));
            r.http_status = http;
            r.final_url = final_url;
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [ctx, cb, r]() {
                    if (ctx && cb)
                        cb(r);
                },
                Qt::QueuedConnection);
        });
    });
}

QString RssFeedProbeService::describe(const RssProbeResult& r) {
    using namespace rss_probe_detail;
    switch (r.verdict) {
        case RssProbeResult::Verdict::NetworkError:
            return r.http_status > 0 ? tr_probe("✗ HTTP %1 — %2").arg(r.http_status).arg(r.error)
                                     : tr_probe("✗ %1").arg(r.error);
        case RssProbeResult::Verdict::Html:
            return tr_probe("✗ The server returned an HTML page (login, consent or block page), not a feed.");
        case RssProbeResult::Verdict::NotFeed:
            return r.root_element.isEmpty()
                       ? tr_probe("✗ Not RSS/Atom XML (%1 bytes).").arg(r.bytes)
                       : tr_probe("✗ Not an RSS/Atom feed (root element <%1>).").arg(r.root_element);
        case RssProbeResult::Verdict::Feed:
            break;
    }
    QStringList parts;
    parts << r.format;
    if (!r.title.isEmpty())
        parts << r.title.left(60);
    parts << QCoreApplication::translate("RssFeedProbeService", "%n item(s)", "", r.item_count);
    QString stale;
    if (r.newest_item_secs > 0) {
        const qint64 age = QDateTime::currentSecsSinceEpoch() - r.newest_item_secs;
        parts << tr_probe("newest %1").arg(age_text(age));
        if (age > kStaleFeedSecs)
            stale = tr_probe(" — newest item is over a week old; the feed may be stale");
    }
    parts << QLocale(QLocale::English).formattedDataSize(r.bytes, 0);
    const bool empty = r.item_count == 0;
    return (empty || !stale.isEmpty() ? QStringLiteral("⚠ ") : QStringLiteral("✓ ")) + parts.join(QStringLiteral(" · ")) +
           stale + (empty ? tr_probe(" — the feed has no items") : QString());
}

} // namespace fincept::services
