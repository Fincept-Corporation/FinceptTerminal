/**
 * Backtesting.py Adapter
 *
 * Adapter for Backtesting.py - lightweight, fast backtesting library.
 * Event-driven bar-by-bar processing with interactive visualizations.
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseBacktestingAdapter } from '../BaseAdapter';
import { backtestingLogger } from '../../../core/loggerService';
import {
  ProviderCapabilities,
  ProviderConfig,
  InitResult,
  TestResult,
  BacktestRequest,
  BacktestResult,
  DataRequest,
  HistoricalData,
  DataCatalog,
  IndicatorType,
  IndicatorParams,
  IndicatorResult,
  IndicatorCatalog,
  OptimizationRequest,
  OptimizationResult,
} from '../../interfaces/types';
import { StrategyDefinition } from '../../interfaces/IStrategyDefinition';

/**
 * Backtesting.py Capabilities
 */
const BACKTESTINGPY_CAPABILITIES: ProviderCapabilities = {
  backtesting: true,
  optimization: true,
  liveTrading: false,
  research: true,

  multiAsset: ['stocks' as const, 'crypto' as const, 'forex' as const, 'futures' as const],

  indicators: true,
  customStrategies: true,
  maxConcurrentBacktests: 20, // Lightweight and fast

  supportedTimeframes: ['tick', 'second', 'minute', 'hour', 'daily'],

  supportedMarkets: ['us-equity', 'crypto', 'forex', 'futures']
};

/**
 * Backtesting.py Adapter
 */
export class BacktestingPyAdapter extends BaseBacktestingAdapter {
  readonly name = 'Backtesting.py';
  readonly version = '0.3.3';
  readonly capabilities = BACKTESTINGPY_CAPABILITIES;
  readonly description = 'Lightweight, fast event-driven backtesting with interactive charts';

