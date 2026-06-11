// UnifiedPortfolioService — merge layer between per-account AccountDataStreams
// and the Portfolio Monitor screen. See header for the contract.

#include "trading/UnifiedPortfolioService.h"

#include "trading/AccountDataStream.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"

#include <QMap>
#include <QPointer>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

namespace fincept::trading {

namespace {
// EquityBottomPanel convention: side carries the sign for brokers that report
// unsigned quantity; brokers that already sign the quantity (Dhan netQty) pass
// through unchanged because -|q| == q for q < 0.
double signed_qty(const QString& side, double qty) {
    return side.startsWith(QLatin1Char('s'), Qt::CaseInsensitive) ? -std::fabs(qty) : qty;
}
} // namespace

UnifiedPortfolioService& UnifiedPortfolioService::instance() {
    static UnifiedPortfolioService inst;
    return inst;
}

UnifiedPortfolioService::UnifiedPortfolioService(QObject* parent) : QObject(parent) {
    summary_debounce_ = new QTimer(this);
    summary_debounce_->setSingleShot(true);
    summary_debounce_->setInterval(500);
    connect(summary_debounce_, &QTimer::timeout, this, [this]() {
        recompute_summary();
        emit summary_changed();
    });
}

// ── Account enumeration & wiring ─────────────────────────────────────────────

void UnifiedPortfolioService::activate() {
    auto& mgr = AccountManager::instance();
    auto& dsm = DataStreamManager::instance();
    bool set_changed = false;

    for (const auto& acct : mgr.active_accounts()) {
        auto* broker = BrokerRegistry::instance().get(acct.broker_id);
        if (!broker)
            continue;
        const auto profile = broker->profile();
        if (QString(profile.currency) != QLatin1String("INR"))
            continue; // user-confirmed scope: ₹ brokers only for now

        Acct& a = accts_[acct.account_id];
        if (a.label.isEmpty()) {
            a.label = QString("%1 — %2").arg(profile.display_name, acct.display_name);
            a.broker_id = acct.broker_id;
            a.default_exchange = profile.default_exchange;
            set_changed = true;
        }
        a.paper_portfolio_id = acct.paper_portfolio_id; // may be set after account creation
        a.live = acct.state == ConnectionState::Connected || acct.state == ConnectionState::Connecting;

        dsm.start_stream(acct.account_id);
        connect_stream(acct.account_id);

        // Seed from the stream's cache so the monitor isn't blank until the next
        // signal; the refresh below brings it current.
        if (auto* stream = dsm.stream_for(acct.account_id)) {
            a.positions = stream->cached_positions();
            a.holdings = stream->cached_holdings();
        }
    }

    if (mode_ == Mode::Paper)
        rebuild_paper();
    else {
        rebuild_positions();
        rebuild_holdings();
    }
    schedule_summary();
    if (set_changed)
        emit accounts_changed();
    refresh_all();
}

void UnifiedPortfolioService::set_mode(Mode mode) {
    if (mode_ == mode)
        return;
    mode_ = mode;
    if (mode_ == Mode::Paper)
        rebuild_paper();
    else {
        rebuild_positions();
        rebuild_holdings();
    }
    schedule_summary();
    emit_all_changed();
}

void UnifiedPortfolioService::emit_all_changed() {
    emit positions_changed();
    emit holdings_changed();
}

void UnifiedPortfolioService::connect_stream(const QString& account_id) {
    Acct& a = accts_[account_id];
    if (a.wired)
        return;
    auto* stream = DataStreamManager::instance().stream_for(account_id);
    if (!stream)
        return;
    a.wired = true;

    connect(stream, &AccountDataStream::positions_updated, this, &UnifiedPortfolioService::ingest_positions);
    connect(stream, &AccountDataStream::holdings_updated, this, &UnifiedPortfolioService::ingest_holdings);
    connect(stream, &AccountDataStream::quote_updated, this, &UnifiedPortfolioService::ingest_quote);
    connect(stream, &AccountDataStream::connection_state_changed, this,
            [this](const QString& acct_id, ConnectionState state) {
                auto it = accts_.find(acct_id);
                if (it == accts_.end())
                    return;
                const bool live = state == ConnectionState::Connected || state == ConnectionState::Connecting;
                if (it->live != live) {
                    it->live = live;
                    schedule_summary();
                    emit accounts_changed();
                }
            });
}

void UnifiedPortfolioService::refresh_all() {
    if (mode_ == Mode::Paper) {
        // Paper state lives in the local pt_* engine — re-read and repaint now.
        // Streams keep running regardless (they mark paper prices via quotes).
        rebuild_paper();
        schedule_summary();
        emit_all_changed();
        return;
    }
    auto& dsm = DataStreamManager::instance();
    for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it)
        dsm.refresh_portfolio(it.key());
}

