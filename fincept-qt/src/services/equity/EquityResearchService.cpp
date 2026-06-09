// src/services/equity/EquityResearchService.cpp
#include "services/equity/EquityResearchService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/DataSourceRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/HistoricalDataService.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <vector>

namespace fincept::services::equity {

namespace {

// yfinance symbol → broker region. ".NS"/".BO" are Indian (NSE/BSE); a bare
// ticker with no exchange suffix is a US listing (yfinance convention, e.g.
// AAPL/SPGI). Other suffixes (.L, .TO, .HK …) aren't routable via the wired
// brokers → empty region. Mirrors research_symbol_region() in the screen.
QString candle_symbol_region(const QString& sym) {
    if (sym.endsWith(QStringLiteral(".NS"), Qt::CaseInsensitive) ||
        sym.endsWith(QStringLiteral(".BO"), Qt::CaseInsensitive))
        return QStringLiteral("IN");
    if (!sym.contains(QLatin1Char('.')))
        return QStringLiteral("US");
    return {};
}

// yfinance form (RELIANCE.NS / TCS.BO) → bare broker symbol. US tickers are
// already bare.
QString candle_bare_symbol(const QString& sym) {
    if (sym.endsWith(QStringLiteral(".NS"), Qt::CaseInsensitive) ||
        sym.endsWith(QStringLiteral(".BO"), Qt::CaseInsensitive))
        return sym.left(sym.length() - 3);
    return sym;
}

// Pick the first *Connected* active account whose broker region matches the
// symbol's market, so RELIANCE.NS routes through a live Indian broker and a US
// ticker through a live US broker — never cross-region. Returns false when no
// usable broker exists (caller then uses yfinance).
bool resolve_connected_data_broker(const QString& symbol, QString& broker_id, QString& account_id) {
    const QString region = candle_symbol_region(symbol);
    if (region.isEmpty())
        return false;
    const auto accounts = fincept::trading::AccountManager::instance().active_accounts();
    for (const auto& a : accounts) {
        if (a.state != fincept::trading::ConnectionState::Connected)
            continue;
        auto* b = fincept::trading::BrokerRegistry::instance().get(a.broker_id);
        if (b && b->profile().region == region) {
            broker_id = a.broker_id;
            account_id = a.account_id;
            return true;
        }
    }
    return false;
}

// yfinance period string ("2y"/"1y"/"6mo"/"3mo"/"1mo"/"5d"/"max") → lookback
// days for the broker history request. Defaults to ~2y (good indicator warm-up).
int candle_lookback_days(const QString& period) {
    const QString p = period.trimmed().toLower();
    if (p == QLatin1String("max"))
        return 3650;
    auto num = [&](int trim) { return p.left(p.size() - trim).toInt(); };
    if (p.endsWith(QLatin1String("mo"))) {
        int n = num(2);
        if (n > 0)
            return n * 31;
    } else if (p.endsWith(QLatin1Char('y'))) {
        int n = num(1);
        if (n > 0)
            return n * 365;
    } else if (p.endsWith(QLatin1String("wk"))) {
        int n = num(2);
        if (n > 0)
            return n * 7;
    } else if (p.endsWith(QLatin1Char('d'))) {
        int n = num(1);
        if (n > 0)
            return n;
    }
    return 730;
}

// trading-layer BrokerCandle[] → the yfinance-shaped JSON array the indicator
// scripts consume: [{timestamp(sec), open, high, low, close, volume}, …], ascending.
// Timestamps are normalised to epoch SECONDS (some brokers, e.g. Fyers, return
// milliseconds — the chart and the indicator engine assume seconds).
QString broker_candles_to_json(const QVector<fincept::trading::BrokerCandle>& candles) {
    struct Row {
        qint64 ts;
        const fincept::trading::BrokerCandle* c;
    };
    std::vector<Row> rows;
    rows.reserve(static_cast<size_t>(candles.size()));
    for (const auto& c : candles) {
        qint64 ts = c.timestamp;
        if (ts > 1000000000000LL) // > ~Sep 2001 in ms → milliseconds, convert to seconds
            ts /= 1000;
        if (ts <= 0)
            continue;
        rows.push_back({ts, &c});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.ts < b.ts; });

