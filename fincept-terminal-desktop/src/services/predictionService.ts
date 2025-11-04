/**
 * AI Prediction Service for FinceptTerminal
 * Provides stock price forecasting, volatility prediction, and backtesting
 */

import { invoke } from '@tauri-apps/api/core';
import { Command } from '@tauri-apps/plugin-shell';

export interface PredictionResult {
  success: boolean;
  symbol?: string;
  predictions?: {
    forecast: number[];
    lower_ci: number[];
    upper_ci: number[];
    forecast_dates?: string[];
  };
  ensemble?: {
    forecast: number[];
    lower_ci: number[];
    upper_ci: number[];
    model_weights: Record<string, number>;
    forecast_dates: string[];
  };
  forecasts?: Record<string, any>;
  signals?: {
    type: 'BUY' | 'SELL' | 'HOLD';
    strength: number;
    confidence: string;
    expected_return_1d: number;
    current_price: number;
    predicted_price_1d: number;
  }[];
  error?: string;
  execution_time_ms?: number;
}

export interface VolatilityForecast {
  success: boolean;
  model_type: string;
  current_volatility: number;
  forecast_horizon: number;
  volatility_forecast: number[];
  forecast_dates: string[];
  mean_forecast_vol: number;
  max_forecast_vol: number;
  min_forecast_vol: number;
  error?: string;
}

export interface BacktestResult {
  success: boolean;
  start_date: string;
  end_date: string;
  initial_capital: number;
  final_capital: number;
  final_portfolio_value: number;
  total_trades: number;
  equity_curve: Array<{ date: string; value: number }>;
  trades: Array<any>;
  metrics: {
    total_return: number;
    total_return_dollar: number;
    sharpe_ratio: number;
    sortino_ratio: number;
    max_drawdown: number;
    calmar_ratio: number;
    win_rate: number;
    total_trades: number;
    winning_trades: number;
    losing_trades: number;
    avg_win: number;
    avg_loss: number;
    profit_factor: number;
    volatility: number;
    avg_daily_return: number;
  };
  error?: string;
}

export interface TimeSeriesPredictionParams {
  data: number[];
  dates?: string[];
  model_type: 'arima' | 'prophet' | 'lstm' | 'sarima';
  steps_ahead: number;
  params?: Record<string, any>;
}

export interface StockPredictionParams {
  symbol: string;
  ohlcv_data: {
    dates: string[];
    open: number[];
    high: number[];
    low: number[];
    close: number[];
    volume: number[];
  };
  action: 'fit' | 'predict' | 'both';
  steps_ahead?: number;
  method?: 'arima' | 'xgboost' | 'random_forest' | 'lstm' | 'ensemble';
}

export interface VolatilityPredictionParams {
  prices: number[];
  dates: string[];
  action: 'fit_garch' | 'predict' | 'var' | 'regime' | 'historical';
  params?: {
    p?: number;
    q?: number;
    vol?: 'GARCH' | 'EGARCH' | 'GJR-GARCH';
    horizon?: number;
    confidence_level?: number;
    portfolio_value?: number;
    low_threshold?: number;
    high_threshold?: number;
    window?: number;
  };
}

export interface BacktestParams {
  data: {
    dates: string[];
    close: number[];
    high?: number[];
    low?: number[];
    volume?: number[];
  };
  strategy: 'ma_crossover' | 'rsi';
  strategy_params: Record<string, any>;
  initial_capital?: number;
  commission?: number;
  slippage?: number;
  action: 'backtest' | 'walk_forward';
  train_window?: number;
  test_window?: number;
  step_size?: number;
}

class PredictionService {
  private pythonPath: string = 'python3';

  /**
   * Execute Python prediction script
   */
  private async executePythonScript(
    scriptPath: string,
    input: any
  ): Promise<any> {
    try {
      const command = Command.create('run-python', [scriptPath]);

      // Convert input to JSON string
      const inputJson = JSON.stringify(input);

      const output = await command.execute();

      if (output.code !== 0) {
        throw new Error(output.stderr || 'Python script execution failed');
      }

      // Parse JSON output
      const result = JSON.parse(output.stdout);
      return result;
    } catch (error: any) {
      console.error('[PredictionService] Script execution failed:', error);
      throw error;
    }
  }

