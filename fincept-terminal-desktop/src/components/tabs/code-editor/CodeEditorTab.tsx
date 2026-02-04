import React, { useState, useRef, useEffect, useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  Play, Save, FileText, Plus, X, Terminal as TerminalIcon,
  Upload, Trash2, ChevronRight, Code, Braces, Hash,
  AlertTriangle, CheckCircle, Zap, FolderOpen, Copy,
  RotateCcw, Search, Replace, ChevronDown, Settings,
  Maximize2, Minimize2, SplitSquareHorizontal, BookOpen
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { open, save } from '@tauri-apps/plugin-dialog';
import { FinScriptOutputPanel, FinScriptExecutionResult } from './FinScriptOutputPanel';
import { showConfirm } from '@/utils/notifications';

// ─── Design System Constants ────────────────────────────────────────────────
const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Interfaces ─────────────────────────────────────────────────────────────
interface EditorFile {
  id: string;
  name: string;
  content: string;
  language: 'finscript' | 'python' | 'javascript' | 'json';
  unsaved: boolean;
  cursorLine: number;
  cursorCol: number;
}

interface SearchState {
  isOpen: boolean;
  query: string;
  replaceQuery: string;
  showReplace: boolean;
  matchCount: number;
  currentMatch: number;
}

// ─── Language Config ────────────────────────────────────────────────────────
const LANG_CONFIG: Record<string, { icon: React.ReactNode; color: string; ext: string }> = {
  finscript: { icon: <Zap size={10} />, color: F.ORANGE, ext: '.fincept' },
  python: { icon: <Code size={10} />, color: F.BLUE, ext: '.py' },
  javascript: { icon: <Braces size={10} />, color: F.YELLOW, ext: '.js' },
  json: { icon: <Hash size={10} />, color: F.GREEN, ext: '.json' },
};

const DEFAULT_FINSCRIPT = `// FinScript - EMA/RSI Crossover Strategy
// Data is generated synthetically for instant execution

// Calculate indicators on AAPL
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_val = rsi(AAPL, 14)

// Get latest values
fast = last(ema_fast)
slow = last(ema_slow)
rsi_now = last(rsi_val)

// Print analysis
print "EMA(12):", fast
print "EMA(26):", slow
print "RSI(14):", rsi_now

// Trading logic
if fast > slow {
    if rsi_now < 70 {
        buy "Bullish crossover + RSI not overbought"
    }
}

if fast < slow {
    if rsi_now > 30 {
        sell "Bearish crossover + RSI not oversold"
    }
}

// Visualize
plot_candlestick AAPL, "AAPL Price"
plot_line ema_fast, "EMA (12)", "cyan"
plot_line ema_slow, "EMA (26)", "orange"
plot rsi_val, "RSI (14)"
`;

// ─── Example Scripts ─────────────────────────────────────────────────────────
interface ExampleScript {
  id: string;
  name: string;
  category: string;
  description: string;
  content: string;
}

const EXAMPLE_SCRIPTS: ExampleScript[] = [
  {
    id: 'ex_quickstart',
    name: '01_quickstart.fincept',
    category: 'Getting Started',
    description: 'Basic syntax, variables, and indicators',
    content: `// ═══════════════════════════════════════════════════════════════
// FinScript Quick Start Guide
// Learn the basics: variables, indicators, plotting
// ═══════════════════════════════════════════════════════════════

// ─── Variables ───────────────────────────────────────────────
// Assign values to variables (no "let" or "var" needed)
ticker = "AAPL"
period = 14
threshold = 70.0

// ─── Data Types ──────────────────────────────────────────────
my_number = 42.5
my_string = "Hello FinScript"
my_bool = true
my_array = [10, 20, 30, 40, 50]

// Print values to output
print "Number:", my_number
print "String:", my_string
print "Array length:", len(my_array)

// ─── Fetching Market Data ────────────────────────────────────
// Use UPPERCASE ticker symbols to fetch OHLCV data
// Data is auto-fetched from Yahoo Finance
close_prices = close(AAPL)
high_prices = high(AAPL)
volume_data = volume(AAPL)

print "Data points:", len(close_prices)
print "Latest close:", last(close_prices)

// ─── Technical Indicators ────────────────────────────────────
// All indicators return Series (arrays of values over time)
sma_20 = sma(AAPL, 20)
ema_12 = ema(AAPL, 12)
rsi_14 = rsi(AAPL, 14)

print "SMA(20):", last(sma_20)
print "EMA(12):", last(ema_12)
print "RSI(14):", last(rsi_14)

// ─── Plotting ────────────────────────────────────────────────
// Plot candlestick chart
plot_candlestick AAPL, "Apple Inc."

// Plot indicators as line overlays
plot_line sma_20, "SMA (20)", "blue"
plot_line ema_12, "EMA (12)", "orange"

// Plot RSI separately
plot rsi_14, "RSI (14)"

// Plot horizontal reference lines
hline 70, "Overbought", "red"
hline 30, "Oversold", "green"
`,
  },
  {
    id: 'ex_control_flow',
    name: '02_control_flow.fincept',
    category: 'Getting Started',
    description: 'If/else, loops, functions, ternary',
    content: `// ═══════════════════════════════════════════════════════════════
// Control Flow & Functions
// If/else, for/while loops, user functions, ternary operator
// ═══════════════════════════════════════════════════════════════

// ─── If / Else If / Else ─────────────────────────────────────
rsi_val = last(rsi(AAPL, 14))

if rsi_val > 70 {
    print "RSI is overbought:", rsi_val
} else if rsi_val < 30 {
    print "RSI is oversold:", rsi_val
} else {
    print "RSI is neutral:", rsi_val
}

// ─── Ternary Operator ────────────────────────────────────────
signal = rsi_val > 50 ? "Bullish" : "Bearish"
print "Market sentiment:", signal

// ─── For Loops ───────────────────────────────────────────────
// Iterate over a range
total = 0
for i in 1..6 {
    total += i
}
print "Sum of 1-5:", total

// Iterate over an array
tickers = ["AAPL", "MSFT", "GOOGL"]
for t in tickers {
    print "Ticker:", t
}

// ─── While Loops ─────────────────────────────────────────────
counter = 0
result = 1
while counter < 10 {
    result *= 2
    counter += 1
}
print "2^10 =", result

// ─── User-Defined Functions ──────────────────────────────────
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

print "Fibonacci(10):", fibonacci(10)
print "Fibonacci(20):", fibonacci(20)

// Functions with financial logic
fn is_golden_cross(symbol, fast_period, slow_period) {
    fast = last(ema(symbol, fast_period))
    slow = last(ema(symbol, slow_period))
    return fast > slow
}

if is_golden_cross(AAPL, 50, 200) {
    print "AAPL: Golden Cross detected!"
    buy "Golden Cross - Long Signal"
} else {
    print "AAPL: No golden cross"
}

// ─── Switch Statement ────────────────────────────────────────
day_type = "monday"
switch day_type {
    "monday" {
        print "Start of trading week"
    }
    "friday" {
        print "End of trading week"
    }
    else {
        print "Mid-week trading"
    }
}
`,
  },
  {
    id: 'ex_indicators',
    name: '03_indicators.fincept',
    category: 'Technical Analysis',
    description: '30+ built-in indicators: MA, RSI, MACD, etc.',
    content: `// ═══════════════════════════════════════════════════════════════
// Technical Indicators Reference
// FinScript includes 30+ built-in indicators
// ═══════════════════════════════════════════════════════════════

// ─── Moving Averages ─────────────────────────────────────────
sma_val = sma(AAPL, 20)       // Simple Moving Average
ema_val = ema(AAPL, 12)       // Exponential Moving Average
wma_val = wma(AAPL, 14)       // Weighted Moving Average
rma_val = rma(AAPL, 14)       // Relative Moving Average (Wilder)
hma_val = hma(AAPL, 9)        // Hull Moving Average

print "SMA(20):", last(sma_val)
print "EMA(12):", last(ema_val)
print "HMA(9):", last(hma_val)

// ─── Momentum Indicators ────────────────────────────────────
rsi_val = rsi(AAPL, 14)       // Relative Strength Index
stoch_k = stochastic(AAPL, 14) // Stochastic %K
stoch_d = stochastic_d(AAPL, 14) // Stochastic %D
cci_val = cci(AAPL, 20)       // Commodity Channel Index
mfi_val = mfi(AAPL, 14)       // Money Flow Index
wpr_val = williams_r(AAPL, 14) // Williams %R
roc_val = roc(AAPL, 12)       // Rate of Change
mom_val = momentum(AAPL, 10)  // Momentum

print "RSI:", last(rsi_val)
print "Stochastic %K:", last(stoch_k)
print "CCI:", last(cci_val)
print "MFI:", last(mfi_val)
print "Williams %R:", last(wpr_val)

// ─── MACD ────────────────────────────────────────────────────
macd_line = macd(AAPL)              // MACD Line (12,26)
macd_sig = macd_signal(AAPL)        // Signal Line (9)
macd_h = macd_hist(AAPL)            // Histogram
// Custom periods: macd(AAPL, 8, 21, 5)

print "MACD:", last(macd_line)
print "Signal:", last(macd_sig)

// ─── Volatility Indicators ───────────────────────────────────
atr_val = atr(AAPL, 14)       // Average True Range
tr_val = true_range(AAPL)     // True Range
bb_upper = bollinger(AAPL, 20, 2)         // Bollinger Upper
bb_middle = bollinger_middle(AAPL, 20)    // Bollinger Middle
bb_lower = bollinger_lower(AAPL, 20, 2)   // Bollinger Lower
stdev_val = stdev(close(AAPL), 20)        // Standard Deviation

print "ATR(14):", last(atr_val)
print "BB Upper:", last(bb_upper)
print "BB Lower:", last(bb_lower)

// ─── Trend Indicators ────────────────────────────────────────
adx_val = adx(AAPL, 14)       // Average Directional Index
sar_val = sar(AAPL)            // Parabolic SAR
st_val = supertrend(AAPL, 10, 3)   // Supertrend
st_dir = supertrend_dir(AAPL, 10, 3) // Supertrend Direction
linreg_val = linreg(close(AAPL), 20) // Linear Regression

print "ADX:", last(adx_val)
print "SAR:", last(sar_val)
print "Supertrend:", last(st_val)

// ─── Volume Indicators ───────────────────────────────────────
obv_val = obv(AAPL)           // On-Balance Volume
vwap_val = vwap(AAPL)         // Volume Weighted Avg Price

print "OBV:", last(obv_val)
print "VWAP:", last(vwap_val)

// ─── Utility Functions ───────────────────────────────────────
highest_20 = highest(close(AAPL), 20)    // Highest in N bars
lowest_20 = lowest(close(AAPL), 20)      // Lowest in N bars
chg = change(close(AAPL), 1)             // Period-over-period change
cumulative = cum(close(AAPL))            // Cumulative sum
prank = percentrank(close(AAPL), 20)     // Percent Rank

print "20-bar High:", last(highest_20)
print "20-bar Low:", last(lowest_20)

// ─── Crossover Detection ─────────────────────────────────────
fast_ma = ema(AAPL, 12)
slow_ma = ema(AAPL, 26)

if crossover(fast_ma, slow_ma) {
    buy "EMA 12/26 Bullish Crossover"
}
if crossunder(fast_ma, slow_ma) {
    sell "EMA 12/26 Bearish Crossover"
}

// ─── Visualization ───────────────────────────────────────────
plot_candlestick AAPL, "AAPL Price"
plot_line bb_upper, "BB Upper", "gray"
plot_line bb_middle, "BB Middle", "blue"
plot_line bb_lower, "BB Lower", "gray"
plot_histogram macd_h, "MACD Histogram"
`,
  },
  {
    id: 'ex_strategy',
    name: '04_strategy_mode.fincept',
    category: 'Strategies',
    description: 'Strategy backtesting with entry/exit/PnL',
    content: `// ═══════════════════════════════════════════════════════════════
// Strategy Mode - Backtesting Framework
// Uses strategy.entry() / strategy.exit() for PnL tracking
// ═══════════════════════════════════════════════════════════════

// ─── Strategy Configuration ──────────────────────────────────
// Inputs (configurable parameters)
// input.int(14, "RSI Period")
// input.float(70, "Overbought Level")
// input.float(30, "Oversold Level")

// ─── Indicator Calculation ───────────────────────────────────
rsi_val = rsi(MSFT, 14)
ema_fast = ema(MSFT, 9)
ema_slow = ema(MSFT, 21)
atr_val = atr(MSFT, 14)

rsi_now = last(rsi_val)
fast_now = last(ema_fast)
slow_now = last(ema_slow)
atr_now = last(atr_val)
price = last(close(MSFT))

// ─── Entry Conditions ────────────────────────────────────────
// Long: EMA crossover + RSI not overbought
long_condition = fast_now > slow_now and rsi_now < 70
// Short: EMA crossunder + RSI not oversold
short_condition = fast_now < slow_now and rsi_now > 30

// ─── Strategy Execution ──────────────────────────────────────
if long_condition {
    strategy.entry("Long", "long", 100)
    print "[ENTRY] Long at", price, "| RSI:", rsi_now
}

if short_condition {
    strategy.entry("Short", "short", 100)
    print "[ENTRY] Short at", price, "| RSI:", rsi_now
}

// ─── Exit Conditions ─────────────────────────────────────────
// Exit long if RSI overbought
if rsi_now > 75 {
    strategy.exit("TP Long", "Long")
    print "[EXIT] Take profit on long | RSI:", rsi_now
}

// Exit short if RSI oversold
if rsi_now < 25 {
    strategy.exit("TP Short", "Short")
    print "[EXIT] Take profit on short | RSI:", rsi_now
}

// ─── Portfolio Metrics ───────────────────────────────────────
pos_size = strategy_position_size()
equity = strategy_equity()
print "Position Size:", pos_size
print "Equity:", equity

// ─── Visualization ───────────────────────────────────────────
plot_candlestick MSFT, "MSFT Strategy"
plot_line ema_fast, "EMA 9", "cyan"
plot_line ema_slow, "EMA 21", "orange"
plot rsi_val, "RSI (14)"
hline 70, "Overbought", "red"
hline 30, "Oversold", "green"
`,
  },
  {
    id: 'ex_multi_symbol',
    name: '05_multi_symbol.fincept',
    category: 'Strategies',
    description: 'Multi-asset analysis with request.security()',
    content: `// ═══════════════════════════════════════════════════════════════
// Multi-Symbol Analysis
// Compare multiple assets, relative strength, correlations
// ═══════════════════════════════════════════════════════════════

// ─── Fetch Multiple Symbols ──────────────────────────────────
// FinScript auto-fetches data for any UPPERCASE ticker used
aapl_close = close(AAPL)
msft_close = close(MSFT)
googl_close = close(GOOGL)

print "AAPL:", last(aapl_close)
print "MSFT:", last(msft_close)
print "GOOGL:", last(googl_close)

// ─── request.security() ─────────────────────────────────────
// Fetch data for another symbol (like PineScript)
spy_data = request.security(SPY)
print "SPY Close:", last(spy_data)

// ─── Relative Strength ──────────────────────────────────────
// Compare stock performance to benchmark
aapl_rsi = rsi(AAPL, 14)
msft_rsi = rsi(MSFT, 14)
googl_rsi = rsi(GOOGL, 14)

print "\\n=== Relative Strength ==="
print "AAPL RSI:", last(aapl_rsi)
print "MSFT RSI:", last(msft_rsi)
print "GOOGL RSI:", last(googl_rsi)

// ─── Sector Momentum Scan ────────────────────────────────────
fn momentum_signal(ticker, name) {
    ema_f = last(ema(ticker, 12))
    ema_s = last(ema(ticker, 26))
    rsi_v = last(rsi(ticker, 14))
    signal = ema_f > ema_s ? "BULLISH" : "BEARISH"
    print name, ":", signal, "| RSI:", rsi_v
    if ema_f > ema_s and rsi_v < 70 {
        buy name
    }
}

print "\\n=== Momentum Scanner ==="
momentum_signal(AAPL, "Apple")
momentum_signal(MSFT, "Microsoft")
momentum_signal(GOOGL, "Google")

// ─── Spread Analysis ─────────────────────────────────────────
// Cross-series arithmetic
aapl_sma = sma(AAPL, 20)
msft_sma = sma(MSFT, 20)
spread = aapl_sma - msft_sma

print "\\n=== Price Spread ==="
print "AAPL-MSFT Spread:", last(spread)

// ─── Multi-Chart Output ──────────────────────────────────────
plot_candlestick AAPL, "AAPL"
plot_candlestick MSFT, "MSFT"
plot_line aapl_rsi, "AAPL RSI", "blue"
plot_line msft_rsi, "MSFT RSI", "red"
hline 50, "Midline"
`,
  },
  {
    id: 'ex_structs_maps',
    name: '06_structs_maps.fincept',
    category: 'Data Structures',
    description: 'Structs, maps, matrices, arrays',
    content: `// ═══════════════════════════════════════════════════════════════
// Data Structures
// Structs, Maps, Matrices, and Arrays
// ═══════════════════════════════════════════════════════════════

// ─── Arrays ──────────────────────────────────────────────────
prices = [150.0, 152.5, 148.0, 155.0, 153.5]
print "Prices:", prices
print "Length:", len(prices)
print "Max:", max(prices)
print "Min:", min(prices)
print "Average:", avg(prices)
print "First:", first(prices)
print "Last:", last(prices)

// Array operations
push(prices, 157.0)
print "After push:", prices
sliced = slice(prices, 1, 4)
print "Sliced [1:4]:", sliced

// Index access and assignment
prices[0] = 151.0
print "Modified:", prices[0]

// ─── Structs (User-Defined Types) ────────────────────────────
struct TradeSignal {
    symbol: string
    direction: string
    price: float
    confidence: float
}

// Create struct instance
my_signal = TradeSignal {
    symbol: "AAPL",
    direction: "long",
    price: 150.50,
    confidence: 0.85
}

// Access fields via dot notation
print "\\n=== Trade Signal ==="
print "Symbol:", my_signal.symbol
print "Direction:", my_signal.direction
print "Price:", my_signal.price
print "Confidence:", my_signal.confidence

// ─── Maps (Key-Value Dictionaries) ──────────────────────────
// Create empty map and add entries
portfolio = map_new()
map_put(portfolio, "AAPL", 100)
map_put(portfolio, "MSFT", 50)
map_put(portfolio, "GOOGL", 25)

print "\\n=== Portfolio ==="
print "AAPL shares:", map_get(portfolio, "AAPL")
print "MSFT shares:", map_get(portfolio, "MSFT")
print "Total positions:", map_size(portfolio)
print "Has TSLA?", map_contains(portfolio, "TSLA")

// Map method calls
keys = portfolio.keys()
print "Holdings:", keys

// ─── Matrices ────────────────────────────────────────────────
// Create a 3x3 correlation matrix
corr_matrix = matrix_new(3, 3, 0.0)
matrix_set(corr_matrix, 0, 0, 1.0)   // AAPL-AAPL
matrix_set(corr_matrix, 0, 1, 0.85)  // AAPL-MSFT
matrix_set(corr_matrix, 0, 2, 0.72)  // AAPL-GOOGL
matrix_set(corr_matrix, 1, 0, 0.85)  // MSFT-AAPL
matrix_set(corr_matrix, 1, 1, 1.0)   // MSFT-MSFT
matrix_set(corr_matrix, 1, 2, 0.78)  // MSFT-GOOGL
matrix_set(corr_matrix, 2, 0, 0.72)
matrix_set(corr_matrix, 2, 1, 0.78)
matrix_set(corr_matrix, 2, 2, 1.0)

print "\\n=== Correlation Matrix ==="
print "Rows:", matrix_rows(corr_matrix)
print "Cols:", matrix_cols(corr_matrix)
print "AAPL-MSFT corr:", matrix_get(corr_matrix, 0, 1)
print "AAPL-GOOGL corr:", matrix_get(corr_matrix, 0, 2)

// ─── For Loops with Data Structures ─────────────────────────
print "\\n=== Price Analysis ==="
data = [100, 105, 103, 108, 112, 107, 115]
running_sum = 0
for val in data {
    running_sum += val
}
print "Sum:", running_sum
print "Mean:", running_sum / len(data)
`,
  },
  {
    id: 'ex_colors_drawing',
    name: '07_colors_drawing.fincept',
    category: 'Visualization',
    description: 'Colors, drawings, shapes, and tables',
    content: `// ═══════════════════════════════════════════════════════════════
// Colors, Drawings & Tables
// Visual annotations, chart drawing tools, color system
// ═══════════════════════════════════════════════════════════════

// ─── Color System ────────────────────────────────────────────
// Named colors
red = color.red
green = color.green
blue = color.blue
orange = color.orange

// Custom RGB colors
my_color = color.rgb(100, 200, 150)        // RGB
faded = color.rgb(255, 0, 0, 50)           // With 50% transparency
gold = color.rgb(255, 215, 0)

print "Red hex:", red.hex
print "Custom color:", my_color.hex
print "Red R:", red.r
print "Red G:", red.g

// ─── Chart Plotting with Colors ──────────────────────────────
ema_20 = ema(AAPL, 20)
ema_50 = ema(AAPL, 50)
rsi_val = rsi(AAPL, 14)

plot_candlestick AAPL, "Apple Inc."
plot_line ema_20, "EMA 20", "cyan"
plot_line ema_50, "EMA 50", "orange"

// Histogram with up/down colors
macd_h = macd_hist(AAPL)
plot_histogram macd_h, "MACD Hist", "#26a69a", "#ef5350"

// ─── Horizontal Lines ────────────────────────────────────────
plot rsi_val, "RSI"
hline 70, "Overbought", "red"
hline 50, "Midline", "gray"
hline 30, "Oversold", "green"

// ─── Drawing Tools ───────────────────────────────────────────
// Line: line_new(x1, y1, x2, y2, color, style, width)
support = line_new(0, 145.0, 250, 145.0, "green", "solid", 2)
resistance = line_new(0, 165.0, 250, 165.0, "red", "dashed", 2)

// Label: label_new(x, y, text, color)
label_new(100, 155.0, "Key Level", "white")
label_new(200, 170.0, "Resistance Zone", "red")

// Box: box_new(x1, y1, x2, y2, border_color, bg_color, width)
box_new(50, 148.0, 200, 152.0, "blue", "rgba(0,0,255,0.1)", 1)

print "\\n=== Drawing Objects Created ==="
print "Support line at $145"
print "Resistance line at $165"
print "Labels and boxes placed"

// ─── Plot Shapes (Markers) ───────────────────────────────────
rsi_now = last(rsi_val)
// Shape markers on chart when conditions are met
if rsi_now > 70 {
    plot_shape true, "triangledown", "abovebar", "red", "Overbought"
}
if rsi_now < 30 {
    plot_shape true, "triangleup", "belowbar", "green", "Oversold"
}

// ─── Tables (On-Chart Data Display) ─────────────────────────
// Create a summary table
summary = table_new("top_right", 4, 2)
table_cell(summary, 0, 0, "Metric", "#333333", "#ffffff")
table_cell(summary, 0, 1, "Value", "#333333", "#ffffff")
table_cell(summary, 1, 0, "Price", "#1a1a1a", "#00e5ff")
table_cell(summary, 1, 1, str(last(close(AAPL))), "#1a1a1a", "#ffffff")
table_cell(summary, 2, 0, "RSI", "#1a1a1a", "#00e5ff")
table_cell(summary, 2, 1, str(round(rsi_now, 1)), "#1a1a1a", "#ffffff")
table_cell(summary, 3, 0, "EMA 20", "#1a1a1a", "#00e5ff")
table_cell(summary, 3, 1, str(round(last(ema_20), 2)), "#1a1a1a", "#ffffff")

print "\\nTable created at top-right corner"
`,
  },
  {
    id: 'ex_alerts',
    name: '08_alerts.fincept',
    category: 'Automation',
    description: 'Alert system: alert(), alertcondition()',
    content: `// ═══════════════════════════════════════════════════════════════
// Alerts & Conditions
// Trigger notifications based on market conditions
// ═══════════════════════════════════════════════════════════════

// ─── Setup ───────────────────────────────────────────────────
rsi_val = rsi(AAPL, 14)
macd_line = macd(AAPL)
macd_sig = macd_signal(AAPL)
ema_20 = ema(AAPL, 20)
ema_50 = ema(AAPL, 50)

rsi_now = last(rsi_val)
price = last(close(AAPL))
ema20_now = last(ema_20)
ema50_now = last(ema_50)

// ─── Basic Alerts ────────────────────────────────────────────
// alert(message, type) - triggers an alert
if rsi_now > 70 {
    alert("AAPL RSI Overbought: " + str(round(rsi_now, 1)), "once")
}

if rsi_now < 30 {
    alert("AAPL RSI Oversold: " + str(round(rsi_now, 1)), "once")
}

// ─── Alert Conditions ────────────────────────────────────────
// alertcondition(condition, title, message)
// Only fires when condition is true

// Price alert
alertcondition(
    price > ema20_now,
    "Price Above EMA 20",
    "AAPL trading above 20-day EMA at " + str(round(price, 2))
)

// Trend alert
alertcondition(
    ema20_now > ema50_now,
    "Bullish Trend",
    "EMA 20 above EMA 50 - Uptrend confirmed"
)

// MACD crossover alert
macd_cross = crossover(macd_line, macd_sig)
if macd_cross {
    alert("MACD Bullish Crossover on AAPL!", "every_bar")
}

// ─── Multi-Condition Alerts ──────────────────────────────────
// Combine multiple conditions for stronger signals
strong_buy = rsi_now < 40 and ema20_now > ema50_now and price > ema20_now
strong_sell = rsi_now > 60 and ema20_now < ema50_now and price < ema20_now

if strong_buy {
    alert("STRONG BUY: RSI low + Uptrend + Price above EMA")
    buy "Multi-factor buy signal"
}

if strong_sell {
    alert("STRONG SELL: RSI high + Downtrend + Price below EMA")
    sell "Multi-factor sell signal"
}

// ─── Summary Output ──────────────────────────────────────────
print "=== Alert Status ==="
print "Price:", round(price, 2)
print "RSI:", round(rsi_now, 1)
print "EMA20:", round(ema20_now, 2)
print "EMA50:", round(ema50_now, 2)
print "Trend:", ema20_now > ema50_now ? "UP" : "DOWN"
print "Strong Buy?", strong_buy
print "Strong Sell?", strong_sell

// Visualization
plot_candlestick AAPL, "AAPL Alerts"
plot_line ema_20, "EMA 20", "cyan"
plot_line ema_50, "EMA 50", "orange"
plot rsi_val, "RSI"
`,
  },
  {
    id: 'ex_bollinger_squeeze',
    name: '09_bollinger_squeeze.fincept',
    category: 'Strategies',
    description: 'Bollinger Band squeeze breakout strategy',
    content: `// ═══════════════════════════════════════════════════════════════
// Bollinger Band Squeeze Strategy
// Detects volatility contractions for breakout trades
// ═══════════════════════════════════════════════════════════════

// ─── Parameters ──────────────────────────────────────────────
bb_period = 20
bb_std = 2.0
atr_period = 14
squeeze_threshold = 0.03  // 3% bandwidth = squeeze

// ─── Calculate Indicators ────────────────────────────────────
bb_up = bollinger(TSLA, bb_period, bb_std)
bb_mid = bollinger_middle(TSLA, bb_period)
bb_low = bollinger_lower(TSLA, bb_period, bb_std)
atr_val = atr(TSLA, atr_period)
close_prices = close(TSLA)

// ─── Bandwidth Calculation ───────────────────────────────────
// Bandwidth = (Upper - Lower) / Middle
bb_up_last = last(bb_up)
bb_low_last = last(bb_low)
bb_mid_last = last(bb_mid)
price = last(close_prices)
atr_now = last(atr_val)

bandwidth = (bb_up_last - bb_low_last) / bb_mid_last
print "Bollinger Bandwidth:", round(bandwidth, 4)
print "ATR:", round(atr_now, 2)

// ─── Squeeze Detection ───────────────────────────────────────
is_squeeze = bandwidth < squeeze_threshold
print "In Squeeze?", is_squeeze

// ─── Breakout Direction ──────────────────────────────────────
if is_squeeze {
    print "*** VOLATILITY SQUEEZE DETECTED ***"
    alert("TSLA: Bollinger Squeeze - Breakout Imminent!")

    // Determine likely breakout direction using momentum
    rsi_now = last(rsi(TSLA, 14))
    mom = last(momentum(TSLA, 10))

    if rsi_now > 50 and mom > 0 {
        buy "Squeeze breakout - Bullish bias"
        print "Bias: BULLISH (RSI:", round(rsi_now, 1), "Mom:", round(mom, 2), ")"
    } else if rsi_now < 50 and mom < 0 {
        sell "Squeeze breakout - Bearish bias"
        print "Bias: BEARISH (RSI:", round(rsi_now, 1), "Mom:", round(mom, 2), ")"
    } else {
        print "Bias: NEUTRAL - wait for confirmation"
    }
} else {
    // Not in squeeze - check for band touches
    if price >= bb_up_last {
        print "Price at upper band - Possible overbought"
    } else if price <= bb_low_last {
        print "Price at lower band - Possible oversold"
    }
}

// ─── Visualization ───────────────────────────────────────────
plot_candlestick TSLA, "TSLA - Bollinger Squeeze"
plot_line bb_up, "BB Upper", "gray"
plot_line bb_mid, "BB Middle", "blue"
plot_line bb_low, "BB Lower", "gray"
plot_line atr_val, "ATR(14)", "orange"
`,
  },
  {
    id: 'ex_libraries',
    name: '10_libraries_modules.fincept',
    category: 'Advanced',
    description: 'Import/export, modules, reusable code',
    content: `// ═══════════════════════════════════════════════════════════════
// Libraries & Modules
// Organize reusable code with import/export
// ═══════════════════════════════════════════════════════════════

// ─── Import Syntax ───────────────────────────────────────────
// Import a library module (symbolic in current version)
import indicators.custom as custom
import strategies.momentum as mom_lib
import utils.math as math_utils

print "Modules imported successfully"

// ─── Export Functions ────────────────────────────────────────
// Mark functions for export to be used by other scripts
fn calculate_sharpe(returns, risk_free_rate) {
    avg_return = avg(returns)
    std_return = 0.15  // simplified
    return (avg_return - risk_free_rate) / std_return
}

fn calculate_max_drawdown(equity_curve) {
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
export calculate_max_drawdown

// ─── Using Exported Functions ────────────────────────────────
test_returns = [0.02, -0.01, 0.03, 0.01, -0.02, 0.04, 0.01]
sharpe = calculate_sharpe(test_returns, 0.005)
print "\\nSharpe Ratio:", round(sharpe, 3)

equity = [100000, 102000, 101500, 104000, 103000, 106000, 105500, 108000]
max_dd = calculate_max_drawdown(equity)
print "Max Drawdown:", round(max_dd * 100, 2), "%"

// ─── Modular Strategy Pattern ────────────────────────────────
// Reusable strategy components

fn compute_signals(symbol) {
    fast = last(ema(symbol, 12))
    slow = last(ema(symbol, 26))
    rsi_v = last(rsi(symbol, 14))

    bullish = fast > slow and rsi_v < 70
    bearish = fast < slow and rsi_v > 30

    return bullish ? 1 : (bearish ? -1 : 0)
}

fn risk_check(signal, atr_value, max_risk) {
    // Only take signal if ATR is within acceptable range
    if atr_value > max_risk {
        return 0  // Skip - too volatile
    }
    return signal
}

// Run the modular strategy
print "\\n=== Modular Strategy ==="
raw_signal = compute_signals(AAPL)
atr_now = last(atr(AAPL, 14))
final_signal = risk_check(raw_signal, atr_now, 10.0)

print "Raw Signal:", raw_signal
print "ATR:", round(atr_now, 2)
print "Final Signal:", final_signal

if final_signal > 0 {
    buy "Modular Strategy: Long"
} else if final_signal < 0 {
    sell "Modular Strategy: Short"
} else {
    print "No trade - risk filter or neutral"
}

plot_candlestick AAPL, "Modular Strategy"
`,
  },
  {
    id: 'ex_math_series',
    name: '11_math_operations.fincept',
    category: 'Advanced',
    description: 'Math functions, series arithmetic, NA handling',
    content: `// ═══════════════════════════════════════════════════════════════
// Math Operations & Series Arithmetic
// Built-in math, NA handling, series element-wise ops
// ═══════════════════════════════════════════════════════════════

// ─── Math Functions ──────────────────────────────────────────
print "=== Basic Math ==="
print "abs(-5):", abs(-5)
print "sqrt(144):", sqrt(144)
print "pow(2, 10):", pow(2, 10)
print "log(100):", round(log(100), 4)
print "log10(1000):", log10(1000)
print "exp(1):", round(exp(1), 4)
print "round(3.14159, 2):", round(3.14159, 2)
print "floor(3.7):", floor(3.7)
print "ceil(3.2):", ceil(3.2)
print "sign(-42):", sign(-42)
print "max(10, 20):", max(10, 20)
print "min(10, 20):", min(10, 20)

// ─── Trigonometry ────────────────────────────────────────────
pi = 3.14159265
print "\\n=== Trigonometry ==="
print "sin(π/2):", round(sin(pi / 2), 4)
print "cos(0):", round(cos(0), 4)
print "tan(π/4):", round(tan(pi / 4), 4)

// ─── Series Arithmetic ──────────────────────────────────────
// Operations between Series are element-wise
close_aapl = close(AAPL)
open_aapl = open(AAPL)
high_aapl = high(AAPL)
low_aapl = low(AAPL)

// Calculate derived series
body = close_aapl - open_aapl        // Candle body size
range_series = high_aapl - low_aapl  // High-Low range
midpoint = (high_aapl + low_aapl) / 2  // HL2
typical = (high_aapl + low_aapl + close_aapl) / 3  // HLC3

print "\\n=== Series Arithmetic ==="
print "Last body:", round(last(body), 2)
print "Last range:", round(last(range_series), 2)
print "Last midpoint:", round(last(midpoint), 2)

// Series + Number (broadcast)
normalized = close_aapl / last(close_aapl) * 100
print "Normalized last:", last(normalized)

// ─── NA Handling ─────────────────────────────────────────────
print "\\n=== NA (Missing Values) ==="
x = na        // na is FinScript's null/NaN equivalent
print "x is na?", na(x)
print "na is truthy?", x ? "yes" : "no"

// nz() - replace NA with a default
safe_val = nz(x, 0.0)
print "nz(na, 0):", safe_val

y = 42.0
print "nz(42, 0):", nz(y, 0)

// ─── Statistical Functions ───────────────────────────────────
print "\\n=== Statistics ==="
data = close(AAPL)
print "Data points:", len(data)
print "Sum:", round(sum(data), 2)
print "Average:", round(avg(data), 2)
print "Max:", round(max(data), 2)
print "Min:", round(min(data), 2)

// Standard deviation
sd = stdev(data, 20)
print "StdDev(20):", round(last(sd), 4)

// Percent rank
pr = percentrank(data, 20)
print "PercentRank(20):", round(last(pr), 2)

// ─── Pivot Points ────────────────────────────────────────────
ph = pivothigh(high(AAPL), 5, 5)
pl = pivotlow(low(AAPL), 5, 5)
print "\\nPivot High:", last(ph)
print "Pivot Low:", last(pl)

plot_candlestick AAPL, "Math & Series Demo"
plot_line typical, "Typical Price", "purple"
`,
  },
  {
    id: 'ex_complete_system',
    name: '12_complete_system.fincept',
    category: 'Advanced',
    description: 'Full trading system with all features combined',
    content: `// ═══════════════════════════════════════════════════════════════
// Complete Trading System
// Combines all FinScript features into one system
// ═══════════════════════════════════════════════════════════════

// ─── Configuration ───────────────────────────────────────────
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

print "=== Trading System Config ==="
print "Fast EMA:", config.fast_period
print "Slow EMA:", config.slow_period
print "RSI Period:", config.rsi_period

// ─── Data Collection ─────────────────────────────────────────
ema_fast = ema(AAPL, config.fast_period)
ema_slow = ema(AAPL, config.slow_period)
rsi_val = rsi(AAPL, config.rsi_period)
atr_val = atr(AAPL, 14)
macd_h = macd_hist(AAPL, config.fast_period, config.slow_period, 9)
bb_up = bollinger(AAPL, 20, 2)
bb_low = bollinger_lower(AAPL, 20, 2)

// ─── State Variables ─────────────────────────────────────────
price = last(close(AAPL))
rsi_now = last(rsi_val)
atr_now = last(atr_val)
ema_f = last(ema_fast)
ema_s = last(ema_slow)
macd_now = last(macd_h)

// ─── Score-Based Signal ──────────────────────────────────────
fn calculate_score() {
    score = 0

    // Trend component (+/- 2)
    if ema_f > ema_s { score += 2 }
    else { score -= 2 }

    // Momentum component (+/- 1)
    if rsi_now > 50 and rsi_now < 70 { score += 1 }
    else if rsi_now < 50 and rsi_now > 30 { score -= 1 }

    // MACD component (+/- 1)
    if macd_now > 0 { score += 1 }
    else { score -= 1 }

    // Volatility filter
    if atr_now > price * 0.05 {
        score = 0  // Too volatile, flatten
    }

    return score
}

signal_score = calculate_score()

// ─── Position Sizing ─────────────────────────────────────────
capital = 100000
risk_per_trade = 0.02  // 2% risk
stop_distance = atr_now * config.atr_mult
position_size = (capital * risk_per_trade) / stop_distance
position_size = round(position_size, 0)

// ─── Trade Execution ─────────────────────────────────────────
print "\\n=== Signal Analysis ==="
print "Score:", signal_score, "(range: -4 to +4)"
print "Price:", round(price, 2)
print "ATR:", round(atr_now, 2)
print "Stop Distance:", round(stop_distance, 2)
print "Position Size:", position_size, "shares"

if signal_score >= 3 {
    strategy.entry("Strong Buy", "long", position_size)
    alert("AAPL Strong Buy Signal! Score: " + str(signal_score))
    print "ACTION: STRONG BUY"
} else if signal_score <= -3 {
    strategy.entry("Strong Sell", "short", position_size)
    alert("AAPL Strong Sell Signal! Score: " + str(signal_score))
    print "ACTION: STRONG SELL"
} else if signal_score >= 1 {
    print "ACTION: Mild bullish bias - consider scaling in"
} else if signal_score <= -1 {
    print "ACTION: Mild bearish bias - consider scaling out"
} else {
    print "ACTION: NEUTRAL - no trade"
}

// ─── Risk Management ─────────────────────────────────────────
stop_loss = price - stop_distance
take_profit = price + (stop_distance * 2)  // 2:1 R:R

print "\\n=== Risk Management ==="
print "Stop Loss:", round(stop_loss, 2)
print "Take Profit:", round(take_profit, 2)
print "Risk:Reward = 1:2"

// ─── Performance Tracking ────────────────────────────────────
print "\\n=== Portfolio Status ==="
print "Equity:", strategy_equity()
print "Position:", strategy_position_size()

// ─── Summary Table ───────────────────────────────────────────
dash = table_new("top_right", 5, 2)
table_cell(dash, 0, 0, "METRIC", "#333", "#fff")
table_cell(dash, 0, 1, "VALUE", "#333", "#fff")
table_cell(dash, 1, 0, "Score", "#111", "#0af")
table_cell(dash, 1, 1, str(signal_score), "#111", "#fff")
table_cell(dash, 2, 0, "RSI", "#111", "#0af")
table_cell(dash, 2, 1, str(round(rsi_now, 1)), "#111", "#fff")
table_cell(dash, 3, 0, "ATR", "#111", "#0af")
table_cell(dash, 3, 1, str(round(atr_now, 2)), "#111", "#fff")
table_cell(dash, 4, 0, "Size", "#111", "#0af")
table_cell(dash, 4, 1, str(position_size), "#111", "#fff")

// ─── Full Visualization ──────────────────────────────────────
plot_candlestick AAPL, "Complete Trading System"
plot_line ema_fast, "EMA Fast", "cyan"
plot_line ema_slow, "EMA Slow", "orange"
plot_line bb_up, "BB Upper", "gray"
plot_line bb_low, "BB Lower", "gray"
plot_histogram macd_h, "MACD Hist"
plot rsi_val, "RSI"
hline 70, "OB", "red"
hline 30, "OS", "green"

// Draw stop/take-profit levels
line_new(200, stop_loss, 250, stop_loss, "red", "dashed", 1)
line_new(200, take_profit, 250, take_profit, "green", "dashed", 1)
label_new(250, stop_loss, "SL", "red")
label_new(250, take_profit, "TP", "green")
`,
  },
];

// ─── Component ──────────────────────────────────────────────────────────────
export default function CodeEditorTab() {
  // ─── State ──────────────────────────────────────────────────────────────
  const [files, setFiles] = useState<EditorFile[]>([
    {
      id: '1',
      name: 'strategy.fincept',
      content: DEFAULT_FINSCRIPT,
      language: 'finscript',
      unsaved: false,
      cursorLine: 1,
      cursorCol: 1,
    }
  ]);
  const [activeFileId, setActiveFileId] = useState('1');
  const [isRunning, setIsRunning] = useState(false);
  const [finscriptResult, setFinscriptResult] = useState<FinScriptExecutionResult | null>(null);
  const [showExplorer, setShowExplorer] = useState(true);
  const [outputHeight, setOutputHeight] = useState(280);
  const [isOutputMaximized, setIsOutputMaximized] = useState(false);
  const [showOutput, setShowOutput] = useState(true);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeFileId, showExplorer, showOutput,
  }), [activeFileId, showExplorer, showOutput]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeFileId === 'string') setActiveFileId(state.activeFileId);
    if (typeof state.showExplorer === 'boolean') setShowExplorer(state.showExplorer);
    if (typeof state.showOutput === 'boolean') setShowOutput(state.showOutput);
  }, []);

  useWorkspaceTabState('code-editor', getWorkspaceState, setWorkspaceState);
  const [search, setSearch] = useState<SearchState>({
    isOpen: false, query: '', replaceQuery: '', showReplace: false, matchCount: 0, currentMatch: 0
  });
  const [executionHistory, setExecutionHistory] = useState<Array<{ time: string; status: 'success' | 'error'; ms: number }>>([]);
  const [examplesExpanded, setExamplesExpanded] = useState(true);

  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const editorContainerRef = useRef<HTMLDivElement>(null);
  const resizingRef = useRef(false);

  const activeFile = files.find(f => f.id === activeFileId) || files[0];
  const lines = activeFile.content.split('\n');

  // ─── Keyboard Shortcuts ─────────────────────────────────────────────────
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey) {
        switch (e.key) {
          case 's':
            e.preventDefault();
            saveFile();
            break;
          case 'Enter':
            e.preventDefault();
            runCode();
            break;
          case 'f':
            e.preventDefault();
            setSearch(s => ({ ...s, isOpen: !s.isOpen }));
            break;
          case 'n':
            e.preventDefault();
            createNewFile();
            break;
          case 'b':
            e.preventDefault();
            setShowExplorer(v => !v);
            break;
          case 'j':
            e.preventDefault();
            setShowOutput(v => !v);
            break;
        }
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [activeFileId, files, isRunning]);

  // ─── Cursor Tracking ────────────────────────────────────────────────────
  const updateCursorPosition = useCallback(() => {
    if (!textareaRef.current) return;
    const pos = textareaRef.current.selectionStart;
    const text = activeFile.content.substring(0, pos);
    const line = text.split('\n').length;
    const col = pos - text.lastIndexOf('\n');
    setFiles(prev => prev.map(f =>
      f.id === activeFileId ? { ...f, cursorLine: line, cursorCol: col } : f
    ));
  }, [activeFileId, activeFile.content]);

  // ─── Output Resize ─────────────────────────────────────────────────────
  const handleResizeStart = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    resizingRef.current = true;
    const startY = e.clientY;
    const startHeight = outputHeight;

    const handleMove = (me: MouseEvent) => {
      if (!resizingRef.current) return;
      const delta = startY - me.clientY;
      setOutputHeight(Math.max(100, Math.min(600, startHeight + delta)));
    };

    const handleUp = () => {
      resizingRef.current = false;
      window.removeEventListener('mousemove', handleMove);
      window.removeEventListener('mouseup', handleUp);
    };

    window.addEventListener('mousemove', handleMove);
    window.addEventListener('mouseup', handleUp);
  }, [outputHeight]);

  // ─── File Operations ────────────────────────────────────────────────────
  const updateFileContent = (content: string) => {
    setFiles(files.map(f =>
      f.id === activeFileId ? { ...f, content, unsaved: true } : f
    ));
  };

  const saveFile = async () => {
    setFiles(files.map(f =>
      f.id === activeFileId ? { ...f, unsaved: false } : f
    ));
  };

  const createNewFile = () => {
    const newId = Date.now().toString();
    const newFile: EditorFile = {
      id: newId,
      name: `untitled-${files.length + 1}.fincept`,
      content: '// New FinScript file\n\n',
      language: 'finscript',
      unsaved: false,
      cursorLine: 1,
      cursorCol: 1,
    };
    setFiles([...files, newFile]);
    setActiveFileId(newId);
  };

  const closeFile = async (id: string) => {
    if (files.length === 1) return;
    const fileToClose = files.find(f => f.id === id);
    if (fileToClose?.unsaved) {
      const confirmed = await showConfirm(
        'You have unsaved changes.',
        { title: 'Close file without saving?', type: 'warning' }
      );
      if (!confirmed) return;
    }
    const newFiles = files.filter(f => f.id !== id);
    setFiles(newFiles);
    if (activeFileId === id) {
      setActiveFileId(newFiles[newFiles.length - 1].id);
    }
  };

  const loadExample = (example: ExampleScript) => {
    // Check if this example is already open
    const existing = files.find(f => f.name === example.name);
    if (existing) {
      setActiveFileId(existing.id);
      return;
    }
    const newId = `ex_${Date.now()}`;
    const newFile: EditorFile = {
      id: newId,
      name: example.name,
      content: example.content,
      language: 'finscript',
      unsaved: false,
      cursorLine: 1,
      cursorCol: 1,
    };
    setFiles(prev => [...prev, newFile]);
    setActiveFileId(newId);
  };

  const openFileFromDisk = async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [
          { name: 'FinScript', extensions: ['fincept', 'fin'] },
          { name: 'Python', extensions: ['py'] },
          { name: 'All Files', extensions: ['*'] },
        ]
      });
      if (!selected) return;
      const path = selected as string;
      const content = await invoke<string>('read_file_content', { filePath: path });
      const name = path.split(/[/\\]/).pop() || 'unknown';
      const ext = name.split('.').pop()?.toLowerCase();
      let language: EditorFile['language'] = 'finscript';
      if (ext === 'py') language = 'python';
      else if (ext === 'js') language = 'javascript';
      else if (ext === 'json') language = 'json';

      const newId = Date.now().toString();
      setFiles([...files, { id: newId, name, content, language, unsaved: false, cursorLine: 1, cursorCol: 1 }]);
      setActiveFileId(newId);
    } catch (error) {
      console.error('Failed to open file:', error);
    }
  };

  const saveFileToDisk = async () => {
    try {
      const selected = await save({
        filters: [
          { name: 'FinScript', extensions: ['fincept'] },
          { name: 'Python', extensions: ['py'] },
          { name: 'All Files', extensions: ['*'] },
        ]
      });
      if (!selected) return;
      await invoke('write_file_content', { filePath: selected, content: activeFile.content });
      setFiles(files.map(f => f.id === activeFileId ? { ...f, unsaved: false } : f));
    } catch (error) {
      console.error('Failed to save file:', error);
    }
  };

  // ─── Execution ──────────────────────────────────────────────────────────
  const executeFinScript = async () => {
    setIsRunning(true);
    setFinscriptResult(null);
    setShowOutput(true);

    try {
      const result = await invoke<FinScriptExecutionResult>('execute_finscript', {
        code: activeFile.content
      });
      setFinscriptResult(result);
      setExecutionHistory(prev => [...prev.slice(-9), {
        time: new Date().toLocaleTimeString(),
        status: result.success ? 'success' : 'error',
        ms: result.execution_time_ms
      }]);
    } catch (error) {
      const errorResult: FinScriptExecutionResult = {
        success: false,
        output: '',
        signals: [],
        plots: [],
        errors: [String(error)],
        execution_time_ms: 0,
      };
      setFinscriptResult(errorResult);
      setExecutionHistory(prev => [...prev.slice(-9), {
        time: new Date().toLocaleTimeString(),
        status: 'error',
        ms: 0
      }]);
    } finally {
      setIsRunning(false);
    }
  };

  const runCode = () => {
    if (activeFile.language === 'finscript') {
      executeFinScript();
    }
  };

  // ─── Editor Tab Indent ──────────────────────────────────────────────────
  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Tab') {
      e.preventDefault();
      const textarea = textareaRef.current;
      if (!textarea) return;
      const start = textarea.selectionStart;
      const end = textarea.selectionEnd;
      const content = activeFile.content;
      const newContent = content.substring(0, start) + '    ' + content.substring(end);
      updateFileContent(newContent);
      setTimeout(() => {
        textarea.selectionStart = textarea.selectionEnd = start + 4;
      }, 0);
    }
  };

  // ─── Search ─────────────────────────────────────────────────────────────
  const getMatchCount = useCallback(() => {
    if (!search.query) return 0;
    try {
      const regex = new RegExp(search.query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');
      return (activeFile.content.match(regex) || []).length;
    } catch { return 0; }
  }, [search.query, activeFile.content]);

  // ─── Render ─────────────────────────────────────────────────────────────
  const effectiveOutputHeight = isOutputMaximized ? 500 : (showOutput ? outputHeight : 0);

  return (
    <div style={{
      width: '100%', height: '100%',
      backgroundColor: F.DARK_BG,
      color: F.WHITE,
      fontFamily: FONT,
      display: 'flex', flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* ─── CSS ──────────────────────────────────────────────────────── */}
      <style>{`
        .ce-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
        .ce-scroll::-webkit-scrollbar-track { background: ${F.DARK_BG}; }
        .ce-scroll::-webkit-scrollbar-thumb { background: ${F.BORDER}; border-radius: 3px; }
        .ce-scroll::-webkit-scrollbar-thumb:hover { background: ${F.MUTED}; }
        .ce-tab:hover { background-color: ${F.HOVER} !important; }
        .ce-explorer-item:hover { background-color: ${F.HOVER}; }
        .ce-resize-handle:hover { background-color: ${F.ORANGE}; }
      `}</style>

      {/* ─── Top Navigation Bar ───────────────────────────────────────── */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        padding: '6px 16px',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`,
        minHeight: '38px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Zap size={14} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>FINSCRIPT IDE</span>
          <span style={{ color: F.MUTED }}>|</span>
          <span style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px' }}>v1.0</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {/* Toggle Explorer */}
          <button
            onClick={() => setShowExplorer(v => !v)}
            title="Toggle Explorer (Ctrl+B)"
            style={{
              padding: '4px 8px', backgroundColor: showExplorer ? `${F.ORANGE}20` : 'transparent',
              border: `1px solid ${showExplorer ? F.ORANGE : F.BORDER}`, borderRadius: '2px',
              color: showExplorer ? F.ORANGE : F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', fontFamily: FONT, transition: 'all 0.2s',
            }}
          >
            <FolderOpen size={11} />
          </button>

          {/* Open File */}
          <button
            onClick={openFileFromDisk}
            title="Open File"
            style={{
              padding: '4px 8px', backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              color: F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', fontFamily: FONT, transition: 'all 0.2s',
            }}
          >
            <Upload size={11} />
          </button>

          {/* Save */}
          <button
            onClick={saveFile}
            disabled={!activeFile.unsaved}
            title="Save (Ctrl+S)"
            style={{
              padding: '4px 8px', backgroundColor: 'transparent',
              border: `1px solid ${activeFile.unsaved ? F.YELLOW : F.BORDER}`, borderRadius: '2px',
              color: activeFile.unsaved ? F.YELLOW : F.MUTED, fontSize: '9px', fontWeight: 700,
              cursor: activeFile.unsaved ? 'pointer' : 'default', fontFamily: FONT,
              opacity: activeFile.unsaved ? 1 : 0.5, transition: 'all 0.2s',
            }}
          >
            <Save size={11} />
          </button>

          {/* Search */}
          <button
            onClick={() => setSearch(s => ({ ...s, isOpen: !s.isOpen }))}
            title="Find (Ctrl+F)"
            style={{
              padding: '4px 8px', backgroundColor: search.isOpen ? `${F.CYAN}20` : 'transparent',
              border: `1px solid ${search.isOpen ? F.CYAN : F.BORDER}`, borderRadius: '2px',
              color: search.isOpen ? F.CYAN : F.GRAY, fontSize: '9px', fontWeight: 700,
              cursor: 'pointer', fontFamily: FONT, transition: 'all 0.2s',
            }}
          >
            <Search size={11} />
          </button>

          <div style={{ width: '1px', height: '18px', backgroundColor: F.BORDER, margin: '0 4px' }} />

          {/* Run Button */}
          <button
            onClick={runCode}
            disabled={isRunning || activeFile.language !== 'finscript'}
            title="Run (Ctrl+Enter)"
            style={{
              padding: '5px 14px', display: 'flex', alignItems: 'center', gap: '6px',
              backgroundColor: isRunning ? F.MUTED : F.ORANGE,
              color: F.DARK_BG, border: 'none', borderRadius: '2px',
              fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
              cursor: isRunning ? 'not-allowed' : 'pointer', fontFamily: FONT,
              transition: 'all 0.2s',
            }}
          >
            <Play size={11} fill={F.DARK_BG} />
            {isRunning ? 'RUNNING...' : 'RUN'}
          </button>
        </div>
      </div>

      {/* ─── Main Area ────────────────────────────────────────────────── */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>

        {/* ─── Explorer Panel ─────────────────────────────────────────── */}
        {showExplorer && (
          <div style={{
            width: '220px', backgroundColor: F.PANEL_BG,
            borderRight: `1px solid ${F.BORDER}`,
            display: 'flex', flexDirection: 'column',
          }}>
            {/* Explorer Header */}
            <div style={{
              padding: '10px 12px',
              backgroundColor: F.HEADER_BG,
              borderBottom: `1px solid ${F.BORDER}`,
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                EXPLORER
              </span>
              <button
                onClick={createNewFile}
                title="New File (Ctrl+N)"
                style={{
                  padding: '2px 4px', backgroundColor: 'transparent',
                  border: 'none', color: F.GRAY, cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
              >
                <Plus size={12} />
              </button>
            </div>

            {/* File List */}
            <div className="ce-scroll" style={{ flex: 1, overflowY: 'auto', padding: '4px 0' }}>
              {/* Section Label */}
              <div style={{
                padding: '6px 12px', display: 'flex', alignItems: 'center', gap: '4px',
                fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
              }}>
                <ChevronDown size={10} />
                OPEN FILES ({files.length})
              </div>

              {files.map(file => {
                const lang = LANG_CONFIG[file.language];
                const isActive = file.id === activeFileId;
                return (
                  <div
                    key={file.id}
                    className="ce-explorer-item"
                    onClick={() => setActiveFileId(file.id)}
                    style={{
                      padding: '6px 12px 6px 24px',
                      display: 'flex', alignItems: 'center', gap: '8px',
                      backgroundColor: isActive ? `${F.ORANGE}15` : 'transparent',
                      borderLeft: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer', transition: 'all 0.2s',
                    }}
                  >
                    <span style={{ color: lang.color }}>{lang.icon}</span>
                    <span style={{
                      fontSize: '10px', color: isActive ? F.WHITE : F.GRAY,
                      flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                    }}>
                      {file.name}
                    </span>
                    {file.unsaved && (
                      <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: F.YELLOW }} />
                    )}
                    {files.length > 1 && (
                      <X size={10}
                        onClick={(e) => { e.stopPropagation(); closeFile(file.id); }}
                        style={{ color: F.MUTED, cursor: 'pointer', opacity: 0.6 }}
                      />
                    )}
                  </div>
                );
              })}

              {/* Examples Section */}
              <div style={{
                borderTop: `1px solid ${F.BORDER}`, marginTop: '8px',
              }}>
                <div
                  onClick={() => setExamplesExpanded(!examplesExpanded)}
                  style={{
                    padding: '10px 12px 6px', display: 'flex', alignItems: 'center', gap: '4px',
                    fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
                    cursor: 'pointer',
                  }}
                >
                  <ChevronDown size={10} style={{
                    transform: examplesExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
                    transition: 'transform 0.2s',
                  }} />
                  <BookOpen size={10} style={{ color: F.ORANGE }} />
                  EXAMPLES
                </div>
                {examplesExpanded && (
                  <div>
                    {Array.from(new Set(EXAMPLE_SCRIPTS.map(e => e.category))).map(category => (
                      <div key={category}>
                        <div style={{
                          padding: '4px 12px 2px 20px',
                          fontSize: '9px', color: F.MUTED, fontWeight: 600,
                          letterSpacing: '0.3px',
                        }}>
                          {category}
                        </div>
                        {EXAMPLE_SCRIPTS.filter(e => e.category === category).map(example => (
                          <div
                            key={example.id}
                            className="ce-explorer-item"
                            onClick={() => loadExample(example)}
                            title={example.description}
                            style={{
                              padding: '4px 12px 4px 28px',
                              display: 'flex', alignItems: 'center', gap: '6px',
                              cursor: 'pointer', transition: 'all 0.2s',
                            }}
                          >
                            <Zap size={9} style={{ color: F.ORANGE, flexShrink: 0 }} />
                            <span style={{
                              fontSize: '10px', color: F.GRAY,
                              overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                            }}>
                              {example.name.replace('.fincept', '')}
                            </span>
                          </div>
                        ))}
                      </div>
                    ))}
                  </div>
                )}
              </div>

              {/* Execution History */}
              {executionHistory.length > 0 && (
                <>
                  <div style={{
                    padding: '10px 12px 6px', display: 'flex', alignItems: 'center', gap: '4px',
                    fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
                    borderTop: `1px solid ${F.BORDER}`, marginTop: '8px',
                  }}>
                    <ChevronDown size={10} />
                    RUN HISTORY
                  </div>
                  {executionHistory.slice().reverse().map((h, idx) => (
                    <div key={idx} style={{
                      padding: '4px 12px 4px 24px',
                      display: 'flex', alignItems: 'center', gap: '6px',
                      fontSize: '9px',
                    }}>
                      {h.status === 'success'
                        ? <CheckCircle size={9} style={{ color: F.GREEN }} />
                        : <AlertTriangle size={9} style={{ color: F.RED }} />
                      }
                      <span style={{ color: F.MUTED }}>{h.time}</span>
                      <span style={{ color: F.GRAY }}>{h.ms}ms</span>
                    </div>
                  ))}
                </>
              )}
            </div>

            {/* Explorer Footer */}
            <div style={{
              padding: '8px 12px',
              borderTop: `1px solid ${F.BORDER}`,
              fontSize: '9px', color: F.MUTED,
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <Zap size={9} style={{ color: F.ORANGE }} />
                <span>FinScript Engine</span>
              </div>
            </div>
          </div>
        )}

        {/* ─── Editor + Output ────────────────────────────────────────── */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>

          {/* ─── Tab Bar ──────────────────────────────────────────────── */}
          <div style={{
            backgroundColor: F.HEADER_BG,
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', alignItems: 'center',
            overflow: 'hidden',
          }}>
            <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
              {files.map(file => {
                const lang = LANG_CONFIG[file.language];
                const isActive = file.id === activeFileId;
                return (
                  <div
                    key={file.id}
                    className={isActive ? '' : 'ce-tab'}
                    onClick={() => setActiveFileId(file.id)}
                    style={{
                      padding: '7px 14px',
                      display: 'flex', alignItems: 'center', gap: '6px',
                      backgroundColor: isActive ? F.PANEL_BG : 'transparent',
                      borderRight: `1px solid ${F.BORDER}`,
                      borderBottom: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer', transition: 'all 0.2s',
                      maxWidth: '180px',
                    }}
                  >
                    <span style={{ color: lang.color }}>{lang.icon}</span>
                    <span style={{
                      fontSize: '10px',
                      color: isActive ? F.WHITE : F.GRAY,
                      overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                    }}>
                      {file.name}{file.unsaved ? ' *' : ''}
                    </span>
                    {files.length > 1 && (
                      <X size={10}
                        onClick={(e) => { e.stopPropagation(); closeFile(file.id); }}
                        style={{ color: F.MUTED, cursor: 'pointer' }}
                      />
                    )}
                  </div>
                );
              })}
            </div>
          </div>

          {/* ─── Search Bar ───────────────────────────────────────────── */}
          {search.isOpen && (
            <div style={{
              backgroundColor: F.HEADER_BG,
              borderBottom: `1px solid ${F.BORDER}`,
              padding: '6px 12px',
              display: 'flex', alignItems: 'center', gap: '8px',
            }}>
              <Search size={12} style={{ color: F.GRAY }} />
              <input
                autoFocus
                type="text"
                value={search.query}
                onChange={(e) => setSearch(s => ({ ...s, query: e.target.value }))}
                placeholder="Find..."
                style={{
                  width: '200px', padding: '4px 8px',
                  backgroundColor: F.DARK_BG, color: F.WHITE,
                  border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                  fontSize: '10px', fontFamily: FONT,
                  outline: 'none',
                }}
              />
              {search.query && (
                <span style={{ fontSize: '9px', color: F.GRAY }}>
                  {getMatchCount()} matches
                </span>
              )}
              <button
                onClick={() => setSearch(s => ({ ...s, showReplace: !s.showReplace }))}
                style={{
                  padding: '2px 6px', backgroundColor: 'transparent',
                  border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                  color: F.GRAY, fontSize: '9px', cursor: 'pointer', fontFamily: FONT,
                }}
              >
                <Replace size={10} />
              </button>
              <X size={12}
                style={{ color: F.GRAY, cursor: 'pointer' }}
                onClick={() => setSearch(s => ({ ...s, isOpen: false }))}
              />
            </div>
          )}

          {/* ─── Editor Area ──────────────────────────────────────────── */}
          <div ref={editorContainerRef} style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
            {/* Line Numbers Gutter */}
            <div className="ce-scroll" style={{
              width: '48px', backgroundColor: F.DARK_BG,
              borderRight: `1px solid ${F.BORDER}`,
              overflowY: 'hidden', padding: '12px 0',
              userSelect: 'none',
            }}>
              {lines.map((_, i) => (
                <div key={i} style={{
                  padding: '0 8px', textAlign: 'right',
                  fontSize: '11px', lineHeight: '20px',
                  color: (i + 1) === activeFile.cursorLine ? F.WHITE : F.MUTED,
                  fontFamily: FONT,
                }}>
                  {i + 1}
                </div>
              ))}
            </div>

            {/* Text Editor */}
            <textarea
              ref={textareaRef}
              className="ce-scroll"
              value={activeFile.content}
              onChange={(e) => updateFileContent(e.target.value)}
              onKeyUp={updateCursorPosition}
              onClick={updateCursorPosition}
              onKeyDown={handleKeyDown}
              spellCheck={false}
              style={{
                flex: 1,
                backgroundColor: F.PANEL_BG,
                color: F.WHITE,
                border: 'none', outline: 'none', resize: 'none',
                padding: '12px 16px',
                fontSize: '12px', lineHeight: '20px',
                fontFamily: FONT,
                whiteSpace: 'pre', overflowWrap: 'normal',
                overflowX: 'auto',
                tabSize: 4,
              }}
            />

            {/* Minimap Gutter */}
            <div style={{
              width: '4px', backgroundColor: F.DARK_BG,
              borderLeft: `1px solid ${F.BORDER}`,
              position: 'relative',
            }}>
              {/* Simple scrollbar indicator */}
              <div style={{
                position: 'absolute', top: '0',
                width: '100%', height: `${Math.min(100, (20 / lines.length) * 100)}%`,
                backgroundColor: `${F.ORANGE}40`, borderRadius: '2px',
              }} />
            </div>
          </div>

          {/* ─── Resize Handle ────────────────────────────────────────── */}
          {showOutput && (
            <div
              className="ce-resize-handle"
              onMouseDown={handleResizeStart}
              style={{
                height: '3px', backgroundColor: F.BORDER,
                cursor: 'ns-resize', transition: 'background-color 0.2s',
              }}
            />
          )}

          {/* ─── Output Panel ─────────────────────────────────────────── */}
          {showOutput && (
            <div style={{
              height: `${effectiveOutputHeight}px`,
              display: 'flex', flexDirection: 'column',
              borderTop: `1px solid ${F.BORDER}`,
            }}>
              {/* Output Header */}
              <div style={{
                backgroundColor: F.HEADER_BG,
                padding: '6px 12px',
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                borderBottom: `1px solid ${F.BORDER}`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <TerminalIcon size={11} style={{ color: F.GREEN }} />
                  <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                    OUTPUT
                  </span>
                  {finscriptResult && (
                    <span style={{
                      padding: '1px 5px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                      backgroundColor: finscriptResult.success ? `${F.GREEN}20` : `${F.RED}20`,
                      color: finscriptResult.success ? F.GREEN : F.RED,
                    }}>
                      {finscriptResult.success ? 'SUCCESS' : 'ERROR'}
                    </span>
                  )}
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <button
                    onClick={() => setIsOutputMaximized(v => !v)}
                    style={{
                      padding: '2px 4px', backgroundColor: 'transparent',
                      border: 'none', color: F.GRAY, cursor: 'pointer',
                    }}
                  >
                    {isOutputMaximized ? <Minimize2 size={10} /> : <Maximize2 size={10} />}
                  </button>
                  <button
                    onClick={() => setShowOutput(false)}
                    style={{
                      padding: '2px 4px', backgroundColor: 'transparent',
                      border: 'none', color: F.GRAY, cursor: 'pointer',
                    }}
                  >
                    <X size={10} />
                  </button>
                </div>
              </div>

              {/* Output Content */}
              <div style={{ flex: 1, overflow: 'hidden' }}>
                <FinScriptOutputPanel
                  result={finscriptResult}
                  isRunning={isRunning}
                  onClear={() => setFinscriptResult(null)}
                  colors={{
                    primary: F.ORANGE,
                    text: F.WHITE,
                    secondary: F.GREEN,
                    alert: F.RED,
                    warning: F.YELLOW,
                    info: F.BLUE,
                    accent: F.CYAN,
                    textMuted: F.GRAY,
                    background: F.DARK_BG,
                    panel: F.PANEL_BG,
                  }}
                />
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ─── Status Bar ───────────────────────────────────────────────── */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        padding: '3px 16px',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        fontSize: '9px', color: F.GRAY, fontFamily: FONT,
        minHeight: '22px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Language Indicator */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span style={{ color: LANG_CONFIG[activeFile.language].color }}>
              {LANG_CONFIG[activeFile.language].icon}
            </span>
            <span style={{ letterSpacing: '0.5px' }}>{activeFile.language.toUpperCase()}</span>
          </div>

          <span style={{ color: F.MUTED }}>|</span>

          {/* Cursor Position */}
          <span>Ln {activeFile.cursorLine}, Col {activeFile.cursorCol}</span>

          <span style={{ color: F.MUTED }}>|</span>

          {/* Line Count */}
          <span>{lines.length} lines</span>

          {activeFile.unsaved && (
            <>
              <span style={{ color: F.MUTED }}>|</span>
              <span style={{ color: F.YELLOW }}>MODIFIED</span>
            </>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Output Toggle */}
          {!showOutput && (
            <button
              onClick={() => setShowOutput(true)}
              style={{
                padding: '1px 6px', backgroundColor: 'transparent',
                border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                color: F.GRAY, fontSize: '8px', cursor: 'pointer', fontFamily: FONT,
              }}
            >
              SHOW OUTPUT
            </button>
          )}

          {/* Encoding */}
          <span>UTF-8</span>
          <span style={{ color: F.MUTED }}>|</span>
          <span>4 Spaces</span>
          <span style={{ color: F.MUTED }}>|</span>

          {/* Shortcuts */}
          <span style={{ color: F.MUTED }}>Ctrl+Enter: Run</span>
        </div>
      </div>
    </div>
  );
}
