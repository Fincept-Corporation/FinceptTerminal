/**
 * Fortitudo.tech Analytics Service - Frontend API for portfolio risk analytics
 * Provides VaR, CVaR, volatility, Sharpe ratio, option pricing, and entropy pooling
 */

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

export interface FortitudoStatusResponse {
  success: boolean;
  available: boolean;
  wrappers_available: boolean;
  version?: string;
  message?: string;
  error?: string;
}

export interface PortfolioMetrics {
  expected_return: number;
  volatility: number;
  var: number;
  cvar: number;
  sharpe_ratio: number;
  alpha: number;
}

export interface PortfolioMetricsResponse {
  success: boolean;
  metrics?: PortfolioMetrics;
  n_scenarios?: number;
  n_assets?: number;
  error?: string;
}

export interface CovarianceAnalysisResponse {
  success: boolean;
  covariance_matrix?: Record<string, Record<string, number>>;
  correlation_matrix?: Record<string, Record<string, number>>;
  moments?: Record<string, Record<string, number>>;
  assets?: string[];
  error?: string;
}

export interface ExpDecayResponse {
  success: boolean;
  probabilities?: number[];
  half_life?: number;
  n_scenarios?: number;
  sum?: number;
  error?: string;
}

export interface OptionPricingResult {
  call_price: number;
  put_price: number;
  straddle_price: number;
  forward_price: number;
  strike: number;
  volatility: number;
  risk_free_rate: number;
  time_to_maturity: number;
}

export interface ParityCheck {
  call_price: number;
  put_price: number;
  forward_price: number;
  parity_lhs: number;
  parity_rhs: number;
  parity_difference: number;
  parity_holds: boolean;
}

export interface OptionPricingResponse {
  success: boolean;
  forward_price?: number;
  call_price?: number;
  put_price?: number;
  straddle?: OptionPricingResult;
  parity_check?: ParityCheck;
  error?: string;
}

export interface OptionChainItem {
  strike: number;
  call_price: number;
  put_price: number;
  moneyness: number;
}

export interface OptionChainResponse {
  success: boolean;
  spot_price?: number;
  forward_price?: number;
  chain?: OptionChainItem[];
  error?: string;
}

export interface EntropyPoolingResponse {
  success: boolean;
  prior_probabilities?: number[];
  posterior_probabilities?: number[];
  effective_scenarios_prior?: number;
  effective_scenarios_posterior?: number;
  max_probability?: number;
  min_probability?: number;
  error?: string;
}

export interface ExposureStackingResponse {
  success: boolean;
  stacked_weights?: number[];
  mean_sample_weights?: number[];
  std_sample_weights?: number[];
  n_assets?: number;
  n_samples?: number;
  n_partitions?: number;
  weights_sum?: number;
  error?: string;
}

export interface OptionGreeks {
  delta: number;
  gamma: number;
  vega: number;
  theta: number;
  rho: number;
}

export interface OptionGreeksResponse {
  success: boolean;
  call?: OptionGreeks;
  put?: OptionGreeks;
  inputs?: {
    spot_price: number;
    strike: number;
    volatility: number;
    risk_free_rate: number;
    dividend_yield: number;
    time_to_maturity: number;
  };
  error?: string;
}

export interface FullAnalysisResponse {
  success: boolean;
  analysis?: {
    metrics_equal_weight: PortfolioMetrics;
    metrics_exp_decay: PortfolioMetrics;
    covariance_matrix?: Record<string, Record<string, number>>;
    correlation_matrix?: Record<string, Record<string, number>>;
    moments?: Record<string, Record<string, number>>;
    half_life: number;
    alpha: number;
    n_scenarios: number;
    n_assets: number;
    assets?: string[];
  };
  error?: string;
}

// Portfolio Optimization Types
export type OptimizationObjective = 'min_variance' | 'max_sharpe' | 'target_return' | 'min_cvar';

export interface OptimizationResult {
  success: boolean;
  weights?: number[] | Record<string, number>;
  expected_return?: number;
  volatility?: number;
  sharpe_ratio?: number;
  var?: number;
  cvar?: number;
  assets?: string[];
  objective?: string;
  error?: string;
}

