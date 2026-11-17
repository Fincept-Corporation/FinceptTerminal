// src/screens/crypto_trading/CryptoTradingScreen_Handlers.cpp
//
// User-action handlers: exchange/symbol switching, mode toggle, API config,
// order submit/cancel, order-book click, search.
//
// Part of the partial-class split of CryptoTradingScreen.cpp.
//
// LIVE orders (and cancels, closes, leverage / margin changes) never go through
// ExchangeService's process-wide "current exchange": the exchange, the account
// (credentials) and the full order are captured when the user acts, and every
// request names that session. An order is two daemon calls: prepare_order
// decides everything that would be sent (exact market, size in the exchange's
// own unit, price, params, client order id) without sending; the user confirms
// THAT; place_order rebuilds it and refuses if it differs (confirm_hash) or the
// account changed (account_id). The reply is read and classified accepted /
// rejected / unknown — an unknown outcome is looked up by its client order id,
// never re-sent.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoChart.h"
#include "screens/crypto_trading/CryptoCredentials.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoTradingScreen.h"
#include "screens/crypto_trading/CryptoWatchlist.h"
#include "trading/ExchangeDaemonPool.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderMatcher.h"
#include "trading/OrderSourceCapture.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QAbstractButton>
#include <QCompleter>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <optional>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;

static const QString TAG = "CryptoTrading";

namespace {
// Budgets (ms) the app waits for the daemon. place_order is only sent while
// at least 11 s of its budget is left (10 s HTTP timeout for the order POST).
constexpr int kPrepareTimeoutMs = 45000; // may load a cold market list first
constexpr int kPlaceTimeoutMs = 30000;
constexpr int kAccountCallTimeoutMs = 20000;
constexpr qint64 kReuseClientIdMs = 10 * 60 * 1000;

// Closing-order side for a LIVE position as the exchange daemon reports it (the raw
// ccxt unified position). ccxt gives `contracts` as an ABSOLUTE size and the
// direction in `side` ("long" / "short"); a few venues leave `side` null and sign the
// size instead. Returns "sell" (close a long), "buy" (cover a short), or an empty
// string when the direction cannot be determined — the caller must then refuse to
// close rather than guess: a wrong guess sends a same-direction order that is either
// rejected or, where reduceOnly is not honoured, ADDS to the position.
QString live_close_side(const QJsonObject& p, double contracts) {
    const QString side = p.value("side").toString().trimmed().toLower();
    if (side == QLatin1String("long") || side == QLatin1String("buy"))
        return contracts < 0 ? QString() : QStringLiteral("sell"); // a negative size contradicts "long"
    if (side == QLatin1String("short") || side == QLatin1String("sell"))
        return QStringLiteral("buy");
    if (contracts < 0)
        return QStringLiteral("buy"); // no side, signed size: negative is unambiguously short
    return {};                        // no side and an unsigned size — direction unknown
}

QString new_client_token() {
    return QUuid::createUuid().toString(QUuid::Id128);
}

// Identity of a ticket (everything but its client id), for reusing the client
// order id when the same ticket is pressed again after an unknown outcome.
QString ticket_key(const QString& exchange, QJsonObject args) {
    args.remove(QStringLiteral("client_order_token"));
    return exchange + QLatin1Char('|') + QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));
}

QString num_text(double v) {
    return QString::number(v, 'g', 12);
}

// Contract symbols ("BTC/USDC:USDC") with the same rule as PaperMarkService::
// prepare_symbol_fx / prepare_close_fx: a real rate, or (closing only) the factor
// this run already took from one; otherwise nothing is booked. (fmt::
// instrument_for_symbol reads the ":SETTLE" form as the quote asset too since the
// currency pass — this screen-level path predates that and gives the same answer.)
bool cth_is_contract_symbol(const QString& symbol) {
    return symbol.contains(QLatin1Char(':'));
}

bool cth_prepare_contract_fx(const QString& portfolio_id, const QString& symbol, const QString& account_ccy,
                             bool closing, QString* why) {
    const QString px_ccy = pair_quote_ccy(symbol);
    const QString acct = account_ccy.trimmed().toUpper();
    if (portfolio_id.isEmpty() || px_ccy.isEmpty() || acct.isEmpty() || px_ccy == acct)
        return true;
    if (const auto f = PaperMarkService::fx_factor(px_ccy, acct)) {
        pt_set_symbol_fx(portfolio_id, symbol, *f);
        return true;
    }
    if (closing && pt_has_symbol_fx(portfolio_id, symbol))
        return true; // the factor the open was booked at, taken from a real rate this run
    if (why)
        *why = closing ? QCoreApplication::translate("CryptoTrading",
                                                     "%3 was not closed: it is priced in %1 and this paper account "
                                                     "books cash in %2, and the %1 to %2 exchange rate has not "
                                                     "arrived yet (it is being fetched). Nothing was booked — try "
                                                     "again in a few seconds.")
                             .arg(px_ccy, acct, symbol)
                       : QCoreApplication::translate("CryptoTrading",
                                                     "The %1 to %2 exchange rate has not arrived yet (it is being "
                                                     "fetched). %3 is priced in %1 and this paper account books cash "
                                                     "in %2, so the order was not placed. Try again in a few seconds.")
                             .arg(px_ccy, acct, symbol);
    return false;
}

// The market descriptor of a prepare_order reply, as a ResolvedMarket (for describe()).
ResolvedMarket market_of(const QJsonObject& prepared) {
    const QJsonObject m = prepared.value("market").toObject();
    ResolvedMarket r;
    r.symbol = m.value("symbol").toString();
    r.kind = m.value("kind").toString();
    r.base = m.value("base").toString();
    r.quote = m.value("quote").toString();
    r.settle = m.value("settle").toString();
    r.linear = m.value("linear").toBool(false);
    r.inverse = m.value("inverse").toBool(false);
    r.contract_size = m.value("contract_size").toDouble();
    return r;
}

// One line naming a prepared order ("SELL 5 contracts of BTC/USDT:USDT (= 0.05 BTC), market, reduce-only").
QString prepared_line(const QJsonObject& p) {
    const ResolvedMarket m = market_of(p);
    QString size = p.value("amount_unit").toString() == QLatin1String("contracts")
                       ? QCoreApplication::translate("CryptoTrading", "%1 contracts (= %2 %3)")
                             .arg(p.value("amount_text").toString(), num_text(p.value("base_amount").toDouble()),
                                  p.value("base_ccy").toString())
                       : QStringLiteral("%1 %2").arg(p.value("amount_text").toString(), p.value("base_ccy").toString());
    QString line = QStringLiteral("%1 %2 %3 — %4")
                       .arg(p.value("side").toString().toUpper(), size, m.symbol,
                            p.value("type").toString().toUpper().replace(QLatin1Char('_'), QLatin1Char('-')));
    if (p.value("reduce_only").toBool(false))
        line += QCoreApplication::translate("CryptoTrading", ", reduce-only");
    if (!p.value("market_bound").toString().isEmpty())
        line += QCoreApplication::translate("CryptoTrading", ", fills only up to %1").arg(p.value("market_bound").toString());
    return line;
}
} // namespace

void CryptoTradingScreen::on_exchange_changed(const QString& exchange) {
    if (exchange == exchange_id_)
        return;
    // LIVE was acknowledged for ONE venue (the arming dialog names it). Carrying it
    // over to another exchange let the leverage / margin controls and the ticket act
    // on the new venue's real account without that acknowledgement. Drop to PAPER
    // before switching; LIVE has to be re-armed (and re-confirmed) for the new venue.
    if (trading_mode_ == TradingMode::Live) {
        mode_btn_->setChecked(false);
        on_mode_toggled();
    }
    LOG_DEBUG(TAG, QString("Exchange changed: %1 → %2").arg(exchange_id_, exchange));
    exchange_id_ = exchange;
    exchange_btn_->setText(exchange.toUpper());
    if (ws_transport_) {
        ws_transport_->setText(tr("DAEMON"));
        ws_transport_->setToolTip(tr("ws_stream.py via ccxt.pro — Python subprocess"));
    }

    // Phase 3: sessions stay warm for the app lifetime. Do NOT stop the old
    // exchange's WS — `ExchangeSessionManager` keeps it running so switching
    // back is instant. Dropping our hub subscriptions is enough to stop the
    // old exchange's ticks landing in our pending_* buffers; the session
    // keeps updating its cache and publishing to the hub in the background.
    hub_unsubscribe_topics();

    // Clear accumulated buffers — stale data from the old exchange is useless.
    pending_tickers_.clear();
    pending_orderbook_ = {};
    has_pending_orderbook_ = false;
    pending_primary_ticker_ = {};
    has_pending_primary_ = false;
    pending_candles_.clear();
    pending_trades_.clear();
    last_ws_state_ = -1; // re-evaluate feed mode after new stream connects

    // The previous venue's book / tape / candles are meaningless on the new one.
    market_info_cache_ = {};
    reset_market_panes(); // chart + book skeletons, captioned with the new venue
    orderbook_->clear();
    bottom_panel_->reset_market_data();
    chart_->clear();
    chart_symbol_.clear();
    // And so is its resolved market: "checking" until the new venue answers.
    market_ = ResolvedMarket{};
    market_.exchange = exchange_id_;
    market_.requested = selected_symbol_;
    order_entry_->set_market(market_);
    order_entry_->set_venue(exchange_id_);
    bottom_panel_->set_scope_market(QString());
    apply_price_market();

    // Reset fetch guards — a prior in-flight fetch from the old exchange must
    // not suppress the first fetch against the new exchange.
    candles_fetching_.store(false);
    live_inflight_.store(0);

    // Re-initialize — attaches this screen to the new exchange's session (and
    // points the paper price feed at it). If that session's WS is already warm
    // (we visited it earlier), init_exchange skips the start, so there's no
    // second handshake.
    initialized_ = false;
    init_exchange();

    load_portfolio();

    // Chart history (left of "now") still needs one REST fetch — Kraken WS
    // only streams the current bar. Everything else (ticker, orderbook,
    // watchlist) repopulates from the new session's WS subscriptions within
    // ~1s of the handshake. Funding/OI is perp-only and stays on its 30s
    // market_info_timer_ — those endpoints aren't on the public WS feed.
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    refresh_market_info();
    async_seed_trade_tape();     // the tape stays empty until the first streamed print otherwise
    async_fetch_market_limits(); // the resolved market, its minimums and what may be sent there
    update_futures_visibility(); // leverage/margin/reduce-only depend on perp-ness
    if (bottom_panel_->fee_view_visible())
        maybe_fetch_fees(false); // fees are per-exchange; FEES / MARKET is on screen

    ScreenStateManager::instance().notify_changed(this);
}

