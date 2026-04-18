#include "services/markets/MarketDataService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"
#    include "datahub/TopicPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSet>

#include <memory>

namespace fincept::services {

MarketDataService& MarketDataService::instance() {
    static MarketDataService s;
    return s;
}

MarketDataService::MarketDataService() {}


// ── DataHub Producer integration ────────────────────────────────────────────

QStringList MarketDataService::topic_patterns() const {
    return {QStringLiteral("market:quote:*"), QStringLiteral("market:sparkline:*"),
            QStringLiteral("market:history:*")};
}

int MarketDataService::max_requests_per_sec() const {
    // PythonRunner caps at 3 concurrent processes; a batched-quote fetch
    // is 2–3 s typical. Capping hub-driven refreshes at 2/s keeps at
    // least one PythonRunner slot for non-quote work.
    return 2;
}

void MarketDataService::refresh(const QStringList& topics) {
    // Hub guarantees `topics` all match one of our patterns. Route each
    // topic family to the matching fetcher.
    static const QString kQuote = QStringLiteral("market:quote:");
    static const QString kSpark = QStringLiteral("market:sparkline:");
    static const QString kHist  = QStringLiteral("market:history:");

    QStringList quote_syms;
    QStringList spark_syms;
    QStringList hist_topics;  // keep raw — history is parameterised by period/interval
    for (const auto& t : topics) {
        if (t.startsWith(kQuote))
            quote_syms.append(t.mid(kQuote.size()));
        else if (t.startsWith(kSpark))
            spark_syms.append(t.mid(kSpark.size()));
        else if (t.startsWith(kHist))
            hist_topics.append(t);
    }

    if (!quote_syms.isEmpty()) {
        LOG_INFO("DataHub", QString("refresh market:quote batch=%1").arg(quote_syms.size()));
        // Routed through batched fetch path. Per-symbol publish happens in
        // flush_batch() so consumers see each result as soon as it resolves.
        fetch_quotes(quote_syms, [](bool, QVector<QuoteData>) {});
    }

    if (!spark_syms.isEmpty()) {
        LOG_INFO("DataHub", QString("refresh market:sparkline batch=%1").arg(spark_syms.size()));
        // Sparkline fetcher returns QHash<symbol, prices>; fan out to hub.
        QPointer<MarketDataService> self = this;
        fetch_sparklines(spark_syms, [self](bool ok, QHash<QString, QVector<double>> data) {
            if (!self || !ok)
                return;
            for (auto it = data.cbegin(); it != data.cend(); ++it)
                self->publish_sparkline_to_hub(it.key(), it.value());
        });
    }

    if (!hist_topics.isEmpty()) {
        // History topics are parameterised: `market:history:<sym>:<period>:<interval>`.
        // Dispatch one fetch per unique topic.
        LOG_INFO("DataHub", QString("refresh market:history batch=%1").arg(hist_topics.size()));
        QPointer<MarketDataService> self = this;
        for (const QString& topic : hist_topics) {
            // Strip the prefix and split "<sym>:<period>:<interval>".
            const QString tail = topic.mid(kHist.size());
            const QStringList parts = tail.split(QLatin1Char(':'));
            if (parts.size() != 3)
                continue;
            const QString sym = parts.at(0);
            const QString period = parts.at(1);
            const QString interval = parts.at(2);
            fetch_history(sym, period, interval,
                          [self, sym, period, interval](bool ok, QVector<HistoryPoint> points) {
                              if (!self || !ok)
                                  return;
                              self->publish_history_to_hub(sym, period, interval, points);
                          });
        }
    }
}

void MarketDataService::publish_quote_to_hub(const QuoteData& q) {
    datahub::DataHub::instance().publish(
        QStringLiteral("market:quote:") + q.symbol,
        QVariant::fromValue(q));
}

void MarketDataService::publish_history_to_hub(const QString& symbol, const QString& period,
                                               const QString& interval,
                                               const QVector<HistoryPoint>& points) {
    const QString topic = QStringLiteral("market:history:") + symbol + QLatin1Char(':') + period +
                          QLatin1Char(':') + interval;
    datahub::DataHub::instance().publish(topic, QVariant::fromValue(points));
}

void MarketDataService::publish_sparkline_to_hub(const QString& symbol, const QVector<double>& points) {
    datahub::DataHub::instance().publish(QStringLiteral("market:sparkline:") + symbol,
                                         QVariant::fromValue(points));
}

void MarketDataService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = datahub::DataHub::instance();
    hub.register_producer(this);

