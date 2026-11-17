#include "trading/PaperMarkService.h"

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/FxRateService.h"
#include "services/markets/MarketDataService.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountDataStream.h"
#include "trading/AccountManager.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/websocket/FyersTickTypes.h"
#include "ui/formatting/NumberFormat.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QThread>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::trading {

namespace {
const QString TAG = QStringLiteral("PaperMark");
const QString kConsumer = QStringLiteral("paper-mark");
// Re-evaluate tracked accounts/positions on this cadence (cheap: a few accounts,
// a handful of positions each). Fills also trigger an immediate resync.
constexpr int kResyncMs = 4000;
// Persist marked prices to SQLite at most once per this window per burst of
// ticks — one UPDATE per symbol per window instead of one per tick.
constexpr int kFlushMs = 1000;

// DAY-order semantics for Indian-equity (INR) portfolios: an order from an earlier
// IST session has expired — pt_settle_intraday() cancels those — so it must not be
// resurrected into the matcher at launch. Other currencies (crypto / US) are
// good-till-cancelled and always resume.
bool pm_is_expired_day_order(const PtPortfolio& pf, const PtOrder& o) {
    if (pf.currency.compare(QLatin1String("INR"), Qt::CaseInsensitive) != 0)
        return false;
    QDateTime created = QDateTime::fromString(o.created_at, Qt::ISODate);
    if (!created.isValid())
        return false;
    created.setTimeZone(QTimeZone::UTC); // timestamps are stored as UTC
    const QDate created_ist = created.addSecs(330 * 60).date();
    const QDate today_ist = QDateTime::currentDateTimeUtc().addSecs(330 * 60).date();
    return created_ist < today_ist;
}
} // namespace

PaperMarkService& PaperMarkService::instance() {
    static PaperMarkService s;
    return s;
}

PaperMarkService::PaperMarkService() = default;

void PaperMarkService::start() {
    if (started_)
        return;
    started_ = true;

    // A fill changes positions/orders. The callback can fire on a worker thread
    // (e.g. square-off runs in QtConcurrent), so marshal to this object's thread
    // before touching the bindings / emitting.
    fill_cb_id_ = OrderMatcher::instance().on_order_fill([this](const OrderFillEvent&) {
        QMetaObject::invokeMethod(
            this,
            [this]() {
                resync();
                for (auto it = bound_.cbegin(); it != bound_.cend(); ++it)
                    emit portfolio_changed(it->portfolio_id);
                for (auto it = public_.cbegin(); it != public_.cend(); ++it)
                    emit portfolio_changed(it.key());
            },
            Qt::QueuedConnection);
    });

    // The built-in paper account trades on delayed public prices — track it from
    // launch, whether or not the Equity Trading screen is ever opened, so its
    // positions mark and its resting orders fill. A reset moves it to a new portfolio.
    auto track_builtin = [this]() {
        const auto acct = AccountManager::instance().builtin_paper_account();
        if (acct.paper_portfolio_id.isEmpty())
            return;
        try {
            const auto pf = pt_get_portfolio(acct.paper_portfolio_id);
            track_public_portfolio(pf.id, pf.currency);
        } catch (const std::exception& e) {
            LOG_WARN(TAG, QString("Built-in paper portfolio unavailable: %1").arg(e.what()));
        }
    };
    connect(&AccountManager::instance(), &AccountManager::builtin_paper_reset, this,
            [this, track_builtin](const QString&) {
                // Drop the old portfolio's binding; the new one is tracked below.
                for (auto it = public_.begin(); it != public_.end();) {
                    bool builtin = false;
                    try {
                        builtin = pt_portfolio_settles_by_venue(pt_get_portfolio(it.key()));
                    } catch (...) {
                        builtin = true; // deleted by the reset
                        LOG_DEBUG(TAG, logf("Paper portfolio could not be read, dropping its mark subscriptions",
                                            {{"portfolio", log_mask(it.key())}}));
                    }
                    it = builtin ? public_.erase(it) : std::next(it);
                }
                track_builtin();
            });

    // Resume matching of resting limit / stop orders that were persisted before this
    // launch. Nothing ever re-registered them with the matcher after a restart
    // (load_orders() had no caller; only the Equity screen re-added its own focused
    // account's), so they sat "pending" in the DB — holding margin — and never
    // filled. Idempotent: OrderMatcher::add_order() dedupes by order id.
    for (const auto& pf : pt_list_portfolios()) {
        QVector<PtOrder> resting;
        for (const auto& o : pt_get_orders(pf.id, QStringLiteral("pending"))) {
            if (!pm_is_expired_day_order(pf, o))
                resting.push_back(o);
        }
        OrderMatcher::instance().load_orders(resting);
    }

    resync_timer_ = new QTimer(this);
    resync_timer_->setInterval(kResyncMs);
    connect(resync_timer_, &QTimer::timeout, this, &PaperMarkService::resync);
    resync_timer_->start();

    resync();
    track_builtin();
    LOG_DEBUG(TAG, "Paper mark-to-market service started");
}

