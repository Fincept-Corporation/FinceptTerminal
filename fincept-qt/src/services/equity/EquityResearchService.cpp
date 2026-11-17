// src/services/equity/EquityResearchService.cpp
#include "services/equity/EquityResearchService.h"

#include "core/logging/Logger.h"
#include "core/market/MarketSessions.h"
#include "python/PythonRunner.h"
#include "services/equity/EquityTechnicalMath.h"
#include "services/equity/TechnicalIndicators.h"
#include "services/news/NewsNlpEngine.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/DataSourceRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/HistoricalDataService.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
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

// trading-layer BrokerCandle[] → the yfinance-shaped JSON array the Technicals
// computation consumes: [{timestamp(sec), open, high, low, close, volume}, …], ascending.
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

// True when an identical request is already running and young enough to still be
// trusted (see EquityResearchService::kInflightMaxAgeMs).
bool equity_svc_inflight_busy(const QHash<QString, qint64>& inflight, const QString& key, qint64 max_age_ms) {
    const auto it = inflight.constFind(key);
    return it != inflight.constEnd() && (QDateTime::currentMSecsSinceEpoch() - it.value()) < max_age_ms;
}

// The "code" field a failing yfinance_data.py action prints next to its "error"
// (not_found | no_data | rate_limited | network | unknown). Empty for an older script.
QString eqrs_code_from_output(const QString& raw_output) {
    const QJsonDocument doc = QJsonDocument::fromJson(python::extract_json(raw_output).toUtf8());
    if (!doc.isObject())
        return {};
    return doc.object().value(QStringLiteral("code")).toString().trimmed().toLower();
}

// Best-effort failure class for a message that came without a code (older script, runner
// or daemon text). Only the classes a caller acts on are guessed; "" when unclear.
QString eqrs_classify_message(const QString& message) {
    const QString m = message.toLower();
    if (m.contains(QLatin1String("too many requests")) || m.contains(QLatin1String("rate limit")) ||
        m.contains(QLatin1String("ratelimit")))
        return QStringLiteral("rate_limited");
    static const char* const kNetworkMarkers[] = {
        "failed to connect",        "could not resolve host",    "curl: (",           "connectionerror",
        "connection refused",       "connection reset",          "connection aborted", "network is unreachable",
        "name or service not known", "certificate verify failed", "proxyerror",
    };
    for (const char* marker : kNetworkMarkers) {
        if (m.contains(QLatin1String(marker)))
            return QStringLiteral("network");
    }
    if (m.contains(QLatin1String("no data available")) || m.contains(QLatin1String("no data found")) ||
        m.contains(QLatin1String("no price data")))
        return QStringLiteral("no_data");
    return {};
}

// Reasons can be whole tracebacks; the banner/tooltip needs the gist, not 40 lines.
QString eqrs_clip_reason(const QString& reason) {
    constexpr qsizetype kMaxReasonChars = 500;
    const QString t = reason.trimmed();
    return t.size() > kMaxReasonChars ? t.left(kMaxReasonChars) + QStringLiteral("…") : t;
}

// An info payload with no name, price, market cap or 52-week range is what an older
// yfinance_data.py returns for a symbol Yahoo does not know ("N/A"/null everywhere). It is
// not data: caching it for 5 minutes showed a page of dashes and hid the real problem.
bool eqrs_info_is_blank(const QJsonObject& o) {
    if (o.isEmpty())
        return true;
    const auto blank = [](const QJsonValue& v) {
        if (v.isNull() || v.isUndefined())
            return true;
        if (v.isString()) {
            const QString s = v.toString().trimmed();
            return s.isEmpty() || s == QLatin1String("N/A");
        }
        if (v.isDouble())
            return v.toDouble() == 0.0;
        return false;
    };
    return blank(o.value(QStringLiteral("company_name"))) && blank(o.value(QStringLiteral("current_price"))) &&
           blank(o.value(QStringLiteral("market_cap"))) && blank(o.value(QStringLiteral("fifty_two_week_high"))) &&
           blank(o.value(QStringLiteral("average_volume")));
}

// A statements payload where all three statements are empty ({"symbol", "timestamp"} plus
// three {}) is the failure an older script reports as success. It must never be cached,
// and above all never overwrite the 30-day last-known-good copy.
bool eqrs_financials_has_statements(const QJsonObject& o) {
    for (const char* key : {"income_statement", "balance_sheet", "cash_flow"}) {
        if (!o.value(QLatin1String(key)).toObject().isEmpty())
            return true;
    }
    return false;
}

// Daily bars (yfinance JSON: timestamp seconds, OHLCV) → tmath::Bars. Thread-safe.
tmath::Bars eqrs_bars_from_json(const QJsonArray& arr) {
    QVector<Candle> candles;
    candles.reserve(arr.size());
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        Candle c;
        c.timestamp = static_cast<qint64>(o.value(QStringLiteral("timestamp")).toDouble());
        c.open = o.value(QStringLiteral("open")).toDouble();
        c.high = o.value(QStringLiteral("high")).toDouble();
        c.low = o.value(QStringLiteral("low")).toDouble();
        c.close = o.value(QStringLiteral("close")).toDouble();
        c.volume = static_cast<qint64>(o.value(QStringLiteral("volume")).toDouble());
        candles.append(c);
    }
    return tmath::bars_from_candles(candles);
}

QJsonObject eqrs_group_to_json(const RatingGroup& g) {
    QJsonArray votes;
    for (const auto& v : g.votes)
        votes.append(QJsonObject{{QStringLiteral("name"), v.name},
                                 {QStringLiteral("key"), v.key},
                                 {QStringLiteral("value"), v.value},
                                 {QStringLiteral("signal"), static_cast<int>(v.signal)}});
    return QJsonObject{{QStringLiteral("score"), std::isfinite(g.score) ? QJsonValue(g.score) : QJsonValue()},
                       {QStringLiteral("buy"), g.buy},
                       {QStringLiteral("sell"), g.sell},
                       {QStringLiteral("neutral"), g.neutral},
                       {QStringLiteral("votes"), votes}};
}

