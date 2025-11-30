/**
 * Strategy Templates Library
 *
 * Pre-built strategy templates for quick backtesting.
 * Each template is a working strategy with configurable parameters.
 */

export interface StrategyTemplate {
  id: string;
  name: string;
  category: 'momentum' | 'mean-reversion' | 'trend-following' | 'breakout' | 'ml' | 'arbitrage';
  description: string;
  difficulty: 'beginner' | 'intermediate' | 'advanced';
  pythonCode: string;
  parameters: Array<{
    name: string;
    type: 'number' | 'string' | 'boolean';
    default: any;
    min?: number;
    max?: number;
    description: string;
  }>;
  requiredData: {
    symbols: string[];
    assetClass: string;
    resolution: string;
    minHistoryDays: number;
  };
  expectedPerformance?: {
    avgReturn: string;
    sharpeRange: string;
    maxDD: string;
  };
}

export const STRATEGY_TEMPLATES: StrategyTemplate[] = [
  {
    id: 'sma_crossover',
    name: 'Simple Moving Average Crossover',
    category: 'trend-following',
    difficulty: 'beginner',
    description: 'Classic trend-following strategy using fast and slow moving averages. Buys when fast crosses above slow, sells when fast crosses below slow.',
    pythonCode: `from AlgorithmImports import *

class SMA_Crossover(QCAlgorithm):
    """
    Simple Moving Average Crossover Strategy

    Parameters:
    - fast_period: Fast SMA period (default: {{fast_period}})
    - slow_period: Slow SMA period (default: {{slow_period}})
    - symbol: Trading symbol (default: {{symbol}})
    """

    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Parameters (will be replaced with actual values)
        self.fast_period = {{fast_period}}
        self.slow_period = {{slow_period}}
        self.symbol_name = "{{symbol}}"

        # Add equity
        self.symbol = self.AddEquity(self.symbol_name, Resolution.Daily).Symbol

        # Create indicators
        self.fast_sma = self.SMA(self.symbol, self.fast_period, Resolution.Daily)
        self.slow_sma = self.SMA(self.symbol, self.slow_period, Resolution.Daily)

        # Warm up indicators
        self.SetWarmUp(self.slow_period)

    def OnData(self, data):
        if self.IsWarmingUp:
            return

        if not self.fast_sma.IsReady or not self.slow_sma.IsReady:
            return

        # Get current indicator values
        fast = self.fast_sma.Current.Value
        slow = self.slow_sma.Current.Value

        # Buy signal: fast crosses above slow
        if fast > slow:
            if not self.Portfolio.Invested:
                self.SetHoldings(self.symbol, 1.0)
                self.Debug(f"BUY: Fast SMA ({fast:.2f}) > Slow SMA ({slow:.2f})")

        # Sell signal: fast crosses below slow
        elif fast < slow:
            if self.Portfolio.Invested:
                self.Liquidate(self.symbol)
                self.Debug(f"SELL: Fast SMA ({fast:.2f}) < Slow SMA ({slow:.2f})")
`,
    parameters: [
      { name: 'fast_period', type: 'number', default: 50, min: 10, max: 100, description: 'Fast moving average period' },
      { name: 'slow_period', type: 'number', default: 200, min: 100, max: 300, description: 'Slow moving average period' },
      { name: 'symbol', type: 'string', default: 'SPY', description: 'Trading symbol' },
    ],
    requiredData: {
      symbols: ['SPY'],
      assetClass: 'equity',
      resolution: 'daily',
      minHistoryDays: 365,
    },
    expectedPerformance: {
      avgReturn: '8-12% annually',
      sharpeRange: '0.8-1.2',
      maxDD: '15-25%',
    },
  },

  {
    id: 'rsi_mean_reversion',
    name: 'RSI Mean Reversion',
    category: 'mean-reversion',
    difficulty: 'beginner',
    description: 'Mean reversion strategy using RSI indicator. Buys when oversold (RSI < 30), sells when overbought (RSI > 70).',
    pythonCode: `from AlgorithmImports import *

class RSI_MeanReversion(QCAlgorithm):
    """
    RSI Mean Reversion Strategy

    Parameters:
    - rsi_period: RSI period (default: {{rsi_period}})
    - oversold_threshold: Oversold level (default: {{oversold_threshold}})
    - overbought_threshold: Overbought level (default: {{overbought_threshold}})
    """

    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Parameters
        self.rsi_period = {{rsi_period}}
        self.oversold = {{oversold_threshold}}
        self.overbought = {{overbought_threshold}}
        self.symbol_name = "{{symbol}}"

        # Add equity
        self.symbol = self.AddEquity(self.symbol_name, Resolution.Daily).Symbol

        # Create RSI indicator
        self.rsi = self.RSI(self.symbol, self.rsi_period, MovingAverageType.Wilders, Resolution.Daily)

        self.SetWarmUp(self.rsi_period)

    def OnData(self, data):
        if self.IsWarmingUp:
            return

        if not self.rsi.IsReady:
            return

        rsi_value = self.rsi.Current.Value

        # Buy signal: RSI below oversold threshold
        if rsi_value < self.oversold:
            if not self.Portfolio.Invested:
                self.SetHoldings(self.symbol, 1.0)
                self.Debug(f"BUY: RSI {rsi_value:.2f} < {self.oversold}")

        # Sell signal: RSI above overbought threshold
        elif rsi_value > self.overbought:
            if self.Portfolio.Invested:
                self.Liquidate(self.symbol)
                self.Debug(f"SELL: RSI {rsi_value:.2f} > {self.overbought}")
`,
    parameters: [
      { name: 'rsi_period', type: 'number', default: 14, min: 5, max: 30, description: 'RSI calculation period' },
      { name: 'oversold_threshold', type: 'number', default: 30, min: 20, max: 40, description: 'Oversold threshold for buying' },
      { name: 'overbought_threshold', type: 'number', default: 70, min: 60, max: 80, description: 'Overbought threshold for selling' },
      { name: 'symbol', type: 'string', default: 'SPY', description: 'Trading symbol' },
    ],
    requiredData: {
      symbols: ['SPY'],
      assetClass: 'equity',
      resolution: 'daily',
      minHistoryDays: 180,
    },
    expectedPerformance: {
      avgReturn: '10-15% annually',
      sharpeRange: '0.9-1.3',
      maxDD: '12-20%',
    },
  },

  {
    id: 'macd_momentum',
    name: 'MACD Momentum',
    category: 'momentum',
    difficulty: 'intermediate',
    description: 'Momentum strategy using MACD indicator. Trades based on MACD line crossing signal line.',
    pythonCode: `from AlgorithmImports import *

class MACD_Momentum(QCAlgorithm):
    """
    MACD Momentum Strategy

    Parameters:
    - fast_period: MACD fast period (default: {{fast_period}})
    - slow_period: MACD slow period (default: {{slow_period}})
    - signal_period: Signal line period (default: {{signal_period}})
    """

    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Parameters
        self.fast = {{fast_period}}
        self.slow = {{slow_period}}
        self.signal = {{signal_period}}
        self.symbol_name = "{{symbol}}"

        # Add equity
        self.symbol = self.AddEquity(self.symbol_name, Resolution.Daily).Symbol

        # Create MACD indicator
        self.macd = self.MACD(self.symbol, self.fast, self.slow, self.signal, MovingAverageType.Exponential, Resolution.Daily)

        self.SetWarmUp(max(self.fast, self.slow) + self.signal)

    def OnData(self, data):
        if self.IsWarmingUp:
            return

        if not self.macd.IsReady:
            return

        macd_value = self.macd.Current.Value
        signal_value = self.macd.Signal.Current.Value

        # Buy signal: MACD crosses above signal
        if macd_value > signal_value:
            if not self.Portfolio.Invested:
                self.SetHoldings(self.symbol, 1.0)
                self.Debug(f"BUY: MACD {macd_value:.2f} > Signal {signal_value:.2f}")

        # Sell signal: MACD crosses below signal
        elif macd_value < signal_value:
            if self.Portfolio.Invested:
                self.Liquidate(self.symbol)
                self.Debug(f"SELL: MACD {macd_value:.2f} < Signal {signal_value:.2f}")
`,
    parameters: [
      { name: 'fast_period', type: 'number', default: 12, min: 8, max: 20, description: 'Fast EMA period' },
      { name: 'slow_period', type: 'number', default: 26, min: 20, max: 40, description: 'Slow EMA period' },
      { name: 'signal_period', type: 'number', default: 9, min: 5, max: 15, description: 'Signal line period' },
      { name: 'symbol', type: 'string', default: 'SPY', description: 'Trading symbol' },
    ],
    requiredData: {
      symbols: ['SPY'],
      assetClass: 'equity',
      resolution: 'daily',
      minHistoryDays: 180,
    },
    expectedPerformance: {
      avgReturn: '12-18% annually',
      sharpeRange: '1.0-1.5',
      maxDD: '10-18%',
    },
  },

  {
    id: 'bollinger_breakout',
    name: 'Bollinger Bands Breakout',
    category: 'breakout',
    difficulty: 'intermediate',
    description: 'Breakout strategy using Bollinger Bands. Buys on upper band breakout, sells on lower band breakdown.',
    pythonCode: `from AlgorithmImports import *

class BollingerBreakout(QCAlgorithm):
    """
    Bollinger Bands Breakout Strategy

    Parameters:
    - bb_period: Bollinger Bands period (default: {{bb_period}})
    - bb_std_dev: Standard deviation multiplier (default: {{bb_std_dev}})
    """

    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Parameters
        self.period = {{bb_period}}
        self.std_dev = {{bb_std_dev}}
        self.symbol_name = "{{symbol}}"

        # Add equity
        self.symbol = self.AddEquity(self.symbol_name, Resolution.Daily).Symbol

        # Create Bollinger Bands
        self.bb = self.BB(self.symbol, self.period, self.std_dev, MovingAverageType.Simple, Resolution.Daily)

        self.SetWarmUp(self.period)

    def OnData(self, data):
        if self.IsWarmingUp:
            return

        if not self.bb.IsReady:
            return

        if not data.ContainsKey(self.symbol):
            return

        price = data[self.symbol].Close
        upper = self.bb.UpperBand.Current.Value
        lower = self.bb.LowerBand.Current.Value
        middle = self.bb.MiddleBand.Current.Value

        # Buy signal: price breaks above upper band
        if price > upper:
            if not self.Portfolio.Invested:
                self.SetHoldings(self.symbol, 1.0)
                self.Debug(f"BUY: Price {price:.2f} > Upper Band {upper:.2f}")

        # Sell signal: price breaks below lower band
        elif price < lower:
            if self.Portfolio.Invested:
                self.Liquidate(self.symbol)
                self.Debug(f"SELL: Price {price:.2f} < Lower Band {lower:.2f}")

        # Exit on mean reversion to middle band
        elif self.Portfolio.Invested and abs(price - middle) < (upper - middle) * 0.2:
            self.Liquidate(self.symbol)
            self.Debug(f"EXIT: Price {price:.2f} near Middle Band {middle:.2f}")
`,
    parameters: [
      { name: 'bb_period', type: 'number', default: 20, min: 10, max: 50, description: 'Bollinger Bands period' },
      { name: 'bb_std_dev', type: 'number', default: 2, min: 1, max: 3, description: 'Standard deviation multiplier' },
      { name: 'symbol', type: 'string', default: 'SPY', description: 'Trading symbol' },
    ],
    requiredData: {
      symbols: ['SPY'],
      assetClass: 'equity',
      resolution: 'daily',
      minHistoryDays: 120,
    },
    expectedPerformance: {
      avgReturn: '15-20% annually',
      sharpeRange: '1.1-1.6',
      maxDD: '12-22%',
    },
  },

  {
    id: 'pairs_trading',
    name: 'Pairs Trading (Cointegration)',
    category: 'arbitrage',
    difficulty: 'advanced',
    description: 'Statistical arbitrage strategy trading two cointegrated assets. Goes long/short based on spread deviation.',
    pythonCode: `from AlgorithmImports import *
import numpy as np

class PairsTrading(QCAlgorithm):
    """
    Pairs Trading Strategy

    Parameters:
    - symbol1: First symbol (default: {{symbol1}})
    - symbol2: Second symbol (default: {{symbol2}})
    - lookback: Lookback period for spread calculation (default: {{lookback}})
    - entry_z: Z-score threshold for entry (default: {{entry_z}})
    - exit_z: Z-score threshold for exit (default: {{exit_z}})
    """

    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Parameters
        self.lookback = {{lookback}}
        self.entry_z = {{entry_z}}
        self.exit_z = {{exit_z}}

        # Add symbols
        self.symbol1 = self.AddEquity("{{symbol1}}", Resolution.Daily).Symbol
        self.symbol2 = self.AddEquity("{{symbol2}}", Resolution.Daily).Symbol

        # Store price history
        self.history = []
        self.SetWarmUp(self.lookback)

    def OnData(self, data):
        if self.IsWarmingUp:
            return

        if not data.ContainsKey(self.symbol1) or not data.ContainsKey(self.symbol2):
            return

        price1 = data[self.symbol1].Close
        price2 = data[self.symbol2].Close

        # Calculate spread
        spread = price1 - price2
        self.history.append(spread)

        if len(self.history) < self.lookback:
            return

        # Keep only lookback period
        self.history = self.history[-self.lookback:]

        # Calculate z-score
        mean_spread = np.mean(self.history)
        std_spread = np.std(self.history)

        if std_spread == 0:
            return

        z_score = (spread - mean_spread) / std_spread

        # Entry signals
        if z_score > self.entry_z and not self.Portfolio.Invested:
            # Spread too high: short symbol1, long symbol2
            self.SetHoldings(self.symbol1, -0.5)
            self.SetHoldings(self.symbol2, 0.5)
            self.Debug(f"SHORT PAIR: Z-score {z_score:.2f}")

        elif z_score < -self.entry_z and not self.Portfolio.Invested:
            # Spread too low: long symbol1, short symbol2
            self.SetHoldings(self.symbol1, 0.5)
            self.SetHoldings(self.symbol2, -0.5)
            self.Debug(f"LONG PAIR: Z-score {z_score:.2f}")

        # Exit signals
        elif abs(z_score) < self.exit_z and self.Portfolio.Invested:
            self.Liquidate()
            self.Debug(f"EXIT: Z-score {z_score:.2f}")
`,
    parameters: [
      { name: 'symbol1', type: 'string', default: 'SPY', description: 'First symbol' },
      { name: 'symbol2', type: 'string', default: 'IWM', description: 'Second symbol (should be cointegrated)' },
      { name: 'lookback', type: 'number', default: 60, min: 30, max: 120, description: 'Lookback period for spread' },
      { name: 'entry_z', type: 'number', default: 2.0, min: 1.5, max: 3.0, description: 'Z-score entry threshold' },
      { name: 'exit_z', type: 'number', default: 0.5, min: 0.1, max: 1.0, description: 'Z-score exit threshold' },
    ],
    requiredData: {
      symbols: ['SPY', 'IWM'],
      assetClass: 'equity',
      resolution: 'daily',
      minHistoryDays: 180,
    },
    expectedPerformance: {
      avgReturn: '8-12% annually',
      sharpeRange: '1.5-2.0',
      maxDD: '8-15%',
    },
  },
];