void PaperMarkService::resync() {
    auto& dsm = DataStreamManager::instance();
    QSet<QString> should_track; // account_ids that currently warrant a binding

    for (const auto& acct : AccountManager::instance().active_accounts()) {
        if (acct.trading_mode != QLatin1String("paper") || acct.paper_portfolio_id.isEmpty())
            continue;
        // Marked from delayed public prices while its broker feed is down: the broker
        // binding is suspended so two sources never mark the same portfolio.
        if (public_.contains(acct.paper_portfolio_id))
            continue;

        const auto positions = pt_get_positions(acct.paper_portfolio_id);
        QStringList syms;
        QVector<PosLeg> legs;
        for (const auto& p : positions) {
            if (p.quantity == 0.0 || p.symbol.isEmpty())
                continue;
            syms << p.symbol;
            const auto leg = fyers_parse_option(p.symbol);
            if (leg.valid)
                legs.push_back({leg.underlying, leg.is_call, p.symbol});
        }
        // Resting limit / stop orders need ticks too: without their symbols on the
        // stream they could only ever fill while a screen happened to feed the
        // matcher. (Market orders fill immediately and never rest.)
        for (const auto& o : pt_get_orders(acct.paper_portfolio_id, QStringLiteral("pending"))) {
            if (o.order_type == QLatin1String("market") || o.symbol.isEmpty())
                continue;
            syms << o.symbol;
            const auto leg = fyers_parse_option(o.symbol);
            if (leg.valid)
                legs.push_back({leg.underlying, leg.is_call, o.symbol});
        }
        if (syms.isEmpty())
            continue; // nothing to mark — don't force a stream up for an idle portfolio

        should_track.insert(acct.account_id);
        auto& b = bound_[acct.account_id];
        b.portfolio_id = acct.paper_portfolio_id;
        b.exact = QSet<QString>(syms.cbegin(), syms.cend());
        b.legs = legs;

        auto* stream = dsm.stream_for(acct.account_id);
        if (!stream)
            continue;

        // (Re)bind quote_updated. Reconnect when the stream object changed — a
        // token refresh runs restart_stream(), which destroys + recreates it; the
        // stored connection would be dead and marking would silently stall.
        if (b.stream != stream || !b.conn) {
            if (b.conn)
                QObject::disconnect(b.conn);
            const QString pid = acct.paper_portfolio_id;
            b.conn = connect(stream, &AccountDataStream::quote_updated, this,
                             [this, pid](const QString& aid, const QString& sym, const BrokerQuote& q) {
                                 on_quote(aid, pid, sym, q);
                             });
            b.stream = stream;
            b.symbols.clear(); // force a fresh subscribe on the new stream
        }

        const QSet<QString> want(syms.cbegin(), syms.cend());
        if (b.symbols != want) {
            stream->subscribe_symbols(kConsumer, syms);
            b.symbols = want;
        }
        dsm.start_stream(acct.account_id);
    }

    // Drop bindings for accounts that no longer hold positions / are no longer
    // active paper, releasing their stream subscription.
    for (auto it = bound_.begin(); it != bound_.end();) {
        if (!should_track.contains(it.key())) {
            if (it->conn)
                QObject::disconnect(it->conn);
            if (auto* s = dsm.stream_for(it.key()))
                s->unsubscribe_consumer(kConsumer);
            it = bound_.erase(it);
        } else {
            ++it;
        }
    }

    resync_public();
}

