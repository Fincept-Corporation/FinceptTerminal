// crypto_trading_screen.cpp — split into focused files.
// This file is intentionally empty; all implementations live in:
//   crypto_lifecycle.cpp    — init, destructor, exchange/symbol switching, WS, data fetch
//   crypto_screen.cpp       — render(), render_top_nav, render_ticker_bar, render_status_bar
//   crypto_watchlist.cpp    — render_watchlist
//   crypto_chart.cpp        — render_chart_area, render_candlestick_chart, render_bottom_panel
//   crypto_bottom_tabs.cpp  — positions/orders/history/stats/trades/market_info tabs
//   crypto_order_entry.cpp  — render_order_entry, render_orderbook, OB view modes
//   crypto_order_actions.cpp — submit_order, cancel_order, live data fetchers
//   crypto_credentials.cpp  — load/save/has credentials, render_credentials_popup
