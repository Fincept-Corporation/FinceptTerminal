import { DocSection } from '../types';
import { Code } from 'lucide-react';

export const finscriptLanguageDoc: DocSection = {
  id: 'finscript',
  title: 'FinScript Language',
  icon: Code,
  subsections: [
    {
      id: 'getting-started',
      title: 'Getting Started',
      content: `FinScript is a domain-specific language (DSL) designed for financial analysis and algorithmic trading. It provides a simple, intuitive syntax for defining trading strategies, calculating technical indicators, and generating trading signals.

**Key Features:**
• Simple syntax inspired by Pine Script
• Built-in technical indicators (EMA, SMA, WMA, RSI, MACD, etc.)
• Real-time data integration with Yahoo Finance
• Automatic chart generation
• Buy/Sell signal generation
• Backtesting capabilities
• Paper trading support
• Live execution integration

**Use Cases:**
• Developing trading strategies
• Technical analysis automation
• Signal generation
• Backtesting strategies
• Portfolio optimization`,
      codeExample: `// Your first FinScript program
if ema(AAPL, 21) > wma(AAPL, 50) {
    buy "Bullish crossover signal"
}

plot ema(AAPL, 21), "EMA 21"
plot wma(AAPL, 50), "WMA 50"`
    },
    {
      id: 'syntax',
      title: 'Basic Syntax',
      content: `FinScript uses a clean, declarative syntax. Here are the core language constructs:

**Comments:**
Use // for single-line comments

**Variables:**
Variables are automatically typed based on indicator functions

**Conditionals:**
Use if statements to define trading logic

**Functions:**
Built-in functions for technical analysis

**Operators:**
• Comparison: >, <, >=, <=, ==, !=
• Logical: and, or, not
• Arithmetic: +, -, *, /, %`,
      codeExample: `// Comments explain your strategy
// Variables are created by indicators
fast_ema = ema(TSLA, 12)
slow_ema = ema(TSLA, 26)

// Conditionals for signals
if fast_ema > slow_ema {
    buy "Fast EMA crossed above Slow EMA"
}

if fast_ema < slow_ema {
    sell "Fast EMA crossed below Slow EMA"
}`
    },
    {
      id: 'indicators',
      title: 'Technical Indicators',
      content: `FinScript provides a comprehensive suite of technical indicators for market analysis:

**Moving Averages:**
• ema(symbol, period) - Exponential Moving Average
• sma(symbol, period) - Simple Moving Average
• wma(symbol, period) - Weighted Moving Average

**Momentum Indicators:**
• rsi(symbol, period) - Relative Strength Index
• macd(symbol, fast, slow, signal) - MACD
• stochastic(symbol, period) - Stochastic Oscillator

**Volatility:**
• atr(symbol, period) - Average True Range
• bollinger(symbol, period, std) - Bollinger Bands

**Volume:**
• volume(symbol) - Trading Volume
• obv(symbol) - On-Balance Volume

**Price Functions:**
• close(symbol) - Closing price
• open(symbol) - Opening price
• high(symbol) - High price
• low(symbol) - Low price`,
      codeExample: `// Using multiple indicators
price = close(AAPL)
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_value = rsi(AAPL, 14)

// Combine indicators for strategy
if ema_fast > ema_slow and rsi_value < 30 {
    buy "Oversold with bullish trend"
}

plot ema_fast, "EMA 12"
plot ema_slow, "EMA 26"
plot rsi_value, "RSI 14"`
    },
    {
      id: 'plotting',
      title: 'Visualization & Plotting',
      content: `FinScript provides powerful charting capabilities to visualize your strategies:

**plot** - Plot indicator values
Syntax: plot indicator, "label"

**plot_candlestick** - Generate OHLC candlestick charts
Syntax: plot_candlestick symbol, "chart_title"

**plot_line** - Draw custom lines
Syntax: plot_line value, "label", color

**Chart Options:**
• Candlestick charts
• Line charts
• Bar charts
• Indicator overlays
• Support/resistance lines

Charts are automatically generated and can be viewed in the terminal's chart panel.`,
      codeExample: `// Visualize your strategy
symbol = MSFT

// Plot multiple indicators
plot ema(symbol, 7), "EMA 7"
plot ema(symbol, 21), "EMA 21"
plot sma(symbol, 50), "SMA 50"

// Generate candlestick chart
plot_candlestick symbol, "Microsoft Stock Chart"

// Add support/resistance lines
plot_line 300.0, "Resistance", "red"
plot_line 250.0, "Support", "green"`
    },
    {
      id: 'signals',
      title: 'Trading Signals',
      content: `Generate buy and sell signals based on your trading logic:

**buy** - Generate a buy signal
Syntax: buy "reason for buying"

**sell** - Generate a sell signal
Syntax: sell "reason for selling"

Signals are logged with timestamps and can trigger automated trading when integrated with brokers.

**Best Practices:**
• Always include descriptive messages
• Use multiple confirmation indicators
• Implement risk management
• Test thoroughly with historical data
• Use stop-loss and take-profit levels
• Monitor signal performance`,
      codeExample: `// Crossover strategy with signals
fast = ema(GOOGL, 12)
slow = ema(GOOGL, 26)
momentum = rsi(GOOGL, 14)

// Buy signal with multiple confirmations
if fast > slow and momentum > 50 {
    buy "EMA crossover with positive momentum"
}

// Sell signal with risk management
if fast < slow or momentum > 70 {
    sell "EMA crossover or overbought condition"
}

// Plot for visual confirmation
plot fast, "Fast EMA"
plot slow, "Slow EMA"`
    }
  ]
};
