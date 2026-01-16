/**
 * Portfolio-Functime Integration Service
 * Bridges user portfolios from Portfolio Tab to Functime time series analytics
 *
 * Fetches historical price data for portfolio holdings and formats them
 * for use with forecasting, anomaly detection, seasonality analysis, etc.
 */

import { portfolioService, Portfolio, PortfolioSummary, HoldingWithQuote } from '../portfolio/portfolioService';
import { yfinanceService, HistoricalDataPoint } from '../markets/yfinanceService';
import {
  functimeService,
  PanelData,
  ForecastResult,
  ForecastConfig,
  AdvancedForecastConfig,
  AnomalyDetectionConfig,
  SeasonalityConfig,
  ConfidenceIntervalsConfig,
  BacktestConfig,
  ForecastResponse,
  AdvancedForecastResponse,
  AnomalyDetectionResponse,
  SeasonalityResponse,
  ConfidenceIntervalsResponse,
  BacktestResponse,
  AutoEnsembleResponse
} from './functimeService';

// ============================================================================
// TYPES
// ============================================================================

export interface PortfolioPriceData {
  panelData: Array<{ entity_id: string; time: string; value: number }>;
  assets: string[];
  nDataPoints: number;
  startDate: string;
  endDate: string;
}

export interface PortfolioForForecasting {
  portfolio: Portfolio;
  summary: PortfolioSummary;
  priceData: PortfolioPriceData | null;
  isLoading: boolean;
  error: string | null;
}

export interface PortfolioForecastResult {
  success: boolean;
  portfolio: Portfolio;
  forecasts?: Record<string, ForecastResult[]>; // {symbol: forecasts}
  metrics?: Record<string, {
    mae?: number;
    rmse?: number;
    smape?: number;
  }>;
  error?: string;
}

export interface PortfolioAnomalyResult {
  success: boolean;
  portfolio: Portfolio;
  anomalies?: Record<string, {
    count: number;
    rate: number;
    dates: string[];
    values: number[];
  }>;
  error?: string;
}

export interface PortfolioSeasonalityResult {
  success: boolean;
  portfolio: Portfolio;
  seasonality?: Record<string, {
    period: number;
    strength: number;
    is_seasonal: boolean;
    trend_strength?: number;
  }>;
  error?: string;
}

// ============================================================================
// SERVICE CLASS
// ============================================================================

class PortfolioFunctimeService {
  /**
   * Get all user portfolios available for analysis
   */
  async getAvailablePortfolios(): Promise<Portfolio[]> {
    try {
      return await portfolioService.getPortfolios();
    } catch (error) {
      console.error('[PortfolioFunctimeService] Error fetching portfolios:', error);
      return [];
    }
  }

  /**
   * Get portfolio summary with current holdings
   */
  async getPortfolioSummary(portfolioId: string): Promise<PortfolioSummary | null> {
    try {
      return await portfolioService.getPortfolioSummary(portfolioId);
    } catch (error) {
      console.error('[PortfolioFunctimeService] Error fetching portfolio summary:', error);
      return null;
    }
  }

