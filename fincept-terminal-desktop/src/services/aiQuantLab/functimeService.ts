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
  advanced_models_available?: boolean;
  ensemble_available?: boolean;
  anomaly_available?: boolean;
  seasonality_available?: boolean;
  confidence_available?: boolean;
  feature_importance_available?: boolean;
  advanced_cv_available?: boolean;
  backtesting_available?: boolean;
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
// ADVANCED MODELS TYPES
// ============================================================================

export type AdvancedModel = 'naive' | 'seasonal_naive' | 'drift' | 'ses' | 'holt' |
                            'theta' | 'croston' | 'xgboost' | 'catboost';

export interface AdvancedForecastConfig {
  model: AdvancedModel;
  horizon?: number;
  frequency?: string;
  model_params?: {
    seasonal_period?: number;
    alpha?: number;
    beta?: number;
    n_lags?: number;
    xgb_params?: Record<string, unknown>;
    cat_params?: Record<string, unknown>;
  };
}

export interface AdvancedForecastResponse {
  success: boolean;
  model?: string;
  forecast?: ForecastResult[];
  shape?: [number, number];
  error?: string;
}


// ============================================================================
// ENSEMBLE TYPES
// ============================================================================

export type EnsembleMethod = 'mean' | 'median' | 'trimmed_mean' | 'weighted' | 'stacking';

export interface EnsembleConfig {
  method: EnsembleMethod;
  trim_pct?: number;
  weights?: number[];
}

export interface EnsembleResponse {
  success: boolean;
  method?: string;
  data?: Array<{ entity_id: string; time: string; value: number }>;
  shape?: [number, number];
  error?: string;
}

export interface AutoEnsembleResponse {
  success: boolean;
  method?: string;
  models_used?: string[];
  model_scores?: Record<string, number>;
  ensemble_forecast?: Array<{ entity_id: string; time: string; value: number }>;
  error?: string;
}


// ============================================================================
// ANOMALY DETECTION TYPES
// ============================================================================

export type AnomalyMethod = 'zscore' | 'iqr' | 'grubbs' | 'isolation_forest' | 'lof' | 'residual' | 'change_points';

export interface AnomalyDetectionConfig {
  method: AnomalyMethod;
  threshold?: number;
  multiplier?: number;
  alpha?: number;
  contamination?: number;
  n_neighbors?: number;
  n_bkps?: number;
}

export interface AnomalyResult {
  entity_id: string;
  anomalies: Array<{
    index: number;
    time: string;
    value: number;
    zscore?: number;
    is_anomaly: boolean;
  }>;
  n_anomalies: number;
  anomaly_rate: number;
}

export interface AnomalyDetectionResponse {
  success: boolean;
  method?: string;
  results?: Record<string, AnomalyResult>;
  error?: string;
}


// ============================================================================
// SEASONALITY TYPES
// ============================================================================

export type SeasonalityOperation = 'detect_period' | 'decompose_stl' | 'decompose_classical' | 'adjust' | 'metrics';

export interface SeasonalityConfig {
  operation: SeasonalityOperation;
  period?: number;
  method?: 'fft' | 'acf' | 'both' | 'stl' | 'classical';
  max_period?: number;
  robust?: boolean;
  model?: 'additive' | 'multiplicative';
}

export interface SeasonalDecompositionResult {
  trend: number[];
  seasonal: number[];
  residual: number[];
  times: string[];
  period: number;
  seasonal_strength?: number;
}

export interface SeasonalityResponse {
  success: boolean;
  method?: string;
  results?: Record<string, SeasonalDecompositionResult | { period: number; strength: number }>;
  data?: Array<{ entity_id: string; time: string; original: number; adjusted: number }>;
  metrics?: Record<string, {
    detected_period?: number;
    seasonal_strength: number;
    trend_strength: number;
    is_seasonal: boolean;
    is_trending: boolean;
  }>;
  error?: string;
}


// ============================================================================
// CONFIDENCE INTERVALS TYPES
// ============================================================================

export type IntervalMethod = 'bootstrap' | 'residual' | 'quantile' | 'conformal' | 'monte_carlo';

export interface ConfidenceIntervalsConfig {
  method: IntervalMethod;
  horizon?: number;
  frequency?: string;
  confidence_levels?: number[];
  n_bootstrap?: number;
  n_simulations?: number;
  lags?: number;
  quantiles?: number[];
  calibration_size?: number;
  model?: 'random_walk' | 'drift' | 'mean_revert';
}

export interface IntervalDataPoint {
  entity_id: string;
  time: string;
  forecast: number;
  horizon: number;
  lower_80?: number;
  upper_80?: number;
  lower_95?: number;
  upper_95?: number;
  [key: string]: unknown;
}