  /**
   * Predict time series using ARIMA, Prophet, or LSTM
   */
  async predictTimeSeries(
    params: TimeSeriesPredictionParams
  ): Promise<PredictionResult> {
    try {
      const scriptPath = 'scripts/Analytics/prediction/time_series_models.py';

      const result = await this.executePythonScript(scriptPath, params);

      return {
        success: !result.error,
        predictions: result.predictions,
        error: result.error,
      };
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Time series prediction failed',
      };
    }
  }

  /**
   * Predict stock prices using ensemble models
   */
  async predictStockPrice(
    params: StockPredictionParams
  ): Promise<PredictionResult> {
    try {
      const scriptPath = 'scripts/Analytics/prediction/stock_price_predictor.py';

      const result = await this.executePythonScript(scriptPath, params);

      if (result.error) {
        return {
          success: false,
          error: result.error,
        };
      }

      return {
        success: true,
        symbol: params.symbol,
        predictions: result.predictions,
        ensemble: result.predictions?.ensemble,
        forecasts: result.predictions?.forecasts,
        signals: result.signals?.signals,
      };
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Stock prediction failed',
      };
    }
  }

  /**
   * Forecast volatility using GARCH models
   */
  async predictVolatility(
    params: VolatilityPredictionParams
  ): Promise<VolatilityForecast> {
    try {
      const scriptPath = 'scripts/Analytics/prediction/volatility_forecaster.py';

      const result = await this.executePythonScript(scriptPath, params);

      if (result.error) {
        return {
          success: false,
          error: result.error,
        } as VolatilityForecast;
      }

      return result as VolatilityForecast;
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Volatility prediction failed',
      } as VolatilityForecast;
    }
  }

  /**
   * Run strategy backtest
   */
  async runBacktest(params: BacktestParams): Promise<BacktestResult> {
    try {
      const scriptPath = 'scripts/Analytics/prediction/backtesting_engine.py';

      const result = await this.executePythonScript(scriptPath, params);

      if (result.error) {
        return {
          success: false,
          error: result.error,
        } as BacktestResult;
      }

      return result as BacktestResult;
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Backtesting failed',
      } as BacktestResult;
    }
  }

  /**
   * Execute stock prediction agent
   */
  async executeStockPredictionAgent(
    tickers: string[],
    mode: 'single' | 'multi' = 'multi'
  ): Promise<any> {
    try {
      const scriptPath = 'scripts/agents/PredictionAgents/stock_prediction_agent.py';

      const input = {
        tickers,
        mode,
      };

      const result = await this.executePythonScript(scriptPath, input);
      return result;
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Stock prediction agent failed',
      };
    }
  }

  /**
   * Execute volatility prediction agent
   */
  async executeVolatilityAgent(
    tickers: string[],
    mode: 'single' | 'portfolio' = 'portfolio'
  ): Promise<any> {
    try {
      const scriptPath =
        'scripts/agents/PredictionAgents/volatility_prediction_agent.py';

      const input = {
        tickers,
        mode,
      };

      const result = await this.executePythonScript(scriptPath, input);
      return result;
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Volatility agent failed',
      };
    }
  }

  /**
   * Get historical data for prediction
   */
  async getHistoricalData(
    symbol: string,
    period: string = '1y'
  ): Promise<StockPredictionParams['ohlcv_data'] | null> {
    try {
      // Use existing market data service
      // This is a placeholder - integrate with actual market data service
      const response = await fetch(
        `https://query1.finance.yahoo.com/v8/finance/chart/${symbol}?period1=0&period2=9999999999&interval=1d`
      );

      if (!response.ok) {
        throw new Error('Failed to fetch historical data');
      }

      const data = await response.json();
      const quotes = data.chart.result[0];
      const timestamps = quotes.timestamp;
      const prices = quotes.indicators.quote[0];

      return {
        dates: timestamps.map((ts: number) => new Date(ts * 1000).toISOString()),
        open: prices.open,
        high: prices.high,
        low: prices.low,
        close: prices.close,
        volume: prices.volume,
      };
    } catch (error) {
      console.error('[PredictionService] Failed to get historical data:', error);
      return null;
    }
  }

  /**
   * Quick prediction for a single stock
   */
  async quickPredict(
    symbol: string,
    daysAhead: number = 30
  ): Promise<PredictionResult> {
    try {
      // Get historical data
      const historicalData = await this.getHistoricalData(symbol, '1y');

      if (!historicalData) {
        return {
          success: false,
          error: 'Failed to fetch historical data',
        };
      }

      // Run prediction
      const result = await this.predictStockPrice({
        symbol,
        ohlcv_data: historicalData,
        action: 'both',
        steps_ahead: daysAhead,
        method: 'ensemble',
      });

      return result;
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Quick prediction failed',
      };
    }
  }

  /**
   * Get prediction recommendations for portfolio
   */
  async getPortfolioPredictions(symbols: string[]): Promise<any> {
    try {
      const predictions = await Promise.all(
        symbols.map((symbol) => this.quickPredict(symbol, 30))
      );

      const successful = predictions.filter((p) => p.success);

      // Rank by expected return
      const ranked = successful
        .map((pred) => ({
          symbol: pred.symbol,
          expected_return:
            pred.signals && pred.signals.length > 0
              ? pred.signals[0].expected_return_1d
              : 0,
          signal:
            pred.signals && pred.signals.length > 0
              ? pred.signals[0].type
              : 'HOLD',
          forecast: pred.ensemble?.forecast[0],
        }))
        .sort((a, b) => b.expected_return - a.expected_return);

      return {
        success: true,
        predictions: ranked,
        total_stocks: symbols.length,
        successful_predictions: successful.length,
      };
    } catch (error: any) {
      return {
        success: false,
        error: error.message || 'Portfolio predictions failed',
      };
    }
  }
}

// Export singleton instance
export const predictionService = new PredictionService();
export default predictionService;
