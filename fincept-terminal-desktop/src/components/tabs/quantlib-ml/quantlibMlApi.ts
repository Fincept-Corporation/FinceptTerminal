// File: src/components/tabs/quantlib-ml/quantlibMlApi.ts
// API service for QuantLib ML endpoints (48 endpoints)

const BASE_URL = 'https://api.fincept.in';

let _apiKey: string | null = null;
export function setMlApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;
  const res = await fetch(`${BASE_URL}${path}`, { method: 'POST', headers, body: JSON.stringify(body) });
  if (!res.ok) { const text = await res.text(); throw new Error(`API ${res.status}: ${text}`); }
  return res.json();
}

// === CREDIT ===
export const creditApi = {
  logisticRegression: (X: number[][], y: number[], predict_X?: number[][]) =>
    apiCall('/quantlib/ml/credit/logistic-regression', { X, y, predict_X }),
  discrimination: (y_true: number[], y_score: number[]) =>
    apiCall('/quantlib/ml/credit/discrimination', { y_true, y_score }),
  woeBinning: (feature: number[], target: number[], n_bins?: number) =>
    apiCall('/quantlib/ml/credit/woe-binning', { feature, target, n_bins }),
  migration: (transitions: number[][], periods?: number) =>
    apiCall('/quantlib/ml/credit/migration', { transitions, periods }),
  calibration: (y_true: number[], y_score: number[], method?: string) =>
    apiCall('/quantlib/ml/credit/calibration', { y_true, y_score, method }),
  scorecard: (X: number[][], y: number[], n_bins?: number) =>
    apiCall('/quantlib/ml/credit/scorecard', { X, y, n_bins }),
  performance: (y_true: number[], y_score: number[]) =>
    apiCall('/quantlib/ml/credit/performance', { y_true, y_score }),
  twoStageLgd: (X: number[][], lgd: number[], cure_indicator: number[], predict_X?: number[][]) =>
    apiCall('/quantlib/ml/credit/two-stage-lgd', { X, lgd, cure_indicator, predict_X }),
  betaLgd: (X: number[][], lgd: number[], predict_X?: number[][]) =>
    apiCall('/quantlib/ml/credit/beta-lgd', { X, lgd, predict_X }),
};

// === REGRESSION ===
export const regressionApi = {
  fit: (X: number[][], y: number[], method?: string, alpha?: number, l1_ratio?: number, predict_X?: number[][]) =>
    apiCall('/quantlib/ml/regression/fit', { X, y, method, alpha, l1_ratio, predict_X }),
  tree: (X: number[][], y: number[], method?: string, max_depth?: number, n_estimators?: number, learning_rate?: number, predict_X?: number[][]) =>
    apiCall('/quantlib/ml/regression/tree', { X, y, method, max_depth, n_estimators, learning_rate, predict_X }),
  ead: (X: number[][], y: number[], predict_X?: number[][]) =>
    apiCall('/quantlib/ml/regression/ead', { X, y, predict_X }),
  lgd: (X: number[][], y: number[], predict_X?: number[][]) =>
    apiCall('/quantlib/ml/regression/lgd', { X, y, predict_X }),
  ensemble: (X: number[][], y: number[], method?: string, n_estimators?: number, max_depth?: number, predict_X?: number[][]) =>
    apiCall('/quantlib/ml/regression/ensemble', { X, y, method, n_estimators, max_depth, predict_X }),
};

// === CLUSTERING ===
export const clusteringApi = {
  kmeans: (X: number[][], n_clusters?: number, max_iter?: number) =>
    apiCall('/quantlib/ml/clustering/kmeans', { X, n_clusters, max_iter }),
  pca: (X: number[][], n_components?: number) =>
    apiCall('/quantlib/ml/clustering/pca', { X, n_components }),
  dbscan: (X: number[][], eps?: number, min_samples?: number) =>
    apiCall('/quantlib/ml/clustering/dbscan', { X, eps, min_samples }),
  hierarchical: (X: number[][], n_clusters?: number, linkage?: string) =>
    apiCall('/quantlib/ml/clustering/hierarchical', { X, n_clusters, linkage }),
  isolationForest: (X: number[][], n_trees?: number, contamination?: number) =>
    apiCall('/quantlib/ml/clustering/isolation-forest', { X, n_trees, contamination }),
};

// === PREPROCESSING ===
export const preprocessingApi = {
  scale: (X: number[][], method?: string) =>
    apiCall('/quantlib/ml/preprocessing/scale', { X, method }),
  outliers: (X: number[][], method?: string, threshold?: number) =>
    apiCall('/quantlib/ml/preprocessing/outliers', { X, method, threshold }),
  stationarity: (data: number[], transform?: string) =>
    apiCall('/quantlib/ml/preprocessing/stationarity', { data, transform }),
  transform: (X: number[][], method?: string) =>
    apiCall('/quantlib/ml/preprocessing/transform', { X, method }),
  winsorize: (data: number[], lower?: number, upper?: number) =>
    apiCall('/quantlib/ml/preprocessing/winsorize', { data, lower, upper }),
};

