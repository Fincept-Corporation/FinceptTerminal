import React from 'react';

const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

// FinScript function documentation database
export const FINSCRIPT_FUNCTIONS: Record<string, {
  signature: string;
  description: string;
  params?: string[];
  returns?: string;
  example?: string;
}> = {
  // Moving Averages
  'sma': {
    signature: 'sma(symbol, period)',
    description: 'Simple Moving Average - arithmetic mean over N periods',
    params: ['symbol: Ticker symbol', 'period: Number of periods'],
    returns: 'Series',
    example: 'sma_20 = sma(AAPL, 20)'
  },
  'ema': {
    signature: 'ema(symbol, period)',
    description: 'Exponential Moving Average - weighted toward recent prices',
    params: ['symbol: Ticker symbol', 'period: Number of periods'],
    returns: 'Series',
    example: 'ema_12 = ema(AAPL, 12)'
  },
  'wma': {
    signature: 'wma(symbol, period)',
    description: 'Weighted Moving Average',
    params: ['symbol: Ticker symbol', 'period: Number of periods'],
    returns: 'Series'
  },
  'hma': {
    signature: 'hma(symbol, period)',
    description: 'Hull Moving Average - reduced lag moving average',
    params: ['symbol: Ticker symbol', 'period: Number of periods'],
    returns: 'Series'
  },

  // Momentum
  'rsi': {
    signature: 'rsi(symbol, period)',
    description: 'Relative Strength Index - momentum oscillator (0-100)',
    params: ['symbol: Ticker symbol', 'period: Number of periods (typically 14)'],
    returns: 'Series',
    example: 'rsi_val = rsi(AAPL, 14)\nif last(rsi_val) > 70 { sell "Overbought" }'
  },
  'macd': {
    signature: 'macd(symbol, fast?, slow?, signal?)',
    description: 'MACD Line - trend-following momentum indicator',
    params: ['symbol: Ticker symbol', 'fast: Fast period (default 12)', 'slow: Slow period (default 26)', 'signal: Signal period (default 9)'],
    returns: 'Series',
    example: 'macd_line = macd(AAPL)'
  },
  'macd_signal': {
    signature: 'macd_signal(symbol)',
    description: 'MACD Signal Line - 9-period EMA of MACD',
    returns: 'Series'
  },
  'macd_hist': {
    signature: 'macd_hist(symbol)',
    description: 'MACD Histogram - difference between MACD and Signal',
    returns: 'Series',
    example: 'plot_histogram macd_hist(AAPL), "MACD"'
  },
  'stochastic': {
    signature: 'stochastic(symbol, period)',
    description: 'Stochastic %K - momentum indicator comparing close to range',
    params: ['symbol: Ticker symbol', 'period: Look-back period (typically 14)'],
    returns: 'Series'
  },
  'cci': {
    signature: 'cci(symbol, period)',
    description: 'Commodity Channel Index - measures deviation from average',
    returns: 'Series'
  },
  'mfi': {
    signature: 'mfi(symbol, period)',
    description: 'Money Flow Index - volume-weighted RSI',
    returns: 'Series'
  },
  'williams_r': {
    signature: 'williams_r(symbol, period)',
    description: "Williams %R - momentum indicator",
    returns: 'Series'
  },

  // Volatility
  'atr': {
    signature: 'atr(symbol, period)',
    description: 'Average True Range - measures market volatility',
    params: ['symbol: Ticker symbol', 'period: Number of periods (typically 14)'],
    returns: 'Series',
    example: 'atr_val = atr(AAPL, 14)\nstop = last(close(AAPL)) - last(atr_val) * 2'
  },
  'bollinger': {
    signature: 'bollinger(symbol, period, std)',
    description: 'Bollinger Upper Band - volatility band above SMA',
    params: ['symbol: Ticker symbol', 'period: MA period', 'std: Standard deviations (typically 2)'],
    returns: 'Series',
    example: 'bb_up = bollinger(AAPL, 20, 2)'
  },
  'bollinger_middle': {
    signature: 'bollinger_middle(symbol, period)',
    description: 'Bollinger Middle Band - simple moving average',
    returns: 'Series'
  },
  'bollinger_lower': {
    signature: 'bollinger_lower(symbol, period, std)',
    description: 'Bollinger Lower Band - volatility band below SMA',
    returns: 'Series'
  },
  'stdev': {
    signature: 'stdev(series, period)',
    description: 'Standard Deviation - measures volatility',
    returns: 'Series'
  },

  // Trend
  'adx': {
    signature: 'adx(symbol, period)',
    description: 'Average Directional Index - trend strength indicator',
    returns: 'Series'
  },
  'sar': {
    signature: 'sar(symbol)',
    description: 'Parabolic SAR - stop and reverse indicator',
    returns: 'Series'
  },
  'supertrend': {
    signature: 'supertrend(symbol, period, multiplier)',
    description: 'Supertrend - trend-following indicator using ATR',
    returns: 'Series'
  },

  // Price functions
  'close': {
    signature: 'close(symbol)',
    description: 'Closing prices series for the symbol',
    params: ['symbol: Ticker symbol'],
    returns: 'Series',
    example: 'prices = close(AAPL)\nlatest = last(prices)'
  },
  'open': {
    signature: 'open(symbol)',
    description: 'Opening prices series',
    returns: 'Series'
  },
  'high': {
    signature: 'high(symbol)',
    description: 'High prices series',
    returns: 'Series'
  },
  'low': {
    signature: 'low(symbol)',
    description: 'Low prices series',
    returns: 'Series'
  },
  'volume': {
    signature: 'volume(symbol)',
    description: 'Volume series',
    returns: 'Series'
  },

  // Series utilities
  'last': {
    signature: 'last(series)',
    description: 'Returns the most recent (last) value from a series',
    params: ['series: Series or array'],
    returns: 'Number',
    example: 'current_price = last(close(AAPL))'
  },
  'first': {
    signature: 'first(series)',
    description: 'Returns the first value from a series',
    returns: 'Number'
  },
  'len': {
    signature: 'len(series)',
    description: 'Returns the length of a series or array',
    returns: 'Number'
  },
  'highest': {
    signature: 'highest(series, period)',
    description: 'Highest value over N periods',
    params: ['series: Data series', 'period: Look-back period'],
    returns: 'Series'
  },
  'lowest': {
    signature: 'lowest(series, period)',
    description: 'Lowest value over N periods',
    returns: 'Series'
  },

  // Math functions
  'abs': { signature: 'abs(x)', description: 'Absolute value', returns: 'Number' },
  'sqrt': { signature: 'sqrt(x)', description: 'Square root', returns: 'Number' },
  'pow': { signature: 'pow(base, exp)', description: 'Power function', returns: 'Number' },
  'round': { signature: 'round(x, decimals?)', description: 'Round to N decimal places', returns: 'Number' },
  'floor': { signature: 'floor(x)', description: 'Round down', returns: 'Number' },
  'ceil': { signature: 'ceil(x)', description: 'Round up', returns: 'Number' },
  'log': { signature: 'log(x)', description: 'Natural logarithm', returns: 'Number' },
  'exp': { signature: 'exp(x)', description: 'Exponential (e^x)', returns: 'Number' },

  // Crossover
  'crossover': {
    signature: 'crossover(series_a, series_b)',
    description: 'Returns true when series A crosses above series B',
    params: ['series_a: First series', 'series_b: Second series'],
    returns: 'Bool',
    example: 'if crossover(ema(AAPL, 12), ema(AAPL, 26)) {\n  buy "Golden cross"\n}'
  },
  'crossunder': {
    signature: 'crossunder(series_a, series_b)',
    description: 'Returns true when series A crosses below series B',
    returns: 'Bool'
  },

  // Signals
  'buy': {
    signature: 'buy "reason"',
    description: 'Generate a BUY signal with optional reason',
    example: 'if rsi_now < 30 {\n  buy "Oversold condition"\n}'
  },
  'sell': {
    signature: 'sell "reason"',
    description: 'Generate a SELL signal with optional reason',
    example: 'if rsi_now > 70 {\n  sell "Overbought condition"\n}'
  },

  // Plotting
  'plot_candlestick': {
    signature: 'plot_candlestick symbol, "title"',
    description: 'Plot candlestick chart for symbol',
    example: 'plot_candlestick AAPL, "Apple Inc."'
  },
  'plot_line': {
    signature: 'plot_line series, "label", "color"?',
    description: 'Plot line overlay on chart',
    params: ['series: Data to plot', 'label: Display name', 'color: Optional color (cyan, orange, red, green, etc.)'],
    example: 'plot_line ema(AAPL, 20), "EMA 20", "cyan"'
  },
  'plot': {
    signature: 'plot series, "label"',
    description: 'Plot indicator in separate panel',
    example: 'plot rsi(AAPL, 14), "RSI"'
  },
  'hline': {
    signature: 'hline value, "label", "color"',
    description: 'Draw horizontal reference line',
    example: 'hline 70, "Overbought", "red"'
  },

  // Print
  'print': {
    signature: 'print value1, value2, ...',
    description: 'Print values to output console',
    example: 'print "Price:", round(last(close(AAPL)), 2)'
  },
};