// ============================================================================
// Delayed public prices
// ============================================================================

void PaperMarkService::track_public_portfolio(const QString& portfolio_id, const QString& account_currency,
                                              std::function<QString(const QString&)> to_yahoo) {
    if (portfolio_id.isEmpty())
        return;
    const bool fresh = !public_.contains(portfolio_id);
    PublicBinding& b = public_[portfolio_id];
    b.account_currency = account_currency.trimmed();
    if (b.account_currency.isEmpty()) { // the book's own currency, never a guessed one
        try {
            b.account_currency = pt_get_portfolio(portfolio_id).currency.trimmed();
        } catch (...) {
        }
        if (b.account_currency.isEmpty())
            LOG_WARN(TAG, QString("Delayed-price tracking for %1: the book's currency is unknown — fills of "
                                  "foreign-currency symbols wait for one")
                              .arg(portfolio_id));
    }
    b.to_yahoo = std::move(to_yahoo);
    if (fresh) {
        // Resume resting orders. A venue-settled portfolio (the built-in account) also
        // resumes its queued MARKET orders — they wait for the open by design;
        // OrderMatcher::load_orders() skips market orders for every other portfolio.
        bool venue_settled = false;
        try {
            venue_settled = pt_portfolio_settles_by_venue(pt_get_portfolio(portfolio_id));
        } catch (...) {
            LOG_WARN(TAG, logf("Paper portfolio could not be read while resuming its orders; queued market orders "
                               "wait",
                               {{"portfolio", log_mask(portfolio_id)}}));
        }
        for (const auto& o : pt_get_orders(portfolio_id, QStringLiteral("pending"))) {
            if (o.order_type != QLatin1String("market") || venue_settled)
                OrderMatcher::instance().add_order(o);
        }
        LOG_INFO(TAG, QString("Delayed-price tracking on for portfolio %1 (%2)").arg(portfolio_id, b.account_currency));
    }
    if (started_)
        resync(); // suspends a broker binding of the same portfolio, subscribes its symbols
    else
        resync_public();
}

void PaperMarkService::untrack_public_portfolio(const QString& portfolio_id) {
    if (public_.remove(portfolio_id) == 0)
        return;
    LOG_INFO(TAG, QString("Delayed-price tracking off for portfolio %1").arg(portfolio_id));
    if (started_)
        resync(); // re-binds the broker stream, drops unused quote subscriptions
    else
        resync_public();
}

bool PaperMarkService::is_public_tracked(const QString& portfolio_id) const {
    return public_.contains(portfolio_id);
}

PtDelayedQuote PaperMarkService::delayed_quote(const QString& yahoo) const {
    return public_quotes_.value(yahoo);
}

QString PaperMarkService::yahoo_for(const PublicBinding& b, const QString& stored) const {
    if (!b.to_yahoo)
        return stored; // canonical symbols (built-in account)
    return b.to_yahoo(stored);
}

