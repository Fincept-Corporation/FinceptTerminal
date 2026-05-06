#include "services/options/FiiDiiService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "python/PythonRunner.h"
#include "storage/repositories/FiiDiiRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QVariant>
#include <QVector>

namespace fincept::services::options {

namespace {

const QString kTopic = QStringLiteral("fno:fii_dii:daily");
constexpr int kPublishWindowDays = 30;
constexpr int kTtlMs = 60 * 60 * 1000;          // 1 h
constexpr int kMinIntervalMs = 30 * 60 * 1000;  // 30 min

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
    QPointer<FiiDiiService> self = this;
    fincept::python::PythonRunner::instance().run(
        QStringLiteral("fii_dii_scraper.py"), {},
        [self](fincept::python::PythonResult res) {
            if (!self)
                return;
            self->in_flight_ = false;

            if (!res.success) {
                LOG_WARN("FiiDii", QString("scraper failed: %1").arg(res.error));
                fincept::datahub::DataHub::instance().publish_error(kTopic, res.error);
                emit self->failed(res.error);
                return;
            }

            const QString out = fincept::python::extract_json(res.output);
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8(), &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
                const QString err = QString("bad scraper JSON: %1").arg(pe.errorString());
                LOG_WARN("FiiDii", err);
                fincept::datahub::DataHub::instance().publish_error(kTopic, err);
                emit self->failed(err);
                return;
            }

            QVector<FiiDiiDay> fresh;
            for (const auto& v : doc.array()) {
                const QJsonObject o = v.toObject();
                FiiDiiDay d;
                d.date_iso = o.value(QStringLiteral("date")).toString();
                d.fii_buy = o.value(QStringLiteral("fii_buy")).toDouble();
                d.fii_sell = o.value(QStringLiteral("fii_sell")).toDouble();
                d.fii_net = o.value(QStringLiteral("fii_net")).toDouble();
                d.dii_buy = o.value(QStringLiteral("dii_buy")).toDouble();
                d.dii_sell = o.value(QStringLiteral("dii_sell")).toDouble();
                d.dii_net = o.value(QStringLiteral("dii_net")).toDouble();
                if (!d.date_iso.isEmpty())
                    fresh.append(d);
            }

            // Persist what we got (could be empty when NSE rate-limits us).
            if (!fresh.isEmpty()) {
                auto upr = fincept::FiiDiiRepository::instance().upsert_batch(fresh);
                if (upr.is_err()) {
                    LOG_WARN("FiiDii", QString("upsert failed: %1")
                                            .arg(QString::fromStdString(upr.error())));
                }
            }

            // Publish the rolling kPublishWindowDays window from disk so the
            // subscriber sees cached history even when the scraper returned
            // an empty array (e.g. before market hours on a fresh DB).
            auto qr = fincept::FiiDiiRepository::instance().get_recent(kPublishWindowDays);
            if (qr.is_err()) {
                LOG_WARN("FiiDii", QString("get_recent failed: %1")
                                        .arg(QString::fromStdString(qr.error())));
                return;
            }
            QVector<FiiDiiDay> rows = qr.value();
            // get_recent returns DESC; flip to ASC for chart-friendly order.
            std::reverse(rows.begin(), rows.end());

            fincept::datahub::DataHub::instance().publish(kTopic, QVariant::fromValue(rows));
            emit self->published(rows);
            LOG_INFO("FiiDii", QString("Published %1 days (latest: %2)")
                                    .arg(rows.size())
                                    .arg(rows.isEmpty() ? "—" : rows.last().date_iso));
        });
}

} // namespace fincept::services::options
