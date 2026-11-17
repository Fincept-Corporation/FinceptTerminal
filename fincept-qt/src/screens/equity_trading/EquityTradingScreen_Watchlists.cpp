// src/screens/equity_trading/EquityTradingScreen_Watchlists.cpp
//
// Named-watchlist controller for the Equity Trading screen. Backed by the
// shared (global) WatchlistRepository — the same lists the dedicated Watchlist
// screen manages, so they stay unified and benefit from cloud sync. The active
// list's symbols drive the live stream subscriptions + hub quote topics.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "core/logging/Logger.h"
#include "screens/equity_trading/EquityTradingScreen.h"
#include "screens/equity_trading/EquityTypes.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"
#include "core/market/MarketProfile.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "storage/repositories/WatchlistRepository.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/instruments/InstrumentService.h"
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
    // broker's default_watchlist so existing behaviour is preserved; the built-in
    // paper account seeds canonical symbols for the user's market plus a world mix.
    if (lists.isEmpty()) {
        QString seed_name = QStringLiteral("Watchlist");
        QStringList seed_syms;
        QString seed_exchange = selected_exchange_;
        if (focused_is_builtin()) {
            seed_exchange.clear(); // canonical symbols carry their own market
            const QString region = fincept::core::market::MarketProfileStore::instance().current_id();
            if (region == QLatin1String("IN")) {
                seed_name = QStringLiteral("Nifty 50");
                for (const auto& s : equity::NIFTY50_WATCHLIST)
                    seed_syms << fincept::core::instrument::canonical_symbol(s, QStringLiteral("NSE"));
            } else {
                seed_name = QStringLiteral("World");
                seed_syms = {"AAPL",   "MSFT",   "NVDA",     "AMZN",     "SPY",   "^GSPC",   "RELIANCE.NS",
                             "^NSEI",  "VOD.L",  "SAP.DE",   "7203.T",   "0700.HK", "BHP.AX", "EURUSD=X",
                             "BTC-USD", "GC=F",  "ES=F"};
            }
        } else if (auto* b = trading::BrokerRegistry::instance().get(broker_id_for_focused())) {
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
            for (const auto& s : seed_syms) {
                if (s.isEmpty())
                    continue;
                // Canonical rows store their own venue; broker rows the broker's market.
                const QString ex = seed_exchange.isEmpty() ? equity::eq_instrument_for_stored(s, {}).venue : seed_exchange;
                repo.add_stock(new_id, s, {}, ex);
            }
            auto r2 = repo.list_all();
            if (r2.is_ok())
                lists = r2.value();
        }
    }

    if (lists.isEmpty()) {
        // The store's own failure (if any) is logged by the repository just before.
        LOG_WARN(WL_TAG, logf("No watchlists available: the default list could not be created",
                              {{"lists", 0}, {"reason", "the watchlist store returned no lists after seeding"}}));
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
    watchlist_exchanges_.clear();
    stored_symbol_of_.clear();
    row_block_reason_.clear();
    // Every stored row is kept (never dropped): rows saved broker-style ("RELIANCE" +
    // "NSE") are migrated on the fly to their canonical instrument (RELIANCE.NS) on the
    // paper account, and canonical rows are mapped to the focused broker's own symbol
    // (core::instrument::to_broker_symbol) — or shown marked when it cannot trade them.
    QStringList broker_exchanges;
    QString broker_name;
    const QString bid = broker_id_for_focused();
    if (!canonical_mode()) {
        if (auto* b = trading::BrokerRegistry::instance().get(bid)) {
            broker_exchanges = b->profile().exchanges;
            broker_name = b->profile().display_name;
        }
    }
    if (r.is_ok()) {
        const auto& stocks = r.value();
        syms.reserve(stocks.size());
        for (const auto& s : stocks) {
            QString screen = s.symbol;
            QString exch = s.exchange;
            if (canonical_mode()) {
                const auto inst = equity::eq_instrument_for_stored(s.symbol, s.exchange);
                if (!inst.symbol.isEmpty())
                    screen = inst.symbol;
                exch = inst.venue;
                instruments_.insert(screen, inst);
            } else {
                // Broker-style row on one of this broker's exchanges: as it always was.
                const bool bare = fincept::core::instrument::canonical_symbol(s.symbol) == s.symbol.trimmed().toUpper();
                const bool legacy = !s.exchange.isEmpty() && broker_exchanges.contains(s.exchange, Qt::CaseInsensitive) &&
                                    (bare || !s.symbol.contains(QLatin1Char('.')));
                if (!legacy) {
                    const auto inst = equity::eq_instrument_for_stored(s.symbol, s.exchange);
                    QString bex;
                    const QString bsym = inst.symbol.isEmpty()
                                             ? QString()
                                             : fincept::core::instrument::to_broker_symbol(inst, bid, &bex);
                    if (!bsym.isEmpty() &&
                        (broker_exchanges.isEmpty() || broker_exchanges.contains(bex, Qt::CaseInsensitive))) {
                        screen = bsym;
                        exch = bex;
                    } else {
                        screen = inst.symbol.isEmpty() ? s.symbol : inst.symbol;
                        exch = inst.venue;
                        row_block_reason_.insert(
                            screen, tr("%1 cannot trade %2").arg(broker_name.isEmpty() ? tr("This broker") : broker_name,
                                                                 inst.display.isEmpty() ? screen : inst.display));
                    }
                    instruments_.insert(screen, inst);
                }
            }
            if (syms.contains(screen))
                continue;
            syms << screen;
            stored_symbol_of_.insert(screen, s.symbol);
            // The exchange stored with each row lets the public feed map a US row
            // in a shared list to AAPL rather than AAPL.NS (exchange_for_symbol).
            if (!exch.isEmpty())
                watchlist_exchanges_.insert(screen, exch);
        }
    }
    watchlist_symbols_ = syms;
    const QStringList eff = effective_symbols(); // active list ∪ open positions
    if (watchlist_)
        watchlist_->set_symbols(eff);
    refresh_row_info();
    {
        QVector<QPair<QString, QPair<QString, QString>>> rows;
        if (r.is_ok())
            for (const auto& s : r.value()) {
                // The screen symbol the row was mapped to above.
                for (auto it = stored_symbol_of_.cbegin(); it != stored_symbol_of_.cend(); ++it)
                    if (it.value() == s.symbol) {
                        rows.append({it.key(), {s.symbol, s.exchange}});
                        break;
                    }
            }
        refine_stored_rows(rows);
    }

    if (!resubscribe)
        return;

    auto* stream = trading::DataStreamManager::instance().stream_for(focused_account_id_);
    if (stream) {
        stream->subscribe_symbols(QStringLiteral("equity:watchlist"), stream_symbols());
        if (!public_mode_)
            stream->fetch_orderbook(selected_symbol_);
    }
    // Public fallback: quote the new symbol set from Yahoo instead.
    if (public_mode_)
        push_public_targets();

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
            opt_quote_conn_ = connect(stream, &trading::AccountDataStream::quote_updated, this,
                                      &EquityTradingScreen::route_option_quote);
        LOG_DEBUG(WL_TAG, QString("opt route bind: has_option=%1 stream=%2 conn=%3 pos=[%4]")
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
    load_watchlists(); // refresh combo, selects the new (empty) list
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

void EquityTradingScreen::on_watchlist_instrument_added(const fincept::core::instrument::Instrument& picked) {
    const auto inst = equity::eq_complete(picked);
    if (inst.symbol.isEmpty()) {
        order_entry_->show_status_message(
            tr("%1 has no public market data, so it cannot be watched here.").arg(inst.display), false);
        return;
    }
    // Stored by its canonical symbol (the Yahoo-form key every market:* topic uses)
    // with its venue; the Watchlist tab reads the same rows.
    for (auto it = stored_symbol_of_.cbegin(); it != stored_symbol_of_.cend(); ++it) {
        if (it.value() == inst.symbol || it.key() == inst.symbol) {
            on_symbol_selected(it.key()); // already listed: just show it
            return;
        }
    }
    if (active_watchlist_id_.isEmpty()) {
        const QString screen = canonical_mode() ? inst.symbol : inst.symbol;
        instruments_.insert(screen, inst);
        if (!watchlist_symbols_.contains(screen))
            watchlist_symbols_ << screen;
        if (watchlist_)
            watchlist_->set_symbols(effective_symbols());
        refresh_row_info();
        if (public_mode_)
            push_public_targets();
        return;
    }
    fincept::WatchlistRepository::instance().add_stock(active_watchlist_id_, inst.symbol, inst.name, inst.venue);
    apply_active_watchlist(true);
}

void EquityTradingScreen::on_watchlist_symbol_added(const QString& symbol) {
    QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty())
        return;
    if (canonical_mode()) {
        // Paper account: the row is the canonical instrument (chart "add", raw text).
        const auto inst = equity::eq_instrument_for_stored(sym, exchange_for_symbol(sym));
        if (inst.symbol.isEmpty()) {
            order_entry_->show_status_message(tr("%1 has no public market data").arg(sym), false);
            return;
        }
        on_watchlist_instrument_added(inst);
        return;
    }
    // Broker account, raw text (F&O contracts, broker-only codes): validate against
    // every connected broker's instrument master, as the old add box did.
    {
        const QStringList ids = search_broker_ids();
        bool any_loaded = false;
        for (const auto& id : ids)
            any_loaded = any_loaded || trading::InstrumentService::instance().is_loaded(id);
        if (any_loaded && sym != selected_symbol_) {
            const auto results = trading::InstrumentService::instance().search_all(sym, "", ids, 1);
            if (results.isEmpty()) {
                order_entry_->show_status_message(tr("%1 was not found in the broker's instrument list").arg(sym),
                                                  false);
                return;
            }
            sym = results.first().symbol;
        }
    }
    if (active_watchlist_id_.isEmpty()) {
        // No named list is loaded (lists load with a focused broker account), e.g.
        // a new user on public data: keep the row for this session and quote it,
        // instead of showing a row that never gets a price.
        if (!watchlist_symbols_.contains(sym))
            watchlist_symbols_ << sym;
        if (watchlist_)
            watchlist_->set_symbols(effective_symbols()); // also covers the chart's "add to watchlist"
        if (public_mode_)
            push_public_targets();
        return;
    }
    fincept::WatchlistRepository::instance().add_stock(active_watchlist_id_, sym, {}, selected_exchange_);
    apply_active_watchlist(true);
}

void EquityTradingScreen::on_watchlist_symbol_removed(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    if (active_watchlist_id_.isEmpty()) {
        // Session-only list (see on_watchlist_symbol_added).
        watchlist_symbols_.removeAll(symbol);
        if (watchlist_)
            watchlist_->set_symbols(effective_symbols());
        if (public_mode_)
            push_public_targets();
        return;
    }
    // The row as it is stored (a broker-style row migrated on screen keeps its old key).
    fincept::WatchlistRepository::instance().remove_stock(active_watchlist_id_, stored_symbol_of_.value(symbol, symbol));
    apply_active_watchlist(true);
}

void EquityTradingScreen::refine_stored_rows(const QVector<QPair<QString, QPair<QString, QString>>>& rows) {
    const quint64 gen = ++stored_resolve_gen_;
    rename_notes_.clear();
    // On the paper account the notice slot is this note's alone (a broker account uses it
    // for its "no live quotes" state, which must not be wiped here).
    if (watchlist_ && canonical_mode())
        watchlist_->set_feed_notice(QString(), QString());
#if FINCEPT_EQ_SHARED_SYMBOL_BOX
    auto& resolver = fincept::services::InstrumentResolver::instance();
    for (const auto& row : rows) {
        const QString screen = row.first;
        const QString stored = row.second.first;
        const QString exch = row.second.second;
        if (stored.isEmpty())
            continue;
        resolver.resolve_stored(stored, exch, this,
                                [this, gen, screen, stored](const fincept::services::ResolveResult& res) {
                                    if (gen != stored_resolve_gen_ || res.matches.isEmpty())
                                        return;
                                    const auto& found = res.matches.first();
                                    // The stored instrument itself: richer fields (name, currency).
                                    if (!found.symbol.isEmpty() && found.symbol == instrument_of(screen).symbol) {
                                        auto inst = instruments_.value(screen);
                                        if (inst.name.isEmpty())
                                            inst.name = found.name;
                                        if (inst.currency.isEmpty())
                                            inst.currency = found.currency;
                                        if (inst.display.isEmpty())
                                            inst.display = found.display;
                                        if (inst.symbol.isEmpty())
                                            inst.symbol = found.symbol;
                                        instruments_.insert(screen, equity::eq_complete(inst));
                                        schedule_row_info();
                                    }
                                    // Renamed: say so; the user decides (remove + add the successor).
                                    if (res.matches.size() > 1 && !res.note.isEmpty() &&
                                        !rename_notes_.contains(res.note)) {
                                        rename_notes_ << res.note;
                                        if (watchlist_)
                                            watchlist_->set_feed_notice(
                                                tr("RENAMED SYMBOLS"),
                                                tr("%1. Nothing was changed: add the new symbol and remove the old row "
                                                   "to switch.")
                                                    .arg(rename_notes_.join(QStringLiteral("; "))));
                                    }
                                    Q_UNUSED(stored);
                                });
    }
#else
    Q_UNUSED(rows);
#endif
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
    LOG_DEBUG(WL_TAG, QString("update_position_symbols: [%1]").arg(deduped.join(',')));
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
    LOG_DEBUG(WL_TAG, QString("update_holding_symbols: [%1]").arg(deduped.join(',')));
    // Same path as positions: re-subscribe the active∪positions∪holdings union so
    // holding LTP streams live instead of waiting on the 5-minute REST poll.
    // Deferred because this fires from inside the holdings hub callback.
    QMetaObject::invokeMethod(this, [this]() { apply_active_watchlist(true); }, Qt::QueuedConnection);
}

} // namespace fincept::screens
