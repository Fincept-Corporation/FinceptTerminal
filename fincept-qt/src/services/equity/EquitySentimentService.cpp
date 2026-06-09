#include "services/equity/EquitySentimentService.h"

#include "services/equity/EquityResearchService.h"
#include "services/equity/MarketSentimentService.h"
#include "services/news/NewsNlpService.h"

#include <QDateTime>
#include <QHash>
#include <QTimer>

#include <algorithm>

namespace fincept::services::equity {

namespace {

// Blend weights (base, before confidence scaling) and label band. Tunable.
constexpr double kBaseWeightNews = 0.5;
constexpr double kBaseWeightPrice = 0.3;
constexpr double kBaseWeightAdanos = 0.2;
constexpr double kLabelThreshold = 0.15;

// An available source always carries at least this much weight, so an all-NEUTRAL
// reading still counts (→ NEUTRAL) instead of collapsing to "no signal".
constexpr double kConfidenceFloor = 0.15;

constexpr int kTimeoutMs = 15000;
constexpr int kMaxArticles = 40;

QString sentiment_label(double score) {
    if (score >= kLabelThreshold)
        return QStringLiteral("BULLISH");
    if (score <= -kLabelThreshold)
        return QStringLiteral("BEARISH");
    return QStringLiteral("NEUTRAL");
}

} // namespace

EquitySentimentService& EquitySentimentService::instance() {
    static EquitySentimentService service;
    return service;
}

EquitySentimentService::EquitySentimentService(QObject* parent) : QObject(parent) {
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]() {
        if (finalized_)
            return;
        // Force completion with whatever legs resolved; unresolved sources stay
        // unavailable and simply drop out of the blend.
        news_.done = true;
        price_.done = true;
        adanos_.done = true;
        finalize();
    });

    connect(&EquityResearchService::instance(), &EquityResearchService::news_loaded, this,
            [this](const QString& symbol, const QVector<NewsArticle>& articles) { on_news_loaded(symbol, articles); });
    connect(&EquityResearchService::instance(), &EquityResearchService::technicals_loaded, this,
            [this](const TechnicalsData& data) { on_technicals_loaded(data); });
    connect(&MarketSentimentService::instance(), &MarketSentimentService::snapshot_loaded, this,
            [this](const QString& symbol, const MarketSentimentSnapshot& snap) { on_adanos_loaded(symbol, snap); });
}

void EquitySentimentService::fetch_sentiment(const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty())
        return;

    // New request: bump the id (drops stale callbacks) and reset all state
    // BEFORE firing fetches — the underlying fetchers may emit synchronously on
    // a cache hit, re-entering our slots within these calls.
    ++active_request_id_;
    active_symbol_ = sym;
    finalized_ = false;
    news_consumed_ = false;
    tech_consumed_ = false;
    adanos_consumed_ = false;
    news_ = SourceState{};
    price_ = SourceState{};
    adanos_ = SourceState{};
    bullish_ = 0;
    bearish_ = 0;
    neutral_ = 0;
    engine_.clear();
    articles_.clear();

    need_adanos_ = MarketSentimentService::instance().is_configured();
    adanos_.done = !need_adanos_; // not awaited when no key is configured

    timeout_timer_->start(kTimeoutMs);

    EquityResearchService::instance().fetch_news(sym);
    EquityResearchService::instance().fetch_technicals(sym);
    if (need_adanos_)
        MarketSentimentService::instance().fetch_snapshot(sym);
}

void EquitySentimentService::on_news_loaded(const QString& symbol, const QVector<NewsArticle>& articles) {
    if (symbol != active_symbol_ || news_consumed_)
        return;
    news_consumed_ = true;
    const quint64 rid = active_request_id_;

    if (articles.isEmpty()) {
        news_.available = false;
        news_.done = true;
        try_finalize();
        return;
    }

    // news_nlp.py reads headline/summary; equity articles use title/description.
    // Map across, tagging a stable id so we can rejoin scores for display.
    QVector<fincept::services::NewsArticle> mapped;
    mapped.reserve(articles.size());
    for (int i = 0; i < articles.size(); ++i) {
        fincept::services::NewsArticle n;
        n.id = QString::number(i);
        n.headline = articles[i].title;
        n.summary = articles[i].description;
        mapped.append(n);
    }
    const QVector<NewsArticle> source_articles = articles;

    NewsNlpService::instance().analyze_sentiment(
        mapped, [this, rid, source_articles](bool ok, const NewsSentimentResult& result) {
            if (rid != active_request_id_)
                return; // superseded by a newer request
            if (!ok) {
                news_.available = false;
                news_.done = true;
                try_finalize();
                return;
            }

            engine_ = result.engine;
            bullish_ = result.bullish;
            bearish_ = result.bearish;
            neutral_ = result.neutral;

            QHash<QString, ArticleSentimentResult> by_id;
            by_id.reserve(result.per_article.size());
            for (const auto& ar : result.per_article)
                by_id.insert(ar.id, ar);

            articles_.clear();
            const int limit = std::min<int>(source_articles.size(), kMaxArticles);
            for (int i = 0; i < limit; ++i) {
                ArticleSentiment a;
                a.title = source_articles[i].title;
                a.publisher = source_articles[i].publisher;
                a.published_date = source_articles[i].published_date;
                a.url = source_articles[i].url;
                const auto res = by_id.value(QString::number(i));
                a.label = res.label.isEmpty() ? QStringLiteral("NEUTRAL") : res.label;
                a.score = res.score;
                articles_.append(a);
            }

            const int scored = result.bullish + result.bearish + result.neutral;
            const int non_neutral = result.bullish + result.bearish;
            news_.score = result.overall_score;
            news_.confidence = scored > 0 ? static_cast<double>(non_neutral) / scored : 0.0;
            news_.available = scored > 0;
            news_.done = true;
            try_finalize();
        });
}

