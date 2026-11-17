#include "services/news/NewsCorrelationService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace fincept::services {

namespace news_correlation_detail {

/// Manifold Markets' public API: the markets with the most recent bets, newest first.
constexpr int kManifoldMarketLimit = 20;

QString manifold_markets_url() {
    return QStringLiteral("https://api.manifold.markets/v0/markets?limit=%1&sort=last-bet-time&order=desc")
        .arg(kManifoldMarketLimit);
}

/// Rounds like Python's round(x, ndigits) (0 <= ndigits <= 15): the exact binary value to
/// `ndigits` decimals with exact ties to even, as the nearest double — the rounding these
/// odds have always been shown with, so the panel's percentages do not shift.
double round_half_even(double x, int ndigits) {
    if (!std::isfinite(x))
        return x;
    double scale = 1.0;
    for (int i = 0; i < ndigits; ++i)
        scale *= 10.0;
    // A tie (x * 10^n == k + 1/2) is exactly a binary fraction x == odd / 2^(n+1),
    // and then x * scale is exact.
    const double halves = std::ldexp(x, ndigits + 1);
    if (halves == std::trunc(halves) && std::fmod(std::fabs(halves), 2.0) == 1.0) {
        const double lower = std::floor(x * scale);
        const double even = std::fmod(lower, 2.0) == 0.0 ? lower : lower + 1.0;
        return std::copysign(even / scale, x);
    }
    // Not a tie: the correctly rounded fixed-point text is unambiguous.
    return QString::number(x, 'f', ndigits).toDouble();
}

/// Manifold /v0/markets -> open binary markets, in API order. The first `limit` BINARY markets
/// are taken (resolved ones count toward it), resolved ones are dropped, the YES probability is
/// rounded to 4 decimals and the volume to a whole number. A market without a numeric
/// probability is skipped: a price is never invented.
QVector<PredictionMarket> parse_manifold_markets(const QJsonArray& raw, int limit) {
    QVector<PredictionMarket> markets;
    int binary_seen = 0;
    for (const auto& v : raw) {
        if (binary_seen >= limit)
            break;
        const QJsonObject m = v.toObject();
        if (m.value(QLatin1String("outcomeType")).toString() != QLatin1String("BINARY"))
            continue;
        ++binary_seen;
        if (m.value(QLatin1String("isResolved")).toBool(false))
            continue;
        const QJsonValue probability = m.value(QLatin1String("probability"));
        if (!probability.isDouble())
            continue;
        const double yes = round_half_even(probability.toDouble(), 4);
        if (yes < 0.0 || yes > 1.0)
            continue;
        PredictionMarket pm;
        pm.id = m.value(QLatin1String("id")).toString();
        pm.question = m.value(QLatin1String("question")).toString();
        pm.url = m.value(QLatin1String("url")).toString();
        pm.yes_price = yes;
        pm.no_price = 1.0 - yes;
        pm.volume = round_half_even(m.value(QLatin1String("volume")).toDouble(0.0), 0);
        markets.append(pm);
    }
    return markets;
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

void NewsCorrelationService::fetch_predictions_for(const QStringList& terms_in, PredictionCallback cb) {
    // Normalise: distinct, non-trivial terms, at most kMaxPredictionTerms.
    QStringList terms;
    for (const QString& t : terms_in) {
        const QString term = t.trimmed();
        if (term.size() < 3 || terms.contains(term, Qt::CaseInsensitive))
            continue;
        terms.append(term);
        if (terms.size() >= kMaxPredictionTerms)
            break;
    }
    if (terms.isEmpty()) {
        if (cb)
            cb(true, {});
        return;
    }

    // Answer once every term has either a fresh cache entry or a finished request.
    struct Pending {
        QStringList terms;
        int waiting = 0;
        PredictionCallback cb;
    };
    auto pending = std::make_shared<Pending>();
    pending->terms = terms;
    pending->cb = std::move(cb);
    auto finish = [this, pending]() {
        QVector<PredictionMarket> out;
        QSet<QString> ids;
        for (const QString& term : pending->terms) {
            const auto it = term_cache_.constFind(term.toLower());
            if (it == term_cache_.constEnd())
                continue;
            for (const auto& m : it->markets) {
                if (ids.contains(m.id))
                    continue;
                ids.insert(m.id);
                out.append(m);
            }
        }
        std::stable_sort(out.begin(), out.end(),
                         [](const PredictionMarket& a, const PredictionMarket& b) { return a.volume > b.volume; });
        if (out.size() > 8)
            out.resize(8);
        if (pending->cb)
            pending->cb(true, out);
    };

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const HttpClient::Headers headers{{"Accept", "application/json"}, {"User-Agent", "FinceptTerminal/4.0.2"}};
    for (const QString& term : terms) {
        TermCache& tc = term_cache_[term.toLower()];
        const bool fresh = tc.fetched_at > 0 && now - tc.fetched_at < kPredictionsTtlSec;
        const bool cooling = tc.failed_at > 0 && now - tc.failed_at < kPredictionsRetrySec;
        if (fresh || cooling || tc.inflight)
            continue; // an in-flight request is not awaited: its result shows next time
        tc.inflight = true;
        ++pending->waiting;
        const QString url = QStringLiteral("https://api.manifold.markets/v0/search-markets?term=%1&filter=open"
                                           "&contractType=BINARY&limit=10&sort=liquidity")
                                .arg(QString::fromLatin1(QUrl::toPercentEncoding(term)));
        HttpClient::instance().get(
            url,
            [this, term, pending, finish](Result<QJsonDocument> result) {
                TermCache& c = term_cache_[term.toLower()];
                c.inflight = false;
                if (result.is_ok() && result.value().isArray()) {
                    auto markets = news_correlation_detail::parse_manifold_markets(result.value().array(), 10);
                    QVector<PredictionMarket> matched;
                    for (auto& m : markets) {
                        // The search also matches descriptions; only a question that
                        // names the topic is shown as related to the headlines.
                        if (!m.question.contains(term, Qt::CaseInsensitive))
                            continue;
                        m.matched_term = term;
                        matched.append(m);
                    }
                    c.markets = matched;
                    c.fetched_at = QDateTime::currentSecsSinceEpoch();
                    c.failed_at = 0;
                } else {
                    c.failed_at = QDateTime::currentSecsSinceEpoch();
                    LOG_WARN("NewsCorrelation", QStringLiteral("Prediction search failed for \"%1\"").arg(term));
                }
                if (--pending->waiting == 0)
                    finish();
            },
            this, headers);
    }
    if (pending->waiting == 0)
        finish();
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

    const HttpClient::Headers headers{{"Accept", "application/json"}, {"User-Agent", "FinceptTerminal/4.0.2"}};
    // `this` as the context: the reply cannot outlive the service it answers.
    HttpClient::instance().get(
        news_correlation_detail::manifold_markets_url(),
        [this](Result<QJsonDocument> result) {
            predictions_inflight_ = false;
            auto waiters = std::exchange(predictions_waiters_, {});

            // Anything but a JSON array (HTTP error, transport failure, bad body) is a failed fetch.
            const bool ok = result.is_ok() && result.value().isArray();
            QVector<PredictionMarket> markets;
            if (ok) {
                markets = news_correlation_detail::parse_manifold_markets(
                    result.value().array(), news_correlation_detail::kManifoldMarketLimit);
                predictions_cache_ = markets;
                predictions_fetched_at_ = QDateTime::currentSecsSinceEpoch();
                predictions_failed_at_ = 0;
            } else {
                const QString why = result.is_ok() ? QStringLiteral("response is not a JSON array")
                                                   : QString::fromStdString(result.error());
                LOG_WARN("NewsCorrelation", "Prediction fetch failed: " + why.left(200));
                predictions_failed_at_ = QDateTime::currentSecsSinceEpoch();
            }
            for (auto& w : waiters)
                w(ok, ok ? markets : predictions_cache_);
        },
        this, headers);
}

} // namespace fincept::services
