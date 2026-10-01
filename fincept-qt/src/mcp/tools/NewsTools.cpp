// NewsTools.cpp — News feed, search, filtering, and monitor MCP tools

#include "mcp/tools/NewsTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/tools/ThreadHelper.h"
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QMetaObject>
#include <QVariantMap>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "NewsTools";

using namespace fincept::services;

// Synchronously fetch articles with a timeout.
//
// MCP tool handlers run on a pool thread. NewsService's QNetworkAccessManager lives on the
// main thread; firing requests at it from a worker corrupts QObject parentage and eventually
// crashes Qt, so the fetch is marshalled to the service's thread by detail::run_async_wait
// (ThreadHelper.h), which waits on a bounded condition rather than a nested event loop.
static QVector<NewsArticle> fetch_articles_sync(bool force = false) {
    auto* svc = &NewsService::instance();

    // The hand-rolled QMutex/QWaitCondition version waited with no bound at all: a
    // fetch whose callback never fired (a service error branch that returns early)
    // parked the tool thread forever — and every news tool and the AI chat's tool pool
    // with it. detail::run_async_wait covers both the worker-thread and same-thread
    // cases and gives up after its bound with an error log; the result then stays
    // empty and the callers already report "No news articles available".
    //
    // The result lives on the heap so a callback that arrives AFTER the bound writes
    // into live memory instead of an unwound stack frame.
    auto result = std::make_shared<QVector<NewsArticle>>();
    detail::run_async_wait(svc, [svc, force, result](auto signal_done) {
        svc->fetch_all_news(force, [result, signal_done](bool ok, QVector<NewsArticle> arts) {
            if (ok)
                *result = std::move(arts);
            signal_done();
        });
    });
    return *result;
}

