/**
 * VectorBT Adapter
 *
 * Adapter for VectorBT - fast backtesting library using vectorized operations.
 * Specializes in high-performance backtesting with NumPy/Pandas.
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseBacktestingAdapter } from '../BaseAdapter';
import {
  ProviderCapabilities,
  ProviderConfig,
  InitResult,
  TestResult,
  BacktestRequest,
  BacktestResult,
  OptimizationRequest,
  OptimizationResult,
  DataRequest,
  HistoricalData,
  IndicatorType,
  IndicatorParams,
  IndicatorResult,
  DataCatalog,
  IndicatorCatalog,
  ValidationResult,
  HealthStatus,
} from '../../interfaces/types';
import { StrategyDefinition } from '../../interfaces/IStrategyDefinition';

/**
 * VectorBT Capabilities
 */
const VECTORBT_CAPABILITIES: ProviderCapabilities = {
  backtesting: true,
  optimization: true,
  liveTrading: false,
  research: true,

  multiAsset: ['stocks' as const, 'crypto' as const, 'forex' as const],

  indicators: true,
  customStrategies: true,
  maxConcurrentBacktests: 10, // VectorBT is very fast

  supportedTimeframes: ['tick', 'second', 'minute', 'hour', 'daily'],

  supportedMarkets: ['us-equity', 'crypto', 'forex']
};

/**
 * VectorBT Adapter
 */
export class VectorBTAdapter extends BaseBacktestingAdapter {
  readonly name = 'VectorBT';
  readonly version = '0.25.0';
  readonly capabilities = VECTORBT_CAPABILITIES;
  readonly description = 'Ultra-fast vectorized backtesting with NumPy/Pandas';

  async initialize(config: ProviderConfig): Promise<InitResult> {
    this.config = config;
    return this.createSuccessResult('VectorBT ready');
  }