  /**
   * Fetch historical price data for portfolio holdings in panel data format
   * This format is required by functime for multi-entity time series analysis
   *
   * @param holdings - Array of portfolio holdings with symbols
   * @param days - Number of historical days (default: 365)
   */
  async fetchHistoricalPrices(
    holdings: HoldingWithQuote[],
    days: number = 365
  ): Promise<PortfolioPriceData | null> {
    try {
      const symbols = holdings.map(h => h.symbol);

      // Calculate date range
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(startDate.getDate() - days - 30); // Extra buffer

      const formatDate = (date: Date) => date.toISOString().split('T')[0];

      // Fetch historical data for all symbols in parallel
      const historicalPromises = symbols.map(symbol =>
        yfinanceService.getHistoricalData(symbol, formatDate(startDate), formatDate(endDate))
          .then(data => ({ symbol, data }))
          .catch(err => {
            console.warn(`[PortfolioFunctimeService] Failed to fetch data for ${symbol}:`, err);
            return { symbol, data: [] as HistoricalDataPoint[] };
          })
      );

      const historicalResults = await Promise.all(historicalPromises);

      // Convert to panel data format
      const panelData: Array<{ entity_id: string; time: string; value: number }> = [];
      const validSymbols: string[] = [];
      const allDates = new Set<string>();

      historicalResults.forEach(({ symbol, data }) => {
        if (data.length < 10) {
          console.warn(`[PortfolioFunctimeService] Insufficient data for ${symbol}`);
          return;
        }

        validSymbols.push(symbol);

        data.forEach(point => {
          const dateStr = new Date(point.timestamp * 1000).toISOString().split('T')[0];
          const price = point.adj_close || point.close;

          panelData.push({
            entity_id: symbol,
            time: dateStr,
            value: price
          });

          allDates.add(dateStr);
        });
      });

      if (validSymbols.length === 0) {
        console.error('[PortfolioFunctimeService] No symbols with sufficient historical data');
        return null;
      }

      // Sort panel data by entity and time
      panelData.sort((a, b) => {
        if (a.entity_id !== b.entity_id) {
          return a.entity_id.localeCompare(b.entity_id);
        }
        return a.time.localeCompare(b.time);
      });

      const sortedDates = Array.from(allDates).sort();

      return {
        panelData,
        assets: validSymbols,
        nDataPoints: panelData.length,
        startDate: sortedDates[0],
        endDate: sortedDates[sortedDates.length - 1]
      };
    } catch (error) {
      console.error('[PortfolioFunctimeService] Error fetching historical prices:', error);
      return null;
    }
  }

  /**
   * Run forecasting for all portfolio holdings
   *
   * @param portfolioId - Portfolio ID to forecast
   * @param model - Forecast model to use
   * @param horizon - Forecast horizon in days
   * @param frequency - Data frequency
   * @param days - Historical days to fetch
   */
  async forecastPortfolio(
    portfolioId: string,
    model: string = 'linear',
    horizon: number = 30,
    frequency: string = '1d',
    days: number = 365
  ): Promise<PortfolioForecastResult> {
    try {
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

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: 'Failed to fetch historical price data'
        };
      }

      // Run forecast using functime
      const result = await functimeService.fullPipeline(
        priceData.panelData,
        model,
        horizon,
        frequency,
        Math.min(horizon, 30) // Test size
      );