QVector<UnifiedPortfolioService::AccountInfo> UnifiedPortfolioService::accounts() const {
    QVector<AccountInfo> out;
    out.reserve(accts_.size());
    for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it)
        out.append({it.key(), it->label, it->broker_id, it->default_exchange, it->paper_portfolio_id, it->live});
    std::sort(out.begin(), out.end(),
              [](const AccountInfo& x, const AccountInfo& y) { return x.label < y.label; });
    return out;
}

// ── Ingestion ────────────────────────────────────────────────────────────────

void UnifiedPortfolioService::ingest_positions(const QString& account_id,
                                               const QVector<BrokerPosition>& positions) {
    auto it = accts_.find(account_id);
    if (it == accts_.end())
        return; // not a tracked (INR) account
    it->positions = positions;
    if (mode_ != Mode::Live)
        return; // cache for later; paper view must never blend live rows
    rebuild_positions();
    schedule_summary();
    emit positions_changed();
}

void UnifiedPortfolioService::ingest_holdings(const QString& account_id,
                                              const QVector<BrokerHolding>& holdings) {
    auto it = accts_.find(account_id);
    if (it == accts_.end())
        return;
    it->holdings = holdings;
    if (mode_ != Mode::Live)
        return; // cache for later; paper view must never blend live rows
    rebuild_holdings();
    schedule_summary();
    emit holdings_changed();
}

// Per-tick patch: O(children of one symbol). Keep this lean — quote ticks are
// unthrottled on the GUI thread (project hot-path constraint).
void UnifiedPortfolioService::ingest_quote(const QString& account_id, const QString& symbol,
                                           const BrokerQuote& quote) {
    if (!accts_.contains(account_id) || quote.ltp <= 0.0)
        return;

    // Paper rows belong to the local engine, not to the broker whose feed the
    // tick arrived on — any tracked account's quote for the symbol marks them.
    const bool match_any_account = mode_ == Mode::Paper;

    auto patch = [&](QVector<AggRow>& rows, bool holdings) -> bool {
        for (AggRow& row : rows) {
            if (row.symbol != symbol)
                continue;
            bool touched = false;
            double pnl_sum = 0, day_sum = 0, current_sum = 0;
            for (AggChild& c : row.children) {
                if (match_any_account || c.account_id == account_id) {
                    c.ltp = quote.ltp;
                    const double sq = signed_qty(c.side, c.qty);
                    c.pnl = (quote.ltp - c.avg_price) * sq;
                    if (holdings) {
                        if (quote.close > 0.0)
                            c.prev_close = quote.close;
                        c.current = quote.ltp * c.qty;
                        c.day_pnl = c.prev_close > 0.0 ? (quote.ltp - c.prev_close) * c.qty : c.day_pnl;
                    }
                    touched = true;
                }
                pnl_sum += c.pnl;
                day_sum += c.day_pnl;
                current_sum += c.current;
            }
            if (touched) {
                row.ltp = quote.ltp;
                row.pnl = pnl_sum;
                row.day_pnl = day_sum;
                if (holdings)
                    row.current = current_sum;
            }
            return touched;
        }
        return false;
    };

    if (patch(positions_, /*holdings=*/false)) {
        schedule_summary();
        emit position_patched(symbol);
    }
    if (patch(holdings_, /*holdings=*/true)) {
        schedule_summary();
        emit holding_patched(symbol);
    }
}

// ── Merge ────────────────────────────────────────────────────────────────────