    QJsonArray arr;
    for (const auto& r : rows) {
        QJsonObject o;
        o["timestamp"] = static_cast<double>(r.ts);
        o["open"] = r.c->open;
        o["high"] = r.c->high;
        o["low"] = r.c->low;
        o["close"] = r.c->close;
        o["volume"] = r.c->volume;
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

} // namespace

// ── Singleton ─────────────────────────────────────────────────────────────────
EquityResearchService& EquityResearchService::instance() {
    static EquityResearchService inst;
    return inst;
}

EquityResearchService::EquityResearchService(QObject* parent) : QObject(parent) {
    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(kDebounceMs);
    connect(search_debounce_, &QTimer::timeout, this, [this]() { search_symbols(pending_query_); });
}

// ── Python helper ─────────────────────────────────────────────────────────────
void EquityResearchService::run_python(const QString& script, const QStringList& args,
                                       std::function<void(bool, const QString&)> cb) {
    QPointer<EquityResearchService> self = this;
    python::PythonRunner::instance().run(script, args, [self, cb](python::PythonResult result) {
        if (!self)
            return;
        cb(result.success, result.success ? result.output : result.error);
    });
}

// ── Search ────────────────────────────────────────────────────────────────────
void EquityResearchService::schedule_search(const QString& query) {
    pending_query_ = query;
    search_debounce_->start();
}

void EquityResearchService::search_symbols(const QString& query) {
    if (query.trimmed().isEmpty())
        return;
    run_python("yfinance_data.py", {"search", query, "20"}, [this](bool ok, const QString& out) {
        if (!ok) {
            LOG_WARN("EquityResearch", "Symbol search failed");
            return;
        }
        auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
        auto arr = doc.object()["results"].toArray();
        QVector<SearchResult> results;
        results.reserve(arr.size());
        for (const auto& v : arr) {
            auto o = v.toObject();
            SearchResult r;
            r.symbol = o["symbol"].toString();
            r.name = o["name"].toString();
            r.exchange = o["exchange"].toString();
            r.type = o["type"].toString();
            r.currency = o["currency"].toString();
            r.industry = o["industry"].toString();
            if (!r.symbol.isEmpty())
                results.append(r);
        }
        emit search_results_loaded(results);
    });
}

// ── Load symbol (quote + info + historical in parallel) ───────────────────────
void EquityResearchService::load_quote_only(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    const QVariant qcv = fincept::CacheManager::instance().get("equity:quote:" + symbol);
    if (!qcv.isNull()) {
        emit quote_loaded(parse_quote(QJsonDocument::fromJson(qcv.toString().toUtf8()).object()));
        return;
    }
    run_python("yfinance_data.py", {"quote", symbol}, [this, symbol](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("Quote", "Failed to fetch quote for " + symbol);
            return;
        }
        auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
        if (obj.contains("error")) {
            emit error_occurred("Quote", obj["error"].toString());
            return;
        }
        fincept::CacheManager::instance().put(
            "equity:quote:" + symbol,
            QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))), kQuoteTtlSec,
            "equity");
        emit quote_loaded(parse_quote(obj));
    });
}

void EquityResearchService::load_info_only(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    const QVariant icv = fincept::CacheManager::instance().get("equity:info:" + symbol);
    if (!icv.isNull()) {
        emit info_loaded(parse_info(QJsonDocument::fromJson(icv.toString().toUtf8()).object()));
        return;
    }
    run_python("yfinance_data.py", {"info", symbol}, [this, symbol](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("Info", "Failed to fetch info for " + symbol);
            return;
        }
        auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
        if (obj.contains("error")) {
            emit error_occurred("Info", obj["error"].toString());
            return;
        }
        fincept::CacheManager::instance().put(
            "equity:info:" + symbol,
            QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))), kInfoTtlSec,
            "equity");
        emit info_loaded(parse_info(obj));
    });
}

void EquityResearchService::load_historical_only(const QString& symbol, const QString& period) {
    if (symbol.isEmpty())
        return;
    const QString cache_key = "equity:candles:" + symbol + ":" + period;
    const QVariant hcv = fincept::CacheManager::instance().get(cache_key);
    if (!hcv.isNull()) {
        auto arr = QJsonDocument::fromJson(hcv.toString().toUtf8()).array();
        if (!arr.isEmpty()) {
            emit historical_loaded(symbol, parse_candles(arr));
            return;
        }
    }
    run_python("yfinance_data.py", {"historical_period", symbol, period, "1d"},
               [this, symbol, cache_key](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("Historical", "Failed to fetch historical for " + symbol);
                       return;
                   }
                   auto arr = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).array();
                   if (!arr.isEmpty()) {
                       fincept::CacheManager::instance().put(
                           cache_key,
                           QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                           kHistoricalTtlSec, "equity");
                   }
                   emit historical_loaded(symbol, parse_candles(arr));
               });
}

void EquityResearchService::load_symbol(const QString& symbol, const QString& period) {
    if (symbol.isEmpty())
        return;
    load_quote_only(symbol);
    load_info_only(symbol);
    load_historical_only(symbol, period);
}

// ── Financials ────────────────────────────────────────────────────────────────
void EquityResearchService::fetch_financials(const QString& symbol) {
    run_python("yfinance_data.py", {"financials", symbol}, [this, symbol](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("Financials", "Failed to fetch financials for " + symbol);
            return;
        }
        auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
        if (obj.contains("error")) {
            emit error_occurred("Financials", obj["error"].toString());
            return;
        }
        emit financials_loaded(parse_financials(obj));
    });
}

