#include "services/options/FiiDiiService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "storage/repositories/FiiDiiRepository.h"

#include <QDate>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QVariant>
#include <QVector>

#include <algorithm>

namespace fincept::services::options {

namespace {

const QString kTopic = QStringLiteral("fno:fii_dii:daily");
const QString kUrl = QStringLiteral("https://blissquants.com/Bliss_Fii_Dii_Data");
constexpr int kPublishWindowDays = 30;
constexpr int kTtlMs = 60 * 60 * 1000;          // 1 h
constexpr int kMinIntervalMs = 30 * 60 * 1000;  // 30 min
constexpr int kTimeoutMs = 15000;

// Parse "22 May 2026" → "2026-05-22"
QString parse_date(const QString& raw) {
    QString s = raw.trimmed();
    s.remove(QRegularExpression("<[^>]*>"));
    s = s.trimmed();
    QDate d = QLocale(QLocale::English).toDate(s, "d MMM yyyy");
    if (!d.isValid())
        d = QLocale(QLocale::English).toDate(s, "dd MMM yyyy");
    return d.isValid() ? d.toString(Qt::ISODate) : QString();
}

// Strip HTML tags and parse a number like "-4,440.47" → -4440.47
double parse_amount(const QString& raw) {
    QString s = raw.trimmed();
    s.remove(QRegularExpression("<[^>]*>"));
    s.remove(',');
    s = s.trimmed();
    bool ok = false;
    double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

// Extract FiiDiiDay rows from the HTML page.
QVector<FiiDiiDay> parse_html(const QString& html) {
    QVector<FiiDiiDay> rows;

    // Find all <tr> blocks
    static const QRegularExpression rx_tr("<tr[^>]*>(.*?)</tr>",
                                           QRegularExpression::DotMatchesEverythingOption
                                           | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rx_td("<td[^>]*>(.*?)</td>",
                                           QRegularExpression::DotMatchesEverythingOption
                                           | QRegularExpression::CaseInsensitiveOption);

    auto tr_it = rx_tr.globalMatch(html);
    while (tr_it.hasNext()) {
        auto tr_match = tr_it.next();
        const QString tr_content = tr_match.captured(1);

        QStringList cells;
        auto td_it = rx_td.globalMatch(tr_content);
        while (td_it.hasNext())
            cells.append(td_it.next().captured(1));

        // Expect: Date | FII Net | DII Net (3 columns minimum)
        if (cells.size() < 3)
            continue;

        QString date_iso = parse_date(cells[0]);
        if (date_iso.isEmpty())
            continue;

        FiiDiiDay d;
        d.date_iso = date_iso;
        d.fii_net = parse_amount(cells[1]);
        d.dii_net = parse_amount(cells[2]);
        rows.append(d);
    }
    return rows;
}

}  // namespace

FiiDiiService& FiiDiiService::instance() {
    static FiiDiiService s;
    return s;
}

FiiDiiService::FiiDiiService() = default;

void FiiDiiService::ensure_registered_with_hub() {
    if (registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy pol;
    pol.ttl_ms = kTtlMs;
    pol.min_interval_ms = kMinIntervalMs;
    hub.set_policy(kTopic, pol);

    registered_ = true;
    LOG_INFO("FiiDii", "Registered with DataHub (fno:fii_dii:daily)");
}

QStringList FiiDiiService::topic_patterns() const {
    return {kTopic};
}

void FiiDiiService::refresh(const QStringList& topics) {
    Q_UNUSED(topics);
    if (in_flight_) {
        LOG_DEBUG("FiiDii", "refresh skipped — already in flight");
        return;
    }
    kick_off_refresh();
}

void FiiDiiService::kick_off_refresh() {
    in_flight_ = true;
    LOG_INFO("FiiDii", "Fetching FII/DII data from blissquants.com");

    auto* nam = new QNetworkAccessManager(this);
    QUrl url(kUrl);
    QNetworkRequest req(url);
    req.setRawHeader(QByteArray("User-Agent"), QByteArray("Mozilla/5.0 (Windows NT 10.0; Win64; x64)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QVariant::fromValue(QNetworkRequest::NoLessSafeRedirectPolicy));
    req.setTransferTimeout(kTimeoutMs);

    QNetworkReply* reply = nam->get(req);
    QPointer<FiiDiiService> self = this;

    connect(reply, &QNetworkReply::finished, this, [self, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (!self)
            return;
        self->in_flight_ = false;

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = QString("HTTP fetch failed: %1").arg(reply->errorString());
            LOG_WARN("FiiDii", err);
            fincept::datahub::DataHub::instance().publish_error(kTopic, err);
            emit self->failed(err);
            return;
        }

        const QString html = QString::fromUtf8(reply->readAll());
        QVector<FiiDiiDay> fresh = parse_html(html);
        LOG_INFO("FiiDii", QString("Parsed %1 rows from blissquants").arg(fresh.size()));

        if (!fresh.isEmpty()) {
            auto upr = fincept::FiiDiiRepository::instance().upsert_batch(fresh);
            if (upr.is_err())
                LOG_WARN("FiiDii", QString("upsert failed: %1")
                                        .arg(QString::fromStdString(upr.error())));
        }

        auto qr = fincept::FiiDiiRepository::instance().get_recent(kPublishWindowDays);
        if (qr.is_err()) {
            LOG_WARN("FiiDii", QString("get_recent failed: %1")
                                    .arg(QString::fromStdString(qr.error())));
            return;
        }
        QVector<FiiDiiDay> rows = qr.value();
        std::reverse(rows.begin(), rows.end());

        fincept::datahub::DataHub::instance().publish(kTopic, QVariant::fromValue(rows));
        emit self->published(rows);
        LOG_INFO("FiiDii", QString("Published %1 days (latest: %2)")
                                .arg(rows.size())
                                .arg(rows.isEmpty() ? QString("—") : rows.last().date_iso));
    });
}

} // namespace fincept::services::options
