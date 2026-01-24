import { DocSection } from '../types';
import { Code } from 'lucide-react';

export const finscriptLanguageDoc: DocSection = {
  id: 'finscript',
  title: 'FinScript Language',
  icon: Code,
  subsections: [
    {
      id: 'fs-overview',
      title: 'Overview',
      content: `FinScript is a high-performance, domain-specific language for financial analysis and algorithmic trading. It is compiled to a standalone Rust binary that executes scripts in under 1 millisecond.

**Key Features:**
• Pine Script-inspired syntax — familiar to traders
• 30+ built-in technical indicators (EMA, SMA, RSI, MACD, Bollinger, ATR, etc.)
• Deterministic OHLCV data generation for instant backtesting
• Candlestick and line chart visualization
• Buy/Sell signal generation with strategy mode
• User-defined functions, structs, maps, and matrices
• For/while loops, if/else, switch, ternary operator
• Alerts and alert conditions
• Print debugging with formatted output
• Zero external dependencies — pure Rust execution

**Architecture:**
• Lexer tokenizes source code into tokens
• Recursive descent parser produces an AST
• Tree-walking interpreter evaluates the program
• Results returned as JSON (signals, plots, output, alerts)
• Standalone CLI binary — no Python or network required

**File Extension:** \`.fincept\``,
      codeExample: `// Your first FinScript program
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
plot_line ema_slow, "EMA 26", "orange"`
    },
    {
      id: 'fs-variables',
      title: 'Variables & Data Types',
      content: `Variables are declared by assignment — no \`let\`, \`var\`, or \`const\` needed. Types are inferred automatically.

**Data Types:**
• **Number** — 64-bit floating point (e.g., \`42\`, \`3.14\`, \`-0.5\`)
• **String** — Double-quoted text (e.g., \`"Hello"\`)
• **Bool** — \`true\` or \`false\`
• **Series** — Array of numbers (returned by indicators)
• **Array** — Ordered collection (e.g., \`[10, 20, 30]\`)
• **Na** — Missing/null value (similar to NaN)
• **Struct** — User-defined composite type
• **Map** — Key-value dictionary
• **Matrix** — 2D numeric grid

**Variable Assignment:**
• Simple: \`x = 42\`
• From indicator: \`prices = close(AAPL)\`
• Compound: \`x += 1\`, \`x -= 2\`, \`x *= 3\`, \`x /= 4\`

**Type Checking:**
• \`na(x)\` — Returns true if x is Na
• \`nz(x, default)\` — Returns x if not Na, otherwise default

**Constants:**
• \`true\`, \`false\` — Boolean literals
• \`na\` — Missing value literal`,
      codeExample: `// Variable declarations
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
my_array[0] = 99`
    },
    {
      id: 'fs-operators',
      title: 'Operators & Expressions',
      content: `FinScript supports standard arithmetic, comparison, logical, and string operators.

**Arithmetic Operators:**
• \`+\` Addition (also string concatenation)
• \`-\` Subtraction
• \`*\` Multiplication
• \`/\` Division
• \`%\` Modulo

**Comparison Operators:**
• \`>\` Greater than
• \`<\` Less than
• \`>=\` Greater than or equal
• \`<=\` Less than or equal
• \`==\` Equal
• \`!=\` Not equal

**Logical Operators:**
• \`and\` Logical AND
• \`or\` Logical OR
• \`not\` Logical NOT

**Ternary Operator:**
• \`condition ? value_if_true : value_if_false\`

**String Concatenation:**
• \`"Hello " + "World"\` → \`"Hello World"\`
• \`"Price: " + str(150.5)\` → \`"Price: 150.5"\`

**Series Arithmetic:**
Operations between Series are element-wise:
• \`close(AAPL) - open(AAPL)\` → Series of candle bodies
• \`(high + low) / 2\` → Midpoint series
• \`series / last(series) * 100\` → Normalized series

**Range Operator:**
• \`1..10\` — Range from 1 to 9 (exclusive end)
• \`start..end\` — Works with variables`,
      codeExample: `// Arithmetic
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
print "Last range:", round(last(range_size), 2)`
    },
    {
      id: 'fs-control-flow',
      title: 'Control Flow',
      content: `FinScript provides if/else, for loops, while loops, and switch statements.

**If / Else If / Else:**
• Braces \`{}\` are required for blocks
• \`else if\` for chained conditions
• Conditions must evaluate to Bool or Number (0 = false, non-zero = true)

**For Loops:**
• Range-based: \`for i in 1..10 { ... }\`
• Array iteration: \`for item in my_array { ... }\`
• \`break\` to exit early
• \`continue\` to skip iteration

**While Loops:**
• \`while condition { ... }\`
• \`break\` and \`continue\` supported

**Switch Statement:**
• Match a value against multiple cases
• \`else\` for default case
• No fall-through (each case is independent)`,
      codeExample: `// If / Else If / Else
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
}`
    },
    {
      id: 'fs-functions',
      title: 'User-Defined Functions',
      content: `Define reusable functions with \`fn\`. Functions support parameters, return values, and can access outer-scope variables.

**Syntax:**
• \`fn name(param1, param2) { ... }\`
• \`return value\` to return a value
• Last expression is NOT auto-returned — use \`return\`
• Functions can call other functions (recursion supported)
• Parameters are passed by value (numbers, strings) or by reference (arrays, series)

**Closures:**
Functions can read variables from the enclosing scope.

**Best Practices:**
• Keep functions focused on one task
• Use descriptive parameter names
• Return meaningful values for composability
• Document complex logic with comments`,
      codeExample: `// Simple function
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
print "Fib(20):", fibonacci(20)   // 4181

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

fn risk_filter(signal, symbol, max_atr) {
    atr_now = last(atr(symbol, 14))
    if atr_now > max_atr {
        return 0   // Too volatile
    }
    return signal
}

raw = compute_signal(AAPL)
final = risk_filter(raw, AAPL, 10.0)
print "Final signal:", final`
    },
    {
      id: 'fs-indicators',
      title: 'Technical Indicators',
      content: `FinScript includes 30+ built-in technical indicators implemented in pure Rust for maximum performance. All indicators return a Series (array of values over time).

**Moving Averages:**
• \`sma(symbol, period)\` — Simple Moving Average
• \`ema(symbol, period)\` — Exponential Moving Average
• \`wma(symbol, period)\` — Weighted Moving Average
• \`rma(symbol, period)\` — Relative Moving Average (Wilder's)
• \`hma(symbol, period)\` — Hull Moving Average

**Momentum:**
• \`rsi(symbol, period)\` — Relative Strength Index (0-100)
• \`stochastic(symbol, period)\` — Stochastic %K
• \`stochastic_d(symbol, period)\` — Stochastic %D
• \`cci(symbol, period)\` — Commodity Channel Index
• \`mfi(symbol, period)\` — Money Flow Index
• \`williams_r(symbol, period)\` — Williams %R
• \`roc(symbol, period)\` — Rate of Change
• \`momentum(symbol, period)\` — Momentum

**MACD:**
• \`macd(symbol)\` — MACD Line (default 12,26)
• \`macd_signal(symbol)\` — Signal Line (9-period EMA of MACD)
• \`macd_hist(symbol)\` — Histogram (MACD - Signal)
• \`macd(symbol, fast, slow, signal)\` — Custom periods

**Volatility:**
• \`atr(symbol, period)\` — Average True Range
• \`true_range(symbol)\` — True Range
• \`bollinger(symbol, period, std)\` — Bollinger Upper Band
• \`bollinger_middle(symbol, period)\` — Bollinger Middle
• \`bollinger_lower(symbol, period, std)\` — Bollinger Lower Band
• \`stdev(series, period)\` — Standard Deviation

**Trend:**
• \`adx(symbol, period)\` — Average Directional Index
• \`sar(symbol)\` — Parabolic SAR
• \`supertrend(symbol, period, mult)\` — Supertrend Value
• \`supertrend_dir(symbol, period, mult)\` — Supertrend Direction
• \`linreg(series, period)\` — Linear Regression

**Volume:**
• \`obv(symbol)\` — On-Balance Volume
• \`vwap(symbol)\` — Volume Weighted Average Price

**Crossover Detection:**
• \`crossover(series_a, series_b)\` — True when A crosses above B
• \`crossunder(series_a, series_b)\` — True when A crosses below B`,
      codeExample: `// Moving Averages
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
bb_mid = bollinger_middle(AAPL, 20)
bb_low = bollinger_lower(AAPL, 20, 2)
print "BB Width:", last(bb_up) - last(bb_low)

// Crossover detection
fast_ma = ema(AAPL, 12)
slow_ma = ema(AAPL, 26)
if crossover(fast_ma, slow_ma) {
    buy "Golden crossover"
}
if crossunder(fast_ma, slow_ma) {
    sell "Death crossover"
}`
    },
    {
      id: 'fs-price-functions',
      title: 'Price & Utility Functions',
      content: `Access OHLCV price data and utility functions for series manipulation.

**Price Functions:**
• \`close(symbol)\` — Closing prices series
• \`open(symbol)\` — Opening prices series
• \`high(symbol)\` — High prices series
• \`low(symbol)\` — Low prices series
• \`volume(symbol)\` — Volume series

**Series Utilities:**
• \`last(series)\` — Last (most recent) value
• \`first(series)\` — First value
• \`len(series)\` — Length of series/array
• \`highest(series, period)\` — Highest value in N bars
• \`lowest(series, period)\` — Lowest value in N bars
• \`change(series, period)\` — Period-over-period change
• \`cum(series)\` — Cumulative sum
• \`percentrank(series, period)\` — Percent rank
• \`pivothigh(series, left, right)\` — Pivot high detection
• \`pivotlow(series, left, right)\` — Pivot low detection
• \`sum(series)\` — Total sum of all values
• \`avg(series)\` — Average of all values
• \`max(series)\` — Maximum value
• \`min(series)\` — Minimum value

**Math Functions:**
• \`abs(x)\` — Absolute value
• \`sqrt(x)\` — Square root
• \`pow(base, exp)\` — Power
• \`log(x)\` — Natural logarithm
• \`log10(x)\` — Base-10 logarithm
• \`exp(x)\` — e^x
• \`round(x, decimals)\` — Round to decimals
• \`floor(x)\` — Floor
• \`ceil(x)\` — Ceiling
• \`sign(x)\` — Sign (-1, 0, or 1)
• \`sin(x)\`, \`cos(x)\`, \`tan(x)\` — Trigonometry

**String Functions:**
• \`str(value)\` — Convert to string
• \`round(value, decimals)\` — Format number`,
      codeExample: `// Price data access
prices = close(AAPL)
highs = high(AAPL)
lows = low(AAPL)
vols = volume(AAPL)

print "Data points:", len(prices)
print "Latest close:", last(prices)
print "Latest high:", last(highs)
print "Latest volume:", last(vols)

// Series utilities
print "20-bar high:", last(highest(prices, 20))
print "20-bar low:", last(lowest(prices, 20))
print "1-bar change:", last(change(prices, 1))

// Math functions
print "abs(-5):", abs(-5)
print "sqrt(144):", sqrt(144)
print "pow(2, 10):", pow(2, 10)
print "round(3.14159, 2):", round(3.14159, 2)

// Derived series
typical = (highs + lows + prices) / 3
body = prices - open(AAPL)
range_pct = (highs - lows) / prices * 100

print "Typical price:", round(last(typical), 2)
print "Body size:", round(last(body), 2)`
    },
    {
      id: 'fs-plotting',
      title: 'Visualization & Charts',
      content: `FinScript generates charts rendered with Lightweight Charts. Multiple chart types and overlays are supported.

**Candlestick Charts:**
• \`plot_candlestick symbol, "title"\`
• Displays full OHLCV data as candlestick chart

**Line Charts:**
• \`plot_line series, "label", "color"\`
• Color is optional (defaults to auto)
• Supports: "cyan", "orange", "red", "green", "blue", "gray", "purple", "white", "yellow"

**Indicator Plots:**
• \`plot series, "label"\`
• Generic plot — appears in separate panel

**Histogram:**
• \`plot_histogram series, "label"\`
• \`plot_histogram series, "label", "up_color", "down_color"\`

**Horizontal Lines:**
• \`hline value, "label", "color"\`
• Draws a constant horizontal reference line

**Drawing Tools:**
• \`line_new(x1, y1, x2, y2, "color", "style", width)\`
• \`label_new(x, y, "text", "color")\`
• \`box_new(x1, y1, x2, y2, "border_color", "bg_color", width)\`

**Shape Markers:**
• \`plot_shape condition, "shape", "location", "color", "text"\`
• Shapes: "triangleup", "triangledown", "circle", "cross"
• Locations: "abovebar", "belowbar"

**Tables (On-Chart):**
• \`table_new("position", rows, cols)\`
• \`table_cell(table, row, col, "text", "bg_color", "text_color")\`
• Positions: "top_right", "top_left", "bottom_right", "bottom_left"`,
      codeExample: `// Candlestick with overlays
plot_candlestick AAPL, "AAPL Price"

// Moving average overlays
ema_20 = ema(AAPL, 20)
ema_50 = ema(AAPL, 50)
plot_line ema_20, "EMA 20", "cyan"
plot_line ema_50, "EMA 50", "orange"

// Bollinger Bands
bb_up = bollinger(AAPL, 20, 2)
bb_low = bollinger_lower(AAPL, 20, 2)
plot_line bb_up, "BB Upper", "gray"
plot_line bb_low, "BB Lower", "gray"

// RSI in separate panel
rsi_val = rsi(AAPL, 14)
plot rsi_val, "RSI (14)"
hline 70, "Overbought", "red"
hline 30, "Oversold", "green"

// MACD histogram
macd_h = macd_hist(AAPL)
plot_histogram macd_h, "MACD Hist", "#26a69a", "#ef5350"

// Drawing tools
line_new(0, 145.0, 250, 145.0, "green", "solid", 2)
label_new(100, 155.0, "Key Level", "white")

// On-chart table
info = table_new("top_right", 3, 2)
table_cell(info, 0, 0, "RSI", "#333", "#0af")
table_cell(info, 0, 1, str(round(last(rsi_val), 1)), "#333", "#fff")`
    },
    {
      id: 'fs-signals',
      title: 'Trading Signals',
      content: `Generate buy/sell signals and use strategy mode for simulated execution.

**Simple Signals:**
• \`buy "reason"\` — Generate a buy signal
• \`sell "reason"\` — Generate a sell signal
• Signals include timestamp and latest close price

**Strategy Mode:**
For backtesting with position tracking:
• \`strategy.entry("name", "long"/"short", quantity)\` — Open position
• \`strategy.exit("name", "entry_name")\` — Close position
• \`strategy_position_size()\` — Current position size
• \`strategy_equity()\` — Current equity value

**Alerts:**
• \`alert("message")\` — Trigger an alert
• \`alert("message", "type")\` — Types: "once", "every_bar"
• \`alertcondition(condition, "title", "message")\` — Conditional alert

**Best Practices:**
• Use multiple confirmation indicators before signaling
• Include descriptive messages for debugging
• Implement position sizing with ATR-based stops
• Test with print statements before live execution`,
      codeExample: `// Simple buy/sell signals
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
}
if rsi_now > 75 {
    strategy.exit("TP", "Long")
    print "Take profit at", price
}

// Alerts
if rsi_now > 70 {
    alert("RSI Overbought: " + str(round(rsi_now, 1)), "once")
}
alertcondition(
    ema_f > ema_s,
    "Bullish Trend",
    "EMA 12 above EMA 26"
)`
    },
    {
      id: 'fs-structs',
      title: 'Structs & Data Structures',
      content: `FinScript supports user-defined structs, maps (dictionaries), and matrices for complex data modeling.

**Structs:**
• Define with \`struct Name { field: type }\`
• Create instances with \`Name { field: value }\`
• Access fields with dot notation: \`instance.field\`
• Supported types: \`string\`, \`int\`, \`float\`, \`bool\`

**Maps (Dictionaries):**
• \`map_new()\` — Create empty map
• \`map_put(map, key, value)\` — Set key-value pair
• \`map_get(map, key)\` — Get value by key
• \`map_size(map)\` — Number of entries
• \`map_contains(map, key)\` — Check if key exists
• \`map.keys()\` — Get all keys

**Matrices:**
• \`matrix_new(rows, cols, default)\` — Create matrix
• \`matrix_set(m, row, col, value)\` — Set cell value
• \`matrix_get(m, row, col)\` — Get cell value
• \`matrix_rows(m)\` — Number of rows
• \`matrix_cols(m)\` — Number of columns

**Arrays:**
• Literal: \`[10, 20, 30]\`
• \`push(arr, value)\` — Append value
• \`slice(arr, start, end)\` — Sub-array
• \`arr[index]\` — Index access
• \`arr[index] = value\` — Index assignment`,
      codeExample: `// Struct definition and usage
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
print "Direction:", setup.direction
print "Confidence:", setup.confidence

// Maps for portfolio tracking
portfolio = map_new()
map_put(portfolio, "AAPL", 100)
map_put(portfolio, "MSFT", 50)
map_put(portfolio, "GOOGL", 25)

print "AAPL shares:", map_get(portfolio, "AAPL")
print "Total positions:", map_size(portfolio)
print "Has TSLA?", map_contains(portfolio, "TSLA")

// Matrix for correlation data
corr = matrix_new(3, 3, 0.0)
matrix_set(corr, 0, 0, 1.0)    // AAPL-AAPL
matrix_set(corr, 0, 1, 0.85)   // AAPL-MSFT
matrix_set(corr, 1, 0, 0.85)   // MSFT-AAPL
matrix_set(corr, 1, 1, 1.0)    // MSFT-MSFT

print "AAPL-MSFT corr:", matrix_get(corr, 0, 1)`
    },
    {
      id: 'fs-multi-symbol',
      title: 'Multi-Symbol Analysis',
      content: `FinScript automatically fetches data for any uppercase ticker symbol used in your script. You can analyze multiple assets simultaneously.

**Ticker Symbols:**
• Any ALL-UPPERCASE identifier is treated as a ticker symbol
• Examples: \`AAPL\`, \`MSFT\`, \`GOOGL\`, \`TSLA\`, \`SPY\`
• Data is automatically generated (deterministic random walk)
• 180 days of daily OHLCV data per symbol

**request.security():**
• \`request.security(SYMBOL)\` — Fetch close data for another symbol
• Similar to Pine Script's security() function

**Cross-Symbol Operations:**
• Compare indicators across symbols
• Compute spreads and ratios
• Build sector scanners with loops

**Relative Strength:**
• Compare RSI/momentum across assets
• Identify strongest/weakest stocks
• Build pair trading setups`,
      codeExample: `// Analyze multiple symbols
aapl_close = close(AAPL)
msft_close = close(MSFT)
googl_close = close(GOOGL)

print "AAPL:", round(last(aapl_close), 2)
print "MSFT:", round(last(msft_close), 2)
print "GOOGL:", round(last(googl_close), 2)

// Sector momentum scanner
fn scan_momentum(ticker, name) {
    ema_f = last(ema(ticker, 12))
    ema_s = last(ema(ticker, 26))
    rsi_v = last(rsi(ticker, 14))
    signal = ema_f > ema_s ? "BULL" : "BEAR"
    print name, ":", signal, "| RSI:", round(rsi_v, 1)
    if ema_f > ema_s and rsi_v < 70 {
        buy name
    }
}

print "=== Momentum Scanner ==="
scan_momentum(AAPL, "Apple")
scan_momentum(MSFT, "Microsoft")
scan_momentum(GOOGL, "Google")

// Spread analysis
aapl_sma = sma(AAPL, 20)
msft_sma = sma(MSFT, 20)
spread = aapl_sma - msft_sma
print "AAPL-MSFT Spread:", round(last(spread), 2)

// Multi-chart output
plot_candlestick AAPL, "AAPL"
plot_candlestick MSFT, "MSFT"`
    },
    {
      id: 'fs-colors-drawing',
      title: 'Colors & Drawing System',
      content: `FinScript provides a color system and drawing tools for chart annotations.

**Named Colors:**
• \`color.red\`, \`color.green\`, \`color.blue\`
• \`color.orange\`, \`color.yellow\`, \`color.white\`
• \`color.cyan\`, \`color.purple\`, \`color.gray\`

**Custom RGB:**
• \`color.rgb(r, g, b)\` — Create custom color
• \`color.rgb(r, g, b, alpha)\` — With transparency (0-100)

**Color Properties:**
• \`.hex\` — Hex string representation
• \`.r\`, \`.g\`, \`.b\` — Component values

**Drawing Primitives:**
• Lines: \`line_new(x1, y1, x2, y2, color, style, width)\`
  - Styles: "solid", "dashed", "dotted"
• Labels: \`label_new(x, y, text, color)\`
• Boxes: \`box_new(x1, y1, x2, y2, border, bg, width)\`

**Horizontal Lines:**
• \`hline value, "label", "color"\`
• Useful for support/resistance, overbought/oversold levels`,
      codeExample: `// Color system
red = color.red
green = color.green
custom = color.rgb(100, 200, 150)
faded_red = color.rgb(255, 0, 0, 50)

// Chart with colored overlays
plot_candlestick AAPL, "AAPL Chart"

ema_20 = ema(AAPL, 20)
ema_50 = ema(AAPL, 50)
plot_line ema_20, "EMA 20", "cyan"
plot_line ema_50, "EMA 50", "orange"

// Horizontal reference lines
rsi_val = rsi(AAPL, 14)
plot rsi_val, "RSI"
hline 70, "Overbought", "red"
hline 50, "Midline", "gray"
hline 30, "Oversold", "green"

// Drawing support/resistance
line_new(0, 145.0, 250, 145.0, "green", "solid", 2)
line_new(0, 165.0, 250, 165.0, "red", "dashed", 2)

// Labels
label_new(100, 155.0, "Key Level", "white")
label_new(200, 170.0, "Resistance", "red")

// Box for price zone
box_new(50, 148.0, 200, 152.0, "blue", "rgba(0,0,255,0.1)", 1)`
    },
    {
      id: 'fs-imports',
      title: 'Modules & Imports',
      content: `FinScript supports a module system for organizing reusable code.

**Import Syntax:**
• \`import module.name as alias\`
• Imports are currently symbolic (no file resolution yet)
• Used to document dependencies and organize code

**Export:**
• \`export function_name\`
• Marks a function for use by other scripts

**Modular Patterns:**
• Separate indicator calculations from signal logic
• Create reusable utility libraries
• Build composable strategy components
• Share risk management functions

**Future Roadmap:**
• File-based module resolution
• Standard library of common patterns
• Package manager for sharing strategies
• Version-pinned dependencies`,
      codeExample: `// Import modules (symbolic)
import indicators.custom as custom
import strategies.momentum as mom
import utils.math as math_utils

print "Modules imported"

// Exportable utility functions
fn calculate_sharpe(returns, risk_free) {
    avg_ret = avg(returns)
    std_ret = 0.15  // simplified
    return (avg_ret - risk_free) / std_ret
}

fn max_drawdown(equity_curve) {
    peak = first(equity_curve)
    max_dd = 0
    for val in equity_curve {
        if val > peak {
            peak = val
        }
        dd = (peak - val) / peak
        if dd > max_dd {
            max_dd = dd
        }
    }
    return max_dd
}

export calculate_sharpe
export max_drawdown

// Use the functions
returns = [0.02, -0.01, 0.03, 0.01, -0.02, 0.04]
sharpe = calculate_sharpe(returns, 0.005)
print "Sharpe:", round(sharpe, 3)

equity = [100000, 102000, 101500, 104000, 103000, 106000]
dd = max_drawdown(equity)
print "Max Drawdown:", round(dd * 100, 2), "%"`
    },
    {
      id: 'fs-complete-example',
      title: 'Complete Trading System',
      content: `This example demonstrates a full trading system combining all FinScript features: structs for configuration, functions for modularity, indicators for analysis, strategy mode for execution, and visualization for review.

**System Components:**
• Configuration via struct
• Multi-indicator scoring model
• ATR-based position sizing
• Dynamic stop-loss and take-profit
• On-chart information table
• Full visualization with overlays

**Scoring Model:**
• +2/-2 for EMA trend direction
• +1/-1 for RSI momentum
• +1/-1 for MACD direction
• Volatility filter (flatten if ATR > 5% of price)
• Trade when score >= 3 or <= -3`,
      codeExample: `// === Configuration ===
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
atr_val = atr(AAPL, 14)
macd_h = macd_hist(AAPL)

price = last(close(AAPL))
rsi_now = last(rsi_val)
atr_now = last(atr_val)
ema_f = last(ema_fast)
ema_s = last(ema_slow)

// === Scoring Function ===
fn calculate_score() {
    score = 0
    if ema_f > ema_s { score += 2 }
    else { score -= 2 }
    if rsi_now > 50 and rsi_now < 70 { score += 1 }
    else if rsi_now < 50 and rsi_now > 30 { score -= 1 }
    if last(macd_h) > 0 { score += 1 }
    else { score -= 1 }
    if atr_now > price * 0.05 { score = 0 }
    return score
}

// === Execution ===
signal_score = calculate_score()
stop_dist = atr_now * config.atr_mult
pos_size = round((100000 * 0.02) / stop_dist, 0)

print "Score:", signal_score, "| Price:", round(price, 2)
print "Position:", pos_size, "shares"

if signal_score >= 3 {
    strategy.entry("Buy", "long", pos_size)
    alert("Strong Buy! Score: " + str(signal_score))
} else if signal_score <= -3 {
    strategy.entry("Sell", "short", pos_size)
}

// === Visualization ===
plot_candlestick AAPL, "Trading System"
plot_line ema_fast, "Fast EMA", "cyan"
plot_line ema_slow, "Slow EMA", "orange"
plot rsi_val, "RSI"
hline 70, "OB", "red"
hline 30, "OS", "green"`
    }
  ]
};
