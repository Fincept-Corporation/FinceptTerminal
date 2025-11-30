/**
 * Backtesting Service
 * High-level facade for backtesting operations
 * Simplifies interaction with the provider registry
 */

import { backtestingRegistry } from './BacktestingProviderRegistry';
import { IBacktestingProvider } from './interfaces/IBacktestingProvider';
import { StrategyDefinition } from './interfaces/IStrategyDefinition';
import {
  BacktestRequest,
  BacktestResult,
  OptimizationRequest,
  OptimizationResult,
  DataRequest,
  HistoricalData,
  IndicatorType,
  IndicatorParams,
  IndicatorResult,
} from './interfaces/types';

/**
 * BacktestingService
 *
 * Provides a convenient API for backtesting operations.
 * Automatically uses the active provider from the registry.
 */
export class BacktestingService {
  /**
   * Get active provider
   * @throws Error if no provider is active
   */
  private getActiveProvider(): IBacktestingProvider {
    const provider = backtestingRegistry.getActiveProvider();
    if (!provider) {
      throw new Error(
        'No backtesting provider active. Please activate a provider in Settings.'
      );
    }
    return provider;
  }

  /**
   * Run a backtest using the active provider
   */
  async runBacktest(request: BacktestRequest): Promise<BacktestResult> {
    const provider = this.getActiveProvider();
    return provider.runBacktest(request);
  }

  /**
   * Run optimization using the active provider
   */
  async optimize(request: OptimizationRequest): Promise<OptimizationResult> {
    const provider = this.getActiveProvider();

    if (!provider.optimize) {
      throw new Error(
        `Active provider "${provider.name}" does not support optimization`
      );
    }

    return provider.optimize(request);
  }

  /**
   * Get historical data using the active provider
   */
  async getHistoricalData(
    request: DataRequest
  ): Promise<HistoricalData[]> {
    const provider = this.getActiveProvider();
    return provider.getHistoricalData(request);
  }

  /**
   * Calculate indicator using the active provider
   */
  async calculateIndicator(
    type: IndicatorType,
    params: IndicatorParams
  ): Promise<IndicatorResult> {
    const provider = this.getActiveProvider();
    return provider.calculateIndicator(type, params);
  }

  /**
   * Validate strategy using the active provider
   */
  async validateStrategy(strategy: StrategyDefinition): Promise<{
    valid: boolean;
    errors: string[];
    warnings: string[];
  }> {
    const provider = this.getActiveProvider();

    if (provider.validateStrategy) {
      const result = await provider.validateStrategy(strategy);
      return {
        ...result,
        warnings: result.warnings || []
      };
    }

    // Default basic validation
    const errors: string[] = [];
    const warnings: string[] = [];

    if (!strategy.name) errors.push('Strategy name is required');
    if (!strategy.type) errors.push('Strategy type is required');

    return { valid: errors.length === 0, errors, warnings };
  }

  /**
   * Get available data catalog from active provider
   */
  async getAvailableData() {
    const provider = this.getActiveProvider();
    return provider.getAvailableData();
  }

  /**
   * Get available indicators from active provider
   */
  async getAvailableIndicators() {
    const provider = this.getActiveProvider();
    return provider.getAvailableIndicators();
  }

  /**
   * Check if optimization is supported by active provider
   */
  isOptimizationSupported(): boolean {
    const provider = backtestingRegistry.getActiveProvider();
    return provider?.capabilities.optimization || false;
  }

  /**
   * Check if live trading is supported by active provider
   */
  isLiveTradingSupported(): boolean {
    const provider = backtestingRegistry.getActiveProvider();
    return provider?.capabilities.liveTrading || false;
  }

  /**
   * Check if research/notebooks is supported by active provider
   */
  isResearchSupported(): boolean {
    const provider = backtestingRegistry.getActiveProvider();
    return provider?.capabilities.research || false;
  }

  /**
   * Get active provider info
   */
  getActiveProviderInfo() {
    const provider = backtestingRegistry.getActiveProvider();
    if (!provider) return null;

    return {
      name: provider.name,
      version: provider.version,
      capabilities: provider.capabilities,
      description: provider.description,
    };
  }

  /**
   * Get health status of active provider
   */
  async getProviderHealth() {
    const provider = this.getActiveProvider();
    if (provider.getHealth) {
      return provider.getHealth();
    }
    return {
      status: 'healthy' as const,
      message: 'Health check not supported',
    };
  }
}

/**
 * Singleton instance export
 */
export const backtestingService = new BacktestingService();
