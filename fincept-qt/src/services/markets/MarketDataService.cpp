#include "services/markets/MarketDataService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonWorker.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"
#    include "datahub/TopicPolicy.h"

#include <QDateTime>
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
    // Raised from 2 to 10: with the merged batch-all refresh path and the
    // persistent yfinance worker, refreshes complete in <200ms, so gating at
    // 2 req/s starved cold-start (quote + spark + history arrived 500ms apart).
    // 10 req/s still backs off enough for upstream yfinance rate limits while
    // letting dashboard + markets refresh converge in one scheduler tick.
    return 10;
}

void MarketDataService::refresh(const QStringList& topics) {
    // Hub guarantees `topics` all match one of our patterns. We bundle ALL
    // three families into a single `yfinance_data.py batch_all` invocation so
    // one refresh tick spawns one Python process instead of three. Individual
    // fetch_quotes/fetch_sparklines/fetch_history callback APIs remain untouched
    // — they're still used by report builder and other one-shot paths.
    static const QString kQuote = QStringLiteral("market:quote:");
    static const QString kSpark = QStringLiteral("market:sparkline:");
    static const QString kHist  = QStringLiteral("market:history:");

    const qint64 refresh_t0 = QDateTime::currentMSecsSinceEpoch();

    QStringList quote_syms;
    QStringList spark_syms;
    struct HistReq { QString topic; QString symbol; QString period; QString interval; };
    QVector<HistReq> hist_reqs;
    for (const auto& t : topics) {
        if (t.startsWith(kQuote)) {
            quote_syms.append(t.mid(kQuote.size()));
        } else if (t.startsWith(kSpark)) {
            spark_syms.append(t.mid(kSpark.size()));
        } else if (t.startsWith(kHist)) {
            const QString tail = t.mid(kHist.size());
            const QStringList parts = tail.split(QLatin1Char(':'));
            if (parts.size() == 3)
                hist_reqs.append({t, parts.at(0), parts.at(1), parts.at(2)});
        }
    }

    LOG_INFO("DataHub", QString("refresh() quotes=%1 sparks=%2 histories=%3 (1 python spawn)")
                            .arg(quote_syms.size())
                            .arg(spark_syms.size())
                            .arg(hist_reqs.size()));

    // Build the unified batch_all payload.
    QJsonObject payload;
    if (!quote_syms.isEmpty()) {
        QJsonArray arr;
        for (const auto& s : quote_syms) arr.append(s);
        payload["quotes"] = arr;
    }
    if (!spark_syms.isEmpty()) {
        QJsonArray arr;
        for (const auto& s : spark_syms) arr.append(s);
        payload["sparklines"] = arr;
    }
    if (!hist_reqs.isEmpty()) {
        QJsonArray arr;
        for (const auto& h : hist_reqs) {
            QJsonObject o;
            o["symbol"] = h.symbol;
            o["period"] = h.period;
            o["interval"] = h.interval;
            arr.append(o);
        }
        payload["histories"] = arr;
    }

    if (payload.isEmpty()) {
        return;  // Nothing to fetch — hub guarantees this won't happen in practice.
    }

    QPointer<MarketDataService> self = this;

    // Route via PythonWorker (persistent daemon) instead of PythonRunner so
    // we don't pay the 2–3s yfinance/pandas import cost per refresh tick. See
    // PythonWorker docs — scope is yfinance_data.py only.
    python::PythonWorker::instance().submit(
        "batch_all", payload,
        [self, quote_syms, spark_syms, hist_reqs, refresh_t0](bool ok, QJsonObject root, QString err) {
            if (!self)
                return;

            const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - refresh_t0;

            if (!ok) {
                LOG_WARN("MarketData",
                         QString("batch_all failed in %1ms: %2").arg(elapsed).arg(err.left(200)));
                // Notify hub so subscribers can react (clear spinner / show
                // error) and so the scheduler clears the in_flight flag for
                // retry on the next pass. Without this widgets that depend
                // on these topics would spin forever on producer failure.
                auto& hub = datahub::DataHub::instance();
                const QString msg = err.isEmpty()
                                        ? QStringLiteral("Market data refresh failed")
                                        : err.left(200);
                for (const auto& s : quote_syms)
                    hub.publish_error(QStringLiteral("market:quote:") + s, msg);
                for (const auto& s : spark_syms)
                    hub.publish_error(QStringLiteral("market:sparkline:") + s, msg);
                for (const auto& h : hist_reqs)
                    hub.publish_error(h.topic, msg);
                return;
            }

            // PythonWorker passes through the `result` JSON as-is (or wraps
            // a scalar under "_value"). For batch_all the daemon returns an
            // object so root is already the {quotes,sparklines,histories} map.
            // publish to hub, store in cache. We don't need the flush_batch code
            // path here because no callback chain is waiting on this refresh tick.
            const QJsonArray quotes_arr = root.value("quotes").toArray();
            int quotes_ok = 0;
            for (const auto& v : quotes_arr) {
                const QJsonObject q = v.toObject();
                if (q.isEmpty() || q.contains("error"))
                    continue;
                QuoteData qd{
                    q["symbol"].toString(),
                    q["name"].toString(q["symbol"].toString()),
                    q["price"].toDouble(),
                    q["change"].toDouble(),
                    q["change_percent"].toDouble(),
                    q["high"].toDouble(),
                    q["low"].toDouble(),
                    q["volume"].toDouble()};

                // Cache write — mirrors store_quote() in flush_batch.
                QJsonObject co;
                co["symbol"] = qd.symbol;
                co["name"] = qd.name;
                co["price"] = qd.price;
                co["change"] = qd.change;
                co["change_pct"] = qd.change_pct;
                co["high"] = qd.high;
                co["low"] = qd.low;
                co["volume"] = qd.volume;
                fincept::CacheManager::instance().put(
                    "market:" + qd.symbol,
                    QVariant(QString::fromUtf8(QJsonDocument(co).toJson(QJsonDocument::Compact))),
                    kQuoteCacheTtlSec, "market_data");

                self->publish_quote_to_hub(qd);
                ++quotes_ok;
            }

            // Sparklines — {sym: [closes]}
            const QJsonObject sparks = root.value("sparklines").toObject();
            int sparks_ok = 0;
            for (auto it = sparks.begin(); it != sparks.end(); ++it) {
                const QJsonArray closes = it.value().toArray();
                if (closes.isEmpty())
                    continue;
                QVector<double> prices;
                prices.reserve(closes.size());
                for (const auto& c : closes)
                    prices.append(c.toDouble());
                self->publish_sparkline_to_hub(it.key(), prices);
                ++sparks_ok;
            }

            // Histories — array of {symbol, period, interval, points[]}
            const QJsonArray hists = root.value("histories").toArray();
            int hists_ok = 0;
            for (const auto& hv : hists) {
                const QJsonObject h = hv.toObject();
                if (h.contains("error"))
                    continue;
                const QString sym = h.value("symbol").toString();
                const QString per = h.value("period").toString();
                const QString ivl = h.value("interval").toString();
                const QJsonArray pts = h.value("points").toArray();
                QVector<HistoryPoint> points;
                points.reserve(pts.size());
                for (const auto& pv : pts) {
                    const QJsonObject p = pv.toObject();
                    HistoryPoint pt;
                    pt.timestamp = static_cast<qint64>(p["timestamp"].toDouble());
                    pt.open = p["open"].toDouble();
                    pt.high = p["high"].toDouble();
                    pt.low = p["low"].toDouble();
                    pt.close = p["close"].toDouble();
                    pt.volume = static_cast<qint64>(p["volume"].toDouble());
                    points.append(pt);
                }
                self->publish_history_to_hub(sym, per, ivl, points);
                ++hists_ok;
            }

            LOG_INFO("MarketData",
                     QString("batch_all OK in %1ms: quotes=%2/%3 sparks=%4/%5 hists=%6/%7")
                         .arg(elapsed)
                         .arg(quotes_ok).arg(quote_syms.size())
                         .arg(sparks_ok).arg(spark_syms.size())
                         .arg(hists_ok).arg(hist_reqs.size()));
        });
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

    // Quotes: 30s TTL, 2s min interval. Dropped min_interval from 5s → 2s so
    // user-triggered refreshes and initial cold-start paint don't queue behind
    // the gate. 2s still prevents hammering yfinance on scheduler ticks.
    //
    // Phase 8 / decision 9.2: pause_when_inactive=true. Quotes drive
    // visible price tickers; when a frame is minimised the user can't see
    // them and the fan-out cost (table re-paints, format computations) is
    // pure waste. The cached value still updates so the next show reads
    // a fresh price from peek().
    datahub::TopicPolicy quote_p;
    quote_p.ttl_ms = 30'000;
    quote_p.min_interval_ms = 2'000;
    quote_p.pause_when_inactive = true;
    hub.set_policy_pattern(QStringLiteral("market:quote:*"), quote_p);

    // Sparklines: 5-day hourly data, changes slowly — cache 10 minutes,
    // refresh at most every 30s. Reduced from 60s so a user flipping between
    // screens doesn't wait a full minute for sparkline refresh after swapping
    // the symbol set.
    //
    // Phase 8 / decision 9.2: pause_when_inactive=true. Sparkline updates
    // re-paint inline charts in tables — substantial work for inactive
    // frames. Same reasoning as quotes.
    datahub::TopicPolicy spark_p;
    spark_p.ttl_ms = 10 * 60'000;
    spark_p.min_interval_ms = 30'000;
    spark_p.pause_when_inactive = true;
    hub.set_policy_pattern(QStringLiteral("market:sparkline:*"), spark_p);

    // History: OHLCV series. One-shot per chart; policies are conservative to
    // avoid re-fetching for every open of the same chart. 30 min TTL, 60s
    // min-interval (was 5 min) so a user flipping periods/intervals on a
    // chart doesn't stare at a stale view for 5 minutes.
    datahub::TopicPolicy hist_p;
    hist_p.ttl_ms = 30 * 60'000;
    hist_p.min_interval_ms = 60'000;
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
