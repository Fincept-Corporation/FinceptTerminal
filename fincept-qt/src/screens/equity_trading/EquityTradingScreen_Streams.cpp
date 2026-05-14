// src/screens/equity_trading/EquityTradingScreen_Streams.cpp
//
// DataStream broker→UI slots: per-stream callbacks that propagate quote,
// watchlist, positions, holdings, orders, funds, candles, orderbook,
// time/sales, latest trade, calendar, and market clock updates into the
// screen widgets.
//
// Part of the partial-class split of EquityTradingScreen.cpp.

#include "screens/equity_trading/EquityTradingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/equity_trading/BroadcastOrderDialog.h"
#include "screens/equity_trading/EquityBottomPanel.h"
#include "screens/equity_trading/EquityChart.h"
#include "screens/equity_trading/EquityOrderBook.h"
#include "screens/equity_trading/EquityOrderEntry.h"
#include "screens/equity_trading/EquityTickerBar.h"
#include "screens/equity_trading/EquityWatchlist.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStringListModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <memory>

#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::equity;

void EquityTradingScreen::on_stream_quote_updated(const QString& account_id, const QString& symbol,
                                                   const BrokerQuote& quote) {
    // Update UI only for focused account's selected symbol
    if (account_id == focused_account_id_ && symbol == selected_symbol_) {
        current_price_ = quote.ltp;
        ticker_bar_->update_quote(quote.ltp, quote.change, quote.change_pct, quote.high, quote.low,
                                  quote.volume, quote.bid, quote.ask);
        order_entry_->set_current_price(quote.ltp);
    }

    // Feed ALL account quotes into paper trading engine for order matching
    auto account = AccountManager::instance().get_account(account_id);
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty() && quote.ltp > 0) {
        pt_update_position_price(account.paper_portfolio_id, symbol, quote.ltp);
        PriceData pd;
        pd.last = quote.ltp;
        pd.bid = quote.bid;
        pd.ask = quote.ask;
        pd.timestamp = quote.timestamp;
        OrderMatcher::instance().check_orders(symbol, pd, account.paper_portfolio_id);
        OrderMatcher::instance().check_sl_tp_triggers(account.paper_portfolio_id, symbol, quote.ltp);
    }
}

void EquityTradingScreen::on_stream_watchlist_updated(const QString& account_id,
                                                       const QVector<BrokerQuote>& quotes) {
    if (account_id == focused_account_id_)
        watchlist_->update_quotes(quotes);

    // Feed into paper trading for all accounts
    auto account = AccountManager::instance().get_account(account_id);
    if (account.trading_mode == "paper" && !account.paper_portfolio_id.isEmpty()) {
        for (const auto& q : quotes) {
            if (q.ltp <= 0)
                continue;
            pt_update_position_price(account.paper_portfolio_id, q.symbol, q.ltp);
            PriceData pd;
            pd.last = q.ltp;
            pd.bid = q.bid;
            pd.ask = q.ask;
            pd.timestamp = q.timestamp;
            OrderMatcher::instance().check_orders(q.symbol, pd, account.paper_portfolio_id);
        }
    }
}

void EquityTradingScreen::on_stream_positions_updated(const QString& account_id,
                                                       const QVector<BrokerPosition>& positions) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_positions(positions);
}

void EquityTradingScreen::on_stream_holdings_updated(const QString& account_id,
                                                      const QVector<BrokerHolding>& holdings) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_holdings(holdings);
}

void EquityTradingScreen::on_stream_orders_updated(const QString& account_id,
                                                    const QVector<BrokerOrderInfo>& orders) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_orders(orders);
}

void EquityTradingScreen::on_stream_funds_updated(const QString& account_id, const BrokerFunds& funds) {
    if (account_id == focused_account_id_) {
        bottom_panel_->set_funds(funds);
        order_entry_->set_balance(funds.available_balance);
    }
}

void EquityTradingScreen::on_stream_candles_fetched(const QString& account_id,
                                                     const QVector<BrokerCandle>& candles) {
    if (account_id == focused_account_id_)
        chart_->set_candles(candles);
}

void EquityTradingScreen::on_stream_orderbook_fetched(const QString& account_id,
                                                       const QVector<QPair<double, double>>& bids,
                                                       const QVector<QPair<double, double>>& asks,
                                                       double spread, double spread_pct) {
    if (account_id == focused_account_id_)
        orderbook_->set_data(bids, asks, spread, spread_pct);
}

void EquityTradingScreen::on_stream_time_sales_fetched(const QString& account_id,
                                                        const QVector<BrokerTrade>& trades) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_time_sales(trades);
}

void EquityTradingScreen::on_stream_latest_trade_fetched(const QString& account_id,
                                                          const BrokerTrade& trade) {
    if (account_id == focused_account_id_)
        bottom_panel_->prepend_trade(trade);
}

void EquityTradingScreen::on_stream_calendar_fetched(const QString& account_id,
                                                      const QVector<MarketCalendarDay>& days) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_calendar(days);
}

void EquityTradingScreen::on_stream_clock_fetched(const QString& account_id, const MarketClock& clock) {
    if (account_id == focused_account_id_)
        bottom_panel_->set_clock(clock);
}

// ============================================================================
// Slot Handlers
// ============================================================================

} // namespace fincept::screens