      if (!result.success) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: result.error
        };
      }

      // Group forecasts by symbol
      const forecastsBySymbol: Record<string, ForecastResult[]> = {};
      result.forecast?.forEach(fc => {
        if (!forecastsBySymbol[fc.entity_id]) {
          forecastsBySymbol[fc.entity_id] = [];
        }
        forecastsBySymbol[fc.entity_id].push(fc);
      });

      return {
        success: true,
        portfolio: summary.portfolio,
        forecasts: forecastsBySymbol,
        metrics: result.evaluation_metrics ? { overall: result.evaluation_metrics } : undefined
      };
    } catch (error) {
      console.error('[PortfolioFunctimeService] Forecast error:', error);
      return { success: false, portfolio: null as any, error: String(error) };
    }
  }

  /**
   * Run advanced forecasting models on portfolio holdings
   */
  async advancedForecastPortfolio(
    portfolioId: string,
    config: AdvancedForecastConfig,
    days: number = 365
  ): Promise<PortfolioForecastResult> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, portfolio: summary?.portfolio || null as any, error: 'Portfolio not found or empty' };
      }

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: 'Failed to fetch historical price data'
        };
      }

      const result = await functimeService.advancedForecast(priceData.panelData, config);

      if (!result.success) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: result.error
        };
      }

      // Group forecasts by symbol
      const forecastsBySymbol: Record<string, ForecastResult[]> = {};
      result.forecast?.forEach(fc => {
        if (!forecastsBySymbol[fc.entity_id]) {
          forecastsBySymbol[fc.entity_id] = [];
        }
        forecastsBySymbol[fc.entity_id].push(fc);
      });

      return {
        success: true,
        portfolio: summary.portfolio,
        forecasts: forecastsBySymbol
      };
    } catch (error) {
      return { success: false, portfolio: null as any, error: String(error) };
    }
  }

  /**
   * Run auto ensemble forecasting on portfolio
   */
  async autoEnsemblePortfolio(
    portfolioId: string,
    horizon: number = 30,
    frequency: string = '1d',
    nBest: number = 3,
    days: number = 365
  ): Promise<AutoEnsembleResponse & { portfolio?: Portfolio }> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return { success: false, error: 'Failed to fetch historical price data' };
      }

      const result = await functimeService.autoEnsemble(priceData.panelData, horizon, frequency, nBest);
      return { ...result, portfolio: summary.portfolio };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Detect anomalies in portfolio holdings' price movements
   */
  async detectPortfolioAnomalies(
    portfolioId: string,
    config: AnomalyDetectionConfig = { method: 'zscore', threshold: 3.0 },
    days: number = 365
  ): Promise<PortfolioAnomalyResult> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, portfolio: summary?.portfolio || null as any, error: 'Portfolio not found or empty' };
      }

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: 'Failed to fetch historical price data'
        };
      }

      const result = await functimeService.detectAnomalies(priceData.panelData, config);

      if (!result.success || !result.results) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: result.error || 'Anomaly detection failed'
        };
      }

      // Format results by symbol
      const anomalies: Record<string, any> = {};
      Object.entries(result.results).forEach(([symbol, data]) => {
        const anomalyList = data.anomalies?.filter(a => a.is_anomaly) || [];
        anomalies[symbol] = {
          count: data.n_anomalies,
          rate: data.anomaly_rate,
          dates: anomalyList.map(a => a.time),
          values: anomalyList.map(a => a.value)
        };
      });

      return {
        success: true,
        portfolio: summary.portfolio,
        anomalies
      };
    } catch (error) {
      return { success: false, portfolio: null as any, error: String(error) };
    }
  }

  /**
   * Analyze seasonality patterns in portfolio holdings
   */
  async analyzePortfolioSeasonality(
    portfolioId: string,
    days: number = 730 // 2 years for better seasonality detection
  ): Promise<PortfolioSeasonalityResult> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, portfolio: summary?.portfolio || null as any, error: 'Portfolio not found or empty' };
      }

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: 'Failed to fetch historical price data'
        };
      }

      // Get seasonality metrics
      const result = await functimeService.analyzeSeasonality(priceData.panelData, {
        operation: 'metrics'
      });

      if (!result.success) {
        return {
          success: false,
          portfolio: summary.portfolio,
          error: result.error || 'Seasonality analysis failed'
        };
      }

      // Format results by symbol
      const seasonality: Record<string, any> = {};
      if (result.metrics) {
        Object.entries(result.metrics).forEach(([symbol, data]) => {
          seasonality[symbol] = {
            period: data.detected_period || 0,
            strength: data.seasonal_strength,
            is_seasonal: data.is_seasonal,
            trend_strength: data.trend_strength
          };
        });
      }

      return {
        success: true,
        portfolio: summary.portfolio,
        seasonality
      };
    } catch (error) {
      return { success: false, portfolio: null as any, error: String(error) };
    }
  }

  /**
   * Generate prediction intervals for portfolio forecasts
   */
  async generatePortfolioIntervals(
    portfolioId: string,
    config: ConfidenceIntervalsConfig,
    days: number = 365
  ): Promise<ConfidenceIntervalsResponse & { portfolio?: Portfolio }> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return { success: false, error: 'Failed to fetch historical price data' };
      }

      const result = await functimeService.generateIntervals(priceData.panelData, config);
      return { ...result, portfolio: summary.portfolio };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Run backtesting on portfolio holdings
   */
  async backtestPortfolio(
    portfolioId: string,
    config: BacktestConfig,
    days: number = 500
  ): Promise<BacktestResponse & { portfolio?: Portfolio }> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      const priceData = await this.fetchHistoricalPrices(summary.holdings, days);
      if (!priceData) {
        return { success: false, error: 'Failed to fetch historical price data' };
      }

      const result = await functimeService.backtest(priceData.panelData, config);
      return { ...result, portfolio: summary.portfolio };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Get portfolio-wide forecast summary with all holdings
   */
  async getPortfolioForecastSummary(
    portfolioId: string,
    horizon: number = 30,
    days: number = 365
  ): Promise<{
    success: boolean;
    portfolio?: Portfolio;
    holdings?: Array<{
      symbol: string;
      currentPrice: number;
      forecastedPrice: number;
      change: number;
      changePct: number;
    }>;
    portfolioForecast?: {
      currentValue: number;
      forecastedValue: number;
      change: number;
      changePct: number;
    };
    error?: string;
  }> {
    try {
      const summary = await this.getPortfolioSummary(portfolioId);
      if (!summary || summary.holdings.length === 0) {
        return { success: false, error: 'Portfolio not found or empty' };
      }

      // Forecast each holding
      const forecastResult = await this.forecastPortfolio(
        portfolioId,
        'auto_ridge', // Use auto-tuned model
        horizon,
        '1d',
        days
      );

      if (!forecastResult.success || !forecastResult.forecasts) {
        return { success: false, portfolio: summary.portfolio, error: forecastResult.error };
      }

      const holdingsSummary: Array<any> = [];
      let currentPortfolioValue = 0;
      let forecastedPortfolioValue = 0;

      summary.holdings.forEach(holding => {
        const forecasts = forecastResult.forecasts?.[holding.symbol];
        const lastForecast = forecasts?.[forecasts.length - 1];

        const currentPrice = holding.current_price || 0;
        const forecastedPrice = lastForecast?.value || currentPrice;
        const change = forecastedPrice - currentPrice;
        const changePct = currentPrice > 0 ? (change / currentPrice) * 100 : 0;

        holdingsSummary.push({
          symbol: holding.symbol,
          currentPrice,
          forecastedPrice,
          change,
          changePct
        });

        currentPortfolioValue += currentPrice * holding.quantity;
        forecastedPortfolioValue += forecastedPrice * holding.quantity;
      });

      const portfolioChange = forecastedPortfolioValue - currentPortfolioValue;
      const portfolioChangePct = currentPortfolioValue > 0
        ? (portfolioChange / currentPortfolioValue) * 100
        : 0;

      return {
        success: true,
        portfolio: summary.portfolio,
        holdings: holdingsSummary,
        portfolioForecast: {
          currentValue: currentPortfolioValue,
          forecastedValue: forecastedPortfolioValue,
          change: portfolioChange,
          changePct: portfolioChangePct
        }
      };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  }

  /**
   * Fetch data for a single symbol (not portfolio-based)
   */
  async fetchSymbolData(
    symbol: string,
    days: number = 365
  ): Promise<Array<{ entity_id: string; time: string; value: number }> | null> {
    try {
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(startDate.getDate() - days);

      const formatDate = (date: Date) => date.toISOString().split('T')[0];

      const data = await yfinanceService.getHistoricalData(
        symbol,
        formatDate(startDate),
        formatDate(endDate)
      );

      if (!data || data.length === 0) {
        return null;
      }

      return data.map(point => ({
        entity_id: symbol,
        time: new Date(point.timestamp * 1000).toISOString().split('T')[0],
        value: point.adj_close || point.close
      }));
    } catch (error) {
      console.error(`[PortfolioFunctimeService] Error fetching symbol data for ${symbol}:`, error);
      return null;
    }
  }

  /**
   * Forecast a single symbol
   */
  async forecastSymbol(
    symbol: string,
    model: string = 'linear',
    horizon: number = 30,
    frequency: string = '1d',
    days: number = 365
  ): Promise<ForecastResponse> {
    const data = await this.fetchSymbolData(symbol, days);
    if (!data) {
      return { success: false, error: `No data available for ${symbol}` };
    }

    return functimeService.fullPipeline(data, model, horizon, frequency);
  }
}

// Export singleton instance
export const portfolioFunctimeService = new PortfolioFunctimeService();