export interface ConfidenceIntervalsResponse {
  success: boolean;
  method?: string;
  confidence_levels?: number[];
  data?: IntervalDataPoint[];
  shape?: [number, number];
  error?: string;
}


// ============================================================================
// FEATURE IMPORTANCE TYPES
// ============================================================================

export type ImportanceMethod = 'permutation' | 'model' | 'shap' | 'lag_analysis' | 'sensitivity';

export interface FeatureImportanceConfig {
  method: ImportanceMethod;
  n_lags?: number;
  n_repeats?: number;
  model_type?: 'ridge' | 'lasso' | 'rf';
  max_samples?: number;
  max_lags?: number;
  horizon?: number;
  perturbation_pct?: number;
  frequency?: string;
}

export interface FeatureImportanceResult {
  feature_names: string[];
  importance_mean?: number[];
  importance_std?: number[];
  importance?: number[];
  acf?: number[];
  pacf?: number[];
  significant_lags?: number[];
  recommended_lags?: number[];
  sensitivity_by_point?: Array<{
    point_index: number;
    recency: number;
    sensitivity: number;
  }>;
}

export interface FeatureImportanceResponse {
  success: boolean;
  method?: string;
  n_lags?: number;
  results?: Record<string, FeatureImportanceResult>;
  error?: string;
}


// ============================================================================
// ADVANCED CROSS-VALIDATION TYPES
// ============================================================================

export type AdvancedCVMethod = 'blocked' | 'purged' | 'combinatorial' | 'walk_forward' | 'nested';

export interface AdvancedCVConfig {
  method: AdvancedCVMethod;
  n_splits?: number;
  test_size?: number;
  gap?: number;
  embargo_pct?: number;
  purge_pct?: number;
  n_test_splits?: number;
  n_groups?: number;
  initial_train_size?: number;
  step_size?: number;
  anchored?: boolean;
  outer_splits?: number;
  inner_splits?: number;
}

export interface CVSplitInfo {
  split: number;
  train_shape: [number, number];
  test_shape: [number, number];
  train_size?: number;
  test_size?: number;
}

export interface AdvancedCVResponse {
  success: boolean;
  method?: string;
  n_splits?: number;
  data?: Array<{
    entity_id: string;
    n_splits: number;
    splits: CVSplitInfo[];
  }>;
  error?: string;
}


// ============================================================================
// BACKTESTING TYPES
// ============================================================================

export type BacktestOperation = 'walk_forward' | 'compare_models' | 'rolling_origin' | 'attribution' | 'summary';

export interface BacktestConfig {
  operation: BacktestOperation;
  initial_train_size?: number;
  test_size?: number;
  step_size?: number;
  horizons?: number[];
  frequency?: string;
  retrain?: boolean;
  min_train_size?: number;
  max_train_size?: number;
}

export interface BacktestForecast {
  origin: number;
  horizon: number;
  forecast: number;
  actual: number;
  error: number;
  abs_error: number;
}

export interface BacktestEntityResult {
  entity_id: string;
  n_origins: number;
  forecasts: BacktestForecast[];
  metrics_by_horizon: Record<number, {
    mae: number;
    rmse: number;
    mape: number;
    bias: number;
    n_forecasts: number;
  }>;
}