void UnifiedPortfolioService::rebuild_positions() {
    // QMap keeps rows sorted by symbol for a stable display order.
    QMap<QPair<QString, QString>, AggRow> merged;
    for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it) {
        for (const BrokerPosition& p : it->positions) {
            if (p.quantity == 0.0)
                continue;
            AggRow& row = merged[{p.symbol, p.exchange}];
            row.symbol = p.symbol;
            row.exchange = p.exchange;
            AggChild c;
            c.account_id = it.key();
            c.broker_label = it->label;
            c.product = p.product_type;
            c.side = p.side;
            c.qty = p.quantity;
            c.avg_price = p.avg_price;
            c.ltp = p.ltp;
            c.pnl = p.pnl != 0.0 ? p.pnl
                                 : (p.ltp > 0.0 ? (p.ltp - p.avg_price) * signed_qty(p.side, p.quantity) : 0.0);
            c.day_pnl = p.day_pnl;
            row.children.append(c);
        }
    }

    positions_.clear();
    positions_.reserve(merged.size());
    for (AggRow& row : merged) {
        double abs_qty = 0;
        for (const AggChild& c : row.children) {
            const double sq = signed_qty(c.side, c.qty);
            row.total_qty += sq;
            row.w_avg_price += c.avg_price * std::fabs(sq);
            abs_qty += std::fabs(sq);
            row.pnl += c.pnl;
            row.day_pnl += c.day_pnl;
            if (c.ltp > 0.0)
                row.ltp = c.ltp;
        }
        row.w_avg_price = abs_qty > 0.0 ? row.w_avg_price / abs_qty : 0.0;
        positions_.append(row);
    }
}

void UnifiedPortfolioService::rebuild_holdings() {
    QMap<QPair<QString, QString>, AggRow> merged;
    for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it) {
        for (const BrokerHolding& h : it->holdings) {
            if (h.quantity == 0.0)
                continue;
            AggRow& row = merged[{h.symbol, h.exchange}];
            row.symbol = h.symbol;
            row.exchange = h.exchange;
            AggChild c;
            c.account_id = it.key();
            c.broker_label = it->label;
            c.product = QStringLiteral("CNC");
            c.qty = h.quantity;
            c.avg_price = h.avg_price;
            c.ltp = h.ltp;
            c.prev_close = h.prev_close;
            c.invested = h.invested_value > 0.0 ? h.invested_value : h.quantity * h.avg_price;
            c.current = h.current_value > 0.0 ? h.current_value : (h.ltp > 0.0 ? h.quantity * h.ltp : 0.0);
            c.pnl = h.pnl != 0.0 ? h.pnl : c.current - c.invested;
            c.day_pnl = h.prev_close > 0.0 && h.ltp > 0.0 ? (h.ltp - h.prev_close) * h.quantity : 0.0;
            row.children.append(c);
        }
    }

    holdings_.clear();
    holdings_.reserve(merged.size());
    for (AggRow& row : merged) {
        double abs_qty = 0;
        for (const AggChild& c : row.children) {
            row.total_qty += c.qty;
            row.w_avg_price += c.avg_price * std::fabs(c.qty);
            abs_qty += std::fabs(c.qty);
            row.pnl += c.pnl;
            row.day_pnl += c.day_pnl;
            row.invested += c.invested;
            row.current += c.current;
            if (c.ltp > 0.0)
                row.ltp = c.ltp;
        }
        row.w_avg_price = abs_qty > 0.0 ? row.w_avg_price / abs_qty : 0.0;
        holdings_.append(row);
    }
}

