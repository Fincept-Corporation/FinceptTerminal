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
  BacktestStatus,
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

  private activeBacktests: Map<string, BacktestStatus> = new Map();

  async initialize(config: ProviderConfig): Promise<InitResult> {
    try {
      this.ensureInitialized();

      // Test Python and VectorBT availability
      const result = await invoke<{ success: boolean; stdout: string }>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'test_import',
        args: JSON.stringify({}),
      });

      if (!result.success) {
        return this.createErrorResult('VectorBT not installed. Run: pip install vectorbt');
      }

      this.config = config;
      this.initialized = true;

      return this.createSuccessResult('VectorBT initialized successfully');
    } catch (error) {
      return this.createErrorResult(error instanceof Error ? error : String(error));
    }
  }

  async testConnection(): Promise<TestResult> {
    this.ensureInitialized();

    try {
      const result = await invoke<{ success: boolean; stdout: string }>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'test_connection',
        args: JSON.stringify({}),
      });

      if (!result.success) {
        return {
          success: false,
          message: 'VectorBT connection test failed',
          error: 'Failed to import vectorbt module',
        };
      }

      this.connected = true;

      const response = JSON.parse(result.stdout);
      return {
        success: true,
        message: `VectorBT ${response.version} is available`,
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

  async disconnect(): Promise<void> {
    this.activeBacktests.clear();
    this.connected = false;
    this.initialized = false;
  }

  async runBacktest(request: BacktestRequest): Promise<BacktestResult> {
    this.ensureConnected();

    const backtestId = this.generateId();

    try {
      // Track as active
      this.activeBacktests.set(backtestId, {
        id: backtestId,
        status: 'running',
        progress: 0,
        message: 'Running VectorBT backtest...',
      });

      // Execute Python backtest
      const result = await invoke<{ success: boolean; stdout: string }>('execute_python_backtest', {
        provider: 'vectorbt',
        command: 'run_backtest',
        args: JSON.stringify({
          id: backtestId,
          strategy: request.strategy,
          startDate: request.startDate,
          endDate: request.endDate,
          initialCapital: request.initialCapital,
          assets: request.assets,
          parameters: request.parameters,
        }),
      });

      this.activeBacktests.delete(backtestId);

      if (!result.success) {
        throw new Error('VectorBT backtest failed');
      }

      const backtestResult = JSON.parse(result.stdout);
      return backtestResult as BacktestResult;
    } catch (error) {
      this.activeBacktests.delete(backtestId);
      throw new Error(`VectorBT backtest failed: ${error}`);
    }
  }

  async getBacktestStatus(id: string): Promise<BacktestStatus> {
    const status = this.activeBacktests.get(id);
    if (!status) {
      return {
        id,
        status: 'completed',
        progress: 100,
        message: 'Backtest not found or already completed',
      };
    }
    return status;
  }

  async cancelBacktest(id: string): Promise<void> {
    this.activeBacktests.delete(id);
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
    throw new Error('Use VectorBT indicators within backtests');
  }

  async getAvailableIndicators(): Promise<IndicatorCatalog> {
    return {
      categories: [
        {
          name: 'Trend',
          indicators: [
            { id: 'sma', name: 'Simple Moving Average', params: ['window'] },
            { id: 'ema', name: 'Exponential Moving Average', params: ['span'] },
            { id: 'bb', name: 'Bollinger Bands', params: ['window', 'alpha'] },
          ],
        },
        {
          name: 'Momentum',
          indicators: [
            { id: 'rsi', name: 'RSI', params: ['window'] },
            { id: 'macd', name: 'MACD', params: ['fast', 'slow', 'signal'] },
          ],
        },
        {
          name: 'Volatility',
          indicators: [
            { id: 'atr', name: 'ATR', params: ['window'] },
            { id: 'stdev', name: 'Standard Deviation', params: ['window'] },
          ],
        },
      ],
      total: 50,
      description: 'VectorBT has 50+ vectorized indicators',
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
    if (!this.initialized) {
      return { status: 'unhealthy', message: 'Not initialized' };
    }

    if (!this.connected) {
      return { status: 'degraded', message: 'Not connected' };
    }

    return { status: 'healthy', message: 'VectorBT operational' };
  }
}