RatingGroup eqrs_group_from_json(const QJsonObject& o) {
    RatingGroup g;
    const QJsonValue score = o.value(QStringLiteral("score"));
    g.score = score.isDouble() ? score.toDouble() : std::numeric_limits<double>::quiet_NaN();
    g.signal = tmath::signal_for_score(g.score);
    g.buy = o.value(QStringLiteral("buy")).toInt();
    g.sell = o.value(QStringLiteral("sell")).toInt();
    g.neutral = o.value(QStringLiteral("neutral")).toInt();
    for (const auto& v : o.value(QStringLiteral("votes")).toArray()) {
        const QJsonObject vo = v.toObject();
        RatingVote rv;
        rv.name = vo.value(QStringLiteral("name")).toString();
        rv.key = vo.value(QStringLiteral("key")).toString();
        rv.value = vo.value(QStringLiteral("value")).toDouble(std::numeric_limits<double>::quiet_NaN());
        rv.signal = static_cast<TechSignal>(std::clamp(vo.value(QStringLiteral("signal")).toInt(2), 0, 4));
        g.votes.append(rv);
    }
    return g;
}

QJsonObject eqrs_rating_to_json(const tmath::Rating& r) {
    return QJsonObject{{QStringLiteral("ok"), r.ok},
                       {QStringLiteral("ma"), eqrs_group_to_json(r.ma)},
                       {QStringLiteral("osc"), eqrs_group_to_json(r.osc)}};
}

tmath::Rating eqrs_rating_from_json(const QJsonObject& o) {
    tmath::Rating r;
    if (!o.value(QStringLiteral("ok")).toBool())
        return r;
    r.ma = eqrs_group_from_json(o.value(QStringLiteral("ma")).toObject());
    r.osc = eqrs_group_from_json(o.value(QStringLiteral("osc")).toObject());
    const bool ma_ok = std::isfinite(r.ma.score), osc_ok = std::isfinite(r.osc.score);
    r.summary = ma_ok && osc_ok ? (r.ma.score + r.osc.score) / 2.0 : ma_ok ? r.ma.score : r.osc.score;
    r.summary_signal = tmath::signal_for_score(r.summary);
    r.ok = std::isfinite(r.summary);
    return r;
}

// The two-group rating replaces the old all-rows tally: only voting indicators count, and
// the overall signal is the summary of the moving-average and oscillator groups.
void eqrs_apply_rating(TechnicalsData& td, const tmath::Rating& r) {
    if (!r.ok)
        return;
    td.ma = r.ma;
    td.osc = r.osc;
    td.summary_score = r.summary;
    td.summary_signal = r.summary_signal;
    td.rating_ok = true;
    td.overall_signal = r.summary_signal;
    td.strong_buy = td.strong_sell = 0;
    td.buy = r.ma.buy + r.osc.buy;
    td.sell = r.ma.sell + r.osc.sell;
    td.neutral = r.ma.neutral + r.osc.neutral;
}

// Google News / NewsAPI edition for a listing: the home market's country and the language
// its business press writes in; English where that is the business language (IN, SG, HK,
// ZA, GB, …) or the market is unknown.
void eqrs_news_locale(const QString& symbol, QString* country, QString* language) {
    *country = QStringLiteral("US");
    *language = QStringLiteral("en");
    const auto* venue = core::market::venue_for_symbol(symbol);
    if (!venue || venue->country.isEmpty())
        return;
    *country = venue->country.toUpper();
    static const QHash<QString, QString> kNewsLanguage = {
        {"JP", "ja"}, {"DE", "de"}, {"AT", "de"}, {"FR", "fr"}, {"IT", "it"}, {"ES", "es"}, {"BR", "pt"},
        {"PT", "pt"}, {"KR", "ko"}, {"CN", "zh"}, {"TW", "zh"}, {"NL", "nl"}, {"SE", "sv"}, {"NO", "no"},
        {"DK", "da"}, {"FI", "fi"}, {"MX", "es"}, {"AR", "es"}, {"CL", "es"}, {"PL", "pl"}, {"TR", "tr"},
        {"ID", "id"}, {"TH", "th"}, {"IL", "he"}, {"SA", "ar"}, {"AE", "ar"}, {"RU", "ru"}, {"CZ", "cs"},
    };
    *language = kNewsLanguage.value(*country, QStringLiteral("en"));
}

// A history payload that is a non-empty JSON array of bars; anything else ("[]", an
// {"error"} object, garbage) is a failure.
bool eqrs_history_array(const QString& json, QJsonArray* out) {
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray() || doc.array().isEmpty())
        return false;
    if (out)
        *out = doc.array();
    return true;
}

} // namespace

// ── Singleton ─────────────────────────────────────────────────────────────────
EquityResearchService& EquityResearchService::instance() {
    static EquityResearchService inst;
    return inst;
}

EquityResearchService::EquityResearchService(QObject* parent) : QObject(parent) {}

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

void EquityResearchService::run_python_coded(const QString& script, const QStringList& args, PyCodedCallback cb) {
    QPointer<EquityResearchService> self = this;
    python::PythonRunner::instance().run(script, args, [self, cb](python::PythonResult result) {
        if (!self)
            return;
        if (result.success) {
            cb(true, result.output, QString());
            return;
        }
        // On failure the runner keeps the script's JSON in `output` (both the daemon and the
        // subprocess path) — that is where the "code" lives.
        cb(false, result.error, eqrs_code_from_output(result.output));
    });
}

