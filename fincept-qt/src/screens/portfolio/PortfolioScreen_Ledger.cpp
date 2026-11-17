// src/screens/portfolio/PortfolioScreen_Ledger.cpp
//
// Ledger input flows of the portfolio screen (BUY / SELL with costs, dividends on
// the ex-date entitlement, cash movements, splits, portfolio settings), the metric
// requests (whole history + the ribbon's period) and the market-info refresh that
// feeds the blotter's descriptive columns.
//
// Part of the partial-class split of PortfolioScreen.cpp.

#include "core/market/InstrumentCurrency.h"
#include "screens/portfolio/PortfolioBlotter.h"
#include "screens/portfolio/PortfolioDetailWrapper.h"
#include "screens/portfolio/PortfolioDialogs.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "screens/portfolio/PortfolioLedgerDialogs.h"
#include "screens/portfolio/PortfolioScreen.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioStatsRibbon.h"
#include "screens/portfolio/PortfolioTxnPanel.h"
#include "services/portfolio/PortfolioService.h"

#include <QMessageBox>

namespace fincept::screens {

namespace {

QHash<QString, QString> pfs_quote_ccy(const portfolio::PortfolioSummary& s) {
    QHash<QString, QString> out;
    for (const auto& h : s.holdings)
        out.insert(h.symbol, h.price_currency.isEmpty() ? core::market::currency_for_symbol(h.symbol, h.exchange)
                                                        : h.price_currency);
    return out;
}

} // namespace

void PortfolioScreen::request_metrics() {
    if (!summary_loaded_)
        return;
    auto& svc = services::PortfolioService::instance();
    svc.compute_metrics(current_summary_, portfolio::PerfWindow::All);
    if (stats_ribbon_ && stats_ribbon_->window() != portfolio::PerfWindow::All)
        svc.compute_metrics(current_summary_, stats_ribbon_->window());
}

void PortfolioScreen::reload_transactions() {
    if (!txn_panel_ || selected_id_.isEmpty())
        return;
    txn_panel_->set_base_currency(pfmt::base_ccy(current_summary_));
    txn_panel_->set_transactions(services::PortfolioService::instance().ledger(selected_id_));
}

void PortfolioScreen::refresh_allocation() {
    if (!summary_loaded_ || !blotter_)
        return;
    auto& svc = services::PortfolioService::instance();
    const auto report = svc.allocation(current_summary_);
    blotter_->set_allocation(report);
    if (sector_panel_)
        sector_panel_->set_allocation(report);
    QHash<QString, services::InfoData> info;
    for (const auto& h : current_summary_.holdings)
        if (const auto i = svc.cached_info(h.symbol))
            info.insert(h.symbol, *i);
    blotter_->set_market_info(info);
}

void PortfolioScreen::open_buy_dialog(const QString& preset_symbol) {
    if (selected_id_.isEmpty())
        return;
    const QString base = pfmt::base_ccy(current_summary_);
    AddAssetDialog dlg(this);
    dlg.set_base_currency(base);
    if (const auto* h = find_holding(preset_symbol))
        dlg.preset(h->symbol, h->current_price);
    else if (!preset_symbol.isEmpty())
        dlg.preset(preset_symbol);
    if (dlg.exec() != QDialog::Accepted)
        return;
    portfolio::Transaction t;
    t.portfolio_id = selected_id_;
    t.symbol = dlg.symbol();
    t.transaction_type = portfolio::txn::kBuy;
    t.quantity = dlg.quantity();
    t.price = dlg.price();
    t.total_value = t.quantity * t.price;
    t.transaction_date = dlg.date();
    if (auto* c = dlg.costs()) {
        t.fees = c->fees();
        t.taxes = c->taxes();
        t.currency = c->currency();
        t.fx_rate = c->fx_rate();
    }
    services::PortfolioService::instance().record_transaction(t);
}

void PortfolioScreen::open_sell_dialog(const QString& symbol) {
    const auto* h = find_holding(symbol);
    if (!h)
        return;
    // Copy before exec(): a refresh during the modal loop replaces current_summary_.
    const QString sym = h->symbol;
    const double held = h->quantity;
    const double last = h->current_price;
    const QString qccy = h->price_currency;
    const QString base = pfmt::base_ccy(current_summary_);
    SellAssetDialog dlg(sym, held, this);
    dlg.set_currency_context(base, qccy);
    dlg.preset_price(last);
    if (dlg.exec() != QDialog::Accepted)
        return;
    portfolio::Transaction t;
    t.portfolio_id = selected_id_;
    t.symbol = sym;
    t.transaction_type = portfolio::txn::kSell;
    t.quantity = dlg.quantity();
    t.price = dlg.price();
    t.total_value = t.quantity * t.price;
    t.transaction_date = dlg.date();
    if (auto* c = dlg.costs()) {
        t.fees = c->fees();
        t.taxes = c->taxes();
        t.currency = c->currency();
        t.fx_rate = c->fx_rate();
    }
    services::PortfolioService::instance().record_transaction(t);
}

void PortfolioScreen::open_dividend_dialog() {
    if (selected_id_.isEmpty() || current_summary_.holdings.isEmpty())
        return;
    QStringList syms;
    QHash<QString, double> held_now;
    for (const auto& h : current_summary_.holdings) {
        syms.append(h.symbol);
        held_now.insert(h.symbol, h.quantity);
    }
    AddDividendDialog dlg(syms, this);
    if (!selected_symbol_.isEmpty() && syms.contains(selected_symbol_))
        dlg.set_symbol(selected_symbol_);
    dlg.set_context(pfmt::base_ccy(current_summary_), services::PortfolioService::instance().ledger(selected_id_),
                    held_now, pfs_quote_ccy(current_summary_));
    if (dlg.exec() != QDialog::Accepted)
        return;
    portfolio::Transaction t;
    t.portfolio_id = selected_id_;
    t.symbol = dlg.symbol();
    t.transaction_type = portfolio::txn::kDividend;
    t.quantity = dlg.quantity();
    t.price = dlg.amount_per_share();
    t.total_value = t.quantity * t.price;
    t.transaction_date = dlg.date();
    t.notes = dlg.notes();
    if (auto* c = dlg.costs()) {
        t.fees = c->fees();
        t.taxes = c->taxes();
        t.currency = c->currency();
        t.fx_rate = c->fx_rate();
    }
    services::PortfolioService::instance().record_transaction(t);
}

void PortfolioScreen::open_cash_dialog() {
    if (selected_id_.isEmpty())
        return;
    QStringList syms;
    for (const auto& h : current_summary_.holdings)
        syms.append(h.symbol);
    CashMovementDialog dlg(pfmt::base_ccy(current_summary_), current_summary_.portfolio.track_cash, syms, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    portfolio::Transaction t = dlg.transaction();
    t.portfolio_id = selected_id_;
    services::PortfolioService::instance().record_transaction(t);
}

void PortfolioScreen::open_split_dialog() {
    if (selected_id_.isEmpty() || current_summary_.holdings.isEmpty())
        return;
    SplitDialog dlg(current_summary_.holdings, selected_symbol_, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString sym = dlg.symbol();
    const double f = dlg.factor();
    // A split rewrites the position's quantity and every later lot: confirm it.
    const auto answer = QMessageBox::question(
        this, tr("Record Split"),
        tr("Record a split of %1 with factor %2 effective %3?\n\nThe position and its tax lots are re-derived from the "
           "ledger; the cost basis is unchanged. You can delete the SPLIT row later to undo it.")
            .arg(sym, pfmt::qty(f), pfmt::date(dlg.date())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    services::PortfolioService::instance().record_split(selected_id_, sym, f, dlg.date(), dlg.notes());
}

void PortfolioScreen::open_settings_dialog() {
    if (selected_id_.isEmpty())
        return;
    portfolio::Portfolio p = current_summary_.portfolio;
    for (const auto& x : portfolios_)
        if (x.id == selected_id_)
            p = x;
    PortfolioSettingsDialog dlg(p, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    services::PortfolioService::instance().update_portfolio_settings(
        selected_id_, dlg.benchmark(), dlg.risk_free_override(), dlg.track_cash(), dlg.lot_method(),
        dlg.long_term_days());
}

} // namespace fincept::screens