PtDelayedQuote PaperMarkService::make_delayed_quote(const QString& yahoo, const services::QuoteData& q,
                                                    qint64 received_ms) {
    PtDelayedQuote d;
    d.price = q.price;
    d.received_ms = received_ms > 0 ? received_ms : QDateTime::currentMSecsSinceEpoch();
    d.printed_ms = q.market_time > 0 ? q.market_time * 1000 : 0;
    namespace mk = fincept::core::market;
    if (const mk::Venue* venue = mk::venue_for_symbol(yahoo)) {
        d.session_open =
            mk::session_status(*venue, QDateTime::currentDateTimeUtc()).state == mk::SessionState::Open;
    } else {
        d.session_open = q.market_state == QLatin1String("REGULAR");
    }
    return d;
}

std::optional<double> PaperMarkService::fx_factor(const QString& quote_ccy, const QString& account_ccy) {
    namespace mk = fincept::core::market;
    const QString from = quote_ccy.trimmed();
    const QString to = account_ccy.trimmed().toUpper();
    if (from.isEmpty() || to.isEmpty())
        return std::nullopt;
    if (from == to)
        return 1.0;
    if (mk::major_currency(from) == to)
        return 1.0 / mk::minor_unit_divisor(from); // pence in a GBP account
    auto& fx = services::FxRateService::instance();
    // A stale rate is still a market rate (weekend close) and is used. A stablecoin
    // "rate" the service makes up 1:1 while the real leg is missing is not: cash
    // booked at it would be wrong without saying so — wait for the real leg.
    if (const auto q = fx.quote(from, to); q && !q->approximate && std::isfinite(q->rate) && q->rate > 0.0)
        return q->rate;
    fx.ensure({{from, to}}); // rates_changed() follows; the next quote picks it up
    return std::nullopt;
}

bool PaperMarkService::prepare_symbol_fx(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                         QString* why) {
    namespace mk = fincept::core::market;
    if (portfolio_id.isEmpty() || symbol.isEmpty())
        return true;
    const QString exch = exchange.trimmed().toUpper();
    QString px_ccy;
    if (exch == QLatin1String("CRYPTO"))
        px_ccy = fincept::ui::formatting::instrument_for_symbol(symbol.trimmed()).currency; // "BTC-USDT" -> USDT
    else if (const mk::Venue* v = mk::venue_by_code(exch))
        px_ccy = v->currency;
    if (px_ccy.isEmpty())
        return true; // segment not in the venue table (MCX, CDS, FOREX): the account's own currency
    QString acct_ccy;
    try {
        acct_ccy = pt_get_portfolio(portfolio_id).currency.trimmed();
    } catch (...) {
        return true; // no portfolio: pt_place_order reports that itself
    }
    if (acct_ccy.isEmpty() || px_ccy == acct_ccy)
        return true;
    if (const auto f = fx_factor(px_ccy, acct_ccy)) {
        pt_set_symbol_fx(portfolio_id, symbol, *f);
        return true;
    }
    // An order path asked (it wants the reason): the order is refused — audited here once
    // for every caller (order ticket, built-in account, crypto, workflows, UnifiedTrading).
    if (why)
        LOG_AUDIT("Orders", "Paper order refused, exchange rate missing",
                  {{"venue", "paper"},
                   {"mode", "paper"},
                   {"portfolio", log_mask(portfolio_id)},
                   {"symbol", symbol},
                   {"exchange", exch},
                   {"from", px_ccy},
                   {"to", acct_ccy}});
    if (why)
        *why = QCoreApplication::translate("PaperMarkService",
                                           "The %1 to %2 exchange rate has not arrived yet (it is being fetched). %3 is "
                                           "priced in %1 and this paper account books cash in %2, so the order was not "
                                           "placed. Try again in a few seconds.")
                   .arg(px_ccy, acct_ccy, symbol);
    return false;
}

