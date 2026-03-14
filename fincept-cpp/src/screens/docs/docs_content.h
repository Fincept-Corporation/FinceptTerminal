#pragma once
// Static documentation content — all 5 sections with subsections

#include "docs_types.h"
#include <vector>

namespace fincept::docs {

inline std::vector<DocSection> get_doc_sections() {
    return {
        // =====================================================================
        // 1. FinScript Language
        // =====================================================================
        {
            "finscript", "FinScript Language", "[FS]",
            {
                {"fs-overview", "Overview", R"(FinScript is Fincept Terminal's built-in scripting language for creating custom trading strategies, indicators, and automated analysis workflows.

FinScript is designed to be:
- Easy to learn for traders and analysts
- Powerful enough for complex quantitative strategies
- Integrated directly with the terminal's data and charting systems

FinScript files use the .fs extension and can be loaded from the Code Editor tab.)", std::nullopt},

                {"fs-variables", "Variables & Types", R"(FinScript supports the following data types:

- number: Floating-point numeric values (prices, volumes, ratios)
- string: Text values (ticker symbols, labels)
- bool: Boolean true/false values
- series: Time-series data (price history, indicator values)
- array: Ordered collections of values

Variable declaration:
  let price = close
  let name = "AAPL"
  let is_bullish = close > open
  let prices = series(close, 20))", R"(// Example: Variable declarations
let sma_20 = sma(close, 20)
let sma_50 = sma(close, 50)
let is_golden_cross = sma_20 > sma_50

if is_golden_cross:
    signal("BUY", "Golden Cross detected"))"},

                {"fs-indicators", "Built-in Indicators", R"(FinScript includes a comprehensive library of technical indicators:

Moving Averages:
  sma(source, period) - Simple Moving Average
  ema(source, period) - Exponential Moving Average
  wma(source, period) - Weighted Moving Average

Oscillators:
  rsi(source, period) - Relative Strength Index
  macd(source, fast, slow, signal) - MACD
  stoch(high, low, close, k, d) - Stochastic

Volatility:
  bb(source, period, std) - Bollinger Bands
  atr(high, low, close, period) - Average True Range

Volume:
  vwap(close, volume) - Volume Weighted Average Price
  obv(close, volume) - On-Balance Volume)", std::nullopt},

                {"fs-signals", "Trading Signals", R"(Generate trading signals using the signal() function:

  signal(action, reason)
    action: "BUY", "SELL", "HOLD"
    reason: Description string for the signal

Signals appear in the Output panel and can trigger alerts when configured.

Conditional signals:
  if rsi(close, 14) < 30:
      signal("BUY", "RSI oversold")
  elif rsi(close, 14) > 70:
      signal("SELL", "RSI overbought"))", R"(// RSI-based signal strategy
let rsi_val = rsi(close, 14)

if rsi_val < 30:
    signal("BUY", "RSI oversold at " + str(rsi_val))
elif rsi_val > 70:
    signal("SELL", "RSI overbought at " + str(rsi_val))
else:
    signal("HOLD", "RSI neutral"))"},

                {"fs-strategies", "Strategy Framework", R"(Build complete trading strategies with entry/exit rules, position sizing, and risk management.

A strategy consists of:
1. Entry conditions - when to open positions
2. Exit conditions - when to close positions
3. Position sizing - how much to allocate
4. Risk management - stop losses, take profits

Use the strategy() function to define:
  strategy("name", initial_capital, commission)

And entry()/exit() for trade signals.)", R"(// Simple Moving Average Crossover Strategy
strategy("SMA Crossover", 100000, 0.001)

let fast = sma(close, 10)
let slow = sma(close, 30)

if crossover(fast, slow):
    entry("LONG", size=100)
elif crossunder(fast, slow):
    exit("LONG"))"},

                {"fs-reference", "Function Reference", R"(Complete list of FinScript built-in functions:

Math: abs, ceil, floor, round, max, min, pow, sqrt, log
Series: highest, lowest, sum, avg, stdev, change, crossover, crossunder
String: str, len, upper, lower, contains, split
Time: now, date, time, dayofweek, hour, minute
Chart: plot, hline, fill, bgcolor, barcolor
I/O: print, signal, alert, export)", std::nullopt},
            }
        },

        // =====================================================================
        // 2. Tabs Reference
        // =====================================================================
        {
            "tabs", "Tabs Reference", "[TAB]",
            {
                {"tabs-overview", "Overview", R"(Fincept Terminal provides 70+ specialized tabs organized into functional groups:

Core: Dashboard, Markets, News, Watchlist
Trading: Crypto Trading, Equity Trading, Algo Trading, Backtesting
Research: Equity Research, Screener, Analytics, Portfolio, Surface Analytics
QuantLib: 18 quantitative analysis modules
AI/ML: AI Quant Lab, Agent Config, Chat, Alpha Arena
Economics: Economics, DBnomics, AkShare Data
Geopolitics: Geopolitics, Geopolitics HDX, Relationship Map
Tools: Code Editor, Node Editor, Excel, Report Builder, Notes
Community: Forum, Documentation, Support, About

Each tab is accessible from the tab bar or the Navigate menu.)", std::nullopt},

                {"tabs-dashboard", "Dashboard", R"(The Dashboard is the home screen showing:

- Market summary: Major indices and their daily performance
- Account overview: Portfolio value, P&L, credit balance
- Quick actions: Navigate to frequently used tabs
- Market ticker: Scrolling ticker with live price updates
- Watchlist preview: Top watchlist items with price/change

Keyboard shortcut: F1)", std::nullopt},

                {"tabs-markets", "Markets", R"(The Markets tab provides comprehensive market data:

- Index overview: Major global indices with real-time data
- Sector performance: Heat map of sector returns
- Market movers: Top gainers, losers, most active
- Futures & commodities: Key futures contracts
- Currency pairs: Major FX rates

Data refreshes every 10 minutes. Keyboard shortcut: F2)", std::nullopt},

                {"tabs-portfolio", "Portfolio", R"(The Portfolio tab offers professional-grade portfolio management:

Views available:
- Analytics: Sector allocation, diversification metrics
- Performance: Time-weighted returns, benchmarking
- Risk: VaR, volatility, drawdown analysis
- Reports: Generated PDF reports
- Planning: Financial planning tools
- Optimization: Mean-variance optimization
- QuantStats: Comprehensive quantitative statistics

Keyboard shortcut: F4)", std::nullopt},

                {"tabs-trading", "Trading", R"(Multiple trading interfaces:

Crypto Trading (F9):
  Real-time WebSocket feeds from Kraken/HyperLiquid
  Order book visualization, trade history, chart

Algo Trading:
  Algorithmic trading strategy configuration
  Backtesting engine integration

Paper Trading:
  Simulated trading with virtual capital
  Full order matching engine)", std::nullopt},
            }
        },

        // =====================================================================
        // 3. Terminal Features
        // =====================================================================
        {
            "features", "Terminal Features", "[FT]",
            {
                {"ft-overview", "Overview", R"(Fincept Terminal is a cross-platform financial intelligence platform offering:

- Real-time market data from 100+ data sources
- CFA-level analytics for equity, portfolio, derivatives, and economics
- AI-powered automation with multiple agent frameworks
- Python analytics engine with 60+ data scripts
- Visual workflow automation via node editor
- Encrypted credential storage (AES-256-GCM)
- Multi-language support (8 languages))", std::nullopt},

                {"ft-shortcuts", "Keyboard Shortcuts", R"(Global Shortcuts:
  F11         Toggle fullscreen
  Alt+F4      Exit application

Tab Navigation:
  F1          Dashboard
  F2          Markets
  F3          News
  F4          Portfolio
  F5          Backtesting
  F6          Surface Analytics
  F7          Geopolitics
  F8          Watchlist
  F9          Crypto Trading
  F10         Agent Studio
  F12         Profile

Menu Access:
  File        Workspace management, export, exit
  Navigate    Quick tab switching
  View        Fullscreen, layout reset
  Help        Documentation, about, support)", std::nullopt},

                {"ft-settings", "Settings", R"(The Settings tab provides configuration for:

- Credentials: API keys for data providers
- LLM Configuration: AI model selection and API keys
- Terminal Appearance: Theme, font size, density
- Storage & Cache: Database management, cache clearing
- Notifications: Alert preferences
- General: Language, startup behavior, updates)", std::nullopt},

                {"ft-security", "Security", R"(Fincept Terminal implements multiple security layers:

- AES-256-GCM encrypted credential storage
- TOTP auto-authentication for supported brokers
- Input sanitization across all user inputs
- No plaintext secret storage
- Session-based authentication with JWT
- Device fingerprinting for session management)", std::nullopt},
            }
        },

        // =====================================================================
        // 4. API Reference
        // =====================================================================
        {
            "api", "API Reference", "[API]",
            {
                {"api-overview", "Overview", R"(The Fincept API (api.fincept.in) provides RESTful endpoints for:

- Authentication: User registration, login, session management
- Market Data: Real-time and historical price data
- Analytics: CFA-level financial analysis
- Portfolio: Portfolio management and optimization
- News: Financial news aggregation
- AI Agents: Automated analysis workflows

All API responses follow the format:
  { "status": "success", "data": { ... } }

Base URL: https://api.fincept.in
Authentication: API key in X-API-Key header)", std::nullopt},

                {"api-auth", "Authentication", R"(Authentication Endpoints:

POST /auth/register
  Body: { email, password, username }
  Returns: { session_token, api_key }

POST /auth/login
  Body: { email, password, device_id }
  Returns: { session_token, api_key, user_info }

POST /auth/guest
  Body: { device_id }
  Returns: { session_token, api_key }

POST /auth/forgot-password
  Body: { email }
  Returns: { message }

Headers required for authenticated endpoints:
  X-API-Key: <api_key>
  X-Device-ID: <device_id>)", std::nullopt},

                {"api-endpoints", "Data Endpoints", R"(Market Data:
  GET /markets/overview - Global market summary
  GET /markets/indices - Index data
  GET /markets/movers - Top gainers/losers

Research:
  GET /research/quote/{symbol} - Stock quote
  GET /research/financials/{symbol} - Financial statements
  GET /research/technicals/{symbol} - Technical analysis

News:
  GET /news/latest - Latest financial news
  GET /news/search?q=query - Search news

Forum:
  GET /forum/categories - Forum categories
  GET /forum/posts - Forum posts
  POST /forum/posts - Create post (auth required))", std::nullopt},

                {"api-errors", "Error Handling", R"(API Error Format:
  { "status": "error", "message": "Description", "code": 400 }

Common Error Codes:
  400 - Bad Request (invalid parameters)
  401 - Unauthorized (missing or invalid API key)
  403 - Forbidden (insufficient permissions)
  404 - Not Found (resource doesn't exist)
  429 - Rate Limited (too many requests)
  500 - Internal Server Error

Rate Limits:
  Free tier: 100 requests/minute
  Paid tier: 1000 requests/minute)", std::nullopt},
            }
        },

        // =====================================================================
        // 5. Tutorials
        // =====================================================================
        {
            "tutorials", "Tutorials", "[TUT]",
            {
                {"tut-first-strategy", "Your First Strategy", R"(This tutorial walks you through creating a simple moving average crossover strategy.

Step 1: Open the Code Editor tab
Step 2: Create a new file called "my_strategy.fs"
Step 3: Write the strategy code (see example)
Step 4: Run the strategy using the Execute button
Step 5: View results in the Output panel

The strategy will generate BUY/SELL signals based on moving average crossovers.)", R"(// Tutorial: Simple SMA Crossover
strategy("My First Strategy", 100000, 0.001)

// Define moving averages
let fast_sma = sma(close, 10)
let slow_sma = sma(close, 50)

// Plot on chart
plot(fast_sma, "Fast SMA", color="blue")
plot(slow_sma, "Slow SMA", color="red")

// Generate signals
if crossover(fast_sma, slow_sma):
    signal("BUY", "Fast SMA crossed above Slow SMA")
    entry("LONG", size=100)

if crossunder(fast_sma, slow_sma):
    signal("SELL", "Fast SMA crossed below Slow SMA")
    exit("LONG"))"},

                {"tut-portfolio", "Portfolio Setup", R"(Learn how to set up and manage your portfolio in Fincept Terminal.

Step 1: Navigate to the Portfolio tab (F4)
Step 2: Add positions using the "Add Position" button
Step 3: Enter ticker symbol, quantity, and purchase price
Step 4: View analytics in the Analytics sub-tab
Step 5: Check risk metrics in the Risk sub-tab

Tips:
- Use paper trading first to test strategies
- Set stop-losses to manage risk
- Review the Performance tab regularly
- Export reports for record-keeping)", std::nullopt},

                {"tut-alerts", "Setting Up Alerts", R"(Configure alerts to stay informed about market conditions.

Price Alerts:
  Set alerts for when a security reaches a target price.
  Navigate to Watchlist > Select ticker > Set Alert.

Technical Alerts:
  RSI crossing thresholds
  Moving average crossovers
  Volume spikes

News Alerts:
  Keywords: Set keywords to monitor
  Tickers: Monitor specific securities
  Sentiment: Alert on sentiment changes

All alerts appear in the Notifications panel and can be configured in Settings.)", std::nullopt},

                {"tut-custom-indicators", "Custom Indicators", R"(Build your own technical indicators using FinScript.

A custom indicator is a FinScript function that takes price/volume data as input and produces a plotable output.

Steps:
1. Define the calculation logic
2. Use plot() to display on charts
3. Use signal() for trade alerts
4. Save as .fs file for reuse

Common patterns:
- Combine multiple timeframes
- Use volume confirmation
- Add filter conditions
- Create composite scores)", R"(// Custom RSI with Bollinger Bands overlay
let rsi_val = rsi(close, 14)
let rsi_sma = sma(rsi_val, 20)
let rsi_std = stdev(rsi_val, 20)
let upper = rsi_sma + 2 * rsi_std
let lower = rsi_sma - 2 * rsi_std

plot(rsi_val, "RSI", color="purple")
plot(upper, "Upper BB", color="gray")
plot(lower, "Lower BB", color="gray")
fill(upper, lower, color="gray", opacity=0.1)

hline(70, "Overbought", color="red")
hline(30, "Oversold", color="green")

if rsi_val < lower and rsi_val < 30:
    signal("BUY", "RSI below lower BB + oversold"))"},
            }
        },
    };
}

} // namespace fincept::docs