interface FinScriptTooltipProps {
  functionName: string;
  position: { x: number; y: number };
}

export const FinScriptTooltip: React.FC<FinScriptTooltipProps> = ({ functionName, position }) => {
  const doc = FINSCRIPT_FUNCTIONS[functionName];

  if (!doc) return null;

  return (
    <div style={{
      position: 'fixed',
      left: position.x,
      top: position.y,
      zIndex: 10000,
      maxWidth: '320px',
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.ORANGE}`,
      borderRadius: '3px',
      boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
      padding: '8px',
      fontFamily: FONT,
      pointerEvents: 'none',
    }}>
      {/* Function signature */}
      <div style={{
        fontSize: '10px',
        fontWeight: 'bold',
        color: F.ORANGE,
        marginBottom: '6px',
        fontFamily: 'Consolas, monospace',
      }}>
        {doc.signature}
      </div>

      {/* Description */}
      <div style={{
        fontSize: '9px',
        color: F.WHITE,
        lineHeight: '1.4',
        marginBottom: doc.params || doc.returns || doc.example ? '6px' : 0,
      }}>
        {doc.description}
      </div>

      {/* Parameters */}
      {doc.params && doc.params.length > 0 && (
        <div style={{ marginBottom: '6px' }}>
          <div style={{ fontSize: '8px', color: F.CYAN, fontWeight: 'bold', marginBottom: '2px' }}>
            PARAMETERS
          </div>
          {doc.params.map((param, idx) => (
            <div key={idx} style={{
              fontSize: '8px',
              color: F.GRAY,
              lineHeight: '1.3',
              paddingLeft: '8px',
            }}>
              • {param}
            </div>
          ))}
        </div>
      )}

      {/* Returns */}
      {doc.returns && (
        <div style={{ marginBottom: doc.example ? '6px' : 0 }}>
          <span style={{ fontSize: '8px', color: F.CYAN, fontWeight: 'bold' }}>RETURNS: </span>
          <span style={{ fontSize: '8px', color: F.GRAY }}>{doc.returns}</span>
        </div>
      )}

      {/* Example */}
      {doc.example && (
        <div>
          <div style={{ fontSize: '8px', color: F.CYAN, fontWeight: 'bold', marginBottom: '2px' }}>
            EXAMPLE
          </div>
          <pre style={{
            margin: 0,
            padding: '4px',
            backgroundColor: F.DARK_BG,
            border: `1px solid ${F.BORDER}`,
            borderRadius: '2px',
            fontSize: '8px',
            lineHeight: '1.4',
            color: F.WHITE,
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-word',
          }}>
            {doc.example}
          </pre>
        </div>
      )}
    </div>
  );
};