bool PaperMarkService::prepare_close_fx(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                        int wait_ms, QString* why) {
    namespace mk = fincept::core::market;
    if (portfolio_id.isEmpty() || symbol.isEmpty())
        return true;
    PtPortfolio pf;
    try {
        pf = pt_get_portfolio(portfolio_id);
    } catch (...) {
        return true; // no portfolio: pt_place_order reports that itself
    }
    const QString acct_ccy = pf.currency.trimmed();
    if (acct_ccy.isEmpty())
        return true;

    // 1. The instrument's currency, as prepare_symbol_fx() reads it at the open.
    auto venue_ccy = [&symbol](const QString& exch_in) -> QString {
        const QString exch = exch_in.trimmed().toUpper();
        if (exch.isEmpty())
            return {};
        if (exch == QLatin1String("CRYPTO"))
            return fincept::ui::formatting::instrument_for_symbol(symbol.trimmed()).currency;
        if (const mk::Venue* v = mk::venue_by_code(exch))
            return v->currency;
        return {};
    };
    QString px_ccy = venue_ccy(exchange);
    if (px_ccy.isEmpty() && exchange.trimmed().isEmpty()) {
        // Positions carry no venue; the orders that opened them do (newest first).
        for (const auto& o : pt_get_orders(portfolio_id)) {
            if (o.symbol == symbol && !o.exchange.trimmed().isEmpty()) {
                px_ccy = venue_ccy(o.exchange);
                break;
            }
        }
        if (px_ccy.isEmpty() && pt_portfolio_settles_by_venue(pf))
            px_ccy = mk::currency_for_symbol(symbol); // canonical Yahoo symbol ("VOD.L" -> GBp)
    }
    // 2. Unknown (booked as the account's currency at the open too) or the same currency.
    if (px_ccy.isEmpty() || px_ccy == acct_ccy)
        return true;

    // 3. / 4. A real rate, or the factor this run already took from one.
    if (const auto f = fx_factor(px_ccy, acct_ccy)) {
        pt_set_symbol_fx(portfolio_id, symbol, *f);
        return true;
    }
    if (pt_has_symbol_fx(portfolio_id, symbol))
        return true;

    // 5. Wait briefly for the leg fx_factor() just requested — never on the GUI thread.
    auto* app = QCoreApplication::instance();
    const bool gui_thread = app && QThread::currentThread() == app->thread();
    if (!gui_thread && wait_ms > 0) {
        QDeadlineTimer deadline(wait_ms);
        while (!deadline.hasExpired()) {
            QThread::msleep(static_cast<unsigned long>(std::min<qint64>(250, std::max<qint64>(1, deadline.remainingTime()))));
            if (const auto f = fx_factor(px_ccy, acct_ccy)) {
                pt_set_symbol_fx(portfolio_id, symbol, *f);
                return true;
            }
        }
    }
    if (why) // an exit was asked for and refused (fill_fx_ready() passes no reason: it only defers a fill)
        LOG_AUDIT("Orders", "Paper position close refused, exchange rate missing",
                  {{"venue", "paper"},
                   {"mode", "paper"},
                   {"portfolio", log_mask(portfolio_id)},
                   {"symbol", symbol},
                   {"from", px_ccy},
                   {"to", acct_ccy},
                   {"waited_ms", wait_ms}});
    if (why)
        *why = QCoreApplication::translate("PaperMarkService",
                                           "%3 was not closed: it is priced in %1 and this paper account books cash in "
                                           "%2, and the %1 to %2 exchange rate has not arrived yet (it is being "
                                           "fetched). Nothing was booked — try the exit again in a few seconds.")
                   .arg(px_ccy, acct_ccy, symbol);
    return false;
}

namespace {
// fill_fx_ready() decisions per "portfolio\x1fsymbol" (any thread, so under a mutex).
struct PmFillFxState {
    QMutex mutex;
    QSet<QString> no_factor_needed;   // same currency as the book (or unknown, as at the open)
    QHash<QString, qint64> next_check; // epoch ms: rate missing -> retry; factor set -> refresh
};
PmFillFxState& pm_fill_fx_state() {
    static PmFillFxState s;
    return s;
}
constexpr qint64 kPmFillFxRetryMs = 2000;    // a missing rate is looked for again after this
constexpr qint64 kPmFillFxRefreshMs = 60000; // a factor in use is refreshed from the rate this often
} // namespace

