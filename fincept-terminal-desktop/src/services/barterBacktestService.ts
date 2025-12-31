/**
 * Barter Backtesting Service
 * TypeScript integration for the Rust barter-rs backend
 */

import { invoke } from '@tauri-apps/api/core';

// ==================== Types ====================

export enum TradingMode {
  Live = 'Live',
  Paper = 'Paper',
  Backtest = 'Backtest',
}

export enum Exchange {
  Binance = 'Binance',
  Bybit = 'Bybit',
  Coinbase = 'Coinbase',
  Kraken = 'Kraken',
  OKX = 'OKX',
  Bitfinex = 'Bitfinex',
  Bitmex = 'Bitmex',
  GateIO = 'GateIO',
}

export enum OrderSide {
  Buy = 'Buy',
  Sell = 'Sell',
}

export enum OrderType {
  Market = 'Market',
  Limit = 'Limit',
  StopMarket = 'StopMarket',
  StopLimit = 'StopLimit',
}

export interface Candle {
  timestamp: string;
  open: string;
  high: string;
  low: string;
  close: string;
  volume: string;
}

export interface StrategyConfig {
  name: string;
  mode: TradingMode;
  exchanges: Exchange[];
  symbols: string[];
  parameters: Record<string, any>;
}

export interface BacktestConfig {
  strategy: StrategyConfig;
  start_date: string;
  end_date: string;
  initial_capital: string;
  commission_rate: string;
}

export interface BacktestResult {
  total_return: string;
  sharpe_ratio: string;
  sortino_ratio: string;
  max_drawdown: string;
  win_rate: string;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  average_win: string;
  average_loss: string;
  profit_factor: string;
  final_capital: string;
  equity_curve: Array<[string, string]>;
  trades: TradeRecord[];
}

export interface TradeRecord {
  entry_time: string;
  exit_time: string;
  symbol: string;
  side: OrderSide;
  entry_price: string;
  exit_price: string;
  quantity: string;
  pnl: string;
  return_pct: string;
}

export interface DatasetMapping {
  timestamp_column: string;
  timestamp_format: TimestampFormat;
  open_column?: string;
  high_column?: string;
  low_column?: string;
  close_column?: string;
  volume_column?: string;
  price_column?: string;
  size_column?: string;
  symbol_column?: string;
  aggregation_interval?: string;
}

export type TimestampFormat =
  | { UnixSeconds: null }
  | { UnixMilliseconds: null }
  | { UnixMicroseconds: null }
  | { UnixNanoseconds: null }
  | { Rfc3339: null }
  | { Custom: string };

// ==================== Backtesting API ====================

export class BarterBacktestService {
  /**
   * Run a backtest with given configuration and candle data
   */
  static async runBacktest(
    config: BacktestConfig,
    candleData: Record<string, Candle[]>
  ): Promise<BacktestResult> {
    return invoke('run_backtest', {
      config,
      candleData,
    });
  }

  /**
   * Optimize strategy parameters using grid search
   */
  static async optimizeStrategy(
    config: BacktestConfig,
    parameterGrid: Record<string, any[]>,
    candleData: Record<string, Candle[]>
  ): Promise<Array<[Record<string, any>, BacktestResult]>> {
    return invoke('optimize_strategy', {
      config,
      parameterGrid,
      candleData,
    });
  }

  /**
   * List available strategies
   */
  static async listStrategies(): Promise<string[]> {
    return invoke('list_strategies');
  }

  /**
   * Get parameters for a strategy
   */
  static async getStrategyParameters(strategyName: string): Promise<Record<string, any>> {
    return invoke('get_strategy_parameters', { strategyName });
  }

  /**
   * Update strategy parameters
   */
  static async updateStrategyParameters(
    strategyName: string,
    parameters: Record<string, any>
  ): Promise<void> {
    return invoke('update_strategy_parameters', {
      strategyName,
      parameters,
    });
  }