// === FEATURES ===
export const featuresApi = {
  technical: (prices: number[], indicator: string, period?: number, high?: number[], low?: number[], volume?: number[]) =>
    apiCall('/quantlib/ml/features/technical', { prices, indicator, period, high, low, volume }),
  rolling: (data: number[], window: number, statistic?: string, benchmark?: number[]) =>
    apiCall('/quantlib/ml/features/rolling', { data, window, statistic, benchmark }),
  calendar: (dates: string[]) =>
    apiCall('/quantlib/ml/features/calendar', { dates }),
  crossSectional: (data: number[][], method?: string) =>
    apiCall('/quantlib/ml/features/cross-sectional', { data, method }),
  financialRatios: (data: Record<string, any>) =>
    apiCall('/quantlib/ml/features/financial-ratios', { data }),
  lags: (data: number[], n_lags?: number, include_leads?: boolean, include_returns?: boolean, include_log_returns?: boolean) =>
    apiCall('/quantlib/ml/features/lags', { data, n_lags, include_leads, include_returns, include_log_returns }),
};

// === VALIDATION ===
export const validationApi = {
  calibrationReport: (y_true: number[], y_score: number[]) =>
    apiCall('/quantlib/ml/validation/calibration-report', { y_true, y_score }),
  discriminationReport: (y_true: number[], y_score: number[]) =>
    apiCall('/quantlib/ml/validation/discrimination-report', { y_true, y_score }),
  interpretability: (model_predict: string, X: number[][], y: number[], method?: string) =>
    apiCall('/quantlib/ml/validation/interpretability', { model_predict, X, y, method }),
  stability: (expected: number[], actual: number[], n_bins?: number) =>
    apiCall('/quantlib/ml/validation/stability', { expected, actual, n_bins }),
};

// === METRICS ===
export const metricsApi = {
  regression: (y_true: number[], y_pred: number[]) =>
    apiCall('/quantlib/ml/metrics/regression', { y_true, y_pred }),
  classification: (y_true: number[], y_pred: number[]) =>
    apiCall('/quantlib/ml/metrics/classification', { y_true, y_pred }),
};

// === TIMESERIES ===
export const timeseriesApi = {
  featureImportance: (X: number[][], y: number[], n_top?: number) =>
    apiCall('/quantlib/ml/timeseries/feature-importance', { X, y, n_top }),
};

// === HMM / CHANGEPOINT ===
export const hmmApi = {
  fit: (returns: number[], n_states?: number, max_iter?: number) =>
    apiCall('/quantlib/ml/hmm/fit', { returns, n_states, max_iter }),
  changepoint: (data: number[], method?: string, threshold?: number, hazard_rate?: number, drift?: number) =>
    apiCall('/quantlib/ml/changepoint/detect', { data, method, threshold, hazard_rate, drift }),
  garchHybrid: (returns: number[], X?: number[][], p?: number, q?: number, forecast_horizon?: number) =>
    apiCall('/quantlib/ml/garch-hybrid', { returns, X, p, q, forecast_horizon }),
};

// === GP / NN ===
export const gpNnApi = {
  gpCurve: (tenors: number[], rates: number[], query_tenors: number[], length_scale?: number, noise?: number) =>
    apiCall('/quantlib/ml/gp/curve', { tenors, rates, query_tenors, length_scale, noise }),
  gpVolSurface: (strikes: number[], expiries: number[], vols: number[], query_strikes: number[], query_expiries: number[]) =>
    apiCall('/quantlib/ml/gp/vol-surface', { strikes, expiries, vols, query_strikes, query_expiries }),
  nnCurve: (tenors: number[], rates: number[], query_tenors: number[], hidden_layers?: number[], epochs?: number) =>
    apiCall('/quantlib/ml/nn/curve', { tenors, rates, query_tenors, hidden_layers, epochs }),
  nnVolSurface: (strikes: number[], expiries: number[], vols: number[], query_strikes: number[], query_expiries: number[], hidden_layers?: number[], epochs?: number) =>
    apiCall('/quantlib/ml/nn/vol-surface', { strikes, expiries, vols, query_strikes, query_expiries, hidden_layers, epochs }),
  nnPortfolio: (returns: number[][], risk_aversion?: number, hidden_layers?: number[]) =>
    apiCall('/quantlib/ml/nn/portfolio', { returns, risk_aversion, hidden_layers }),
};

// === FACTOR / COVARIANCE ===
export const factorApi = {
  statistical: (returns: number[][], n_factors?: number) =>
    apiCall('/quantlib/ml/factor/statistical', { returns, n_factors }),
  crossSectional: (returns: number[][], characteristics: number[][], n_periods?: number) =>
    apiCall('/quantlib/ml/factor/cross-sectional', { returns, characteristics, n_periods }),
  covarianceEstimate: (returns: number[][], method?: string) =>
    apiCall('/quantlib/ml/covariance/estimate', { returns, method }),
};