bool PaperMarkService::fill_fx_ready(const QString& portfolio_id, const QString& symbol, const QString& exchange) {
    if (portfolio_id.isEmpty() || symbol.isEmpty())
        return true;
    const QString key = portfolio_id + QLatin1Char('\x1f') + symbol;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool has_factor = pt_has_symbol_fx(portfolio_id, symbol);
    auto& st = pm_fill_fx_state();
    {
        QMutexLocker lock(&st.mutex);
        if (st.no_factor_needed.contains(key))
            return true;
        if (now < st.next_check.value(key, 0))
            return has_factor; // decided recently: a factor in use, or still no rate
    }
    // Outside the lock: DB reads and the FX service.
    const bool ok = prepare_close_fx(portfolio_id, symbol, exchange, /*wait_ms=*/0, nullptr);
    const bool factor_now = pt_has_symbol_fx(portfolio_id, symbol);
    QMutexLocker lock(&st.mutex);
    if (ok && !factor_now) {
        st.no_factor_needed.insert(key);
        st.next_check.remove(key);
    } else {
        st.next_check.insert(key, now + (ok ? kPmFillFxRefreshMs : kPmFillFxRetryMs));
    }
    return ok;
}

double PaperMarkService::display_fx(const QString& portfolio_id, const QString& symbol, const QString& price_ccy,
                                    const QString& account_ccy) {
    if (!portfolio_id.isEmpty() && pt_has_symbol_fx(portfolio_id, symbol))
        return pt_get_symbol_fx(portfolio_id, symbol);
    if (price_ccy.trimmed().isEmpty() || account_ccy.trimmed().isEmpty())
        return 1.0; // currency unknown: taken as the account's own
    if (const auto f = fx_factor(price_ccy, account_ccy))
        return *f;
    return std::numeric_limits<double>::quiet_NaN();
}

QDateTime PaperMarkService::session_close_after(const QString& yahoo, const QDateTime& after_utc) {
    namespace mk = fincept::core::market;
    const mk::Venue* venue = mk::venue_for_symbol(yahoo);
    if (!venue || venue->is_24x7() || !after_utc.isValid())
        return {};
    QDate d = mk::trading_date(*venue, after_utc.toUTC());
    for (int i = 0; i < 16; ++i, d = d.addDays(1)) {
        const QDateTime close = mk::session_close_utc(*venue, d);
        if (close.isValid() && close > after_utc)
            return close;
    }
    return {};
}