// ── Technicals ────────────────────────────────────────────────────────────────
void EquityResearchService::fetch_technicals(const QString& symbol, const QString& period) {
    // Candles come from ensure_candles (cache → region-matched connected broker →
    // yfinance fallback); compute_technicals.py then derives the rating + indicators.
    auto run_compute = [this, symbol](const QString& hist_json) {
        run_python("compute_technicals.py", {hist_json}, [this, symbol](bool ok2, const QString& out2) {
            if (!ok2) {
                emit error_occurred("Technicals", "Indicator computation failed");
                return;
            }
            auto doc = QJsonDocument::fromJson(python::extract_json(out2).toUtf8()).object();
            if (!doc["success"].toBool()) {
                emit error_occurred("Technicals", "compute_technicals returned failure");
                return;
            }
            emit technicals_loaded(parse_technicals(symbol, doc["data"].toArray()));
        });
    };

    QPointer<EquityResearchService> self = this;
    ensure_candles(symbol, period, [self, run_compute](bool ok, const QString& hist_json) {
        if (!self)
            return;
        if (!ok || hist_json.trimmed().isEmpty()) {
            emit self->error_occurred("Technicals", "Failed historical fetch");
            return;
        }
        run_compute(hist_json);
    });
}

// ── Peers ─────────────────────────────────────────────────────────────────────
void EquityResearchService::fetch_peers(const QString& symbol, const QStringList& peer_symbols) {
    QStringList all;
    all << symbol;
    all.append(peer_symbols);
    QString joined = all.join(",");

    run_python("yfinance_data.py", {"multiple_ratios", joined}, [this](bool ok, const QString& out) {
        if (!ok) {
            emit error_occurred("Peers", "Failed to fetch peer data");
            return;
        }
        auto arr = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).array();
        emit peers_loaded(parse_peers(arr));
    });
}

// ── News ──────────────────────────────────────────────────────────────────────
// Turn a yfinance company_name into a Google-News-friendly query by dropping the
// trailing corporate suffix(es) and punctuation — "Reliance Industries Limited" →
// "Reliance Industries", "Tesla, Inc." → "Tesla". Materially improves GNews recall.
static QString gnews_query_from_company_name(const QString& company_name) {
    static const QStringList kSuffixes = {"Limited", "Ltd",  "Incorporated", "Inc",     "Corporation", "Corp",
                                          "Company", "Co",    "PLC",          "Holdings", "Holding",     "Group",
                                          "SA",      "NV",    "AG",           "SE",       "SpA",         "AB"};
    QString q = company_name.trimmed();
    bool changed = true;
    while (changed) {
        changed = false;
        q = q.trimmed();
        while (q.endsWith(QLatin1Char(',')) || q.endsWith(QLatin1Char('.')))
            q.chop(1);
        for (const auto& suf : kSuffixes) {
            if (q.endsWith(QLatin1Char(' ') + suf, Qt::CaseInsensitive)) {
                q.chop(suf.size() + 1);
                changed = true;
                break;
            }
        }
    }
    return q.trimmed();
}

// Build the news search query for a symbol: the cleaned company name from cached
// info when available, else the bare ticker (.NS/.BO suffix stripped).
static QString news_query_for_symbol(const QString& symbol) {
    const bool indian = symbol.endsWith(QStringLiteral(".NS"), Qt::CaseInsensitive) ||
                        symbol.endsWith(QStringLiteral(".BO"), Qt::CaseInsensitive);
    QString query = indian ? symbol.left(symbol.length() - 3) : symbol;
    const QVariant icv = fincept::CacheManager::instance().get("equity:info:" + symbol);
    if (!icv.isNull()) {
        const auto info = QJsonDocument::fromJson(icv.toString().toUtf8()).object();
        const QString cleaned = gnews_query_from_company_name(info.value("company_name").toString());
        if (!cleaned.isEmpty())
            query = cleaned;
    }
    return query;
}

// NewsAPI /v2/everything query for a symbol. Builds an EXACT-PHRASE (quoted) match
// on the company name (ticker as fallback) so results are about THIS company.
// Paired with searchIn=title,description at the call site, this keeps the feed
// stock-specific — the default everything search also scans the full article body,
// so a bare word like "Apple"/"Reliance" otherwise pulled in unrelated general news.
static QString newsapi_query_for_symbol(const QString& symbol) {
    const bool indian = symbol.endsWith(QStringLiteral(".NS"), Qt::CaseInsensitive) ||
                        symbol.endsWith(QStringLiteral(".BO"), Qt::CaseInsensitive);
    const QString ticker = indian ? symbol.left(symbol.length() - 3) : symbol;
    QString company;
    const QVariant icv = fincept::CacheManager::instance().get("equity:info:" + symbol);
    if (!icv.isNull()) {
        const auto info = QJsonDocument::fromJson(icv.toString().toUtf8()).object();
        company = gnews_query_from_company_name(info.value("company_name").toString());
    }
    QString phrase = company;
    if (phrase.isEmpty())
        phrase = ticker;
    if (phrase.isEmpty())
        phrase = symbol;
    return QStringLiteral("\"%1\"").arg(phrase); // quotes → NewsAPI exact-phrase match
}