    // Quotes: 30s TTL, 5s min interval (matches Phase 2 pilot).
    datahub::TopicPolicy quote_p;
    quote_p.ttl_ms = 30'000;
    quote_p.min_interval_ms = 5'000;
    hub.set_policy_pattern(QStringLiteral("market:quote:*"), quote_p);

    // Sparklines: 5-day hourly data, changes slowly — cache 10 minutes, refresh at most
    // every 60s so a dashboard with 20 holdings doesn't hammer the sparkline script.
    datahub::TopicPolicy spark_p;
    spark_p.ttl_ms = 10 * 60'000;
    spark_p.min_interval_ms = 60'000;
    hub.set_policy_pattern(QStringLiteral("market:sparkline:*"), spark_p);

    // History: OHLCV series. One-shot per chart; policies are conservative to
    // avoid re-fetching for every open of the same chart. 30 min TTL, 5 min
    // min-interval so a user flipping periods still triggers a fresh fetch.
    datahub::TopicPolicy hist_p;
    hist_p.ttl_ms = 30 * 60'000;
    hist_p.min_interval_ms = 5 * 60'000;
    hub.set_policy_pattern(QStringLiteral("market:history:*"), hist_p);

    hub_registered_ = true;
    LOG_INFO("DataHub",
             "MarketDataService registered as producer for market:quote:*, market:sparkline:*, market:history:*");
}


// ── Batched + Cached fetch_quotes ───────────────────────────────────────────

void MarketDataService::fetch_quotes(const QStringList& symbols, QuoteCallback cb) {
    if (symbols.isEmpty()) {
        cb(true, {});
        return;
    }

    // Check if ALL requested symbols are cached and fresh
    bool all_cached = true;
    QVector<QuoteData> cached_results;
    for (const auto& sym : symbols) {
        const QVariant cv = fincept::CacheManager::instance().get("market:" + sym);
        if (!cv.isNull()) {
            const QJsonObject o = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
            cached_results.append({o["symbol"].toString(), o["name"].toString(), o["price"].toDouble(),
                                   o["change"].toDouble(), o["change_pct"].toDouble(), o["high"].toDouble(),
                                   o["low"].toDouble(), o["volume"].toDouble()});
        } else {
            all_cached = false;
            break;
        }
    }
    if (all_cached) {
        cb(true, cached_results);
        return;
    }

    // Queue for batching
    pending_.append({symbols, std::move(cb)});

    if (!batch_scheduled_) {
        batch_scheduled_ = true;
        QTimer::singleShot(100, this, &MarketDataService::flush_batch);
    }
}

