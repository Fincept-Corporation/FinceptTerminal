/**
 * FFN Analytics Service - Frontend API for FFN portfolio performance analysis
 * Provides comprehensive performance statistics, risk metrics, and drawdown analysis
 */

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

export interface FFNConfig {
  risk_free_rate?: number;
  annualization_factor?: number;
  rebase_value?: number;
  log_returns?: boolean;
  drawdown_threshold?: number;
}

export interface FFNStatusResponse {
  success: boolean;
  available: boolean;
  version?: string;
  message?: string;
  error?: string;
}

export interface PerformanceMetrics {
  total_return: number;
  cagr: number;
  sharpe_ratio: number;
  sortino_ratio: number;
  max_drawdown: number;
  calmar_ratio: number;
  volatility: number;
  daily_mean: number;
  daily_vol: number;
  best_day: number;
  worst_day: number;
  mtd?: number;
  ytd?: number;
}

export interface PerformanceResponse {
  success: boolean;
  metrics?: PerformanceMetrics | Record<string, PerformanceMetrics>;
  data_points?: number;
  date_range?: {
    start: string;
    end: string;
  };
  error?: string;
}

export interface DrawdownInfo {
  start: string;
  end: string;
  length?: number;
  drawdown: number;
}

export interface DrawdownResponse {
  success: boolean;
  max_drawdown?: number;
  current_drawdown?: number;
  drawdowns?: DrawdownInfo[];
  drawdown_series?: Record<string, number>;
  error?: string;
}

export interface RollingMetricsResponse {
  success: boolean;
  window?: number;
  metrics?: {
    rolling_sharpe?: Record<string, number>;
    rolling_volatility?: Record<string, number>;
    rolling_returns?: Record<string, number>;
  };
  error?: string;
}

export interface MonthlyReturnsResponse {
  success: boolean;
  monthly_returns?: Record<string, Record<string, number | null>>;
  years?: string[];
  error?: string;
}

export interface RiskMetricsResponse {
  success: boolean;
  ulcer_index?: number;
  ulcer_performance_index?: number;
  skewness?: number;
  kurtosis?: number;
  var_95?: number;
  cvar_95?: number;
  max_drawdown?: number;
  daily_vol?: number;
  annual_vol?: number;
  error?: string;
}

export interface AssetComparisonResponse {
  success: boolean;
  asset_stats?: Record<string, PerformanceMetrics>;
  correlation_matrix?: Record<string, Record<string, number>>;
  rebased_performance?: Record<string, Record<string, number>>;
  benchmark?: string;
  error?: string;
}

export interface FullAnalysisResponse {
  success: boolean;
  performance?: PerformanceMetrics | Record<string, PerformanceMetrics>;
  drawdowns?: {
    max_drawdown: number;
    top_drawdowns: DrawdownInfo[];
  };
  monthly_returns?: Record<string, Record<string, number | null>>;
  rolling_sharpe?: Record<string, number>;
  data_summary?: {
    data_points: number;
    start_date: string;
    end_date: string;
    assets: string[];
  };
  error?: string;
}

// Price data can be provided in multiple formats
export type PriceData =
  | Record<string, number>                           // {date: price}
  | Record<string, Record<string, number>>           // {symbol: {date: price}}
  | Array<{ date: string; price: number; symbol?: string }>; // Array format

// ============================================================================
// FFN SERVICE CLASS
// ============================================================================

