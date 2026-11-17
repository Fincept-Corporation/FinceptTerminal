#include "services/prediction/kalshi/KalshiAdapter.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/prediction/kalshi/KalshiMarketProducer.h"
#include "services/prediction/kalshi/KalshiRestClient.h"
#include "services/prediction/kalshi/KalshiWsClient.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace fincept::services::prediction::kalshi_ns {

namespace pr = fincept::services::prediction;

namespace {

// The bridge reports a rejection as {"ok":false,"error":"HTTP 400","detail":{…}}.
// PythonRunner reduces that to just the "error" text, so the ticket / status bar
// showed "HTTP 400" while Kalshi's actual reason sat unread in "detail". Append it
// (compacted to one line, capped) to whatever message we already have.
QString kalshi_error_with_detail(const QString& base, const QJsonObject& obj) {
    QString msg = base.trimmed();
    if (msg.isEmpty())
        msg = obj.value(QStringLiteral("error")).toString().trimmed();
    const QJsonValue d = obj.value(QStringLiteral("detail"));
    QString detail;
    if (d.isString())
        detail = d.toString();
    else if (d.isObject())
        detail = QString::fromUtf8(QJsonDocument(d.toObject()).toJson(QJsonDocument::Compact));
    else if (d.isArray())
        detail = QString::fromUtf8(QJsonDocument(d.toArray()).toJson(QJsonDocument::Compact));
    detail = detail.simplified();
    if (detail.size() > 200)
        detail = detail.left(200) + QStringLiteral("…");
    if (!detail.isEmpty() && !msg.contains(detail))
        msg = msg.isEmpty() ? detail : msg + QStringLiteral(" — ") + detail;
    return msg;
}

// ── Account-history row normalisation ───────────────────────────────────────
// The bridge passes Kalshi's settlement / fill / order objects through as-is.
// Counts and dollar amounts are fixed-point strings ("10.00", "0.5600"); a plain
// number is accepted too.
double fixed_point(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString().toDouble() : v.toDouble();
}

qint64 iso_to_ms(QString iso) {
    if (iso.isEmpty())
        return 0;
    // Kalshi stamps can carry microseconds; keep the milliseconds Qt parses.
    static const QRegularExpression kFraction(QStringLiteral("(\\.\\d{3})\\d+"));
    iso.replace(kFraction, QStringLiteral("\\1"));
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    return dt.isValid() ? dt.toMSecsSinceEpoch() : 0;
}

// `side` + `action` are deprecated in favour of `outcome_side` — the outcome the
// fill / order positions you for (buy-yes and sell-no both give "yes"). Use the
// explicit pair while Kalshi still sends it; otherwise the outcome, with no action.
// Returns (OUTCOME, ACTION) upper-cased; the price field follows the outcome.
std::pair<QString, QString> outcome_and_action(const QJsonObject& o) {
    const QString side = o.value(QStringLiteral("side")).toString().toUpper();
    const QString action = o.value(QStringLiteral("action")).toString().toUpper();
    if (!side.isEmpty() && !action.isEmpty())
        return {side, action};
    return {o.value(QStringLiteral("outcome_side")).toString().toUpper(), QString()};
}

double outcome_price(const QJsonObject& o, const QString& outcome) {
    return fixed_point(o, outcome == QStringLiteral("NO") ? "no_price_dollars" : "yes_price_dollars");
}

// Keys: ts_ms, market_id, event_id, result, yes_count, no_count, cost, payout, fees, net.
// revenue is integer cents (100 per winning contract); the rest are dollars.
QVariantMap settlement_row(const QJsonObject& s) {
    QVariantMap m;
    const double cost = fixed_point(s, "yes_total_cost_dollars") + fixed_point(s, "no_total_cost_dollars");
    const double payout = s.value(QStringLiteral("revenue")).toDouble() / 100.0;
    const double fees = fixed_point(s, "fee_cost");
    m.insert(QStringLiteral("ts_ms"), iso_to_ms(s.value(QStringLiteral("settled_time")).toString()));
    m.insert(QStringLiteral("market_id"), s.value(QStringLiteral("ticker")).toString());
    m.insert(QStringLiteral("event_id"), s.value(QStringLiteral("event_ticker")).toString());
    m.insert(QStringLiteral("result"), s.value(QStringLiteral("market_result")).toString().toUpper());
    m.insert(QStringLiteral("yes_count"), fixed_point(s, "yes_count_fp"));
    m.insert(QStringLiteral("no_count"), fixed_point(s, "no_count_fp"));
    m.insert(QStringLiteral("cost"), cost);
    m.insert(QStringLiteral("payout"), payout);
    m.insert(QStringLiteral("fees"), fees);
    m.insert(QStringLiteral("net"), payout - cost - fees);
    return m;
}

// Keys: ts_ms, market_id, outcome, action, price, size, fees, is_taker, order_id, fill_id.
QVariantMap fill_row(const QJsonObject& f) {
    QVariantMap m;
    const auto [outcome, action] = outcome_and_action(f);
    qint64 ts_ms = iso_to_ms(f.value(QStringLiteral("created_time")).toString());
    if (ts_ms == 0)
        ts_ms = qint64(f.value(QStringLiteral("ts")).toDouble()) * 1000; // legacy unix seconds
    QString ticker = f.value(QStringLiteral("ticker")).toString();
    if (ticker.isEmpty())
        ticker = f.value(QStringLiteral("market_ticker")).toString();
    m.insert(QStringLiteral("ts_ms"), ts_ms);
    m.insert(QStringLiteral("market_id"), ticker);
    m.insert(QStringLiteral("outcome"), outcome);
    m.insert(QStringLiteral("action"), action);
    m.insert(QStringLiteral("price"), outcome_price(f, outcome));
    m.insert(QStringLiteral("size"), fixed_point(f, "count_fp"));
    m.insert(QStringLiteral("fees"), fixed_point(f, "fee_cost"));
    m.insert(QStringLiteral("is_taker"), f.value(QStringLiteral("is_taker")).toBool());
    m.insert(QStringLiteral("order_id"), f.value(QStringLiteral("order_id")).toString());
    m.insert(QStringLiteral("fill_id"), f.value(QStringLiteral("fill_id")).toString());
    return m;
}

// Keys: ts_ms, updated_ms, market_id, outcome, action, order_type, price, size, filled,
// status, fees, order_id.
QVariantMap order_row(const QJsonObject& o) {
    QVariantMap m;
    const auto [outcome, action] = outcome_and_action(o);
    const double initial = fixed_point(o, "initial_count_fp");
    const double filled = o.contains(QStringLiteral("fill_count_fp"))
                              ? fixed_point(o, "fill_count_fp")
                              : qMax(0.0, initial - fixed_point(o, "remaining_count_fp"));
    m.insert(QStringLiteral("ts_ms"), iso_to_ms(o.value(QStringLiteral("created_time")).toString()));
    m.insert(QStringLiteral("updated_ms"), iso_to_ms(o.value(QStringLiteral("last_update_time")).toString()));
    m.insert(QStringLiteral("market_id"), o.value(QStringLiteral("ticker")).toString());
    m.insert(QStringLiteral("outcome"), outcome);
    m.insert(QStringLiteral("action"), action);
    m.insert(QStringLiteral("order_type"), o.value(QStringLiteral("type")).toString().toUpper());
    m.insert(QStringLiteral("price"), outcome_price(o, outcome));
    m.insert(QStringLiteral("size"), initial);
    m.insert(QStringLiteral("filled"), filled);
    m.insert(QStringLiteral("status"), o.value(QStringLiteral("status")).toString().toUpper());
    m.insert(QStringLiteral("fees"), fixed_point(o, "taker_fees_dollars") + fixed_point(o, "maker_fees_dollars"));
    m.insert(QStringLiteral("order_id"), o.value(QStringLiteral("order_id")).toString());
    return m;
}

// Orders audit (policy B.7): one AUDIT line when an order / cancel / amend leaves the
// terminal and one with its outcome, paired by client_id (the order's client_order_id
// when it has one, else a local correlation id).
struct KxOrderAudit {
    enum class Kind { None, Place, Cancel, CancelBatch, Amend };
    Kind kind = Kind::None;
    QString client_id;
    QString account;
    QString order_id;
    QString source; // who placed it (OrderRequest::source); empty = not stated
    QElapsedTimer clock;
};

// `extra` key carrying OrderRequest::source from place_order to run_py's audit lines;
// run_py never copies it into the payload it sends.
constexpr const char* kKxSourceKey = "_fincept_source";

// `answered`: the bridge replied with JSON (a definite refusal); otherwise the outcome is unknown.
void kx_audit_outcome(const KxOrderAudit& a, bool ok, bool answered, const QString& order_id, const QString& status,
                      const QString& reason) {
    using Kind = KxOrderAudit::Kind;
    if (a.kind == Kind::None)
        return;
    const char* sentence = nullptr;
    if (a.kind == Kind::Place)
        sentence = ok ? "Order accepted" : (answered ? "Order rejected" : "Order outcome unknown");
    else if (a.kind == Kind::Amend)
        sentence = ok ? "Order modified" : (answered ? "Order modify rejected" : "Order modify outcome unknown");
    else
        sentence = ok ? "Order cancel accepted" : (answered ? "Order cancel rejected" : "Order cancel outcome unknown");
    QList<LogField> f{{"venue", "kalshi"}, {"account", a.account}, {"client_id", a.client_id}};
    if (!order_id.isEmpty())
        f.append(LogField("order_id", order_id));
    if (!status.isEmpty())
        f.append(LogField("status", status));
    if (!ok)
        f.append(LogField("reason", answered ? reason : QStringLiteral("no_answer")));
    f.append(LogField("elapsed_ms", a.clock.elapsed()));
    if (!ok && !answered)
        f.append(LogField("error", reason));
    if (!a.source.isEmpty())
        f.append(LogField("source", a.source));
    LOG_AUDIT("Orders", logf(QString::fromLatin1(sentence), f));
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

KalshiAdapter::KalshiAdapter(QObject* parent)
    : fincept::services::prediction::PredictionExchangeAdapter(parent),
      rest_(std::make_unique<KalshiRestClient>(this)),
      ws_(std::make_unique<KalshiWsClient>(this)) {
    wire();
}

KalshiAdapter::~KalshiAdapter() = default;

// ── Identity ────────────────────────────────────────────────────────────────

QString KalshiAdapter::id() const {
    return QStringLiteral("kalshi");
}
QString KalshiAdapter::display_name() const {
    return QStringLiteral("Kalshi");
}

pr::ExchangeCapabilities KalshiAdapter::capabilities() const {
    pr::ExchangeCapabilities c;
    c.has_events = true;
    c.has_multi_outcome = false; // binary yes/no only
    // KalshiWsClient never opens its socket (ensure_connected() only logs), so
    // nothing streams: the screen polls the selected market instead (on_refresh).
    // Flip these back once the authenticated WS connect is real.
    c.has_orderbook_ws = false;
    c.has_trade_ws = false;
    c.has_rewards = false;
    c.has_maker_rebates = false;
    c.has_leaderboard = false;
    c.supports_limit_orders = true;
    c.supports_market_orders = true;
    c.supports_gtd = true;
    c.supports_fok = false; // the Python bridge has no time-in-force field (see place_order)
    c.supports_fak = false;
    c.supports_decrease_order = true;
    c.supports_batch_orders = true;
    c.quote_currency = QStringLiteral("USD");
    c.min_order_size = 1.0;     // 1 contract
    c.default_tick_size = 0.01; // 1 cent
    c.max_requests_per_sec = 2; // conservative for standard tier
    return c;
}

// ── Asset ID helpers ────────────────────────────────────────────────────────

std::pair<QString, QString> KalshiAdapter::split_asset_id(const QString& asset_id) {
    const int colon = asset_id.lastIndexOf(':');
    if (colon <= 0)
        return {};
    return {asset_id.left(colon), asset_id.mid(colon + 1)};
}

// ── Read endpoints ──────────────────────────────────────────────────────────

void KalshiAdapter::list_markets(const QString& category, const QString& /*sort_by*/, int limit, int /*offset*/) {
    // Kalshi uses cursor-based pagination. For MVP we serve the first page;
    // pagination wiring is Phase 8 polish.
    const QString series = (category.isEmpty() || category == QStringLiteral("ALL")) ? QString() : category;
    rest_->fetch_markets(QStringLiteral("open"), QString(), series, QString(), limit, QString());
}

void KalshiAdapter::list_events(const QString& category, const QString& /*sort_by*/, int limit, int /*offset*/) {
    const QString series = (category.isEmpty() || category == QStringLiteral("ALL")) ? QString() : category;
    rest_->fetch_events(QStringLiteral("open"), series, /*with_nested_markets=*/true, limit, QString());
}

void KalshiAdapter::list_events_by_status(const QString& status, const QString& category, int limit) {
    const QString series = (category.isEmpty() || category == QStringLiteral("ALL")) ? QString() : category;
    rest_->fetch_events(status, series, /*with_nested_markets=*/true, limit, QString());
}

void KalshiAdapter::search(const QString& query, int limit) {
    // Kalshi has no free-text search endpoint. The query used to be sent as a
    // series_ticker, so anything but an exact ticker ("bitcoin") returned 0 rows.
    // Now: an exact series ticker keeps that path; otherwise the query is matched
    // (every word, case-insensitive) against the series catalogue list_tags()
    // already downloads (ticker / title / category / tags), and the open markets
    // of the best-matching series are fetched and merged.
    //
    // Replies go through per-request callbacks tagged with a sequence number, so
    // a superseded or failed search can neither leak into nor block a later list
    // load (the old search_pending_ flag stayed set after a failed request and
    // turned the next ordinary list into "search results").
    const int seq = ++search_seq_;
    const QString q = query.trimmed();
    QPointer<KalshiAdapter> self = this;
    const auto deliver_later = [this, self, seq](const QVector<pr::PredictionMarket>& markets) {
        QMetaObject::invokeMethod(
            this,
            [self, seq, markets]() {
                if (self && seq == self->search_seq_)
                    emit self->search_results_ready(markets, {});
            },
            Qt::QueuedConnection);
    };
    if (q.isEmpty()) {
        deliver_later({});
        return;
    }

    constexpr int kMaxSeries = 6;
    const auto& index = rest_->series_search_index();
    const QString upper = q.toUpper();
    QStringList series;
    if (index.contains(upper) || (index.isEmpty() && !q.contains(QLatin1Char(' ')))) {
        series << upper; // an exact series ticker (or no catalogue yet: best effort, as before)
    } else {
        const QString lower = q.toLower();
        const QStringList terms = lower.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        struct Hit {
            int score;
            QString updated;
            QString ticker;
        };
        QVector<Hit> hits;
        for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
            const auto& e = it.value();
            bool all = true;
            for (const auto& t : terms) {
                if (!e.haystack.contains(t)) {
                    all = false;
                    break;
                }
            }
            if (!all)
                continue;
            int score = 1;
            if (it.key().startsWith(upper))
                score = 4;
            else if (e.title.contains(lower))
                score = 3;
            else if (std::all_of(terms.cbegin(), terms.cend(), [&e](const QString& t) { return e.title.contains(t); }))
                score = 2;
            hits.push_back({score, e.updated, it.key()});
        }
        // Best match first; among equals the most recently updated series (old,
        // retired series keep their titles but have no open markets).
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
            if (a.score != b.score)
                return a.score > b.score;
            if (a.updated != b.updated)
                return a.updated > b.updated;
            return a.ticker < b.ticker;
        });
        for (int i = 0; i < hits.size() && series.size() < kMaxSeries; ++i)
            series << hits[i].ticker;
    }
    if (series.isEmpty()) {
        deliver_later({});
        return;
    }

    struct SearchState {
        int remaining = 0;
        int failures = 0;
        QString last_error;
        QVector<QVector<pr::PredictionMarket>> per_series; // kept in match order
    };
    auto state = std::make_shared<SearchState>();
    state->remaining = static_cast<int>(series.size());
    state->per_series.resize(series.size());
    const int per_series_limit = qBound(10, limit, 200);
    const int total_cap = qMax(limit, 50) * 2;

    const auto finish_one = [self, seq, state, total_cap]() {
        if (--state->remaining > 0 || !self || seq != self->search_seq_)
            return;
        if (state->failures == static_cast<int>(state->per_series.size())) {
            emit self->error_occurred(QStringLiteral("search"), state->last_error);
            emit self->search_results_ready({}, {});
            return;
        }
        QVector<pr::PredictionMarket> merged;
        for (auto& chunk : state->per_series) {
            // Most-traded first within each series.
            std::stable_sort(chunk.begin(), chunk.end(), [](const pr::PredictionMarket& a, const pr::PredictionMarket& b) {
                return a.volume > b.volume;
            });
            for (const auto& m : chunk) {
                if (merged.size() >= total_cap)
                    break;
                merged.push_back(m);
            }
        }
        for (const auto& m : merged)
            self->remember_market(m);
        emit self->search_results_ready(merged, {});
    };

    for (int i = 0; i < series.size(); ++i) {
        rest_->fetch_open_series_markets(
            series[i], per_series_limit,
            [state, i, finish_one](const QVector<pr::PredictionMarket>& markets) {
                state->per_series[i] = markets;
                finish_one();
            },
            [state, finish_one](const QString& err) {
                ++state->failures;
                state->last_error = err;
                finish_one();
            });
    }
}