void MarketDataService::flush_batch() {
    batch_scheduled_ = false;

    if (pending_.isEmpty())
        return;

    // Collect all unique symbols from pending requests
    QSet<QString> all_symbols_set;
    for (const auto& req : pending_) {
        for (const auto& sym : req.symbols) {
            all_symbols_set.insert(sym);
        }
    }
    QStringList all_symbols = all_symbols_set.values();

    // Take ownership of pending callbacks
    auto requests = std::move(pending_);
    pending_.clear();

    LOG_INFO("MarketData",
             QString("Batch fetch: %1 unique symbols from %2 requests").arg(all_symbols.size()).arg(requests.size()));

    QStringList args;
    args << "batch_quotes";
    args.append(all_symbols);

    python::PythonRunner::instance().run(
        "yfinance_data.py", args, [this, requests = std::move(requests)](python::PythonResult result) {
            QVector<QuoteData> all_quotes;

            if (result.success) {
                auto doc = QJsonDocument::fromJson(result.output.toUtf8());

                auto parse_quote = [](const QJsonObject& q) -> QuoteData {
                    return {q["symbol"].toString(),
                            q["name"].toString(q["symbol"].toString()),
                            q["price"].toDouble(),
                            q["change"].toDouble(),
                            q["change_percent"].toDouble(),
                            q["high"].toDouble(),
                            q["low"].toDouble(),
                            q["volume"].toDouble()};
                };

                auto store_quote = [](const QuoteData& q) {
                    QJsonObject o;
                    o["symbol"] = q.symbol;
                    o["name"] = q.name;
                    o["price"] = q.price;
                    o["change"] = q.change;
                    o["change_pct"] = q.change_pct;
                    o["high"] = q.high;
                    o["low"] = q.low;
                    o["volume"] = q.volume;
                    fincept::CacheManager::instance().put(
                        "market:" + q.symbol,
                        QVariant(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact))), kQuoteCacheTtlSec,
                        "market_data");
                };

                if (doc.isArray()) {
                    for (const auto& v : doc.array()) {
                        auto q = v.toObject();
                        if (q.contains("error"))
                            continue;
                        auto quote = parse_quote(q);
                        all_quotes.append(quote);
                        store_quote(quote);
                        publish_quote_to_hub(quote);
                    }
                } else if (doc.isObject()) {
                    auto obj = doc.object();
                    if (obj.contains("symbol") && !obj.contains("error")) {
                        auto quote = parse_quote(obj);
                        all_quotes.append(quote);
                        store_quote(quote);
                        publish_quote_to_hub(quote);
                    }
                }

                LOG_INFO("MarketData", QString("Fetched %1 quotes (cached)").arg(all_quotes.size()));
            } else {
                LOG_WARN("MarketData", "Batch fetch failed: " + result.error);
            }

            // Fan out results to each waiting callback, filtered to their requested symbols
            for (const auto& req : requests) {
                if (!result.success) {
                    // On failure, try to serve from stale cache (ignoring TTL)
                    QVector<QuoteData> stale;
                    for (const auto& sym : req.symbols) {
                        const QVariant cv = fincept::CacheManager::instance().get("market:" + sym);
                        if (!cv.isNull()) {
                            const QJsonObject o = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
                            stale.append({o["symbol"].toString(), o["name"].toString(), o["price"].toDouble(),
                                          o["change"].toDouble(), o["change_pct"].toDouble(), o["high"].toDouble(),
                                          o["low"].toDouble(), o["volume"].toDouble()});
                        }
                    }
                    req.cb(!stale.isEmpty(), stale);
                    continue;
                }

                QSet<QString> wanted(req.symbols.begin(), req.symbols.end());
                QVector<QuoteData> filtered;
                for (const auto& q : all_quotes) {
                    if (wanted.contains(q.symbol)) {
                        filtered.append(q);
                    }
                }
                req.cb(true, filtered);
            }
        });
}

// ── News fetch (unchanged) ──────────────────────────────────────────────────

void MarketDataService::fetch_news(const QString& symbol, int count, NewsCallback cb) {
    QStringList args;
    args << "news" << symbol << QString::number(count);

    python::PythonRunner::instance().run("yfinance_data.py", args, [cb](python::PythonResult result) {
        if (!result.success) {
            LOG_WARN("MarketData", "News fetch failed: " + result.error);
            cb(false, {});
            return;
        }

        auto doc = QJsonDocument::fromJson(result.output.toUtf8());
        if (doc.isObject() && doc.object().contains("articles")) {
            cb(true, doc.object()["articles"].toArray());
        } else {
            cb(false, {});
        }
    });
}