void EquityResearchService::fetch_news(const QString& symbol, int count, NewsProvider provider) {
    // NewsAPI.org provider (opt-in): use it only when a key is configured, and
    // cache it under a separate key so switching providers doesn't surface the
    // other provider's cached result.
    if (provider == NewsProvider::NewsApi) {
        const QString key = configured_newsapi_key();
        if (!key.isEmpty()) {
            const QVariant ncv = fincept::CacheManager::instance().get("equity:news:newsapi:" + symbol);
            if (!ncv.isNull()) {
                const QJsonArray arr = QJsonDocument::fromJson(ncv.toString().toUtf8()).array();
                emit news_loaded(symbol, parse_news(arr));
                return;
            }
            fetch_news_newsapi(symbol, count, key);
            return;
        }
        // No key configured → fall through to the default Auto chain below.
    }

    {
        const QVariant ncv = fincept::CacheManager::instance().get("equity:news:" + symbol);
        if (!ncv.isNull()) {
            const QJsonArray arr = QJsonDocument::fromJson(ncv.toString().toUtf8()).array();
            emit news_loaded(symbol, parse_news(arr));
            return;
        }
    }

    // Primary source: Google News (fetch_company_news.py) — broader, more current
    // company coverage than yfinance's Yahoo feed. Google News recall is far better
    // for a clean company NAME than a bare ticker or the full legal name (e.g.
    // "Reliance Industries" returns articles where "RELIANCE" and "Reliance
    // Industries Limited" both return none), so query by the cleaned company_name
    // from the cached info when available, falling back to the bare ticker.
    const bool indian = symbol.endsWith(QStringLiteral(".NS"), Qt::CaseInsensitive) ||
                        symbol.endsWith(QStringLiteral(".BO"), Qt::CaseInsensitive);
    const QString country = indian ? QStringLiteral("IN") : QStringLiteral("US");
    const QString query = news_query_for_symbol(symbol);

    run_python(
        "fetch_company_news.py",
        {"search", query, QString::number(count), QStringLiteral("1M"), QStringLiteral("en"), country},
        [this, symbol, count](bool ok, const QString& out) {
            if (ok) {
                const auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                // GNews returns {"success":true,"data":[...]}. Only accept a
                // non-empty result; anything else falls through to yfinance.
                if (obj.value("success").toBool()) {
                    const auto arr = obj.value("data").toArray();
                    if (!arr.isEmpty()) {
                        fincept::CacheManager::instance().put(
                            "equity:news:" + symbol,
                            QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))), kNewsTtlSec,
                            "equity");
                        emit news_loaded(symbol, parse_news(arr));
                        return;
                    }
                }
            }
            // Google News unavailable (e.g. gnews missing / network) or empty →
            // fall back to the Yahoo/yfinance feed.
            fetch_news_yfinance(symbol, count);
        });
}

void EquityResearchService::fetch_news_yfinance(const QString& symbol, int count) {
    run_python(
        "yfinance_data.py", {"news", symbol, QString::number(count)}, [this, symbol](bool ok, const QString& out) {
            if (!ok) {
                emit error_occurred("News", "Failed to fetch news for " + symbol);
                return;
            }
            auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
            auto arr = obj["articles"].toArray();
            fincept::CacheManager::instance().put(
                "equity:news:" + symbol, QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                kNewsTtlSec, "equity");
            emit news_loaded(symbol, parse_news(arr));
        });
}

// ── NewsAPI.org ─────────────────────────────────────────────────────────────
// The configured & enabled NewsAPI key from the Data Sources tab (provider id
// "newsapi", set by ConnectionConfigDialog as ds.provider = connector id). Empty
// when no enabled NewsAPI connection has a non-empty apiKey.
QString EquityResearchService::configured_newsapi_key() const {
    const auto res = fincept::DataSourceRepository::instance().list_all();
    if (res.is_err())
        return {};
    for (const auto& ds : res.value()) {
        if (!ds.enabled || ds.provider != QLatin1String("newsapi"))
            continue;
        const auto cfg = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        const QString key = cfg.value("apiKey").toString().trimmed();
        if (!key.isEmpty())
            return key;
    }
    return {};
}

