// src/screens/equity_trading/EquityTradingScreen_Watchlists.cpp
//
// Named-watchlist controller for the Equity Trading screen. Backed by the
// shared (global) WatchlistRepository — the same lists the dedicated Watchlist
// screen manages, so they stay unified and benefit from cloud sync. The active
// list's symbols drive the live stream subscriptions + hub quote topics.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "screens/equity_trading/EquityTypes.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "storage/repositories/WatchlistRepository.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/websocket/FyersTickTypes.h"

#include <QPair>
#include <QVector>

namespace fincept::screens {

namespace {
const QString WL_TAG = "EquityTrading";
}

void EquityTradingScreen::load_watchlists() {
    auto& repo = fincept::WatchlistRepository::instance();
    auto r = repo.list_all();
    QVector<fincept::Watchlist> lists = r.is_ok() ? r.value() : QVector<fincept::Watchlist>{};

    // First run anywhere (no lists in the repo) — seed one from the focused
    // broker's default_watchlist so existing behaviour is preserved.
    if (lists.isEmpty()) {
        QString seed_name = QStringLiteral("Watchlist");
        QStringList seed_syms;
        if (auto* b = trading::BrokerRegistry::instance().get(broker_id_for_focused())) {
            const auto prof = b->profile();
            if (prof.region == QLatin1String("IN")) {
                seed_name = QStringLiteral("Nifty 50");
                seed_syms = equity::NIFTY50_WATCHLIST;
            } else if (prof.region == QLatin1String("US")) {
                seed_name = QStringLiteral("US Large Caps");
                seed_syms = equity::US_WATCHLIST;
            } else {
                seed_syms = QStringList(prof.default_watchlist.begin(), prof.default_watchlist.end());
            }
        }
        auto cr = repo.create(seed_name);
        if (cr.is_ok()) {
            const QString new_id = cr.value().id;
            for (const auto& s : seed_syms)
                repo.add_stock(new_id, s, {}, selected_exchange_);
            auto r2 = repo.list_all();
            if (r2.is_ok())
                lists = r2.value();
        }
    }

    if (lists.isEmpty()) {
        LOG_WARN(WL_TAG, "No watchlists available (seed failed)");
        return;
    }

    // For Indian brokers, guarantee a "Nifty 50" list exists even when other
    // lists are already present. If we have to create it, make it the active
    // selection so connecting an Indian broker surfaces Nifty 50 by default;
    // an existing Nifty 50 (or the user's current pick) is left untouched.
    bool is_indian = false;
    if (auto* b = trading::BrokerRegistry::instance().get(broker_id_for_focused()))
        is_indian = (b->profile().region == QLatin1String("IN"));
    if (is_indian) {
        QString nifty_id;
        for (const auto& w : lists) {
            if (w.name.compare(QLatin1String("Nifty 50"), Qt::CaseInsensitive) == 0) {
                nifty_id = w.id;
                break;
            }
        }
        if (nifty_id.isEmpty()) {
            auto cr = repo.create(QStringLiteral("Nifty 50"));
            if (cr.is_ok()) {
                nifty_id = cr.value().id;
                for (const auto& s : equity::NIFTY50_WATCHLIST)
                    repo.add_stock(nifty_id, s, {}, selected_exchange_);
                auto r3 = repo.list_all();
                if (r3.is_ok())
                    lists = r3.value();
            }
        }
        // No user-selected watchlist yet → default Indian brokers to Nifty 50,
        // even when other lists exist. A present active_watchlist_id_ means the
        // user already picked a list, so leave their choice untouched.
        bool indian_active_present = false;
        for (const auto& w : lists)
            if (w.id == active_watchlist_id_) {
                indian_active_present = true;
                break;
            }
        if (!indian_active_present && !nifty_id.isEmpty())
            active_watchlist_id_ = nifty_id;
    }

    // Pick the active list: keep the current one if still present, else the
    // default-flagged one, else the first.
    bool active_present = false;
    for (const auto& w : lists) {
        if (w.id == active_watchlist_id_) {
            active_present = true;
            break;
        }
    }
    if (!active_present) {
        active_watchlist_id_ = lists.first().id;
        for (const auto& w : lists) {
            if (w.is_default) {
                active_watchlist_id_ = w.id;
                break;
            }
        }
    }

    QVector<QPair<QString, QString>> combo;
    combo.reserve(lists.size());
    for (const auto& w : lists)
        combo.append({w.id, w.name});
    if (watchlist_)
        watchlist_->set_watchlists(combo, active_watchlist_id_);

    // on_account_changed wires the stream itself; standalone callers pass true.
    apply_active_watchlist(false);
}

void EquityTradingScreen::apply_active_watchlist(bool resubscribe) {
    if (active_watchlist_id_.isEmpty())
        return;

    auto r = fincept::WatchlistRepository::instance().get_stocks(active_watchlist_id_);
    QStringList syms;
    if (r.is_ok()) {
        const auto& stocks = r.value();
        syms.reserve(stocks.size());
        for (const auto& s : stocks)
            syms << s.symbol;
    }
    watchlist_symbols_ = syms;
    const QStringList eff = effective_symbols(); // active list ∪ open positions
    if (watchlist_)
        watchlist_->set_symbols(eff);

    if (!resubscribe)
        return;

    auto* stream = trading::DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream) {
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), eff);
        stream->fetch_orderbook(selected_symbol_);
    }

    // (Re)bind the F&O option-position price router to the focused stream — but
    // only while an option is actually held. Fyers' live HSM tick spells an
    // option differently from its REST position symbol, so option ticks never
    // arrive on the per-symbol quote topics; route_option_quote reconciles them
    // by (underlying, strike, side). Keeping the binding off when no option is
    // held avoids a per-tick parse on the equity hot path. See route_option_quote.
    if (opt_quote_conn_) {
        QObject::disconnect(opt_quote_conn_);
        opt_quote_conn_ = {};
    }
    if (stream) {
        bool has_option = false;
        for (const auto& s : position_symbols_)
            if (trading::fyers_parse_option(s).valid) {
                has_option = true;
                break;
            }
        if (has_option)
            opt_quote_conn_ = connect(stream, &trading::AccountDataStream::quote_updated,
                                      this, &EquityTradingScreen::route_option_quote);
        LOG_INFO("posdbg", QString("opt route bind: has_option=%1 stream=%2 conn=%3 pos=[%4]")
                               .arg(has_option ? "Y" : "N")
                               .arg(stream ? "Y" : "N")
                               .arg(opt_quote_conn_ ? "Y" : "N")
                               .arg(position_symbols_.join(',')));
    }

    if (isVisible() && hub_active_)
        hub_subscribe_streaming(); // re-point quote topics at the new symbol set
}