  async testConnection(): Promise<TestResult> {
    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'test_connection',
        args: JSON.stringify({}),
      });

      return {
        success: true,
        message: 'VectorBT is available',
        latency: 0,
      };
    } catch (error) {
      return {
        success: false,
        message: 'Connection test failed',
        error: String(error),
      };
    }
  }

  async runBacktest(request: BacktestRequest): Promise<BacktestResult> {
    const backtestId = this.generateId();

    try {
      const args: Record<string, any> = {
        id: backtestId,
        strategy: request.strategy,
        startDate: request.startDate,
        endDate: request.endDate,
        initialCapital: request.initialCapital,
        assets: request.assets,
        parameters: request.parameters,
      };

      // Pass advanced execution parameters
      if (request.commission !== undefined) args.commission = request.commission;
      if (request.slippage !== undefined) args.slippage = request.slippage;
      if (request.benchmark) args.benchmark = request.benchmark;
      if (request.stopLoss) args.stopLoss = request.stopLoss;
      if (request.takeProfit) args.takeProfit = request.takeProfit;
      if (request.trailingStop) args.trailingStop = request.trailingStop;
      if (request.positionSizing) args.positionSizing = request.positionSizing;
      if (request.positionSizeValue !== undefined) args.positionSizeValue = request.positionSizeValue;
      if (request.allowShort !== undefined) args.allowShort = request.allowShort;
      if (request.leverage !== undefined && request.leverage > 1) args.leverage = request.leverage;
      if (request.compareRandom) args.compareRandom = true;

      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'run_backtest',
        args: JSON.stringify(args),
      });

      const result = this.extractJSON(stdout);
      if (!result.success) {
        // Include logs from Python in the error for debugging
        const logs = result?.data?.logs || result?.logs || [];
        const logsStr = Array.isArray(logs) && logs.length > 0
          ? `\nLogs: ${logs.slice(-5).join('\n')}`
          : '';
        throw new Error((result.error || 'VectorBT backtest failed') + logsStr);
      }

      return result.data as BacktestResult;
    } catch (error) {
      throw new Error(`VectorBT backtest failed: ${error}`);
    }
  }

  /**
   * Run vectorized parameter optimization
   */
  async optimize(request: OptimizationRequest): Promise<OptimizationResult> {
    try {
      const args: Record<string, any> = {
        strategy: request.strategy,
        parameterRanges: request.parameterGrid,
        objective: request.objective || 'sharpe_ratio',
        startDate: request.startDate || request.config?.startDate,
        endDate: request.endDate || request.config?.endDate,
        initialCapital: request.initialCapital || request.config?.initialCapital || 100000,
        assets: request.assets || request.config?.assets || [],
        parameters: request.parameters,
        maxIterations: request.maxIterations,
        algorithm: request.algorithm || 'grid',
      };

      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'optimize',
        args: JSON.stringify(args),
      });

      const result = this.extractJSON(stdout);
      if (!result.success) {
        throw new Error(result.error || 'VectorBT optimization failed');
      }

      return result.data as OptimizationResult;
    } catch (error) {
      throw new Error(`VectorBT optimization failed: ${error}`);
    }
  }

  /**
   * Run walk-forward optimization
   */
  async walkForward(request: {
    strategy: any;
    parameterRanges: Record<string, { min: number; max: number; step: number }>;
    objective: string;
    nSplits: number;
    trainRatio: number;
    startDate: string;
    endDate: string;
    initialCapital: number;
    assets: any[];
  }): Promise<any> {
    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'walk_forward',
        args: JSON.stringify(request),
      });

      const result = this.extractJSON(stdout);
      if (!result.success) {
        throw new Error(result.error || 'VectorBT walk-forward failed');
      }

      return result.data;
    } catch (error) {
      throw new Error(`VectorBT walk-forward failed: ${error}`);
    }
  }

  /**
   * Extract JSON from stdout that may contain non-JSON lines
   */
  private extractJSON(stdout: string): any {
    // Try parsing directly first
    try {
      return JSON.parse(stdout);
    } catch {
      // Find the last JSON object in the output
      const lines = stdout.split('\n');
      for (let i = lines.length - 1; i >= 0; i--) {
        const line = lines[i].trim();
        if (line.startsWith('{')) {
          try {
            return JSON.parse(line);
          } catch {
            continue;
          }
        }
      }
      throw new Error('No valid JSON found in VectorBT output');
    }
  }

  async getHistoricalData(request: DataRequest): Promise<HistoricalData[]> {
    // VectorBT can use any data source (yfinance, ccxt, etc.)
    return [];
  }

  async getAvailableData(): Promise<DataCatalog> {
    return {
      markets: [
        { id: 'us-equity', name: 'US Equities', assetClasses: ['equity'] },
        { id: 'crypto', name: 'Cryptocurrency', assetClasses: ['crypto'] },
        { id: 'forex', name: 'Forex', assetClasses: ['forex'] },
      ],
      timeframes: ['tick', 'second', 'minute', 'hour', 'daily'],
      dateRange: {
        start: '2000-01-01',
        end: new Date().toISOString().split('T')[0],
      },
      dataSize: 'Flexible (uses yfinance, ccxt, etc.)',
      description: 'VectorBT supports multiple data sources',
    };
  }

  async calculateIndicator(type: IndicatorType, params: IndicatorParams): Promise<IndicatorResult> {
    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'calculate_indicator',
        args: JSON.stringify({ indicator: type, ...params }),
      });
      const result = this.extractJSON(stdout);
      if (!result.success) throw new Error(result.error || 'Indicator calculation failed');
      return result.data as IndicatorResult;
    } catch (error) {
      throw new Error(`Indicator calculation failed: ${error}`);
    }
  }

  async getAvailableIndicators(): Promise<IndicatorCatalog> {
    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'get_indicators',
        args: JSON.stringify({}),
      });
      const result = this.extractJSON(stdout);
      if (result.success && result.data) return result.data as IndicatorCatalog;
    } catch { /* fall through to static catalog */ }

    return {
      categories: [
        {
          name: 'Trend',
          indicators: [
            { id: 'ma', name: 'Moving Average (SMA/EMA)', params: ['period', 'type'] },
            { id: 'bbands', name: 'Bollinger Bands', params: ['period', 'std_dev'] },
            { id: 'keltner', name: 'Keltner Channel', params: ['ema_period', 'atr_period', 'multiplier'] },
            { id: 'donchian', name: 'Donchian Channel', params: ['period'] },
          ],
        },
        {
          name: 'Momentum',
          indicators: [
            { id: 'rsi', name: 'RSI', params: ['period'] },
            { id: 'macd', name: 'MACD', params: ['fast', 'slow', 'signal'] },
            { id: 'stoch', name: 'Stochastic', params: ['k_period', 'd_period'] },
            { id: 'momentum', name: 'Momentum', params: ['period'] },
            { id: 'williams_r', name: 'Williams %R', params: ['period'] },
            { id: 'cci', name: 'CCI', params: ['period'] },
          ],
        },
        {
          name: 'Volatility',
          indicators: [
            { id: 'atr', name: 'ATR', params: ['period'] },
            { id: 'mstd', name: 'Moving Std Dev', params: ['period'] },
            { id: 'zscore', name: 'Z-Score', params: ['period'] },
          ],
        },
        {
          name: 'Volume',
          indicators: [
            { id: 'obv', name: 'On-Balance Volume', params: [] },
            { id: 'vwap', name: 'VWAP', params: [] },
          ],
        },
        {
          name: 'Trend Strength',
          indicators: [
            { id: 'adx', name: 'ADX', params: ['period'] },
          ],
        },
      ],
      total: 16,
      description: 'VectorBT 16 vectorized indicators available',
    };
  }

  async validateStrategy(strategy: StrategyDefinition): Promise<ValidationResult> {
    const errors: string[] = [];
    const warnings: string[] = [];

    if (!strategy.code || strategy.code.language !== 'python') {
      errors.push('VectorBT requires Python strategy code');
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings,
    };
  }

  async getHealth(): Promise<HealthStatus> {
    return { status: 'healthy', message: 'VectorBT operational' };
  }
}