void EquityResearchService::report_failure(const QString& symbol, const QString& leg, const QString& period,
                                           const QString& message, const QString& code, const QString& fallback) {
    LoadFailure f;
    f.symbol = symbol;
    f.leg = leg;
    f.period = period;
    f.message = eqrs_clip_reason(message);
    if (f.message.isEmpty())
        f.message = fallback;
    f.code = code.trimmed().toLower();
    if (f.code.isEmpty())
        f.code = eqrs_classify_message(f.message);
    LOG_WARN("EquityResearch", QString("%1 load for %2 failed [%3]: %4")
                                   .arg(leg, symbol, f.code.isEmpty() ? QStringLiteral("?") : f.code, f.message.left(200)));
    emit error_occurred(leg, f.message);
    emit load_failed(f);
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
    // The header bar and the Overview tab both ask when a symbol opens; the answer is
    // broadcast to every listener, so a second request while one runs is redundant.
    if (equity_svc_inflight_busy(quote_inflight_, symbol, kInflightMaxAgeMs))
        return;
    quote_inflight_.insert(symbol, QDateTime::currentMSecsSinceEpoch());
    run_python_coded("yfinance_data.py", {"quote", symbol}, [this, symbol](bool ok, const QString& out,
                                                                           const QString& code) {
        quote_inflight_.remove(symbol);
        const QString fallback = tr("Failed to fetch the quote for %1").arg(symbol);
        if (!ok) {
            report_failure(symbol, QStringLiteral("Quote"), {}, out, code, fallback);
            return;
        }
        auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
        if (obj.contains("error")) {
            report_failure(symbol, QStringLiteral("Quote"), {}, obj["error"].toString(), obj["code"].toString(),
                           fallback);
            return;
        }
        if (obj.isEmpty()) {
            report_failure(symbol, QStringLiteral("Quote"), {}, tr("The quote reply for %1 was empty").arg(symbol),
                           QStringLiteral("no_data"), fallback);
            return;
        }
        fincept::CacheManager::instance().put(
            "equity:quote:" + symbol, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
            kQuoteTtlSec, "equity");
        emit quote_loaded(parse_quote(obj));
    });
}

void EquityResearchService::load_info_only(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    const QString cache_key = "equity:info:" + symbol;
    const QVariant icv = fincept::CacheManager::instance().get(cache_key);
    if (!icv.isNull()) {
        const QJsonObject cached = QJsonDocument::fromJson(icv.toString().toUtf8()).object();
        if (!eqrs_info_is_blank(cached)) {
            emit info_loaded(parse_info(cached));
            return;
        }
        fincept::CacheManager::instance().remove(cache_key); // a blank copy cached before the check existed
    }
    if (equity_svc_inflight_busy(info_inflight_, symbol, kInflightMaxAgeMs))
        return; // header bar + Overview + Analysis all ask on open — one daemon call serves them
    info_inflight_.insert(symbol, QDateTime::currentMSecsSinceEpoch());
    run_python_coded("yfinance_data.py", {"info", symbol}, [this, symbol](bool ok, const QString& out,
                                                                          const QString& code) {
        info_inflight_.remove(symbol);
        const QString fallback = tr("Failed to fetch company information for %1").arg(symbol);
        if (!ok) {
            report_failure(symbol, QStringLiteral("Info"), {}, out, code, fallback);
            return;
        }
        auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
        if (obj.contains("error")) {
            report_failure(symbol, QStringLiteral("Info"), {}, obj["error"].toString(), obj["code"].toString(),
                           fallback);
            return;
        }
        if (eqrs_info_is_blank(obj)) {
            report_failure(symbol, QStringLiteral("Info"), {},
                           tr("Yahoo Finance has no company information for %1").arg(symbol),
                           QStringLiteral("not_found"), fallback);
            return;
        }
        fincept::CacheManager::instance().put(
            "equity:info:" + symbol, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
            kInfoTtlSec, "equity");
        emit info_loaded(parse_info(obj));
    });
}