// ── Info fetch (company fundamentals) ───────────────────────────────────────

void MarketDataService::fetch_info(const QString& symbol, InfoCallback cb) {
    // Use financial_ratios command — it has all ratio fields in one call
    // and falls back gracefully. We run two parallel Python calls and merge:
    // 1) get_info  → name, sector, market cap, 52W range, beta, avg volume
    // 2) financial_ratios → P/E, P/B, dividend yield, ROE, margins, D/E, current ratio

    struct Partial {
        InfoData info;
        bool info_done = false;
        bool ratios_done = false;
        bool info_ok = false;
        bool ratios_ok = false;
    };
    auto shared = std::make_shared<Partial>();

    auto try_complete = [cb, shared, symbol]() {
        if (!shared->info_done || !shared->ratios_done)
            return;
        bool ok = shared->info_ok || shared->ratios_ok;
        if (ok)
            LOG_INFO("MarketData", "Fetched info for " + symbol);
        else
            LOG_WARN("MarketData", "Both info+ratios failed for " + symbol);
        cb(ok, shared->info);
    };

    // ── Call 1: get_info ──────────────────────────────────────────────────────
    QStringList args1;
    args1 << "info" << symbol;
    python::PythonRunner::instance().run("yfinance_data.py", args1,
                                         [shared, symbol, try_complete](python::PythonResult result) {
                                             shared->info_done = true;
                                             if (!result.success) {
                                                 LOG_WARN("MarketData", "get_info failed for " + symbol);
                                                 try_complete();
                                                 return;
                                             }
                                             auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                                             if (!doc.isObject() || doc.object().contains("error")) {
                                                 try_complete();
                                                 return;
                                             }
                                             QJsonObject o = doc.object();
                                             shared->info.symbol = o["symbol"].toString(symbol);
                                             shared->info.name = o["company_name"].toString();
                                             shared->info.sector = o["sector"].toString();
                                             shared->info.industry = o["industry"].toString();
                                             shared->info.country = o["country"].toString();
                                             shared->info.currency = o["currency"].toString("USD");
                                             shared->info.market_cap = o["market_cap"].toDouble();
                                             shared->info.beta = o["beta"].toDouble();
                                             shared->info.week52_high = o["fifty_two_week_high"].toDouble();
                                             shared->info.week52_low = o["fifty_two_week_low"].toDouble();
                                             shared->info.avg_volume = o["average_volume"].toDouble();
                                             shared->info.eps = o["revenue_per_share"].toDouble();
                                             shared->info_ok = true;
                                             try_complete();
                                         });

    // ── Call 2: financial_ratios ──────────────────────────────────────────────
    QStringList args2;
    args2 << "financial_ratios" << symbol;
    python::PythonRunner::instance().run("yfinance_data.py", args2,
                                         [shared, symbol, try_complete](python::PythonResult result) {
                                             shared->ratios_done = true;
                                             if (!result.success) {
                                                 LOG_WARN("MarketData", "financial_ratios failed for " + symbol);
                                                 try_complete();
                                                 return;
                                             }
                                             auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                                             if (!doc.isObject() || doc.object().contains("error")) {
                                                 try_complete();
                                                 return;
                                             }
                                             QJsonObject o = doc.object();
                                             shared->info.pe_ratio = o["peRatio"].toDouble();
                                             shared->info.forward_pe = o["forwardPE"].toDouble();
                                             shared->info.price_to_book = o["priceToBook"].toDouble();
                                             shared->info.dividend_yield = o["dividendYield"].toDouble();
                                             shared->info.roe = o["returnOnEquity"].toDouble();
                                             shared->info.profit_margin = o["profitMargin"].toDouble();
                                             shared->info.debt_to_equity = o["debtToEquity"].toDouble();
                                             shared->info.current_ratio = o["currentRatio"].toDouble();
                                             shared->info.eps = o["revenuePerShare"].toDouble();
                                             shared->ratios_ok = true;
                                             try_complete();
                                         });
}

