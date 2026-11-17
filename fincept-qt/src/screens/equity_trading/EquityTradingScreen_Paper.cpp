// src/screens/equity_trading/EquityTradingScreen_Paper.cpp
//
// The built-in paper account (no broker; see trading/BrokerAccount.h) and the
// canonical-symbol model of the Equity Trading screen: focusing the account,
// simulated orders / closes on delayed public prices, instrument labels and
// public-data targets, the right-column market snapshot, the status bar, the
// first-run note and splitter persistence.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "core/instrument/VenueSymbols.h"
#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketProfile.h"
#include "core/market/MarketSessions.h"
#include "core/events/EventBus.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChartPanel.h"
#include "screens/equity_trading/EquityCommandBar.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityQuoteSnapshot.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityTradingScreen.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "screens/equity_trading/EquityWorkspace.h"
#include "screens/equity_trading/OrderConfirmDialog.h"
#include "services/markets/FxRateService.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderMatcher.h"
#include "trading/OrderSourceCapture.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>

#include <cmath>
#include <limits>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;
namespace ci = fincept::core::instrument;
namespace mk = fincept::core::market;
namespace fmt = fincept::ui::formatting;

namespace {
const char* const eqpp_kTag = "EquityTrading";

// A canonical symbol to open the built-in account on, by the user's region.
QString eqpp_default_symbol(const QString& region) {
    static const QHash<QString, QString> k = {
        {"IN", "RELIANCE.NS"}, {"UK", "SHEL.L"},    {"EU", "SAP.DE"},     {"JP", "7203.T"},  {"CN", "0700.HK"},
        {"AU", "BHP.AX"},      {"CA", "RY.TO"},     {"KR", "005930.KS"},  {"BR", "PETR4.SA"}, {"US", "AAPL"},
    };
    return k.value(region, QStringLiteral("AAPL"));
}

QString eqpp_age(qint64 ms) {
    const qint64 s = std::max<qint64>(0, ms / 1000);
    if (s < 90)
        return QCoreApplication::translate("EquityTradingScreen", "%1 s ago").arg(s);
    if (s < 5400)
        return QCoreApplication::translate("EquityTradingScreen", "%1 min ago").arg(s / 60);
    return QCoreApplication::translate("EquityTradingScreen", "%1 h ago").arg(s / 3600);
}
} // namespace

// ============================================================================
// Built-in paper account
// ============================================================================

bool EquityTradingScreen::focused_is_builtin() const {
    return is_builtin_paper_account(focused_account_id_);
}

bool EquityTradingScreen::simulated_on_delayed_prices() const {
    return focused_is_builtin() || (focused_is_paper_ && public_mode_);
}

QString EquityTradingScreen::base_currency() const {
    // The user's display currency (Settings > Currency & Number Format; Automatic =
    // the region's currency). Never empty. Fixed for the account once created.
    return fmt::display_currency();
}

