import { invoke } from '@tauri-apps/api/core';

export interface StatsmodelsResult<T = Record<string, unknown>> {
  success: boolean;
  analysis_type?: string;
  result?: T;
  error?: string;
  traceback?: string;
  timestamp?: string;
  available_commands?: string[];
  available_analyses?: Record<string, Record<string, string>>;
  message?: string;
  suggestions?: string[];
  [key: string]: unknown;
}

export interface ARIMAParams extends Record<string, unknown> {
  data: number[] | string;
  order?: [number, number, number];
}

export interface ARIMAForecastParams extends Record<string, unknown> {
  data: number[] | string;
  order?: [number, number, number];
  steps?: number;
}

export interface SARIMAXParams extends Record<string, unknown> {
  data: number[] | string;
  order?: [number, number, number];
  seasonal_order?: [number, number, number, number];
}

export interface ExponentialSmoothingParams extends Record<string, unknown> {
  data: number[] | string;
  trend?: 'add' | 'mul' | null;
  seasonal?: 'add' | 'mul' | null;
  seasonal_periods?: number;
}

export interface STLDecomposeParams extends Record<string, unknown> {
  data: number[] | string;
  period?: number;
  seasonal?: number;
}

export interface StationarityTestParams extends Record<string, unknown> {
  data: number[] | string;
}

export interface ACFPACFParams extends Record<string, unknown> {
  data: number[] | string;
  nlags?: number;
}

export interface OLSParams extends Record<string, unknown> {
  data: number[] | string;
  X?: number[][] | string;
}

export interface TTestIndParams extends Record<string, unknown> {
  sample1: number[] | string;
  sample2: number[] | string;
}

export interface TTestOneSampleParams extends Record<string, unknown> {
  data: number[] | string;
  popmean?: number;
}

export interface PowerAnalysisParams extends Record<string, unknown> {
  effect_size?: number;
  alpha?: number;
  power?: number;
  nobs?: number;
}

export interface PCAParams extends Record<string, unknown> {
  data: Record<string, number[]>;
  n_components?: number;
}

export interface LOWESSParams extends Record<string, unknown> {
  data: number[] | string;
  x?: number[] | string;
  frac?: number;
}

export interface ARIMAResult {
  params: number[];
  aic: number;
  bic: number;
  hqic?: number;
  resid: number[];
  fittedvalues: number[];
}

export interface ForecastResult {
  forecast: number[];
  aic: number;
  bic: number;
}

export interface STLResult {
  trend: number[];
  seasonal: number[];
  resid: number[];
}

export interface StationarityResult {
  statistic: number;
  pvalue: number;
  critical_values?: Record<string, number>;
  is_stationary?: boolean;
}

export interface RegressionResult {
  params: number[];
  rsquared: number;
  rsquared_adj?: number;
  fvalue?: number;
  f_pvalue?: number;
  aic?: number;
  bic?: number;
  resid?: number[];
}

export interface DescriptiveResult {
  mean: number;
  std: number;
  min: number;
  max: number;
  median?: number;
  skewness?: number;
  kurtosis?: number;
  nobs: number;
}

async function executeStatsmodels<T>(
  command: string,
  params?: Record<string, unknown>
): Promise<StatsmodelsResult<T>> {
  try {
    const paramsJson = params ? JSON.stringify(params) : undefined;

    const result = await invoke<string>('execute_statsmodels_analytics', {
      command,
      params: paramsJson,
    });

    return JSON.parse(result) as StatsmodelsResult<T>;
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

export const statsmodelsService = {
  listAvailableAnalyses(): Promise<StatsmodelsResult> {
    return executeStatsmodels('list');
  },

  fitARIMA(params: ARIMAParams): Promise<StatsmodelsResult<ARIMAResult>> {
    return executeStatsmodels('arima', params);
  },

  forecastARIMA(params: ARIMAForecastParams): Promise<StatsmodelsResult<ForecastResult>> {
    return executeStatsmodels('arima_forecast', params);
  },

  fitSARIMAX(params: SARIMAXParams): Promise<StatsmodelsResult<ARIMAResult>> {
    return executeStatsmodels('sarimax', params);
  },

  fitExponentialSmoothing(params: ExponentialSmoothingParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('exponential_smoothing', params);
  },

  stlDecompose(params: STLDecomposeParams): Promise<StatsmodelsResult<STLResult>> {
    return executeStatsmodels('stl_decompose', params);
  },

  adfTest(params: StationarityTestParams): Promise<StatsmodelsResult<StationarityResult>> {
    return executeStatsmodels('adf_test', params);
  },

  kpssTest(params: StationarityTestParams): Promise<StatsmodelsResult<StationarityResult>> {
    return executeStatsmodels('kpss_test', params);
  },

  calculateACF(params: ACFPACFParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('acf', params);
  },

  calculatePACF(params: ACFPACFParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('pacf', params);
  },

  ljungBoxTest(params: StationarityTestParams & { lags?: number }): Promise<StatsmodelsResult> {
    return executeStatsmodels('ljung_box', params);
  },

  fitOLS(params: OLSParams): Promise<StatsmodelsResult<RegressionResult>> {
    return executeStatsmodels('ols', params);
  },

  regressionDiagnostics(params: OLSParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('regression_diagnostics', params);
  },

  fitLogit(params: OLSParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('logit', params);
  },

  ttestIndependent(params: TTestIndParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('ttest_ind', params);
  },

  ttestOneSample(params: TTestOneSampleParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('ttest_1samp', params);
  },

  jarqueBera(params: StationarityTestParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('jarque_bera', params);
  },

  descriptiveStats(params: StationarityTestParams): Promise<StatsmodelsResult<DescriptiveResult>> {
    return executeStatsmodels('descriptive', params);
  },

  ttestPower(params: PowerAnalysisParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('ttest_power', params);
  },

  performPCA(params: PCAParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('pca', params);
  },

  kernelDensity(params: StationarityTestParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('kde', params);
  },

  lowessSmooth(params: LOWESSParams): Promise<StatsmodelsResult> {
    return executeStatsmodels('lowess', params);
  },

  executeCustomCommand<T>(
    command: string,
    params?: Record<string, unknown>
  ): Promise<StatsmodelsResult<T>> {
    return executeStatsmodels<T>(command, params);
  },
};

export default statsmodelsService;