export interface BacktestResponse {
  success: boolean;
  method?: string;
  results?: BacktestEntityResult[];
  comparison?: Array<{
    model: string;
    mae: number;
    rmse: number;
    mape: number;
    rank: number;
  }>;
  best_model?: string;
  summary?: {
    total_forecasts: number;
    total_origins: number;
    overall_metrics: {
      mae: number;
      rmse: number;
      bias: number;
      std_error: number;
    };
    error_distribution: {
      min: number;
      p25: number;
      median: number;
      p75: number;
      p95: number;
      max: number;
    };
    by_horizon: Record<number, {
      mae: number;
      rmse: number;
      n_forecasts: number;
    }>;
  };
  error?: string;
}

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
  // ADVANCED MODELS METHODS
  // ============================================================================

  /**
   * Run advanced forecasting models (Naive, SES, Holt, Theta, Croston, XGBoost, CatBoost)
   */
  async advancedForecast(
    data: PanelData,
    config: AdvancedForecastConfig
  ): Promise<AdvancedForecastResponse> {
    try {
      const result = await invoke<string>('functime_advanced_forecast', {
        dataJson: JSON.stringify(data),
        model: config.model,
        horizon: config.horizon,
        frequency: config.frequency,
        modelParams: config.model_params
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] advancedForecast error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // ENSEMBLE METHODS
  // ============================================================================

  /**
   * Combine multiple forecasts using ensemble methods
   */
  async ensemble(
    forecasts: Array<Array<{ entity_id: string; time: string; value: number }>>,
    config: EnsembleConfig,
    yTrain?: PanelData
  ): Promise<EnsembleResponse> {
    try {
      const result = await invoke<string>('functime_ensemble', {
        forecastsJson: JSON.stringify(forecasts),
        method: config.method,
        trimPct: config.trim_pct,
        weights: config.weights,
        yTrainJson: yTrain ? JSON.stringify(yTrain) : undefined
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] ensemble error:', error);
      return { success: false, error: String(error) };
    }
  }

  /**
   * Automatic ensemble selection and combination
   */
  async autoEnsemble(
    data: PanelData,
    horizon?: number,
    frequency?: string,
    nBest?: number
  ): Promise<AutoEnsembleResponse> {
    try {
      const result = await invoke<string>('functime_auto_ensemble', {
        dataJson: JSON.stringify(data),
        horizon,
        frequency,
        nBest
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] autoEnsemble error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // ANOMALY DETECTION METHODS
  // ============================================================================

  /**
   * Detect anomalies in time series data
   */
  async detectAnomalies(
    data: PanelData,
    config: AnomalyDetectionConfig
  ): Promise<AnomalyDetectionResponse> {
    try {
      const result = await invoke<string>('functime_anomaly_detection', {
        dataJson: JSON.stringify(data),
        method: config.method,
        threshold: config.threshold,
        multiplier: config.multiplier,
        alpha: config.alpha,
        contamination: config.contamination,
        nNeighbors: config.n_neighbors,
        nBkps: config.n_bkps
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] detectAnomalies error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // SEASONALITY METHODS
  // ============================================================================

  /**
   * Analyze seasonality patterns in time series
   */
  async analyzeSeasonality(
    data: PanelData,
    config: SeasonalityConfig
  ): Promise<SeasonalityResponse> {
    try {
      const result = await invoke<string>('functime_seasonality', {
        dataJson: JSON.stringify(data),
        operation: config.operation,
        period: config.period,
        method: config.method,
        maxPeriod: config.max_period,
        robust: config.robust,
        model: config.model
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] analyzeSeasonality error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // CONFIDENCE INTERVALS METHODS
  // ============================================================================

  /**
   * Generate prediction/confidence intervals
   */
  async generateIntervals(
    data: PanelData,
    config: ConfidenceIntervalsConfig,
    forecast?: PanelData
  ): Promise<ConfidenceIntervalsResponse> {
    try {
      const result = await invoke<string>('functime_confidence_intervals', {
        dataJson: JSON.stringify(data),
        forecastJson: forecast ? JSON.stringify(forecast) : undefined,
        method: config.method,
        horizon: config.horizon,
        frequency: config.frequency,
        confidenceLevels: config.confidence_levels,
        nBootstrap: config.n_bootstrap,
        nSimulations: config.n_simulations,
        lags: config.lags,
        quantiles: config.quantiles,
        calibrationSize: config.calibration_size,
        model: config.model
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] generateIntervals error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // FEATURE IMPORTANCE METHODS
  // ============================================================================

  /**
   * Calculate feature importance for time series models
   */
  async calculateFeatureImportance(
    data: PanelData,
    config: FeatureImportanceConfig
  ): Promise<FeatureImportanceResponse> {
    try {
      const result = await invoke<string>('functime_feature_importance', {
        dataJson: JSON.stringify(data),
        method: config.method,
        nLags: config.n_lags,
        nRepeats: config.n_repeats,
        modelType: config.model_type,
        maxSamples: config.max_samples,
        maxLags: config.max_lags,
        horizon: config.horizon,
        perturbationPct: config.perturbation_pct,
        frequency: config.frequency
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] calculateFeatureImportance error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // ADVANCED CROSS-VALIDATION METHODS
  // ============================================================================

  /**
   * Run advanced cross-validation methods
   */
  async advancedCrossValidate(
    data: PanelData,
    config: AdvancedCVConfig
  ): Promise<AdvancedCVResponse> {
    try {
      const result = await invoke<string>('functime_advanced_cv', {
        dataJson: JSON.stringify(data),
        method: config.method,
        nSplits: config.n_splits,
        testSize: config.test_size,
        gap: config.gap,
        embargoPct: config.embargo_pct,
        purgePct: config.purge_pct,
        nTestSplits: config.n_test_splits,
        nGroups: config.n_groups,
        initialTrainSize: config.initial_train_size,
        stepSize: config.step_size,
        anchored: config.anchored,
        outerSplits: config.outer_splits,
        innerSplits: config.inner_splits
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] advancedCrossValidate error:', error);
      return { success: false, error: String(error) };
    }
  }

  // ============================================================================
  // BACKTESTING METHODS
  // ============================================================================

  /**
   * Run backtesting on time series forecasts
   */
  async backtest(
    data: PanelData,
    config: BacktestConfig
  ): Promise<BacktestResponse> {
    try {
      const result = await invoke<string>('functime_backtest', {
        dataJson: JSON.stringify(data),
        operation: config.operation,
        initialTrainSize: config.initial_train_size,
        testSize: config.test_size,
        stepSize: config.step_size,
        horizons: config.horizons,
        frequency: config.frequency,
        retrain: config.retrain,
        minTrainSize: config.min_train_size,
        maxTrainSize: config.max_train_size
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[FunctimeService] backtest error:', error);
      return { success: false, error: String(error) };
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
   * Get available advanced forecast models
   */
  getAdvancedModels(): Array<{ id: string; name: string; description: string; category: string }> {
    return [
      // Baseline models
      { id: 'naive', name: 'Naive', description: 'Last value forecast', category: 'Baseline' },
      { id: 'seasonal_naive', name: 'Seasonal Naive', description: 'Same period last season', category: 'Baseline' },
      { id: 'drift', name: 'Drift', description: 'Linear extrapolation from history', category: 'Baseline' },
      // Exponential smoothing
      { id: 'ses', name: 'Simple Exponential Smoothing', description: 'Level smoothing only', category: 'Exponential Smoothing' },
      { id: 'holt', name: 'Holt\'s Linear Trend', description: 'Level and trend smoothing', category: 'Exponential Smoothing' },
      { id: 'theta', name: 'Theta Method', description: 'Decomposition-based forecasting', category: 'Exponential Smoothing' },
      // Specialized
      { id: 'croston', name: 'Croston\'s Method', description: 'For intermittent demand', category: 'Specialized' },
      // ML models
      { id: 'xgboost', name: 'XGBoost', description: 'Gradient boosting with lag features', category: 'Machine Learning' },
      { id: 'catboost', name: 'CatBoost', description: 'Gradient boosting with categorical support', category: 'Machine Learning' }
    ];
  }

  /**
   * Get available anomaly detection methods
   */
  getAnomalyMethods(): Array<{ id: string; name: string; description: string }> {
    return [
      { id: 'zscore', name: 'Z-Score', description: 'Statistical outlier detection' },
      { id: 'iqr', name: 'IQR', description: 'Interquartile range method' },
      { id: 'grubbs', name: 'Grubbs Test', description: 'Statistical hypothesis testing' },
      { id: 'isolation_forest', name: 'Isolation Forest', description: 'Tree-based anomaly detection' },
      { id: 'lof', name: 'Local Outlier Factor', description: 'Density-based detection' },
      { id: 'residual', name: 'Residual', description: 'Model residual-based detection' },
      { id: 'change_points', name: 'Change Points', description: 'Structural break detection' }
    ];
  }

  /**
   * Get available ensemble methods
   */
  getEnsembleMethods(): Array<{ id: string; name: string; description: string }> {
    return [
      { id: 'mean', name: 'Mean', description: 'Simple average of forecasts' },
      { id: 'median', name: 'Median', description: 'Median of forecasts (robust)' },
      { id: 'trimmed_mean', name: 'Trimmed Mean', description: 'Mean with outliers removed' },
      { id: 'weighted', name: 'Weighted', description: 'Error-weighted combination' },
      { id: 'stacking', name: 'Stacking', description: 'Meta-learner combination' }
    ];
  }

  /**
   * Get available confidence interval methods
   */
  getIntervalMethods(): Array<{ id: string; name: string; description: string }> {
    return [
      { id: 'bootstrap', name: 'Bootstrap', description: 'Resampling-based intervals' },
      { id: 'residual', name: 'Residual', description: 'Based on historical residuals' },
      { id: 'quantile', name: 'Quantile Regression', description: 'Direct quantile modeling' },
      { id: 'conformal', name: 'Conformal Prediction', description: 'Coverage-guaranteed intervals' },
      { id: 'monte_carlo', name: 'Monte Carlo', description: 'Simulation-based intervals' }
    ];
  }

  /**
   * Get available CV methods
   */
  getCVMethods(): Array<{ id: string; name: string; description: string }> {
    return [
      { id: 'blocked', name: 'Blocked', description: 'Fixed-size training blocks' },
      { id: 'purged', name: 'Purged K-Fold', description: 'For overlapping labels' },
      { id: 'combinatorial', name: 'Combinatorial Purged', description: 'All test group combinations' },
      { id: 'walk_forward', name: 'Walk-Forward', description: 'Rolling origin validation' },
      { id: 'nested', name: 'Nested', description: 'For model selection' }
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