void KalshiAdapter::list_tags() {
    rest_->fetch_series(QStringLiteral("open"));
}

void KalshiAdapter::fetch_market(const pr::MarketKey& key) {
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_market"), QStringLiteral("Kalshi fetch_market requires a ticker"));
        return;
    }
    rest_->fetch_market(key.market_id);
}

void KalshiAdapter::fetch_event(const pr::MarketKey& key) {
    if (key.event_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_event"),
                            QStringLiteral("Kalshi fetch_event requires an event_ticker"));
        return;
    }
    rest_->fetch_event(key.event_id);
}

void KalshiAdapter::fetch_order_book(const QString& asset_id) {
    const auto [ticker, side] = split_asset_id(asset_id);
    if (ticker.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_order_book"),
                            QStringLiteral("Kalshi asset_id must be '<ticker>:yes|no'"));
        return;
    }
    rest_->fetch_order_book(ticker, 20);
}

void KalshiAdapter::fetch_price_history(const QString& asset_id, const QString& interval, int /*fidelity*/) {
    const auto [ticker, side] = split_asset_id(asset_id);
    if (ticker.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_price_history"),
                            QStringLiteral("Kalshi asset_id must be '<ticker>:yes|no'"));
        return;
    }
    last_history_asset_id_ = asset_id;

    // Map UI interval to Kalshi's period_interval (minutes). Kalshi only
    // supports 1, 60, 1440.
    int period = 60;
    if (interval == QStringLiteral("1h") || interval == QStringLiteral("6h"))
        period = 60;
    else if (interval == QStringLiteral("1d"))
        period = 60;
    else if (interval == QStringLiteral("1w") || interval == QStringLiteral("1m") || interval == QStringLiteral("max"))
        period = 1440;
    else if (interval == QStringLiteral("1min") || interval == QStringLiteral("5min"))
        period = 1;

    // Default to last 7 days if the caller didn't give us a range.
    const qint64 end = QDateTime::currentSecsSinceEpoch();
    qint64 start = end - (qint64{7} * 24 * 3600);
    if (interval == QStringLiteral("1d"))
        start = end - (qint64{24} * 3600); // 1D (the default) used to fall through to 7 days
    else if (interval == QStringLiteral("1w"))
        start = end - (qint64{7} * 24 * 3600);
    else if (interval == QStringLiteral("1m"))
        start = end - (qint64{30} * 24 * 3600);
    else if (interval == QStringLiteral("6h"))
        start = end - (qint64{6} * 3600);
    else if (interval == QStringLiteral("1h"))
        start = end - 3600;
    else if (interval == QStringLiteral("max")) {
        // ALL: from the market's open time (daily candles); a year when the open
        // time is unknown. It used to be the 7-day default, i.e. 7 points.
        const qint64 opened = open_ts_by_market_.value(ticker, 0);
        start = (opened > 0 && opened < end) ? opened : end - (qint64{365} * 24 * 3600);
    }

    // Kalshi candlestick endpoint requires the series ticker. Prefer the
    // cached mapping populated from markets_ready/events_ready responses;
    // fall back to the prefix-before-first-dash heuristic for tickers we
    // haven't seen. Many Kalshi tickers have multi-segment structure (e.g.
    // KXPRES-24-DJT) so the heuristic is best-effort, not authoritative.
    QString series = series_by_market_.value(ticker);
    if (series.isEmpty()) {
        const int dash = ticker.indexOf('-');
        series = (dash > 0) ? ticker.left(dash) : ticker;
    }

    rest_->fetch_candlesticks(series, ticker, period, start, end);
}

