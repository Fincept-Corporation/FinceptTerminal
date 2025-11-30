/**
 * Base Adapter for Backtesting Providers
 * Abstract base class that provides common functionality for all adapters
 */

import { IBacktestingProvider } from '../interfaces/IBacktestingProvider';
import { StrategyDefinition } from '../interfaces/IStrategyDefinition';
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
} from '../interfaces/types';

/**
 * BaseBacktestingAdapter
 *
 * Base class that all provider adapters should extend.
 * Provides common functionality and helper methods.
 */
export abstract class BaseBacktestingAdapter implements IBacktestingProvider {
  // ============================================================================
  // Abstract Properties (must be implemented by subclasses)
  // ============================================================================

  abstract readonly name: string;
  abstract readonly version: string;
  abstract readonly capabilities: ProviderCapabilities;
  abstract readonly description?: string;

  // ============================================================================
  // Protected State
  // ============================================================================

  protected config: ProviderConfig | null = null;
  protected initialized: boolean = false;
  protected connected: boolean = false;

  // ============================================================================
  // Constructor
  // ============================================================================

  constructor() {
    // Base constructor - subclasses can extend if needed
  }

  // ============================================================================
  // Abstract Methods (must be implemented by subclasses)
  // ============================================================================

  abstract initialize(config: ProviderConfig): Promise<InitResult>;
  abstract testConnection(): Promise<TestResult>;
  abstract runBacktest(request: BacktestRequest): Promise<BacktestResult>;
  abstract getHistoricalData(request: DataRequest): Promise<HistoricalData[]>;
  abstract getAvailableData(): Promise<DataCatalog>;
  abstract calculateIndicator(
    type: IndicatorType,
    params: IndicatorParams
  ): Promise<IndicatorResult>;
  abstract getAvailableIndicators(): Promise<IndicatorCatalog>;

  // ============================================================================
  // Connection Management (default implementation)
  // ============================================================================

  async disconnect(): Promise<void> {
    this.connected = false;
    this.initialized = false;
    console.log(`Disconnected from ${this.name}`);
  }

  // ============================================================================
  // Backtest Status (default implementation - override if needed)
  // ============================================================================

  async getBacktestStatus(id: string): Promise<BacktestStatus> {
    // Default implementation - subclasses should override with actual status checking
    return {
      id,
      status: 'running',
      progress: 0,
    };
  }

  async cancelBacktest(id: string): Promise<void> {
    // Default implementation - subclasses should override
    console.warn(`Cancel backtest not implemented for ${this.name}`);
  }

  // ============================================================================
  // Optional Features (default implementations)
  // ============================================================================

  async optimize(request: OptimizationRequest): Promise<OptimizationResult> {
    throw new Error(`Optimization not supported by ${this.name}`);
  }

  async getOptimizationStatus(id: string): Promise<BacktestStatus> {
    throw new Error(`Optimization not supported by ${this.name}`);
  }

  async cancelOptimization(id: string): Promise<void> {
    throw new Error(`Optimization not supported by ${this.name}`);
  }

  async deployLive(request: LiveDeployRequest): Promise<LiveDeployResult> {
    throw new Error(`Live trading not supported by ${this.name}`);
  }

  async getLiveStatus(id: string): Promise<LiveStatus> {
    throw new Error(`Live trading not supported by ${this.name}`);
  }

  async stopLive(id: string): Promise<void> {
    throw new Error(`Live trading not supported by ${this.name}`);
  }

  async pauseLive(id: string): Promise<void> {
    throw new Error(`Live trading pause not supported by ${this.name}`);
  }

  async resumeLive(id: string): Promise<void> {
    throw new Error(`Live trading resume not supported by ${this.name}`);
  }

  async createNotebook(
    name: string,
    template?: string
  ): Promise<NotebookResult> {
    throw new Error(`Notebooks not supported by ${this.name}`);
  }

  async getNotebookTemplates(): Promise<string[]> {
    throw new Error(`Notebooks not supported by ${this.name}`);
  }

  async validateStrategy(strategy: StrategyDefinition): Promise<{
    valid: boolean;
    errors: string[];
    warnings?: string[];
  }> {
    // Default basic validation
    const errors: string[] = [];
    const warnings: string[] = [];

    if (!strategy.name) {
      errors.push('Strategy name is required');
    }

    if (!strategy.type) {
      errors.push('Strategy type is required');
    }

    if (strategy.type === 'code' && !strategy.code) {
      errors.push('Code strategy requires code definition');
    }

    if (strategy.type === 'visual' && !strategy.visual) {
      errors.push('Visual strategy requires visual definition');
    }

    if (strategy.type === 'template' && !strategy.template) {
      errors.push('Template strategy requires template definition');
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings: warnings.length > 0 ? warnings : undefined,
    };
  }

  async getExampleStrategies(): Promise<StrategyDefinition[]> {
    // Default: no examples
    return [];
  }

