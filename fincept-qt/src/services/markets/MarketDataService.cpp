#include "services/markets/MarketDataService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace fincept::services {

MarketDataService& MarketDataService::instance() {
    static MarketDataService s;
    return s;
}

MarketDataService::MarketDataService() {}

// ── Batched + Cached fetch_quotes ───────────────────────────────────────────

void MarketDataService::fetch_quotes(const QStringList& symbols, QuoteCallback cb) {
    if (symbols.isEmpty()) {
        cb(true, {});
        return;
    }

    // Check if ALL requested symbols are cached and fresh
    auto now = QDateTime::currentSecsSinceEpoch();
    bool all_cached = true;
    QVector<QuoteData> cached_results;
    for (const auto& sym : symbols) {
        auto it = cache_.find(sym);
        if (it != cache_.end() && (now - it->timestamp) < cache_ttl_sec_) {
            cached_results.append(it->data);
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
                auto now = QDateTime::currentSecsSinceEpoch();

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

                if (doc.isArray()) {
                    for (const auto& v : doc.array()) {
                        auto q = v.toObject();
                        if (q.contains("error"))
                            continue;
                        auto quote = parse_quote(q);
                        all_quotes.append(quote);
                        cache_[quote.symbol] = {quote, now};
                    }
                } else if (doc.isObject()) {
                    auto obj = doc.object();
                    if (obj.contains("symbol") && !obj.contains("error")) {
                        auto quote = parse_quote(obj);
                        all_quotes.append(quote);
                        cache_[quote.symbol] = {quote, now};
                    }
                }

                LOG_INFO("MarketData", QString("Fetched %1 quotes (cached)").arg(all_quotes.size()));
            } else {
                LOG_WARN("MarketData", "Batch fetch failed: " + result.error);
            }

            // Fan out results to each waiting callback, filtered to their requested symbols
            for (const auto& req : requests) {
                if (!result.success) {
                    // On failure, try to serve from stale cache
                    QVector<QuoteData> stale;
                    for (const auto& sym : req.symbols) {
                        auto it = cache_.find(sym);
                        if (it != cache_.end())
                            stale.append(it->data);
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

} // namespace fincept::services
