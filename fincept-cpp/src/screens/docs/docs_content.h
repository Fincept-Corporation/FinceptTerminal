#pragma once
// Static documentation content -- all 5 sections matching Tauri DocsTab
// Uses R"DOC(...)DOC" delimiter to avoid conflicts with )" in content

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
                {"fs-overview", "Overview",
R"DOC(FinScript is a high-performance, domain-specific language for financial analysis and algorithmic trading. It is compiled to a standalone Rust binary that executes scripts in under 1 millisecond.

Key Features:
- Pine Script-inspired syntax -- familiar to traders
- 30+ built-in technical indicators (EMA, SMA, RSI, MACD, Bollinger, ATR, etc.)
- Live market data from Yahoo Finance or demo mode with synthetic data
- Risk analytics: Sharpe, Sortino, max drawdown, correlation, beta
- Terminal integration: watchlist, paper trading, alerts, screener
- Candlestick and line chart visualization
- Buy/Sell signal generation with strategy mode
- User-defined functions, structs, maps, and matrices
- For/while loops, if/else, switch, ternary operator
- Alerts and alert conditions
- Batch execution for multi-symbol screening
- Print debugging with formatted output
- Zero external dependencies -- compiled Rust library

Architecture:
- Lexer tokenizes source code into tokens
- Recursive descent parser produces an AST
- Tree-walking interpreter evaluates the program
- Results returned as JSON (signals, plots, output, alerts, integration actions)
- Linked as a Rust library into the app for native performance
- Live mode fetches real OHLCV data via Yahoo Finance

File Extension: .fincept)DOC",
R"DOC(// Your first FinScript program
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_val = rsi(AAPL, 14)

fast = last(ema_fast)
slow = last(ema_slow)

print "EMA(12):", fast
print "EMA(26):", slow

if fast > slow {
    buy "Bullish EMA crossover"
}

plot_candlestick AAPL, "Apple Inc."
plot_line ema_fast, "EMA 12", "cyan"
plot_line ema_slow, "EMA 26", "orange")DOC"},

                {"fs-variables", "Variables & Data Types",
R"DOC(Variables are declared by assignment -- no let, var, or const needed. Types are inferred automatically.

Data Types:
- Number -- 64-bit floating point (e.g., 42, 3.14, -0.5)
- String -- Double-quoted text (e.g., "Hello")
- Bool -- true or false
- Series -- Array of numbers (returned by indicators)
- Array -- Ordered collection (e.g., [10, 20, 30])
- Na -- Missing/null value (similar to NaN)
- Struct -- User-defined composite type
- Map -- Key-value dictionary
- Matrix -- 2D numeric grid

Variable Assignment:
- Simple: x = 42
- From indicator: prices = close(AAPL)
- Compound: x += 1, x -= 2, x *= 3, x /= 4

Type Checking:
- na(x) -- Returns true if x is Na
- nz(x, default) -- Returns x if not Na, otherwise default

Constants:
- true, false -- Boolean literals
- na -- Missing value literal)DOC",
R"DOC(// Variable declarations
ticker = "AAPL"
period = 14
threshold = 70.0
my_bool = true
my_array = [10, 20, 30, 40, 50]

// Series from indicators
close_prices = close(AAPL)
ema_values = ema(AAPL, 12)

// Compound assignment
counter = 0
counter += 1
counter *= 2

// Na handling
x = na
print "Is na?", na(x)       // true
safe = nz(x, 0.0)
print "Safe value:", safe    // 0.0

// Array operations
print "Length:", len(my_array)
print "First:", first(my_array)
print "Last:", last(my_array)
push(my_array, 60)
my_array[0] = 99)DOC"},

                {"fs-operators", "Operators & Expressions",
R"DOC(FinScript supports standard arithmetic, comparison, logical, and string operators.

Arithmetic Operators:
- + Addition (also string concatenation)
- - Subtraction
- * Multiplication
- / Division
- % Modulo

Comparison Operators:
- > Greater than
- < Less than
- >= Greater than or equal
- <= Less than or equal
- == Equal
- != Not equal

Logical Operators:
- and  Logical AND
- or   Logical OR
- not  Logical NOT

Ternary Operator:
- condition ? value_if_true : value_if_false

String Concatenation:
- "Hello " + "World"
- "Price: " + str(150.5)

Series Arithmetic:
Operations between Series are element-wise:
- close(AAPL) - open(AAPL) -> Series of candle bodies
- (high + low) / 2 -> Midpoint series

Range Operator:
- 1..10 -- Range from 1 to 9 (exclusive end))DOC",
R"DOC(// Arithmetic
price = 150.50
change = price * 0.02
new_price = price + change

// Comparison & Logic
rsi_now = last(rsi(AAPL, 14))
bullish = rsi_now < 70 and rsi_now > 30
print "Bullish zone:", bullish

// Ternary
signal = rsi_now > 50 ? "BUY" : "SELL"
print "Signal:", signal