// Paper model: every account's pt_* portfolio, CNC rows → holdings (matching the
// equity tab's storage model: ALL paper exposure lives in pt_positions tagged by
// product), MIS/NRML rows → positions. Reads fresh from the engine each time.
void UnifiedPortfolioService::rebuild_paper() {
    QMap<QPair<QString, QString>, AggRow> pos_merged;
    QMap<QPair<QString, QString>, AggRow> hold_merged;

    for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it) {
        if (it->paper_portfolio_id.isEmpty())
            continue;
        for (const PtPosition& p : pt_get_positions(it->paper_portfolio_id)) {
            if (p.quantity == 0.0)
                continue;
            const bool is_holding = p.product.compare(QLatin1String("CNC"), Qt::CaseInsensitive) == 0;
            // Paper symbols are stored bare; exchange isn't carried on the row, so
            // group by symbol only (empty exchange) — consistent across accounts.
            AggRow& row = (is_holding ? hold_merged : pos_merged)[{p.symbol, QString()}];
            row.symbol = p.symbol;

            AggChild c;
            c.account_id = it.key();
            c.broker_label = it->label + QStringLiteral(" (paper)");
            c.product = p.product;
            c.side = p.side.toUpper();
            c.qty = p.quantity;
            c.avg_price = p.entry_price;
            c.ltp = p.current_price;
            c.pnl = p.unrealized_pnl;
            c.day_pnl = 0.0; // paper engine doesn't track prev-close day P&L
            if (is_holding) {
                c.invested = p.quantity * p.entry_price;
                c.current = p.current_price > 0.0 ? p.quantity * p.current_price : 0.0;
            }
            row.children.append(c);
        }
    }

    auto finalize = [](QMap<QPair<QString, QString>, AggRow>& merged, QVector<AggRow>& out) {
        out.clear();
        out.reserve(merged.size());
        for (AggRow& row : merged) {
            double abs_qty = 0;
            for (const AggChild& c : row.children) {
                const double sq = signed_qty(c.side, c.qty);
                row.total_qty += sq;
                row.w_avg_price += c.avg_price * std::fabs(sq);
                abs_qty += std::fabs(sq);
                row.pnl += c.pnl;
                row.day_pnl += c.day_pnl;
                row.invested += c.invested;
                row.current += c.current;
                if (c.ltp > 0.0)
                    row.ltp = c.ltp;
            }
            row.w_avg_price = abs_qty > 0.0 ? row.w_avg_price / abs_qty : 0.0;
            out.append(row);
        }
    };
    finalize(pos_merged, positions_);
    finalize(hold_merged, holdings_);
}

void UnifiedPortfolioService::schedule_summary() {
    // Throttle, NOT debounce: restarting the timer on every tick starves the
    // summary under a continuous quote stream (it would never fire), leaving the
    // header frozen at stale totals while the rows keep moving.
    if (!summary_debounce_->isActive())
        summary_debounce_->start();
}

void UnifiedPortfolioService::recompute_summary() {
    PortfolioSummary s;
    for (const AggRow& r : positions_) {
        s.positions_pnl += r.pnl;
        s.positions_day_pnl += r.day_pnl;
    }
    for (const AggRow& r : holdings_) {
        s.holdings_pnl += r.pnl;
        s.holdings_day_pnl += r.day_pnl;
        s.invested += r.invested;
        s.current += r.current;
    }
    s.return_pct = s.invested > 0.0 ? (s.current - s.invested) / s.invested * 100.0 : 0.0;
    s.accounts_total = accts_.size();
    for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it)
        s.accounts_live += it->live ? 1 : 0;
    summary_ = s;
}

// ── Actions ──────────────────────────────────────────────────────────────────
// LIVE actions run on a worker thread (broker REST is blocking) and report back
// through action_finished on the main thread. PAPER actions hit the local pt_*
// engine synchronously (SQLite — the same main-thread path the equity tab uses).

namespace {
// Close one paper position: reduce-only opposite market order filled at the
// position's marked price. Returns true on success, fills `err` otherwise.
bool close_paper_position(const QString& portfolio_id, const QString& symbol, const QString& product,
                          QString* err) {
    for (const PtPosition& p : pt_get_positions(portfolio_id)) {
        if (p.symbol != symbol || (!product.isEmpty() && p.product != product) || p.quantity == 0.0)
            continue;
        const double px = p.current_price > 0.0 ? p.current_price : p.entry_price;
        if (px <= 0.0) { // zero-price guard — never book a fill at 0
            if (err)
                *err = QString("%1: no price available for fill").arg(symbol);
            return false;
        }
        const QString side = p.side.startsWith(QLatin1Char('s'), Qt::CaseInsensitive)
                                 ? QStringLiteral("buy")
                                 : QStringLiteral("sell");
        try {
            auto order = pt_place_order(portfolio_id, p.symbol, side, QStringLiteral("market"), p.quantity,
                                        px, std::nullopt, /*reduce_only=*/true, QString(), p.product);
            pt_fill_order(order.id, px);
            return true;
        } catch (const std::exception& e) {
            if (err)
                *err = QString("%1: %2").arg(symbol, QString::fromUtf8(e.what()));
            return false;
        }
    }
    if (err)
        *err = QString("%1: paper position not found").arg(symbol);
    return false;
}
} // namespace