void KalshiAdapter::fetch_recent_trades(const pr::MarketKey& key, int limit) {
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_recent_trades"), QStringLiteral("Kalshi trades require a ticker"));
        return;
    }
    rest_->fetch_market_trades(key.market_id, limit);
}

// ── WebSocket ───────────────────────────────────────────────────────────────

void KalshiAdapter::subscribe_market(const QStringList& asset_ids) {
    // Kalshi WS subscribes per market ticker, not per YES/NO asset. Dedup
    // the ticker portion of the asset IDs before forwarding.
    QStringList tickers;
    tickers.reserve(asset_ids.size());
    for (const auto& aid : asset_ids) {
        const auto [t, s] = split_asset_id(aid);
        if (!t.isEmpty() && !tickers.contains(t))
            tickers.push_back(t);
    }
    ws_->subscribe(tickers);
}

void KalshiAdapter::unsubscribe_market(const QStringList& asset_ids) {
    QStringList tickers;
    tickers.reserve(asset_ids.size());
    for (const auto& aid : asset_ids) {
        const auto [t, s] = split_asset_id(aid);
        if (!t.isEmpty() && !tickers.contains(t))
            tickers.push_back(t);
    }
    ws_->unsubscribe(tickers);
}

bool KalshiAdapter::is_ws_connected() const {
    return ws_->is_connected();
}