void EquityResearchService::load_historical_only(const QString& symbol, const QString& period) {
    if (symbol.isEmpty())
        return;
    const QString cache_key = "equity:candles:" + symbol + ":" + period;
    if (equity_svc_inflight_busy(historical_inflight_, cache_key, kInflightMaxAgeMs))
        return; // the pending load broadcasts historical_loaded to every listener
    historical_inflight_.insert(cache_key, QDateTime::currentMSecsSinceEpoch());

    // A "max" history is ~11,000 bars (~1 MB of JSON for AAPL). Reading it from the cache,
    // parsing it, writing it back and turning it into candles all ran in the GUI thread —
    // inside the click that switched symbols (cache hit) or the daemon's reply — and froze
    // the UI for 0.3–0.6 s per history (ui_stalls.log). All of it now runs on a worker; only
    // the emit (or the failure report) comes back to the GUI thread.
    auto finish = [this, symbol, period, cache_key](QVector<Candle> candles, QString message, QString code) {
        QMetaObject::invokeMethod(
            this,
            [this, symbol, period, cache_key, candles = std::move(candles), message = std::move(message),
             code = std::move(code)]() {
                historical_inflight_.remove(cache_key);
                if (!candles.isEmpty()) {
                    emit historical_loaded(symbol, period, candles);
                    return;
                }
                report_failure(symbol, QStringLiteral("Historical"), period, message, code,
                               tr("Failed to fetch price history for %1").arg(symbol));
            },
            Qt::QueuedConnection);
    };
    auto fetch = [this, symbol, period, cache_key, finish]() {
        run_python_coded(
            "yfinance_data.py", {"historical_period", symbol, period, "1d"},
            [this, symbol, period, cache_key, finish](bool ok, const QString& out, const QString& code) {
                if (!ok) {
                    finish({}, out, code);
                    return;
                }
                (void)QtConcurrent::run([this, symbol, period, cache_key, finish, out]() {
                    const QString json = python::extract_json(out);
                    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
                    if (doc.isObject() && doc.object().contains(QStringLiteral("error"))) {
                        finish({}, doc.object()["error"].toString(), doc.object()["code"].toString());
                        return;
                    }
                    // An empty series is how an older script reports "Yahoo returned nothing"
                    // (rate limit, blocked network, unknown ticker). It is a failure, never a chart.
                    if (!doc.isArray() || doc.array().isEmpty()) {
                        finish({}, tr("No price history returned for %1 (%2)").arg(symbol, period),
                               QStringLiteral("no_data"));
                        return;
                    }
                    const QJsonArray arr = doc.array();
                    fincept::CacheManager::instance().put(
                        cache_key, QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                        kHistoricalTtlSec, "equity");
                    finish(parse_candles(arr), {}, {});
                });
            });
    };
    (void)QtConcurrent::run([this, cache_key, finish, fetch]() {
        const QVariant hcv = fincept::CacheManager::instance().get(cache_key);
        QJsonArray arr;
        if (!hcv.isNull() && eqrs_history_array(hcv.toString(), &arr)) {
            finish(parse_candles(arr), {}, {});
            return;
        }
        QMetaObject::invokeMethod(this, fetch, Qt::QueuedConnection); // the runner lives on the GUI thread
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
    // Statements change quarterly, yet every Financials/Valuation tab visit used to
    // re-run the yfinance statements pull (and the two tabs each started their own).
    // Serve a cached copy, and fold a request that arrives while one is in flight.
    //
    // Reading the cached / last-known-good copies, parsing them, parsing the reply,
    // re-serialising it for the two cache rows and building the statement model all
    // ran in the GUI thread (a statements payload is hundreds of KB — 0.1–0.4 s per
    // visit in ui_stalls.log). All of it now runs on a worker, like
    // load_historical_only(); the GUI thread only emits the parsed model (or reports
    // the failure). The in-flight entry now also covers the cache read, so callers
    // that ask while it runs are folded into the one broadcast financials_loaded.
    if (equity_svc_inflight_busy(financials_inflight_, symbol, kInflightMaxAgeMs))
        return;
    financials_inflight_.insert(symbol, QDateTime::currentMSecsSinceEpoch());
    const QString cache_key = "equity:financials:" + symbol;
    const QString lkg_key = "equity:lkg:financials:" + symbol;

    // Last-known-good statements, flagged stale (FinancialsData::stale → the Financials tab's
    // "CACHED COPY — REFRESHING" / "— REFRESH FAILED" note, the Valuation tab's statements
    // note). Shown at once while the refresh runs, and replayed if the refresh fails so a
    // listener that asked while this request was in flight gets them too. Worker thread.
    // parse_financials() reads only its argument, so it is safe off the GUI thread.
    const auto read_lkg = [this, lkg_key]() -> std::optional<FinancialsData> {
        const auto lkg = fincept::CacheManager::instance().try_get(lkg_key);
        if (!lkg)
            return std::nullopt;
        const auto lkg_obj = QJsonDocument::fromJson(lkg->toUtf8()).object();
        if (!eqrs_financials_has_statements(lkg_obj))
            return std::nullopt;
        FinancialsData stale = parse_financials(lkg_obj);
        stale.stale = true;
        return stale;
    };

    // GUI thread (the runner lives there); the reply is handled on a worker.
    auto fetch = [this, symbol, cache_key, lkg_key, read_lkg]() {
        run_python_coded(
            "yfinance_data.py", {"financials", symbol},
            [this, symbol, cache_key, lkg_key, read_lkg](bool ok, const QString& out, const QString& code) {
                (void)QtConcurrent::run([this, symbol, cache_key, lkg_key, read_lkg, ok, out, code]() {
                    bool failed = false;
                    QString reason;
                    QString reason_code;
                    FinancialsData fresh;
                    if (!ok) {
                        failed = true;
                        reason = out;
                        reason_code = code;
                    } else {
                        const QJsonObject obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                        if (obj.contains(QStringLiteral("error"))) {
                            failed = true;
                            reason = obj.value(QStringLiteral("error")).toString();
                            reason_code = obj.value(QStringLiteral("code")).toString();
                        } else if (!eqrs_financials_has_statements(obj)) {
                            // Never cache emptiness, and never let it overwrite the 30-day copy.
                            failed = true;
                            reason = tr("No financial statements returned for %1").arg(symbol);
                            reason_code = QStringLiteral("no_data");
                        } else {
                            const QVariant payload(
                                QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
                            auto& c = fincept::CacheManager::instance();
                            c.put(cache_key, payload, kFinancialsTtlSec, "equity");
                            c.put(lkg_key, payload, kFinancialsLkgTtlSec, "equity");
                            fresh = parse_financials(obj);
                        }
                    }
                    // A failure keeps the last good statements on screen, still flagged stale.
                    const std::optional<FinancialsData> stale = failed ? read_lkg() : std::nullopt;
                    QMetaObject::invokeMethod(
                        this,
                        [this, symbol, failed, reason, reason_code, fresh = std::move(fresh), stale]() {
                            financials_inflight_.remove(symbol);
                            if (!failed) {
                                emit financials_loaded(fresh);
                                return;
                            }
                            if (stale)
                                emit financials_loaded(*stale);
                            report_failure(symbol, QStringLiteral("Financials"), {}, reason, reason_code,
                                           tr("Failed to fetch financial statements for %1").arg(symbol));
                        },
                        Qt::QueuedConnection);
                });
            });
    };

    (void)QtConcurrent::run([this, symbol, cache_key, read_lkg, fetch]() {
        auto& cache = fincept::CacheManager::instance();
        const QVariant cached = cache.get(cache_key);
        if (!cached.isNull()) {
            const auto cached_obj = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
            // {"symbol","timestamp"} + three empty statements is not "a cached copy" — it is a
            // failure an older build cached; drop it and fetch.
            if (eqrs_financials_has_statements(cached_obj)) {
                FinancialsData fd = parse_financials(cached_obj);
                QMetaObject::invokeMethod(
                    this,
                    [this, symbol, fd = std::move(fd)]() {
                        financials_inflight_.remove(symbol);
                        emit financials_loaded(fd);
                    },
                    Qt::QueuedConnection);
                return;
            }
            cache.remove(cache_key);
        }
        const std::optional<FinancialsData> stale = read_lkg();
        QMetaObject::invokeMethod(
            this,
            [this, stale, fetch]() {
                if (stale)
                    emit financials_loaded(*stale);
                fetch();
            },
            Qt::QueuedConnection);
    });
}

// ── Technicals ────────────────────────────────────────────────────────────────
void EquityResearchService::fetch_technicals(const QString& symbol, const QString& period, bool force) {
    // Indicators off daily bars: serve a recent computation instead of re-fetching the candles
    // and recomputing on every Technicals-tab visit / Sentiment blend.
    const QString result_key = "equity:technicals:" + symbol + ":" + period;
    if (force) {
        // User-pressed REFRESH: drop the 5-minute frame and the candles it was computed from,
        // otherwise the same snapshot comes straight back.
        fincept::CacheManager::instance().remove(result_key);
        fincept::CacheManager::instance().remove("equity:candles:" + symbol + ":" + period);
    }
    if (const auto hit = fincept::CacheManager::instance().try_get(result_key)) {
        // {"rows": frame tail, "rating": two-group rating}. A bare array is a copy cached
        // before the rating existed — recompute it.
        const QJsonDocument doc = QJsonDocument::fromJson(hit->toUtf8());
        const QJsonArray rows = doc.isObject() ? doc.object().value(QStringLiteral("rows")).toArray() : QJsonArray();
        if (!rows.isEmpty()) {
            TechnicalsData td = parse_technicals(symbol, rows);
            eqrs_apply_rating(td, eqrs_rating_from_json(doc.object().value(QStringLiteral("rating")).toObject()));
            emit technicals_loaded(td);
            return;
        }
    }

    // The Technicals tab and the Sentiment blend both ask for the same symbol; the second
    // request would only repeat the candle fetch and the computation.
    const QString inflight_key = symbol + QLatin1Char(':') + period;
    if (equity_svc_inflight_busy(technicals_inflight_, inflight_key, kInflightMaxAgeMs))
        return;
    technicals_inflight_.insert(inflight_key, QDateTime::currentMSecsSinceEpoch());

    // Candles come from ensure_candles (cache → region-matched connected broker → yfinance
    // fallback). The indicators are computed natively over the whole series on a worker thread
    // (a 5-year daily series is ~5 ms, a 'max' history ~40 ms). parse_technicals() reads the last
    // row plus the last 50 closes (SMA 50), so only the frame's 60-row tail is built and cached.
    QPointer<EquityResearchService> self = this;
    ensure_candles(symbol, period, [self, symbol, period, inflight_key, result_key](bool ok, const QString& hist_json) {
        if (!self)
            return;
        if (!ok || hist_json.trimmed().isEmpty()) {
            self->technicals_inflight_.remove(inflight_key);
            // On failure ensure_candles hands back the reason instead of the series.
            self->report_failure(symbol, QStringLiteral("Technicals"), period, ok ? QString() : hist_json, {},
                                 tr("No price history to compute indicators for %1").arg(symbol));
            return;
        }
        (void)QtConcurrent::run([self, symbol, period, inflight_key, result_key, hist_json]() {
            const QJsonArray bars_json = QJsonDocument::fromJson(hist_json.toUtf8()).array();
            const technicals::TechnicalsFrame frame = technicals::compute_frame(bars_json, 60);
            // The rating needs the whole series (SMA 200), not the frame's 60-row tail.
            const tmath::Rating rating = tmath::technical_rating(eqrs_bars_from_json(bars_json));
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, symbol, period, inflight_key, result_key, frame, rating]() {
                    if (!self)
                        return;
                    self->technicals_inflight_.remove(inflight_key);
                    if (!frame.ok) {
                        self->report_failure(symbol, QStringLiteral("Technicals"), period,
                                             tr("Indicator computation failed: %1").arg(frame.error), QStringLiteral("unknown"),
                                             tr("Indicator computation failed"));
                        return;
                    }
                    if (!frame.rows.isEmpty()) {
                        const QJsonObject cached{{QStringLiteral("rows"), frame.rows},
                                                 {QStringLiteral("rating"), eqrs_rating_to_json(rating)}};
                        fincept::CacheManager::instance().put(
                            result_key,
                            QVariant(QString::fromUtf8(QJsonDocument(cached).toJson(QJsonDocument::Compact))),
                            kTechnicalsTtlSec, "equity");
                    }
                    TechnicalsData td = self->parse_technicals(symbol, frame.rows);
                    eqrs_apply_rating(td, rating);
                    emit self->technicals_loaded(td);
                },
                Qt::QueuedConnection);
        });
    });
}

// ── Peers ─────────────────────────────────────────────────────────────────────
void EquityResearchService::fetch_suggested_peers(const QString& symbol, int count) {
    // Region-aware discovery (equity_research_data.py peers_suggest): same industry on the
    // subject's own market first (BT and Airtel Africa for Vodafone, Honda and Suzuki for
    // Toyota), then the industry's global leaders. Cached: the answer changes rarely.
    const QString cache_key = QStringLiteral("equity:peers_suggest:%1:%2").arg(symbol).arg(count);
    auto emit_from = [this, symbol](const QJsonObject& obj) {
        QStringList peers;
        for (const auto& v : obj.value("peers").toArray())
            if (!v.toString().trimmed().isEmpty())
                peers.append(v.toString().trimmed().toUpper());
        emit suggested_peers_loaded(symbol, peers);
    };
    if (const auto hit = fincept::CacheManager::instance().try_get(cache_key)) {
        const QJsonObject obj = QJsonDocument::fromJson(hit->toUtf8()).object();
        if (!obj.value("peers").toArray().isEmpty()) {
            emit_from(obj);
            return;
        }
    }
    run_python("equity_research_data.py", {"peers_suggest", symbol, QString::number(count)},
               [this, symbol, count, cache_key, emit_from](bool ok, const QString& out) {
                   const auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                   if (ok && !obj.value("peers").toArray().isEmpty()) {
                       fincept::CacheManager::instance().put(
                           cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                           kSuggestedPeersTtlSec, "equity");
                       emit_from(obj);
                       return;
                   }
                   // Older bundle without the new script: Yahoo's (US-centric) industry leaders.
                   run_python("fetch_peers.py", {symbol, QString::number(count)},
                              [this, symbol](bool ok2, const QString& out2) {
                                  QStringList peers;
                                  if (ok2) {
                                      const auto o2 = QJsonDocument::fromJson(python::extract_json(out2).toUtf8()).object();
                                      for (const auto& v : o2.value("peers").toArray())
                                          if (!v.toString().trimmed().isEmpty())
                                              peers.append(v.toString().trimmed().toUpper());
                                  }
                                  emit suggested_peers_loaded(symbol, peers);
                              });
               });
}

void EquityResearchService::fetch_peers(const QString& symbol, const QStringList& peer_symbols) {
    QStringList all;
    all << symbol;
    all.append(peer_symbols);
    all.removeDuplicates();
    const QString joined = all.join(",");
    // One pull per peer list (equity_research_data.py peers_compare): ratios, profile and
    // listing context — trading / reporting currency and a USD market cap, so caps listed
    // in pounds, yen and rupees compare.
    const QString cache_key = QStringLiteral("equity:peers:") + joined;
    if (const auto hit = fincept::CacheManager::instance().try_get(cache_key)) {
        const QJsonArray arr = QJsonDocument::fromJson(hit->toUtf8()).object().value("peers").toArray();
        if (!arr.isEmpty()) {
            emit peers_loaded(parse_peers(arr));
            return;
        }
    }
    run_python_coded("equity_research_data.py", {"peers_compare", joined},
                     [this, symbol, cache_key](bool ok, const QString& out, const QString& code) {
                         const QString fallback = tr("Failed to fetch peer data for %1").arg(symbol);
                         if (!ok) {
                             report_failure(symbol, QStringLiteral("Peers"), {}, out, code, fallback);
                             return;
                         }
                         const QJsonObject obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                         if (obj.contains("error")) {
                             report_failure(symbol, QStringLiteral("Peers"), {}, obj["error"].toString(),
                                            obj["code"].toString(), fallback);
                             return;
                         }
                         const QJsonArray arr = obj.value("peers").toArray();
                         if (!arr.isEmpty())
                             fincept::CacheManager::instance().put(
                                 cache_key,
                                 QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                                 kPeersTtlSec, "equity");
                         emit peers_loaded(parse_peers(arr));
                     });
}

// ── News ──────────────────────────────────────────────────────────────────────
// Turn a yfinance company_name into a Google-News-friendly query by dropping the
// trailing corporate suffix(es) and punctuation — "Reliance Industries Limited" →
// "Reliance Industries", "Tesla, Inc." → "Tesla". Materially improves GNews recall.
static QString gnews_query_from_company_name(const QString& company_name) {
    static const QStringList kSuffixes = {"Limited", "Ltd", "Incorporated", "Inc",      "Corporation", "Corp",
                                          "Company", "Co",  "PLC",          "Holdings", "Holding",     "Group",
                                          "SA",      "NV",  "AG",           "SE",       "SpA",         "AB"};
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

void EquityResearchService::fetch_news(const QString& symbol, int count, NewsProvider provider, bool force) {
    // User-pressed REFRESH: the 10-minute cache would otherwise hand back the very
    // same articles the tab is already showing.
    if (force) {
        fincept::CacheManager::instance().remove("equity:news:" + symbol);
        fincept::CacheManager::instance().remove("equity:news:newsapi:" + symbol);
    }

    // NewsAPI.org provider (opt-in): use it only when a key is configured, and
    // cache it under a separate key so switching providers doesn't surface the
    // other provider's cached result.
    if (provider == NewsProvider::NewsApi) {
        const QString key = configured_newsapi_key();
        if (!key.isEmpty()) {
            const QVariant ncv = fincept::CacheManager::instance().get("equity:news:newsapi:" + symbol);
            if (!ncv.isNull()) {
                const QJsonArray arr = QJsonDocument::fromJson(ncv.toString().toUtf8()).array();
                emit news_loaded(symbol, NewsProvider::NewsApi, false, parse_news(arr));
                return;
            }
            fetch_news_newsapi(symbol, count, key);
            return;
        }
        // No key configured → the default Auto chain, tagged as a NewsAPI fallback.
    }
    fetch_news_auto(symbol, count, provider);
}

void EquityResearchService::fetch_news_auto(const QString& symbol, int count, NewsProvider requested) {
    const bool fell_back = requested != NewsProvider::Auto;
    {
        const QVariant ncv = fincept::CacheManager::instance().get("equity:news:" + symbol);
        if (!ncv.isNull()) {
            const QJsonArray arr = QJsonDocument::fromJson(ncv.toString().toUtf8()).array();
            emit news_loaded(symbol, requested, fell_back, parse_news(arr));
            return;
        }
    }

    // Primary source: Google News (fetch_company_news.py) — broader, more current
    // company coverage than yfinance's Yahoo feed. Google News recall is far better
    // for a clean company NAME than a bare ticker or the full legal name (e.g.
    // "Reliance Industries" returns articles where "RELIANCE" and "Reliance
    // Industries Limited" both return none), so query by the cleaned company_name
    // from the cached info when available, falling back to the bare ticker.
    // Google News edition of the listing's home market (VOD.L → GB, 7203.T → JP, SAP.DE →
    // DE), in the language its business press writes in; English where that is the
    // business language (IN, SG, HK, ZA, …) or the market is unknown.
    QString country, language;
    eqrs_news_locale(symbol, &country, &language);
    const QString query = news_query_for_symbol(symbol);

    run_python("fetch_company_news.py",
               // GNews periods are h/d/m/y with "m" meaning minutes on Google News, so "1M"
               // returned zero articles for every symbol. 30 days gives a month of coverage.
               {"search", query, QString::number(count), QStringLiteral("30d"), language, country},
               [this, symbol, count, requested, fell_back](bool ok, const QString& out) {
                   if (ok) {
                       const auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
                       // GNews returns {"success":true,"data":[...]}. Only accept a
                       // non-empty result; anything else falls through to yfinance.
                       if (obj.value("success").toBool()) {
                           const auto arr = obj.value("data").toArray();
                           if (!arr.isEmpty()) {
                               fincept::CacheManager::instance().put(
                                   "equity:news:" + symbol,
                                   QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                                   kNewsTtlSec, "equity");
                               emit news_loaded(symbol, requested, fell_back, parse_news(arr));
                               return;
                           }
                       }
                   }
                   // Google News unavailable (e.g. gnews missing / network) or empty →
                   // fall back to the Yahoo/yfinance feed.
                   fetch_news_yfinance(symbol, count, requested);
               });
}

void EquityResearchService::fetch_news_yfinance(const QString& symbol, int count, NewsProvider requested) {
    run_python_coded(
        "yfinance_data.py", {"news", symbol, QString::number(count)},
        [this, symbol, requested](bool ok, const QString& out, const QString& code) {
            if (!ok) {
                report_failure(symbol, QStringLiteral("News"), {}, out, code,
                               tr("Failed to fetch news for %1").arg(symbol));
                return;
            }
            auto obj = QJsonDocument::fromJson(python::extract_json(out).toUtf8()).object();
            auto arr = obj["articles"].toArray();
            fincept::CacheManager::instance().put(
                "equity:news:" + symbol, QVariant(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact))),
                kNewsTtlSec, "equity");
            emit news_loaded(symbol, requested, requested != NewsProvider::Auto, parse_news(arr));
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
    {
        // NewsAPI's languages: ar de en es fr he it nl no pt ru sv ud zh.
        QString country, language;
        eqrs_news_locale(symbol, &country, &language);
        static const QStringList kNewsApiLanguages = {"ar", "de", "en", "es", "fr", "he", "it",
                                                      "nl", "no", "pt", "ru", "sv", "zh"};
        params.addQueryItem(QStringLiteral("language"),
                            kNewsApiLanguages.contains(language) ? language : QStringLiteral("en"));
    }
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
        // Answered as a NewsAPI result flagged fell_back, so the News tab (on NewsAPI) shows it
        // and can say where the articles came from.
        const auto fall_back = [this, symbol, count]() { fetch_news_auto(symbol, count, NewsProvider::NewsApi); };

        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("EquityResearch", QString("NewsAPI request failed (%1) — falling back").arg(reply->errorString()));
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
        emit news_loaded(symbol, NewsProvider::NewsApi, false, parse_news(reshaped));
    });
}

