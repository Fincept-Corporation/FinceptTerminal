// File: src/components/tabs/quantlib-portfolio/quantlibPortfolioApi.ts
// API service for QuantLib Portfolio endpoints (15 endpoints)

const BASE_URL = 'https://api.fincept.in';

let _apiKey: string | null = null;
export function setPortfolioApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;
  const res = await fetch(`${BASE_URL}${path}`, { method: 'POST', headers, body: JSON.stringify(body) });
  if (!res.ok) { const text = await res.text(); throw new Error(`API ${res.status}: ${text}`); }
  return res.json();
}

// === OPTIMIZE (4) ===
export const optimizeApi = {
  minVariance: (expected_returns: number[], covariance_matrix: number[][], rf_rate?: number) =>
    apiCall('/quantlib/portfolio/optimize/min-variance', { expected_returns, covariance_matrix, rf_rate }),
  maxSharpe: (expected_returns: number[], covariance_matrix: number[][], rf_rate?: number) =>
    apiCall('/quantlib/portfolio/optimize/max-sharpe', { expected_returns, covariance_matrix, rf_rate }),
  targetReturn: (expected_returns: number[], covariance_matrix: number[][], target_return: number) =>
    apiCall('/quantlib/portfolio/optimize/target-return', { expected_returns, covariance_matrix, target_return }),
  efficientFrontier: (expected_returns: number[], covariance_matrix: number[][], n_points?: number) =>
    apiCall('/quantlib/portfolio/optimize/efficient-frontier', { expected_returns, covariance_matrix, n_points }),
};

// === RISK (8) ===
export const riskApi = {
  comprehensive: (weights: number[], covariance_matrix: number[][], returns?: number[], benchmark_returns?: number[], rf_rate?: number) =>
    apiCall('/quantlib/portfolio/risk/comprehensive', { weights, covariance_matrix, returns, benchmark_returns, rf_rate }),
  var: (weights: number[], covariance_matrix: number[][], confidence?: number, horizon?: number, portfolio_value?: number, method?: string) =>
    apiCall('/quantlib/portfolio/risk/var', { weights, covariance_matrix, confidence, horizon, portfolio_value, method }),
  cvar: (weights: number[], covariance_matrix: number[][], confidence?: number, horizon?: number, portfolio_value?: number, method?: string) =>
    apiCall('/quantlib/portfolio/risk/cvar', { weights, covariance_matrix, confidence, horizon, portfolio_value, method }),
  contribution: (weights: number[], covariance_matrix: number[][]) =>
    apiCall('/quantlib/portfolio/risk/contribution', { weights, covariance_matrix }),
  ratios: (returns: number[], rf_rate?: number, benchmark_returns?: number[], target_return?: number) =>
    apiCall('/quantlib/portfolio/risk/ratios', { returns, rf_rate, benchmark_returns, target_return }),
  incrementalVar: (weights: number[], covariance_matrix: number[][], new_weight?: number, asset_index?: number, confidence?: number) =>
    apiCall('/quantlib/portfolio/risk/incremental-var', { weights, covariance_matrix, new_weight, asset_index, confidence }),
  inverseVolWeights: (covariance_matrix: number[][]) =>
    apiCall('/quantlib/portfolio/risk/inverse-volatility', { covariance_matrix }),
  portfolioComprehensive: (returns: number[][], weights: number[], confidence?: number, risk_free_rate?: number) =>
    apiCall('/quantlib/portfolio/risk/portfolio-comprehensive', { returns, weights, confidence, risk_free_rate }),
};

// === BLACK-LITTERMAN (2) ===
export const blackLittermanApi = {
  equilibrium: (covariance_matrix: number[][], market_caps: number[], risk_aversion?: number) =>
    apiCall('/quantlib/portfolio/black-litterman/equilibrium', { covariance_matrix, market_caps, risk_aversion }),
  posterior: (covariance_matrix: number[][], market_caps: number[], risk_aversion?: number) =>
    apiCall('/quantlib/portfolio/black-litterman/posterior', { covariance_matrix, market_caps, risk_aversion }),
};

// === RISK PARITY (1) ===
export const riskParityApi = {
  optimize: (covariance_matrix: number[][], method?: string) =>
    apiCall('/quantlib/portfolio/risk-parity', { covariance_matrix, method }),
};
