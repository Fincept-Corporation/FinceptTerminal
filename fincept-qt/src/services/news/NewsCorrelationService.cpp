#include "services/news/NewsCorrelationService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include <utility>

namespace fincept::services {

namespace news_correlation_detail {

/// polymarket.py reports numbers as strings ("0.33", "247802.0"); accept both forms.
double json_number(const QJsonValue& v, bool* ok = nullptr) {
    if (v.isDouble()) {
        if (ok)
            *ok = true;
        return v.toDouble();
    }
    bool parsed = false;
    const double d = v.toString().trimmed().toDouble(&parsed);
    if (ok)
        *ok = parsed;
    return parsed ? d : 0.0;
}

} // namespace news_correlation_detail

NewsCorrelationService& NewsCorrelationService::instance() {
    static NewsCorrelationService s;
    return s;
}

NewsCorrelationService::NewsCorrelationService() = default;

void NewsCorrelationService::detect_signals(const QVector<NewsArticle>& articles, const SignalsCallback& cb) {
    QVector<NewsSignalInput> inputs;
    inputs.reserve(articles.size());
    for (const auto& a : articles)
        inputs.append({a.category, a.headline, a.source, a.sort_ts});
    const auto detected = detect_news_signals(inputs, QDateTime::currentSecsSinceEpoch());
    if (cb)
        cb(true, detected);
}

QString NewsCorrelationService::predictions_source_label() {
    return QCoreApplication::translate("NewsCorrelationService", "Manifold Markets (play money)");
}

void NewsCorrelationService::fetch_predictions(PredictionCallback cb) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (predictions_fetched_at_ > 0 && now - predictions_fetched_at_ < kPredictionsTtlSec) {
        if (cb)
            cb(true, predictions_cache_);
        return;
    }
    // A failing source (offline, rate-limited) must not be retried on every filter
    // change — back off and answer with the last good list meanwhile.
    if (predictions_failed_at_ > 0 && now - predictions_failed_at_ < kPredictionsRetrySec) {
        if (cb)
            cb(!predictions_cache_.isEmpty(), predictions_cache_);
        return;
    }
    if (cb)
        predictions_waiters_.append(std::move(cb));
    if (predictions_inflight_)
        return; // the running fetch answers every waiter
    predictions_inflight_ = true;

    QPointer<NewsCorrelationService> self = this;
    python::PythonRunner::instance().run("polymarket.py", {"get_markets", "20"}, [self](python::PythonResult result) {
        if (!self)
            return;
        self->predictions_inflight_ = false;
        auto waiters = std::exchange(self->predictions_waiters_, {});

        bool ok = result.success;
        QVector<PredictionMarket> markets;
        if (ok) {
            const QJsonObject obj = QJsonDocument::fromJson(result.output.toUtf8()).object();
            const QJsonArray arr = obj.contains("markets") ? obj["markets"].toArray() : obj["data"].toArray();
            for (const auto& v : arr) {
                const QJsonObject m = v.toObject();
                if (m.value("closed").toBool(false))
                    continue;
                // The script emits outcomePrices ["yes","no"] as strings. This used to read
                // "yes_price"/"probability" (absent) and default to 0.5, so EVERY market was
                // shown at 50% — never fabricate a price: skip markets without one.
                bool have_price = false;
                double yes = 0.0;
                const QJsonArray prices = m.value("outcomePrices").toArray();
                if (!prices.isEmpty())
                    yes = news_correlation_detail::json_number(prices.first(), &have_price);
                if (!have_price && m.contains("yes_price"))
                    yes = news_correlation_detail::json_number(m.value("yes_price"), &have_price);
                if (!have_price && m.contains("probability"))
                    yes = news_correlation_detail::json_number(m.value("probability"), &have_price);
                if (!have_price || yes < 0.0 || yes > 1.0)
                    continue;
                PredictionMarket pm;
                pm.id = m.value("id").toString();
                pm.question = m.value("question").toString();
                pm.url = m.value("url").toString();
                pm.yes_price = yes;
                pm.no_price = 1.0 - yes;
                pm.volume = news_correlation_detail::json_number(m.value("volume"));
                markets.append(pm);
            }
        } else {
            LOG_WARN("NewsCorrelation", "Prediction fetch failed: " + result.error.left(200));
        }

        if (ok) {
            self->predictions_cache_ = markets;
            self->predictions_fetched_at_ = QDateTime::currentSecsSinceEpoch();
            self->predictions_failed_at_ = 0;
        } else {
            self->predictions_failed_at_ = QDateTime::currentSecsSinceEpoch();
        }
        for (auto& w : waiters)
            w(ok, ok ? markets : self->predictions_cache_);
    });
}

} // namespace fincept::services