// ── Auth / trading (Python bridge) ──────────────────────────────────────────

bool KalshiAdapter::has_credentials() const {
    return creds_.is_valid();
}

QString KalshiAdapter::account_label() const {
    if (!creds_.is_valid())
        return {};
    return QStringLiteral("Kalshi:") + creds_.api_key_id.left(8);
}

void KalshiAdapter::stub_unsupported(const QString& ctx) {
    emit error_occurred(ctx, QStringLiteral("Kalshi adapter: not supported in this call"));
}

// prediction_kalshi.py also exposes a `decrease_order` command. It's reachable
// directly via PythonRunner for scripted testing but intentionally not surfaced
// on the adapter because no UI consumes a decrease-order control yet.

QJsonObject KalshiAdapter::creds_to_json() const {
    QJsonObject o;
    o.insert("api_key_id", creds_.api_key_id);
    o.insert("private_key_pem", creds_.private_key_pem);
    o.insert("use_demo", creds_.use_demo);
    return o;
}

void KalshiAdapter::run_py(const QString& command, const QJsonObject& extra,
                           std::function<void(const QJsonObject&)> on_ok, const QString& ctx) {
    if (!creds_.is_valid()) {
        emit error_occurred(ctx, QStringLiteral("No Kalshi credentials — connect an account first"));
        return;
    }
    QJsonObject payload = creds_to_json();
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        if (it.key() == QLatin1String(kKxSourceKey))
            continue; // provenance for the audit lines only — never sent
        payload.insert(it.key(), it.value());
    }
    // SECURITY: the payload carries the RSA private key PEM. It used to be argv[2],
    // readable by any same-user process (Win32_Process.CommandLine,
    // /proc/<pid>/cmdline) and captured in crash dumps. It now goes over the
    // child's stdin (PythonRunner writes it and closes the channel); the "--stdin"
    // marker tells prediction_kalshi.py to read it from there. Never log `payload`.
    const QByteArray payload_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // Orders audit. Only the order fields are read from `extra` — never the
    // credentials `payload` also carries.
    KxOrderAudit audit;
    if (command == QLatin1String("place_order"))
        audit.kind = KxOrderAudit::Kind::Place;
    else if (command == QLatin1String("cancel_order"))
        audit.kind = KxOrderAudit::Kind::Cancel;
    else if (command == QLatin1String("cancel_orders_batch"))
        audit.kind = KxOrderAudit::Kind::CancelBatch;
    else if (command == QLatin1String("amend_order"))
        audit.kind = KxOrderAudit::Kind::Amend;
    if (audit.kind != KxOrderAudit::Kind::None) {
        audit.client_id = extra.value(QStringLiteral("client_order_id")).toString();
        if (audit.client_id.isEmpty())
            audit.client_id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        audit.account = log_mask(creds_.api_key_id);
        audit.order_id = extra.value(QStringLiteral("order_id")).toString();
        audit.source = extra.value(QLatin1String(kKxSourceKey)).toString();
        audit.clock.start();
        const QString mode = creds_.use_demo ? QStringLiteral("demo") : QStringLiteral("live");
        const int cents = extra.contains(QStringLiteral("yes_price_cents"))
                              ? extra.value(QStringLiteral("yes_price_cents")).toInt()
                              : extra.value(QStringLiteral("no_price_cents")).toInt();
        if (audit.kind == KxOrderAudit::Kind::Place) {
            QList<LogField> f{{"venue", "kalshi"},
                              {"account", audit.account},
                              {"mode", mode},
                              {"symbol", extra.value(QStringLiteral("ticker")).toString()},
                              {"outcome", extra.value(QStringLiteral("side")).toString()},
                              {"side", extra.value(QStringLiteral("action")).toString()},
                              {"type", extra.value(QStringLiteral("order_type")).toString()},
                              {"qty", extra.value(QStringLiteral("count")).toInt()},
                              {"price", cents / 100.0},
                              {"client_id", audit.client_id}};
            if (!audit.source.isEmpty())
                f.append(LogField("source", audit.source));
            LOG_AUDIT("Orders", logf(QStringLiteral("Order submitted"), f));
        } else if (audit.kind == KxOrderAudit::Kind::Amend) {
            LOG_AUDIT("Orders", "Order modify requested",
                      {{"venue", "kalshi"},
                       {"account", audit.account},
                       {"mode", mode},
                       {"order_id", audit.order_id},
                       {"price", cents / 100.0},
                       {"client_id", audit.client_id}});
        } else if (audit.kind == KxOrderAudit::Kind::Cancel) {
            LOG_AUDIT("Orders", "Order cancel requested",
                      {{"venue", "kalshi"},
                       {"account", audit.account},
                       {"mode", mode},
                       {"order_id", audit.order_id},
                       {"client_id", audit.client_id}});
        } else {
            LOG_AUDIT("Orders", "Order cancel requested",
                      {{"venue", "kalshi"},
                       {"account", audit.account},
                       {"mode", mode},
                       {"scope", "batch"},
                       {"count", extra.value(QStringLiteral("order_ids")).toArray().size()},
                       {"client_id", audit.client_id}});
        }
    }

    QPointer<KalshiAdapter> self = this;
    fincept::python::PythonRunner::instance().run(
        QStringLiteral("prediction_kalshi.py"), {command, QStringLiteral("--stdin")},
        [self, on_ok, ctx, audit](fincept::python::PythonResult r) {
            if (!self)
                return;
            if (!r.success) {
                // r.output holds the script's JSON reply when it produced one.
                const QJsonObject reply =
                    QJsonDocument::fromJson(fincept::python::extract_json(r.output).toUtf8()).object();
                QString msg = kalshi_error_with_detail(r.error, reply);
                if (msg.isEmpty())
                    msg = r.output.trimmed().left(300);
                kx_audit_outcome(audit, false, !reply.isEmpty(), audit.order_id, QString(), msg);
                emit self->error_occurred(ctx, msg);
                return;
            }
            const QString json_str = fincept::python::extract_json(r.output);
            QJsonParseError perr;
            auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &perr);
            if (doc.isNull() || !doc.isObject()) {
                kx_audit_outcome(audit, false, false, audit.order_id, QString(),
                                 QStringLiteral("non-JSON reply: ") + perr.errorString());
                emit self->error_occurred(ctx,
                                          QStringLiteral("Kalshi bridge: non-JSON response — ") + perr.errorString());
                return;
            }
            const auto obj = doc.object();
            if (!obj.value("ok").toBool()) {
                const QString msg = kalshi_error_with_detail(QString(), obj);
                kx_audit_outcome(audit, false, true, audit.order_id, QString(),
                                 msg.isEmpty() ? QStringLiteral("Kalshi command failed") : msg);
                emit self->error_occurred(ctx, msg.isEmpty() ? QStringLiteral("Kalshi command failed") : msg);
                return;
            }
            kx_audit_outcome(audit, true, true,
                             audit.order_id.isEmpty() ? obj.value(QStringLiteral("order_id")).toString()
                                                      : audit.order_id,
                             obj.value(QStringLiteral("status")).toString(), QString());
            on_ok(obj);
        },
        {}, payload_bytes);
}

