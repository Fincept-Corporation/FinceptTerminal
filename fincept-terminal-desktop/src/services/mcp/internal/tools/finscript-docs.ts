import { InternalTool } from '../types';
import { finscriptLanguageDoc } from '@/components/tabs/docs/content/finscriptLanguage';

export const finscriptDocsTools: InternalTool[] = [
  {
    name: 'get_finscript_overview',
    description: 'Get an overview of FinScript language features, syntax, and capabilities',
    inputSchema: {
      type: 'object',
      properties: {},
      required: [],
    },
    handler: async () => {
      const overview = finscriptLanguageDoc.subsections.find(s => s.id === 'fs-overview');
      if (!overview) {
        return { success: false, error: 'Overview not found' };
      }
      return {
        success: true,
        data: {
          title: overview.title,
          content: overview.content,
          example: overview.codeExample,
        },
        message: 'FinScript overview retrieved successfully',
      };
    },
  },
  {
    name: 'list_finscript_topics',
    description: 'List all available FinScript documentation topics',
    inputSchema: {
      type: 'object',
      properties: {},
      required: [],
    },
    handler: async () => {
      const topics = finscriptLanguageDoc.subsections.map(sub => ({
        id: sub.id,
        title: sub.title,
        description: sub.content.split('\n')[0].substring(0, 150),
      }));
      return {
        success: true,
        data: { topics, count: topics.length },
        message: `Found ${topics.length} FinScript documentation topics`,
      };
    },
  },
  {
    name: 'get_finscript_topic',
    description: 'Get detailed documentation for a specific FinScript topic',
    inputSchema: {
      type: 'object',
      properties: {
        topic: {
          type: 'string',
          description: 'Topic to retrieve. Options: overview, variables, operators, control-flow, functions, indicators, price-functions, plotting, signals, structs, multi-symbol, colors-drawing, imports, complete-example',
        },
      },
      required: ['topic'],
    },
    handler: async (args) => {
      const topicMap: Record<string, string> = {
        'overview': 'fs-overview',
        'variables': 'fs-variables',
        'operators': 'fs-operators',
        'control-flow': 'fs-control-flow',
        'functions': 'fs-functions',
        'indicators': 'fs-indicators',
        'price-functions': 'fs-price-functions',
        'plotting': 'fs-plotting',
        'signals': 'fs-signals',
        'structs': 'fs-structs',
        'multi-symbol': 'fs-multi-symbol',
        'colors-drawing': 'fs-colors-drawing',
        'imports': 'fs-imports',
        'complete-example': 'fs-complete-example',
      };

      const topicId = topicMap[args.topic] || args.topic;
      const topic = finscriptLanguageDoc.subsections.find(s => s.id === topicId);

      if (!topic) {
        const available = Object.keys(topicMap).join(', ');
        return {
          success: false,
          error: `Topic '${args.topic}' not found`,
          suggestion: `Available topics: ${available}`,
        };
      }

      return {
        success: true,
        data: {
          id: topic.id,
          title: topic.title,
          content: topic.content,
          example: topic.codeExample,
        },
        message: `Retrieved documentation for: ${topic.title}`,
      };
    },
  },
  {
    name: 'search_finscript_docs',
    description: 'Search FinScript documentation for specific keywords or functions',
    inputSchema: {
      type: 'object',
      properties: {
        query: {
          type: 'string',
          description: 'Search query (function name, keyword, or phrase)',
        },
      },
      required: ['query'],
    },
    handler: async (args) => {
      const query = args.query.toLowerCase();
      const results: Array<{ topic: string; title: string; matches: string[] }> = [];

      for (const sub of finscriptLanguageDoc.subsections) {
        const matches: string[] = [];

        // Search in title
        if (sub.title.toLowerCase().includes(query)) {
          matches.push(`Title: ${sub.title}`);
        }

        // Search in content
        const contentLines = sub.content.split('\n');
        for (const line of contentLines) {
          if (line.toLowerCase().includes(query)) {
            matches.push(line.trim().substring(0, 100));
            if (matches.length >= 3) break;
          }
        }

        // Search in code example
        if (sub.codeExample && sub.codeExample.toLowerCase().includes(query)) {
          const exampleLines = sub.codeExample.split('\n').filter(l => l.toLowerCase().includes(query));
          matches.push(...exampleLines.slice(0, 2).map(l => `Code: ${l.trim()}`));
        }

        if (matches.length > 0) {
          results.push({
            topic: sub.id,
            title: sub.title,
            matches: matches.slice(0, 5),
          });
        }
      }

      if (results.length === 0) {
        return {
          success: false,
          error: `No results found for: ${args.query}`,
          suggestion: 'Try searching for: ema, rsi, buy, sell, plot, if, for, function',
        };
      }

      return {
        success: true,
        data: { results, count: results.length },
        message: `Found ${results.length} results for: ${args.query}`,
      };
    },
  },
  {
    name: 'get_finscript_function_help',
    description: 'Get help for a specific FinScript built-in function',
    inputSchema: {
      type: 'object',
      properties: {
        function_name: {
          type: 'string',
          description: 'Name of the function (e.g., ema, rsi, macd, buy, sell, plot_line)',
        },
      },
      required: ['function_name'],
    },
    handler: async (args) => {
      const functionDocs: Record<string, { signature: string; description: string; example: string }> = {
        // Moving Averages
        'ema': {
          signature: 'ema(symbol, period)',
          description: 'Exponential Moving Average - weighted toward recent prices. Period typically 12, 26, or 200.',
          example: 'ema_fast = ema(AAPL, 12)\nprint "EMA(12):", last(ema_fast)',
        },
        'sma': {
          signature: 'sma(symbol, period)',
          description: 'Simple Moving Average - arithmetic mean over N periods.',
          example: 'sma_20 = sma(AAPL, 20)\nplot_line sma_20, "SMA 20", "cyan"',
        },
        'wma': {
          signature: 'wma(symbol, period)',
          description: 'Weighted Moving Average',
          example: 'wma_val = wma(AAPL, 20)',
        },
        'hma': {
          signature: 'hma(symbol, period)',
          description: 'Hull Moving Average - reduced lag moving average',
          example: 'hma_9 = hma(AAPL, 9)',
        },

        // Momentum
        'rsi': {
          signature: 'rsi(symbol, period)',
          description: 'Relative Strength Index - momentum oscillator (0-100). Period typically 14. >70 overbought, <30 oversold.',
          example: 'rsi_val = rsi(AAPL, 14)\nif last(rsi_val) < 30 { buy "Oversold" }',
        },
        'macd': {
          signature: 'macd(symbol, fast?, slow?, signal?)',
          description: 'MACD Line - trend-following momentum indicator. Default: fast=12, slow=26, signal=9',
          example: 'macd_line = macd(AAPL)\nmacd_sig = macd_signal(AAPL)\nmacd_h = macd_hist(AAPL)',
        },
        'macd_signal': {
          signature: 'macd_signal(symbol)',
          description: 'MACD Signal Line - 9-period EMA of MACD line',
          example: 'signal = macd_signal(AAPL)',
        },
        'macd_hist': {
          signature: 'macd_hist(symbol)',
          description: 'MACD Histogram - difference between MACD and Signal line',
          example: 'plot_histogram macd_hist(AAPL), "MACD Histogram"',
        },
        'stochastic': {
          signature: 'stochastic(symbol, period)',
          description: 'Stochastic %K - momentum indicator comparing close to range. Period typically 14.',
          example: 'stoch = stochastic(AAPL, 14)',
        },
        'cci': {
          signature: 'cci(symbol, period)',
          description: 'Commodity Channel Index - measures deviation from average',
          example: 'cci_val = cci(AAPL, 20)',
        },
        'mfi': {
          signature: 'mfi(symbol, period)',
          description: 'Money Flow Index - volume-weighted RSI',
          example: 'mfi_val = mfi(AAPL, 14)',
        },

        // Volatility
        'atr': {
          signature: 'atr(symbol, period)',
          description: 'Average True Range - measures market volatility. Period typically 14. Used for stop-loss sizing.',
          example: 'atr_val = atr(AAPL, 14)\nstop = last(close(AAPL)) - last(atr_val) * 2',
        },
        'bollinger': {
          signature: 'bollinger(symbol, period, std)',
          description: 'Bollinger Upper Band - volatility band above SMA. Period typically 20, std typically 2.',
          example: 'bb_up = bollinger(AAPL, 20, 2)\nbb_mid = bollinger_middle(AAPL, 20)\nbb_low = bollinger_lower(AAPL, 20, 2)',
        },
        'bollinger_middle': {
          signature: 'bollinger_middle(symbol, period)',
          description: 'Bollinger Middle Band - simple moving average',
          example: 'bb_mid = bollinger_middle(AAPL, 20)',
        },
        'bollinger_lower': {
          signature: 'bollinger_lower(symbol, period, std)',
          description: 'Bollinger Lower Band - volatility band below SMA',
          example: 'bb_low = bollinger_lower(AAPL, 20, 2)',
        },
        'stdev': {
          signature: 'stdev(series, period)',
          description: 'Standard Deviation - measures volatility',
          example: 'sd = stdev(close(AAPL), 20)',
        },

        // Trend
        'adx': {
          signature: 'adx(symbol, period)',
          description: 'Average Directional Index - trend strength indicator. Period typically 14.',
          example: 'adx_val = adx(AAPL, 14)',
        },
        'sar': {
          signature: 'sar(symbol)',
          description: 'Parabolic SAR - stop and reverse indicator',
          example: 'sar_val = sar(AAPL)',
        },
        'supertrend': {
          signature: 'supertrend(symbol, period, multiplier)',
          description: 'Supertrend - trend-following indicator using ATR',
          example: 'st = supertrend(AAPL, 10, 3)',
        },

        // Volume
        'obv': {
          signature: 'obv(symbol)',
          description: 'On-Balance Volume - cumulative volume indicator',
          example: 'obv_val = obv(AAPL)',
        },
        'vwap': {
          signature: 'vwap(symbol)',
          description: 'Volume Weighted Average Price',
          example: 'vwap_val = vwap(AAPL)',
        },

        // Price functions
        'close': {
          signature: 'close(symbol)',
          description: 'Closing prices series for the symbol',
          example: 'prices = close(AAPL)\nlatest = last(prices)',
        },
        'open': {
          signature: 'open(symbol)',
          description: 'Opening prices series',
          example: 'opens = open(AAPL)',
        },
        'high': {
          signature: 'high(symbol)',
          description: 'High prices series',
          example: 'highs = high(AAPL)',
        },
        'low': {
          signature: 'low(symbol)',
          description: 'Low prices series',
          example: 'lows = low(AAPL)',
        },
        'volume': {
          signature: 'volume(symbol)',
          description: 'Volume series',
          example: 'vol = volume(AAPL)',
        },

        // Series utilities
        'last': {
          signature: 'last(series)',
          description: 'Returns the most recent (last) value from a series',
          example: 'current_price = last(close(AAPL))\nprint "Price:", current_price',
        },
        'first': {
          signature: 'first(series)',
          description: 'Returns the first value from a series',
          example: 'first_price = first(close(AAPL))',
        },
        'len': {
          signature: 'len(series)',
          description: 'Returns the length of a series or array',
          example: 'data_points = len(close(AAPL))',
        },
        'highest': {
          signature: 'highest(series, period)',
          description: 'Highest value over N periods',
          example: 'high_20 = highest(close(AAPL), 20)',
        },
        'lowest': {
          signature: 'lowest(series, period)',
          description: 'Lowest value over N periods',
          example: 'low_20 = lowest(close(AAPL), 20)',
        },

        // Math
        'abs': { signature: 'abs(x)', description: 'Absolute value', example: 'val = abs(-5)  // 5' },
        'sqrt': { signature: 'sqrt(x)', description: 'Square root', example: 'val = sqrt(144)  // 12' },
        'pow': { signature: 'pow(base, exp)', description: 'Power function', example: 'val = pow(2, 10)  // 1024' },
        'round': { signature: 'round(x, decimals?)', description: 'Round to N decimal places', example: 'val = round(3.14159, 2)  // 3.14' },
        'floor': { signature: 'floor(x)', description: 'Round down', example: 'val = floor(3.9)  // 3' },
        'ceil': { signature: 'ceil(x)', description: 'Round up', example: 'val = ceil(3.1)  // 4' },

        // Crossover
        'crossover': {
          signature: 'crossover(series_a, series_b)',
          description: 'Returns true when series A crosses above series B',
          example: 'if crossover(ema(AAPL, 12), ema(AAPL, 26)) {\n  buy "Golden cross"\n}',
        },
        'crossunder': {
          signature: 'crossunder(series_a, series_b)',
          description: 'Returns true when series A crosses below series B',
          example: 'if crossunder(ema(AAPL, 12), ema(AAPL, 26)) {\n  sell "Death cross"\n}',
        },

        // Signals
        'buy': {
          signature: 'buy "reason"',
          description: 'Generate a BUY signal with optional reason',
          example: 'if rsi_now < 30 {\n  buy "Oversold condition"\n}',
        },
        'sell': {
          signature: 'sell "reason"',
          description: 'Generate a SELL signal with optional reason',
          example: 'if rsi_now > 70 {\n  sell "Overbought condition"\n}',
        },

        // Plotting
        'plot_candlestick': {
          signature: 'plot_candlestick symbol, "title"',
          description: 'Plot candlestick chart for symbol',
          example: 'plot_candlestick AAPL, "Apple Inc."',
        },
        'plot_line': {
          signature: 'plot_line series, "label", "color"?',
          description: 'Plot line overlay on chart. Colors: cyan, orange, red, green, blue, gray, purple, white, yellow',
          example: 'plot_line ema(AAPL, 20), "EMA 20", "cyan"',
        },
        'plot': {
          signature: 'plot series, "label"',
          description: 'Plot indicator in separate panel',
          example: 'plot rsi(AAPL, 14), "RSI"',
        },
        'plot_histogram': {
          signature: 'plot_histogram series, "label", "up_color"?, "down_color"?',
          description: 'Plot histogram',
          example: 'plot_histogram macd_hist(AAPL), "MACD Hist", "#26a69a", "#ef5350"',
        },
        'hline': {
          signature: 'hline value, "label", "color"',
          description: 'Draw horizontal reference line',
          example: 'hline 70, "Overbought", "red"\nhline 30, "Oversold", "green"',
        },

        // Print
        'print': {
          signature: 'print value1, value2, ...',
          description: 'Print values to output console',
          example: 'print "Price:", round(last(close(AAPL)), 2)\nprint "RSI:", last(rsi(AAPL, 14))',
        },
      };

      const funcName = args.function_name.toLowerCase();
      const doc = functionDocs[funcName];

      if (!doc) {
        return {
          success: false,
          error: `Function '${args.function_name}' not found in documentation`,
          suggestion: 'Available functions: ema, sma, rsi, macd, atr, bollinger, buy, sell, plot_line, plot_candlestick, print',
        };
      }

      return {
        success: true,
        data: {
          function: funcName,
          signature: doc.signature,
          description: doc.description,
          example: doc.example,
        },
        message: `Help for: ${funcName}`,
      };
    },
  },
  {
    name: 'get_finscript_complete_example',
    description: 'Get a complete, runnable FinScript trading system example',
    inputSchema: {
      type: 'object',
      properties: {},
      required: [],
    },
    handler: async () => {
      const example = finscriptLanguageDoc.subsections.find(s => s.id === 'fs-complete-example');
      if (!example) {
        return { success: false, error: 'Complete example not found' };
      }
      return {
        success: true,
        data: {
          title: example.title,
          description: example.content,
          code: example.codeExample,
        },
        message: 'Complete trading system example retrieved',
      };
    },
  },
  {
    name: 'generate_finscript_template',
    description: 'Generate a FinScript code template based on strategy type',
    inputSchema: {
      type: 'object',
      properties: {
        strategy_type: {
          type: 'string',
          description: 'Type of strategy template',
          enum: ['trend-following', 'mean-reversion', 'momentum', 'multi-indicator', 'basic'],
        },
        symbol: {
          type: 'string',
          description: 'Trading symbol (default: AAPL)',
          default: 'AAPL',
        },
      },
      required: ['strategy_type'],
    },
    handler: async (args) => {
      const symbol = args.symbol || 'AAPL';
      const templates: Record<string, string> = {
        'trend-following': `// Trend Following Strategy
// Uses EMA crossover with ADX filter

ema_fast = ema(${symbol}, 12)
ema_slow = ema(${symbol}, 50)
adx_val = adx(${symbol}, 14)

fast = last(ema_fast)
slow = last(ema_slow)
trend_strength = last(adx_val)

print "EMA(12):", round(fast, 2)
print "EMA(50):", round(slow, 2)
print "ADX:", round(trend_strength, 1)

// Strong trend filter (ADX > 25)
if trend_strength > 25 {
    if fast > slow {
        buy "Strong uptrend"
    } else {
        sell "Strong downtrend"
    }
}

plot_candlestick ${symbol}, "${symbol} Trend Following"
plot_line ema_fast, "EMA 12", "cyan"
plot_line ema_slow, "EMA 50", "orange"`,

        'mean-reversion': `// Mean Reversion Strategy
// Uses Bollinger Bands and RSI

bb_up = bollinger(${symbol}, 20, 2)
bb_mid = bollinger_middle(${symbol}, 20)
bb_low = bollinger_lower(${symbol}, 20, 2)
rsi_val = rsi(${symbol}, 14)

price = last(close(${symbol}))
rsi_now = last(rsi_val)

print "Price:", round(price, 2)
print "BB Upper:", round(last(bb_up), 2)
print "BB Lower:", round(last(bb_low), 2)
print "RSI:", round(rsi_now, 1)

// Buy when price touches lower band and RSI oversold
if price < last(bb_low) and rsi_now < 35 {
    buy "Oversold - mean reversion"
}

// Sell when price touches upper band and RSI overbought
if price > last(bb_up) and rsi_now > 65 {
    sell "Overbought - mean reversion"
}

plot_candlestick ${symbol}, "${symbol} Mean Reversion"
plot_line bb_up, "BB Upper", "red"
plot_line bb_mid, "BB Mid", "gray"
plot_line bb_low, "BB Lower", "green"
plot rsi_val, "RSI"
hline 70, "OB", "red"
hline 30, "OS", "green"`,

        'momentum': `// Momentum Strategy
// Uses RSI and MACD for momentum confirmation

rsi_val = rsi(${symbol}, 14)
macd_line = macd(${symbol})
macd_sig = macd_signal(${symbol})
macd_h = macd_hist(${symbol})

rsi_now = last(rsi_val)
macd_now = last(macd_line)
signal_now = last(macd_sig)
hist = last(macd_h)

print "RSI:", round(rsi_now, 1)
print "MACD:", round(macd_now, 3)
print "Histogram:", round(hist, 3)

// Momentum buy: RSI strong + MACD positive
if rsi_now > 55 and rsi_now < 75 and hist > 0 {
    buy "Positive momentum"
}

// Momentum sell: RSI weak + MACD negative
if rsi_now < 45 and rsi_now > 25 and hist < 0 {
    sell "Negative momentum"
}

plot_candlestick ${symbol}, "${symbol} Momentum"
plot rsi_val, "RSI"
plot_histogram macd_h, "MACD Histogram"`,

        'multi-indicator': `// Multi-Indicator Strategy
// Combines trend, momentum, and volatility

// Indicators
ema_fast = ema(${symbol}, 12)
ema_slow = ema(${symbol}, 26)
rsi_val = rsi(${symbol}, 14)
atr_val = atr(${symbol}, 14)
macd_h = macd_hist(${symbol})

// Current values
ema_f = last(ema_fast)
ema_s = last(ema_slow)
rsi_now = last(rsi_val)
atr_now = last(atr_val)
macd_now = last(macd_h)
price = last(close(${symbol}))

// Score-based signal
fn calculate_score() {
    score = 0
    if ema_f > ema_s { score += 2 } else { score -= 2 }
    if rsi_now > 50 and rsi_now < 70 { score += 1 }
    if macd_now > 0 { score += 1 }
    // Volatility filter
    if atr_now > price * 0.05 { score = 0 }
    return score
}

signal = calculate_score()
print "Score:", signal, "(range: -4 to +4)"
print "ATR:", round(atr_now, 2)

if signal >= 3 {
    buy "Strong buy signal"
} else if signal <= -3 {
    sell "Strong sell signal"
}

plot_candlestick ${symbol}, "${symbol} Multi-Indicator"
plot_line ema_fast, "EMA 12", "cyan"
plot_line ema_slow, "EMA 26", "orange"
plot rsi_val, "RSI"`,

        'basic': `// Basic Strategy Template
// Simple EMA crossover with RSI filter

ema_fast = ema(${symbol}, 12)
ema_slow = ema(${symbol}, 26)
rsi_val = rsi(${symbol}, 14)

fast = last(ema_fast)
slow = last(ema_slow)
rsi_now = last(rsi_val)

print "EMA(12):", round(fast, 2)
print "EMA(26):", round(slow, 2)
print "RSI:", round(rsi_now, 1)

if fast > slow and rsi_now < 70 {
    buy "Bullish crossover"
}

if fast < slow and rsi_now > 30 {
    sell "Bearish crossover"
}

plot_candlestick ${symbol}, "${symbol} Strategy"
plot_line ema_fast, "EMA 12", "cyan"
plot_line ema_slow, "EMA 26", "orange"
plot rsi_val, "RSI"`,
      };

      const template = templates[args.strategy_type];
      if (!template) {
        return {
          success: false,
          error: `Strategy type '${args.strategy_type}' not found`,
          suggestion: 'Available types: trend-following, mean-reversion, momentum, multi-indicator, basic',
        };
      }

      return {
        success: true,
        data: {
          strategy_type: args.strategy_type,
          symbol: symbol,
          code: template,
        },
        message: `Generated ${args.strategy_type} template for ${symbol}`,
      };
    },
  },
];