void CryptoTradingScreen::on_symbol_selected(const QString& typed) {
    QString symbol = typed.trimmed();
    if (symbol.isEmpty())
        return;
    // A unified pair always has a "/" ("BTC/USDT"). A bare ticker typed into the box
    // ("BTC") used to be subscribed as-is — a topic that never ticks — leaving the
    // whole screen waiting on a symbol that doesn't exist. Resolve it against the
    // watchlist's pairs; if nothing matches, put the current pair back instead.
    if (!symbol.contains(QLatin1Char('/'))) {
        QString match;
        for (const QString& s : watchlist_symbols_) {
            if (s.startsWith(symbol + QLatin1Char('/'), Qt::CaseInsensitive)) {
                match = s;
                break;
            }
        }
        if (match.isEmpty()) {
            LOG_WARN(TAG, QString("Ignoring symbol '%1' — expected BASE/QUOTE").arg(symbol));
            symbol_input_->setText(selected_symbol_);
            return;
        }
        symbol = match;
    }
    if (symbol == selected_symbol_) {
        symbol_input_->setText(selected_symbol_); // normalise a resolved/re-typed entry
        return;
    }
    switch_symbol(symbol);
    ScreenStateManager::instance().notify_changed(this);

    // Phase 7: publish to the linked group so other panels in the same
    // group switch to this pair. Source = `this` so on_group_symbol_changed
    // can suppress its own re-publish below.
    if (link_group_ != SymbolGroup::None) {
        SymbolRef ref;
        ref.symbol = symbol;
        ref.asset_class = QStringLiteral("crypto");
        ref.exchange = exchange_id_;
        SymbolContext::instance().set_group_symbol(link_group_, ref, this);
    }
}

void CryptoTradingScreen::on_market_choice(const QString& symbol) {
    // The ticket's "TRADE <market> INSTEAD": the user explicitly picks the
    // market the pair was shown from (or an alternative) as the pair to trade.
    if (!symbol.trimmed().isEmpty())
        on_symbol_selected(symbol.trimmed());
}

void CryptoTradingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    // Phase 7: subscribe-side. Only react to crypto symbols — an inbound
    // equity ticker has nothing meaningful to do here. Empty asset_class
    // is treated as "unknown" and ignored to avoid surprising the user
    // with a stale cross-asset propagation.
    if (!ref.is_valid())
        return;
    if (ref.asset_class != QStringLiteral("crypto"))
        return;
    if (ref.symbol == selected_symbol_)
        return; // already showing this pair
    // Reuse the existing publish-suppressing path: switch_symbol mutates
    // selected_symbol_ but doesn't re-emit because we don't go through
    // on_symbol_selected. A LIVE order already being confirmed is unaffected:
    // it carries its own snapshot and is refused if the pair changed.
    switch_symbol(ref.symbol);
}

SymbolRef CryptoTradingScreen::current_symbol() const {
    if (selected_symbol_.isEmpty())
        return {};
    SymbolRef r;
    r.symbol = selected_symbol_;
    r.asset_class = QStringLiteral("crypto");
    r.exchange = exchange_id_;
    return r;
}

void CryptoTradingScreen::apply_price_market() {
    // The resolved market only when it is the one for the pair and venue on
    // screen (market_ lags a switch until async_fetch_market_limits() answers).
    const bool resolved =
        market_.valid() && market_.requested == selected_symbol_ && market_.exchange == exchange_id_;
    const QString pm = resolved ? market_.symbol : selected_symbol_;
    if (ticker_bar_)
        ticker_bar_->set_price_market(pm);
    if (orderbook_)
        orderbook_->set_price_market(pm);
    if (chart_)
        chart_->set_price_market(pm);
    if (bottom_panel_)
        bottom_panel_->set_price_market(pm);
}

void CryptoTradingScreen::switch_symbol(const QString& symbol) {
    LOG_DEBUG(TAG, QString("Symbol changed: %1 → %2").arg(selected_symbol_, symbol));
    auto& es = ExchangeService::instance(); // paper price-feed bookkeeping only
    es.unwatch_symbol(selected_symbol_, portfolio_id_);
    selected_symbol_ = symbol;
    symbol_input_->setText(symbol);
    ticker_bar_->set_symbol(symbol);
    order_entry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);
    apply_price_market(); // the pair's quote asset until its market resolves

    // Drop per-symbol buffers tied to the old symbol to prevent cross-symbol leakage.
    has_pending_primary_ = false;
    pending_primary_ticker_ = {};
    has_pending_orderbook_ = false;
    pending_orderbook_ = {};
    pending_candles_.clear();
    pending_trades_.clear();
    market_info_cache_ = {};

    // Drop what is on screen for the old pair too — the book (whose levels are
    // clickable), the tape, the depth chart, the MARKET-tab readouts and the
    // candles — instead of mixing them with the new pair's first updates.
    reset_market_panes(); // chart + book skeletons, captioned with the new pair
    orderbook_->clear();
    bottom_panel_->reset_market_data();
    chart_->clear();
    chart_symbol_.clear();

    es.watch_symbol(selected_symbol_, portfolio_id_);
    session()->set_ws_primary_symbol(symbol);

    // Three of the five subscriptions (ticker/orderbook/trades + ohlc
    // pattern) are primary-symbol-specific and need to be re-bound to the
    // new symbol. The watchlist ticker pattern doesn't change. Simpler to
    // resubscribe wholesale than diff.
    hub_subscribe_topics();

    // WS-only mode: ticker + orderbook reflow naturally as the new symbol's
    // WS subscriptions kick in. Only history needs a REST fetch.
    async_fetch_candles(selected_symbol_, chart_->current_timeframe());
    refresh_market_info(); // funding / OI / mark are per-symbol and were only polled every 30 s
    async_seed_trade_tape();
    async_fetch_market_limits();
    update_futures_visibility(); // a perp↔spot symbol switch toggles the controls
    if (bottom_panel_->fee_view_visible())
        maybe_fetch_fees(false); // the new pair's fee schedule, FEES / MARKET being on screen
}

void CryptoTradingScreen::on_mode_toggled() {
    const bool is_live = mode_btn_->isChecked();

    // Arming LIVE turns every subsequent click on the ticket into real money
    // on a real exchange. It used to happen silently on one toggle click, with
    // only a colour change on a 22 px button to signal it. Require an explicit
    // acknowledgement; on decline, snap the toggle back rather than leaving
    // the button and the mode out of sync.
    if (is_live && trading_mode_ != TradingMode::Live) {
        const QString account = session() ? session()->account_label() : tr("no API key");
        const auto answer =
            QMessageBox::warning(this, tr("Switch to LIVE trading"),
                                 tr("LIVE mode places real orders on %1 with real funds (account: %2).\n\n"
                                    "Orders are irreversible once filled. Make sure the API key configured "
                                    "for this exchange is the account you intend to trade.\n\nSwitch to LIVE?")
                                     .arg(exchange_id_.toUpper(), account),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            mode_btn_->setChecked(false);
            return;
        }
    }

    const bool mode_changed = (trading_mode_ == TradingMode::Live) != is_live;
    trading_mode_ = is_live ? TradingMode::Live : TradingMode::Paper;
    mode_btn_->setText(is_live ? tr("LIVE") : tr("PAPER"));
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    order_entry_->set_mode(!is_live);
    bottom_panel_->set_mode(!is_live);
    // Positions / orders / history / stats on screen belong to the mode just left —
    // paper rows used to sit next to the LIVE balance until (or, on a failed fetch,
    // instead of) the first live reply. Blank them; the refresh below refills them.
    if (mode_changed)
        bottom_panel_->clear_account_views();
    push_key_state(); // what the LIVE views say about this exchange's keys

    if (is_live) {
        live_data_timer_->start(5000);
        warm_live_session(); // authenticated client + market list ready before the first order
        refresh_live_data();
        maybe_fetch_fees(true); // the account's own fee tier, once on entering live
        async_fetch_leverage_info();
    } else {
        live_data_timer_->stop();
        refresh_portfolio();
    }
}

void CryptoTradingScreen::on_api_clicked() {
    auto* dlg = new CryptoCredentials(exchange_id_, this);
    // Show what is already stored (the session rehydrates it from SecureStorage at
    // start-up). The dialog always opened blank, so editing one field — e.g. just the
    // passphrase — meant re-typing everything, and there was no way to tell whether
    // keys were saved at all. set_values() existed for exactly this and had no caller.
    if (auto* sess = session()) {
        const ExchangeCredentials stored = sess->get_credentials();
        dlg->set_values(stored.api_key, stored.secret, stored.password, stored.wallet_address, stored.private_key);
    }
    // The keys belong to the exchange this dialog was opened for, even if the
    // screen switches meanwhile (they used to land on the global "current" one).
    const QString exchange = exchange_id_;
    connect(dlg, &CryptoCredentials::credentials_saved, this,
            [exchange](const QString& key, const QString& secret, const QString& pw, const QString& wallet,
                       const QString& pk) {
                ExchangeCredentials creds;
                creds.api_key = key;
                creds.secret = secret;
                creds.password = pw;
                creds.wallet_address = wallet;
                creds.private_key = pk;
                ExchangeSessionManager::instance().session(exchange)->set_credentials(creds);
                const bool cleared =
                    key.isEmpty() && secret.isEmpty() && pw.isEmpty() && wallet.isEmpty() && pk.isEmpty();
                LOG_INFO(TAG, (cleared ? "Credentials cleared for " : "Credentials saved for ") + exchange);
            });
    dlg->exec();
    dlg->deleteLater();
    // Saved, cleared or unchanged: the LIVE views and MY TRADES follow the key state.
    push_key_state();
    if (trading_mode_ == TradingMode::Live)
        refresh_live_data();
}