// String concatenation
msg = "RSI is " + str(round(rsi_now, 1))
print msg

// Series arithmetic (element-wise)
close_data = close(AAPL)
open_data = open(AAPL)
body = close_data - open_data
range_size = high(AAPL) - low(AAPL)
typical = (high(AAPL) + low(AAPL) + close_data) / 3

print "Last body:", round(last(body), 2)
print "Last range:", round(last(range_size), 2))DOC"},

                {"fs-control-flow", "Control Flow",
R"DOC(FinScript provides if/else, for loops, while loops, and switch statements.

If / Else If / Else:
- Braces {} are required for blocks
- else if for chained conditions
- Conditions must evaluate to Bool or Number

For Loops:
- Range-based: for i in 1..10 { ... }
- Array iteration: for item in my_array { ... }
- break to exit early
- continue to skip iteration

While Loops:
- while condition { ... }
- break and continue supported

Switch Statement:
- Match a value against multiple cases
- else for default case
- No fall-through)DOC",
R"DOC(// If / Else If / Else
rsi_val = last(rsi(AAPL, 14))

if rsi_val > 70 {
    print "Overbought"
    sell "RSI overbought"
} else if rsi_val < 30 {
    print "Oversold"
    buy "RSI oversold"
} else {
    print "Neutral zone"
}

// For loop with range
total = 0
for i in 1..11 {
    total += i
}
print "Sum 1-10:", total   // 55

// For loop over array
tickers = ["AAPL", "MSFT", "GOOGL"]
for t in tickers {
    print "Analyzing:", t
}

// While loop
counter = 1
result = 1
while counter <= 10 {
    result *= 2
    counter += 1
}
print "2^10 =", result   // 1024

// Switch statement
day = "monday"
switch day {
    "monday" {
        print "Start of week"
    }
    "friday" {
        print "End of week"
    }
    else {
        print "Mid-week"
    }
})DOC"},

                {"fs-functions", "User-Defined Functions",
R"DOC(Define reusable functions with fn. Functions support parameters, return values, and can access outer-scope variables.

Syntax:
- fn name(param1, param2) { ... }
- return value to return a value
- Last expression is NOT auto-returned -- use return
- Functions can call other functions (recursion supported)

Closures:
Functions can read variables from the enclosing scope.

Best Practices:
- Keep functions focused on one task
- Use descriptive parameter names
- Return meaningful values for composability)DOC",
R"DOC(// Simple function
fn fibonacci(n) {
    if n < 2 {
        return n
    }
    a = 0
    b = 1
    for i in 2..n {
        temp = a + b
        a = b
        b = temp
    }
    return b
}
print "Fib(10):", fibonacci(10)   // 34

// Function with financial logic
fn is_golden_cross(symbol, fast_p, slow_p) {
    fast = last(ema(symbol, fast_p))
    slow = last(ema(symbol, slow_p))
    return fast > slow
}

if is_golden_cross(AAPL, 50, 200) {
    buy "Golden Cross detected"
}

// Modular strategy components
fn compute_signal(symbol) {
    ema_f = last(ema(symbol, 12))
    ema_s = last(ema(symbol, 26))
    rsi_v = last(rsi(symbol, 14))
    bullish = ema_f > ema_s and rsi_v < 70
    return bullish ? 1 : -1
}

raw = compute_signal(AAPL)
print "Signal:", raw)DOC"},

                {"fs-indicators", "Technical Indicators",
R"DOC(FinScript includes 30+ built-in technical indicators. All return a Series.

Moving Averages:
  sma, ema, wma, rma, hma

Momentum:
  rsi, stochastic, stochastic_d, cci, mfi, williams_r, roc, momentum

MACD:
  macd, macd_signal, macd_hist

Volatility:
  atr, true_range, bollinger, bollinger_middle, bollinger_lower, stdev

Trend:
  adx, sar, supertrend, supertrend_dir, linreg

Volume:
  obv, vwap

Crossover Detection:
  crossover(series_a, series_b) -- True when A crosses above B
  crossunder(series_a, series_b) -- True when A crosses below B)DOC",
R"DOC(// Moving Averages
sma_20 = sma(AAPL, 20)
ema_12 = ema(AAPL, 12)
hma_9 = hma(AAPL, 9)

print "SMA(20):", last(sma_20)
print "EMA(12):", last(ema_12)

// Momentum
rsi_val = rsi(AAPL, 14)
stoch_k = stochastic(AAPL, 14)
print "RSI:", last(rsi_val)

// MACD with custom periods
macd_line = macd(AAPL, 12, 26, 9)
macd_sig = macd_signal(AAPL)
macd_h = macd_hist(AAPL)
print "MACD:", last(macd_line)

// Bollinger Bands
bb_up = bollinger(AAPL, 20, 2)
bb_low = bollinger_lower(AAPL, 20, 2)
print "BB Width:", last(bb_up) - last(bb_low)

