// NewsTools.cpp — News feed, search, filtering, and monitor MCP tools

#include "mcp/tools/NewsTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QEventLoop>
#include <QVariantMap>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "NewsTools";

using namespace fincept::services;

// Synchronously fetch articles with a timeout
static QVector<NewsArticle> fetch_articles_sync(bool force = false) {
    QVector<NewsArticle> result;
    bool done = false;
    QEventLoop loop;

    NewsService::instance().fetch_all_news(force, [&](bool ok, QVector<NewsArticle> arts) {
        if (ok)
            result = std::move(arts);
        done = true;
        loop.quit();
    });

    if (!done)
        loop.exec();

    return result;
}

static QVector<NewsArticle> filter_articles(
    const QVector<NewsArticle>& articles,
    const QString& category,
    const QString& time_range,
    const QString& query,
    const QString& sentiment,
    int limit)
{
    int64_t now = QDateTime::currentSecsSinceEpoch();
    int64_t cutoff = now - 24 * 3600; // default 24H
    if (time_range == "1H")       cutoff = now - 3600;
    else if (time_range == "6H")  cutoff = now - 6 * 3600;
    else if (time_range == "48H") cutoff = now - 48 * 3600;
    else if (time_range == "7D")  cutoff = now - 7 * 86400;

    QString cat_upper  = category.toUpper().trimmed();
    QString q_lower    = query.toLower().trimmed();
    QString sent_upper = sentiment.toUpper().trimmed();

    // Map short codes → full category names used in articles
    static const QMap<QString, QString> cat_map{
        {"MKT",  "MARKETS"},  {"EARN", "EARNINGS"}, {"ECO",  "ECONOMIC"},
        {"CRPT", "CRYPTO"},   {"GEO",  "GEOPOLITICS"}, {"NRG", "ENERGY"},
        {"DEF",  "DEFENSE"},
    };
    if (cat_map.contains(cat_upper))
        cat_upper = cat_map[cat_upper];

    QVector<NewsArticle> out;
    for (const auto& a : articles) {
        if (a.sort_ts > 0 && a.sort_ts < cutoff)
            continue;
        if (!cat_upper.isEmpty() && cat_upper != "ALL" && a.category.toUpper() != cat_upper)
            continue;
        if (!sent_upper.isEmpty() && sent_upper != "ALL" &&
            sentiment_string(a.sentiment).toUpper() != sent_upper)
            continue;
        if (!q_lower.isEmpty()) {
            bool match = a.headline.toLower().contains(q_lower)
                      || a.summary.toLower().contains(q_lower)
                      || a.source.toLower().contains(q_lower)
                      || a.tickers.join(' ').toLower().contains(q_lower);
            if (!match)
                continue;
        }
        out.append(a);
        if (limit > 0 && out.size() >= limit)
            break;
    }
    return out;
}

static QJsonObject article_to_json(const NewsArticle& a) {
    return QJsonObject{
        {"id",        a.id},
        {"headline",  a.headline},
        {"summary",   a.summary},
        {"source",    a.source},
        {"category",  a.category},
        {"region",    a.region},
        {"sentiment", sentiment_string(a.sentiment)},
        {"priority",  priority_string(a.priority)},
        {"impact",    impact_string(a.impact)},
        {"tickers",   QJsonArray::fromStringList(a.tickers)},
        {"link",      a.link},
        {"time",      a.time},
        {"tier",      a.tier}
    };
}