class FFNService {
  /**
   * Check FFN library status and availability
   */
  async checkStatus(): Promise<FFNStatusResponse> {
    try {
      const result = await invoke<string>('ffn_check_status');
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] checkStatus error:', error);
      return {
        success: false,
        available: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate comprehensive performance statistics
   * @param prices - Price data (object with dates as keys, or array)
   * @param config - Optional FFN configuration
   */
  async calculatePerformance(
    prices: PriceData,
    config?: FFNConfig
  ): Promise<PerformanceResponse> {
    try {
      const result = await invoke<string>('ffn_calculate_performance', {
        pricesJson: JSON.stringify(prices),
        config: config ? JSON.stringify(config) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] calculatePerformance error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate drawdown analysis with detailed breakdown
   * @param prices - Price data
   * @param threshold - Minimum drawdown to report (default: 0.10 = 10%)
   */
  async calculateDrawdowns(
    prices: PriceData,
    threshold?: number
  ): Promise<DrawdownResponse> {
    try {
      const result = await invoke<string>('ffn_calculate_drawdowns', {
        pricesJson: JSON.stringify(prices),
        threshold
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] calculateDrawdowns error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate rolling performance metrics
   * @param prices - Price data
   * @param window - Rolling window in days (default: 252 = 1 year)
   * @param metrics - Which metrics to calculate ['sharpe', 'volatility', 'returns']
   */
  async calculateRollingMetrics(
    prices: PriceData,
    window?: number,
    metrics?: string[]
  ): Promise<RollingMetricsResponse> {
    try {
      const result = await invoke<string>('ffn_calculate_rolling_metrics', {
        pricesJson: JSON.stringify(prices),
        window,
        metrics
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] calculateRollingMetrics error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate monthly returns table
   * @param prices - Price data
   */
  async monthlyReturns(prices: PriceData): Promise<MonthlyReturnsResponse> {
    try {
      const result = await invoke<string>('ffn_monthly_returns', {
        pricesJson: JSON.stringify(prices)
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] monthlyReturns error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Rebase prices to a starting value
   * @param prices - Price data
   * @param baseValue - Starting value (default: 100)
   */
  async rebasePrices(
    prices: PriceData,
    baseValue?: number
  ): Promise<{ success: boolean; base_value?: number; rebased_prices?: Record<string, number>; error?: string }> {
    try {
      const result = await invoke<string>('ffn_rebase_prices', {
        pricesJson: JSON.stringify(prices),
        baseValue
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] rebasePrices error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Compare multiple assets performance
   * @param prices - Price data with multiple assets
   * @param benchmark - Optional benchmark symbol
   * @param riskFreeRate - Risk-free rate for calculations
   */
  async compareAssets(
    prices: PriceData,
    benchmark?: string,
    riskFreeRate?: number
  ): Promise<AssetComparisonResponse> {
    try {
      const result = await invoke<string>('ffn_compare_assets', {
        pricesJson: JSON.stringify(prices),
        benchmark,
        riskFreeRate
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] compareAssets error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate comprehensive risk metrics
   * @param prices - Price data
   * @param riskFreeRate - Risk-free rate
   */
  async riskMetrics(
    prices: PriceData,
    riskFreeRate?: number
  ): Promise<RiskMetricsResponse> {
    try {
      const result = await invoke<string>('ffn_risk_metrics', {
        pricesJson: JSON.stringify(prices),
        riskFreeRate
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] riskMetrics error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Full portfolio analysis - combines all metrics
   * @param prices - Price data
   * @param config - Optional FFN configuration
   */
  async fullAnalysis(
    prices: PriceData,
    config?: FFNConfig
  ): Promise<FullAnalysisResponse> {
    try {
      const result = await invoke<string>('ffn_full_analysis', {
        pricesJson: JSON.stringify(prices),
        config: config ? JSON.stringify(config) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FFNService] fullAnalysis error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  // ============================================================================
  // UTILITY METHODS
  // ============================================================================

  /**
   * Convert array of OHLCV data to price format
   */
  ohlcvToPrices(
    data: Array<{ date: string; close: number }>,
    symbol?: string
  ): PriceData {
    if (symbol) {
      return data.map(d => ({ date: d.date, price: d.close, symbol }));
    }
    return data.reduce((acc, d) => {
      acc[d.date] = d.close;
      return acc;
    }, {} as Record<string, number>);
  }

  /**
   * Format percentage for display
   */
  formatPercent(value: number | null | undefined, decimals = 2): string {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return `${(value * 100).toFixed(decimals)}%`;
  }

  /**
   * Format ratio for display
   */
  formatRatio(value: number | null | undefined, decimals = 2): string {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(decimals);
  }
}

// Export singleton instance
export const ffnService = new FFNService();
