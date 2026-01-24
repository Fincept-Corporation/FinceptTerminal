/**
 * Fast-Trade Adapter
 *
 * Adapter for Fast-Trade - fast, low-code backtesting library with JSON-based strategies.
 * Utilizes pandas and 80+ FINTA technical analysis indicators.
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
} from '../../interfaces/types';

/**
 * Fast-Trade Capabilities
 */
const FASTTRADE_CAPABILITIES: ProviderCapabilities = {
  backtesting: true,
  optimization: false, // Manual parameter iteration only
  liveTrading: false,
  research: true,

  multiAsset: ['stocks' as const, 'crypto' as const, 'forex' as const, 'futures' as const],

  indicators: true,
  customStrategies: true, // JSON-based logic
  maxConcurrentBacktests: 50, // Very fast library

  supportedTimeframes: ['minute', 'hour', 'daily'],

  supportedMarkets: ['us-equity', 'crypto', 'forex', 'futures']
};

/**
 * Fast-Trade Adapter
 */
export class FastTradeAdapter extends BaseBacktestingAdapter {
  readonly name = 'Fast-Trade';
  readonly version = '0.4.0';
  readonly capabilities = FASTTRADE_CAPABILITIES;
  readonly description = 'Fast, low-code backtesting with JSON-based strategies and 80+ FINTA indicators';


  /**
   * Extract JSON from Python output
   * CRITICAL: Handles log messages mixed with JSON
   * Returns the LAST valid JSON object found
   */
  private extractJSON(stdout: string): any {
    const lines = stdout.split('\n');
    let jsonText = '';
    let inJson = false;
    let braceCount = 0;
    let lastValidJson: any = null;

    for (const line of lines) {
      const trimmed = line.trim();

      // Start of JSON object
      if (trimmed.startsWith('{')) {
        inJson = true;
        braceCount = 0;
        jsonText = '';
      }

      if (inJson) {
        jsonText += line + '\n';

        // Count braces to find end of JSON
        for (const char of line) {
          if (char === '{') braceCount++;
          if (char === '}') braceCount--;
        }

        // Complete JSON object found
        if (braceCount === 0 && jsonText.trim().length > 0) {
          try {
            lastValidJson = JSON.parse(jsonText);
            jsonText = '';
            inJson = false;
          } catch (e) {
            // Continue searching
            jsonText = '';
            inJson = false;
          }
        }
      }
    }

    if (lastValidJson) {
      return lastValidJson;
    }

    throw new Error('No valid JSON found in output: ' + stdout.substring(0, 200));
  }

  async initialize(config: ProviderConfig): Promise<InitResult> {
    this.config = config;
    return this.createSuccessResult('Fast-Trade ready');
  }

  async testConnection(): Promise<TestResult> {
    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'fasttrade',
        command: 'test_connection',
        args: JSON.stringify({}),
      });

      const result = this.extractJSON(stdout);

      if (!result.success) {
        return {
          success: false,
          message: 'Fast-Trade connection test failed',
          error: result.error,
        };
      }

      return {
        success: true,
        message: result.message || 'Fast-Trade is available',
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
    const backtestId = this.generateBacktestId();

    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: 'fasttrade',
        command: 'run_backtest',
        args: JSON.stringify({
          id: backtestId,
          strategy: request.strategy,
          startDate: request.startDate,
          endDate: request.endDate,
          initialCapital: request.initialCapital,
          assets: request.assets,
        }),
      });

      const result = this.extractJSON(stdout);

      if (!result.success) {
        throw new Error(result.error || 'Backtest failed');
      }

      if (!result.data) {
        throw new Error('No backtest data returned');
      }

      return result.data as BacktestResult;
    } catch (error) {
      console.error('[FastTradeAdapter] Backtest error:', error);
      throw error;
    }
  }

  async getHistoricalData(request: DataRequest): Promise<HistoricalData[]> {
    throw new Error('Historical data fetching not supported by Fast-Trade adapter');
  }

  async getAvailableData(): Promise<import('../../interfaces/types').DataCatalog> {
    // Fast-Trade doesn't provide data catalog
    return {
      timeframes: ['minute', 'hour', 'daily'],
      markets: [
        { id: 'us-equity', name: 'US Equity', assetClasses: ['stocks'] },
        { id: 'crypto', name: 'Cryptocurrency', assetClasses: ['crypto'] },
        { id: 'forex', name: 'Foreign Exchange', assetClasses: ['forex'] },
        { id: 'futures', name: 'Futures', assetClasses: ['futures'] },
      ],
    };
  }

  async calculateIndicator(
    type: IndicatorType,
    params: IndicatorParams
  ): Promise<IndicatorResult> {
    throw new Error('Standalone indicator calculation not supported by Fast-Trade adapter');
  }

  async getAvailableIndicators(): Promise<import('../../interfaces/types').IndicatorCatalog> {
    // Return FINTA indicators catalog
    return {
      categories: [
        {
          name: 'Moving Averages',
          indicators: [
            { id: 'sma', name: 'SMA', params: ['period'], description: 'Simple Moving Average' },
            { id: 'ema', name: 'EMA', params: ['period'], description: 'Exponential Moving Average' },
          ],
        },
        {
          name: 'Oscillators',
          indicators: [
            { id: 'rsi', name: 'RSI', params: ['period'], description: 'Relative Strength Index' },
            { id: 'macd', name: 'MACD', params: ['fast', 'slow', 'signal'], description: 'Moving Average Convergence Divergence' },
          ],
        },
        {
          name: 'Volatility',
          indicators: [
            { id: 'bbands', name: 'BBANDS', params: ['period', 'std'], description: 'Bollinger Bands' },
            { id: 'atr', name: 'ATR', params: ['period'], description: 'Average True Range' },
          ],
        },
        {
          name: 'Volume',
          indicators: [
            { id: 'obv', name: 'OBV', params: [], description: 'On Balance Volume' },
            { id: 'mfi', name: 'MFI', params: ['period'], description: 'Money Flow Index' },
          ],
        },
      ],
    };
  }

  async getBacktestStatus(backtestId: string): Promise<BacktestStatus> {
    return {
      id: backtestId,
      status: 'completed',
      progress: 100,
      message: 'Backtest not found or already completed',
    };
  }

  async cancelBacktest(backtestId: string): Promise<void> {
    // Fast-Trade backtests are very fast, cancellation is a no-op
  }

  protected generateBacktestId(): string {
    return `bt-fasttrade-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }
}

export const fastTradeAdapter = new FastTradeAdapter();