void KalshiAdapter::fetch_balance() {
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("balance"), {},
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            pr::AccountBalance b;
            b.available = resp.value("available").toDouble();
            b.total_value = resp.value("total_value").toDouble();
            b.currency = resp.value("currency").toString("USD");
            emit self->balance_ready(b);
        },
        QStringLiteral("fetch_balance"));
}

void KalshiAdapter::fetch_positions() {
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("positions"), {},
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            QVector<pr::PredictionPosition> out;
            const auto arr = resp.value("positions").toArray();
            out.reserve(arr.size());
            for (const auto& v : arr) {
                const auto o = v.toObject();
                pr::PredictionPosition p;
                p.asset_id = o.value("asset_id").toString();
                p.market_id = o.value("market_id").toString();
                p.outcome = o.value("outcome").toString();
                p.size = o.value("size").toDouble();
                p.avg_price = o.value("avg_price").toDouble();
                p.realized_pnl = o.value("realized_pnl").toDouble();
                p.unrealized_pnl = o.value("unrealized_pnl").toDouble();
                p.current_value = o.value("current_value").toDouble();
                out.push_back(p);
            }
            emit self->positions_ready(out);
        },
        QStringLiteral("fetch_positions"));
}

void KalshiAdapter::fetch_open_orders() {
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("open_orders"), {},
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            QVector<pr::OpenOrder> out;
            const auto arr = resp.value("orders").toArray();
            out.reserve(arr.size());
            for (const auto& v : arr) {
                const auto o = v.toObject();
                pr::OpenOrder ord;
                ord.order_id = o.value("order_id").toString();
                ord.asset_id = o.value("asset_id").toString();
                ord.market_id = o.value("market_id").toString();
                ord.outcome = o.value("outcome").toString();
                ord.side = o.value("side").toString();
                ord.order_type = o.value("order_type").toString();
                ord.price = o.value("price").toDouble();
                ord.size = o.value("size").toDouble();
                ord.filled = o.value("filled").toDouble();
                ord.status = o.value("status").toString();
                ord.expires_ms = qint64(o.value("expires_ms").toDouble());
                out.push_back(ord);
            }
            emit self->open_orders_ready(out);
        },
        QStringLiteral("fetch_open_orders"));
}