// Crossover detection
fast_ma = ema(AAPL, 12)
slow_ma = ema(AAPL, 26)
if crossover(fast_ma, slow_ma) {
    buy "Golden crossover"
})DOC"},

                {"fs-price-functions", "Price & Utility Functions",
R"DOC(Access OHLCV price data and utility functions for series manipulation.

Price Functions:
  close, open, high, low, volume

Series Utilities:
  last, first, len, highest, lowest, change, cum, sum, avg, max, min
  percentrank, pivothigh, pivotlow

Math Functions:
  abs, sqrt, pow, log, log10, exp, round, floor, ceil, sign
  sin, cos, tan

String Functions:
  str(value) -- Convert to string)DOC",
R"DOC(// Price data access
prices = close(AAPL)
highs = high(AAPL)
lows = low(AAPL)
vols = volume(AAPL)

print "Data points:", len(prices)
print "Latest close:", last(prices)

// Series utilities
print "20-bar high:", last(highest(prices, 20))
print "20-bar low:", last(lowest(prices, 20))
print "1-bar change:", last(change(prices, 1))

// Math functions
print "abs(-5):", abs(-5)
print "sqrt(144):", sqrt(144)
print "pow(2, 10):", pow(2, 10)
print "round(3.14159, 2):", round(3.14159, 2))DOC"},

                {"fs-plotting", "Visualization & Charts",
R"DOC(FinScript generates charts rendered with Lightweight Charts.

Candlestick Charts:
  plot_candlestick symbol, "title"

Line Charts:
  plot_line series, "label", "color"

Indicator Plots:
  plot series, "label" -- appears in separate panel

Histogram:
  plot_histogram series, "label", "up_color", "down_color"

Horizontal Lines:
  hline value, "label", "color"

Drawing Tools:
  line_new, label_new, box_new

Shape Markers:
  plot_shape condition, "shape", "location", "color", "text"

Tables (On-Chart):
  table_new, table_cell)DOC",
R"DOC(// Candlestick with overlays
plot_candlestick AAPL, "AAPL Price"

// Moving average overlays
ema_20 = ema(AAPL, 20)
ema_50 = ema(AAPL, 50)
plot_line ema_20, "EMA 20", "cyan"
plot_line ema_50, "EMA 50", "orange"

// RSI in separate panel
rsi_val = rsi(AAPL, 14)
plot rsi_val, "RSI"
hline 70, "Overbought", "red"
hline 30, "Oversold", "green"

// MACD histogram
macd_h = macd_hist(AAPL)
plot_histogram macd_h, "MACD Hist", "#26a69a", "#ef5350")DOC"},

                {"fs-signals", "Trading Signals",
R"DOC(Generate buy/sell signals and use strategy mode for simulated execution.

Simple Signals:
  buy "reason" -- Generate a buy signal
  sell "reason" -- Generate a sell signal

Strategy Mode:
  strategy.entry("name", "long"/"short", quantity)
  strategy.exit("name", "entry_name")

Alerts:
  alert("message")
  alert("message", "type") -- Types: "once", "every_bar"
  alertcondition(condition, "title", "message")

Best Practices:
- Use multiple confirmation indicators before signaling
- Implement position sizing with ATR-based stops
- Test with print statements before live execution)DOC",
R"DOC(// Simple buy/sell signals
ema_f = last(ema(AAPL, 12))
ema_s = last(ema(AAPL, 26))
rsi_now = last(rsi(AAPL, 14))

if ema_f > ema_s and rsi_now < 70 {
    buy "Bullish crossover, RSI not overbought"
}
if ema_f < ema_s and rsi_now > 30 {
    sell "Bearish crossover, RSI not oversold"
}

// Strategy mode with position sizing
price = last(close(AAPL))
atr_now = last(atr(AAPL, 14))
capital = 100000
risk = 0.02
stop_distance = atr_now * 1.5
position_size = round((capital * risk) / stop_distance, 0)

if ema_f > ema_s {
    strategy.entry("Long", "long", position_size)
    print "Entry at", price, "Size:", position_size
})DOC"},

                {"fs-structs", "Structs & Data Structures",
R"DOC(FinScript supports user-defined structs, maps, and matrices.

Structs:
  Define with struct Name { field: type }
  Create with Name { field: value }
  Access with dot notation: instance.field

Maps:
  map_new, map_put, map_get, map_size, map_contains

Matrices:
  matrix_new, matrix_set, matrix_get, matrix_rows, matrix_cols

Arrays:
  Literal: [10, 20, 30]
  push(arr, value), slice(arr, start, end), arr[index])DOC",
R"DOC(// Struct definition and usage
struct TradeSetup {
    symbol: string
    direction: string
    entry_price: float
    confidence: float
}

setup = TradeSetup {
    symbol: "AAPL",
    direction: "long",
    entry_price: 150.50,
    confidence: 0.85
}

print "Symbol:", setup.symbol
print "Confidence:", setup.confidence