// Fetch stock-specific news from newsapi.org/v2/everything. On any failure
// (network error, non-"ok" status such as 401/quota, or no usable articles) it
// falls back to the default Auto chain so the tab always shows news.
void EquityResearchService::fetch_news_newsapi(const QString& symbol, int count, const QString& api_key) {
    if (!news_nam_)
        news_nam_ = new QNetworkAccessManager(this);

    QUrl url(QStringLiteral("https://newsapi.org/v2/everything"));
    QUrlQuery params;
    // Exact-phrase company query restricted to title+description (NOT the full
    // article body) so results are about THIS company, not any article that
    // merely mentions the word. This is what makes the feed stock-specific.
    params.addQueryItem(QStringLiteral("q"), newsapi_query_for_symbol(symbol));
    params.addQueryItem(QStringLiteral("searchIn"), QStringLiteral("title,description"));
    params.addQueryItem(QStringLiteral("language"), QStringLiteral("en"));
    params.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("publishedAt"));
    params.addQueryItem(QStringLiteral("pageSize"), QString::number(std::clamp(count, 1, 100)));
    params.addQueryItem(QStringLiteral("from"), QDate::currentDate().addDays(-30).toString(Qt::ISODate));
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setRawHeader("X-Api-Key", api_key.toUtf8()); // key in header, not the URL
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FinceptTerminal"));

    QNetworkReply* reply = news_nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, symbol, count]() {
        reply->deleteLater();
        const auto fall_back = [this, symbol, count]() { fetch_news(symbol, count, NewsProvider::Auto); };

        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("EquityResearch",
                     QString("NewsAPI request failed (%1) — falling back").arg(reply->errorString()));
            fall_back();
            return;
        }
        const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (obj.value("status").toString() != QLatin1String("ok")) {
            LOG_WARN("EquityResearch",
                     QString("NewsAPI error: %1 — falling back").arg(obj.value("message").toString("unknown")));
            fall_back();
            return;
        }

        // Reshape NewsAPI articles into the {title, description, url, publisher,
        // published_date} shape parse_news() consumes.
        QJsonArray reshaped;
        for (const auto& v : obj.value("articles").toArray()) {
            const auto a = v.toObject();
            const QString title = a.value("title").toString();
            if (title.isEmpty() || title == QLatin1String("[Removed]"))
                continue;
            reshaped.append(QJsonObject{
                {"title", title},
                {"description", a.value("description").toString()},
                {"url", a.value("url").toString()},
                {"publisher", a.value("source").toObject().value("name").toString()},
                {"published_date", a.value("publishedAt").toString()},
            });
        }
        if (reshaped.isEmpty()) {
            fall_back(); // nothing usable from NewsAPI → Auto chain
            return;
        }
        fincept::CacheManager::instance().put(
            "equity:news:newsapi:" + symbol,
            QVariant(QString::fromUtf8(QJsonDocument(reshaped).toJson(QJsonDocument::Compact))), kNewsTtlSec, "equity");
        emit news_loaded(symbol, parse_news(reshaped));
    });
}

// ── Candle source: connected broker first, yfinance fallback ─────────────────
// Shared by fetch_technicals (and any future indicator path). Order:
//   1. cache hit                                 → return immediately
//   2. region-matched *connected* broker         → broker get_history (live, reliable)
//   3. yfinance                                  → fallback (no broker, or broker empty/failed)
// The result is cached under "equity:candles:<symbol>". done(ok, hist_json) is
// always invoked on the main thread.
void EquityResearchService::ensure_candles(const QString& symbol, const QString& period,
                                           std::function<void(bool, const QString&)> done) {
    const QString cache_key = "equity:candles:" + symbol;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        // Reject a cached empty payload ("" or "[]") — a stale rate-limited
        // result would otherwise wedge the tab on "No data returned".
        const QString c = cached.toString().trimmed();
        if (!c.isEmpty() && c != QLatin1String("[]")) {
            done(true, cached.toString());
            return;
        }
    }

    QPointer<EquityResearchService> self = this;
    auto run_yfinance = [self, symbol, period, cache_key, done]() {
        if (!self) {
            done(false, {});
            return;
        }
        self->run_python("yfinance_data.py", {"historical_period", symbol, period, "1d"},
                         [cache_key, done](bool ok, const QString& out) {
                             if (!ok) {
                                 done(false, {});
                                 return;
                             }
                             const QString raw = python::extract_json(out);
                             const QString trimmed = raw.trimmed();
                             if (trimmed.isEmpty() || trimmed == QLatin1String("[]")) {
                                 done(false, {}); // yfinance rate-limited / unknown symbol
                                 return;
                             }
                             fincept::CacheManager::instance().put(cache_key, QVariant(raw), kHistoricalTtlSec, "equity");
                             done(true, raw);
                         });
    };

    QString broker_id, account_id;
    if (resolve_connected_data_broker(symbol, broker_id, account_id)) {
        const QString bare = candle_bare_symbol(symbol);
        const int lookback = candle_lookback_days(period);
        LOG_INFO("EquityResearch",
                 QString("Candles for %1 via broker %2").arg(symbol, broker_id));
        fincept::trading::HistoricalDataService::instance().fetch(
            bare, QStringLiteral("1d"), lookback, broker_id, account_id,
            [cache_key, done, run_yfinance](bool ok, const QVector<fincept::trading::BrokerCandle>& candles,
                                            const QString& /*err*/) {
                if (ok && !candles.isEmpty()) {
                    const QString json = broker_candles_to_json(candles);
                    if (!json.isEmpty() && json != QLatin1String("[]")) {
                        fincept::CacheManager::instance().put(cache_key, QVariant(json), kHistoricalTtlSec, "equity");
                        done(true, json);
                        return;
                    }
                }
                run_yfinance(); // broker had no data — fall back
            });
        return;
    }

    run_yfinance(); // no connected region-matched broker
}

