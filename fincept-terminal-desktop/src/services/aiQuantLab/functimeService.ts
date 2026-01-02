/**
 * Functime Analytics Service - Frontend API for time series forecasting
 * Provides ML-based forecasting, preprocessing, cross-validation, and metrics
 */

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

export interface FunctimeStatusResponse {
  success: boolean;
  available: boolean;
  sklearn_available?: boolean;
  polars_available?: boolean;
  lightgbm_available?: boolean;
  functime_cloud_available?: boolean;
  message?: string;
  error?: string;
}

export interface ForecastConfig {
  model?: 'linear' | 'lasso' | 'ridge' | 'elasticnet' | 'knn' | 'lightgbm' |
          'auto_lasso' | 'auto_ridge' | 'auto_elasticnet' | 'auto_knn' | 'auto_lightgbm';
  horizon?: number;
  frequency?: '1d' | '1w' | '1mo' | '1q' | '1y' | '1h' | '30m' | string;
  alpha?: number;      // Regularization for lasso/ridge/elasticnet
  l1_ratio?: number;   // L1 ratio for elasticnet
  n_neighbors?: number; // K for KNN
  lags?: number;       // Number of lag features for autoregression
}

export interface ForecastResult {
  entity_id: string;
  time: string;
  value: number;
}

export interface ForecastResponse {
  success: boolean;
  model?: string;
  horizon?: number;
  frequency?: string;
  lags?: number;
  forecast?: ForecastResult[];
  shape?: [number, number];
  best_params?: Record<string, unknown>;
  backend?: string;
  error?: string;
}

export interface PreprocessConfig {
  method?: 'boxcox' | 'scale' | 'difference' | 'lags' | 'rolling' | 'impute';
  scale_method?: 'standard' | 'minmax' | 'robust';
  order?: number;           // For differencing
  lags?: number[];          // For lag features
  window_sizes?: number[];  // For rolling features
  stats?: string[];         // For rolling features: 'mean', 'std', 'min', 'max'
  impute_method?: 'forward' | 'backward' | 'mean' | 'median';
  lambda?: number;          // For Box-Cox
}

export interface PreprocessResponse {
  success: boolean;
  method?: string;
  shape?: [number, number];
  columns?: string[];
  data?: unknown[];
  error?: string;
}

export interface CrossValidateConfig {
  method?: 'train_test' | 'expanding' | 'sliding';
  test_size?: number;
  n_splits?: number;
  train_size?: number;  // For sliding window
  step?: number;
}

export interface SplitSummary {
  train_shape: [number, number];
  test_shape: [number, number];
}

export interface CrossValidateResponse {
  success: boolean;
  method?: string;
  train_shape?: [number, number];
  test_shape?: [number, number];
  n_splits?: number;
  splits_summary?: SplitSummary[];
  error?: string;
}

export interface MetricsResponse {
  success: boolean;
  metrics?: {
    mae?: number;
    rmse?: number;
    smape?: number;
    mape?: number;
    mse?: number;
    mase?: number;
    rmsse?: number;
  };
  error?: string;
}

export interface FeatureResponse {
  success: boolean;
  feature_type?: string;
  shape?: [number, number];
  columns?: string[];
  data?: unknown[];
  error?: string;
}

export interface SeasonalPeriodResponse {
  success: boolean;
  frequency?: string;
  seasonal_period?: number;
  error?: string;
}

export interface FullPipelineResponse {
  success: boolean;
  model?: string;
  horizon?: number;
  frequency?: string;
  preprocessing?: {
    method: string;
    type?: string;
    order?: number;
  };
  forecast?: ForecastResult[];
  forecast_shape?: [number, number];
  best_params?: Record<string, unknown>;
  evaluation_metrics?: {
    mae?: number;
    rmse?: number;
    smape?: number;
  };
  data_summary?: {
    total_rows: number;
    entities: number;
    train_size: number;
    test_size: number;
  };
  error?: string;
}

// Panel data format for functime (requires entity_id, time, value)
export type PanelData =
  | Record<string, Record<string, number>>  // {entity: {date: value}}
  | Record<string, number>                   // {date: value} (single entity)
  | Array<{ entity_id: string; time: string; value: number }>; // Array format

// ============================================================================
// FUNCTIME SERVICE CLASS
// ============================================================================