// ── Candle source: connected broker first, yfinance fallback ─────────────────
// Shared by fetch_technicals (and any future indicator path). Order:
//   1. cache hit                                 → return immediately
//   2. region-matched *connected* broker         → broker get_history (live, reliable)
//   3. yfinance                                  → fallback (no broker, or broker empty/failed)
// The result is cached under "equity:candles:<symbol>:<period>" — the key
// load_historical_only() uses, so the two share one copy per period (it used to be
// keyed without the period, and Technicals could compute on another period's bars).
// done(ok, hist_json_or_reason) is always invoked on the main thread.
void EquityResearchService::ensure_candles(const QString& symbol, const QString& period,
                                           std::function<void(bool, const QString&)> done) {
    const QString cache_key = "equity:candles:" + symbol + ":" + period;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    // Reject a cached empty/garbage payload — a stale rate-limited result would otherwise
    // wedge the tab on "No data returned".
    if (!cached.isNull() && eqrs_history_array(cached.toString(), nullptr)) {
        done(true, cached.toString());
        return;
    }

    QPointer<EquityResearchService> self = this;
    auto run_yfinance = [self, symbol, period, cache_key, done]() {
        if (!self) {
            done(false, {});
            return;
        }
        self->run_python("yfinance_data.py", {"historical_period", symbol, period, "1d"},
                         [symbol, period, cache_key, done](bool ok, const QString& out) {
                             if (!ok) {
                                 done(false, out); // the runner / script reason
                                 return;
                             }
                             const QString raw = python::extract_json(out);
                             QJsonArray arr;
                             if (!eqrs_history_array(raw, &arr)) {
                                 const QJsonObject err = QJsonDocument::fromJson(raw.toUtf8()).object();
                                 const QString reason = err.value(QStringLiteral("error")).toString();
                                 // yfinance rate-limited / unknown symbol / blocked network
                                 done(false, reason.isEmpty() ? tr("No price history returned for %1 (%2)")
                                                                    .arg(symbol, period)
                                                              : reason);
                                 return;
                             }
                             const QString compact = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
                             fincept::CacheManager::instance().put(cache_key, QVariant(compact), kHistoricalTtlSec,
                                                                   "equity");
                             done(true, compact);
                         });
    };

    QString broker_id, account_id;
    if (resolve_connected_data_broker(symbol, broker_id, account_id)) {
        const QString bare = candle_bare_symbol(symbol);
        const int lookback = candle_lookback_days(period);
        LOG_DEBUG("EquityResearch", QString("Candles for %1 via broker %2").arg(symbol, broker_id));
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
    s.financial_currency = o["financial_currency"].toString(); // absent from older payloads → empty
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
    fd.fetched_at = static_cast<qint64>(obj["timestamp"].toDouble());

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

    // Statement order of the line items (absent in payloads cached before the script sent it).
    const QJsonObject order = obj["row_order"].toObject();
    auto parse_order = [&order](const char* key) {
        QStringList out;
        for (const auto& v : order.value(QLatin1String(key)).toArray())
            out << v.toString();
        return out;
    };
    fd.income_order = parse_order("income_statement");
    fd.balance_order = parse_order("balance_sheet");
    fd.cash_flow_order = parse_order("cash_flow");

    // Quarterly statements and Yahoo's TTM columns (absent from payloads cached before the
    // script sent them, and for listings that file no quarters).
    const QJsonObject q = obj["quarterly"].toObject();
    fd.q_income = parse_stmt(q["income_statement"].toObject());
    fd.q_balance = parse_stmt(q["balance_sheet"].toObject());
    fd.q_cash_flow = parse_stmt(q["cash_flow"].toObject());
    fd.q_income_order = parse_order("quarterly_income_statement");
    fd.q_balance_order = parse_order("quarterly_balance_sheet");
    fd.q_cash_flow_order = parse_order("quarterly_cash_flow");
    const QJsonObject ttm = obj["ttm"].toObject();
    fd.ttm_income = parse_stmt(ttm["income_statement"].toObject());
    fd.ttm_cash_flow = parse_stmt(ttm["cash_flow"].toObject());
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
    // MACD vote = the histogram ("macd_diff" = MACD line − signal line): the classic
    // crossover signal. The line itself (12-EMA − 26-EMA) used to be scored instead and was
    // labelled "MACD above signal line" whenever it was merely above zero; it is now shown
    // for information only, so MACD still casts a single vote in the rating.
    if (name == "macd_diff") {
        if (value > 0)
            return TechSignal::Buy;
        if (value < 0)
            return TechSignal::Sell;
        return TechSignal::Neutral;
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

    // Use the last row (most recent values) — Technicals frame columns are lowercase snake_case
    auto last = rows.last().toObject();

    // The frame (the `ta` library's default set) has sma_20 but no sma_50, which the "SMA 50" row
    // and the SMA 20/50 crossover signal below need. Every row carries its close, so derive the
    // 50-bar mean here.
    if (rows.size() >= 50 && (!last.contains("sma_50") || last["sma_50"].isNull())) {
        double sum = 0.0;
        bool complete = true;
        for (qsizetype i = rows.size() - 50; i < rows.size(); ++i) {
            const QJsonValue close = rows.at(i).toObject().value("close");
            if (!close.isDouble()) {
                complete = false;
                break;
            }
            sum += close.toDouble();
        }
        if (complete)
            last["sma_50"] = sum / 50.0;
    }

    // Extract SMA values for cross scoring
    double sma20 = last["sma_20"].toDouble();
    double sma50 = last["sma_50"].toDouble(); // may be 0 if not present

    // Maps: display name → actual column key in JSON output
    // Trend: sma_20, sma_50, ema_12, wma_9, macd, macd_signal, cci, adx, aroon_up, aroon_down
    static const QList<QPair<QString, QString>> kTrend = {
        {"SMA 20", "sma_20"},       {"SMA 50", "sma_50"},       {"EMA 12", "ema_12"},
        {"WMA 9", "wma_9"},         {"MACD", "macd"},           {"MACD Signal", "macd_signal"},
        {"MACD Hist", "macd_diff"}, {"CCI", "cci"},             {"ADX", "adx"},
        {"Aroon Up", "aroon_up"},   {"Aroon Down", "aroon_down"},
    };
    // Momentum: rsi, stoch_k, stoch_d, williams_r, roc, mfi, ao, kama
    static const QList<QPair<QString, QString>> kMomentum = {
        {"RSI", "rsi"}, {"Stoch %K", "stoch_k"}, {"Stoch %D", "stoch_d"}, {"Williams %R", "williams_r"},
        {"ROC", "roc"}, {"MFI", "mfi"},          {"Awesome Osc", "ao"},   {"KAMA", "kama"},
    };
    // Volatility: atr, bb_mavg, bb_hband, bb_lband, bb_pband, bb_wband
    // bb_wband comes from ta.BollingerBands.bollinger_wband(): (upper − lower) / mid × 100,
    // i.e. already in PERCENT.
    static const QList<QPair<QString, QString>> kVolatility = {
        {"ATR", "atr"},           {"BB Mid", "bb_mavg"}, {"BB Upper", "bb_hband"},
        {"BB Lower", "bb_lband"}, {"BB %B", "bb_pband"}, {"BB Width %", "bb_wband"},
    };
    // Volume: obv, vwap, cmf, adi. ta's VWAP is a 14-bar ROLLING volume-weighted price, not
    // the session VWAP traders usually mean — say so in the name.
    static const QList<QPair<QString, QString>> kVolume = {
        {"OBV", "obv"},
        {"VWAP (14-bar)", "vwap"},
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
            ti.key = col;
            ti.value = val;
            ti.category = cat;
            ti.signal = score_indicator(col, val, sma20, sma50);
            // ADX only measures trend STRENGTH. score_indicator() maps ADX > 25 to Buy, which
            // called a strong downtrend bullish and tipped the overall rating; take the side
            // from the +DI / -DI pair the same row carries.
            if (col == QLatin1String("adx") && ti.signal == TechSignal::Buy) {
                const double plus_di = last["adx_pos"].toDouble();
                const double minus_di = last["adx_neg"].toDouble();
                if (plus_di < minus_di)
                    ti.signal = TechSignal::Sell;
                else if (plus_di == minus_di)
                    ti.signal = TechSignal::Neutral;
            }
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
    // equity_research_data.py peers_compare rows (snake_case; null = not reported → 0, the
    // "missing" convention of PeerData).
    for (const auto& v : arr) {
        auto o = v.toObject();
        PeerData p;
        p.symbol = o["symbol"].toString();
        p.name = o["name"].toString();
        p.sector = o["sector"].toString();
        p.industry = o["industry"].toString();
        p.country = o["country"].toString();
        p.exchange = o["exchange"].toString();
        p.currency = o["currency"].toString();
        p.financial_currency = o["financial_currency"].toString();
        p.price = o["price"].toDouble();
        p.change_pct = o["change_pct"].toDouble();
        p.market_cap = o["market_cap"].toDouble();
        p.market_cap_usd = o["market_cap_usd"].toDouble();
        p.enterprise_value = o["enterprise_value"].toDouble();
        p.pe_ratio = o["pe"].toDouble();
        p.forward_pe = o["forward_pe"].toDouble();
        p.price_to_book = o["pb"].toDouble();
        p.price_to_sales = o["ps"].toDouble();
        p.peg_ratio = o["peg"].toDouble();
        p.ev_to_ebitda = o["ev_ebitda"].toDouble();
        p.ev_to_revenue = o["ev_revenue"].toDouble();
        p.debt_to_equity = o["debt_to_equity"].toDouble(); // percent (152 = 1.52x)
        p.roe = o["roe"].toDouble();
        p.roa = o["roa"].toDouble();
        p.profit_margin = o["profit_margin"].toDouble();
        p.operating_margin = o["operating_margin"].toDouble();
        p.gross_margin = o["gross_margin"].toDouble();
        p.current_ratio = o["current_ratio"].toDouble();
        p.quick_ratio = o["quick_ratio"].toDouble();
        p.dividend_yield = o["dividend_yield_pct"].toDouble(); // percent
        p.revenue_growth = o["revenue_growth"].toDouble();
        p.earnings_growth = o["earnings_growth"].toDouble();
        p.beta = o["beta"].toDouble();
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
    // Tone per headline — the finance-tuned VADER model the News tab and the Sentiment
    // tab score with, so all three agree on the same story. Pure, in-process, ~µs each.
    if (!articles.isEmpty()) {
        QVector<news_nlp::TextItem> items;
        items.reserve(articles.size());
        for (int i = 0; i < articles.size(); ++i)
            items.append({QString::number(i), articles[i].title, articles[i].description});
        const NewsSentimentResult sr = news_nlp::analyze_sentiment(items);
        for (const auto& r : sr.per_article) {
            bool ok = false;
            const int i = r.id.toInt(&ok);
            if (ok && i >= 0 && i < articles.size()) {
                articles[i].sentiment = r.label;
                articles[i].sentiment_score = r.score;
            }
        }
    }
    return articles;
}

} // namespace fincept::services::equity