export interface EfficientFrontierPoint {
  expected_return: number;
  volatility?: number;
  cvar?: number;
  sharpe_ratio?: number;
  weights: number[] | Record<string, number>;
}

export interface EfficientFrontierResponse {
  success: boolean;
  frontier?: EfficientFrontierPoint[];
  assets?: string[];
  n_points?: number;
  error?: string;
}

// Returns data can be provided in multiple formats
export type ReturnsData =
  | Record<string, number>                           // {date: return}
  | Record<string, Record<string, number>>           // {symbol: {date: return}}
  | Array<{ date: string; returns: number; symbol?: string }>; // Array format

// Weights can be array or object
export type WeightsData = number[] | Record<string, number>;

// ============================================================================
// FORTITUDO SERVICE CLASS
// ============================================================================

class FortitudoService {
  /**
   * Check fortitudo.tech library status and availability
   */
  async checkStatus(): Promise<FortitudoStatusResponse> {
    try {
      const result = await invoke<string>('fortitudo_check_status');
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] checkStatus error:', error);
      return {
        success: false,
        available: false,
        wrappers_available: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate comprehensive portfolio risk metrics
   * @param returns - Historical returns data
   * @param weights - Portfolio weights
   * @param alpha - Confidence level for VaR/CVaR (default: 0.05 = 95%)
   * @param probabilities - Optional scenario probabilities
   */
  async portfolioMetrics(
    returns: ReturnsData,
    weights: WeightsData,
    alpha?: number,
    probabilities?: number[]
  ): Promise<PortfolioMetricsResponse> {
    try {
      const result = await invoke<string>('fortitudo_portfolio_metrics', {
        returnsJson: JSON.stringify(returns),
        weightsJson: JSON.stringify(weights),
        alpha,
        probabilities: probabilities ? JSON.stringify(probabilities) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] portfolioMetrics error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate covariance and correlation matrices with statistical moments
   * @param returns - Historical returns data
   * @param probabilities - Optional scenario probabilities
   */
  async covarianceAnalysis(
    returns: ReturnsData,
    probabilities?: number[]
  ): Promise<CovarianceAnalysisResponse> {
    try {
      const result = await invoke<string>('fortitudo_covariance_analysis', {
        returnsJson: JSON.stringify(returns),
        probabilities: probabilities ? JSON.stringify(probabilities) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] covarianceAnalysis error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate exponentially decaying probabilities for scenario weighting
   * @param returns - Historical returns data
   * @param halfLife - Half-life in days (default: 252 = 1 year)
   */
  async expDecayWeighting(
    returns: ReturnsData,
    halfLife?: number
  ): Promise<ExpDecayResponse> {
    try {
      const result = await invoke<string>('fortitudo_exp_decay_weighting', {
        returnsJson: JSON.stringify(returns),
        halfLife
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] expDecayWeighting error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Price options using Black-Scholes model
   * @param spotPrice - Current spot price
   * @param strike - Strike price
   * @param volatility - Implied volatility (annual, e.g., 0.25 for 25%)
   * @param riskFreeRate - Risk-free rate (annual)
   * @param dividendYield - Dividend yield (annual)
   * @param timeToMaturity - Time to maturity in years
   */
  async optionPricing(
    spotPrice: number,
    strike: number,
    volatility: number,
    riskFreeRate?: number,
    dividendYield?: number,
    timeToMaturity?: number
  ): Promise<OptionPricingResponse> {
    try {
      const result = await invoke<string>('fortitudo_option_pricing', {
        spotPrice,
        strike,
        volatility,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] optionPricing error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Generate option chain for multiple strikes
   * @param spotPrice - Current spot price
   * @param volatility - Implied volatility
   * @param strikes - Array of strike prices (optional, auto-generated if not provided)
   * @param riskFreeRate - Risk-free rate
   * @param dividendYield - Dividend yield
   * @param timeToMaturity - Time to maturity in years
   */
  async optionChain(
    spotPrice: number,
    volatility: number,
    strikes?: number[],
    riskFreeRate?: number,
    dividendYield?: number,
    timeToMaturity?: number
  ): Promise<OptionChainResponse> {
    try {
      const result = await invoke<string>('fortitudo_option_chain', {
        spotPrice,
        volatility,
        strikes,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] optionChain error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Apply entropy pooling for scenario probability adjustment
   * @param nScenarios - Number of scenarios
   * @param maxProbability - Maximum probability for any scenario (e.g., 0.05 for 5%)
   */
  async entropyPooling(
    nScenarios: number,
    maxProbability?: number
  ): Promise<EntropyPoolingResponse> {
    try {
      const result = await invoke<string>('fortitudo_entropy_pooling', {
        nScenarios,
        maxProbability
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] entropyPooling error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate exposure stacking portfolio from sample portfolios
   * @param samplePortfolios - Sample portfolio weights matrix (n_assets x n_samples)
   * @param nPartitions - Number of partition sets
   */
  async exposureStacking(
    samplePortfolios: number[][],
    nPartitions?: number
  ): Promise<ExposureStackingResponse> {
    try {
      const result = await invoke<string>('fortitudo_exposure_stacking', {
        samplePortfoliosJson: JSON.stringify(samplePortfolios),
        nPartitions
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] exposureStacking error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Full portfolio risk analysis combining all metrics
   * @param returns - Historical returns data
   * @param weights - Portfolio weights
   * @param alpha - Confidence level for VaR/CVaR
   * @param halfLife - Half-life for exponential decay
   */
  async fullAnalysis(
    returns: ReturnsData,
    weights: WeightsData,
    alpha?: number,
    halfLife?: number
  ): Promise<FullAnalysisResponse> {
    try {
      const result = await invoke<string>('fortitudo_full_analysis', {
        returnsJson: JSON.stringify(returns),
        weightsJson: JSON.stringify(weights),
        alpha,
        halfLife
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] fullAnalysis error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate option Greeks (Delta, Gamma, Vega, Theta, Rho)
   * @param spotPrice - Current spot price
   * @param strike - Strike price
   * @param volatility - Implied volatility (annual, e.g., 0.25 for 25%)
   * @param riskFreeRate - Risk-free rate (annual)
   * @param dividendYield - Dividend yield (annual)
   * @param timeToMaturity - Time to maturity in years
   */
  async optionGreeks(
    spotPrice: number,
    strike: number,
    volatility: number,
    riskFreeRate?: number,
    dividendYield?: number,
    timeToMaturity?: number
  ): Promise<OptionGreeksResponse> {
    try {
      const result = await invoke<string>('fortitudo_option_greeks', {
        spotPrice,
        strike,
        volatility,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] optionGreeks error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  // ============================================================================
  // PORTFOLIO OPTIMIZATION METHODS
  // ============================================================================

  /**
   * Mean-Variance portfolio optimization
   * @param returns - Historical returns data
   * @param objective - Optimization objective: 'min_variance', 'max_sharpe', 'target_return'
   * @param longOnly - Constraint for long-only portfolios (default: true)
   * @param maxWeight - Maximum weight per asset
   * @param minWeight - Minimum weight per asset
   * @param riskFreeRate - Risk-free rate for Sharpe calculation
   * @param targetReturn - Target return for 'target_return' objective
   */
  async optimizeMeanVariance(
    returns: ReturnsData,
    objective?: OptimizationObjective,
    longOnly?: boolean,
    maxWeight?: number,
    minWeight?: number,
    riskFreeRate?: number,
    targetReturn?: number
  ): Promise<OptimizationResult> {
    try {
      const result = await invoke<string>('fortitudo_optimize_mean_variance', {
        returnsJson: JSON.stringify(returns),
        objective,
        longOnly,
        maxWeight,
        minWeight,
        riskFreeRate,
        targetReturn
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] optimizeMeanVariance error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Mean-CVaR portfolio optimization (Conditional Value-at-Risk)
   * @param returns - Historical returns data
   * @param objective - Optimization objective: 'min_cvar', 'target_return'
   * @param alpha - Confidence level for CVaR (default: 0.05 = 95%)
   * @param longOnly - Constraint for long-only portfolios (default: true)
   * @param maxWeight - Maximum weight per asset
   * @param minWeight - Minimum weight per asset
   * @param riskFreeRate - Risk-free rate
   * @param targetReturn - Target return for 'target_return' objective
   */
  async optimizeMeanCVaR(
    returns: ReturnsData,
    objective?: OptimizationObjective,
    alpha?: number,
    longOnly?: boolean,
    maxWeight?: number,
    minWeight?: number,
    riskFreeRate?: number,
    targetReturn?: number
  ): Promise<OptimizationResult> {
    try {
      const result = await invoke<string>('fortitudo_optimize_mean_cvar', {
        returnsJson: JSON.stringify(returns),
        objective,
        alpha,
        longOnly,
        maxWeight,
        minWeight,
        riskFreeRate,
        targetReturn
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] optimizeMeanCVaR error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Generate Mean-Variance efficient frontier
   * @param returns - Historical returns data
   * @param nPoints - Number of points on the frontier (default: 20)
   * @param longOnly - Constraint for long-only portfolios
   * @param maxWeight - Maximum weight per asset
   * @param riskFreeRate - Risk-free rate
   */
  async efficientFrontierMV(
    returns: ReturnsData,
    nPoints?: number,
    longOnly?: boolean,
    maxWeight?: number,
    riskFreeRate?: number
  ): Promise<EfficientFrontierResponse> {
    try {
      const result = await invoke<string>('fortitudo_efficient_frontier_mv', {
        returnsJson: JSON.stringify(returns),
        nPoints,
        longOnly,
        maxWeight,
        riskFreeRate
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] efficientFrontierMV error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Generate Mean-CVaR efficient frontier
   * @param returns - Historical returns data
   * @param nPoints - Number of points on the frontier (default: 20)
   * @param alpha - Confidence level for CVaR
   * @param longOnly - Constraint for long-only portfolios
   * @param maxWeight - Maximum weight per asset
   */
  async efficientFrontierCVaR(
    returns: ReturnsData,
    nPoints?: number,
    alpha?: number,
    longOnly?: boolean,
    maxWeight?: number
  ): Promise<EfficientFrontierResponse> {
    try {
      const result = await invoke<string>('fortitudo_efficient_frontier_cvar', {
        returnsJson: JSON.stringify(returns),
        nPoints,
        alpha,
        longOnly,
        maxWeight
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FortitudoService] efficientFrontierCVaR error:', error);
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
   * Format percentage for display
   */
  formatPercent(value: number | null | undefined, decimals = 2): string {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return `${(value * 100).toFixed(decimals)}%`;
  }

  /**
   * Format ratio for display
   */
  formatRatio(value: number | null | undefined, decimals = 3): string {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(decimals);
  }

  /**
   * Format currency for display
   */
  formatCurrency(value: number | null | undefined, decimals = 2): string {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return `$${value.toFixed(decimals)}`;
  }

  /**
   * Generate sample returns data for testing
   */
  generateSampleReturns(
    nScenarios = 200,
    nAssets = 4,
    assetNames = ['Stocks', 'Bonds', 'Commodities', 'Cash']
  ): Record<string, Record<string, number>> {
    const returns: Record<string, Record<string, number>> = {};
    const expectedReturns = [0.08, 0.04, 0.06, 0.02];

    for (let i = 0; i < nAssets; i++) {
      const assetName = assetNames[i] || `Asset${i + 1}`;
      returns[assetName] = {};

      for (let j = 0; j < nScenarios; j++) {
        const date = new Date(2024, 0, j + 1).toISOString().split('T')[0];
        // Random return around expected value
        const dailyExpected = expectedReturns[i] / 252;
        const dailyVol = (i === 0 ? 0.02 : i === 1 ? 0.01 : 0.015);
        returns[assetName][date] = dailyExpected + (Math.random() - 0.5) * dailyVol * 2;
      }
    }

    return returns;
  }
}

// Export singleton instance
export const fortitudoService = new FortitudoService();