  /**
   * Get supported exchanges
   */
  static async getSupportedExchanges(): Promise<string[]> {
    return invoke('get_supported_exchanges');
  }

  /**
   * Get trading modes
   */
  static async getTradingModes(): Promise<string[]> {
    return invoke('get_trading_modes');
  }
}

// ==================== Data Mapping API ====================

export class DataMapperService {
  /**
   * Parse Databento dataset file
   */
  static async parseDatabento(filePath: string): Promise<Record<string, Candle[]>> {
    return invoke('parse_databento_dataset', { filePath });
  }

  /**
   * Map custom OHLCV dataset to candle format
   */
  static async mapCustomDataset(
    mapping: DatasetMapping,
    rows: Record<string, string>[],
    symbol: string
  ): Promise<Candle[]> {
    return invoke('map_custom_dataset', {
      mapping,
      rows,
      symbol,
    });
  }

  /**
   * Map tick data to aggregated candles
   */
  static async mapTickDataset(
    mapping: DatasetMapping,
    rows: Record<string, string>[]
  ): Promise<Record<string, Candle[]>> {
    return invoke('map_tick_dataset', {
      mapping,
      rows,
    });
  }
}

// ==================== Helper Functions ====================

/**
 * Create timestamp format helper
 */
export const TimestampFormats = {
  unixSeconds: (): TimestampFormat => ({ UnixSeconds: null }),
  unixMilliseconds: (): TimestampFormat => ({ UnixMilliseconds: null }),
  unixMicroseconds: (): TimestampFormat => ({ UnixMicroseconds: null }),
  unixNanoseconds: (): TimestampFormat => ({ UnixNanoseconds: null }),
  rfc3339: (): TimestampFormat => ({ Rfc3339: null }),
  custom: (format: string): TimestampFormat => ({ Custom: format }),
};

/**
 * Example: Create SMA strategy config
 */
export function createSMAStrategyConfig(
  symbols: string[],
  fastPeriod: number = 10,
  slowPeriod: number = 20,
  positionSize: number = 1.0
): StrategyConfig {
  return {
    name: 'SMA Crossover',
    mode: TradingMode.Backtest,
    exchanges: [Exchange.Binance],
    symbols,
    parameters: {
      fast_period: fastPeriod,
      slow_period: slowPeriod,
      position_size: positionSize.toString(),
    },
  };
}

/**
 * Example: Parse CSV to rows for mapping
 */
export function parseCSVToRows(csvText: string, delimiter: string = ','): Record<string, string>[] {
  const lines = csvText.trim().split('\n');
  if (lines.length < 2) return [];

  const headers = lines[0].split(delimiter).map(h => h.trim());
  const rows: Record<string, string>[] = [];

  for (let i = 1; i < lines.length; i++) {
    const values = lines[i].split(delimiter).map(v => v.trim());
    const row: Record<string, string> = {};
    headers.forEach((header, index) => {
      row[header] = values[index] || '';
    });
    rows.push(row);
  }

  return rows;
}

/**
 * Format backtest results for display
 */
export function formatBacktestResults(result: BacktestResult): string {
  return `
=== Backtest Results ===
Total Return: ${parseFloat(result.total_return).toFixed(2)}%
Sharpe Ratio: ${parseFloat(result.sharpe_ratio).toFixed(2)}
Sortino Ratio: ${parseFloat(result.sortino_ratio).toFixed(2)}
Max Drawdown: ${parseFloat(result.max_drawdown).toFixed(2)}%
Win Rate: ${parseFloat(result.win_rate).toFixed(2)}%
Total Trades: ${result.total_trades}
Winning Trades: ${result.winning_trades}
Losing Trades: ${result.losing_trades}
Average Win: $${parseFloat(result.average_win).toFixed(2)}
Average Loss: $${parseFloat(result.average_loss).toFixed(2)}
Profit Factor: ${parseFloat(result.profit_factor).toFixed(2)}
Final Capital: $${parseFloat(result.final_capital).toFixed(2)}
  `.trim();
}