// Maps for portfolio tracking
portfolio = map_new()
map_put(portfolio, "AAPL", 100)
map_put(portfolio, "MSFT", 50)

print "AAPL shares:", map_get(portfolio, "AAPL")
print "Total positions:", map_size(portfolio)

// Matrix for correlation data
corr = matrix_new(3, 3, 0.0)
matrix_set(corr, 0, 0, 1.0)
matrix_set(corr, 0, 1, 0.85)
print "AAPL-MSFT corr:", matrix_get(corr, 0, 1))DOC"},

                {"fs-multi-symbol", "Multi-Symbol Analysis",
R"DOC(FinScript automatically fetches data for any uppercase ticker symbol.

Ticker Symbols:
- Any ALL-UPPERCASE identifier is treated as a ticker
- Demo mode: synthetic data (180 days)
- Live mode: real Yahoo Finance data

Cross-Symbol Operations:
- Compare indicators across symbols
- Compute spreads and ratios
- Build sector scanners with loops)DOC",
R"DOC(// Analyze multiple symbols
print "AAPL:", round(last(close(AAPL)), 2)
print "MSFT:", round(last(close(MSFT)), 2)
print "GOOGL:", round(last(close(GOOGL)), 2)

// Sector momentum scanner
fn scan_momentum(ticker, name) {
    ema_f = last(ema(ticker, 12))
    ema_s = last(ema(ticker, 26))
    rsi_v = last(rsi(ticker, 14))
    signal = ema_f > ema_s ? "BULL" : "BEAR"
    print name, ":", signal, "| RSI:", round(rsi_v, 1)
}

print "=== Momentum Scanner ==="
scan_momentum(AAPL, "Apple")
scan_momentum(MSFT, "Microsoft")
scan_momentum(GOOGL, "Google")

// Spread analysis
spread = sma(AAPL, 20) - sma(MSFT, 20)
print "AAPL-MSFT Spread:", round(last(spread), 2))DOC"},

                {"fs-risk-analytics", "Risk & Portfolio Analytics",
R"DOC(Built-in risk analytics functions for portfolio analysis.

Risk Metrics:
  sharpe(series) -- Annualized Sharpe ratio
  sortino(series) -- Sortino ratio
  max_drawdown(series) -- Max peak-to-trough decline

Correlation & Beta:
  correlation(series_a, series_b) -- Pearson correlation
  beta(asset, benchmark) -- Beta coefficient

Returns:
  returns(series) -- Daily returns series

Interpretation:
- Sharpe > 1.0 = good, > 2.0 = very good, > 3.0 = excellent
- Max drawdown of 0.20 means 20% drop from peak
- Beta > 1 = more volatile than benchmark)DOC",
R"DOC(// Risk analytics
prices = close(AAPL)
sr = sharpe(prices)
so = sortino(prices)
dd = max_drawdown(prices)

print "=== AAPL Risk Profile ==="
print "Sharpe:", round(sr, 3)
print "Sortino:", round(so, 3)
print "Max Drawdown:", round(dd * 100, 2), "%"

// Compare two assets
corr = correlation(close(AAPL), close(MSFT))
b = beta(close(AAPL), close(SPY))

print "AAPL-MSFT Correlation:", round(corr, 3)
print "AAPL Beta vs SPY:", round(b, 3)

// Risk-adjusted comparison
print "AAPL Sharpe:", round(sharpe(close(AAPL)), 3)
print "MSFT Sharpe:", round(sharpe(close(MSFT)), 3))DOC"},

                {"fs-complete-example", "Complete Trading System",
R"DOC(Full trading system combining all FinScript features.

System Components:
- Configuration via struct
- Multi-indicator scoring model
- ATR-based position sizing
- Full visualization with overlays

Scoring Model:
- +2/-2 for EMA trend direction
- +1/-1 for RSI momentum
- +1/-1 for MACD direction
- Volatility filter
- Trade when score >= 3 or <= -3)DOC",
R"DOC(// === Configuration ===
struct Config {
    fast_period: int
    slow_period: int
    rsi_period: int
    atr_mult: float
}

config = Config {
    fast_period: 12,
    slow_period: 26,
    rsi_period: 14,
    atr_mult: 1.5
}

// === Indicators ===
ema_fast = ema(AAPL, config.fast_period)
ema_slow = ema(AAPL, config.slow_period)
rsi_val = rsi(AAPL, config.rsi_period)
macd_h = macd_hist(AAPL)

price = last(close(AAPL))
rsi_now = last(rsi_val)
ema_f = last(ema_fast)
ema_s = last(ema_slow)

// === Scoring ===
fn calculate_score() {
    score = 0
    if ema_f > ema_s { score += 2 }
    else { score -= 2 }
    if rsi_now > 50 and rsi_now < 70 { score += 1 }
    if last(macd_h) > 0 { score += 1 }
    else { score -= 1 }
    return score
}

signal_score = calculate_score()
print "Score:", signal_score, "| Price:", round(price, 2)