void KalshiAdapter::fetch_user_activity(int limit) {
    QPointer<KalshiAdapter> self = this;
    QJsonObject extra;
    extra.insert("limit", limit);
    run_py(
        QStringLiteral("fills"), extra,
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            emit self->account_activity_ready(resp.value("fills").toArray().toVariantList());
        },
        QStringLiteral("fetch_user_activity"));
}

void KalshiAdapter::place_order(const pr::OrderRequest& req) {
    const auto [ticker, side] = split_asset_id(req.asset_id);
    if (ticker.isEmpty()) {
        emit error_occurred(QStringLiteral("place_order"), QStringLiteral("Kalshi asset_id must be '<ticker>:yes|no'"));
        return;
    }

    // Kalshi trades whole contracts; int(req.size) below would silently truncate 5.9 -> 5.
    if (req.size < 1.0 || std::abs(req.size - std::round(req.size)) > 1e-9) {
        emit error_occurred(QStringLiteral("place_order"),
                            QStringLiteral("Kalshi orders are whole contracts (size must be an integer >= 1)"));
        return;
    }

    // The order ticket offers the time-in-force codes GTC / FOK / FAK (Polymarket's
    // vocabulary), but Kalshi's order `type` is only "limit" | "market". The old code
    // forwarded req.order_type.toLower() ("gtc") verbatim, which the bridge put straight
    // into the request body as type="gtc" — not a valid Kalshi type, and (not being
    // "limit") sent with no price. Map the codes. FOK / FAK have no field in the bridge's
    // request, so refuse them instead of quietly turning them into a resting limit order.
    const QString type_code = req.order_type.trimmed().toUpper();
    QString kalshi_type;
    if (type_code.isEmpty() || type_code == QStringLiteral("GTC") || type_code == QStringLiteral("LIMIT")) {
        kalshi_type = QStringLiteral("limit");
    } else if (type_code == QStringLiteral("MARKET")) {
        kalshi_type = QStringLiteral("market");
    } else {
        emit error_occurred(QStringLiteral("place_order"),
                            QStringLiteral("Kalshi: order type %1 is not supported — use GTC").arg(type_code));
        return;
    }

    QJsonObject extra;
    extra.insert("ticker", ticker);
    extra.insert("side", side);
    extra.insert("action",
                 req.side.toLower() == QStringLiteral("sell") ? QStringLiteral("sell") : QStringLiteral("buy"));
    extra.insert("count", int(req.size));
    extra.insert("order_type", kalshi_type);
    // Kalshi limit price is integer cents 1-99. Refuse anything else rather than
    // clamp / round it: the user confirmed req.price, and 0.555 -> 56c, 0.004 -> 1c
    // or 0.996 -> 99c would send a different order than the one confirmed.
    const double cents_exact = req.price * 100.0;
    const double cents_rounded = std::round(cents_exact);
    if (std::abs(cents_exact - cents_rounded) > 1e-6 || cents_rounded < 1.0 || cents_rounded > 99.0) {
        emit error_occurred(QStringLiteral("place_order"),
                            QStringLiteral("Kalshi prices are whole cents from 0.01 to 0.99 — %1 is not; order not sent")
                                .arg(QString::number(req.price, 'f', 4)));
        return;
    }
    const int price_cents = int(cents_rounded);
    if (side != QStringLiteral("yes") && side != QStringLiteral("no")) {
        emit error_occurred(QStringLiteral("place_order"),
                            QStringLiteral("Kalshi asset_id must end in ':yes' or ':no' — order not sent"));
        return;
    }
    if (side == QStringLiteral("yes"))
        extra.insert("yes_price_cents", price_cents);
    else
        extra.insert("no_price_cents", price_cents);
    if (req.expires_ms > 0)
        extra.insert("expiration_ts", qint64(req.expires_ms / 1000));
    extra.insert("client_order_id", req.client_order_id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                                                  : req.client_order_id);
    if (!req.source.isEmpty())
        extra.insert(QLatin1String(kKxSourceKey), req.source); // audit only; run_py does not send it

    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("place_order"), extra,
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            pr::OrderResult out;
            out.ok = resp.value("ok").toBool();
            out.order_id = resp.value("order_id").toString();
            out.status = resp.value("status").toString();
            emit self->order_placed(out);
        },
        QStringLiteral("place_order"));
}

void KalshiAdapter::cancel_order(const QString& order_id) {
    const QString oid = order_id;
    QJsonObject extra;
    extra.insert("order_id", oid);
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("cancel_order"), extra,
        [self, oid](const QJsonObject& /*resp*/) {
            if (!self)
                return;
            emit self->order_cancelled(oid, true, QString());
        },
        QStringLiteral("cancel_order"));
}

void KalshiAdapter::cancel_all_for_market(const pr::MarketKey& key, const QString& /*asset_id*/) {
    // Fetch the ticker-filtered open orders and cancel them in one batched
    // DELETE /portfolio/orders/batched round-trip (instead of N individual
    // cancels). Empty result set is a silent success.
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("cancel_all_for_market"),
                            QStringLiteral("Kalshi cancel-all requires a ticker"));
        return;
    }
    const QString ticker = key.market_id;
    QPointer<KalshiAdapter> self = this;
    QJsonObject extra;
    extra.insert("ticker", ticker);
    run_py(
        QStringLiteral("open_orders"), extra,
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            QStringList ids;
            const auto arr = resp.value("orders").toArray();
            ids.reserve(arr.size());
            for (const auto& v : arr) {
                const QString oid = v.toObject().value("order_id").toString();
                if (!oid.isEmpty())
                    ids.push_back(oid);
            }
            if (!ids.isEmpty())
                self->cancel_orders_batch(ids);
        },
        QStringLiteral("cancel_all_for_market"));
}

// ── Kalshi-specific public reads ────────────────────────────────────────────

void KalshiAdapter::fetch_exchange_status() {
    rest_->fetch_exchange_status();
}
void KalshiAdapter::fetch_exchange_schedule() {
    rest_->fetch_exchange_schedule();
}