// ── LIVE context ─────────────────────────────────────────────────────────────

CryptoTradingScreen::LiveContext CryptoTradingScreen::live_context() const {
    LiveContext ctx;
    ctx.exchange = exchange_id_;
    ctx.selected_symbol = selected_symbol_;
    if (auto* s = session()) {
        ctx.creds = s->get_credentials();
        ctx.account_id = ExchangeDaemonPool::credential_fingerprint(ctx.creds);
        ctx.account_label = s->account_label();
    }
    return ctx;
}

bool CryptoTradingScreen::live_context_current(const LiveContext& ctx, QString* why) const {
    QString reason;
    if (trading_mode_ != TradingMode::Live)
        reason = tr("the screen is back in PAPER mode");
    else if (exchange_id_ != ctx.exchange)
        reason = tr("the exchange changed from %1 to %2").arg(ctx.exchange.toUpper(), exchange_id_.toUpper());
    else if (selected_symbol_ != ctx.selected_symbol)
        reason = tr("the pair changed from %1 to %2").arg(ctx.selected_symbol, selected_symbol_);
    else if (!session() || session()->account_id() != ctx.account_id)
        reason = tr("the API key for %1 changed").arg(ctx.exchange.toUpper());
    if (why)
        *why = reason;
    return reason.isEmpty();
}

void CryptoTradingScreen::warm_live_session() {
    // Build the authenticated client and load its market list now, so no order
    // waits on a 20 s market load inside its own time budget.
    QPointer<CryptoTradingScreen> self = this;
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, ex]() {
        const QJsonObject r = ExchangeSessionManager::instance().session(ex)->request("warm", {}, 60000);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, ex, r]() {
                if (!self || self->exchange_id_ != ex || self->trading_mode_ != TradingMode::Live)
                    return;
                if (r.contains("error")) {
                    LOG_WARN(TAG, QString("LIVE warm-up on %1 failed: %2").arg(ex, r.value("error").toString()));
                    self->order_entry_->show_notice(tr("LIVE on %1: %2").arg(ex.toUpper(), r.value("error").toString()),
                                                    QStringLiteral("warning"));
                } else if (r.value("markets_stale").toBool(false)) {
                    self->order_entry_->show_notice(tr("LIVE on %1: the market list could not be refreshed; orders "
                                                       "are refused until it can be.")
                                                        .arg(ex.toUpper()),
                                                    QStringLiteral("warning"));
                }
            },
            Qt::QueuedConnection);
    });
}

// ── Orders ───────────────────────────────────────────────────────────────────

void CryptoTradingScreen::on_order_submitted(const QString& side, const QString& order_type, double qty, double price,
                                             double stop_price, double sl, double tp) {
    // The ticket press. The order events themselves (submitted / accepted /
    // rejected / unknown) go to the audit log where the order is actually placed.
    LOG_INFO(TAG, fincept::logf("Order ticket submitted",
                                {{"venue", exchange_id_},
                                 {"mode", trading_mode_ == TradingMode::Paper ? "paper" : "live"},
                                 {"symbol", selected_symbol_},
                                 {"market", market_.symbol},
                                 {"side", side},
                                 {"type", order_type},
                                 {"qty", qty},
                                 {"price", price},
                                 {"stop", stop_price},
                                 {"sl", sl},
                                 {"tp", tp}}));
    try {
        if (trading_mode_ == TradingMode::Paper) {
            // Who placed it (audit "source"): the agent's while an agent pressed the ticket.
            const QString order_source = capture_order_source(QStringLiteral("ticket"));
            // A refusal here happens before the paper engine (which audits what it
            // decides), so it is audited here. Plain English; the portfolio id masked.
            const auto audit_ticket_refusal = [&](const QString& reason) {
                LOG_AUDIT("Orders", "Order rejected",
                          {{"venue", "paper"},
                           {"mode", "paper"},
                           {"portfolio", fincept::log_mask(portfolio_id_)},
                           {"exchange", exchange_id_},
                           {"symbol", selected_symbol_},
                           {"side", side},
                           {"type", order_type},
                           {"qty", qty},
                           {"reason", reason},
                           {"source", order_source}});
            };
            // The paper portfolio loads asynchronously; before it has, pt_place_order
            // would throw on an empty id and the click did nothing visible.
            if (portfolio_id_.isEmpty()) {
                audit_ticket_refusal(QStringLiteral("the paper portfolio has not loaded yet"));
                QMessageBox::warning(this, tr("Order Rejected"),
                                     tr("Paper order rejected:\nThe paper portfolio has not loaded yet. "
                                        "Try again in a moment."));
                return;
            }
            auto ticker = session()->get_cached_price(selected_symbol_);
            // A paper market order fills immediately at the live price. Before the first
            // tick (cold start / just-switched exchange) there is none — refuse rather than
            // fill at a bogus $1000, which would corrupt paper cost-basis and P&L.
            if (order_type == "market" && !(ticker.last > 0)) {
                audit_ticket_refusal(QStringLiteral("no live price for a market fill"));
                QMessageBox::warning(this, tr("Price Unavailable"),
                                     tr("Live price for %1 is not available yet. Wait for the first tick, "
                                        "or place a limit order.")
                                         .arg(selected_symbol_));
                return;
            }
            std::optional<double> price_opt;
            if (order_type == "market")
                price_opt = ticker.last;
            else if (price > 0)
                price_opt = price;

            std::optional<double> stop_opt;
            if (stop_price > 0)
                stop_opt = stop_price;

            // The paper account books cash (margin, fee, realized P&L) in its own
            // currency; the pair is priced in its quote asset (USDT, EUR, BTC…).
            // A pair shown here from a market quoted in another asset ("UNI/USDT"
            // shown from "UNI/USD") would be booked at the wrong rate: refuse it,
            // as LIVE does, and name the market to choose instead.
            const QString pair_quote = crypto::pair_quote_ccy(selected_symbol_);
            if (market_.valid() && market_.symbol != selected_symbol_ && !market_.quote.isEmpty() &&
                market_.quote.compare(pair_quote, Qt::CaseInsensitive) != 0) {
                audit_ticket_refusal(QStringLiteral("the market shown is priced in another quote currency"));
                order_entry_->show_order_refusal(
                    tr("⚠ Not placed: %1 is shown here from %2, which is priced in %3, not %4. Choose %2 to trade it.")
                        .arg(selected_symbol_, market_.symbol, market_.quote, pair_quote));
                return;
            }
            // Set the pair's FX factor from a real rate before any cash is booked
            // (never 1:1). No rate yet: nothing is placed and the ticket says why.
            QString fx_why;
            const bool fx_ready =
                cth_is_contract_symbol(selected_symbol_)
                    ? cth_prepare_contract_fx(portfolio_id_, selected_symbol_, portfolio_.currency, false, &fx_why)
                    : PaperMarkService::prepare_symbol_fx(portfolio_id_, selected_symbol_, QStringLiteral("CRYPTO"),
                                                          &fx_why);
            if (!fx_ready) {
                LOG_WARN(TAG, QString("Paper order not placed (FX): %1").arg(fx_why));
                audit_ticket_refusal(QStringLiteral("no exchange rate for the pair's quote currency yet"));
                order_entry_->show_order_refusal(tr("⚠ Not placed: %1").arg(fx_why));
                return;
            }

            auto order = pt_place_order(portfolio_id_, selected_symbol_, side, order_type, qty, price_opt, stop_opt,
                                        /*reduce_only=*/false, QString(), QString(), order_source);
            LOG_AUDIT("Orders", "Order submitted",
                      {{"venue", exchange_id_},
                       {"mode", "paper"},
                       {"symbol", selected_symbol_},
                       {"side", side},
                       {"type", order_type},
                       {"qty", qty},
                       {"price", price_opt ? *price_opt : 0.0},
                       {"stop", stop_price},
                       {"order_id", order.id},
                       {"source", order_source}});
            if (order_type == "market") {
                pt_fill_order(order.id, ticker.last);
                LOG_AUDIT("Orders", "Order accepted",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"symbol", selected_symbol_},
                           {"order_id", order.id},
                           {"status", "filled"},
                           {"fill_price", ticker.last}});
                // A market order fills immediately, so attach any SL/TP bracket to the
                // resulting position here. The limit/stop branch does this for resting
                // orders; skipping it left market entries silently unprotected.
                if (sl > 0 || tp > 0)
                    OrderMatcher::instance().set_sl_tp(portfolio_id_, selected_symbol_, order.id, sl, tp);
            } else {
                OrderMatcher::instance().add_order(order);
                if (sl > 0 || tp > 0)
                    OrderMatcher::instance().set_sl_tp(portfolio_id_, selected_symbol_, order.id, sl, tp);
                LOG_AUDIT("Orders", "Order accepted",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"symbol", selected_symbol_},
                           {"order_id", order.id},
                           {"status", "open"}});
            }
            refresh_portfolio();
            // Confirm inline (a paper fill used to give no feedback at all) and clear
            // the ticket so its quantity / prices / SL / TP can't ride along on the next order.
            order_entry_->reset_after_fill(
                order_type == "market"
                    ? tr("Paper %1 %2 %3 filled at %4")
                          .arg(side.toUpper(), crypto::format_size(qty), selected_symbol_,
                               crypto::pair_price_with_unit(ticker.last, selected_symbol_))
                    : tr("Paper %1 %2 order placed: %3 %4")
                          .arg(side.toUpper(), order_type.toUpper(), crypto::format_size(qty), selected_symbol_));
            return;
        }

        // ── LIVE ────────────────────────────────────────────────────────────
        // The ticket already refused anything not provably right (unknown /
        // substituted market, unsupported stop or SL/TP); check the market once
        // more here, then snapshot the WHOLE order before anything is shown.
        if (!market_.valid() || market_.match != QLatin1String("exact") || !market_.tradable) {
            order_entry_->show_order_refusal(tr("⚠ %1 is not an exact market on %2; nothing was sent.")
                                                 .arg(selected_symbol_, exchange_id_.toUpper()));
            return;
        }
        QJsonObject args;
        args["symbol"] = market_.symbol; // the resolved market, never the pair as typed
        args["requested"] = selected_symbol_;
        args["side"] = side;
        args["type"] = order_type;
        args["amount"] = qty; // base coins; the daemon converts to the exchange's unit
        args["amount_unit"] = QStringLiteral("base");
        if (price > 0)
            args["price"] = price;
        if (stop_price > 0)
            args["stop_price"] = stop_price;
        if (sl > 0)
            args["sl"] = sl;
        if (tp > 0)
            args["tp"] = tp;
        if (order_entry_->reduce_only())
            args["reduce_only"] = true;
        if (market_.is_contract()) {
            args["margin_mode"] = order_entry_->margin_mode();
            args["leverage"] = order_entry_->leverage();
        }
        const LiveContext ctx = live_context();
        // The same ticket pressed again shortly after an UNKNOWN outcome carries
        // the same client order id: an exchange that keeps ids unique rejects the
        // duplicate instead of placing a second order.
        const QString key = ticket_key(ctx.exchange, args);
        const bool reuse = key == last_unknown_key_ && !last_unknown_token_.isEmpty() &&
                           QDateTime::currentMSecsSinceEpoch() - last_unknown_at_ < kReuseClientIdMs;
        args["client_order_token"] = reuse ? last_unknown_token_ : new_client_token();
        submit_live_order(ctx, args);
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, QString("Order failed: %1").arg(e.what()));
        if (trading_mode_ == TradingMode::Paper)
            LOG_AUDIT("Orders", "Order rejected",
                      {{"venue", exchange_id_},
                       {"mode", "paper"},
                       {"symbol", selected_symbol_},
                       {"side", side},
                       {"type", order_type},
                       {"qty", qty},
                       {"reason", QString::fromUtf8(e.what())}});
        // pt_place_order / pt_fill_order reject by throwing ("Insufficient …", "Limit
        // order requires price", …). The LIVE branch already shows its failures; the
        // paper one only logged, so a rejected paper order was indistinguishable from
        // a click that did nothing.
        if (trading_mode_ == TradingMode::Paper)
            QMessageBox::warning(this, tr("Order Rejected"),
                                 tr("Paper order rejected:\n%1").arg(QString::fromUtf8(e.what())));
        else
            QMessageBox::warning(this, tr("Order Failed"), tr("Order failed:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void CryptoTradingScreen::submit_live_order(const LiveContext& ctx, const QJsonObject& args) {
    // In-flight guard: the order is checked and sent on workers and the GUI
    // thread returns at once, so the submit button stays disabled until the end
    // — a double-click or held Ctrl+Enter must not start a second order (CR-07).
    order_entry_->set_submit_busy(true, tr("CHECKING…"));
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, args]() {
        QJsonObject prepared;
        try {
            prepared = ExchangeSessionManager::instance().session(ctx.exchange)->request_as(
                "prepare_order", args, ctx.creds, kPrepareTimeoutMs);
        } catch (...) {
            LOG_ERROR(TAG, fincept::logf("prepare_order threw",
                                         {{"exchange", ctx.exchange}, {"symbol", args.value("symbol").toString()}}));
            prepared = QJsonObject{{"error", QStringLiteral("could not check the order")}};
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, ctx, args, prepared]() {
                if (!self)
                    return;
                self->order_entry_->set_submit_busy(false);
                QString why;
                if (!self->live_context_current(ctx, &why)) {
                    self->order_entry_->show_order_refusal(tr("⚠ Nothing was sent: %1.").arg(why));
                    return;
                }
                if (prepared.contains("error")) {
                    QString msg = prepared.value("error").toString();
                    const QJsonArray alts = prepared.value("alternatives").toArray();
                    if (!alts.isEmpty() && !msg.contains(QStringLiteral("Choose one of"))) {
                        QStringList list;
                        for (const auto& a : alts)
                            list << a.toString();
                        msg += QLatin1Char('\n') + tr("Choose one of: %1").arg(list.join(", "));
                    }
                    LOG_WARN(TAG, QString("Order refused before sending (%1): %2").arg(ctx.exchange, msg));
                    self->order_entry_->show_order_refusal(tr("⚠ Not sent: %1").arg(msg));
                    return;
                }
                self->confirm_and_place(ctx, args, prepared);
            },
            Qt::QueuedConnection);
    });
}