void EquitySentimentService::on_technicals_loaded(const TechnicalsData& data) {
    if (data.symbol != active_symbol_ || tech_consumed_)
        return;
    tech_consumed_ = true;

    const int total = data.strong_buy + data.buy + data.neutral + data.sell + data.strong_sell;
    if (total > 0) {
        const double raw =
            (data.strong_buy + 0.5 * data.buy - 0.5 * data.sell - data.strong_sell) / static_cast<double>(total);
        price_.score = std::clamp(raw, -1.0, 1.0);
        const int non_neutral = data.strong_buy + data.buy + data.sell + data.strong_sell;
        price_.confidence = static_cast<double>(non_neutral) / total;
        price_.available = true;
    }
    price_.done = true;
    try_finalize();
}

void EquitySentimentService::on_adanos_loaded(const QString& symbol, const MarketSentimentSnapshot& snapshot) {
    if (!need_adanos_ || symbol != active_symbol_ || adanos_consumed_)
        return;
    adanos_consumed_ = true;

    if (snapshot.configured && snapshot.available && snapshot.coverage > 0) {
        adanos_.score = std::clamp((snapshot.average_bullish_pct - 50.0) / 50.0, -1.0, 1.0);
        adanos_.confidence = 1.0;
        adanos_.available = true;
    }
    adanos_.done = true;
    try_finalize();
}

void EquitySentimentService::try_finalize() {
    if (finalized_)
        return;
    if (!news_.done || !price_.done)
        return;
    if (need_adanos_ && !adanos_.done)
        return;
    finalize();
}

void EquitySentimentService::finalize() {
    if (finalized_)
        return;
    finalized_ = true;
    timeout_timer_->stop();

    EquitySentimentSnapshot snap;
    snap.symbol = active_symbol_;
    snap.engine = engine_;
    snap.bullish = bullish_;
    snap.bearish = bearish_;
    snap.neutral = neutral_;
    snap.article_count = bullish_ + bearish_ + neutral_;
    snap.articles = articles_;
    snap.fetched_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    struct Contrib {
        QString id;
        QString label;
        const SourceState* state;
        double base;
        bool include;
    };
    const Contrib contribs[] = {
        {QStringLiteral("news"), QStringLiteral("News Sentiment"), &news_, kBaseWeightNews, true},
        {QStringLiteral("price"), QStringLiteral("Price Momentum"), &price_, kBaseWeightPrice, true},
        // Adanos row only shown/blended when a key is configured.
        {QStringLiteral("adanos"), QStringLiteral("Adanos"), &adanos_, kBaseWeightAdanos, need_adanos_},
    };

    double weight_sum = 0.0;
    for (const auto& c : contribs) {
        if (c.include && c.state->available)
            weight_sum += c.base * std::max(c.state->confidence, kConfidenceFloor);
    }

    double blended = 0.0;
    double confidence_acc = 0.0;
    for (const auto& c : contribs) {
        if (!c.include)
            continue;
        SentimentSource src;
        src.id = c.id;
        src.label = c.label;
        src.available = c.state->available;
        src.score = c.state->score;
        src.confidence = c.state->confidence;
        if (c.state->available && weight_sum > 0.0) {
            const double w = (c.base * std::max(c.state->confidence, kConfidenceFloor)) / weight_sum;
            src.weight = w;
            blended += w * c.state->score;
            confidence_acc += w * c.state->confidence;
        }
        snap.sources.append(src);
    }

    if (weight_sum > 0.0) {
        snap.available = true;
        snap.status = QStringLiteral("ok");
        snap.overall_score = std::clamp(blended, -1.0, 1.0);
        snap.label = sentiment_label(snap.overall_score);
        snap.confidence = std::clamp(confidence_acc, 0.0, 1.0);
    } else {
        snap.available = false;
        snap.status = QStringLiteral("unavailable");
        snap.label = QStringLiteral("NEUTRAL");
        snap.message =
            QStringLiteral("No sentiment signal yet — news and price momentum are unavailable for this symbol.");
    }

    emit sentiment_loaded(active_symbol_, snap);
}

} // namespace fincept::services::equity