void KalshiAdapter::fetch_batch_candles(const QStringList& tickers, int period_interval_min, qint64 start_ts,
                                        qint64 end_ts) {
    rest_->fetch_batch_candlesticks(tickers, period_interval_min, start_ts, end_ts);
}

void KalshiAdapter::fetch_series_detail(const QString& series_ticker) {
    if (series_ticker.isEmpty())
        return;
    // If cached, re-emit synchronously so consumers don't have to track
    // "did I already fetch this?" — they can just call the method.
    const auto it = series_cache_.constFind(series_ticker);
    if (it != series_cache_.constEnd()) {
        QMetaObject::invokeMethod(
            this,
            [this, series_ticker]() { emit series_detail_ready(series_ticker, series_cache_.value(series_ticker)); },
            Qt::QueuedConnection);
        return;
    }
    rest_->fetch_series_detail(series_ticker);
}

QJsonObject KalshiAdapter::cached_series(const QString& series_ticker) const {
    return series_cache_.value(series_ticker);
}

void KalshiAdapter::fetch_historical_markets(const QString& series_ticker, int limit, const QString& cursor) {
    rest_->fetch_historical_markets(series_ticker, limit, cursor);
}

void KalshiAdapter::fetch_historical_candles(const QString& ticker, int period_interval_min, qint64 start_ts,
                                             qint64 end_ts) {
    rest_->fetch_historical_candlesticks(ticker, period_interval_min, start_ts, end_ts);
}

void KalshiAdapter::fetch_historical_trades(const QString& ticker, int limit, const QString& cursor) {
    rest_->fetch_historical_trades(ticker, limit, cursor);
}

// ── Kalshi-specific trading ─────────────────────────────────────────────────

void KalshiAdapter::amend_order(const QString& order_id, const QString& side, int price_cents,
                                const QString& client_order_id) {
    // `side` is the OUTCOME the price applies to. Anything but yes/no used to fall
    // through to a NO price — and the blotter passed the action (buy/sell), so every
    // amend went out as a NO price. Refuse instead of guessing; likewise refuse an
    // out-of-range price rather than clamping it to one the user did not enter.
    const QString outcome = side.trimmed().toLower();
    if (outcome != QStringLiteral("yes") && outcome != QStringLiteral("no")) {
        emit error_occurred(QStringLiteral("amend_order"),
                            QStringLiteral("Kalshi amend needs the order's outcome (yes/no), got \"%1\" — not sent")
                                .arg(side));
        return;
    }
    if (price_cents < 1 || price_cents > 99) {
        emit error_occurred(QStringLiteral("amend_order"),
                            QStringLiteral("Kalshi prices are 1-99 cents, got %1 — not sent").arg(price_cents));
        return;
    }
    QJsonObject extra;
    extra.insert("order_id", order_id);
    if (outcome == QStringLiteral("yes"))
        extra.insert("yes_price_cents", price_cents);
    else
        extra.insert("no_price_cents", price_cents);
    if (!client_order_id.isEmpty())
        extra.insert("client_order_id", client_order_id);
    const QString oid = order_id;
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("amend_order"), extra,
        [self, oid](const QJsonObject& /*resp*/) {
            if (!self)
                return;
            emit self->order_amended(oid, true, QString());
        },
        QStringLiteral("amend_order"));
}

void KalshiAdapter::fetch_order(const QString& order_id) {
    QJsonObject extra;
    extra.insert("order_id", order_id);
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("get_order"), extra,
        [self](const QJsonObject& resp) {
            if (!self)
                return;
            emit self->single_order_ready(resp.value("order").toObject());
        },
        QStringLiteral("fetch_order"));
}

void KalshiAdapter::cancel_orders_batch(const QStringList& order_ids) {
    if (order_ids.isEmpty())
        return;
    QJsonObject extra;
    QJsonArray ids;
    for (const auto& id : order_ids)
        ids.append(id);
    extra.insert("order_ids", ids);
    const QStringList ids_copy = order_ids;
    QPointer<KalshiAdapter> self = this;
    run_py(
        QStringLiteral("cancel_orders_batch"), extra,
        [self, ids_copy](const QJsonObject& /*resp*/) {
            if (!self)
                return;
            emit self->orders_batch_cancelled(ids_copy, true, QString());
        },
        QStringLiteral("cancel_orders_batch"));
}

// ── Kalshi-specific account history ─────────────────────────────────────────

QString KalshiAdapter::history_ctx(const QString& kind) {
    if (kind == history_settlements())
        return QStringLiteral("fetch_settlements");
    if (kind == history_fills())
        return QStringLiteral("fetch_historical_fills");
    return QStringLiteral("fetch_historical_orders");
}

void KalshiAdapter::fetch_settlements(const QString& cursor, int limit) {
    run_history(history_settlements(), QStringLiteral("settlements"), cursor, limit);
}

void KalshiAdapter::fetch_historical_fills(const QString& cursor, int limit) {
    run_history(history_fills(), QStringLiteral("historical_fills"), cursor, limit);
}

void KalshiAdapter::fetch_historical_orders(const QString& cursor, int limit) {
    run_history(history_orders(), QStringLiteral("historical_orders"), cursor, limit);
}

void KalshiAdapter::run_history(const QString& kind, const QString& command, const QString& cursor, int limit) {
    QJsonObject extra;
    extra.insert("limit", qBound(1, limit, 1000)); // Kalshi's page-size range
    if (!cursor.isEmpty())
        extra.insert("cursor", cursor);
    QPointer<KalshiAdapter> self = this;
    run_py(
        command, extra,
        [self, kind, cursor](const QJsonObject& resp) {
            if (!self)
                return;
            // The reply's array key is the kind itself: settlements / fills / orders.
            const auto arr = resp.value(kind).toArray();
            QVariantList rows;
            rows.reserve(arr.size());
            for (const auto& v : arr) {
                const auto o = v.toObject();
                if (kind == history_settlements())
                    rows.push_back(settlement_row(o));
                else if (kind == history_fills())
                    rows.push_back(fill_row(o));
                else
                    rows.push_back(order_row(o));
            }
            emit self->account_history_ready(kind, rows, cursor, resp.value("cursor").toString());
        },
        history_ctx(kind));
}

// ── Credentials plumbing ────────────────────────────────────────────────────

void KalshiAdapter::set_credentials(const KalshiCredentials& creds) {
    creds_ = creds;
    ws_->set_credentials(creds);
    rest_->set_demo_mode(creds.use_demo);
    emit credentials_changed();
}