QString CryptoTradingScreen::order_confirm_text(const LiveContext& ctx, const QJsonObject& p) const {
    const ResolvedMarket m = market_of(p);
    const QString base = p.value("base_ccy").toString();
    const QString quote = m.quote;
    const QString side = p.value("side").toString().toUpper();
    const QString type = p.value("type").toString().toUpper().replace(QLatin1Char('_'), QLatin1Char('-'));
    QStringList l;
    l << tr("Place this LIVE order with real funds?") << QString();
    l << tr("Exchange       %1   (%2)").arg(ctx.exchange.toUpper(), p.value("account").toString());
    l << tr("Market         %1").arg(m.describe());
    const QString requested = p.value("requested").toString();
    if (!requested.isEmpty() && requested != m.symbol)
        l << tr("               (selected pair: %1)").arg(requested);
    l << tr("Order          %1 %2").arg(side, type);
    if (p.value("qty_unit").toString() == QLatin1String("contracts"))
        l << tr("Quantity       %1 contracts").arg(num_text(p.value("qty").toDouble()));
    else
        l << tr("Quantity       %1 %2").arg(num_text(p.value("qty").toDouble()), base);
    if (m.kind != QLatin1String("spot")) {
        if (m.inverse)
            l << tr("Exchange size  %1 contracts × %2 %3 = %4 %3 (≈ %5 %6)")
                     .arg(p.value("amount_text").toString(), num_text(m.contract_size), quote,
                          num_text(p.value("notional").toDouble()), num_text(p.value("base_amount").toDouble()), base);
        else
            l << tr("Exchange size  %1 contracts × %2 %3 = %4 %3")
                     .arg(p.value("amount_text").toString(), num_text(m.contract_size), base,
                          num_text(p.value("base_amount").toDouble()));
    } else {
        l << tr("Exchange size  %1 %2").arg(p.value("amount_text").toString(), base);
    }
    if (!p.value("price").toString().isEmpty())
        l << tr("Limit price    %1 %2").arg(p.value("price").toString(), quote);
    if (!p.value("stop_price").toString().isEmpty())
        l << tr("Stop trigger   %1 %2 — triggers when the price %3 to it")
                 .arg(p.value("stop_price").toString(), quote,
                      side == QLatin1String("BUY") ? tr("rises") : tr("falls"));
    if (!p.value("sl").toString().isEmpty())
        l << tr("Stop loss      %1 %2 (closes at market when reached)").arg(p.value("sl").toString(), quote);
    if (!p.value("tp").toString().isEmpty())
        l << tr("Take profit    %1 %2 (closes at market when reached)").arg(p.value("tp").toString(), quote);
    // The exchange-precision strings above (price, stop, SL / TP, sizes) are shown
    // exactly as they will be sent; the estimates below go through the formatter,
    // each in its own currency (the notional in the daemon's notional_ccy, prices
    // in the market's quote asset).
    namespace fmt = fincept::ui::formatting;
    const double ref = p.value("ref_price").toDouble();
    l << tr("Notional       ≈ %1   (last price %2)")
             .arg(fmt::money(p.value("notional").toDouble(), p.value("notional_ccy").toString()),
                  crypto::pair_price_with_unit(ref, m.symbol));
    if (!p.value("market_bound").toString().isEmpty())
        l << tr("Market order   fills only up to %1 (%2 from %3); any part that cannot fill there is cancelled")
                 .arg(p.value("market_bound").toString(),
                      fmt::pct(p.value("slippage").toString().toDouble() * 100.0, 2, false),
                      crypto::pair_price_with_unit(ref, m.symbol));
    if (p.value("spend").toDouble() > 0.0)
        l << tr("Market buy     sent as a spend of ≈ %1; the coins received depend on the fill price")
                 .arg(fmt::money(p.value("spend").toDouble(), quote));
    if (!p.value("margin_mode").toString().isEmpty())
        l << tr("Margin mode    %1 (sent with the order)").arg(p.value("margin_mode").toString());
    if (p.value("leverage_sent").toInt() > 0)
        l << tr("Leverage       %1x (sent with the order)").arg(p.value("leverage_sent").toInt());
    if (p.value("reduce_only").toBool(false))
        l << tr("Reduce-only    yes — can only shrink an open position");
    if (!p.value("client_order_id").isNull() && !p.value("client_order_id").toVariant().toString().isEmpty())
        l << tr("Client id      %1").arg(p.value("client_order_id").toVariant().toString());
    const QJsonArray warnings = p.value("warnings").toArray();
    if (!warnings.isEmpty()) {
        l << QString();
        for (const auto& w : warnings)
            l << tr("⚠ %1").arg(w.toString());
    }
    return l.join(QLatin1Char('\n'));
}