void PaperMarkService::resync_public() {
    auto& hub = datahub::DataHub::instance();
    QSet<QString> want;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = public_.begin(); it != public_.end(); ++it) {
        const QString pid = it.key();
        PublicBinding& b = it.value();
        b.by_yahoo.clear();
        QStringList stored;
        for (const auto& p : pt_get_positions(pid))
            if (p.quantity != 0.0 && !p.symbol.isEmpty())
                stored << p.symbol;
        for (const auto& o : pt_get_orders(pid, QStringLiteral("pending")))
            if (!o.symbol.isEmpty())
                stored << o.symbol;
        for (const auto& s : stored) {
            const QString y = yahoo_for(b, s);
            if (y.isEmpty())
                continue;
            QStringList& list = b.by_yahoo[y];
            if (!list.contains(s))
                list << s;
            want.insert(y);
        }

        // Venue-settled portfolios: intraday positions square off and DAY orders
        // expire at each instrument's own session close.
        bool venue_settled = false;
        try {
            venue_settled = pt_portfolio_settles_by_venue(pt_get_portfolio(pid));
        } catch (...) {
            LOG_WARN_RL(TAG, QStringLiteral("settle_read|") + pid, 300000,
                        logf("Paper portfolio could not be read at session close; settlement skipped",
                             {{"portfolio", log_mask(pid)}}));
        }
        if (venue_settled) {
            // Settlement books cash at the per-symbol factor, which is 1.0 until set
            // (it lives in memory: unset after a restart until a quote and its rate
            // arrive). Set it now where a rate is known; a cross-currency symbol still
            // without one is not settled this pass (an invalid close defers it to the
            // next resync, seconds later) instead of being booked at 1:1.
            auto fx_ready = [this, &b, &pid](const QString& sym) {
                if (pt_has_symbol_fx(pid, sym))
                    return true;
                const QString yahoo = yahoo_for(b, sym);
                QString ccy = public_ccy_.value(yahoo);
                if (ccy.isEmpty())
                    ccy = fincept::core::market::currency_for_symbol(yahoo);
                if (ccy.isEmpty() || fincept::core::market::major_currency(ccy) == b.account_currency.toUpper()) {
                    if (!ccy.isEmpty() && ccy != b.account_currency)
                        pt_set_symbol_fx(pid, sym, 1.0 / fincept::core::market::minor_unit_divisor(ccy));
                    return true; // unknown (taken as the account's) or the same currency
                }
                if (const auto f = fx_factor(ccy, b.account_currency)) {
                    pt_set_symbol_fx(pid, sym, *f);
                    return true;
                }
                return false;
            };
            const int n = pt_settle_delayed_sessions(
                pid,
                [this, &b, &fx_ready](const QString& sym, const QDateTime& after) {
                    if (!fx_ready(sym))
                        return QDateTime(); // no rate to book the cash yet: settle on a later pass
                    return session_close_after(yahoo_for(b, sym), after);
                },
                now);
            if (n > 0) {
                LOG_INFO(TAG, QString("Session close: %1 position(s)/order(s) settled in %2").arg(n).arg(pid));
                emit portfolio_changed(pid);
            }
        }
    }

    for (auto it = public_subs_.begin(); it != public_subs_.end();) {
        if (!want.contains(*it)) {
            hub.unsubscribe(this, services::MarketDataService::quote_topic(*it));
            it = public_subs_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& y : want) {
        if (public_subs_.contains(y))
            continue;
        public_subs_.insert(y);
        const QString topic = services::MarketDataService::quote_topic(y);
        hub.subscribe(this, topic, [this, y](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            on_public_quote(y, v.value<services::QuoteData>());
        });
    }
}

void PaperMarkService::on_public_quote(const QString& yahoo, const services::QuoteData& q) {
    if (!(q.price > 0.0) || !std::isfinite(q.price))
        return;
    const qint64 received =
        datahub::DataHub::instance().last_publish_ms(services::MarketDataService::quote_topic(yahoo));
    const PtDelayedQuote dq = make_delayed_quote(yahoo, q, received);
    public_quotes_.insert(yahoo, dq);
    const QString ccy = q.currency.isEmpty() ? fincept::core::market::currency_for_symbol(yahoo) : q.currency;
    if (!ccy.isEmpty())
        public_ccy_.insert(yahoo, ccy);

    for (auto it = public_.cbegin(); it != public_.cend(); ++it) {
        const auto found = it->by_yahoo.constFind(yahoo);
        if (found == it->by_yahoo.constEnd())
            continue;
        const QString& pid = it.key();
        // Cash is booked in the account currency: keep the symbol's factor current.
        // Without a rate nothing fills (it would book at a wrong factor); marks still move.
        const auto fx = fx_factor(ccy, it->account_currency);
        for (const QString& stored : found.value()) {
            if (fx)
                pt_set_symbol_fx(pid, stored, *fx);
            pending_[pid][stored] = dq.price;
            if (fx)
                pt_match_delayed_quote(pid, stored, dq);
        }
    }
    if (!flush_armed_) {
        flush_armed_ = true;
        QTimer::singleShot(kFlushMs, this, [this]() { flush_prices(); });
    }
}

