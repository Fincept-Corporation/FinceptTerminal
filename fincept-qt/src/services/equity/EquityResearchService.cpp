// src/services/equity/EquityResearchService.cpp
#include "services/equity/EquityResearchService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include <cmath>

namespace fincept::services::equity {

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
void EquityResearchService::load_symbol(const QString& symbol, const QString& period) {
    if (symbol.isEmpty())
        return;
    auto now = QDateTime::currentSecsSinceEpoch();

    // ── Quote ────────────────────────────────────────────────────────────────
    auto qit = quote_cache_.find(symbol);
    if (qit != quote_cache_.end() && (now - qit->ts) < kQuoteTtlSec) {
        emit quote_loaded(qit->data);
    } else {
        run_python("yfinance_data.py", {"quote", symbol}, [this, symbol](bool ok, const QString& out) {
            if (!ok) {
                emit error_occurred("Quote", "Failed to fetch quote for " + symbol);
                return;
            }
            auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
            if (obj.contains("error"))
                return;
            auto q = parse_quote(obj);
            quote_cache_[symbol] = {q, QDateTime::currentSecsSinceEpoch()};
            emit quote_loaded(q);
        });
    }

    // ── Info ─────────────────────────────────────────────────────────────────
    auto iit = info_cache_.find(symbol);
    if (iit != info_cache_.end() && (now - iit->ts) < kInfoTtlSec) {
        emit info_loaded(iit->data);
    } else {
        run_python("yfinance_data.py", {"info", symbol}, [this, symbol](bool ok, const QString& out) {
            if (!ok) {
                emit error_occurred("Info", "Failed to fetch info for " + symbol);
                return;
            }
            auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
            if (obj.contains("error"))
                return;
            auto info = parse_info(obj);
            info_cache_[symbol] = {info, QDateTime::currentSecsSinceEpoch()};
            emit info_loaded(info);
        });
    }

    // ── Historical ───────────────────────────────────────────────────────────
    auto hit = candle_cache_.find(symbol);
    if (hit != candle_cache_.end() && (now - hit->ts) < kHistoricalTtlSec) {
        emit historical_loaded(symbol, hit->data);
    } else {
        run_python("yfinance_data.py", {"historical_period", symbol, period, "1d"},
                   [this, symbol](bool ok, const QString& out) {
                       if (!ok) {
                           emit error_occurred("Historical", "Failed to fetch historical for " + symbol);
                           return;
                       }
                       auto arr = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).array();
                       auto candles = parse_candles(arr);
                       candle_cache_[symbol] = {candles, QDateTime::currentSecsSinceEpoch()};
                       emit historical_loaded(symbol, candles);
                   });
    }
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
    // Step 1: fetch historical, then step 2: compute technicals
    auto hit = candle_cache_.find(symbol);
    QString cached_json;
    if (hit != candle_cache_.end()) {
        // Build JSON from cache to avoid a second Python call
        QJsonArray arr;
        for (const auto& c : hit->data) {
            QJsonObject o;
            o["timestamp"] = c.timestamp;
            o["open"] = c.open;
            o["high"] = c.high;
            o["low"] = c.low;
            o["close"] = c.close;
            o["volume"] = c.volume;
            arr.append(o);
        }
        cached_json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    }

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

    if (!cached_json.isEmpty()) {
        run_compute(cached_json);
    } else {
        run_python("yfinance_data.py", {"historical_period", symbol, period, "1d"},
                   [this, symbol, run_compute](bool ok, const QString& out) {
                       if (!ok) {
                           emit error_occurred("Technicals", "Failed historical fetch");
                           return;
                       }
                       auto raw = python::extract_json(out);
                       // Also store in cache
                       auto arr = QJsonDocument::fromJson(raw.toUtf8()).array();
                       candle_cache_[symbol] = {parse_candles(arr), QDateTime::currentSecsSinceEpoch()};
                       run_compute(raw);
                   });
    }
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
void EquityResearchService::fetch_news(const QString& symbol, int count) {
    auto now = QDateTime::currentSecsSinceEpoch();
    auto nit = news_cache_.find(symbol);
    if (nit != news_cache_.end() && (now - nit->ts) < kNewsTtlSec) {
        emit news_loaded(symbol, nit->data);
        return;
    }
    run_python("yfinance_data.py", {"news", symbol, QString::number(count)},
               [this, symbol](bool ok, const QString& out) {
                   if (!ok) {
                       emit error_occurred("News", "Failed to fetch news for " + symbol);
                       return;
                   }
                   auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                   auto arr = obj["articles"].toArray();
                   auto articles = parse_news(arr);
                   news_cache_[symbol] = {articles, QDateTime::currentSecsSinceEpoch()};
                   emit news_loaded(symbol, articles);
               });
}

// ── TALIpp ────────────────────────────────────────────────────────────────────
void EquityResearchService::compute_talipp(const QString& symbol, const QString& indicator, const QVariantMap& params,
                                           const QString& period) {
    auto run_talipp = [this, indicator, params](const QString& hist_json) {
        QJsonObject p_obj;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it)
            p_obj[it.key()] = QJsonValue::fromVariant(it.value());
        QString params_json = QJsonDocument(p_obj).toJson(QJsonDocument::Compact);

        run_python("equity_talipp.py", {indicator, hist_json, params_json},
                   [this, indicator](bool ok, const QString& out) {
                       if (!ok) {
                           emit error_occurred("TALIpp", out);
                           return;
                       }
                       auto doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                       QVector<double> values;
                       QVector<qint64> timestamps;
                       for (const auto& v : doc["values"].toArray())
                           values.append(v.toDouble());
                       for (const auto& t : doc["timestamps"].toArray())
                           timestamps.append(static_cast<qint64>(t.toDouble()));
                       emit talipp_result(indicator, values, timestamps);
                   });
    };

    // Use cached historical if available, else fetch
    auto hit = candle_cache_.find(symbol);
    if (hit != candle_cache_.end()) {
        QJsonArray arr;
        for (const auto& c : hit->data) {
            QJsonObject o;
            o["timestamp"] = c.timestamp;
            o["open"] = c.open;
            o["high"] = c.high;
            o["low"] = c.low;
            o["close"] = c.close;
            o["volume"] = c.volume;
            arr.append(o);
        }
        run_talipp(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    } else {
        run_python("yfinance_data.py", {"historical_period", symbol, period, "1d"},
                   [this, symbol, run_talipp](bool ok, const QString& out) {
                       if (!ok) {
                           emit error_occurred("TALIpp", "Historical fetch failed");
                           return;
                       }
                       auto raw = python::extract_json(out);
                       candle_cache_[symbol] = {parse_candles(QJsonDocument::fromJson(raw.toUtf8()).array()),
                                                QDateTime::currentSecsSinceEpoch()};
                       run_talipp(raw);
                   });
    }
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

    s.market_cap = o["market_cap"].toVariant().toDouble();
    s.enterprise_value = o["enterprise_value"].toVariant().toDouble();
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
    s.free_cashflow = o["free_cashflow"].toVariant().toDouble();
    s.operating_cashflow = o["operating_cashflow"].toVariant().toDouble();
    s.total_cash = o["total_cash"].toVariant().toDouble();
    s.total_debt = o["total_debt"].toVariant().toDouble();
    s.total_revenue = o["total_revenue"].toVariant().toDouble();

    s.earnings_growth = o["earnings_growth"].toDouble();
    s.revenue_growth = o["revenue_growth"].toDouble();

    s.shares_outstanding = o["shares_outstanding"].toVariant().toDouble();
    s.float_shares = o["float_shares"].toVariant().toDouble();
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