// ── History fetch (OHLCV) ────────────────────────────────────────────────────

void MarketDataService::fetch_history(const QString& symbol, const QString& period, const QString& interval,
                                      HistoryCallback cb) {
    QStringList args;
    args << "historical_period" << symbol << period << interval;

    python::PythonRunner::instance().run("yfinance_data.py", args, [cb, symbol](python::PythonResult result) {
        if (!result.success) {
            LOG_WARN("MarketData", "History fetch failed for " + symbol + ": " + result.error);
            cb(false, {});
            return;
        }

        auto doc = QJsonDocument::fromJson(result.output.toUtf8());
        if (!doc.isArray()) {
            LOG_WARN("MarketData", "History parse error for " + symbol);
            cb(false, {});
            return;
        }

        QVector<HistoryPoint> history;
        for (const auto& v : doc.array()) {
            QJsonObject o = v.toObject();
            HistoryPoint pt;
            pt.timestamp = static_cast<qint64>(o["timestamp"].toDouble());
            pt.open = o["open"].toDouble();
            pt.high = o["high"].toDouble();
            pt.low = o["low"].toDouble();
            pt.close = o["close"].toDouble();
            pt.volume = static_cast<qint64>(o["volume"].toDouble());
            history.append(pt);
        }

        LOG_INFO("MarketData", QString("Fetched %1 history points for %2").arg(history.size()).arg(symbol));
        cb(true, history);
    });
}

// ── Static symbol lists ─────────────────────────────────────────────────────

QStringList MarketDataService::indices_symbols() {
    return {"^GSPC", "^DJI",  "^IXIC", "^RUT",      "^FTSE",  "^GDAXI",
            "^FCHI", "^N225", "^HSI",  "000001.SS", "^BSESN", "^NSEI"};
}

QStringList MarketDataService::forex_symbols() {
    return {"EURUSD=X", "GBPUSD=X", "USDJPY=X", "AUDUSD=X", "USDCAD=X", "USDCHF=X", "NZDUSD=X", "EURCHF=X"};
}

QStringList MarketDataService::crypto_symbols() {
    return {"BTC-USD", "ETH-USD", "BNB-USD", "SOL-USD", "XRP-USD", "ADA-USD", "DOGE-USD", "DOT-USD", "LTC-USD"};
}

QStringList MarketDataService::commodity_symbols() {
    return {"GC=F", "SI=F", "CL=F", "BZ=F", "NG=F", "HG=F", "PL=F", "PA=F"};
}

QStringList MarketDataService::mover_symbols() {
    return {"SMCI", "PLTR", "MSTR", "NVDA", "AMD", "TSLA", "INTC", "PFE", "BA", "NKE", "DIS", "PYPL"};
}

QStringList MarketDataService::global_snapshot_symbols() {
    return {"^VIX", "^TNX", "DX-Y.NYB", "GC=F", "CL=F", "BTC-USD"};
}