void EquityTradingScreen::on_watchlist_selected(const QString& id) {
    if (id.isEmpty() || id == active_watchlist_id_)
        return;
    active_watchlist_id_ = id;
    apply_active_watchlist(true);
}

void EquityTradingScreen::on_watchlist_create(const QString& name) {
    auto cr = fincept::WatchlistRepository::instance().create(name);
    if (!cr.is_ok())
        return;
    active_watchlist_id_ = cr.value().id;
    load_watchlists();              // refresh combo, selects the new (empty) list
    apply_active_watchlist(true);
}

void EquityTradingScreen::on_watchlist_rename(const QString& id, const QString& name) {
    auto& repo = fincept::WatchlistRepository::instance();
    auto g = repo.get(id);
    if (g.is_ok()) {
        auto w = g.value();
        w.name = name;
        repo.update(w);
    }
    load_watchlists();
}

void EquityTradingScreen::on_watchlist_delete(const QString& id) {
    auto& repo = fincept::WatchlistRepository::instance();
    auto r = repo.list_all();
    if (r.is_ok() && r.value().size() <= 1) {
        LOG_INFO(WL_TAG, "Refusing to delete the last watchlist");
        return; // always keep at least one list
    }
    repo.remove(id);
    if (active_watchlist_id_ == id)
        active_watchlist_id_.clear(); // load_watchlists() picks another
    load_watchlists();
    apply_active_watchlist(true);
}

void EquityTradingScreen::on_watchlist_symbol_added(const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (active_watchlist_id_.isEmpty() || sym.isEmpty())
        return;
    fincept::WatchlistRepository::instance().add_stock(active_watchlist_id_, sym, {}, selected_exchange_);
    apply_active_watchlist(true);
}

void EquityTradingScreen::on_watchlist_symbol_removed(const QString& symbol) {
    if (active_watchlist_id_.isEmpty() || symbol.isEmpty())
        return;
    fincept::WatchlistRepository::instance().remove_stock(active_watchlist_id_, symbol);
    apply_active_watchlist(true);
}

QStringList EquityTradingScreen::effective_symbols() const {
    QStringList eff = watchlist_symbols_;
    for (const auto& s : position_symbols_)
        if (!s.isEmpty() && !eff.contains(s))
            eff << s;
    // Holdings get live WS prices too — otherwise their LTP only refreshes on the
    // 5-minute REST portfolio poll and visibly lags the broker app.
    for (const auto& s : holding_symbols_)
        if (!s.isEmpty() && !eff.contains(s))
            eff << s;
    return eff;
}

void EquityTradingScreen::update_position_symbols(const QStringList& syms) {
    QStringList deduped;
    for (const auto& s : syms)
        if (!s.isEmpty() && !deduped.contains(s))
            deduped << s;
    if (deduped == position_symbols_)
        return; // unchanged — avoid resubscribe churn
    position_symbols_ = deduped;
    LOG_INFO("posdbg", QString("update_position_symbols: [%1]").arg(deduped.join(',')));
    // Re-display + re-subscribe to the new active∪positions union so held symbols
    // get live WebSocket prices. Deferred to the next tick because this can fire
    // from inside a hub callback, and apply_active_watchlist re-subscribes the hub.
    QMetaObject::invokeMethod(this, [this]() { apply_active_watchlist(true); }, Qt::QueuedConnection);
}

void EquityTradingScreen::update_holding_symbols(const QStringList& syms) {
    QStringList deduped;
    for (const auto& s : syms)
        if (!s.isEmpty() && !deduped.contains(s))
            deduped << s;
    if (deduped == holding_symbols_)
        return; // unchanged — avoid resubscribe churn
    holding_symbols_ = deduped;
    LOG_INFO("posdbg", QString("update_holding_symbols: [%1]").arg(deduped.join(',')));
    // Same path as positions: re-subscribe the active∪positions∪holdings union so
    // holding LTP streams live instead of waiting on the 5-minute REST poll.
    // Deferred because this fires from inside the holdings hub callback.
    QMetaObject::invokeMethod(this, [this]() { apply_active_watchlist(true); }, Qt::QueuedConnection);
}

} // namespace fincept::screens