  /**
   * Extract JSON from Python output (filters out log messages)
   * Returns the LAST valid JSON object found (in case there are multiple)
   */
  private extractJSON(stdout: string): any {
    // Python script may output log messages before JSON
    // Find lines that start with '{' (JSON objects)
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
        jsonText = ''; // Reset for new JSON object
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
            // Don't return immediately - keep searching for more JSON objects
            jsonText = '';
            inJson = false;
          } catch (e) {
            // Continue searching for valid JSON
            jsonText = '';
            inJson = false;
          }
        }
      }
    }

    if (lastValidJson) {
      return lastValidJson;
    }

    throw new Error('No valid JSON response found in output: ' + stdout.substring(0, 200));
  }

  async initialize(config: ProviderConfig): Promise<InitResult> {
    this.config = config;
    return this.createSuccessResult('Backtesting.py ready');
  }

  async testConnection(): Promise<TestResult> {
    try {
      const stdout = await invoke<string>(
        'execute_python_backtest',
        {
          provider: 'backtestingpy',
          command: 'test_connection',
          args: JSON.stringify({}),
        }
      );

      const result = this.extractJSON(stdout) as { success: boolean; data?: any; error?: string; message?: string };

      if (!result.success) {
        return {
          success: false,
          message: 'Backtesting.py connection test failed',
          error: result.error || 'Failed to import backtesting module',
        };
      }

      return {
        success: true,
        message: result.message || 'Backtesting.py is available',
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
      backtestingLogger.debug('Running backtest:', {
        id: backtestId,
        strategy: request.strategy?.name,
        assets: request.assets?.map(a => a.symbol),
      });

      // Execute Python backtest
      const stdout = await invoke<string>(
        'execute_python_backtest',
        {
          provider: 'backtestingpy',
          command: 'run_backtest',
          args: JSON.stringify({
            id: backtestId,
            strategy: request.strategy,
            startDate: request.startDate,
            endDate: request.endDate,
            initialCapital: request.initialCapital,
            assets: request.assets,
            // Pass advanced parameters
            commission: request.commission,
            tradeOnClose: request.tradeOnClose,
            hedging: request.hedging,
            exclusiveOrders: request.exclusiveOrders,
            margin: request.margin,
            benchmark: request.benchmark,
          }),
        }
      );

      backtestingLogger.debug('Raw Python output length:', stdout.length);
      backtestingLogger.debug('Raw Python output (first 500):', stdout.substring(0, 500));
      backtestingLogger.debug('Raw Python output (last 500):', stdout.substring(Math.max(0, stdout.length - 500)));

      const result = this.extractJSON(stdout) as { success: boolean; data?: any; error?: string };

      backtestingLogger.debug('Python response:', result);
      backtestingLogger.debug('Has data field?', 'data' in result);

      if (!result.success) {
        throw new Error(result.error || 'Backtest execution failed');
      }

      if (!result.data) {
        backtestingLogger.error('No data in result:', result);
        throw new Error('Backtest completed but no data returned');
      }

      const backtestResult = result.data as BacktestResult;

      backtestingLogger.debug('Full result.data:', JSON.stringify(result.data, null, 2));
      backtestingLogger.info('Backtest completed:', {
        id: backtestId,
        totalReturn: backtestResult.performance?.totalReturn,
        sharpeRatio: backtestResult.performance?.sharpeRatio,
        trades: backtestResult.trades?.length,
        performanceKeys: Object.keys(backtestResult.performance || {}),
      });

      return backtestResult;
    } catch (error) {
      backtestingLogger.error('Backtest error:', error);
      throw error;
    }
  }

  async optimize(request: OptimizationRequest): Promise<OptimizationResult> {
    try {
      backtestingLogger.debug('Running optimization:', {
        strategy: request.strategy?.name,
        parameters: Object.keys(request.parameters || {}),
      });

      const stdout = await invoke<string>(
        'execute_python_backtest',
        {
          provider: 'backtestingpy',
          command: 'optimize',
          args: JSON.stringify({
            strategy: request.strategy,
            parameters: request.parameters,
            objective: request.objective,
            startDate: request.startDate,
            endDate: request.endDate,
            initialCapital: request.initialCapital,
            assets: request.assets,
            constraints: request.constraints,
          }),
        }
      );

      const result = this.extractJSON(stdout) as { success: boolean; data?: any; error?: string };

      if (!result.success) {
        throw new Error(result.error || 'Optimization failed');
      }

      const optimizationResult = result.data as OptimizationResult;

      backtestingLogger.info('Optimization completed:', {
        bestParams: optimizationResult.bestParameters,
        iterations: optimizationResult.iterations,
      });

      return optimizationResult;
    } catch (error) {
      backtestingLogger.error('Optimization error:', error);
      throw error;
    }
  }

  async getHistoricalData(request: DataRequest): Promise<HistoricalData[]> {
    try {
      // Backtesting.py doesn't provide data fetching
      // This would need to be implemented using external data providers
      backtestingLogger.warn('Data fetching not implemented');
      return [];
    } catch (error) {
      backtestingLogger.error('Data fetch error:', error);
      throw error;
    }
  }

  async getAvailableData(): Promise<DataCatalog> {
    return {
      providers: [],
      timeframes: ['daily', 'minute', 'second'],
    };
  }

  async calculateIndicator(
    type: IndicatorType,
    params: IndicatorParams
  ): Promise<IndicatorResult> {
    try {
      // Indicators can be calculated, but backtesting.py uses external libraries
      backtestingLogger.warn('Indicator calculation not directly supported');

      return {
        indicator: type,
        symbol: '',
        values: [],
      };
    } catch (error) {
      backtestingLogger.error('Indicator calculation error:', error);
      throw error;
    }
  }

  async getAvailableIndicators(): Promise<IndicatorCatalog> {
    return {
      categories: [
        {
          name: 'trend',
          indicators: [
            { id: 'sma', name: 'SMA', params: ['period'], description: 'Simple Moving Average' },
            { id: 'ema', name: 'EMA', params: ['period'], description: 'Exponential Moving Average' }
          ]
        },
        {
          name: 'momentum',
          indicators: [
            { id: 'rsi', name: 'RSI', params: ['period'], description: 'Relative Strength Index' }
          ]
        }
      ]
    };
  }

  async validateStrategy(strategy: StrategyDefinition): Promise<{
    valid: boolean;
    errors: string[];
    warnings: string[];
  }> {
    try {
      // Basic validation
      const errors: string[] = [];
      const warnings: string[] = [];

      if (!strategy.name || strategy.name.trim() === '') {
        errors.push('Strategy name is required');
      }

      if (!strategy.code && strategy.type !== 'visual') {
        warnings.push('No strategy code provided, using default SMA crossover');
      }

      return {
        valid: errors.length === 0,
        errors,
        warnings,
      };
    } catch (error) {
      return {
        valid: false,
        errors: [String(error)],
        warnings: [],
      };
    }
  }

  // Utility method to generate unique IDs
  protected generateBacktestId(): string {
    return `bt-py-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }
}

// Export singleton instance
export const backtestingPyAdapter = new BacktestingPyAdapter();