if signal_score >= 3 {
    strategy.entry("Buy", "long", 100)
}

// === Visualization ===
plot_candlestick AAPL, "Trading System"
plot_line ema_fast, "Fast EMA", "cyan"
plot_line ema_slow, "Slow EMA", "orange"
plot rsi_val, "RSI"
hline 70, "OB", "red"
hline 30, "OS", "green")DOC"},
            }
        },

        // =====================================================================
        // 2. Tabs Reference
        // =====================================================================
        {
            "tabs", "Tabs Reference", "[TAB]",
            {
                {"dashboard", "Dashboard Tab",
R"DOC(The Dashboard provides an overview of key market information and indices.

Features:
- Real-time market indices (S&P 500, NASDAQ, Dow Jones, etc.)
- Global indices tracking (Asia, Europe, US)
- Economic indicators display
- Sector performance overview
- Market breadth indicators
- Key statistics at a glance

Data Updates:
Real-time updates during market hours with configurable refresh rate.

Keyboard Shortcut: F1)DOC",
R"DOC(// Dashboard shows:
- S&P 500: 4,850.23 (+1.2%)
- NASDAQ: 15,234.56 (+1.8%)
- DOW: 38,456.78 (+0.9%)

// Global Markets:
- Nikkei 225: 38,234.56
- FTSE 100: 7,654.32
- DAX: 17,123.45

// Economic Indicators:
- US 10Y Treasury: 4.25%
- VIX: 13.45
- USD Index: 103.25)DOC"},

                {"markets", "Markets Tab",
R"DOC(Real-time market data organized by asset classes and regions.

Asset Categories:
- Stocks -- Global equity markets
- Forex -- Currency pairs
- Commodities -- Gold, Oil, Agricultural products
- Bonds -- Government and corporate bonds
- ETFs -- Exchange-traded funds
- Cryptocurrency -- Digital assets

Regional Coverage:
- United States, European, Asian, Emerging markets

Features:
- Live price quotes, Intraday charts
- Volume analysis, Market depth
- Pre-market and after-hours data

Keyboard Shortcut: F2)DOC", std::nullopt},

                {"news", "News Tab",
R"DOC(Real-time financial news aggregation and analysis.

News Sources:
- Fincept, CNBC, Financial Times
- Yahoo Finance, Investing.com

News Categories:
- Markets, Earnings, M&A, Economic
- Geopolitics, Regulatory, Technical

Features:
- Real-time news feed
- Sentiment analysis (Bullish/Bearish/Neutral)
- Impact rating (High/Medium/Low)
- Ticker association and filtering

News Priority Levels:
- FLASH -- Breaking market-moving news
- URGENT -- High-priority updates
- BREAKING -- Important developments
- ROUTINE -- Regular news flow

Keyboard Shortcut: F3)DOC", std::nullopt},

                {"forum", "Forum Tab",
R"DOC(Community discussion platform for traders and investors.

Forum Features:
- Discussion threads on stocks, crypto, and strategies
- User reputation system
- Post voting and ranking
- Category organization

Post Categories:
- Markets, Stocks, Crypto, Options, Forex
- Technical Analysis, Fundamental Analysis, Strategy

User Features:
- Create and reply to posts
- Like and dislike posts
- Track post analytics (views, replies, likes)
- Search across all posts)DOC", std::nullopt},

                {"portfolio", "Portfolio Tab",
R"DOC(Track and manage your investment portfolio.

Portfolio Features:
- Real-time position tracking
- P&L calculation (realized and unrealized)
- Cost basis tracking, Performance metrics
- Sector and asset allocation, Risk metrics

Performance Metrics:
- Total return, Sharpe ratio
- Maximum drawdown, Beta, Alpha

Keyboard Shortcut: F4)DOC", std::nullopt},

                {"watchlist", "Watchlist Tab",
R"DOC(Monitor and track securities of interest.

Watchlist Features:
- Multiple watchlist support
- Real-time quote updates
- Quick add/remove securities
- Custom sorting and filtering, Price alerts

Data Displayed:
- Last price, Change, Volume
- Day high/low, Market cap, P/E ratio
- Dividend yield, 52-week high/low

Keyboard Shortcut: F6)DOC", std::nullopt},

                {"screener", "Screener Tab",
R"DOC(Discover stocks based on custom criteria and filters.

Fundamental Filters:
- Market Cap, P/E Ratio, P/S Ratio, P/B Ratio
- EPS, Dividend Yield, Revenue Growth
- Profit Margin, ROE, ROA, Debt-to-Equity

Technical Filters:
- Price, Volume, Price Change %
- RSI, Moving average crossovers
- 52-week proximity, Beta

Pre-built Screens:
- Value stocks, Growth stocks
- Momentum stocks, Dividend aristocrats

Keyboard Shortcut: F7)DOC", std::nullopt},

                {"chat", "AI Chat Tab",
R"DOC(Interact with AI assistant for market analysis and strategy development.

AI Capabilities:
- Market analysis and insights
- Trading strategy suggestions
- Technical analysis interpretation
- Code generation (FinScript)
- Risk management guidance
- Portfolio optimization suggestions

Chat Features:
- Multi-session support
- Conversation history
- Code highlighting

Keyboard Shortcut: F9)DOC", std::nullopt},

                {"code-editor", "Code Editor Tab",
R"DOC(Write, test, and execute FinScript trading strategies.

Editor Features:
- Syntax highlighting for FinScript
- Auto-completion and error checking
- Multiple file support
- Code snippets, Find and replace

Execution:
- Run strategy on historical data
- Chart generation, Signal visualization
- Performance metrics, Error reporting

Keyboard Shortcuts:
- Ctrl+R -- Run code
- Ctrl+S -- Save file
- Ctrl+/ -- Comment line
- Ctrl+F -- Find)DOC", std::nullopt},

                {"node-editor", "Node Editor / Workflow Tab",
R"DOC(Visual programming for building complex data pipelines and workflows.

Node Types:
- Data Source Nodes -- Import data
- Transformation Nodes -- Process data
- Analysis Nodes -- Perform analysis
- Visualization Nodes -- Create charts
- Agent Nodes -- AI-powered analysis
- Output Nodes -- Export results

Features:
- Drag-and-drop interface
- Visual connection system
- Real-time data flow
- Save/load workflows)DOC", std::nullopt},

                {"agents", "Agents Tab",
R"DOC(Deploy AI agents for automated trading and monitoring.

Hedge Fund Styles:
- Renaissance Technologies, Bridgewater Associates
- Citadel, Two Sigma

Trader Styles:
- Warren Buffett, Ray Dalio, Peter Lynch
- George Soros, Jim Simons

Analysis Agents:
- Technical, Fundamental, Sentiment, Risk

Features:
- Drag-and-drop agent creation
- Connect agents to data sources
- Monitor agent status
- 24/7 automated execution)DOC", std::nullopt},

                {"settings", "Settings Tab",
R"DOC(Configure terminal preferences and account settings.

General Settings:
- Theme, Language (8 languages), Time zone

Editor Settings:
- Font family/size, Tab size, Auto-save

Data Settings:
- Refresh rate, Auto-update, Cache, API limits

Notification Settings:
- Desktop notifications, Sound alerts

Account Settings:
- User profile, Subscription plan
- API keys, Connected brokers

Keyboard Shortcut: F10)DOC", std::nullopt},
            }
        },

        // =====================================================================
        // 3. Terminal Features
        // =====================================================================
        {
            "terminal", "Terminal Features", "[FT]",
            {
                {"overview", "Terminal Overview",
R"DOC(Fincept Terminal is a professional financial terminal designed for traders, analysts, and investors.

Key Features:
- Real-time market data and quotes
- Advanced charting and technical analysis
- News aggregation and sentiment analysis
- Portfolio management and tracking
- Custom alerts and notifications
- Multi-tab workspace for efficient workflow
- Dark theme interface
- Customizable layouts and settings

Target Users:
- Professional traders
- Financial analysts
- Portfolio managers
- Retail investors
- Quant developers)DOC",
R"DOC(// Access terminal via desktop app
// Available on Windows, macOS, and Linux

// Key capabilities:
- Real-time streaming data
- Historical data analysis
- Custom indicator development
- Automated strategy execution
- Risk management tools
- Performance analytics)DOC"},

                {"shortcuts", "Keyboard Shortcuts",
R"DOC(Master the terminal with these keyboard shortcuts:

Global Shortcuts:
- F1 -- Dashboard
- F2 -- Markets
- F3 -- News
- F4 -- Portfolio
- F5 -- Backtesting
- F6 -- Watchlist
- F7 -- Screener
- F8 -- Alerts
- F9 -- Chat/AI Assistant
- F10 -- Settings
- F11 -- Fullscreen toggle
- Ctrl+S -- Save current file
- Ctrl+N -- New file
- Ctrl+W -- Close tab
- Ctrl+Tab -- Next tab

Editor Shortcuts:
- Ctrl+/ -- Comment/Uncomment
- Ctrl+F -- Find
- Ctrl+R -- Run code)DOC",
R"DOC(// Quick actions in editor
// Press Ctrl+/ to toggle comments
// Press Ctrl+R to execute code
plot ema(AAPL, 21), "EMA 21"
// Press Ctrl+S to save file)DOC"},

                {"customization", "Customization",
R"DOC(Customize the terminal to match your workflow:

Themes:
- Fincept Classic (default)
- Dark Mode
- Light Mode

Layouts:
- Single panel
- Dual panel (editor + output)
- Triple panel (editor + output + charts)
- Customizable panel sizes

Settings:
- Font size and family
- Tab size (spaces)
- Auto-save interval
- Data refresh rate
- Chart colors and styles
- Alert notification preferences

Workspace:
- Save custom layouts
- Multiple workspace profiles
- Quick switching between setups)DOC",
R"DOC(// Example settings
{
  "theme": "fincept-classic",
  "editor": {
    "fontSize": 14,
    "fontFamily": "Consolas",
    "tabSize": 4,
    "autoSave": true
  },
  "terminal": {
    "refreshRate": 1000,
    "enableNotifications": true
  },
  "charts": {
    "defaultTheme": "dark",
    "gridLines": true,
    "crosshair": true
  }
})DOC"},
            }
        },

        // =====================================================================
        // 4. API Reference
        // =====================================================================
        {
            "api", "API Reference", "[API]",
            {
                {"data-api", "Data API",
R"DOC(Access real-time and historical market data through our unified API:

Endpoints:
- GET /api/v1/quote/{symbol} -- Get latest quote
- GET /api/v1/historical/{symbol} -- Historical OHLCV data
- GET /api/v1/indicators/{symbol} -- Calculated indicators
- POST /api/v1/backtest -- Run strategy backtest
- GET /api/v1/news/{symbol} -- Latest news
- GET /api/v1/fundamentals/{symbol} -- Company fundamentals

Authentication:
Use API keys in the X-API-Key header

Rate Limits:
- Free: 100/hr, Starter: 500/hr
- Professional: 2,000/hr, Enterprise: 10,000/hr

Response Format:
  { "success": true, "data": {...}, "timestamp": "..." })DOC",
R"DOC(// Get real-time quote
GET https://api.fincept.in/market/price/AAPL
Headers: X-API-Key: your_api_key_here

// Response:
{
  "success": true,
  "data": {
    "symbol": "AAPL",
    "price": 184.92,
    "change": 1.87,
    "changePercent": 1.02,
    "volume": 48234567
  }
})DOC"},

                {"websocket", "WebSocket Streaming",
R"DOC(Real-time data streaming via WebSocket for live market data:

Connection:
  wss://stream.fincept.com/v1/quotes

Subscribe:
  {"action": "subscribe", "symbols": ["AAPL", "TSLA"]}

Features:
- Sub-millisecond latency
- Level 2 order book data
- Trade execution updates
- Automatic reconnection
- Heartbeat messages

Data Types:
- Quotes -- Real-time bid/ask
- Trades -- Executed trades
- Bars -- OHLCV aggregated bars
- Book -- Order book depth)DOC", std::nullopt},

                {"indicators-api", "Indicators API",
R"DOC(Calculate technical indicators via API:

Available Indicators:
- Moving Averages: SMA, EMA, WMA, VWAP
- Momentum: RSI, MACD, Stochastic, CCI
- Volatility: ATR, Bollinger Bands, Standard Deviation
- Volume: OBV, MFI, A/D Line
- Trend: ADX, Aroon, Parabolic SAR

Endpoint:
  GET /api/v1/indicators/{symbol}

Parameters:
- symbol -- Stock ticker (required)
- indicator -- Indicator name (required)
- period -- Time period (required)
- interval -- Data interval (optional, default: 1d)

Example Requests:
- /api/v1/indicators/AAPL?indicator=ema&period=21
- /api/v1/indicators/TSLA?indicator=rsi&period=14)DOC",
R"DOC(// Calculate EMA
GET /api/v1/indicators/AAPL?indicator=ema&period=21

// Response:
{
  "success": true,
  "data": {
    "symbol": "AAPL",
    "indicator": "ema",
    "period": 21,
    "current": 184.56
  }
})DOC"},

                {"backtesting-api", "Backtesting API",
R"DOC(Test trading strategies on historical data:

Endpoint:
  POST /api/v1/backtest

Request Body:
  {
    "symbol": "AAPL",
    "strategy": "ema_crossover",
    "start_date": "2024-01-01",
    "end_date": "2024-12-31",
    "initial_capital": 10000,
    "parameters": { "fast_period": 12, "slow_period": 26 }
  }

Response Metrics:
- Total return, Sharpe ratio, Maximum drawdown
- Win rate, Total trades, Profit factor

Backtests run asynchronously -- poll /api/v1/backtest/{job_id} for results.)DOC",
R"DOC(// Submit backtest
POST /api/v1/backtest
{
  "symbol": "AAPL",
  "strategy": "ema_crossover",
  "initial_capital": 10000
}