// ── Hub registration ────────────────────────────────────────────────────────

void KalshiAdapter::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    ws_->ensure_registered_with_hub();
    // Keyless REST snapshots (`prediction:kalshi:market:*`) — a standalone singleton so consumers
    // can also register it on demand; idempotent.
    KalshiMarketProducer::instance().ensure_registered_with_hub();
    hub_registered_ = true;
}

// ── Signal wiring ───────────────────────────────────────────────────────────

void KalshiAdapter::remember_market(const pr::PredictionMarket& m) {
    if (m.key.market_id.isEmpty())
        return;
    const QString s = m.extras.value(QStringLiteral("series_ticker")).toString();
    if (!s.isEmpty())
        series_by_market_.insert(m.key.market_id, s);
    const QString opened = m.extras.value(QStringLiteral("open_time")).toString();
    if (!opened.isEmpty()) {
        const QDateTime dt = QDateTime::fromString(opened, Qt::ISODate);
        if (dt.isValid())
            open_ts_by_market_.insert(m.key.market_id, dt.toSecsSinceEpoch());
    }
}

void KalshiAdapter::wire() {
    // REST → adapter signal forwarding. Also harvests market→series
    // mappings so fetch_price_history can hit the correct endpoint.
    connect(rest_.get(), &KalshiRestClient::markets_ready, this,
            [this](const QVector<pr::PredictionMarket>& markets, const QString& /*cursor*/) {
                for (const auto& m : markets)
                    remember_market(m);
                // Searches no longer come through here (see search()).
                emit markets_ready(markets);
            });
    connect(rest_.get(), &KalshiRestClient::events_ready, this,
            [this](const QVector<pr::PredictionEvent>& events, const QString& /*cursor*/) {
                for (const auto& e : events) {
                    for (const auto& m : e.markets)
                        remember_market(m);
                }
                emit events_ready(events);
            });
    connect(rest_.get(), &KalshiRestClient::market_detail_ready, this, [this](const pr::PredictionMarket& m) {
        remember_market(m);
        emit market_detail_ready(m);
    });
    connect(rest_.get(), &KalshiRestClient::event_detail_ready, this, [this](const pr::PredictionEvent& e) {
        for (const auto& m : e.markets)
            remember_market(m);
        emit event_detail_ready(e);
    });
    connect(rest_.get(), &KalshiRestClient::historical_markets_ready, this,
            [this](const QVector<pr::PredictionMarket>& markets, const QString& /*cursor*/) {
                for (const auto& m : markets)
                    remember_market(m);
            });
    connect(rest_.get(), &KalshiRestClient::tags_ready, this, &KalshiAdapter::tags_ready);
    connect(rest_.get(), &KalshiRestClient::trades_ready, this, &KalshiAdapter::recent_trades_ready);
    connect(rest_.get(), &KalshiRestClient::request_error, this, &KalshiAdapter::error_occurred);

    // Kalshi REST returns two books (yes + no) in a single response.
    // We emit both books as order_book_ready events. Consumers should
    // filter on asset_id if they only want one side.
    connect(rest_.get(), &KalshiRestClient::order_book_ready, this,
            [this](const pr::PredictionOrderBook& yes_book, const pr::PredictionOrderBook& no_book,
                   const QString& /*ticker*/) {
                emit order_book_ready(yes_book);
                emit order_book_ready(no_book);
            });

    connect(rest_.get(), &KalshiRestClient::price_history_ready, this,
            [this](const pr::PriceHistory& hist, const QString& ticker) {
                // Kalshi candles are YES prices only. Retag with the requested
                // outcome — but only when that request was for THIS ticker, so a
                // late reply for a previously-selected market keeps its own
                // ":yes" tag (and is dropped by the screen) instead of being
                // relabelled. For the NO outcome the series is 1 − YES: it used
                // to be the YES curve under a "No" label (10c YES drawn as 10c NO).
                pr::PriceHistory out = hist;
                if (!ticker.isEmpty() && last_history_asset_id_.startsWith(ticker + QLatin1Char(':'))) {
                    out.asset_id = last_history_asset_id_;
                    if (last_history_asset_id_.endsWith(QStringLiteral(":no"))) {
                        for (auto& p : out.points)
                            p.price = qBound(0.0, 1.0 - p.price, 1.0);
                    }
                }
                emit price_history_ready(out);
            });

    // Kalshi-specific REST signal forwards.
    connect(rest_.get(), &KalshiRestClient::exchange_status_ready, this, &KalshiAdapter::exchange_status_ready);
    connect(rest_.get(), &KalshiRestClient::exchange_schedule_ready, this, &KalshiAdapter::exchange_schedule_ready);
    connect(rest_.get(), &KalshiRestClient::batch_candlesticks_ready, this, &KalshiAdapter::batch_candles_ready);
    connect(rest_.get(), &KalshiRestClient::historical_markets_ready, this, &KalshiAdapter::historical_markets_ready);
    connect(rest_.get(), &KalshiRestClient::historical_trades_ready, this, &KalshiAdapter::historical_trades_ready);
    connect(rest_.get(), &KalshiRestClient::historical_candlesticks_ready, this,
            &KalshiAdapter::historical_candles_ready);

    // Series detail — cache before emitting so sync consumers can read.
    connect(rest_.get(), &KalshiRestClient::series_detail_ready, this, [this](const QJsonObject& series) {
        const QString t = series.value(QStringLiteral("ticker")).toString();
        if (!t.isEmpty())
            series_cache_.insert(t, series);
        emit series_detail_ready(t, series);
    });

    // WebSocket → adapter.
    connect(ws_.get(), &KalshiWsClient::price_updated, this, &KalshiAdapter::ws_price_updated);
    connect(ws_.get(), &KalshiWsClient::orderbook_updated, this, &KalshiAdapter::ws_orderbook_updated);
    connect(ws_.get(), &KalshiWsClient::trade_received, this, &KalshiAdapter::ws_trade_received);
    connect(ws_.get(), &KalshiWsClient::market_lifecycle_changed, this, &KalshiAdapter::ws_market_lifecycle_changed);
    connect(ws_.get(), &KalshiWsClient::connection_status_changed, this, &KalshiAdapter::ws_connection_changed);
}

} // namespace fincept::services::prediction::kalshi_ns