void CryptoTradingScreen::confirm_and_place(const LiveContext& ctx, const QJsonObject& args,
                                            const QJsonObject& prepared) {
    // C6: everything shown here comes from the prepared order (the snapshot taken
    // before the dialog), and exactly that is what place_order sends.
    QMessageBox box(QMessageBox::Warning, tr("Confirm Live Order"), order_confirm_text(ctx, prepared),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setDefaultButton(QMessageBox::No);
    box.setEscapeButton(QMessageBox::No);
    if (auto* yes = box.button(QMessageBox::Yes))
        yes->setText(tr("Place order"));
    if (auto* yes = box.button(QMessageBox::Yes))
        yes->setProperty("fincept_agent_risk", "order");
    box.setDetailedText(tr("Exact exchange request (ccxt create_order):\n") +
                        QString::fromUtf8(QJsonDocument(prepared.value("create_order").toObject())
                                              .toJson(QJsonDocument::Indented)));
    if (box.exec() != QMessageBox::Yes) {
        order_entry_->show_notice(tr("Order not sent."), QStringLiteral("info"));
        return;
    }
    // Anything that moved while the dialog was open (a linked symbol group, a
    // state restore, an agent, another window's key change) cancels it.
    QString why;
    if (!live_context_current(ctx, &why) || prepared.value("account_id").toString() != ctx.account_id) {
        if (why.isEmpty())
            why = tr("the API key changed");
        QMessageBox::warning(this, tr("Order Not Sent"), tr("Nothing was sent: %1.").arg(why));
        return;
    }
    QJsonObject place = args;
    place["confirm_hash"] = prepared.value("confirm_hash");
    place["ref_price"] = prepared.value("ref_price");
    place["account_id"] = ctx.account_id;
    order_entry_->set_submit_busy(true, tr("SENDING…"));
    // Who placed it, captured AFTER the confirmation on the GUI thread: an agent that
    // answered the dialog inside its step is the agent; a person who pressed "Place
    // order" is outside any agent step, so the order is the person's ("ticket").
    // Carried to the outcome lines in a local copy of the prepared order (never sent).
    QJsonObject prepared_src = prepared;
    prepared_src[QStringLiteral("_order_source")] = capture_order_source(QStringLiteral("ticket"));
    LOG_AUDIT("Orders", "Order submitted",
              {{"venue", ctx.exchange},
               {"account", fincept::log_mask(ctx.account_id)},
               {"mode", "live"},
               {"symbol", prepared.value("market").toObject().value("symbol").toString()},
               {"side", prepared.value("side").toString()},
               {"type", prepared.value("type").toString()},
               {"qty", prepared.value("amount_text").toString()},
               {"price", prepared.value("price").toString()},
               {"stop", prepared.value("stop_price").toString()},
               {"client_id", prepared.value("client_order_id").toVariant().toString()},
               {"source", prepared_src.value(QStringLiteral("_order_source")).toString()}});
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, args, prepared = prepared_src, place]() {
        QJsonObject reply;
        try {
            reply = ExchangeSessionManager::instance().session(ctx.exchange)->request_as("place_order", place,
                                                                                         ctx.creds, kPlaceTimeoutMs);
        } catch (...) {
            LOG_ERROR(TAG, fincept::logf("place_order threw, outcome unknown",
                                         {{"exchange", ctx.exchange},
                                          {"symbol", place.value("symbol").toString()},
                                          {"side", place.value("side").toString()},
                                          {"qty", place.value("amount").toDouble()}}));
            reply = QJsonObject{{"error", QStringLiteral("unexpected error while sending")},
                                {"code", "UNKNOWN_ERROR"}};
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, ctx, args, prepared, reply]() {
                if (!self)
                    return;
                self->order_entry_->set_submit_busy(false);
                self->report_order_outcome(ctx, args, prepared, reply);
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::report_order_outcome(const LiveContext& ctx, const QJsonObject& args,
                                               const QJsonObject& prepared, const QJsonObject& reply) {
    QString outcome = crypto::account_change_outcome(reply);
    if (outcome == QLatin1String("accepted") && reply.value("exchange").toString() != ctx.exchange) {
        // The daemon echoes the venue; a mismatch should be impossible — treat as unknown.
        LOG_ERROR(TAG, QString("Order reply from '%1' for an order sent to '%2'")
                           .arg(reply.value("exchange").toString(), ctx.exchange));
        outcome = QStringLiteral("unknown");
    }
    const QString symbol = prepared.value("market").toObject().value("symbol").toString();
    const QString side = prepared.value("side").toString().toUpper();
    const QString type = prepared.value("type").toString().toUpper();
    const QString token = args.value("client_order_token").toString();
    const QString key = ticket_key(ctx.exchange, args);
    const QString err = reply.value("error").toString();
    const QString audit_cid = prepared.value("client_order_id").toVariant().toString();
    const QString audit_source = prepared.value(QStringLiteral("_order_source")).toString(); // see confirm_and_place
    if (outcome == QLatin1String("accepted"))
        LOG_AUDIT("Orders", "Order accepted",
                  {{"venue", ctx.exchange},
                   {"mode", "live"},
                   {"symbol", symbol},
                   {"side", side},
                   {"type", type},
                   {"client_id", audit_cid},
                   {"order_id", reply.value("id").toVariant().toString()},
                   {"status", reply.value("status").toString()},
                   {"source", audit_source}});
    else if (outcome == QLatin1String("rejected"))
        LOG_AUDIT("Orders", "Order rejected",
                  {{"venue", ctx.exchange},
                   {"mode", "live"},
                   {"symbol", symbol},
                   {"side", side},
                   {"type", type},
                   {"client_id", audit_cid},
                   {"reason", err},
                   {"source", audit_source}});
    else
        LOG_AUDIT("Orders", "Order outcome unknown",
                  {{"venue", ctx.exchange},
                   {"mode", "live"},
                   {"symbol", symbol},
                   {"side", side},
                   {"type", type},
                   {"client_id", audit_cid},
                   {"reason", err.isEmpty() ? QStringLiteral("no reply") : err},
                   {"source", audit_source}});

    if (outcome == QLatin1String("accepted")) {
        if (last_unknown_key_ == key)
            last_unknown_key_.clear();
        const QString id = reply.value("id").toVariant().toString();
        const QString status = reply.value("status").toString();
        const QJsonArray mismatch = reply.value("mismatch").toArray();
        const QString line = tr("%1 accepted %2 %3 %4 — order %5%6")
                                 .arg(ctx.exchange.toUpper(), side, type, symbol, id,
                                      status.isEmpty() ? QString() : tr(", status %1").arg(status));
        if (!mismatch.isEmpty()) {
            QStringList diffs;
            for (const auto& d : mismatch)
                diffs << d.toString();
            QMessageBox::warning(this, tr("Order Accepted — Check It"),
                                 tr("%1 accepted order %2 but reports details that differ from what was sent:\n%3\n\n"
                                    "Check the order on the exchange.")
                                     .arg(ctx.exchange.toUpper(), id, diffs.join(QLatin1Char('\n'))));
        } else if (status == QLatin1String("canceled") || status == QLatin1String("cancelled") ||
                   status == QLatin1String("rejected") || status == QLatin1String("expired")) {
            QMessageBox::information(this, tr("Order %1").arg(status),
                                     tr("%1 accepted order %2 and reports it %3 (filled %4). An immediate-or-cancel "
                                        "or post-only order that cannot execute ends this way.")
                                         .arg(ctx.exchange.toUpper(), id, status,
                                              reply.value("filled").toVariant().toString()));
        }
        order_entry_->reset_after_fill(line);
        refresh_live_data();
        return;
    }
    if (outcome == QLatin1String("rejected")) {
        // Definitely not placed: keep the ticket so it can be corrected.
        order_entry_->show_order_refusal(tr("⚠ Not placed: %1").arg(err));
        QMessageBox::warning(this, tr("Order Not Placed"),
                             tr("%1 did not place the %2 %3 order on %4:\n\n%5")
                                 .arg(ctx.exchange.toUpper(), side, type, symbol, err));
        return;
    }
    // Unknown: it may exist. Never re-send; refresh and look it up.
    last_unknown_token_ = token;
    last_unknown_key_ = key;
    last_unknown_at_ = QDateTime::currentMSecsSinceEpoch();
    const QString cid = prepared.value("client_order_id").toVariant().toString();
    order_entry_->reset_after_fill(tr("No confirmation for the last order — looking it up on %1…")
                                       .arg(ctx.exchange.toUpper()),
                                   QStringLiteral("warning"));
    QMessageBox::warning(
        this, tr("Order Not Confirmed"),
        tr("%1 has not confirmed the %2 %3 %4 order (%5).\n\n"
           "It MAY have been placed. Nothing was re-sent. Open Orders and Positions refresh now%6.\n\n"
           "If you enter the identical order again within 10 minutes it carries the same client order id, so an "
           "exchange that keeps ids unique rejects it as a duplicate.")
            .arg(ctx.exchange.toUpper(), side, type, symbol, err.isEmpty() ? tr("no reply") : err,
                 cid.isEmpty() ? tr("; this order carries no client order id, so check Open Orders")
                               : tr(", and the order is looked up by its client order id %1 in 3 s and again in "
                                    "10 s")
                                     .arg(cid)));
    reconcile_after_unconfirmed();
    if (!cid.isEmpty())
        look_up_unconfirmed_order(ctx, symbol, token, 0);
}

void CryptoTradingScreen::look_up_unconfirmed_order(const LiveContext& ctx, const QString& symbol,
                                                    const QString& token, int attempt) {
    QPointer<CryptoTradingScreen> self = this;
    QTimer::singleShot(attempt == 0 ? 3000 : 7000, this, [self, ctx, symbol, token, attempt]() {
        if (!self)
            return;
        (void)QtConcurrent::run([self, ctx, symbol, token, attempt]() {
            const QJsonObject r = ExchangeSessionManager::instance().session(ctx.exchange)->request_as(
                "find_order", {{"symbol", symbol}, {"client_order_token", token}}, ctx.creds, kAccountCallTimeoutMs);
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, ctx, symbol, token, attempt, r]() {
                    if (!self)
                        return;
                    const QString venue = ctx.exchange.toUpper();
                    if (r.value("found").toBool(false)) {
                        const QJsonObject o = r.value("order").toObject();
                        if (self->last_unknown_token_ == token)
                            self->last_unknown_key_.clear();
                        self->order_entry_->show_notice(
                            tr("The unconfirmed order IS on %1: order %2, status %3, filled %4 of %5 — do not "
                               "place it again.")
                                .arg(venue, o.value("id").toVariant().toString(), o.value("status").toString(),
                                     o.value("filled").toVariant().toString(), o.value("amount").toVariant().toString()),
                            QStringLiteral("warning"));
                        self->refresh_live_data();
                        return;
                    }
                    if (r.contains("error") || r.value("supported").toBool(true)) {
                        if (attempt == 0) {
                            self->look_up_unconfirmed_order(ctx, symbol, token, 1);
                            return;
                        }
                    }
                    if (r.contains("error")) {
                        self->order_entry_->show_notice(
                            tr("Could not look the unconfirmed order up on %1 (%2) — check Open Orders on the "
                               "exchange before placing it again.")
                                .arg(venue, r.value("error").toString()),
                            QStringLiteral("warning"));
                    } else if (!r.value("supported").toBool(true)) {
                        self->order_entry_->show_notice(
                            tr("%1: %2 — check Open Orders.").arg(venue, r.value("reason").toString()),
                            QStringLiteral("warning"));
                    } else {
                        self->order_entry_->show_notice(
                            tr("No order with client id %1 on %2 (open and recent orders searched): most likely it "
                               "was not placed. Check the exchange before entering it again.")
                                .arg(r.value("client_order_id").toVariant().toString(), venue),
                            QStringLiteral("warning"));
                    }
                },
                Qt::QueuedConnection);
        });
    });
}

void CryptoTradingScreen::on_cancel_order(const QString& order_id, const QString& order_symbol) {
    LOG_AUDIT("Orders", "Order cancel requested",
              {{"venue", exchange_id_},
               {"mode", trading_mode_ == TradingMode::Paper ? "paper" : "live"},
               {"order_id", order_id},
               {"symbol", order_symbol}});
    if (trading_mode_ == TradingMode::Paper) {
        try {
            pt_cancel_order(order_id);
            OrderMatcher::instance().remove_order(order_id);
            LOG_AUDIT("Orders", "Order cancelled",
                      {{"venue", exchange_id_}, {"mode", "paper"}, {"order_id", order_id}, {"symbol", order_symbol}});
            refresh_portfolio();
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Cancel failed: %1").arg(e.what()));
        }
        return;
    }
    // The order's OWN symbol (the exchange's market symbol from the Open Orders
    // row); the selected pair only when the row has none and it is exact.
    const QString symbol = !order_symbol.isEmpty() ? order_symbol
                           : (market_.valid() && market_.match == QLatin1String("exact")) ? market_.symbol
                                                                                           : QString();
    if (symbol.isEmpty()) {
        QMessageBox::warning(this, tr("Cancel Failed"),
                             tr("Order %1 has no market to cancel it on; cancel it on the exchange.").arg(order_id));
        return;
    }
    const LiveContext ctx = live_context();
    // No confirmation (a fast cancel matters): the row says "CANCELLING…" and its button
    // is disabled until the exchange answers, so a second press cannot race the first.
    bottom_panel_->set_order_cancelling(order_id, true);
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, order_id, symbol]() {
        const QJsonObject r = ExchangeSessionManager::instance().session(ctx.exchange)->request_as(
            "cancel_order", {{"order_id", order_id}, {"symbol", symbol}}, ctx.creds, kAccountCallTimeoutMs);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, r, order_id, symbol, venue = ctx.exchange]() {
                if (!self)
                    return;
                self->bottom_panel_->set_order_cancelling(order_id, false);
                const QString outcome = crypto::account_change_outcome(r);
                const QString err = r.value("error").toString();
                if (outcome == QLatin1String("accepted"))
                    LOG_AUDIT("Orders", "Order cancelled",
                              {{"venue", venue}, {"mode", "live"}, {"order_id", order_id}, {"symbol", symbol}});
                else if (outcome == QLatin1String("rejected"))
                    LOG_AUDIT("Orders", "Order cancel rejected",
                              {{"venue", venue},
                               {"mode", "live"},
                               {"order_id", order_id},
                               {"symbol", symbol},
                               {"reason", err}});
                else
                    LOG_AUDIT("Orders", "Order cancel outcome unknown",
                              {{"venue", venue},
                               {"mode", "live"},
                               {"order_id", order_id},
                               {"symbol", symbol},
                               {"reason", err.isEmpty() ? QStringLiteral("no reply") : err}});
                if (outcome == QLatin1String("unknown")) {
                    LOG_ERROR(TAG, QString("Live cancel not confirmed: %1").arg(err));
                    QMessageBox::warning(self, tr("Cancel Not Confirmed"),
                                         tr("The exchange has not confirmed the cancel of order %1 yet (%2).\n\n"
                                            "The order may already be cancelled or may still be open — check "
                                            "Open Orders, which refresh over the next few seconds.")
                                             .arg(order_id, err));
                    self->reconcile_after_unconfirmed();
                    return;
                }
                if (outcome == QLatin1String("rejected"))
                    QMessageBox::warning(self, tr("Cancel Failed"), tr("Could not cancel the order:\n%1").arg(err));
                self->refresh_live_data();
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::on_ob_price_clicked(double price) {
    // Fill the ticket's limit-price box. This used to call set_current_price(),
    // which only overwrites the MARK readout and the cost-basis price, so a
    // book click never reached the order ticket at all. The book is the stream
    // of the resolved market — the same market an order goes to.
    order_entry_->set_limit_price(price);
}

void CryptoTradingScreen::on_search_requested(const QString& filter) {
    QPointer<CryptoTradingScreen> self = this;
    QString filter_copy = filter;
    // Hyperliquid is a perps DEX (see is_perp_market): its tradable pairs are "swap"
    // markets, so a hard-coded "spot" filter made the search come back empty there.
    const QString market_type = exchange_id_ == QLatin1String("hyperliquid") ? QStringLiteral("swap")
                                                                             : QStringLiteral("spot");
    const QString ex = exchange_id_;
    (void)QtConcurrent::run([self, filter_copy, market_type, ex]() {
        if (!self)
            return;
        auto markets = ExchangeSessionManager::instance().session(ex)->fetch_markets(market_type, filter_copy);
        QMetaObject::invokeMethod(
            self,
            [self, markets, ex]() {
                if (!self || self->exchange_id_ != ex)
                    return;
                self->watchlist_->set_search_results(markets);
            },
            Qt::QueuedConnection);
    });
}

bool CryptoTradingScreen::is_perp_market() const {
    // From the resolved market: a Hyperliquid spot token or a spot pair is not a
    // perp even on a perps venue. Until it is known, a settled pair
    // ("BTC/USDC:USDC") or the perps venue is the best guess for the controls.
    if (market_.valid())
        return market_.is_contract();
    return exchange_id_ == QLatin1String("hyperliquid") || selected_symbol_.contains(QLatin1Char(':'));
}

void CryptoTradingScreen::update_futures_visibility() {
    if (order_entry_)
        order_entry_->set_futures_mode(is_perp_market());
}

// ── Bulk actions (H7: scope explicit, listed fresh, defaults to the market on screen) ──

void CryptoTradingScreen::on_cancel_all_orders() {
    if (trading_mode_ == TradingMode::Paper) {
        if (portfolio_id_.isEmpty())
            return;
        try {
            for (const auto& o : pt_get_orders(portfolio_id_, "pending")) {
                pt_cancel_order(o.id);
                OrderMatcher::instance().remove_order(o.id);
                LOG_AUDIT("Orders", "Order cancelled",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"order_id", o.id},
                           {"symbol", o.symbol},
                           {"source", "cancel_all"}});
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Paper cancel-all failed: %1").arg(e.what()));
        }
        refresh_portfolio();
        return;
    }
    if (account_action_busy_) {
        QMessageBox::information(this, tr("Cancel Orders"),
                                 tr("The previous cancel / close is still running — wait for its result."));
        return;
    }
    const LiveContext ctx = live_context();
    const QString scope = market_.valid() && market_.match == QLatin1String("exact") ? market_.symbol : QString();
    account_action_busy_ = true;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, scope]() {
        auto* s = ExchangeSessionManager::instance().session(ctx.exchange);
        // Fresh: the market in scope, then the whole account (some venues refuse
        // an account-wide listing; then only the scope can be offered).
        QJsonObject in_scope;
        if (!scope.isEmpty())
            in_scope = s->request_as("fetch_open_orders", {{"symbol", scope}}, ctx.creds, kAccountCallTimeoutMs);
        const QJsonObject all = s->request_as("fetch_open_orders", {}, ctx.creds, kAccountCallTimeoutMs);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, ctx, scope, in_scope, all]() {
                if (!self)
                    return;
                self->account_action_busy_ = false;
                QString why;
                if (!self->live_context_current(ctx, &why))
                    return;
                QJsonArray scoped = in_scope.value("orders").toArray();
                const QJsonArray every = all.value("orders").toArray();
                if (scope.isEmpty() || in_scope.contains("error")) {
                    scoped = QJsonArray{};
                    for (const auto& v : every) {
                        if (scope.isEmpty() || v.toObject().value("symbol").toString() == scope)
                            scoped.append(v);
                    }
                }
                const bool all_ok = !all.contains("error");
                const int others = all_ok ? static_cast<int>(every.size()) - static_cast<int>(scoped.size()) : 0;
                if (scoped.isEmpty() && others <= 0) {
                    const QString err = all.value("error").toString(in_scope.value("error").toString());
                    QMessageBox::information(self, tr("Cancel Live Orders"),
                                             err.isEmpty() ? tr("No open orders on %1.").arg(ctx.exchange.toUpper())
                                                           : tr("Could not list open orders:\n%1").arg(err));
                    return;
                }
                const QString where = scope.isEmpty() ? tr("the whole %1 account").arg(ctx.exchange.toUpper()) : scope;
                QStringList lines;
                for (int i = 0; i < scoped.size() && i < 15; ++i) {
                    const QJsonObject o = scoped[i].toObject();
                    lines << tr("%1 %2 %3 %4 @ %5 — id %6")
                                 .arg(o.value("side").toString().toUpper(), o.value("type").toString().toUpper(),
                                      o.value("amount").toVariant().toString(), o.value("symbol").toString(),
                                      o.value("price").toVariant().toString(), o.value("id").toVariant().toString());
                }
                if (scoped.size() > 15)
                    lines << tr("… and %1 more").arg(scoped.size() - 15);
                QString text = tr("Cancel these LIVE orders on %1 (%2)?\n\n").arg(where, ctx.account_label) +
                               (lines.isEmpty() ? tr("(none on %1)").arg(where) : lines.join(QLatin1Char('\n')));
                if (others > 0)
                    text += QLatin1String("\n\n") +
                            tr("%1 other open order(s) on other markets are NOT affected unless you choose the whole "
                               "account.")
                                .arg(others);
                else if (!all_ok)
                    text += QLatin1String("\n\n") + tr("Orders on other markets could not be listed (%1).")
                                                        .arg(all.value("error").toString());
                QMessageBox box(QMessageBox::Warning, tr("Cancel Live Orders"), text, QMessageBox::NoButton, self);
                QAbstractButton* scope_btn =
                    scoped.isEmpty() ? nullptr
                                     : box.addButton(tr("Cancel orders on %1 (%2)").arg(where).arg(scoped.size()),
                                                     QMessageBox::AcceptRole);
                QAbstractButton* all_btn =
                    others > 0 ? box.addButton(tr("Cancel orders on the whole account (%1)").arg(every.size()),
                                               QMessageBox::DestructiveRole)
                               : nullptr;
                for (QAbstractButton* b : {scope_btn, all_btn})
                    if (b)
                        b->setProperty("fincept_agent_risk", "order");
                QPushButton* keep = box.addButton(tr("Keep"), QMessageBox::RejectRole);
                box.setDefaultButton(keep);
                box.setEscapeButton(keep);
                box.exec();
                const QAbstractButton* clicked = box.clickedButton();
                if (!clicked || clicked == keep)
                    return;
                if (!self->live_context_current(ctx, &why)) {
                    QMessageBox::warning(self, tr("Not Cancelled"), tr("Nothing was sent: %1.").arg(why));
                    return;
                }
                const QJsonArray chosen = clicked == all_btn ? every : scoped;
                Q_UNUSED(scope_btn);
                self->account_action_busy_ = true;
                (void)QtConcurrent::run([self, ctx, chosen]() {
                    auto* s = ExchangeSessionManager::instance().session(ctx.exchange);
                    int done = 0, refused = 0, unknown = 0;
                    QString first_err;
                    for (const auto& v : chosen) {
                        const QJsonObject o = v.toObject();
                        const QString id = o.value("id").toVariant().toString();
                        if (id.isEmpty())
                            continue;
                        const QJsonObject r =
                            s->request_as("cancel_order", {{"order_id", id}, {"symbol", o.value("symbol").toString()}},
                                          ctx.creds, kAccountCallTimeoutMs);
                        const QString outcome = crypto::account_change_outcome(r);
                        if (outcome == QLatin1String("accepted"))
                            LOG_AUDIT("Orders", "Order cancelled",
                                      {{"venue", ctx.exchange},
                                       {"mode", "live"},
                                       {"order_id", id},
                                       {"symbol", o.value("symbol").toString()},
                                       {"source", "cancel_all"}});
                        else if (outcome == QLatin1String("rejected"))
                            LOG_AUDIT("Orders", "Order cancel rejected",
                                      {{"venue", ctx.exchange},
                                       {"mode", "live"},
                                       {"order_id", id},
                                       {"symbol", o.value("symbol").toString()},
                                       {"reason", r.value("error").toString()},
                                       {"source", "cancel_all"}});
                        else
                            LOG_AUDIT("Orders", "Order cancel outcome unknown",
                                      {{"venue", ctx.exchange},
                                       {"mode", "live"},
                                       {"order_id", id},
                                       {"symbol", o.value("symbol").toString()},
                                       {"reason", r.value("error").toString()},
                                       {"source", "cancel_all"}});
                        if (outcome == QLatin1String("accepted"))
                            ++done;
                        else if (outcome == QLatin1String("rejected"))
                            ++refused;
                        else
                            ++unknown;
                        if (outcome != QLatin1String("accepted") && first_err.isEmpty())
                            first_err = r.value("error").toString();
                    }
                    if (!self)
                        return;
                    QMetaObject::invokeMethod(
                        self,
                        [self, done, refused, unknown, first_err]() {
                            if (!self)
                                return;
                            self->account_action_busy_ = false;
                            LOG_INFO(TAG, QString("Cancelled %1 live order(s), %2 refused, %3 not confirmed")
                                              .arg(done)
                                              .arg(refused)
                                              .arg(unknown));
                            if (refused > 0 || unknown > 0) {
                                QString msg =
                                    tr("%1 order(s) cancelled, %2 could not be cancelled.").arg(done).arg(refused);
                                if (unknown > 0)
                                    msg += QLatin1Char('\n') + tr("%1 cancel(s) not confirmed yet — those orders may "
                                                                  "be cancelled or still open; check Open Orders.")
                                                                   .arg(unknown);
                                QMessageBox::warning(self, tr("Cancel Live Orders"),
                                                     msg + QLatin1Char('\n') + first_err);
                            }
                            if (unknown > 0)
                                self->reconcile_after_unconfirmed();
                            else
                                self->refresh_live_data();
                        },
                        Qt::QueuedConnection);
                });
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::on_close_all_positions() {
    if (trading_mode_ == TradingMode::Paper) {
        if (portfolio_id_.isEmpty())
            return;
        QStringList not_closed; // positions left open because no exchange rate is known yet
        try {
            for (const auto& p : pt_get_positions(portfolio_id_)) {
                if (p.quantity == 0)
                    continue;
                // The released margin, fee and realized P&L are booked in the paper
                // account's currency at the pair's FX factor (lost on a restart):
                // re-establish it from a real rate first. None: this one stays
                // open — never closed at 1:1 — and the others still close. GUI
                // thread: no waiting (wait_ms 0).
                QString fx_why;
                const bool fx_ready =
                    cth_is_contract_symbol(p.symbol)
                        ? cth_prepare_contract_fx(portfolio_id_, p.symbol, portfolio_.currency, true, &fx_why)
                        : PaperMarkService::prepare_close_fx(portfolio_id_, p.symbol, QStringLiteral("CRYPTO"), 0,
                                                             &fx_why);
                if (!fx_ready) {
                    LOG_WARN(TAG, QString("Paper close-all: %1 not closed (FX): %2").arg(p.symbol, fx_why));
                    not_closed << fx_why;
                    continue;
                }
                auto ticker = session()->get_cached_price(p.symbol);
                const double fill = ticker.last > 0 ? ticker.last : p.current_price;
                // Paper positions store quantity as a positive magnitude with
                // side="long"/"short" (shorts are +qty), so derive the close
                // direction from side, not the sign — else a short is doubled
                // instead of covered. (Matches UnifiedTrading::close_all_positions.)
                const QString pdir = QString(p.side).toLower();
                const bool is_short = pdir == "short" || pdir == "sell" || p.quantity < 0;
                const QString side = is_short ? "buy" : "sell";
                auto order = pt_place_order(portfolio_id_, p.symbol, side, "market", std::abs(p.quantity),
                                            std::optional<double>(fill), std::nullopt);
                LOG_AUDIT("Orders", "Order submitted",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"symbol", p.symbol},
                           {"side", side},
                           {"type", "market"},
                           {"qty", std::abs(p.quantity)},
                           {"order_id", order.id},
                           {"source", "close_all"}});
                pt_fill_order(order.id, fill);
                LOG_AUDIT("Orders", "Order accepted",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"symbol", p.symbol},
                           {"order_id", order.id},
                           {"status", "filled"},
                           {"fill_price", fill}});
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Paper close-all failed: %1").arg(e.what()));
        }
        refresh_portfolio();
        if (!not_closed.isEmpty())
            QMessageBox::warning(this, tr("Positions Not Closed"), not_closed.join(QStringLiteral("\n\n")));
        return;
    }
    close_live_positions(QString());
}