// Response:
{
  "success": true,
  "data": {
    "total_return": 23.5,
    "sharpe_ratio": 1.8,
    "max_drawdown": -8.2,
    "win_rate": 67.3,
    "total_trades": 45
  }
})DOC"},
            }
        },

        // =====================================================================
        // 5. Tutorials
        // =====================================================================
        {
            "tutorials", "Tutorials", "[TUT]",
            {
                {"first-strategy", "Build Your First Strategy",
R"DOC(Let's build a simple moving average crossover strategy from scratch:

Step 1: Define your indicators
Calculate fast and slow moving averages

Step 2: Create trading logic
Buy when fast MA crosses above slow MA
Sell when fast MA crosses below slow MA

Step 3: Add visualization
Plot your indicators on a chart

Step 4: Run and test
Execute your strategy and analyze results

Step 5: Optimize
Adjust parameters to improve performance)DOC",
R"DOC(// Complete MA Crossover Strategy
symbol = AAPL

// Step 1: Define indicators
fast_ma = ema(symbol, 12)
slow_ma = ema(symbol, 26)

// Step 2: Trading logic
if fast_ma > slow_ma {
    buy "Bullish crossover"
}

if fast_ma < slow_ma {
    sell "Bearish crossover"
}

// Step 3: Visualization
plot fast_ma, "Fast EMA (12)"
plot slow_ma, "Slow EMA (26)"
plot_candlestick symbol, "Apple Stock")DOC"},

                {"advanced-strategy", "Advanced: Multi-Indicator Strategy",
R"DOC(Build a sophisticated strategy combining multiple indicators:

Indicators Used:
- EMA (12, 26) for trend
- RSI (14) for momentum
- Bollinger Bands for volatility
- Volume for confirmation

Strategy Rules:
- Buy: Bullish trend + oversold + high volume
- Sell: Bearish trend + overbought OR stop loss

Risk Management:
- Position sizing based on volatility
- Stop loss at 2% below entry
- Take profit at 5% above entry)DOC",
R"DOC(// Advanced Multi-Indicator Strategy
symbol = TSLA

// Indicators
ema_fast = ema(symbol, 12)
ema_slow = ema(symbol, 26)
rsi_val = rsi(symbol, 14)
vol = volume(symbol)
avg_vol = sma(vol, 20)

// Buy conditions
trend_bullish = ema_fast > ema_slow
oversold = rsi_val < 30
volume_spike = vol > avg_vol * 1.5

if trend_bullish and oversold and volume_spike {
    buy "Strong buy signal"
}

// Sell conditions
if ema_fast < ema_slow or rsi_val > 70 {
    sell "Exit: Reversal signal"
}

plot ema_fast, "EMA 12"
plot ema_slow, "EMA 26"
plot rsi_val, "RSI")DOC"},

                {"portfolio-tutorial", "Managing Your Portfolio",
R"DOC(Learn to track and manage your investment portfolio:

Step 1: Add Positions
Enter your current holdings with purchase details

Step 2: Monitor Performance
Track real-time P&L and portfolio value

Step 3: Analyze Allocation
Review sector and asset allocation

Step 4: Set Alerts
Create alerts for portfolio events

Step 5: Rebalance
Adjust positions based on target allocation

Best Practices:
- Diversify across sectors (aim for 5-10 sectors)
- Limit individual position size (max 10-15% each)
- Review portfolio weekly
- Rebalance quarterly
- Monitor correlation between holdings
- Track risk-adjusted returns)DOC", std::nullopt},

                {"watchlist-tutorial", "Using Watchlists Effectively",
R"DOC(Master watchlist management for efficient market monitoring:

By Strategy:
- Momentum -- Stocks with strong price action
- Value -- Undervalued stocks
- Dividend -- High-yielding stocks
- Earnings -- Upcoming earnings plays

Watchlist Workflow:
1. Scan for candidates using Screener
2. Add to appropriate watchlist
3. Monitor for entry signals
4. Set price alerts
5. Execute trades when conditions met
6. Move to Portfolio or remove

Tips:
- Keep watchlists focused (10-20 stocks each)
- Review and update weekly
- Use multiple watchlists for organization
- Set alerts on all watchlist stocks)DOC", std::nullopt},

                {"alerts-tutorial", "Setting Up Smart Alerts",
R"DOC(Create effective alerts to never miss trading opportunities:

Price Alerts:
- Breakout levels, Support/resistance
- Round numbers, All-time highs

Technical Alerts:
- RSI overbought/oversold
- MA crossovers, MACD signals
- Volume spikes

News Alerts:
- Earnings announcements
- Merger rumors
- Analyst upgrades/downgrades

Best Practices:
- Use multiple confirmation alerts
- Set both entry and exit alerts
- Include stop-loss alerts
- Review triggered alerts regularly)DOC", std::nullopt},

                {"agent-workflow", "Building Agent Workflows",
R"DOC(Create automated agent workflows for 24/7 market monitoring:

Workflow Components:
- Data Sources -- Market data feeds
- Agents -- Analysis and decision making
- Connections -- Data flow between nodes
- Outputs -- Alerts, trades, reports

Example Workflows:

Workflow 1: Multi-Source Analysis
  Yahoo Finance -> Warren Buffett Agent -> Portfolio Alerts

Workflow 2: Diversified Strategies
  Market Data -> Momentum Agent -> Aggressive Portfolio
             -> Value Agent    -> Conservative Portfolio

Best Practices:
- Start with simple workflows
- Test with paper trading first
- Monitor agent performance
- Use risk management agents
- Log all agent decisions)DOC", std::nullopt},
            }
        },
    };
}

} // namespace fincept::docs