std::vector<ToolDef> get_news_tools() {
    std::vector<ToolDef> tools;

    // ── get_news ────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_news";
        t.description =
            "Fetch latest financial news headlines. Supports filtering by category, time range, "
            "sentiment, and keyword search. "
            "Categories: ALL, MARKETS (MKT), EARNINGS (EARN), ECONOMIC (ECO), CRYPTO (CRPT), "
            "GEOPOLITICS (GEO), ENERGY (NRG), DEFENSE (DEF), TECH, REGULATORY. "
            "Time ranges: 1H, 6H, 24H (default), 48H, 7D. "
            "Sentiment: ALL (default), BULLISH, BEARISH, NEUTRAL.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"category",   QJsonObject{{"type", "string"}, {"description", "Category filter (default: ALL)"}}},
            {"time_range", QJsonObject{{"type", "string"}, {"description", "Time window: 1H, 6H, 24H, 48H, 7D"}}},
            {"sentiment",  QJsonObject{{"type", "string"}, {"description", "BULLISH, BEARISH, NEUTRAL, or ALL"}}},
            {"query",      QJsonObject{{"type", "string"}, {"description", "Keyword search across headline, summary, source, tickers"}}},
            {"limit",      QJsonObject{{"type", "integer"}, {"description", "Max articles (default: 20, max: 100)"}}},
            {"force",      QJsonObject{{"type", "boolean"}, {"description", "Force refresh ignoring cache"}}}
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString category   = args["category"].toString("ALL");
            QString time_range = args["time_range"].toString("24H");
            QString sentiment  = args["sentiment"].toString("ALL");
            QString query      = args["query"].toString();
            int limit          = qBound(1, args["limit"].toInt(20), 100);
            bool force         = args["force"].toBool(false);

            auto all = fetch_articles_sync(force);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available — feeds may still be loading");

            auto filtered = filter_articles(all, category, time_range, query, sentiment, limit);

            QJsonArray result;
            for (const auto& a : filtered)
                result.append(article_to_json(a));

            LOG_DEBUG(TAG, QString("get_news: %1/%2 articles").arg(filtered.size()).arg(all.size()));

            return ToolResult::ok(
                QString("Found %1 articles").arg(filtered.size()),
                QJsonObject{
                    {"count",      filtered.size()},
                    {"total",      all.size()},
                    {"category",   category},
                    {"time_range", time_range},
                    {"articles",   result}
                });
        };
        tools.push_back(std::move(t));
    }

    // ── get_top_news ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_top_news";
        t.description =
            "Get the top breaking and high-priority news articles (FLASH, URGENT, BREAKING). "
            "Returns the most important stories sorted by priority then recency.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Max articles (default: 10)"}}}
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int limit = qBound(1, args["limit"].toInt(10), 50);

            auto all = fetch_articles_sync(false);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available");

            QVector<NewsArticle> high_prio;
            for (const auto& a : all) {
                if (a.priority != Priority::ROUTINE)
                    high_prio.append(a);
            }

            std::sort(high_prio.begin(), high_prio.end(),
                [](const NewsArticle& x, const NewsArticle& y) {
                    if (x.priority != y.priority)
                        return static_cast<int>(x.priority) < static_cast<int>(y.priority);
                    return x.sort_ts > y.sort_ts;
                });

            if (high_prio.size() > limit)
                high_prio.resize(limit);

            QJsonArray result;
            for (const auto& a : high_prio)
                result.append(article_to_json(a));

            return ToolResult::ok(
                QString("%1 high-priority articles").arg(high_prio.size()),
                QJsonObject{{"count", high_prio.size()}, {"articles", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── search_news ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_news";
        t.description =
            "Search news by keyword, ticker symbol, or company name. "
            "Searches headline, summary, source, and associated tickers.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"query",      QJsonObject{{"type", "string"}, {"description", "Search query"}}},
            {"time_range", QJsonObject{{"type", "string"}, {"description", "Time window (default: 24H)"}}},
            {"limit",      QJsonObject{{"type", "integer"}, {"description", "Max results (default: 20)"}}}
        };
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");

            QString time_range = args["time_range"].toString("24H");
            int limit = qBound(1, args["limit"].toInt(20), 100);

            auto all = fetch_articles_sync(false);
            auto filtered = filter_articles(all, "ALL", time_range, query, "ALL", limit);

            QJsonArray result;
            for (const auto& a : filtered)
                result.append(article_to_json(a));

            return ToolResult::ok(
                QString("Found %1 articles matching '%2'").arg(filtered.size()).arg(query),
                QJsonObject{{"count", filtered.size()}, {"query", query}, {"articles", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_news_summary ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_news_summary";
        t.description =
            "Get a breakdown of current news: article count and sentiment per category. "
            "Useful for a quick market intelligence overview.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"time_range", QJsonObject{{"type", "string"}, {"description", "Time window (default: 24H)"}}}
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString time_range = args["time_range"].toString("24H");
            auto all = fetch_articles_sync(false);
            auto filtered = filter_articles(all, "ALL", time_range, "", "ALL", 0);

            QMap<QString, int> cat_counts;
            int bullish = 0, bearish = 0, neutral = 0;
            for (const auto& a : filtered) {
                cat_counts[a.category.isEmpty() ? "GENERAL" : a.category]++;
                switch (a.sentiment) {
                    case Sentiment::BULLISH: bullish++; break;
                    case Sentiment::BEARISH: bearish++; break;
                    default: neutral++; break;
                }
            }

            QJsonObject categories;
            for (auto it = cat_counts.constBegin(); it != cat_counts.constEnd(); ++it)
                categories[it.key()] = it.value();

            return ToolResult::ok_data(QJsonObject{
                {"total_articles", filtered.size()},
                {"time_range",     time_range},
                {"sentiment",      QJsonObject{
                    {"bullish", bullish},
                    {"bearish", bearish},
                    {"neutral", neutral}
                }},
                {"categories", categories}
            });
        };
        tools.push_back(std::move(t));
    }

    // ── get_news_sources ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_news_sources";
        t.description = "List all active news sources currently publishing articles in the feed.";
        t.category = "news";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto sources = NewsService::instance().active_sources();
            QJsonArray result;
            for (const auto& s : sources)
                result.append(s);
            return ToolResult::ok_data(QJsonObject{
                {"count", sources.size()}, {"sources", result}
            });
        };
        tools.push_back(std::move(t));
    }

    // ── add_news_monitor ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_news_monitor";
        t.description =
            "Create a keyword monitor that highlights articles matching specific keywords in the News screen.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"label",    QJsonObject{{"type", "string"}, {"description", "Monitor name"}}},
            {"keywords", QJsonObject{{"type", "array"},  {"description", "Keywords to track"}}},
            {"color",    QJsonObject{{"type", "string"}, {"description", "Highlight color hex (optional)"}}}
        };
        t.input_schema.required = {"label", "keywords"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString label = args["label"].toString().trimmed();
            if (label.isEmpty())
                return ToolResult::fail("Missing 'label'");

            QStringList keywords;
            for (const auto& v : args["keywords"].toArray()) {
                QString kw = v.toString().trimmed();
                if (!kw.isEmpty())
                    keywords.append(kw);
            }
            if (keywords.isEmpty())
                return ToolResult::fail("'keywords' must be a non-empty array");

            QString color = args["color"].toString();
            NewsMonitorService::instance().add_monitor(label, keywords, color);
            EventBus::instance().publish("news.monitor_added", QVariantMap{{"label", label}});

            LOG_INFO(TAG, "Added news monitor: " + label);
            return ToolResult::ok("Monitor created: " + label,
                QJsonObject{{"label", label}, {"keywords", QJsonArray::fromStringList(keywords)}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_news_monitors ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_news_monitors";
        t.description = "List all configured news keyword monitors.";
        t.category = "news";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto monitors = NewsMonitorService::instance().get_monitors();
            QJsonArray result;
            for (const auto& m : monitors) {
                result.append(QJsonObject{
                    {"id",       m.id},
                    {"label",    m.label},
                    {"keywords", QJsonArray::fromStringList(m.keywords)},
                    {"color",    m.color},
                    {"enabled",  m.enabled}
                });
            }
            return ToolResult::ok_data(QJsonObject{{"count", result.size()}, {"monitors", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── refresh_news ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "refresh_news";
        t.description = "Force a refresh of all news feeds, bypassing the cache.";
        t.category = "news";
        t.handler = [](const QJsonObject&) -> ToolResult {
            NewsService::instance().fetch_all_news(true, [](bool, QVector<NewsArticle>) {});
            EventBus::instance().publish("news.refresh_requested", {});
            return ToolResult::ok("News feeds refresh triggered");
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
