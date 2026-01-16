/**
 * Portfolio-Fortitudo Integration Service
 * Bridges user portfolios from Portfolio Tab to Fortitudo.tech analytics
 *
 * Fetches historical returns for portfolio holdings and formats them
 * for use with portfolio risk metrics, optimization, and efficient frontier.
 */

import { portfolioService, Portfolio, PortfolioSummary, HoldingWithQuote } from '../portfolio/portfolioService';
import { yfinanceService, HistoricalDataPoint } from '../markets/yfinanceService';
import { fortitudoService, ReturnsData, WeightsData, OptimizationResult, EfficientFrontierResponse, FullAnalysisResponse } from './fortitudoService';

// ============================================================================
// TYPES
// ============================================================================

export interface PortfolioReturnsData {
  returns: Record<string, Record<string, number>>; // {symbol: {date: return}}
  weights: Record<string, number>; // {symbol: weight}
  assets: string[];
  nScenarios: number;
  startDate: string;
  endDate: string;
}

export interface PortfolioForAnalysis {
  portfolio: Portfolio;
  summary: PortfolioSummary;
  returnsData: PortfolioReturnsData | null;
  isLoading: boolean;
  error: string | null;
}

export interface PortfolioAnalysisResult {
  success: boolean;
  portfolio: Portfolio;
  analysis?: {
    metrics_equal_weight: any;
    metrics_exp_decay: any;
    covariance_matrix?: Record<string, Record<string, number>>;
    correlation_matrix?: Record<string, Record<string, number>>;
    moments?: Record<string, Record<string, number>>;
    half_life: number;
    alpha: number;
    n_scenarios: number;
    n_assets: number;
    assets: string[];
  };
  error?: string;
}

// ============================================================================
// SERVICE CLASS
// ============================================================================

class PortfolioFortitudoService {
  /**
   * Get all user portfolios available for analysis
   */
  async getAvailablePortfolios(): Promise<Portfolio[]> {
    try {
      return await portfolioService.getPortfolios();
    } catch (error) {
      console.error('[PortfolioFortitudoService] Error fetching portfolios:', error);
      return [];
    }
  }

  /**
   * Get portfolio summary with current holdings and weights
   */
  async getPortfolioSummary(portfolioId: string): Promise<PortfolioSummary | null> {
    try {
      return await portfolioService.getPortfolioSummary(portfolioId);
    } catch (error) {
      console.error('[PortfolioFortitudoService] Error fetching portfolio summary:', error);
      return null;
    }
  }

  /**
   * Fetch historical returns data for portfolio holdings
   * Converts price data to daily returns format required by Fortitudo
   *
   * @param holdings - Array of portfolio holdings with symbols
   * @param days - Number of historical days (default: 252 = 1 year)
   */
  async fetchHistoricalReturns(
    holdings: HoldingWithQuote[],
    days: number = 252
  ): Promise<PortfolioReturnsData | null> {
    try {
      const symbols = holdings.map(h => h.symbol);
      const weights: Record<string, number> = {};

      // Build weights from holdings (normalized to 0-1)
      holdings.forEach(h => {
        weights[h.symbol] = h.weight / 100; // Convert from percentage to decimal
      });

      // Calculate date range
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(startDate.getDate() - days - 30); // Extra buffer for non-trading days

      const formatDate = (date: Date) => date.toISOString().split('T')[0];

      // Fetch historical data for all symbols in parallel
      const historicalPromises = symbols.map(symbol =>
        yfinanceService.getHistoricalData(symbol, formatDate(startDate), formatDate(endDate))
          .then(data => ({ symbol, data }))
          .catch(err => {
            console.warn(`[PortfolioFortitudoService] Failed to fetch data for ${symbol}:`, err);
            return { symbol, data: [] as HistoricalDataPoint[] };
          })
      );

      const historicalResults = await Promise.all(historicalPromises);

      // Convert price data to returns
      const returns: Record<string, Record<string, number>> = {};
      let minLength = Infinity;
      const allDates = new Set<string>();

      historicalResults.forEach(({ symbol, data }) => {
        if (data.length < 2) {
          console.warn(`[PortfolioFortitudoService] Insufficient data for ${symbol}`);
          return;
        }

        returns[symbol] = {};

        // Calculate daily returns: (P_t - P_{t-1}) / P_{t-1}
        for (let i = 1; i < data.length; i++) {
          const prevClose = data[i - 1].adj_close || data[i - 1].close;
          const currClose = data[i].adj_close || data[i].close;

          if (prevClose > 0) {
            const dailyReturn = (currClose - prevClose) / prevClose;
            const dateStr = new Date(data[i].timestamp * 1000).toISOString().split('T')[0];
            returns[symbol][dateStr] = dailyReturn;
            allDates.add(dateStr);
          }
        }

        minLength = Math.min(minLength, Object.keys(returns[symbol]).length);
      });

      // Filter to only symbols with sufficient data
      const validSymbols = Object.keys(returns).filter(s => Object.keys(returns[s]).length >= 20);

      if (validSymbols.length === 0) {
        console.error('[PortfolioFortitudoService] No symbols with sufficient historical data');
        return null;
      }

      // Align returns to common dates
      const sortedDates = Array.from(allDates).sort();
      const alignedReturns: Record<string, Record<string, number>> = {};

      validSymbols.forEach(symbol => {
        alignedReturns[symbol] = {};
        sortedDates.forEach(date => {
          if (returns[symbol][date] !== undefined) {
            alignedReturns[symbol][date] = returns[symbol][date];
          }
        });
      });

      // Normalize weights to only include valid symbols
      const validWeights: Record<string, number> = {};
      let totalWeight = 0;
      validSymbols.forEach(s => {
        if (weights[s]) {
          totalWeight += weights[s];
        }
      });

      validSymbols.forEach(s => {
        validWeights[s] = weights[s] ? weights[s] / totalWeight : 1 / validSymbols.length;
      });

      return {
        returns: alignedReturns,
        weights: validWeights,
        assets: validSymbols,
        nScenarios: Math.min(...validSymbols.map(s => Object.keys(alignedReturns[s]).length)),
        startDate: sortedDates[0],
        endDate: sortedDates[sortedDates.length - 1]
      };
    } catch (error) {
      console.error('[PortfolioFortitudoService] Error fetching historical returns:', error);
      return null;
    }
  }