void CryptoTradingScreen::on_close_position(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    if (trading_mode_ == TradingMode::Paper) {
        if (portfolio_id_.isEmpty())
            return;
        // Released margin, fee and realized P&L are booked in the paper account's
        // currency at the pair's FX factor: re-establish it from a real rate before
        // anything is booked. None yet: nothing is closed (never at 1:1) and the
        // user is told why. GUI thread: no waiting (wait_ms 0).
        QString fx_why;
        const bool fx_ready = cth_is_contract_symbol(symbol)
                                  ? cth_prepare_contract_fx(portfolio_id_, symbol, portfolio_.currency, true, &fx_why)
                                  : PaperMarkService::prepare_close_fx(portfolio_id_, symbol,
                                                                       QStringLiteral("CRYPTO"), 0, &fx_why);
        if (!fx_ready) {
            LOG_WARN(TAG, QString("Paper close of %1 not placed (FX): %2").arg(symbol, fx_why));
            QMessageBox::warning(this, tr("Position Not Closed"), fx_why);
            return;
        }
        try {
            for (const auto& p : pt_get_positions(portfolio_id_)) {
                if (p.symbol != symbol || p.quantity == 0)
                    continue;
                auto ticker = session()->get_cached_price(symbol);
                const double fill = ticker.last > 0 ? ticker.last : p.current_price;
                // Paper positions store quantity as a positive magnitude with
                // side="long"/"short" (shorts are +qty), so derive the close
                // direction from side, not the sign — else a short is doubled
                // instead of covered. (Matches UnifiedTrading::close_all_positions.)
                const QString pdir = QString(p.side).toLower();
                const bool is_short = pdir == "short" || pdir == "sell" || p.quantity < 0;
                const QString side = is_short ? "buy" : "sell";
                auto order = pt_place_order(portfolio_id_, symbol, side, "market", std::abs(p.quantity),
                                            std::optional<double>(fill), std::nullopt);
                LOG_AUDIT("Orders", "Order submitted",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"symbol", symbol},
                           {"side", side},
                           {"type", "market"},
                           {"qty", std::abs(p.quantity)},
                           {"order_id", order.id},
                           {"source", "close_position"}});
                pt_fill_order(order.id, fill);
                LOG_AUDIT("Orders", "Order accepted",
                          {{"venue", exchange_id_},
                           {"mode", "paper"},
                           {"symbol", symbol},
                           {"order_id", order.id},
                           {"status", "filled"},
                           {"fill_price", fill}});
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Paper close failed: %1").arg(e.what()));
        }
        refresh_portfolio();
        return;
    }
    close_live_positions(symbol);
}