/**
 * Get template by ID
 */
export function getTemplateById(id: string): StrategyTemplate | undefined {
  return STRATEGY_TEMPLATES.find((t) => t.id === id);
}

/**
 * Get templates by category
 */
export function getTemplatesByCategory(category: string): StrategyTemplate[] {
  return STRATEGY_TEMPLATES.filter((t) => t.category === category);
}

/**
 * Get templates by difficulty
 */
export function getTemplatesByDifficulty(difficulty: string): StrategyTemplate[] {
  return STRATEGY_TEMPLATES.filter((t) => t.difficulty === difficulty);
}

/**
 * Fill template with parameter values
 */
export function fillTemplate(template: StrategyTemplate, params: Record<string, any>): string {
  let code = template.pythonCode;

  // Replace all {{parameter}} placeholders
  for (const [key, value] of Object.entries(params)) {
    const placeholder = new RegExp(`\\{\\{${key}\\}\\}`, 'g');
    code = code.replace(placeholder, String(value));
  }

  // Fill any remaining placeholders with defaults
  for (const param of template.parameters) {
    const placeholder = new RegExp(`\\{\\{${param.name}\\}\\}`, 'g');
    code = code.replace(placeholder, String(param.default));
  }

  return code;
}

/**
 * Get all categories
 */
export function getAllCategories(): string[] {
  return Array.from(new Set(STRATEGY_TEMPLATES.map((t) => t.category)));
}