  async getHealth(): Promise<{
    status: 'healthy' | 'degraded' | 'unhealthy';
    message?: string;
    details?: Record<string, any>;
  }> {
    // Default health check
    try {
      if (!this.initialized) {
        return {
          status: 'unhealthy',
          message: 'Provider not initialized',
        };
      }

      const testResult = await this.testConnection();

      return {
        status: testResult.success ? 'healthy' : 'unhealthy',
        message: testResult.message,
        details: {
          connected: this.connected,
          initialized: this.initialized,
        },
      };
    } catch (error) {
      return {
        status: 'unhealthy',
        message: error instanceof Error ? error.message : 'Unknown error',
      };
    }
  }

  async getUsageStats(): Promise<{
    totalBacktests: number;
    totalOptimizations: number;
    apiCallsRemaining?: number;
    quotaReset?: string;
  }> {
    // Default: no stats
    return {
      totalBacktests: 0,
      totalOptimizations: 0,
    };
  }

  // ============================================================================
  // Protected Helper Methods
  // ============================================================================

  /**
   * Validate configuration has required fields
   */
  protected validateConfig(
    config: ProviderConfig,
    requiredFields: string[]
  ): { valid: boolean; errors: string[] } {
    const errors: string[] = [];

    for (const field of requiredFields) {
      if (!(field in config) || !config[field as keyof ProviderConfig]) {
        errors.push(`Missing required configuration field: ${field}`);
      }
    }

    return {
      valid: errors.length === 0,
      errors,
    };
  }

  /**
   * Create success result
   */
  protected createSuccessResult(message: string): InitResult {
    return {
      success: true,
      message,
    };
  }

  /**
   * Create error result
   */
  protected createErrorResult(error: string | Error): InitResult {
    return {
      success: false,
      message: 'Initialization failed',
      error: error instanceof Error ? error.message : error,
    };
  }

  /**
   * Create test success result
   */
  protected createTestSuccessResult(
    message: string,
    metadata?: Record<string, any>
  ): TestResult {
    return {
      success: true,
      message,
      metadata,
    };
  }

  /**
   * Create test error result
   */
  protected createTestErrorResult(error: string | Error): TestResult {
    return {
      success: false,
      message: 'Connection test failed',
      error: error instanceof Error ? error.message : error,
    };
  }

  /**
   * Generate unique ID
   */
  protected generateId(): string {
    return `${this.name.toLowerCase().replace(/\s+/g, '-')}-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Log info message
   */
  protected log(message: string): void {
    console.log(`[${this.name}] ${message}`);
  }

  /**
   * Log warning message
   */
  protected warn(message: string): void {
    console.warn(`[${this.name}] ${message}`);
  }

  /**
   * Log error message
   */
  protected error(message: string, error?: Error): void {
    console.error(`[${this.name}] ${message}`, error || '');
  }

  /**
   * Check if provider is initialized
   */
  protected ensureInitialized(): void {
    if (!this.initialized) {
      throw new Error(`${this.name} is not initialized. Call initialize() first.`);
    }
  }

  /**
   * Check if provider is connected
   */
  protected ensureConnected(): void {
    this.ensureInitialized();
    if (!this.connected) {
      throw new Error(
        `${this.name} is not connected. Call testConnection() first.`
      );
    }
  }

  /**
   * Safe async operation with error handling
   */
  protected async safeAsync<T>(
    operation: () => Promise<T>,
    errorMessage: string
  ): Promise<T> {
    try {
      return await operation();
    } catch (error) {
      this.error(errorMessage, error as Error);
      throw error;
    }
  }

  /**
   * Parse date string to Date object
   */
  protected parseDate(dateStr: string): Date {
    const date = new Date(dateStr);
    if (isNaN(date.getTime())) {
      throw new Error(`Invalid date: ${dateStr}`);
    }
    return date;
  }

  /**
   * Format date to YYYY-MM-DD string
   */
  protected formatDate(date: Date): string {
    return date.toISOString().split('T')[0];
  }

  /**
   * Calculate date range in days
   */
  protected calculateDateRange(startDate: string, endDate: string): number {
    const start = this.parseDate(startDate);
    const end = this.parseDate(endDate);
    return Math.floor((end.getTime() - start.getTime()) / (1000 * 60 * 60 * 24));
  }

  /**
   * Validate date range
   */
  protected validateDateRange(
    startDate: string,
    endDate: string
  ): { valid: boolean; error?: string } {
    try {
      const start = this.parseDate(startDate);
      const end = this.parseDate(endDate);

      if (start >= end) {
        return {
          valid: false,
          error: 'Start date must be before end date',
        };
      }

      if (end > new Date()) {
        return {
          valid: false,
          error: 'End date cannot be in the future',
        };
      }

      return { valid: true };
    } catch (error) {
      return {
        valid: false,
        error: error instanceof Error ? error.message : 'Invalid date range',
      };
    }
  }
}