void CryptoTradingScreen::close_live_positions(const QString& only_symbol) {
    // H5 / H7: positions are read fresh; each close is prepared like a ticket
    // order (reduce-only, sized in the position's own contracts, a slippage
    // bound where the venue has no market orders, e.g. Hyperliquid) and listed
    // in one confirmation; only what was listed is sent.
    if (account_action_busy_) {
        QMessageBox::information(this, tr("Close Positions"),
                                 tr("The previous cancel / close is still running — wait for its result."));
        return;
    }
    const LiveContext ctx = live_context();
    const QString scope = !only_symbol.isEmpty() ? only_symbol
                          : (market_.valid() && market_.match == QLatin1String("exact")) ? market_.symbol
                                                                                          : QString();
    const QString ticket_margin = order_entry_->margin_mode();
    const int ticket_leverage = order_entry_->leverage();
    account_action_busy_ = true;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, scope, only_symbol, ticket_margin, ticket_leverage]() {
        auto* s = ExchangeSessionManager::instance().session(ctx.exchange);
        const QJsonObject pos = s->request_as("fetch_positions", {}, ctx.creds, kAccountCallTimeoutMs);
        QVector<QJsonObject> args_in, prep_in, args_out, prep_out;
        QStringList problems;
        for (const auto& v : pos.value("positions").toArray()) {
            const QJsonObject p = v.toObject();
            const QString sym = p.value("symbol").toString();
            const double contracts = p.value("contracts").toDouble();
            if (sym.isEmpty() || contracts == 0)
                continue;
            if (!only_symbol.isEmpty() && sym != only_symbol)
                continue;
            const QString close_side = live_close_side(p, contracts);
            if (close_side.isEmpty()) {
                problems << tr("%1: the exchange did not report whether this position is long or short (side "
                               "\"%2\"), so it is not closed here — close it on the exchange.")
                                .arg(sym, p.value("side").toString());
                continue;
            }
            QString margin = p.value("marginMode").toString().toLower();
            if (margin != QLatin1String("cross") && margin != QLatin1String("isolated"))
                margin = ticket_margin;
            int lev = static_cast<int>(p.value("leverage").toDouble());
            if (lev < 1)
                lev = ticket_leverage;
            QJsonObject a{{"symbol", sym},
                          {"side", close_side},
                          {"type", "market"},
                          {"amount", std::abs(contracts)},
                          {"amount_unit", "contracts"},
                          {"reduce_only", true},
                          {"margin_mode", margin},
                          {"leverage", lev},
                          {"client_order_token", new_client_token()}};
            const QJsonObject prep = s->request_as("prepare_order", a, ctx.creds, kPrepareTimeoutMs);
            if (prep.contains("error")) {
                problems << tr("%1: cannot be closed from here — %2").arg(sym, prep.value("error").toString());
                continue;
            }
            const bool in_scope = scope.isEmpty() || sym == scope;
            (in_scope ? args_in : args_out).append(a);
            (in_scope ? prep_in : prep_out).append(prep);
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, ctx, scope, only_symbol, pos, args_in, prep_in, args_out, prep_out, problems]() {
                if (!self)
                    return;
                self->account_action_busy_ = false;
                QString why;
                if (!self->live_context_current(ctx, &why))
                    return;
                if (pos.contains("error")) {
                    QMessageBox::warning(self, tr("Close Positions"),
                                         tr("Could not read positions from %1:\n%2")
                                             .arg(ctx.exchange.toUpper(), pos.value("error").toString()));
                    return;
                }
                const QString where = scope.isEmpty() ? tr("the whole %1 account").arg(ctx.exchange.toUpper()) : scope;
                if (prep_in.isEmpty() && prep_out.isEmpty()) {
                    QMessageBox::information(self, tr("Close Positions"),
                                             (problems.isEmpty() ? tr("No open position on %1.").arg(where)
                                                                 : problems.join(QLatin1Char('\n'))));
                    return;
                }
                QStringList lines;
                for (const auto& p : prep_in)
                    lines << prepared_line(p);
                QString text = tr("Close these LIVE positions on %1 (%2) with reduce-only market orders?\n\n")
                                   .arg(where, ctx.account_label) +
                               (lines.isEmpty() ? tr("(no open position on %1)").arg(where)
                                                : lines.join(QLatin1Char('\n')));
                if (!prep_out.isEmpty())
                    text += QLatin1String("\n\n") +
                            tr("%1 position(s) on other markets are NOT affected unless you choose the whole account.")
                                .arg(prep_out.size());
                if (!problems.isEmpty())
                    text += QLatin1String("\n\n") + problems.join(QLatin1Char('\n'));
                QMessageBox box(QMessageBox::Warning, tr("Close Live Positions"), text, QMessageBox::NoButton, self);
                QAbstractButton* scope_btn =
                    prep_in.isEmpty()
                        ? nullptr
                        : box.addButton(tr("Close positions on %1 (%2)").arg(where).arg(prep_in.size()),
                                        QMessageBox::AcceptRole);
                QAbstractButton* all_btn =
                    (!prep_out.isEmpty() && only_symbol.isEmpty())
                        ? box.addButton(tr("Close positions on the whole account (%1)")
                                            .arg(prep_in.size() + prep_out.size()),
                                        QMessageBox::DestructiveRole)
                        : nullptr;
                for (QAbstractButton* b : {scope_btn, all_btn})
                    if (b)
                        b->setProperty("fincept_agent_risk", "order");
                QPushButton* keep = box.addButton(tr("Keep"), QMessageBox::RejectRole);
                box.setDefaultButton(keep);
                box.setEscapeButton(keep);
                box.exec();
                const QAbstractButton* clicked = box.clickedButton();
                if (!clicked || clicked == keep || (clicked != scope_btn && clicked != all_btn))
                    return;
                QVector<QJsonObject> a = args_in, p = prep_in;
                if (clicked == all_btn) {
                    a += args_out;
                    p += prep_out;
                }
                self->place_prepared_batch(ctx, a, p, tr("Close Live Positions"));
            },
            Qt::QueuedConnection);
    });
}