QVector<MarketCategory> MarketDataService::default_global_markets() {
    return {
        {"Stock Indices",
         {"^GSPC", "^IXIC", "^DJI", "^RUT", "^VIX", "^FTSE", "^GDAXI", "^N225", "^FCHI", "^HSI", "^AXJO", "^BSESN",
          "^NSEI", "^STOXX50E", "^NYA", "^SOX", "^IBEX", "^AEX"}},
        {"Forex",
         {"EURUSD=X", "GBPUSD=X", "USDJPY=X", "USDCHF=X", "USDCAD=X", "AUDUSD=X", "NZDUSD=X", "EURGBP=X", "EURJPY=X",
          "GBPJPY=X", "USDCNY=X", "USDINR=X"}},
        {"Commodities",
         {"GC=F", "SI=F", "PL=F", "PA=F", "HG=F", "CL=F", "BZ=F", "NG=F", "RB=F", "HO=F", "ZC=F", "ZW=F", "ZS=F",
          "KC=F", "CT=F", "SB=F", "CC=F", "LBS=F"}},
        {"Bonds", {"^TNX", "^TYX", "^IRX", "^FVX", "TLT", "IEF", "SHY", "BND", "AGG", "LQD", "HYG", "JNK"}},
        {"ETFs", {"SPY", "QQQ", "DIA", "EEM", "GLD", "XLK", "XLE", "XLF", "XLV", "VNQ", "IWM", "VTI"}},
        {"Cryptocurrencies",
         {"BTC-USD", "ETH-USD", "BNB-USD", "SOL-USD", "XRP-USD", "ADA-USD", "DOGE-USD", "LINK-USD", "DOT-USD",
          "AVAX-USD", "UNI-USD", "ATOM-USD"}},
    };
}

QVector<RegionalMarket> MarketDataService::default_regional_markets() {
    return {
        {"India",
         {
             {"RELIANCE.NS", "Reliance Industries"},
             {"TCS.NS", "Tata Consultancy"},
             {"HDFCBANK.NS", "HDFC Bank"},
             {"INFY.NS", "Infosys"},
             {"HINDUNILVR.NS", "Hindustan Unilever"},
             {"ICICIBANK.NS", "ICICI Bank"},
             {"SBIN.NS", "State Bank of India"},
             {"BHARTIARTL.NS", "Bharti Airtel"},
             {"ITC.NS", "ITC Limited"},
             {"KOTAKBANK.NS", "Kotak Mahindra Bank"},
             {"LT.NS", "Larsen & Toubro"},
             {"WIPRO.NS", "Wipro Limited"},
         }},
        {"China",
         {
             {"BABA", "Alibaba Group"},
             {"PDD", "PDD Holdings"},
             {"JD", "JD.com"},
             {"BIDU", "Baidu"},
             {"NIO", "NIO Inc"},
             {"LI", "Li Auto"},
             {"XPEV", "XPeng"},
             {"BILI", "Bilibili"},
             {"NTES", "NetEase"},
             {"ZTO", "ZTO Express"},
             {"VNET", "VNET Group"},
             {"TAL", "TAL Education"},
         }},
        {"United States",
         {
             {"AAPL", "Apple Inc"},
             {"MSFT", "Microsoft Corp"},
             {"GOOGL", "Alphabet Inc"},
             {"AMZN", "Amazon.com"},
             {"NVDA", "NVIDIA Corp"},
             {"META", "Meta Platforms"},
             {"TSLA", "Tesla Inc"},
             {"JPM", "JPMorgan Chase"},
             {"V", "Visa Inc"},
             {"WMT", "Walmart Inc"},
             {"UNH", "UnitedHealth Group"},
             {"MA", "Mastercard Inc"},
         }},
    };
}

void MarketDataService::fetch_sparklines(const QStringList& symbols, SparklineCallback cb) {
    if (symbols.isEmpty()) {
        cb(false, {});
        return;
    }

    // Use existing yfinance_data.py — batch_sparklines command takes symbols as args
    QStringList args;
    args << "batch_sparklines" << symbols;

    python::PythonRunner::instance().run("yfinance_data.py", args, [cb](python::PythonResult result) {
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("MarketData", "Sparklines failed: " + result.error.left(200));
            cb(false, {});
            return;
        }
        auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
        if (!doc.isObject()) {
            cb(false, {});
            return;
        }
        QHash<QString, QVector<double>> out;
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QVector<double> prices;
            for (const auto& v : it.value().toArray())
                prices.append(v.toDouble());
            if (!prices.isEmpty())
                out[it.key()] = prices;
        }
        cb(true, out);
    });
}

} // namespace fincept::services
