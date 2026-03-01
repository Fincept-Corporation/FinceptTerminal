/**
 * Functime Analytics Service - Type Definitions
 */

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