class FunctimeService {
  /**
   * Check functime library status and availability
   */
  async checkStatus(): Promise<FunctimeStatusResponse> {
    try {
      const result = await invoke<string>('functime_check_status');
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] checkStatus error:', error);
      return {
        success: false,
        available: false,
        error: String(error)
      };
    }
  }

  /**
   * Run forecasting with specified model
   * @param data - Panel data with entity_id, time, value
   * @param config - Forecast configuration
   */
  async forecast(
    data: PanelData,
    config?: ForecastConfig
  ): Promise<ForecastResponse> {
    try {
      const result = await invoke<string>('functime_forecast', {
        dataJson: JSON.stringify(data),
        model: config?.model,
        horizon: config?.horizon,
        frequency: config?.frequency,
        alpha: config?.alpha,
        l1Ratio: config?.l1_ratio,
        nNeighbors: config?.n_neighbors,
        lags: config?.lags
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] forecast error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Apply preprocessing transformations
   * @param data - Panel data
   * @param config - Preprocessing configuration
   */
  async preprocess(
    data: PanelData,
    config?: PreprocessConfig
  ): Promise<PreprocessResponse> {
    try {
      const result = await invoke<string>('functime_preprocess', {
        dataJson: JSON.stringify(data),
        method: config?.method,
        scaleMethod: config?.scale_method,
        order: config?.order,
        lags: config?.lags,
        windowSizes: config?.window_sizes,
        stats: config?.stats,
        imputeMethod: config?.impute_method,
        lambda: config?.lambda
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] preprocess error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Perform cross-validation splits
   * @param data - Panel data
   * @param config - CV configuration
   */
  async crossValidate(
    data: PanelData,
    config?: CrossValidateConfig
  ): Promise<CrossValidateResponse> {
    try {
      const result = await invoke<string>('functime_cross_validate', {
        dataJson: JSON.stringify(data),
        method: config?.method,
        testSize: config?.test_size,
        nSplits: config?.n_splits,
        trainSize: config?.train_size,
        step: config?.step
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] crossValidate error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Calculate forecast accuracy metrics
   * @param yTrue - Actual values (panel data format)
   * @param yPred - Predicted values (panel data format)
   * @param metrics - Which metrics to calculate
   */
  async calculateMetrics(
    yTrue: PanelData,
    yPred: PanelData,
    metrics?: string[]
  ): Promise<MetricsResponse> {
    try {
      const result = await invoke<string>('functime_metrics', {
        yTrueJson: JSON.stringify(yTrue),
        yPredJson: JSON.stringify(yPred),
        metrics
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] calculateMetrics error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Add calendar or holiday features
   * @param data - Panel data
   * @param featureType - 'calendar' or 'future_calendar'
   * @param frequency - Data frequency
   * @param horizon - For future features
   */
  async addFeatures(
    data: PanelData,
    featureType?: 'calendar' | 'future_calendar',
    frequency?: string,
    horizon?: number
  ): Promise<FeatureResponse> {
    try {
      const result = await invoke<string>('functime_add_features', {
        dataJson: JSON.stringify(data),
        featureType,
        frequency,
        horizon
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] addFeatures error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Get seasonal period for a frequency
   * @param frequency - Frequency string (e.g., '1d', '1w', '1mo')
   */
  async getSeasonalPeriod(frequency: string): Promise<SeasonalPeriodResponse> {
    try {
      const result = await invoke<string>('functime_seasonal_period', {
        frequency
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] getSeasonalPeriod error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Run complete forecasting pipeline with preprocessing and evaluation
   * @param data - Panel data
   * @param model - Model type
   * @param horizon - Forecast horizon
   * @param frequency - Data frequency
   * @param testSize - Test set size for evaluation
   * @param preprocess - Preprocessing method (optional)
   */
  async fullPipeline(
    data: PanelData,
    model?: string,
    horizon?: number,
    frequency?: string,
    testSize?: number,
    preprocess?: 'scale' | 'difference'
  ): Promise<FullPipelineResponse> {
    try {
      const result = await invoke<string>('functime_full_pipeline', {
        dataJson: JSON.stringify(data),
        model,
        horizon,
        frequency,
        testSize,
        preprocess
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] fullPipeline error:', error);
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
   * Convert OHLCV data to panel data format
   */
  ohlcvToPanelData(
    data: Array<{ date: string; close: number }>,
    entityId: string = 'default'
  ): Array<{ entity_id: string; time: string; value: number }> {
    return data.map(d => ({
      entity_id: entityId,
      time: d.date,
      value: d.close
    }));
  }

  /**
   * Convert simple time series to panel data format
   */
  timeSeriesToPanelData(
    data: Record<string, number>,
    entityId: string = 'default'
  ): Array<{ entity_id: string; time: string; value: number }> {
    return Object.entries(data).map(([time, value]) => ({
      entity_id: entityId,
      time,
      value
    }));
  }

  /**
   * Combine multiple time series into panel data
   */
  combineTimeSeries(
    series: Record<string, Record<string, number>>
  ): Array<{ entity_id: string; time: string; value: number }> {
    const result: Array<{ entity_id: string; time: string; value: number }> = [];
    for (const [entityId, data] of Object.entries(series)) {
      for (const [time, value] of Object.entries(data)) {
        result.push({ entity_id: entityId, time, value });
      }
    }
    return result;
  }

  /**
   * Format metric value for display
   */
  formatMetric(value: number | null | undefined, decimals = 4): string {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(decimals);
  }

  /**
   * Get available forecast models
   */
  getAvailableModels(): Array<{ id: string; name: string; description: string }> {
    return [
      { id: 'linear', name: 'Linear Regression', description: 'Simple linear model' },
      { id: 'lasso', name: 'Lasso', description: 'L1 regularized linear model' },
      { id: 'ridge', name: 'Ridge', description: 'L2 regularized linear model' },
      { id: 'elasticnet', name: 'ElasticNet', description: 'Combined L1/L2 regularization' },
      { id: 'knn', name: 'KNN', description: 'K-Nearest Neighbors' },
      { id: 'lightgbm', name: 'LightGBM', description: 'Gradient boosting' },
      { id: 'auto_lasso', name: 'Auto Lasso', description: 'Auto-tuned Lasso' },
      { id: 'auto_ridge', name: 'Auto Ridge', description: 'Auto-tuned Ridge' },
      { id: 'auto_elasticnet', name: 'Auto ElasticNet', description: 'Auto-tuned ElasticNet' },
      { id: 'auto_knn', name: 'Auto KNN', description: 'Auto-tuned KNN' },
      { id: 'auto_lightgbm', name: 'Auto LightGBM', description: 'Auto-tuned LightGBM' }
    ];
  }

  /**
   * Get available frequencies
   */
  getAvailableFrequencies(): Array<{ id: string; name: string }> {
    return [
      { id: '1d', name: 'Daily' },
      { id: '1w', name: 'Weekly' },
      { id: '1mo', name: 'Monthly' },
      { id: '1q', name: 'Quarterly' },
      { id: '1y', name: 'Yearly' },
      { id: '1h', name: 'Hourly' },
      { id: '30m', name: '30 Minutes' }
    ];
  }
}

// Export singleton instance
export const functimeService = new FunctimeService();