void UnifiedPortfolioService::exit_child(const QString& account_id, const QString& symbol,
                                         const QString& exchange, const QString& product) {
    if (mode_ == Mode::Paper) {
        const QString pid = accts_.value(account_id).paper_portfolio_id;
        QString err;
        const bool ok = close_paper_position(pid, symbol, product, &err);
        emit action_finished(ok, ok ? QString("Squared off %1 (paper)").arg(symbol) : err);
        refresh_all();
        return;
    }
    QPointer<UnifiedPortfolioService> self(this);
    auto future = QtConcurrent::run([self, account_id, symbol, exchange, product]() {
        auto r = UnifiedTrading::instance().close_position(account_id, symbol, exchange, product);
        const bool ok = r.success;
        const QString msg = ok ? QString("Squared off %1").arg(symbol)
                               : QString("Exit %1 failed: %2").arg(symbol, r.error);
        QMetaObject::invokeMethod(
            self,
            [self, ok, msg]() {
                if (!self)
                    return;
                emit self->action_finished(ok, msg);
                self->refresh_all();
            },
            Qt::QueuedConnection);
    });
    Q_UNUSED(future)
}

void UnifiedPortfolioService::exit_symbol(const QString& symbol, const QString& exchange,
                                          bool from_holdings) {
    // Snapshot the children on the main thread before hopping to the worker.
    QVector<AggChild> children;
    const QVector<AggRow>& rows = from_holdings ? holdings_ : positions_;
    for (const AggRow& r : rows)
        if (r.symbol == symbol && r.exchange == exchange)
            children = r.children;
    if (children.isEmpty()) {
        emit action_finished(false, QString("No rows found for %1").arg(symbol));
        return;
    }

    if (mode_ == Mode::Paper) {
        int ok_count = 0;
        QStringList errors;
        for (const AggChild& c : children) {
            QString err;
            if (close_paper_position(accts_.value(c.account_id).paper_portfolio_id, symbol, c.product, &err))
                ++ok_count;
            else
                errors << err;
        }
        const bool all_ok = errors.isEmpty();
        emit action_finished(all_ok,
                             all_ok ? QString("Squared off %1 across %2 paper account(s)").arg(symbol).arg(ok_count)
                                    : QString("%1 of %2 squared off; %3")
                                          .arg(ok_count)
                                          .arg(int(children.size()))
                                          .arg(errors.join("; ")));
        refresh_all();
        return;
    }

    QPointer<UnifiedPortfolioService> self(this);
    auto future = QtConcurrent::run([self, symbol, exchange, children]() {
        int ok_count = 0;
        QStringList errors;
        for (const AggChild& c : children) {
            auto r = UnifiedTrading::instance().close_position(c.account_id, symbol, exchange, c.product);
            if (r.success)
                ++ok_count;
            else
                errors << QString("%1: %2").arg(c.broker_label, r.error);
        }
        const bool all_ok = errors.isEmpty();
        const QString msg = all_ok
            ? QString("Squared off %1 across %2 account(s)").arg(symbol).arg(ok_count)
            : QString("%1 of %2 squared off; %3").arg(ok_count).arg(int(children.size())).arg(errors.join("; "));
        QMetaObject::invokeMethod(
            self,
            [self, all_ok, msg]() {
                if (!self)
                    return;
                emit self->action_finished(all_ok, msg);
                self->refresh_all();
            },
            Qt::QueuedConnection);
    });
    Q_UNUSED(future)
}

void UnifiedPortfolioService::square_off_all_positions() {
    if (mode_ == Mode::Paper) {
        int closed = 0;
        QStringList errors;
        for (auto it = accts_.constBegin(); it != accts_.constEnd(); ++it) {
            if (it->paper_portfolio_id.isEmpty())
                continue;
            for (const PtPosition& p : pt_get_positions(it->paper_portfolio_id)) {
                if (p.quantity == 0.0 || p.product.compare(QLatin1String("CNC"), Qt::CaseInsensitive) == 0)
                    continue; // intraday/NRML only, like the live close_all
                QString err;
                if (close_paper_position(it->paper_portfolio_id, p.symbol, p.product, &err))
                    ++closed;
                else
                    errors << err;
            }
        }
        const bool all_ok = errors.isEmpty();
        emit action_finished(all_ok,
                             all_ok ? QString("Squared off %1 paper position(s)").arg(closed)
                                    : QString("Closed %1; errors: %2").arg(closed).arg(errors.join("; ")));
        refresh_all();
        return;
    }

    const QStringList ids = accts_.keys();
    QPointer<UnifiedPortfolioService> self(this);
    auto future = QtConcurrent::run([self, ids]() {
        int closed = 0;
        QStringList errors;
        for (const QString& id : ids) {
            auto r = UnifiedTrading::instance().close_all_positions(id);
            if (r.success && r.data) {
                closed += r.data->closed_symbols.size();
                for (const auto& f : r.data->failed)
                    errors << QString("%1: %2").arg(f.first, f.second);
            } else if (!r.success && !r.error.isEmpty() && !r.error.contains("No open positions")) {
                errors << r.error;
            }
        }
        const bool all_ok = errors.isEmpty();
        const QString msg = all_ok ? QString("Squared off %1 position(s) across all accounts").arg(closed)
                                   : QString("Closed %1; errors: %2").arg(closed).arg(errors.join("; "));
        QMetaObject::invokeMethod(
            self,
            [self, all_ok, msg]() {
                if (!self)
                    return;
                emit self->action_finished(all_ok, msg);
                self->refresh_all();
            },
            Qt::QueuedConnection);
    });
    Q_UNUSED(future)
}