void PaperMarkService::on_quote(const QString& account_id, const QString& portfolio_id, const QString& symbol,
                                const BrokerQuote& quote) {
    if (quote.ltp <= 0.0)
        return; // garbage/zero tick — the repo guard would drop it anyway; skip the work
    auto it = bound_.constFind(account_id);
    if (it == bound_.constEnd())
        return;

    // Reconcile the live quote symbol to a stored position symbol.
    QString pos_sym;
    if (it->exact.contains(symbol)) {
        pos_sym = symbol; // exact match (equities + brokers that normalise identically)
    } else {
        // Option fallback: the live spelling ("…C<strike>") differs from the
        // position spelling ("…<strike>CE"). Match by (underlying, side, strike).
        const auto tick = fyers_parse_option(symbol);
        if (tick.valid && tick.strike > 0.0) {
            const QString strike_str = QString::number(qRound64(tick.strike));
            for (const auto& leg : it->legs) {
                if (leg.is_call != tick.is_call || leg.underlying != tick.underlying)
                    continue;
                QString core = leg.symbol.section(QLatin1Char(':'), -1);
                if (core.endsWith(QLatin1String("CE")) || core.endsWith(QLatin1String("PE")))
                    core.chop(2);
                if (core.endsWith(strike_str)) {
                    pos_sym = leg.symbol;
                    break;
                }
            }
        }
    }
    if (pos_sym.isEmpty())
        return;

    // Coalesce the SQLite write; the in-memory matcher sees every tick.
    pending_[portfolio_id][pos_sym] = quote.ltp;
    if (!flush_armed_) {
        flush_armed_ = true;
        QTimer::singleShot(kFlushMs, this, [this]() { flush_prices(); });
    }

    // A fill here (resting limit / stop, SL / TP bracket) books cash at the per-symbol
    // factor, which is unset after a restart: never match at 1:1 (see fill_fx_ready).
    if (!fill_fx_ready(portfolio_id, pos_sym))
        return;

    PriceData pd;
    pd.last = quote.ltp;
    pd.bid = quote.bid;
    pd.ask = quote.ask;
    pd.timestamp = quote.timestamp;
    OrderMatcher::instance().check_orders(pos_sym, pd, portfolio_id);
    OrderMatcher::instance().check_sl_tp_triggers(portfolio_id, pos_sym, quote.ltp);
}

void PaperMarkService::flush_prices() {
    flush_armed_ = false;
    if (pending_.isEmpty())
        return;
    const auto snapshot = pending_;
    pending_.clear();

    // One transaction for the whole flush. Each pt_update_position_price() is a
    // bare UPDATE, so without this the 1 s timer issued one implicit commit per
    // position — 40 commits/second on a 40-position book, every second the
    // market is open.
    auto& db = fincept::Database::instance();
    const bool batched = db.begin_transaction().is_ok();
    if (!batched)
        LOG_WARN_RL(TAG, QStringLiteral("flush_tx"), 60000,
                    logf("Paper price marks written one by one: a transaction could not be opened",
                         {{"portfolios", static_cast<int>(snapshot.size())}}));

    QStringList marked;
    marked.reserve(snapshot.size());
    for (auto pit = snapshot.cbegin(); pit != snapshot.cend(); ++pit) {
        const QString& pid = pit.key();
        for (auto sit = pit.value().cbegin(); sit != pit.value().cend(); ++sit)
            pt_update_position_price(pid, sit.key(), sit.value()); // repo guards price <= 0
        marked.append(pid);
    }

    if (batched) {
        auto c = db.commit();
        if (c.is_err()) {
            LOG_ERROR_RL(TAG, QStringLiteral("flush_commit"), 60000,
                         logf("Paper price marks could not be saved", {{"error", QString::fromStdString(c.error())}}));
            db.rollback();
            return; // nothing landed — don't tell listeners marks are fresh
        }
    }

    // Emit only after the commit, so a slot that re-reads pt_positions sees the
    // marks it was just told about.
    for (const auto& pid : marked)
        emit prices_marked(pid);
}

} // namespace fincept::trading