// ── Parsers ───────────────────────────────────────────────────────────────────
QuoteData EquityResearchService::parse_quote(const QJsonObject& o) const {
    QuoteData q;
    q.symbol = o["symbol"].toString();
    q.price = o["price"].toDouble();
    q.change = o["change"].toDouble();
    q.change_pct = o["change_percent"].toDouble();
    q.open = o["open"].toDouble();
    q.high = o["high"].toDouble();
    q.low = o["low"].toDouble();
    q.prev_close = o["previous_close"].toDouble();
    q.volume = o["volume"].toDouble();
    q.exchange = o["exchange"].toString();
    q.timestamp = static_cast<qint64>(o["timestamp"].toDouble());
    return q;
}

StockInfo EquityResearchService::parse_info(const QJsonObject& o) const {
    StockInfo s;
    s.symbol = o["symbol"].toString();
    s.company_name = o["company_name"].toString();
    s.sector = o["sector"].toString();
    s.industry = o["industry"].toString();
    s.description = o["description"].toString();
    s.website = o["website"].toString();
    s.country = o["country"].toString();
    s.currency = o["currency"].toString();
    s.exchange = o["exchange"].toString();
    s.employees = o["employees"].toInt();

    s.market_cap = o["market_cap"].toDouble();
    s.enterprise_value = o["enterprise_value"].toDouble();
    s.pe_ratio = o["pe_ratio"].toDouble();
    s.forward_pe = o["forward_pe"].toDouble();
    s.peg_ratio = o["peg_ratio"].toDouble();
    s.price_to_book = o["price_to_book"].toDouble();
    s.ev_to_revenue = o["enterprise_to_revenue"].toDouble();
    s.ev_to_ebitda = o["enterprise_to_ebitda"].toDouble();

    s.gross_margins = o["gross_margins"].toDouble();
    s.operating_margins = o["operating_margins"].toDouble();
    s.ebitda_margins = o["ebitda_margins"].toDouble();
    s.profit_margins = o["profit_margins"].toDouble();
    s.roe = o["return_on_equity"].toDouble();
    s.roa = o["return_on_assets"].toDouble();
    s.gross_profits = o["gross_profits"].toDouble();

    s.book_value = o["book_value"].toDouble();
    s.revenue_per_share = o["revenue_per_share"].toDouble();
    s.free_cashflow = o["free_cashflow"].toDouble();
    s.operating_cashflow = o["operating_cashflow"].toDouble();
    s.total_cash = o["total_cash"].toDouble();
    s.total_debt = o["total_debt"].toDouble();
    s.total_revenue = o["total_revenue"].toDouble();

    s.earnings_growth = o["earnings_growth"].toDouble();
    s.revenue_growth = o["revenue_growth"].toDouble();

    s.shares_outstanding = o["shares_outstanding"].toDouble();
    s.float_shares = o["float_shares"].toDouble();
    s.held_insiders_pct = o["held_percent_insiders"].toDouble();
    s.held_institutions_pct = o["held_percent_institutions"].toDouble();
    s.short_ratio = o["short_ratio"].toDouble();
    s.short_pct_of_float = o["short_percent_of_float"].toDouble();

    s.week52_high = o["fifty_two_week_high"].toDouble();
    s.week52_low = o["fifty_two_week_low"].toDouble();
    s.avg_volume = o["average_volume"].toDouble();
    s.beta = o["beta"].toDouble();
    s.dividend_yield = o["dividend_yield"].toDouble();
    s.current_price = o["current_price"].toDouble();

    s.target_high = o["target_high_price"].toDouble();
    s.target_low = o["target_low_price"].toDouble();
    s.target_mean = o["target_mean_price"].toDouble();
    s.recommendation_mean = o["recommendation_mean"].toDouble();
    s.recommendation_key = o["recommendation_key"].toString();
    s.analyst_count = o["number_of_analyst_opinions"].toInt();
    return s;
}

QVector<Candle> EquityResearchService::parse_candles(const QJsonArray& arr) const {
    QVector<Candle> candles;
    candles.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        Candle c;
        c.timestamp = static_cast<qint64>(o["timestamp"].toDouble());
        c.open = o["open"].toDouble();
        c.high = o["high"].toDouble();
        c.low = o["low"].toDouble();
        c.close = o["close"].toDouble();
        c.volume = static_cast<qint64>(o["volume"].toDouble());
        candles.append(c);
    }
    return candles;
}