static QVector<NewsArticle> filter_articles(const QVector<NewsArticle>& articles, const QString& category,
                                            const QString& time_range, const QString& query, const QString& sentiment,
                                            int limit) {
    int64_t now = QDateTime::currentSecsSinceEpoch();
    int64_t cutoff = now - 24 * 3600; // default 24H
    if (time_range == "1H")
        cutoff = now - 3600;
    else if (time_range == "6H")
        cutoff = now - 6 * 3600;
    else if (time_range == "48H")
        cutoff = now - 48 * 3600;
    else if (time_range == "7D")
        cutoff = now - 7 * 86400;
    else if (time_range == "30D")
        cutoff = now - 30LL * 86400;

    QString cat_upper = category.toUpper().trimmed();
    QString q_lower = query.toLower().trimmed();
    QString sent_upper = sentiment.toUpper().trimmed();

    // Map short codes → full category names used in articles
    static const QMap<QString, QString> cat_map{
        {"MKT", "MARKETS"},     {"EARN", "EARNINGS"}, {"ECO", "ECONOMIC"}, {"CRPT", "CRYPTO"},
        {"GEO", "GEOPOLITICS"}, {"NRG", "ENERGY"},    {"DEF", "DEFENSE"},
    };
    if (cat_map.contains(cat_upper))
        cat_upper = cat_map[cat_upper];

    QVector<NewsArticle> out;
    for (const auto& a : articles) {
        if (a.sort_ts > 0 && a.sort_ts < cutoff)
            continue;
        if (!cat_upper.isEmpty() && cat_upper != "ALL" && a.category.toUpper() != cat_upper)
            continue;
        if (!sent_upper.isEmpty() && sent_upper != "ALL" && sentiment_string(a.sentiment).toUpper() != sent_upper)
            continue;
        if (!q_lower.isEmpty()) {
            bool match = a.headline.toLower().contains(q_lower) || a.summary.toLower().contains(q_lower) ||
                         a.source.toLower().contains(q_lower) || a.tickers.join(' ').toLower().contains(q_lower);
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
    return QJsonObject{{"id", a.id},
                       {"headline", a.headline},
                       {"summary", a.summary},
                       {"source", a.source},
                       {"category", a.category},
                       {"region", a.region},
                       {"sentiment", sentiment_string(a.sentiment)},
                       {"priority", priority_string(a.priority)},
                       {"impact", impact_string(a.impact)},
                       {"tickers", QJsonArray::fromStringList(a.tickers)},
                       {"link", a.link},
                       {"time", a.time},
                       {"tier", a.tier}};
}

std::vector<ToolDef> get_news_tools() {
    std::vector<ToolDef> tools;

    // ── get_news ────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_news";
        t.description = "Fetch latest financial news headlines. Supports filtering by category, time range, "
                        "sentiment, and keyword search. "
                        "Categories: ALL, MARKETS (MKT), EARNINGS (EARN), ECONOMIC (ECO), CRYPTO (CRPT), "
                        "GEOPOLITICS (GEO), ENERGY (NRG), DEFENSE (DEF), TECH, REGULATORY. "
                        "Time ranges: 1H, 6H, 24H (default), 48H, 7D, 30D. "
                        "Sentiment: ALL (default), BULLISH, BEARISH, NEUTRAL.";
        t.category = "news";
        t.input_schema = ToolSchemaBuilder()
                             .string("category", "Category filter")
                             .default_str("ALL")
                             .enums({"ALL", "MARKETS", "MKT", "EARNINGS", "EARN", "ECONOMIC", "ECO", "CRYPTO", "CRPT",
                                     "GEOPOLITICS", "GEO", "ENERGY", "NRG", "DEFENSE", "DEF", "TECH", "REGULATORY"})
                             .string("time_range", "Time window")
                             .default_str("24H")
                             .enums({"1H", "6H", "24H", "48H", "7D", "30D"})
                             .string("sentiment", "Sentiment filter")
                             .default_str("ALL")
                             .enums({"ALL", "BULLISH", "BEARISH", "NEUTRAL"})
                             .string("query", "Keyword search across headline, summary, source, tickers")
                             .integer("limit", "Max articles")
                             .default_int(20)
                             .between(1, 100)
                             .boolean("force", "Force refresh ignoring cache")
                             .default_bool(false)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            // Defaults are now injected by the validator; the .toString(...)
            // fallback args remain harmless but unnecessary.
            QString category = args["category"].toString("ALL");
            QString time_range = args["time_range"].toString("24H");
            QString sentiment = args["sentiment"].toString("ALL");
            QString query = args["query"].toString();
            int limit = qBound(1, args["limit"].toInt(20), 100);
            bool force = args["force"].toBool(false);

            auto all = fetch_articles_sync(force);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available — feeds may still be loading");

            // Filter without a cap, then slice: stopping the filter at `limit` made
            // "20 articles" indistinguishable from "exactly 20 matched" (§M4).
            auto filtered = filter_articles(all, category, time_range, query, sentiment, 0);
            const int matched = static_cast<int>(filtered.size());
            if (filtered.size() > limit)
                filtered.resize(limit);

            QJsonArray result;
            for (const auto& a : filtered)
                result.append(article_to_json(a));

            LOG_DEBUG(TAG, QString("get_news: %1/%2 articles").arg(filtered.size()).arg(all.size()));

            const bool truncated = matched > filtered.size();
            return ToolResult::ok(truncated ? QString("Showing %1 of %2 matching articles (raise `limit`, max 100, or "
                                                      "narrow the filters to see others)")
                                                  .arg(filtered.size())
                                                  .arg(matched)
                                            : QString("Found %1 articles").arg(filtered.size()),
                                  QJsonObject{{"count", filtered.size()},
                                              {"matched", matched},
                                              {"truncated", truncated},
                                              {"total", all.size()},
                                              {"category", category},
                                              {"time_range", time_range},
                                              {"articles", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_top_news ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_top_news";
        t.description = "Get the top breaking and high-priority news articles (FLASH, URGENT, BREAKING). "
                        "Returns the most important stories sorted by priority then recency.";
        t.category = "news";
        t.input_schema.properties =
            QJsonObject{{"limit", QJsonObject{{"type", "integer"}, {"description", "Max articles (default: 10)"}}}};
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

            std::sort(high_prio.begin(), high_prio.end(), [](const NewsArticle& x, const NewsArticle& y) {
                if (x.priority != y.priority)
                    return static_cast<int>(x.priority) < static_cast<int>(y.priority);
                return x.sort_ts > y.sort_ts;
            });

            const int matched = static_cast<int>(high_prio.size());
            if (high_prio.size() > limit)
                high_prio.resize(limit);

            QJsonArray result;
            for (const auto& a : high_prio)
                result.append(article_to_json(a));

            const bool truncated = matched > high_prio.size();
            return ToolResult::ok(truncated ? QString("Showing the top %1 of %2 high-priority articles (raise `limit`, "
                                                      "max 50, to see more)")
                                                  .arg(high_prio.size())
                                                  .arg(matched)
                                            : QString("%1 high-priority articles").arg(high_prio.size()),
                                  QJsonObject{{"count", high_prio.size()},
                                              {"matched", matched},
                                              {"truncated", truncated},
                                              {"articles", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── search_news ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_news";
        t.description = "Search news by keyword, ticker symbol, or company name. "
                        "Searches headline, summary, source, and associated tickers.";
        t.category = "news";
        t.input_schema.properties =
            QJsonObject{{"query", QJsonObject{{"type", "string"}, {"description", "Search query"}}},
                        {"time_range", QJsonObject{{"type", "string"}, {"description", "Time window (default: 24H)"}}},
                        {"limit", QJsonObject{{"type", "integer"}, {"description", "Max results (default: 20)"}}}};
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");

            QString time_range = args["time_range"].toString("24H");
            int limit = qBound(1, args["limit"].toInt(20), 100);

            auto all = fetch_articles_sync(false);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available — feeds may still be loading");
            auto filtered = filter_articles(all, "ALL", time_range, query, "ALL", 0);
            const int matched = static_cast<int>(filtered.size());
            if (filtered.size() > limit)
                filtered.resize(limit);

            QJsonArray result;
            for (const auto& a : filtered)
                result.append(article_to_json(a));

            const bool truncated = matched > filtered.size();
            return ToolResult::ok(
                truncated ? QString("Showing %1 of %2 articles matching '%3' (raise `limit`, max 100, to see others)")
                                .arg(filtered.size())
                                .arg(matched)
                                .arg(query)
                          : QString("Found %1 articles matching '%2'").arg(filtered.size()).arg(query),
                QJsonObject{{"count", filtered.size()},
                            {"matched", matched},
                            {"truncated", truncated},
                            {"query", query},
                            {"articles", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_news_summary ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_news_summary";
        t.description = "Get a breakdown of current news: article count and sentiment per category. "
                        "Useful for a quick market intelligence overview.";
        t.category = "news";
        t.input_schema.properties =
            QJsonObject{{"time_range", QJsonObject{{"type", "string"}, {"description", "Time window (default: 24H)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString time_range = args["time_range"].toString("24H");
            auto all = fetch_articles_sync(false);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available — feeds may still be loading");
            auto filtered = filter_articles(all, "ALL", time_range, "", "ALL", 0);

            QMap<QString, int> cat_counts;
            int bullish = 0, bearish = 0, neutral = 0;
            for (const auto& a : filtered) {
                cat_counts[a.category.isEmpty() ? "GENERAL" : a.category]++;
                switch (a.sentiment) {
                    case Sentiment::BULLISH:
                        bullish++;
                        break;
                    case Sentiment::BEARISH:
                        bearish++;
                        break;
                    default:
                        neutral++;
                        break;
                }
            }

            QJsonObject categories;
            for (auto it = cat_counts.constBegin(); it != cat_counts.constEnd(); ++it)
                categories[it.key()] = it.value();

            return ToolResult::ok_data(QJsonObject{
                {"total_articles", filtered.size()},
                {"time_range", time_range},
                {"sentiment", QJsonObject{{"bullish", bullish}, {"bearish", bearish}, {"neutral", neutral}}},
                {"categories", categories}});
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
            return ToolResult::ok_data(QJsonObject{{"count", sources.size()}, {"sources", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── add_news_monitor ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_news_monitor";
        t.description =
            "Create a news alert: a keyword monitor that notifies you of, and highlights, breaking articles "
            "matching specific keywords or topics in the News screen.";
        t.category = "news";
        t.is_destructive = true; // creates a persisted monitor that drives alerts
        t.input_schema.properties = QJsonObject{
            {"label", QJsonObject{{"type", "string"}, {"description", "Monitor name"}}},
            {"keywords", QJsonObject{{"type", "array"},
                                     {"items", QJsonObject{{"type", "string"}}},
                                     {"description", "Keywords to track"}}},
            {"color", QJsonObject{{"type", "string"}, {"description", "Highlight color hex (optional)"}}}};
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
            // The service owns `monitors_` on its thread, and the News screen reads it
            // there — appending from this worker raced those reads. Marshal it, and
            // read back the id the service generated so the model can toggle/delete
            // the monitor it just made without a get_news_monitors round trip.
            auto* svc = &NewsMonitorService::instance();
            QString new_id;
            detail::run_async_wait(svc, [svc, label, keywords, color, &new_id](auto signal_done) {
                svc->add_monitor(label, keywords, color);
                const auto all = svc->get_monitors();
                if (!all.isEmpty())
                    new_id = all.last().id;
                signal_done();
            });
            EventBus::instance().publish("news.monitor_added", QVariantMap{{"label", label}});

            LOG_INFO(TAG, "Added news monitor: " + label);
            return ToolResult::ok("Monitor created: " + label,
                                  QJsonObject{{"id", new_id},
                                              {"label", label},
                                              {"keywords", QJsonArray::fromStringList(keywords)}});
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
            // Copy the list on the service's thread — the News screen mutates it there.
            auto* svc = &NewsMonitorService::instance();
            QVector<NewsMonitor> monitors;
            detail::run_async_wait(svc, [svc, &monitors](auto signal_done) {
                monitors = svc->get_monitors();
                signal_done();
            });
            QJsonArray result;
            for (const auto& m : monitors) {
                result.append(QJsonObject{{"id", m.id},
                                          {"label", m.label},
                                          {"keywords", QJsonArray::fromStringList(m.keywords)},
                                          {"color", m.color},
                                          {"enabled", m.enabled}});
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
            // NewsService's QNetworkAccessManager lives on the main thread.
            // MCP tool handlers run on a worker thread — calling fetch_all_news
            // directly here would create child QObjects across threads and
            // corrupt Qt's parentage invariants (manifests as random crashes
            // ~10s later). Marshal onto the service's thread.
            auto* svc = &NewsService::instance();
            QMetaObject::invokeMethod(
                svc, [svc]() { svc->fetch_all_news(true, [](bool, QVector<NewsArticle>) {}); }, Qt::QueuedConnection);
            EventBus::instance().publish("news.refresh_requested", {});
            return ToolResult::ok("News feeds refresh triggered");
        };
        tools.push_back(std::move(t));
    }

    // ── toggle_news_monitor ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "toggle_news_monitor";
        t.description = "Enable or disable an existing news keyword monitor by id. Pass `enabled` to set the state "
                        "explicitly; without it the monitor is flipped. Returns the resulting state. "
                        "Use get_news_monitors to discover monitor ids.";
        t.category = "news";
        t.is_destructive = true; // changes which alerts fire
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Monitor id"}}},
            {"enabled", QJsonObject{{"type", "boolean"},
                                    {"description", "Desired state (true = on). Omit to flip the current state."}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");
            const bool has_desired = args.contains("enabled");
            const bool desired = args["enabled"].toBool();
            auto* svc = &NewsMonitorService::instance();
            // The service only offers a blind flip and silently ignores an unknown id, so
            // the old tool reported "Monitor toggled" for ids that do not exist and never
            // said which way it flipped. Look the monitor up first, flip only when needed,
            // and report the state it ended in.
            bool found = false;
            bool now_enabled = false;
            detail::run_async_wait(svc, [svc, id, has_desired, desired, &found, &now_enabled](auto signal_done) {
                for (const auto& m : svc->get_monitors()) {
                    if (m.id != id)
                        continue;
                    found = true;
                    now_enabled = m.enabled;
                    break;
                }
                if (found && (!has_desired || desired != now_enabled)) {
                    svc->toggle_monitor(id);
                    now_enabled = !now_enabled;
                }
                signal_done();
            });
            if (!found)
                return ToolResult::fail("Unknown monitor id '" + id + "' — use get_news_monitors to list ids");
            EventBus::instance().publish("news.monitor_toggled", QVariantMap{{"id", id}});
            return ToolResult::ok(QString("Monitor %1 is now %2").arg(id, now_enabled ? "enabled" : "disabled"),
                                  QJsonObject{{"id", id}, {"enabled", now_enabled}});
        };
        tools.push_back(std::move(t));
    }

    // ── delete_news_monitor ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_news_monitor";
        t.description = "Permanently remove a news keyword monitor by id.";
        t.category = "news";
        t.is_destructive = true; // mutation tool — penalise on read-style queries
        t.input_schema.properties = QJsonObject{{"id", QJsonObject{{"type", "string"}, {"description", "Monitor id"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");
            auto* svc = &NewsMonitorService::instance();
            bool found = false;
            detail::run_async_wait(svc, [svc, id, &found](auto signal_done) {
                for (const auto& m : svc->get_monitors()) {
                    if (m.id == id) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    svc->delete_monitor(id);
                signal_done();
            });
            if (!found)
                return ToolResult::fail("Unknown monitor id '" + id + "' — nothing was deleted. Use "
                                                                        "get_news_monitors to list ids");
            EventBus::instance().publish("news.monitor_deleted", QVariantMap{{"id", id}});
            return ToolResult::ok("Monitor deleted: " + id, QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── analyze_news_article ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "analyze_news_article";
        t.description = "Run AI sentiment + market-impact + risk analysis on a single article URL. "
                        "Returns sentiment score, urgency, key points, and regulatory/geopolitical/"
                        "operational/market risk signals. Consumes API credits.";
        t.category = "news";
        t.input_schema.properties =
            QJsonObject{{"url", QJsonObject{{"type", "string"}, {"description", "Article URL"}}}};
        t.input_schema.required = {"url"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString url = args["url"].toString().trimmed();
            if (url.isEmpty())
                return ToolResult::fail("Missing 'url'");

            auto* svc = &NewsService::instance();
            bool ok = false;
            NewsAnalysis analysis;
            detail::run_async_wait(svc, [svc, url, &ok, &analysis](auto signal_done) {
                svc->analyze_article(url, [signal_done, &ok, &analysis](bool success, NewsAnalysis a) {
                    ok = success;
                    if (success)
                        analysis = std::move(a);
                    signal_done();
                });
            });

            if (!ok)
                return ToolResult::fail("Analysis failed (network error or insufficient credits)");

            auto risk_to_json = [](const RiskSignal& r) {
                return QJsonObject{{"level", r.level}, {"details", r.details}};
            };
            return ToolResult::ok_data(
                QJsonObject{{"summary", analysis.summary},
                            {"sentiment", QJsonObject{{"score", analysis.sentiment.score},
                                                      {"intensity", analysis.sentiment.intensity},
                                                      {"confidence", analysis.sentiment.confidence}}},
                            {"market_impact", QJsonObject{{"urgency", analysis.market_impact.urgency},
                                                          {"prediction", analysis.market_impact.prediction}}},
                            {"keywords", QJsonArray::fromStringList(analysis.keywords)},
                            {"topics", QJsonArray::fromStringList(analysis.topics)},
                            {"key_points", QJsonArray::fromStringList(analysis.key_points)},
                            {"risk_signals", QJsonObject{{"regulatory", risk_to_json(analysis.regulatory)},
                                                         {"geopolitical", risk_to_json(analysis.geopolitical)},
                                                         {"operational", risk_to_json(analysis.operational)},
                                                         {"market", risk_to_json(analysis.market)}}},
                            {"credits_used", analysis.credits_used},
                            {"credits_remaining", analysis.credits_remaining}});
        };
        tools.push_back(std::move(t));
    }

    // ── summarize_news_headlines ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "summarize_news_headlines";
        t.description = "AI-summarize the top N current news headlines into a single paragraph "
                        "of market-relevant insight. Cached for 10 minutes per headline set.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"count", QJsonObject{{"type", "integer"},
                                  {"description", "Number of top headlines to summarize (default: 8, max: 25)"}}},
            {"category", QJsonObject{{"type", "string"},
                                     {"description", "Optional category filter to scope the summary (default: ALL)"}}},
            {"time_range",
             QJsonObject{{"type", "string"}, {"description", "Time window for source articles (default: 24H)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int count = qBound(1, args["count"].toInt(8), 25);
            QString category = args["category"].toString("ALL");
            QString time_range = args["time_range"].toString("24H");

            auto all = fetch_articles_sync(false);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available");

            auto filtered = filter_articles(all, category, time_range, "", "ALL", count);
            if (filtered.isEmpty())
                return ToolResult::fail("No articles match the requested filter");

            auto* svc = &NewsService::instance();
            bool ok = false;
            QString summary;
            detail::run_async_wait(svc, [svc, filtered, count, &ok, &summary](auto signal_done) {
                svc->summarize_headlines(filtered, count, [signal_done, &ok, &summary](bool success, QString s) {
                    ok = success;
                    if (success)
                        summary = std::move(s);
                    signal_done();
                });
            });

            if (!ok || summary.isEmpty())
                return ToolResult::fail("Summarization failed (network error or empty response)");

            return ToolResult::ok_data(
                QJsonObject{{"summary", summary}, {"headline_count", filtered.size()}, {"time_range", time_range}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_news_clusters ───────────────────────────────────────────────
    //
    // Groups related stories across sources. An LLM agent uses this to
    // distinguish a real event ("5 sources covering this") from a one-off
    // post. Each cluster carries a lead article + sibling articles, source
    // count, velocity (rising/stable/falling), and a `is_breaking` flag.
    {
        ToolDef t;
        t.name = "get_news_clusters";
        t.description = "Group related news stories across sources into clusters. "
                        "Returns each cluster's lead article, sibling articles, source count, "
                        "velocity (rising/stable/falling), and breaking flag. "
                        "Use this to find events being covered by multiple outlets — high "
                        "source_count = real story, low = niche or single-source. "
                        "Filters: category, time_range (1H/6H/24H/48H/7D/30D), breaking_only.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"category", QJsonObject{{"type", "string"}, {"description", "Category filter (default: ALL)"}}},
            {"time_range", QJsonObject{{"type", "string"}, {"description", "Time window: 1H, 6H, 24H, 48H, 7D, 30D"}}},
            {"breaking_only", QJsonObject{{"type", "boolean"},
                                          {"description", "Restrict to breaking-news clusters only (default: false)"}}},
            {"min_sources", QJsonObject{{"type", "integer"},
                                        {"description", "Only return clusters with at least N sources (default: 1)"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Max clusters (default: 25, max: 100)"}}},
            {"include_articles",
             QJsonObject{
                 {"type", "boolean"},
                 {"description", "Include the full sibling-article list per cluster (default: false; lead only)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString category = args["category"].toString("ALL");
            QString time_range = args["time_range"].toString("24H");
            bool breaking_only = args["breaking_only"].toBool(false);
            int min_sources = std::max(1, args["min_sources"].toInt(1));
            int limit = qBound(1, args["limit"].toInt(25), 100);
            bool include_articles = args["include_articles"].toBool(false);

            auto all = fetch_articles_sync(false);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available");

            // Filter the article pool first so clustering only sees the
            // relevant subset — cheaper and avoids cross-window dilution.
            auto pool = filter_articles(all, category, time_range, "", "ALL", 0);
            auto clusters = cluster_articles(pool);
            if (breaking_only)
                clusters = get_breaking_clusters(clusters);

            QJsonArray result;
            int eligible = 0;
            for (const auto& c : clusters) {
                if (c.source_count < min_sources)
                    continue;
                ++eligible;
                if (result.size() >= limit)
                    continue; // keep counting so a clipped list is reported as clipped (§M4)
                QJsonObject obj{{"id", c.id},
                                {"lead", article_to_json(c.lead_article)},
                                {"source_count", c.source_count},
                                {"article_count", c.articles.size()},
                                {"velocity", c.velocity},
                                {"sentiment", sentiment_string(c.sentiment)},
                                {"category", c.category},
                                {"tier", c.tier},
                                {"is_breaking", c.is_breaking},
                                {"latest_sort_ts", static_cast<qint64>(c.latest_sort_ts)}};
                if (include_articles) {
                    QJsonArray arts;
                    for (const auto& a : c.articles)
                        arts.append(article_to_json(a));
                    obj["articles"] = arts;
                }
                result.append(obj);
            }

            const bool truncated = eligible > result.size();
            return ToolResult::ok(truncated ? QString("Showing %1 of %2 clusters (raise `limit`, max 100, or raise "
                                                      "`min_sources` to see the strongest stories)")
                                                  .arg(result.size())
                                                  .arg(eligible)
                                            : QString("%1 clusters").arg(result.size()),
                                  QJsonObject{{"count", result.size()},
                                              {"matched", eligible},
                                              {"truncated", truncated},
                                              {"category", category},
                                              {"time_range", time_range},
                                              {"breaking_only", breaking_only},
                                              {"clusters", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_threat_alerts ───────────────────────────────────────────────
    //
    // Surfaces only articles classified as HIGH or CRITICAL threat by
    // NewsService::classify_threat(). Risk-monitoring agents use this to
    // poll for events without sifting routine market news.
    {
        ToolDef t;
        t.name = "get_threat_alerts";
        t.description = "Return only news articles classified as HIGH or CRITICAL threat — "
                        "war/conflict, market crashes, cyberattacks, sovereign defaults, etc. "
                        "Each result carries threat level, threat category (conflict/cyber/"
                        "natural/market/regulatory/general), and confidence score (0-1). "
                        "Filter by min_level (CRITICAL or HIGH; default: HIGH) and time_range.";
        t.category = "news";
        t.input_schema.properties = QJsonObject{
            {"min_level", QJsonObject{{"type", "string"},
                                      {"description", "Minimum threat level: CRITICAL or HIGH (default: HIGH)"}}},
            {"threat_category", QJsonObject{{"type", "string"},
                                            {"description", "Filter by threat category: conflict, cyber, natural, "
                                                            "market, regulatory, general (default: any)"}}},
            {"time_range", QJsonObject{{"type", "string"}, {"description", "Time window: 1H, 6H, 24H, 48H, 7D, 30D"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Max alerts (default: 25, max: 100)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString min_level_s = args["min_level"].toString("HIGH").toUpper().trimmed();
            QString threat_cat_filter = args["threat_category"].toString().toLower().trimmed();
            QString time_range = args["time_range"].toString("24H");
            int limit = qBound(1, args["limit"].toInt(25), 100);

            // Resolve min_level — anything below HIGH is rejected; only the
            // top two tiers count as "alerts".
            const bool critical_only = (min_level_s == "CRITICAL");

            auto all = fetch_articles_sync(false);
            if (all.isEmpty())
                return ToolResult::fail("No news articles available");

            auto pool = filter_articles(all, "ALL", time_range, "", "ALL", 0);

            QJsonArray result;
            int critical = 0, high = 0;
            for (const auto& a : pool) {
                const auto level = a.threat.level;
                const bool keep = critical_only ? (level == ThreatLevel::CRITICAL)
                                                : (level == ThreatLevel::CRITICAL || level == ThreatLevel::HIGH);
                if (!keep)
                    continue;
                if (!threat_cat_filter.isEmpty() && a.threat.category.toLower() != threat_cat_filter)
                    continue;

                // Count every match, but only serialise up to `limit` — the counts
                // below describe ALL alerts, so a clipped list is visibly clipped (§M4).
                if (level == ThreatLevel::CRITICAL)
                    ++critical;
                else
                    ++high;

                if (result.size() >= limit)
                    continue;

                QJsonObject obj = article_to_json(a);
                obj["threat_level"] = threat_level_string(level);
                obj["threat_category"] = a.threat.category;
                obj["threat_confidence"] = a.threat.confidence;
                result.append(obj);
            }

            const int total_alerts = critical + high;
            const bool truncated = total_alerts > result.size();
            return ToolResult::ok(
                truncated ? QString("Showing %1 of %2 threat alerts (%3 critical, %4 high) — raise `limit` (max 100) "
                                    "or set min_level=CRITICAL to see the rest")
                                .arg(result.size())
                                .arg(total_alerts)
                                .arg(critical)
                                .arg(high)
                          : QString("%1 threat alerts (%2 critical, %3 high)").arg(result.size()).arg(critical).arg(high),
                QJsonObject{{"count", result.size()},
                            {"truncated", truncated},
                            {"critical_count", critical},
                            {"high_count", high},
                            {"min_level", min_level_s},
                            {"time_range", time_range},
                            {"alerts", result}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_news_status ─────────────────────────────────────────────────
    //
    // Diagnostic / health-check tool. An agent calls this before relying on
    // news output to confirm feeds are loaded — otherwise a cold-cache run
    // returns 0 articles silently and downstream reasoning is meaningless.
    {
        ToolDef t;
        t.name = "get_news_status";
        t.description = "Health check for the news subsystem: total feeds configured, active "
                        "sources publishing, cached article count, live WebSocket connection "
                        "status, breaking-cluster count, and a snapshot of category counts. "
                        "Call this before running multi-step research so the agent can detect "
                        "an empty / cold-cache state instead of reasoning over no data.";
        t.category = "news";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto* svc = &NewsService::instance();

            // Reuse cached articles if available — explicitly NOT forcing a
            // refresh here. This is a status check, not a fetch trigger.
            auto all = fetch_articles_sync(false);

            QSet<QString> sources;
            QMap<QString, int> categories;
            int bullish = 0, bearish = 0, neutral = 0;
            int high_threat = 0, critical_threat = 0;
            int64_t latest_ts = 0;
            for (const auto& a : all) {
                if (!a.source.isEmpty())
                    sources.insert(a.source);
                if (!a.category.isEmpty())
                    categories[a.category]++;
                switch (a.sentiment) {
                    case Sentiment::BULLISH:
                        ++bullish;
                        break;
                    case Sentiment::BEARISH:
                        ++bearish;
                        break;
                    default:
                        ++neutral;
                        break;
                }
                if (a.threat.level == ThreatLevel::CRITICAL)
                    ++critical_threat;
                else if (a.threat.level == ThreatLevel::HIGH)
                    ++high_threat;
                if (a.sort_ts > latest_ts)
                    latest_ts = a.sort_ts;
            }

            int breaking_clusters = 0;
            if (!all.isEmpty()) {
                auto clusters = cluster_articles(all);
                breaking_clusters = static_cast<int>(get_breaking_clusters(clusters).size());
            }

            QJsonObject cat_json;
            for (auto it = categories.constBegin(); it != categories.constEnd(); ++it)
                cat_json[it.key()] = it.value();

            const bool live = svc->is_live_connected();
            const bool healthy = !all.isEmpty() && sources.size() >= 5;

            QJsonObject data{
                {"healthy", healthy},
                {"feeds_configured", svc->feed_count()},
                {"active_sources", static_cast<int>(sources.size())},
                {"cached_articles", all.size()},
                {"latest_article_ts", static_cast<qint64>(latest_ts)},
                {"breaking_clusters", breaking_clusters},
                {"high_threat_count", high_threat},
                {"critical_threat_count", critical_threat},
                {"live_websocket_connected", live},
                {"sentiment", QJsonObject{{"bullish", bullish}, {"bearish", bearish}, {"neutral", neutral}}},
                {"categories", cat_json}};

            const QString summary =
                healthy ? QString("OK: %1 articles from %2 sources").arg(all.size()).arg(sources.size())
                        : QString("DEGRADED: %1 articles from %2 sources — feeds may still be loading or blocked")
                              .arg(all.size())
                              .arg(sources.size());
            return ToolResult::ok(summary, data);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