void UnifiedPortfolioService::place_new_order(const QString& account_id, const UnifiedOrder& order) {
    if (mode_ == Mode::Paper) {
        const QString pid = accts_.value(account_id).paper_portfolio_id;
        if (pid.isEmpty()) {
            emit action_finished(false, QString("No paper portfolio for this account"));
            return;
        }
        const QString side = order.side == OrderSide::Buy ? QStringLiteral("buy") : QStringLiteral("sell");
        const bool is_market = order.order_type == OrderType::Market;
        // Market fills need a price: use the live cached quote from this
        // account's stream (zero-price guard — never fill at 0).
        double px = order.price;
        if (is_market) {
            if (auto* stream = DataStreamManager::instance().stream_for(account_id))
                px = stream->cached_quote(order.symbol).ltp;
            if (px <= 0.0) {
                emit action_finished(false,
                                     QString("No live price for %1 yet — wait for quotes").arg(order.symbol));
                return;
            }
        }
        const QString product = order.product_type == ProductType::Delivery ? QStringLiteral("CNC")
                                : order.product_type == ProductType::Margin ? QStringLiteral("NRML")
                                                                            : QStringLiteral("MIS");
        try {
            auto pt_order = pt_place_order(pid, order.symbol, side,
                                           is_market ? QStringLiteral("market") : QStringLiteral("limit"),
                                           order.quantity, px, std::nullopt, /*reduce_only=*/false,
                                           order.exchange, product);
            if (is_market) {
                pt_fill_order(pt_order.id, px);
                emit action_finished(true, QString("Paper order filled: %1 @ %2")
                                               .arg(order.symbol)
                                               .arg(px, 0, 'f', 2));
            } else {
                OrderMatcher::instance().add_order(pt_order);
                emit action_finished(true, QString("Paper order queued: %1").arg(order.symbol));
            }
        } catch (const std::exception& e) {
            emit action_finished(false, QString("Paper order failed: %1").arg(QString::fromUtf8(e.what())));
        }
        refresh_all();
        return;
    }

    QPointer<UnifiedPortfolioService> self(this);
    auto future = QtConcurrent::run([self, account_id, order]() {
        auto r = UnifiedTrading::instance().place_order(account_id, order);
        const QString msg = r.success
            ? QString("Order placed: %1 (%2)").arg(order.symbol, r.order_id)
            : QString("Order failed for %1: %2").arg(order.symbol, r.message);
        QMetaObject::invokeMethod(
            self,
            [self, ok = r.success, msg]() {
                if (!self)
                    return;
                emit self->action_finished(ok, msg);
                self->refresh_all();
            },
            Qt::QueuedConnection);
    });
    Q_UNUSED(future)
}

// ── Test seams ───────────────────────────────────────────────────────────────

void UnifiedPortfolioService::test_register_account(const QString& account_id, const QString& label,
                                                    const QString& currency) {
    if (currency != QLatin1String("INR"))
        return; // mirrors the activate() filter — non-INR accounts are excluded
    Acct& a = accts_[account_id];
    a.label = label;
    a.live = true;
    a.wired = true; // no real stream in tests
}

void UnifiedPortfolioService::test_clear() {
    accts_.clear();
    positions_.clear();
    holdings_.clear();
    summary_ = {};
}

} // namespace fincept::trading