FinancialsData EquityResearchService::parse_financials(const QJsonObject& obj) const {
    FinancialsData fd;
    fd.symbol = obj["symbol"].toString();

    auto parse_stmt = [](const QJsonObject& stmt) {
        QVector<QPair<QString, QJsonObject>> result;
        for (auto it = stmt.constBegin(); it != stmt.constEnd(); ++it)
            result.append({it.key(), it.value().toObject()});
        // Sort by period descending (most recent first)
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        return result;
    };

    fd.income_statement = parse_stmt(obj["income_statement"].toObject());
    fd.balance_sheet = parse_stmt(obj["balance_sheet"].toObject());
    fd.cash_flow = parse_stmt(obj["cash_flow"].toObject());
    return fd;
}

TechSignal EquityResearchService::score_indicator(const QString& name, double value, double sma20, double sma50) const {
    // RSI (0-100): <=30 buy, >=70 sell
    if (name == "rsi") {
        if (value <= 25)
            return TechSignal::StrongBuy;
        if (value <= 40)
            return TechSignal::Buy;
        if (value <= 60)
            return TechSignal::Neutral;
        if (value <= 75)
            return TechSignal::Sell;
        return TechSignal::StrongSell;
    }
    // MACD: positive histogram = buy
    if (name == "macd") {
        return value > 0 ? TechSignal::Buy : TechSignal::Sell;
    }
    // SMA crossover
    if (name == "sma_20" && sma50 > 0) {
        if (sma20 > sma50 * 1.02)
            return TechSignal::StrongBuy;
        if (sma20 > sma50)
            return TechSignal::Buy;
        if (sma20 < sma50 * 0.98)
            return TechSignal::StrongSell;
        if (sma20 < sma50)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // CCI: <=-100 buy, >=100 sell
    if (name == "cci") {
        if (value <= -100)
            return TechSignal::StrongBuy;
        if (value <= -50)
            return TechSignal::Buy;
        if (value >= 100)
            return TechSignal::StrongSell;
        if (value >= 50)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // MFI (0-100): <=20 buy, >=80 sell
    if (name == "mfi") {
        if (value <= 20)
            return TechSignal::StrongBuy;
        if (value <= 40)
            return TechSignal::Buy;
        if (value >= 80)
            return TechSignal::StrongSell;
        if (value >= 60)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // Stochastic %K and %D
    if (name == "stoch_k" || name == "stoch_d") {
        if (value <= 20)
            return TechSignal::StrongBuy;
        if (value <= 40)
            return TechSignal::Buy;
        if (value >= 80)
            return TechSignal::StrongSell;
        if (value >= 60)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // Williams %R (-100 to 0): <=-80 buy, >=-20 sell
    if (name == "williams_r") {
        if (value <= -80)
            return TechSignal::StrongBuy;
        if (value <= -60)
            return TechSignal::Buy;
        if (value >= -20)
            return TechSignal::StrongSell;
        if (value >= -40)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // ROC: positive = buy
    if (name == "roc") {
        if (value > 5)
            return TechSignal::Buy;
        if (value < -5)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // ADX: >25 = trending (buy signal), <20 = weak
    if (name == "adx") {
        return value > 25 ? TechSignal::Buy : TechSignal::Neutral;
    }
    // BB %B: <0.2 oversold (buy), >0.8 overbought (sell)
    if (name == "bb_pband") {
        if (value < 0.2)
            return TechSignal::StrongBuy;
        if (value > 0.8)
            return TechSignal::StrongSell;
        return TechSignal::Neutral;
    }
    // CMF: >0.1 buy, <-0.1 sell
    if (name == "cmf") {
        if (value > 0.1)
            return TechSignal::Buy;
        if (value < -0.1)
            return TechSignal::Sell;
        return TechSignal::Neutral;
    }
    // OBV: use trend direction — positive change from context = neutral (no ref price)
    // Aroon Up/Down: aroon_up > aroon_down = buy
    if (name == "aroon_up") {
        return value > 50 ? TechSignal::Buy : TechSignal::Neutral;
    }
    if (name == "aroon_down") {
        return value > 50 ? TechSignal::Sell : TechSignal::Neutral;
    }
    return TechSignal::Neutral;
}

TechnicalsData EquityResearchService::parse_technicals(const QString& symbol, const QJsonArray& rows) const {
    TechnicalsData td;
    td.symbol = symbol;
    if (rows.isEmpty())
        return td;

    // Use the last row (most recent values) — compute_technicals.py outputs lowercase snake_case
    auto last = rows.last().toObject();

    // Extract SMA values for cross scoring
    double sma20 = last["sma_20"].toDouble();
    double sma50 = last["sma_50"].toDouble(); // may be 0 if not present

    // Maps: display name → actual column key in JSON output
    // Trend: sma_20, sma_50, ema_12, wma_9, macd, macd_signal, cci, adx, aroon_up, aroon_down
    static const QList<QPair<QString, QString>> kTrend = {
        {"SMA 20", "sma_20"},     {"SMA 50", "sma_50"},           {"EMA 12", "ema_12"}, {"WMA 9", "wma_9"},
        {"MACD", "macd"},         {"MACD Signal", "macd_signal"}, {"CCI", "cci"},       {"ADX", "adx"},
        {"Aroon Up", "aroon_up"}, {"Aroon Down", "aroon_down"},
    };
    // Momentum: rsi, stoch_k, stoch_d, williams_r, roc, mfi, ao, kama
    static const QList<QPair<QString, QString>> kMomentum = {
        {"RSI", "rsi"}, {"Stoch %K", "stoch_k"}, {"Stoch %D", "stoch_d"}, {"Williams %R", "williams_r"},
        {"ROC", "roc"}, {"MFI", "mfi"},          {"Awesome Osc", "ao"},   {"KAMA", "kama"},
    };
    // Volatility: atr, bb_mavg, bb_hband, bb_lband, bb_pband, bb_wband
    static const QList<QPair<QString, QString>> kVolatility = {
        {"ATR", "atr"},           {"BB Mid", "bb_mavg"}, {"BB Upper", "bb_hband"},
        {"BB Lower", "bb_lband"}, {"BB %B", "bb_pband"}, {"BB Width", "bb_wband"},
    };
    // Volume: obv, vwap, mfi (mfi also in momentum), cmf, adi
    static const QList<QPair<QString, QString>> kVolume = {
        {"OBV", "obv"},
        {"VWAP", "vwap"},
        {"CMF", "cmf"},
        {"ADI", "adi"},
    };

    auto build = [&](const QList<QPair<QString, QString>>& keys, const QString& cat) {
        QVector<TechIndicator> out;
        for (const auto& kv : keys) {
            const QString& col = kv.second;
            if (!last.contains(col))
                continue;
            auto jval = last[col];
            if (jval.isNull() || jval.isUndefined())
                continue;
            double val = jval.toDouble();
            if (std::isnan(val))
                continue;
            TechIndicator ti;
            ti.name = kv.first;
            ti.value = val;
            ti.category = cat;
            ti.signal = score_indicator(col, val, sma20, sma50);
            out.append(ti);
        }
        return out;
    };

    td.trend = build(kTrend, "trend");
    td.momentum = build(kMomentum, "momentum");
    td.volatility = build(kVolatility, "volatility");
    td.volume = build(kVolume, "volume");

    // Tally signals
    auto tally = [&](const QVector<TechIndicator>& v) {
        for (const auto& ti : v) {
            switch (ti.signal) {
                case TechSignal::StrongBuy:
                    td.strong_buy++;
                    break;
                case TechSignal::Buy:
                    td.buy++;
                    break;
                case TechSignal::Neutral:
                    td.neutral++;
                    break;
                case TechSignal::Sell:
                    td.sell++;
                    break;
                case TechSignal::StrongSell:
                    td.strong_sell++;
                    break;
            }
        }
    };
    tally(td.trend);
    tally(td.momentum);
    tally(td.volatility);
    tally(td.volume);

    int bulls = td.strong_buy * 2 + td.buy;
    int bears = td.strong_sell * 2 + td.sell;
    if (bulls > bears * 2)
        td.overall_signal = TechSignal::StrongBuy;
    else if (bulls > bears)
        td.overall_signal = TechSignal::Buy;
    else if (bears > bulls * 2)
        td.overall_signal = TechSignal::StrongSell;
    else if (bears > bulls)
        td.overall_signal = TechSignal::Sell;
    else
        td.overall_signal = TechSignal::Neutral;

    return td;
}

QVector<PeerData> EquityResearchService::parse_peers(const QJsonArray& arr) const {
    QVector<PeerData> peers;
    for (const auto& v : arr) {
        auto o = v.toObject();
        PeerData p;
        p.symbol = o["symbol"].toString();
        p.pe_ratio = o["peRatio"].toDouble();
        p.forward_pe = o["forwardPE"].toDouble();
        p.price_to_book = o["priceToBook"].toDouble();
        p.price_to_sales = o["priceToSales"].toDouble();
        p.peg_ratio = o["pegRatio"].toDouble();
        p.debt_to_equity = o["debtToEquity"].toDouble();
        p.roe = o["returnOnEquity"].toDouble();
        p.roa = o["returnOnAssets"].toDouble();
        p.profit_margin = o["profitMargin"].toDouble();
        p.operating_margin = o["operatingMargin"].toDouble();
        p.gross_margin = o["grossMargin"].toDouble();
        p.current_ratio = o["currentRatio"].toDouble();
        p.quick_ratio = o["quickRatio"].toDouble();
        p.dividend_yield = o["dividendYield"].toDouble();
        peers.append(p);
    }
    return peers;
}

QVector<NewsArticle> EquityResearchService::parse_news(const QJsonArray& arr) const {
    QVector<NewsArticle> articles;
    articles.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        NewsArticle a;
        a.title = o["title"].toString();
        a.description = o["description"].toString();
        a.url = o["url"].toString();
        a.publisher = o["publisher"].toString();
        a.published_date = o["published_date"].toString();
        if (!a.title.isEmpty())
            articles.append(a);
    }
    return articles;
}

} // namespace fincept::services::equity

