import { invoke } from '@tauri-apps/api/core';

export interface QuantAnalyticsResult<T = Record<string, unknown>> {
  success: boolean;
  analysis_type?: string;
  result?: T;
  error?: string;
  traceback?: string;
  timestamp?: string;
  available_analyses?: Record<string, Record<string, string>>;
  available_commands?: string[];
  [key: string]: unknown;
}

export interface TrendAnalysisParams extends Record<string, unknown> {
  data: number[] | string;
  trend_type?: 'linear' | 'log_linear';
}

export interface StationarityTestParams extends Record<string, unknown> {
  data: number[] | string;
  test_type?: 'adf' | 'kpss' | 'pp';
}

export interface ARIMAModelParams extends Record<string, unknown> {
  data: number[] | string;
  order?: [number, number, number];
  seasonal_order?: [number, number, number, number];
}

export interface ForecastingParams extends Record<string, unknown> {
  data: number[] | string;
  horizon?: number;
  method?: 'simple_exponential' | 'linear_trend' | 'moving_average';
  train_size?: number;
}

export interface SupervisedLearningParams extends Record<string, unknown> {
  X: number[][];
  y: number[];
  problem_type?: 'regression' | 'classification';
  algorithms?: string[];
}

export interface UnsupervisedLearningParams extends Record<string, unknown> {
  X: number[][];
  methods?: ('pca' | 'kmeans' | 'hierarchical')[];
}

export interface ModelEvaluationParams extends Record<string, unknown> {
  X: number[][];
  y: number[];
  model_type?: string;
  cv_folds?: number;
}

export interface FeatureEngineeringParams extends Record<string, unknown> {
  data: Record<string, number[]>;
  target_column?: string;
  text_columns?: string[];
}

export interface SamplingTechniquesParams extends Record<string, unknown> {
  population: number[];
  sample_size: number;
  sampling_methods?: ('simple' | 'stratified' | 'systematic' | 'cluster')[];
}

export interface CLTParams extends Record<string, unknown> {
  population_dist?: 'exponential' | 'uniform' | 'binomial' | 'normal';
  population_params?: Record<string, number>;
  sample_sizes?: number[];
  n_samples?: number;
}

export interface ResamplingParams extends Record<string, unknown> {
  data: number[];
  methods?: ('bootstrap' | 'jackknife' | 'permutation')[];
  n_resamples?: number;
}

export interface SamplingErrorParams extends Record<string, unknown> {
  population_mean: number;
  population_std: number;
  sample_sizes?: number[];
  confidence_level?: number;
  n_simulations?: number;
}

export interface ValidateDataParams extends Record<string, unknown> {
  data: number[] | Record<string, number[]>;
  data_type?: 'general' | 'returns' | 'prices' | 'rates';
  data_name?: string;
}

export interface TrendResult {
  value: {
    slope: number;
    intercept: number;
  };
  method: string;
  metadata: {
    fitted_values: number[];
    residuals: number[];
    r_squared: number;
    slope_t_statistic: number;
    slope_p_value: number;
    trend_significant: boolean;
    trend_direction: 'upward' | 'downward' | 'flat';
  };
}

export interface StationarityResult {
  value: {
    test_statistic: number;
    p_value: number;
    critical_values: Record<string, number>;
    is_stationary: boolean;
    null_hypothesis: string;
  };
  method: string;
  metadata: {
    rolling_mean_stability: number;
    rolling_std_stability: number;
    mean_reversion_strength: number;
  };
}

export interface SupervisedLearningResult {
  value: Record<string, {
    mse?: number;
    rmse?: number;
    r2_score?: number;
    accuracy?: number;
    precision?: number;
    recall?: number;
    predictions: number[];
    error?: string;
  }>;
  method: string;
  metadata: {
    train_size: number;
    test_size: number;
    n_features: number;
    best_algorithm: string | null;
  };
}

export interface UnsupervisedLearningResult {
  value: {
    pca?: {
      explained_variance_ratio: number[];
      cumulative_variance: number[];
      components_for_95_variance: number;
      principal_components: number[][];
    };
    kmeans?: {
      optimal_k: number;
      cluster_labels: number[];
      cluster_centers: number[][];
      inertia_by_k: [number, number][];
      silhouette_by_k: [number, number][];
    };
    hierarchical?: {
      cluster_labels: number[];
      n_clusters: number;
    };
  };
  method: string;
  metadata: {
    n_samples: number;
    n_features: number;
    data_scaled: boolean;
  };
}

export interface DataQualityResult {
  data_name: string;
  timestamp: string;
  quality_score: number;
  issues: Array<{
    type: string;
    description: string;
    severity: string;
    timestamp: string;
  }>;
  warnings: Array<{
    type: string;
    message: string;
    timestamp: string;
  }>;
  statistics: Record<string, unknown>;
  recommendations: string[];
}

async function executeQuantAnalytics<T>(
  command: string,
  params?: Record<string, unknown>
): Promise<QuantAnalyticsResult<T>> {
  try {
    const paramsJson = params ? JSON.stringify(params) : undefined;

    const result = await invoke<string>('execute_quant_analytics', {
      command,
      params: paramsJson,
    });

    return JSON.parse(result) as QuantAnalyticsResult<T>;
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

export const quantAnalyticsService = {
  listAvailableAnalyses(): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('list');
  },

  analyzeTrend(params: TrendAnalysisParams): Promise<QuantAnalyticsResult<TrendResult>> {
    return executeQuantAnalytics('trend_analysis', params);
  },

  testStationarity(params: StationarityTestParams): Promise<QuantAnalyticsResult<StationarityResult>> {
    return executeQuantAnalytics('stationarity_test', params);
  },

  fitARIMAModel(params: ARIMAModelParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('arima_model', params);
  },

  generateForecasts(params: ForecastingParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('forecasting', params);
  },

  supervisedLearning(params: SupervisedLearningParams): Promise<QuantAnalyticsResult<SupervisedLearningResult>> {
    return executeQuantAnalytics('supervised_learning', params);
  },

  unsupervisedLearning(params: UnsupervisedLearningParams): Promise<QuantAnalyticsResult<UnsupervisedLearningResult>> {
    return executeQuantAnalytics('unsupervised_learning', params);
  },

  evaluateModel(params: ModelEvaluationParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('model_evaluation', params);
  },

  featureEngineering(params: FeatureEngineeringParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('feature_engineering', params);
  },

  samplingTechniques(params: SamplingTechniquesParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('sampling_techniques', params);
  },

  demonstrateCLT(params: CLTParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('central_limit_theorem', params);
  },

  resamplingMethods(params: ResamplingParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('resampling_methods', params);
  },

  samplingErrorAnalysis(params: SamplingErrorParams): Promise<QuantAnalyticsResult> {
    return executeQuantAnalytics('sampling_error_analysis', params);
  },

  validateData(params: ValidateDataParams): Promise<QuantAnalyticsResult<DataQualityResult>> {
    return executeQuantAnalytics('validate_data', params);
  },

  executeCustomCommand<T>(
    command: string,
    params?: Record<string, unknown>
  ): Promise<QuantAnalyticsResult<T>> {
    return executeQuantAnalytics<T>(command, params);
  },
};

export default quantAnalyticsService;