double EquityTradingScreen::default_paper_balance(const QString& currency) {
    static const QHash<QString, double> k = {
        {"INR", 1'000'000.0},   {"JPY", 10'000'000.0}, {"KRW", 100'000'000.0}, {"CNY", 1'000'000.0},
        {"HKD", 1'000'000.0},   {"IDR", 1'000'000'000.0}, {"BRL", 500'000.0},  {"ZAR", 1'000'000.0},
        {"TWD", 3'000'000.0},   {"THB", 3'000'000.0},
    };
    return k.value(currency.toUpper(), 100'000.0);
}

QString EquityTradingScreen::ensure_builtin_account() {
    auto& am = AccountManager::instance();
    auto acct = am.builtin_paper_account();
    if (acct.paper_portfolio_id.isEmpty()) {
        const QString ccy = base_currency();
        acct = am.ensure_builtin_paper_account(ccy, default_paper_balance(ccy));
    }
    if (acct.paper_portfolio_id.isEmpty())
        return {};
    try {
        const auto pf = pt_get_portfolio(acct.paper_portfolio_id);
        account_currency_ = pf.currency;
        PaperMarkService::instance().track_public_portfolio(pf.id, pf.currency);
    } catch (const std::exception& e) {
        LOG_WARN(eqpp_kTag, QString("Built-in paper portfolio unavailable: %1").arg(e.what()));
        return {};
    }
    return acct.paper_portfolio_id;
}

void EquityTradingScreen::focus_builtin_account() {
    const QString id = QString::fromLatin1(kBuiltinPaperAccountId);
    if (ensure_builtin_account().isEmpty()) {
        order_entry_->show_status_message(tr("The paper account could not be created — the terminal database is not "
                                             "available."),
                                          false);
        return;
    }
    if (focused_is_builtin() && initialized_)
        return;

    // Leaving a broker account: its delayed-price tracking (if any) ends here.
    if (!public_tracked_portfolio_.isEmpty()) {
        PaperMarkService::instance().untrack_public_portfolio(public_tracked_portfolio_);
        public_tracked_portfolio_.clear();
    }
    focused_account_id_ = id;
    broker_quote_seen_ = false;
    broker_quote_refusal_.clear();
    apply_broker_quote_notice();
    row_block_reason_.clear();

    bottom_panel_->set_account_id(id);
    focused_is_us_market_ = false;
    bottom_panel_->set_us_market_tabs_visible(false);
    order_entry_->set_builtin_mode(true, account_currency_);
    order_entry_->set_mode(true);
    watchlist_->set_broker_id(QString());
    symbol_box_->set_row_filter({}); // every instrument with public data can be paper-traded
    watchlist_->add_box()->set_row_filter({});
    watchlist_->add_box()->set_raw_text_allowed(false);

    // PAPER, and it can never be switched to LIVE: there is no broker behind it.
    mode_btn_->setChecked(false);
    mode_btn_->setText(tr("PAPER"));
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    mode_btn_->setEnabled(false);
    mode_btn_->setToolTip(tr("The built-in paper account always simulates — it has no broker to go live with. "
                             "Connect a broker in ACCOUNTS to trade live."));
    sync_native_paper();

    // The symbol last viewed on this account, else a large listing of the user's market.
    QString sym = builtin_selected_symbol_;
    if (sym.isEmpty())
        sym = eqpp_default_symbol(mk::MarketProfileStore::instance().current_id());
    auto inst = eq_instrument_for_stored(sym, QString());
    if (inst.symbol.isEmpty())
        inst = eq_instrument_for_stored(QStringLiteral("AAPL"), QString());
    instruments_.insert(inst.symbol, inst);
    selected_symbol_ = inst.symbol;
    selected_exchange_ = inst.venue;
    current_price_ = 0.0;

    load_watchlists(); // canonical rows (migrated from broker-style symbols on the fly)
    ticker_bar_->set_bid_ask_visible(false);
    ticker_bar_->set_symbol(inst.display.isEmpty() ? inst.symbol : inst.display);
    watchlist_->set_active_symbol(selected_symbol_);
    apply_selected_instrument_ui();

    if (isVisible())
        hub_subscribe_streaming();
    sync_market_feed(/*fetch_on_switch=*/false);
    orders_view_day_ = QDate::currentDate();
    refresh_paper_panels();

    update_account_menu();
    update_connection_status();
    update_feed_notices();
    update_first_run_note();
    update_right_column();
    update_status_bar();
    LOG_INFO(eqpp_kTag, QString("Focused the built-in paper account (%1)").arg(account_currency_));
}

void EquityTradingScreen::open_paper_settings() {
    const auto acct = AccountManager::instance().builtin_paper_account();
    PtPortfolio pf;
    try {
        pf = pt_get_portfolio(ensure_builtin_account());
    } catch (const std::exception& e) {
        LOG_ERROR(eqpp_kTag, logf("Paper account settings not opened: the portfolio could not be read", {{"error", e.what()}}));
        QMessageBox::warning(this, tr("Paper Account"), tr("The paper account is not available."));
        return;
    } catch (...) {
        LOG_ERROR(eqpp_kTag, logf("Paper account settings not opened: the portfolio could not be read",
                                  {{"error", "unknown exception"}}));
        QMessageBox::warning(this, tr("Paper Account"), tr("The paper account is not available."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Paper Account"));
    auto* form = new QFormLayout(&dlg);
    auto* intro = new QLabel(tr("The built-in paper account simulates trading on delayed public prices. No order "
                                "ever reaches a broker. Changing the starting balance or the currency starts the "
                                "account over: positions, orders and history are cleared."),
                             &dlg);
    intro->setWordWrap(true);
    form->addRow(intro);
    form->addRow(tr("Cash now"), new QLabel(fmt::money(pf.balance, pf.currency), &dlg));
    // Any currency of the shared table the FX service can convert (fiat in major
    // units and stablecoins; not pence / cents codes, metals or volatile crypto):
    // the display currency first, then the table order.
    auto* ccy = new QComboBox(&dlg);
    QStringList codes{fmt::display_currency()};
    for (const QString& c : fmt::known_currency_codes()) {
        fmt::CurrencySpec spec;
        if (!fmt::find_currency(c, &spec) || spec.minor_divisor != 1 || spec.metal || (spec.crypto && !spec.stablecoin))
            continue;
        if (!codes.contains(c))
            codes << c;
    }
    for (const QString& c : codes)
        ccy->addItem(QStringLiteral("%1 — %2").arg(c, fmt::currency_name(c)), c);
    if (ccy->findData(pf.currency) < 0)
        ccy->addItem(pf.currency, pf.currency);
    ccy->setCurrentIndex(ccy->findData(pf.currency));
    form->addRow(tr("Account currency"), ccy);
    auto* bal = new QDoubleSpinBox(&dlg);
    bal->setRange(1000.0, 1.0e12);
    bal->setDecimals(0);
    bal->setGroupSeparatorShown(true);
    bal->setValue(pf.initial_balance > 0 ? pf.initial_balance : default_paper_balance(pf.currency));
    fmt::bind_currency_affix(bal, pf.currency); // the unit of the amount, placed per the user's settings
    form->addRow(tr("Starting balance"), bal);
    connect(ccy, &QComboBox::currentIndexChanged, &dlg, [bal, ccy](int) {
        const QString c = ccy->currentData().toString();
        bal->setValue(EquityTradingScreen::default_paper_balance(c));
        fmt::bind_currency_affix(bal, c); // modal dialog: the last binding wins
    });

    auto* buttons = new QDialogButtonBox(&dlg);
    auto* reset_btn = buttons->addButton(tr("Start over"), QDialogButtonBox::AcceptRole);
    auto* topup_btn = buttons->addButton(tr("Set cash only"), QDialogButtonBox::ActionRole);
    reset_btn->setProperty("fincept_agent_risk", "money"); // paper account reset
    topup_btn->setProperty("fincept_agent_risk", "money"); // paper balance change
    buttons->addButton(QDialogButtonBox::Cancel);
    reset_btn->setToolTip(tr("Delete every position, order and fill and start with the balance above"));
    topup_btn->setToolTip(tr("Keep positions and history; set the available cash to the balance above"));
    form->addRow(buttons);
    bool topup = false;
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(topup_btn, &QPushButton::clicked, &dlg, [&dlg, &topup]() {
        topup = true;
        dlg.accept();
    });
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (topup) {
        try {
            pt_set_balance(pf.id, bal->value());
            order_entry_->show_status_message(tr("Paper cash set to %1").arg(fmt::money(bal->value(), pf.currency)),
                                              true);
        } catch (const std::exception& e) {
            LOG_ERROR(eqpp_kTag, logf("Paper cash could not be set", {{"portfolio", log_mask(pf.id)}, {"error", e.what()}}));
            order_entry_->show_status_message(tr("Could not set the cash: %1").arg(e.what()), false);
        }
        refresh_paper_panels();
        return;
    }
    if (QMessageBox::warning(this, tr("Start Over"),
                             tr("Delete every paper position, order and fill of \"%1\" and start again with %2?")
                                 .arg(acct.display_name, fmt::money(bal->value(), ccy->currentData().toString())),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    QString error;
    if (!AccountManager::instance().reset_builtin_paper_account(ccy->currentData().toString(), bal->value(), &error)) {
        order_entry_->show_status_message(tr("Reset failed: %1").arg(error), false);
        return;
    }
    // builtin_paper_reset re-focuses the account onto the new portfolio.
    order_entry_->show_status_message(tr("Paper account started over"), true);
}

// ============================================================================
// Simulated orders on delayed prices
// ============================================================================

PtDelayedQuote EquityTradingScreen::delayed_quote_for(const QString& symbol) const {
    const QString yahoo = public_target(symbol).yahoo;
    if (yahoo.isEmpty())
        return {};
    const auto d = quote_detail_.constFind(symbol);
    if (d != quote_detail_.constEnd() && d->price > 0.0)
        return PaperMarkService::make_delayed_quote(yahoo, *d, quote_rx_ms_.value(symbol));
    return PaperMarkService::instance().delayed_quote(yahoo);
}

void EquityTradingScreen::place_simulated_order(const UnifiedOrder& order, bool confirm) {
    const auto account = AccountManager::instance().get_account(focused_account_id_);
    const QString pid = account.paper_portfolio_id;
    // A paper order refused here, before the engine (which audits everything after).
    const auto audit_refused = [&](const QString& reason) {
        LOG_AUDIT("Orders", "Order rejected",
                  {{"venue", "paper"},
                   {"account", is_builtin_paper_account(account.account_id) ? account.account_id
                                                                             : log_mask(account.account_id)},
                   {"mode", "paper"},
                   {"symbol", order.symbol},
                   {"side", order.side == OrderSide::Buy ? "buy" : "sell"},
                   {"type", order_type_str(order.order_type)},
                   {"qty", order.quantity},
                   {"source", "delayed_price_ticket"},
                   {"reason", reason}});
    };
    if (pid.isEmpty()) {
        audit_refused(QStringLiteral("no paper portfolio for this account"));
        order_entry_->show_order_status(tr("No paper portfolio for this account"), false);
        return;
    }
    const auto target = public_target(order.symbol);
    const QString shown = display_of(order.symbol);
    if (target.yahoo.isEmpty()) {
        audit_refused(QStringLiteral("no public price for this symbol"));
        order_entry_->show_order_status(
            tr("%1 has no public price, so a paper order cannot be simulated. %2").arg(shown, target.unsupported_reason),
            false);
        return;
    }
    PtPortfolio pf;
    try {
        pf = pt_get_portfolio(pid);
    } catch (const std::exception& e) {
        LOG_ERROR(eqpp_kTag, logf("Paper order aborted, account unavailable", {{"portfolio", log_mask(pid)},
                                                                              {"error", e.what()}}));
        audit_refused(QStringLiteral("paper account unavailable"));
        order_entry_->show_order_status(tr("Paper account unavailable: %1").arg(e.what()), false);
        return;
    }
    const auto inst = instrument_of(order.symbol);
    const services::QuoteData detail = quote_detail_.value(order.symbol);
    QString quote_ccy = detail.currency;
    if (quote_ccy.isEmpty())
        quote_ccy = inst.currency;
    if (quote_ccy.isEmpty())
        quote_ccy = mk::currency_for_symbol(target.yahoo);
    const auto fx = PaperMarkService::fx_factor(quote_ccy, pf.currency);
    if (!fx) {
        audit_refused(QStringLiteral("exchange rate missing (%1 to %2)").arg(quote_ccy, pf.currency));
        order_entry_->show_order_status(
            tr("The %1 \xE2\x86\x92 %2 exchange rate has not arrived yet (it is being fetched); paper cash is booked in "
               "%2. Try again in a few seconds.")
                .arg(quote_ccy.isEmpty() ? tr("price currency") : quote_ccy, pf.currency),
            false);
        return;
    }

    const PtDelayedQuote q = delayed_quote_for(order.symbol);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    PtSimRequest req;
    req.portfolio_id = pid;
    req.symbol = order.symbol;
    req.side = order.side == OrderSide::Buy ? QStringLiteral("buy") : QStringLiteral("sell");
    req.order_type = QString::fromLatin1(order_type_str(order.order_type));
    req.quantity = order.quantity;
    if ((order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) && order.price > 0.0)
        req.price = order.price;
    if ((order.order_type == OrderType::StopLoss || order.order_type == OrderType::StopLossLimit) &&
        order.stop_price > 0.0)
        req.stop_price = order.stop_price;
    req.exchange = inst.venue.isEmpty() ? order.exchange : inst.venue;
    req.product = QString::fromLatin1(product_to_broker_str(order.product_type));
    req.fx_to_account = *fx;

    if (confirm && !skip_paper_confirm_) {
        const bool is_market = order.order_type == OrderType::Market;
        const bool fresh = q.valid() && now - q.received_ms <= kPtDelayedQuoteMaxAgeMs;
        fmt::InstrumentFmt pfmt{quote_ccy, detail.price_digits, 0, false};
        QString outcome;
        if (is_market) {
            outcome = !q.valid()          ? tr("No delayed price has arrived yet \xE2\x80\x94 this order will be refused.")
                      : !q.session_open   ? tr("The market is closed \xE2\x80\x94 the order is queued and fills at the "
                                               "first delayed price printed after the open.")
                      : fresh             ? tr("Fills now at the latest delayed price (%1).")
                                    .arg(fmt::price_with_symbol(q.price, pfmt))
                                          : tr("The delayed price is not updating \xE2\x80\x94 this order will be refused.");
        } else {
            outcome = q.session_open ? tr("Rests and is checked against every new delayed price; a price already "
                                          "through the limit fills it now, at the limit.")
                                     : tr("The market is closed \xE2\x80\x94 the order rests and is checked from the open.");
        }
        QString price_note;
        if (q.valid()) {
            QString when = q.printed_ms > 0 ? QDateTime::fromMSecsSinceEpoch(q.printed_ms).toString(QStringLiteral("ddd HH:mm"))
                                            : tr("unknown time");
            price_note = detail.exchange_delay_min > 0
                             ? tr("Price source: Yahoo Finance public data, delayed %1 min \xC2\xB7 last trade %2 (local time).")
                                   .arg(detail.exchange_delay_min)
                                   .arg(when)
                             : tr("Price source: Yahoo Finance public data \xC2\xB7 last trade %1 (local time).").arg(when);
        }
        const double ref_px = req.price.value_or(q.price);
        QString cash_note;
        if (ref_px > 0.0) {
            // The order value in the account currency, with the rate and its time
            // (pence in a GBP book is only rescaled: no rate to show).
            const fmt::ConvertedMoney c =
                services::fx::convert_for_display(order.quantity * ref_px, quote_ccy, pf.currency);
            if (c.converted && c.ok())
                cash_note = tr("Cash is booked in %1: about %2 (%3; the fill uses the rate at that time).")
                                .arg(pf.currency, fmt::converted_money(c), fmt::fx_note(c));
            else
                cash_note = tr("Order value about %1.").arg(fmt::money(order.quantity * ref_px * *fx, pf.currency));
        }
        bool skip = false;
        if (!OrderConfirmDialog::confirm_simulated(this, order, account.display_name, q.price, price_note, outcome,
                                                   cash_note, &skip, quote_ccy, detail.price_digits)) {
            order_entry_->show_order_status(tr("Order cancelled"), false);
            return;
        }
        if (skip) {
            skip_paper_confirm_ = true;
            SettingsRepository::instance().set(QStringLiteral("equity_trading.skip_paper_confirm"), QStringLiteral("1"),
                                               QStringLiteral("ui"));
        }
    }

    // Provenance (audit "source"), captured after the confirmation: the agent's while an
    // agent pressed BUY / SELL, else the ticket.
    req.source = capture_order_source(QStringLiteral("ticket"));
    const PtSimResult res = pt_submit_delayed_order(req, q, now);
    if (res.outcome != PtSimOutcome::Rejected && !res.order.id.isEmpty() &&
        (order.stop_loss > 0.0 || order.take_profit > 0.0))
        OrderMatcher::instance().set_sl_tp(pid, order.symbol, res.order.id, order.stop_loss, order.take_profit);
    // The order is audited by the paper engine (Order accepted / filled / rejected).
    LOG_DEBUG(eqpp_kTag, QString("Simulated %1 %2 x%3 (%4): %5")
                             .arg(req.side, order.symbol)
                             .arg(order.quantity)
                             .arg(req.order_type, res.message));
    order_entry_->show_order_status(res.message, res.outcome != PtSimOutcome::Rejected);
    PaperMarkService::instance().resync(); // subscribe a newly held / resting symbol at once
    refresh_paper_panels();
}

bool EquityTradingScreen::simulated_close(const QString& symbol, const QString& product, double quantity,
                                          QString* message) {
    const auto account = AccountManager::instance().get_account(focused_account_id_);
    const QString pid = account.paper_portfolio_id;
    if (pid.isEmpty()) {
        if (message)
            *message = tr("No paper portfolio for this account");
        return false;
    }
    std::optional<PtPosition> pos;
    for (const auto& p : pt_get_positions(pid)) {
        if (p.symbol != symbol || p.quantity <= 0.0)
            continue;
        if (!product.isEmpty() && p.product.compare(product, Qt::CaseInsensitive) != 0)
            continue;
        pos = p;
        break;
    }
    if (!pos) {
        if (message)
            *message = tr("%1: no open paper position").arg(symbol);
        return false;
    }
    PtDelayedQuote q = delayed_quote_for(symbol);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!q.valid()) {
        // No delayed quote in hand: an exit must still be possible — use the last mark,
        // with the venue's own session state.
        q.price = pos->current_price > 0.0 ? pos->current_price : pos->entry_price;
        q.received_ms = now;
        const QString yahoo = public_target(symbol).yahoo;
        if (const mk::Venue* v = mk::venue_for_symbol(yahoo.isEmpty() ? symbol : yahoo))
            q.session_open = mk::session_status(*v, QDateTime::currentDateTimeUtc()).state == mk::SessionState::Open;
    }
    PtSimRequest req;
    req.portfolio_id = pid;
    req.symbol = symbol;
    req.side = position_is_short(pos->side, pos->quantity) ? QStringLiteral("buy") : QStringLiteral("sell");
    req.order_type = QStringLiteral("market");
    req.quantity = quantity > 0.0 ? std::min(quantity, pos->quantity) : pos->quantity;
    req.reduce_only = true;
    req.product = pos->product;
    req.exchange = instrument_of(symbol).venue;
    // A close books at the newest known factor (the FX feed may lag; never block an
    // exit on a lagging rate). A cross-currency position with no factor at all (none
    // since this launch and no rate yet) is not closed at 1:1 — that would book a
    // pence or yen P&L as if it were the account currency; the message says why.
    QString quote_ccy = quote_detail_.value(symbol).currency;
    if (quote_ccy.isEmpty())
        quote_ccy = instrument_of(symbol).currency;
    QString acct_ccy = account_currency_;
    if (acct_ccy.isEmpty()) {
        try {
            acct_ccy = pt_get_portfolio(pid).currency;
        } catch (...) {
            LOG_DEBUG(eqpp_kTag, logf("Paper portfolio currency could not be read", {{"portfolio", log_mask(pid)}}));
        }
    }
    const auto fx = acct_ccy.isEmpty() ? std::optional<double>() : PaperMarkService::fx_factor(quote_ccy, acct_ccy);
    if (fx) {
        req.fx_to_account = *fx;
    } else if (pt_has_symbol_fx(pid, symbol) || quote_ccy.isEmpty() || acct_ccy.isEmpty()) {
        req.fx_to_account = pt_get_symbol_fx(pid, symbol); // last booked factor (1.0 = same currency)
    } else {
        LOG_AUDIT("Orders", "Paper position close refused, exchange rate missing",
                  {{"venue", "paper"}, {"mode", "paper"}, {"portfolio", log_mask(pid)}, {"symbol", symbol},
                   {"from", quote_ccy}, {"to", acct_ccy}});
        if (message)
            *message = tr("%1: the %2 to %3 exchange rate has not arrived yet (it is being fetched), so the close "
                          "cannot be booked in %3. Try again in a few seconds.")
                           .arg(symbol, quote_ccy, acct_ccy);
        return false;
    }
    // A stale delayed quote never blocks an exit while the market is open: closes use it as the mark.
    req.source = capture_order_source(QString()); // an agent's close is marked; a person's states none
    const PtSimResult res = pt_submit_delayed_order(req, q, now, std::numeric_limits<qint64>::max() / 4);
    if (message)
        *message = res.message;
    return res.outcome != PtSimOutcome::Rejected;
}

void EquityTradingScreen::sync_public_paper_tracking() {
    const bool want = !focused_is_builtin() && focused_is_paper_ && public_mode_ && !focused_paper_portfolio_id_.isEmpty();
    const QString pid = want ? focused_paper_portfolio_id_ : QString();
    if (pid == public_tracked_portfolio_)
        return;
    auto& pms = PaperMarkService::instance();
    if (!public_tracked_portfolio_.isEmpty())
        pms.untrack_public_portfolio(public_tracked_portfolio_);
    public_tracked_portfolio_ = pid;
    if (pid.isEmpty())
        return;
    QString ccy;
    try {
        ccy = pt_get_portfolio(pid).currency;
    } catch (...) {
    }
    // Broker paper positions are stored with broker symbols: map them through their
    // exchange, the same way the public feed does for the watchlist.
    const QHash<QString, QString> exchanges = watchlist_exchanges_;
    const QString default_exchange = selected_exchange_;
    pms.track_public_portfolio(pid, ccy, [exchanges, default_exchange](const QString& s) {
        const QString ex = exchanges.value(s, default_exchange);
        QString y = ci::canonical_symbol(s, ex);
        if (y.isEmpty())
            y = EquityPublicFeed::yahoo_symbol(s, ex).ticker;
        return y;
    });
    LOG_INFO(eqpp_kTag, QString("Paper portfolio %1 now marks and fills on delayed public prices (broker feed down)").arg(pid));
}

// ============================================================================
// Instruments
// ============================================================================

ci::Instrument EquityTradingScreen::instrument_of(const QString& symbol) const {
    const auto it = instruments_.constFind(symbol);
    if (it != instruments_.constEnd())
        return *it;
    if (canonical_mode()) {
        ci::Instrument inst;
        inst.symbol = symbol;
        return eq_complete(inst);
    }
    return eq_instrument_for_stored(symbol, exchange_for_symbol(symbol));
}

QString EquityTradingScreen::display_of(const QString& symbol) const {
    const auto inst = instrument_of(symbol);
    const QString disp = canonical_mode() && !inst.display.isEmpty() ? inst.display : symbol;
    const QString venue = canonical_mode() ? inst.venue : exchange_for_symbol(symbol);
    return venue.isEmpty() ? disp : disp + QStringLiteral(" \xC2\xB7 ") + venue;
}

EquityPublicFeed::Target EquityTradingScreen::public_target(const QString& symbol) const {
    EquityPublicFeed::Target t;
    t.symbol = symbol;
    if (canonical_mode()) {
        t.yahoo = instrument_of(symbol).symbol;
        if (t.yahoo.isEmpty())
            t.unsupported_reason = tr("%1 has no public market data (Yahoo Finance has no symbol for it).").arg(symbol);
        return t;
    }
    const QString ex = exchange_for_symbol(symbol);
    t.yahoo = ci::canonical_symbol(symbol, ex);
    if (t.yahoo.isEmpty()) {
        const auto legacy = EquityPublicFeed::yahoo_symbol(symbol, ex);
        t.yahoo = legacy.ticker;
        t.unsupported_reason = legacy.unsupported_reason;
    }
    return t;
}

QString EquityTradingScreen::broker_block_reason(const QString& symbol) const {
    if (canonical_mode())
        return {};
    return row_block_reason_.value(symbol);
}

QStringList EquityTradingScreen::stream_symbols() const {
    QStringList out;
    for (const auto& s : effective_symbols())
        if (!row_block_reason_.contains(s))
            out << s;
    return out;
}

void EquityTradingScreen::schedule_row_info() {
    if (row_info_queued_)
        return;
    row_info_queued_ = true;
    QTimer::singleShot(250, this, [this]() {
        row_info_queued_ = false;
        refresh_row_info();
    });
}

void EquityTradingScreen::refresh_row_info() {
    QHash<QString, EquityWatchlist::RowInfo> info;
    QHash<QString, EquityBottomPanel::SymbolMeta> meta;
    QStringList syms = effective_symbols();
    if (!syms.contains(selected_symbol_))
        syms << selected_symbol_;
    const QString pid = focused_paper_portfolio_id_;
    for (const auto& sym : syms) {
        const auto inst = instrument_of(sym);
        const auto detail = quote_detail_.constFind(sym);
        EquityWatchlist::RowInfo r;
        r.display = canonical_mode() && !inst.display.isEmpty() ? inst.display : sym;
        r.venue = canonical_mode() ? inst.venue : exchange_for_symbol(sym);
        QStringList tip;
        if (!inst.name.isEmpty())
            tip << inst.name;
        if (!inst.symbol.isEmpty())
            tip << inst.symbol;
        if (!inst.currency.isEmpty())
            tip << inst.currency;
        r.tooltip = tip.join(QStringLiteral(" \xC2\xB7 "));
        r.currency = detail != quote_detail_.constEnd() && !detail->currency.isEmpty() ? detail->currency : inst.currency;
        r.digits = detail != quote_detail_.constEnd() ? detail->price_digits : -1;
        r.market_key = public_target(sym).yahoo;
        r.blocked_reason = broker_block_reason(sym);
        info.insert(sym, r);
        if (canonical_mode() || simulated_on_delayed_prices()) {
            EquityBottomPanel::SymbolMeta m;
            m.currency = r.currency;
            m.digits = r.digits;
            // NaN while the rate to the account currency is missing (never 1:1).
            m.fx = pid.isEmpty() ? 1.0 : PaperMarkService::display_fx(pid, sym, m.currency, account_currency_);
            meta.insert(sym, m);
        }
    }
    // Held symbols that are not on the list: their currency for the blotter.
    if (canonical_mode() && !pid.isEmpty()) {
        for (const auto& p : pt_get_positions(pid)) {
            if (meta.contains(p.symbol))
                continue;
            EquityBottomPanel::SymbolMeta m;
            m.currency = instrument_of(p.symbol).currency;
            m.fx = PaperMarkService::display_fx(pid, p.symbol, m.currency, account_currency_);
            meta.insert(p.symbol, m);
        }
    }
    watchlist_->set_row_info(info);
    bottom_panel_->set_symbol_meta(meta);
}

void EquityTradingScreen::apply_selected_instrument_ui() {
    const auto inst = instrument_of(selected_symbol_);
    const auto detail = quote_detail_.value(selected_symbol_);
    if (canonical_mode()) {
        ci::Instrument shown = inst;
        if (shown.display.isEmpty())
            shown.display = selected_symbol_;
        symbol_box_->set_instrument(shown);
    } else {
        symbol_box_->set_text(selected_symbol_);
    }
    // Instrument line: venue · name · currency.
    QStringList parts;
    const QString venue = canonical_mode() ? inst.venue : selected_exchange_;
    if (!venue.isEmpty())
        parts << venue;
    if (!inst.name.isEmpty())
        parts << inst.name;
    const QString ccy = !detail.currency.isEmpty() ? detail.currency : inst.currency;
    if (!ccy.isEmpty())
        parts << ccy;
    set_instrument_line(parts.join(QStringLiteral(" \xC2\xB7 ")), venue);
    chart_->set_title(display_of(selected_symbol_)); // panel header: what the chart shows

    const bool public_prices = public_mode_ || canonical_mode();
    ticker_bar_->set_price_format(public_prices ? ccy : QString(), public_prices ? detail.price_digits : -1);
    order_entry_->set_symbol(selected_symbol_);
    order_entry_->set_exchange(canonical_mode() ? inst.venue : selected_exchange_);
    order_entry_->set_symbol_display(display_of(selected_symbol_));
    if (canonical_mode()) {
        const auto tm = eq_trading_meta(inst);
        order_entry_->set_instrument_meta(ccy, detail.price_digits, tm.lot_size, tm.lot_known);
    }
    order_entry_->set_trade_block(
        broker_block_reason(selected_symbol_).isEmpty()
            ? QString()
            : tr("%1 \xE2\x80\x94 switch to the paper account (no broker) to paper-trade it.")
                  .arg(broker_block_reason(selected_symbol_)));
    if (snapshot_ && !quote_detail_.contains(selected_symbol_))
        snapshot_->set_waiting(display_of(selected_symbol_), tr("waiting for the delayed quote\xE2\x80\xA6"));
    update_session_lines();
}

void EquityTradingScreen::apply_quote_detail(const QString& symbol) {
    if (symbol != selected_symbol_)
        return;
    const auto& q = quote_detail_[symbol];
    const qint64 rx = quote_rx_ms_.value(symbol);
    if (snapshot_)
        snapshot_->set_quote(display_of(symbol), q, rx, q.price_digits);
    ticker_bar_->set_price_format(q.currency, q.price_digits);
    if (simulated_on_delayed_prices()) {
        QString tz;
        if (!q.exchange_timezone.isEmpty() && q.market_time > 0) {
            const QTimeZone zone(q.exchange_timezone.toUtf8());
            if (zone.isValid())
                tz = zone.abbreviation(QDateTime::fromSecsSinceEpoch(q.market_time, QTimeZone::UTC));
        }
        order_entry_->set_price_source(true, q.exchange_delay_min, q.market_time > 0 ? q.market_time * 1000 : 0, tz,
                                       kPtDelayedQuoteMaxAgeMs);
        const auto tm = eq_trading_meta(instrument_of(symbol));
        order_entry_->set_instrument_meta(q.currency, q.price_digits, tm.lot_size, tm.lot_known);
    }
}

// ============================================================================
// Chrome: status bar, session lines, right column, first-run note, splitters
// ============================================================================

void EquityTradingScreen::update_session_lines() {
    const QString yahoo = public_target(selected_symbol_).yahoo;
    const auto inst = instrument_of(selected_symbol_);
    const mk::Venue* v = mk::venue_for_symbol(yahoo.isEmpty() ? selected_symbol_ : yahoo, inst.venue);
    if (!v) {
        order_entry_->set_session_line(QString(), true);
        if (snapshot_)
            snapshot_->set_session(QString(), true);
        status_bar_->set_item(QStringLiteral("session"), QString(), ui::ScreenStatusBar::Tone::Normal);
        cmd_bar_->set_session(QString(), QString(), false);
        return;
    }
    const auto st = mk::session_status(*v, QDateTime::currentDateTimeUtc());
    const bool open = st.state == mk::SessionState::Open;
    QString text;
    if (v->is_24x7()) {
        text = tr("%1 \xC2\xB7 trades 24/7").arg(mk::venue_display_name(*v));
    } else {
        QString when;
        if (st.next_change_utc.isValid()) {
            const QDateTime local = st.next_change_utc.toTimeZone(v->tz);
            const bool today = local.date() == st.venue_local_now.date();
            when = local.toString(today ? QStringLiteral("HH:mm") : QStringLiteral("ddd HH:mm")) + QLatin1Char(' ') +
                   v->tz.abbreviation(local);
        }
        text = open ? tr("%1 OPEN \xC2\xB7 closes %2").arg(v->id, when)
                    : tr("%1 %2 \xC2\xB7 next change %3")
                          .arg(v->id, mk::session_state_label(st.state).toUpper(), when.isEmpty() ? tr("unknown") : when);
    }
    if (snapshot_)
        snapshot_->set_session(text, open);
    // Status bar: the sentence; command bar: the state at a glance (word, not colour alone).
    status_bar_->set_item(QStringLiteral("session"), text,
                          open ? ui::ScreenStatusBar::Tone::Ok : ui::ScreenStatusBar::Tone::Normal);
    const QString chip = v->is_24x7() ? tr("24/7") : open ? tr("OPEN") : mk::session_state_label(st.state).toUpper();
    cmd_bar_->set_session(chip, text, open);
    order_entry_->set_session_line(simulated_on_delayed_prices() && !open && !v->is_24x7()
                                       ? text + tr(" \xC2\xB7 MARKET orders queue for the open")
                                       : QString(),
                                   open);
}

void EquityTradingScreen::update_right_column() {
    if (!snapshot_ || !orderbook_)
        return;
    // One quote zone above the ticket: the broker's depth ladder, or — without a live
    // broker — the delayed public snapshot (never an empty ladder). The ticket below
    // takes the rest of the column; both keep their own content height.
    const bool depth = !public_mode_; // depth comes only from a live broker stream
    if (orderbook_->isVisibleTo(workspace_) != depth)
        orderbook_->setVisible(depth);
    if (snapshot_->isVisibleTo(workspace_) == depth)
        snapshot_->setVisible(!depth);
    if (!depth)
        snapshot_->set_depth_note(focused_is_builtin()
                                      ? tr("Market depth needs a live broker \xE2\x80\x94 connect one in ACCOUNTS.")
                                      : tr("No market depth: %1.").arg(public_feed_reason()));
}

void EquityTradingScreen::update_first_run_note() {
    if (!first_run_note_)
        return;
    auto r = SettingsRepository::instance().get(QStringLiteral("equity_trading.first_run_note_dismissed"));
    const bool dismissed = r.is_ok() && r.value() == QLatin1String("1");
    first_run_note_->setVisible(focused_is_builtin() && !dismissed);
}

void EquityTradingScreen::update_status_bar() {
    if (!status_bar_)
        return;
    using SB = ui::ScreenStatusBar;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // ── Data source + delay, and freshness ──
    if (public_mode_) {
        int total = 0, current = 0;
        qint64 newest = 0;
        for (const auto& s : effective_symbols()) {
            ++total;
            const qint64 rx = quote_rx_ms_.value(s);
            newest = std::max(newest, rx);
            if (rx > 0 && now - rx <= kPtDelayedQuoteMaxAgeMs)
                ++current;
        }
        const int delay = quote_detail_.value(selected_symbol_).exchange_delay_min;
        status_bar_->set_item(QStringLiteral("source"),
                              delay > 0 ? tr("\xE2\x97\x8F YAHOO FINANCE \xC2\xB7 DELAYED %1 MIN").arg(delay)
                                        : tr("\xE2\x97\x8F YAHOO FINANCE \xC2\xB7 PUBLIC DATA"),
                              SB::Tone::Info);
        status_bar_->set_tooltip(QStringLiteral("source"),
                                 tr("Prices, ranges, volume and charts: public market data from Yahoo Finance, "
                                    "typically delayed 15 minutes. No depth without a live broker."));
        status_bar_->set_item_visible(QStringLiteral("fresh"), true);
        status_bar_->set_text(QStringLiteral("fresh"),
                              tr("%1 \xC2\xB7 %2/%3 symbols current")
                                  .arg(newest > 0 ? tr("updated %1").arg(eqpp_age(now - newest)) : tr("waiting for prices"))
                                  .arg(current)
                                  .arg(total));
    } else {
        const QString broker = feed_state_.broker_name.isEmpty() ? tr("broker") : feed_state_.broker_name;
        status_bar_->set_item(QStringLiteral("source"), tr("\xE2\x97\x8F %1 \xC2\xB7 LIVE FEED").arg(broker.toUpper()),
                              SB::Tone::Ok);
        status_bar_->set_tooltip(QStringLiteral("source"), tr("Prices and depth from the %1 live feed").arg(broker));
        status_bar_->set_item_visible(QStringLiteral("fresh"), false);
    }
    // ── Account + currency (PAPER / LIVE in words and the state tone) ──
    const auto acct = AccountManager::instance().get_account(focused_account_id_);
    if (acct.account_id.isEmpty()) {
        status_bar_->set_item(QStringLiteral("account"), tr("No account"), SB::Tone::Dim);
        status_bar_->set_item_visible(QStringLiteral("funds"), false);
        status_bar_->set_item_visible(QStringLiteral("pnl"), false);
        return;
    }
    const bool live = !focused_is_builtin() && acct.trading_mode == QLatin1String("live");
    const QString ccy = have_funds_view_ ? last_funds_view_.currency : account_currency_;
    status_bar_->set_item(QStringLiteral("account"),
                          QStringLiteral("%1 \xC2\xB7 %2%3")
                              .arg(acct.display_name, live ? tr("LIVE") : tr("PAPER"),
                                   ccy.isEmpty() ? QString() : QStringLiteral(" \xC2\xB7 ") + ccy),
                          live ? SB::Tone::Danger : SB::Tone::Ok);
    status_bar_->set_tooltip(QStringLiteral("account"),
                             live ? tr("LIVE account: orders from this screen reach the broker")
                                  : tr("Paper account: every fill is simulated, nothing reaches a broker"));
    if (!have_funds_view_) {
        status_bar_->set_item_visible(QStringLiteral("funds"), false);
        status_bar_->set_item_visible(QStringLiteral("pnl"), false);
        return;
    }
    const auto& f = last_funds_view_;
    // Equity / open P&L that leave out positions without an FX rate say so.
    const QString partial = f.fx_missing > 0 ? tr(" (excl. %n without an FX rate)", nullptr, f.fx_missing) : QString();
    status_bar_->set_item_visible(QStringLiteral("funds"), true);
    status_bar_->set_text(QStringLiteral("funds"), tr("Buying power %1 \xC2\xB7 Equity %2")
                                                       .arg(fmt::money(f.available, f.currency),
                                                            fmt::money(f.total_equity, f.currency) + partial));
    status_bar_->set_tooltip(QStringLiteral("funds"),
                             tr("Cash / available margin %1 \xC2\xB7 used margin %2 \xC2\xB7 equity %3")
                                 .arg(fmt::money(f.available, f.currency), fmt::money(f.used_margin, f.currency),
                                      fmt::money(f.total_equity, f.currency)));
    // P&L: sign + arrow in the text, direction colour as a second cue.
    const double pnl = f.unrealized_pnl;
    const QString arrow = pnl > 0 ? QStringLiteral("\xE2\x96\xB2 ") : pnl < 0 ? QStringLiteral("\xE2\x96\xBC ") : QString();
    status_bar_->set_item_visible(QStringLiteral("pnl"), true);
    status_bar_->set_item(QStringLiteral("pnl"),
                          tr("Open P&L %1%2").arg(arrow, fmt::signed_money(pnl, f.currency) + partial),
                          pnl > 0 ? SB::Tone::Positive : pnl < 0 ? SB::Tone::Negative : SB::Tone::Normal);
}

QVariantMap EquityTradingScreen::splitter_state() const {
    // Splitter sizes per width class, the rail choice and the compact zone tab.
    return workspace_ ? workspace_->save_layout() : QVariantMap{};
}

void EquityTradingScreen::restore_splitters(const QVariantMap& s) {
    // Saved sizes that would squeeze a zone under its floor are ignored per class
    // (eq_restore_sizes); the defaults then apply.
    if (workspace_)
        workspace_->restore_layout(s);
}

// ============================================================================
// Symbol search + watchlist row actions
// ============================================================================

void EquityTradingScreen::on_symbol_picked(const ci::Instrument& picked) {
    if (!picked.is_valid())
        return;
    const ci::Instrument inst = eq_complete(picked);
    if (canonical_mode()) {
        if (inst.symbol.isEmpty()) {
            order_entry_->show_status_message(
                tr("%1 has no public market data, so it cannot be viewed or paper-traded here.").arg(inst.display),
                false);
            return;
        }
        instruments_.insert(inst.symbol, inst);
        on_symbol_selected(inst.symbol);
        return;
    }
    // A broker account is focused: the broker's own symbol for this listing.
    const QString bid = broker_id_for_focused();
    QString exch;
    const QString bsym = ci::to_broker_symbol(inst, bid, &exch);
    QStringList exchanges;
    QString broker_name = bid;
    if (auto* b = BrokerRegistry::instance().get(bid)) {
        exchanges = b->profile().exchanges;
        broker_name = b->profile().display_name;
    }
    if (bsym.isEmpty() || (!exchanges.isEmpty() && !exchanges.contains(exch, Qt::CaseInsensitive))) {
        order_entry_->show_status_message(tr("%1 (%2) is not tradable on %3. Switch to the paper account (no broker) "
                                             "to view and paper-trade it.")
                                              .arg(inst.display.isEmpty() ? inst.symbol : inst.display,
                                                   inst.venue.isEmpty() ? inst.symbol : inst.venue, broker_name),
                                          false);
        apply_selected_instrument_ui(); // put the box back on the current symbol
        return;
    }
    instruments_.insert(bsym, inst);
    watchlist_exchanges_.insert(bsym, exch);
    if (exch != selected_exchange_) {
        selected_exchange_ = exch;
        order_entry_->set_exchange(exch);
    }
    on_symbol_selected(bsym);
}

void EquityTradingScreen::on_watchlist_research(const QString& symbol) {
    const QString key = public_target(symbol).yahoo;
    if (key.isEmpty())
        return;
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", QStringLiteral("equity_research")},
                                                     {"symbol", key},
                                                     {"exchange", instrument_of(symbol).venue}});
}

void EquityTradingScreen::on_watchlist_news(const QString& symbol) {
    const QString key = public_target(symbol).yahoo;
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", QStringLiteral("news")},
                                                     {"symbol", key.isEmpty() ? symbol : key},
                                                     {"exchange", instrument_of(symbol).venue}});
}

void EquityTradingScreen::on_watchlist_alert(const QString& symbol) {
    const QString key = public_target(symbol).yahoo;
    if (key.isEmpty())
        return;
    const double last = quote_detail_.value(symbol).price;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Price Alert \xE2\x80\x94 %1").arg(display_of(symbol)));
    auto* form = new QFormLayout(&dlg);
    auto* kind = new QComboBox(&dlg);
    kind->addItem(tr("Price rises above"), QStringLiteral("price_above"));
    kind->addItem(tr("Price falls below"), QStringLiteral("price_below"));
    form->addRow(tr("When"), kind);
    auto* level = new QDoubleSpinBox(&dlg);
    level->setRange(0.000001, 1.0e9);
    level->setDecimals(std::max(2, quote_detail_.value(symbol).price_digits));
    level->setValue(last > 0 ? last * 1.02 : 1.0);
    form->addRow(tr("Level"), level);
    connect(kind, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, [kind, level, last](int) {
        if (last > 0)
            level->setValue(kind->currentData().toString() == QLatin1String("price_above") ? last * 1.02 : last * 0.98);
    });
    auto* note = new QLabel(tr("Checked on public quotes by the Watchlist alert service."), &dlg);
    note->setWordWrap(true);
    form->addRow(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);
    if (dlg.exec() != QDialog::Accepted)
        return;
    fincept::WatchlistAlert a;
    a.symbol = key;
    a.watchlist_id = active_watchlist_id_;
    a.kind = kind->currentData().toString();
    a.threshold = level->value();
    a.enabled = true;
    const auto r = fincept::WatchlistRepository::instance().save_alert(a);
    order_entry_->show_status_message(r.is_ok() ? tr("Alert saved for %1").arg(display_of(symbol))
                                                : tr("Could not save the alert"),
                                      r.is_ok());
}

} // namespace fincept::screens