void CryptoTradingScreen::place_prepared_batch(const LiveContext& ctx, const QVector<QJsonObject>& args,
                                               const QVector<QJsonObject>& prepared, const QString& title) {
    QString why;
    if (!live_context_current(ctx, &why)) {
        QMessageBox::warning(this, title, tr("Nothing was sent: %1.").arg(why));
        return;
    }
    account_action_busy_ = true;
    QPointer<CryptoTradingScreen> self = this;
    (void)QtConcurrent::run([self, ctx, args, prepared, title]() {
        auto* s = ExchangeSessionManager::instance().session(ctx.exchange);
        QStringList lines;
        int unknown = 0;
        for (int i = 0; i < args.size() && i < prepared.size(); ++i) {
            QJsonObject place = args[i];
            place["confirm_hash"] = prepared[i].value("confirm_hash");
            place["ref_price"] = prepared[i].value("ref_price");
            place["account_id"] = ctx.account_id;
            const QString close_symbol = prepared[i].value("market").toObject().value("symbol").toString();
            LOG_AUDIT("Orders", "Order submitted",
                      {{"venue", ctx.exchange},
                       {"account", fincept::log_mask(ctx.account_id)},
                       {"mode", "live"},
                       {"symbol", close_symbol},
                       {"side", prepared[i].value("side").toString()},
                       {"type", prepared[i].value("type").toString()},
                       {"qty", prepared[i].value("amount_text").toString()},
                       {"client_id", prepared[i].value("client_order_id").toVariant().toString()},
                       {"source", "position_close"}});
            const QJsonObject r = s->request_as("place_order", place, ctx.creds, kPlaceTimeoutMs);
            QString outcome = crypto::account_change_outcome(r);
            if (outcome == QLatin1String("accepted") && r.value("exchange").toString() != ctx.exchange)
                outcome = QStringLiteral("unknown");
            LOG_AUDIT("Orders", "Position close result",
                      {{"venue", ctx.exchange},
                       {"mode", "live"},
                       {"symbol", close_symbol},
                       {"outcome", outcome},
                       {"order_id", r.value("id").toVariant().toString()},
                       {"reason", r.value("error").toString()}});
            const QString what = prepared_line(prepared[i]);
            if (outcome == QLatin1String("accepted"))
                lines << tr("✓ %1 — order %2%3")
                             .arg(what, r.value("id").toVariant().toString(),
                                  r.value("status").toString().isEmpty() ? QString()
                                                                         : tr(", %1").arg(r.value("status").toString()));
            else if (outcome == QLatin1String("rejected"))
                lines << tr("✗ %1 — not placed: %2").arg(what, r.value("error").toString());
            else {
                ++unknown;
                lines << tr("? %1 — NOT CONFIRMED, it may have been placed (%2); not re-sent")
                             .arg(what, r.value("error").toString());
            }
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, lines, unknown, title]() {
                if (!self)
                    return;
                self->account_action_busy_ = false;
                // Each close was written to the audit log (Position close result) as it was answered.
                LOG_DEBUG(TAG, title + ": " + lines.join(" | "));
                QMessageBox::information(self, title, lines.join(QLatin1Char('\n')));
                if (unknown > 0)
                    self->reconcile_after_unconfirmed();
                else
                    self->refresh_live_data();
            },
            Qt::QueuedConnection);
    });
}

// ============================================================================
// Refresh Functions
// ============================================================================

// ============================================================================
// WS Update Coalescing — flush accumulated data to UI at 10fps
// ============================================================================

} // namespace fincept::screens
