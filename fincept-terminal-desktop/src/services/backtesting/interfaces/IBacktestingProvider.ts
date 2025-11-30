/**
 * Backtesting Provider Interface
 * Core interface that ALL backtesting engines must implement
 * This ensures platform independence - terminal works with any provider
 */

import { StrategyDefinition } from './IStrategyDefinition';
import {
  ProviderCapabilities,
  ProviderConfig,
  InitResult,
  TestResult,
  BacktestRequest,
  BacktestResult,
  BacktestStatus,
  OptimizationRequest,
  OptimizationResult,
  DataRequest,
  HistoricalData,
  DataCatalog,
  IndicatorType,
  IndicatorParams,
  IndicatorResult,
  IndicatorCatalog,
  LiveDeployRequest,
  LiveDeployResult,
  LiveStatus,
  NotebookResult,
} from './types';

/**
 * IBacktestingProvider - Provider-agnostic interface
 *
 * All backtesting engines (Lean, Backtrader, VectorBT, QuantLib, custom)
 * must implement this interface to work with Fincept Terminal.
 *
 * This is the ONLY interface the terminal core uses for backtesting.
 * No provider-specific code should exist outside of adapter implementations.
 */
export interface IBacktestingProvider {
  // ============================================================================
  // Provider Metadata (readonly properties)
  // ============================================================================

  /**
   * Provider name (e.g., "QuantConnect Lean", "Backtrader", "VectorBT")
   */
  readonly name: string;

  /**
   * Provider version
   */
  readonly version: string;

  /**
   * Provider capabilities - what features this provider supports
   */
  readonly capabilities: ProviderCapabilities;

  /**
   * Provider description
   */
  readonly description?: string;

  // ============================================================================
  // Connection & Initialization
  // ============================================================================

  /**
   * Initialize the provider with configuration
   * Called when provider is first registered or when config changes
   *
   * @param config Provider-specific configuration
   * @returns Result indicating success or failure
   */
  initialize(config: ProviderConfig): Promise<InitResult>;

  /**
   * Test connection to provider
   * Used to validate configuration and check provider availability
   *
   * @returns Result with connection status and metadata
   */
  testConnection(): Promise<TestResult>;

  /**
   * Disconnect from provider
   * Clean up resources, close connections
   */
  disconnect(): Promise<void>;

  // ============================================================================
  // Backtesting (Core Feature)
  // ============================================================================

  /**
   * Run a backtest with the given strategy and configuration
   * This is the core functionality that all providers MUST implement
   *
   * @param request Backtest request with strategy, dates, capital, assets
   * @returns Backtest result with performance metrics, trades, equity curve
   */
  runBacktest(request: BacktestRequest): Promise<BacktestResult>;

  /**
   * Get status of a running backtest
   *
   * @param id Backtest ID
   * @returns Current status and progress
   */
  getBacktestStatus(id: string): Promise<BacktestStatus>;

  /**
   * Cancel a running backtest
   *
   * @param id Backtest ID
   */
  cancelBacktest(id: string): Promise<void>;

  // ============================================================================
  // Optimization (Optional but Recommended)
  // ============================================================================

  /**
   * Run parameter optimization
   * Tests multiple parameter combinations to find optimal values
   *
   * @param request Optimization request with parameter grid and objective
   * @returns Optimization result with best parameters and all iterations
   */
  optimize?(request: OptimizationRequest): Promise<OptimizationResult>;

  /**
   * Get status of running optimization
   *
   * @param id Optimization ID
   * @returns Current status and progress
   */
  getOptimizationStatus?(id: string): Promise<BacktestStatus>;

  /**
   * Cancel running optimization
   *
   * @param id Optimization ID
   */
  cancelOptimization?(id: string): Promise<void>;

  // ============================================================================
  // Data Access
  // ============================================================================

  /**
   * Get historical price data
   *
   * @param request Data request with symbols, dates, timeframe
   * @returns Historical data for requested symbols
   */
  getHistoricalData(request: DataRequest): Promise<HistoricalData[]>;

  /**
   * Get catalog of available data
   * Lists what data this provider can access
   *
   * @returns Data catalog with providers, asset classes, date ranges
   */
  getAvailableData(): Promise<DataCatalog>;

  // ============================================================================
  // Indicators
  // ============================================================================