  /**
   * Run full portfolio analysis using Fortitudo.tech
   *
   * @param portfolioId - Portfolio ID to analyze
   * @param alpha - VaR/CVaR confidence level (default: 0.05 = 95%)
   * @param halfLife - Half-life for exponential decay (default: 252 days)
   * @param days - Historical days to fetch (default: 252)
   */
  async runPortfolioAnalysis(
    portfolioId: string,
    alpha: number = 0.05,
    halfLife: number = 252,
    days: number = 252
  ): Promise<PortfolioAnalysisResult> {
    try {
      // Get portfolio summary
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary) {
        return { success: false, portfolio: null as any, error: 'Portfolio not found' };
      }

      if (summary.holdings.length === 0) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: 'Portfolio has no holdings'
        };
      }

      // Fetch historical returns
      const returnsData = await this.fetchHistoricalReturns(summary.holdings, days);
      if (!returnsData) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: 'Failed to fetch historical returns data'
        };
      }

      // Run Fortitudo analysis
      const result = await fortitudoService.fullAnalysis(
        returnsData.returns,
        Object.values(returnsData.weights),
        alpha,
        halfLife
      );

      if (!result.success || !result.analysis) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: result.error || 'Analysis failed'
        };
      }

      return {
        success: true,
        portfolio: summary.portfolio,
        analysis: {
          ...result.analysis,
          assets: returnsData.assets
        }
      };
    } catch (error) {
      console.error('[PortfolioFortitudoService] Analysis error:', error);
      return {
        success: false,
        portfolio: null as any,
        error: String(error)
      };
    }
  }

  /**
   * Run Mean-Variance portfolio optimization
   */
  async optimizeMeanVariance(
    portfolioId: string,
    objective: 'min_variance' | 'max_sharpe' | 'target_return' = 'min_variance',
    longOnly: boolean = true,
    maxWeight: number = 0.5,
    riskFreeRate: number = 0.05,
    targetReturn?: number,
    days: number = 252
  ): Promise<OptimizationResult> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const returnsData = await this.fetchHistoricalReturns(summary.holdings, days);
      if (!returnsData) {
        return { success: false, error: 'Failed to fetch historical returns' };
      }

      return fortitudoService.optimizeMeanVariance(
        returnsData.returns,
        objective,
        longOnly,
        maxWeight,
        0, // minWeight
        riskFreeRate,
        targetReturn
      );
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Run Mean-CVaR portfolio optimization
   */
  async optimizeMeanCVaR(
    portfolioId: string,
    objective: 'min_cvar' | 'target_return' = 'min_cvar',
    alpha: number = 0.05,
    longOnly: boolean = true,
    maxWeight: number = 0.5,
    riskFreeRate: number = 0.05,
    targetReturn?: number,
    days: number = 252
  ): Promise<OptimizationResult> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const returnsData = await this.fetchHistoricalReturns(summary.holdings, days);
      if (!returnsData) {
        return { success: false, error: 'Failed to fetch historical returns' };
      }

      return fortitudoService.optimizeMeanCVaR(
        returnsData.returns,
        objective,
        alpha,
        longOnly,
        maxWeight,
        0,
        riskFreeRate,
        targetReturn
      );
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Generate efficient frontier for portfolio assets
   */
  async generateEfficientFrontier(
    portfolioId: string,
    type: 'mv' | 'cvar' = 'mv',
    nPoints: number = 20,
    longOnly: boolean = true,
    maxWeight: number = 0.5,
    alpha: number = 0.05,
    riskFreeRate: number = 0.05,
    days: number = 252
  ): Promise<EfficientFrontierResponse> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const returnsData = await this.fetchHistoricalReturns(summary.holdings, days);
      if (!returnsData) {
        return { success: false, error: 'Failed to fetch historical returns' };
      }

      if (type === 'mv') {
        return fortitudoService.efficientFrontierMV(
          returnsData.returns,
          nPoints,
          longOnly,
          maxWeight,
          riskFreeRate
        );
      } else {
        return fortitudoService.efficientFrontierCVaR(
          returnsData.returns,
          nPoints,
          alpha,
          longOnly,
          maxWeight
        );
      }
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Compare current portfolio weights vs optimal weights
   */
  async compareWithOptimal(
    portfolioId: string,
    objective: 'min_variance' | 'max_sharpe' = 'min_variance',
    days: number = 252
  ): Promise<{
    success: boolean;
    current: { weights: Record<string, number>; metrics: any } | null;
    optimal: { weights: Record<string, number>; metrics: any } | null;
    improvement: Record<string, number> | null;
    error?: string;
  }> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, current: null, optimal: null, improvement: null, error: 'Portfolio not found or empty' };
      }

      const returnsData = await this.fetchHistoricalReturns(summary.holdings, days);
      if (!returnsData) {
        return { success: false, current: null, optimal: null, improvement: null, error: 'Failed to fetch historical returns' };
      }

      // Calculate current portfolio metrics
      const currentResult = await fortitudoService.fullAnalysis(
        returnsData.returns,
        Object.values(returnsData.weights),
        0.05,
        252
      );

      // Get optimal weights
      const optimalResult = await fortitudoService.optimizeMeanVariance(
        returnsData.returns,
        objective,
        true,
        0.5,
        0,
        0.05
      );

      if (!currentResult.success || !optimalResult.success) {
        return {
          success: false,
          current: null,
          optimal: null,
          improvement: null,
          error: currentResult.error || optimalResult.error
        };
      }

      // Calculate improvement
      const improvement: Record<string, number> = {};

      if (currentResult.analysis && optimalResult) {
        const currentMetrics = currentResult.analysis.metrics_equal_weight;
        improvement.expected_return = (optimalResult.expected_return || 0) - (currentMetrics?.expected_return || 0);
        improvement.volatility = (currentMetrics?.volatility || 0) - (optimalResult.volatility || 0); // Lower is better
        improvement.sharpe_ratio = (optimalResult.sharpe_ratio || 0) - (currentMetrics?.sharpe_ratio || 0);
      }

      return {
        success: true,
        current: {
          weights: returnsData.weights,
          metrics: currentResult.analysis?.metrics_equal_weight
        },
        optimal: {
          weights: optimalResult.weights as Record<string, number>,
          metrics: {
            expected_return: optimalResult.expected_return,
            volatility: optimalResult.volatility,
            sharpe_ratio: optimalResult.sharpe_ratio
          }
        },
        improvement
      };
    } catch (error) {
      return { success: false, current: null, optimal: null, improvement: null, error: String(error) };
    }
  }

  /**
   * Get portfolio risk contribution by asset
   */
  async getRiskContribution(
    portfolioId: string,
    days: number = 252
  ): Promise<{
    success: boolean;
    contributions?: Record<string, { weight: number; marginal_var: number; component_var: number; pct_contribution: number }>;
    total_var?: number;
    error?: string;
  }> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const returnsData = await this.fetchHistoricalReturns(summary.holdings, days);
      if (!returnsData) {
        return { success: false, error: 'Failed to fetch historical returns' };
      }

      // Get covariance matrix
      const covResult = await fortitudoService.covarianceAnalysis(returnsData.returns);

      if (!covResult.success || !covResult.covariance_matrix) {
        return { success: false, error: 'Failed to calculate covariance matrix' };
      }

      // Calculate risk contributions (simplified Euler decomposition)
      const weights = returnsData.weights;
      const assets = returnsData.assets;
      const cov = covResult.covariance_matrix;

      // Portfolio variance = w' * Cov * w
      let portfolioVar = 0;
      assets.forEach((a1, i) => {
        assets.forEach((a2, j) => {
          portfolioVar += weights[a1] * weights[a2] * (cov[a1]?.[a2] || 0);
        });
      });

      const portfolioVol = Math.sqrt(portfolioVar);

      // Marginal contribution = (Cov * w) / portfolio_vol
      const contributions: Record<string, any> = {};

      assets.forEach(asset => {
        let marginalSum = 0;
        assets.forEach(other => {
          marginalSum += (cov[asset]?.[other] || 0) * weights[other];
        });

        const marginalVar = marginalSum / portfolioVol;
        const componentVar = weights[asset] * marginalVar;

        contributions[asset] = {
          weight: weights[asset],
          marginal_var: marginalVar,
          component_var: componentVar,
          pct_contribution: portfolioVol > 0 ? componentVar / portfolioVol * 100 : 0
        };
      });

      return {
        success: true,
        contributions,
        total_var: portfolioVar
      };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }
}

// Export singleton instance
export const portfolioFortitudoService = new PortfolioFortitudoService();
