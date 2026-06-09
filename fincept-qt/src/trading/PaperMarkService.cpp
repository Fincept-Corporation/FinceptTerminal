#include "trading/PaperMarkService.h"

#include "core/logging/Logger.h"
#include "trading/AccountDataStream.h"
#include "trading/AccountManager.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/websocket/FyersTickTypes.h"

#include <QTimer>

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
            },
            Qt::QueuedConnection);
    });

    resync_timer_ = new QTimer(this);
    resync_timer_->setInterval(kResyncMs);
    connect(resync_timer_, &QTimer::timeout, this, &PaperMarkService::resync);
    resync_timer_->start();

    resync();
    LOG_INFO(TAG, "Paper mark-to-market service started");
}

void PaperMarkService::resync() {
    auto& dsm = DataStreamManager::instance();
    QSet<QString> should_track; // account_ids that currently warrant a binding

    for (const auto& acct : AccountManager::instance().active_accounts()) {
        if (acct.trading_mode != QLatin1String("paper") || acct.paper_portfolio_id.isEmpty())
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
    for (auto pit = snapshot.cbegin(); pit != snapshot.cend(); ++pit) {
        const QString& pid = pit.key();
        for (auto sit = pit.value().cbegin(); sit != pit.value().cend(); ++sit)
            pt_update_position_price(pid, sit.key(), sit.value()); // repo guards price <= 0
        emit prices_marked(pid);
    }
}

} // namespace fincept::trading