  /**
   * Calculate technical indicator
   *
   * @param type Indicator type (sma, ema, rsi, etc.)
   * @param params Indicator parameters (period, etc.)
   * @returns Indicator values
   */
  calculateIndicator(
    type: IndicatorType,
    params: IndicatorParams
  ): Promise<IndicatorResult>;

  /**
   * Get list of available indicators
   *
   * @returns Catalog of supported indicators with parameters
   */
  getAvailableIndicators(): Promise<IndicatorCatalog>;

  // ============================================================================
  // Live Trading (Optional)
  // ============================================================================

  /**
   * Deploy strategy to live trading
   *
   * @param request Live deployment request
   * @returns Deployment result with ID and status
   */
  deployLive?(request: LiveDeployRequest): Promise<LiveDeployResult>;

  /**
   * Get status of live trading deployment
   *
   * @param id Deployment ID
   * @returns Current live status with equity, positions, trades
   */
  getLiveStatus?(id: string): Promise<LiveStatus>;

  /**
   * Stop live trading
   *
   * @param id Deployment ID
   */
  stopLive?(id: string): Promise<void>;

  /**
   * Pause live trading (if supported)
   *
   * @param id Deployment ID
   */
  pauseLive?(id: string): Promise<void>;

  /**
   * Resume paused live trading
   *
   * @param id Deployment ID
   */
  resumeLive?(id: string): Promise<void>;

  // ============================================================================
  // Research/Notebooks (Optional)
  // ============================================================================

  /**
   * Create research notebook
   * For providers that support Jupyter/notebook environments
   *
   * @param name Notebook name
   * @param template Optional template to use
   * @returns Notebook result with URL
   */
  createNotebook?(name: string, template?: string): Promise<NotebookResult>;

  /**
   * Get list of available notebook templates
   *
   * @returns Array of template names
   */
  getNotebookTemplates?(): Promise<string[]>;

  // ============================================================================
  // Strategy Management (Optional)
  // ============================================================================

  /**
   * Validate strategy before running
   * Checks if strategy is compatible with this provider
   *
   * @param strategy Strategy definition
   * @returns Validation result with errors/warnings
   */
  validateStrategy?(strategy: StrategyDefinition): Promise<{
    valid: boolean;
    errors: string[];
    warnings?: string[];
  }>;

  /**
   * Get example strategies for this provider
   *
   * @returns Array of example strategy definitions
   */
  getExampleStrategies?(): Promise<StrategyDefinition[]>;

  // ============================================================================
  // Utility Methods (Optional)
  // ============================================================================

  /**
   * Get provider-specific health status
   *
   * @returns Health status information
   */
  getHealth?(): Promise<{
    status: 'healthy' | 'degraded' | 'unhealthy';
    message?: string;
    details?: Record<string, any>;
  }>;

  /**
   * Get provider usage statistics
   *
   * @returns Usage stats (API calls, backtests run, etc.)
   */
  getUsageStats?(): Promise<{
    totalBacktests: number;
    totalOptimizations: number;
    apiCallsRemaining?: number;
    quotaReset?: string;
  }>;
}

/**
 * Type guard to check if optimization is supported
 */
export function supportsOptimization(
  provider: IBacktestingProvider
): provider is IBacktestingProvider & {
  optimize: NonNullable<IBacktestingProvider['optimize']>;
} {
  return (
    provider.capabilities.optimization &&
    typeof provider.optimize === 'function'
  );
}

/**
 * Type guard to check if live trading is supported
 */
export function supportsLiveTrading(
  provider: IBacktestingProvider
): provider is IBacktestingProvider & {
  deployLive: NonNullable<IBacktestingProvider['deployLive']>;
  getLiveStatus: NonNullable<IBacktestingProvider['getLiveStatus']>;
  stopLive: NonNullable<IBacktestingProvider['stopLive']>;
} {
  return (
    provider.capabilities.liveTrading &&
    typeof provider.deployLive === 'function'
  );
}

/**
 * Type guard to check if research/notebooks is supported
 */
export function supportsResearch(
  provider: IBacktestingProvider
): provider is IBacktestingProvider & {
  createNotebook: NonNullable<IBacktestingProvider['createNotebook']>;
} {
  return (
    provider.capabilities.research &&
    typeof provider.createNotebook === 'function'
  );
}
