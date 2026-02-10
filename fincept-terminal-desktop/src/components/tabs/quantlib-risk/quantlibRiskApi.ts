// File: src/components/tabs/quantlib-risk/quantlibRiskApi.ts
// 25 endpoints — VaR, stress testing, EVT, XVA, sensitivities, tail risk, portfolio risk

const BASE_URL = 'https://finceptbackend.share.zrok.io';
let _apiKey: string | null = null;
export function setRiskModApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── VaR (6) ────────────────────────────────────────────────────
export const varApi = {
  parametric: (portfolio_value: number, volatility: number, confidence = 0.99, horizon = 1) =>
    apiCall('/quantlib/risk/var/parametric', { portfolio_value, volatility, confidence, horizon }),
  historical: (returns: number[], confidence = 0.99, portfolio_value = 1000000) =>
    apiCall('/quantlib/risk/var/historical', { returns, confidence, portfolio_value }),
  component: (returns_by_factor: Record<string, number[]>, weights: Record<string, number>, confidence = 0.99, portfolio_value = 1000000) =>
    apiCall('/quantlib/risk/var/component', { returns_by_factor, weights, confidence, portfolio_value }),
  incremental: (portfolio_returns: number[], new_position_returns: number[], position_weight: number, confidence = 0.99, portfolio_value = 1000000) =>
    apiCall('/quantlib/risk/var/incremental', { portfolio_returns, new_position_returns, position_weight, confidence, portfolio_value }),
  marginal: (portfolio_returns: number[], new_position_returns: number[], position_weight: number, confidence = 0.99, portfolio_value = 1000000) =>
    apiCall('/quantlib/risk/var/marginal', { portfolio_returns, new_position_returns, position_weight, confidence, portfolio_value }),
  esOptimization: (returns: number[][], confidence = 0.95, target_return?: number) =>
    apiCall('/quantlib/risk/var/es-optimization', { returns, confidence, target_return }),
};

// ── Stress & Backtest (2) ──────────────────────────────────────
export const stressApi = {
  scenario: (name: string, factor_shocks: Record<string, number>, description = '') =>
    apiCall('/quantlib/risk/stress/scenario', { name, factor_shocks, description }),
  backtest: (actual_returns: number[], var_estimates: number[], confidence = 0.99) =>
    apiCall('/quantlib/risk/backtest', { actual_returns, var_estimates, confidence }),
};

// ── EVT (3) ────────────────────────────────────────────────────
export const evtApi = {
  gpd: (data: number[], threshold?: number, quantile = 0.95) =>
    apiCall('/quantlib/risk/evt/gpd', { data, threshold, quantile }),
  gev: (data: number[], fit_method = 'mle', return_periods?: number[]) =>
    apiCall('/quantlib/risk/evt/gev', { data, fit_method, return_periods }),
  hill: (data: number[], threshold?: number, quantile = 0.95) =>
    apiCall('/quantlib/risk/evt/hill', { data, threshold, quantile }),
};

// ── Tail Risk (2) ──────────────────────────────────────────────
export const tailRiskApi = {
  comprehensive: (returns: number[], confidence = 0.99, portfolio_value = 1000000) =>
    apiCall('/quantlib/risk/tail-risk/comprehensive', { returns, confidence, portfolio_value }),
  tailDependence: (body: any) =>
    apiCall('/quantlib/risk/tail-risk/tail-dependence', body),
};

// ── XVA (2) ────────────────────────────────────────────────────
export const xvaApi = {
  cva: (exposure_paths: number[][], counterparty_spread: number, recovery_rate = 0.4, own_spread?: number, funding_spread?: number) =>
    apiCall('/quantlib/risk/xva/cva', { exposure_paths, counterparty_spread, recovery_rate, own_spread, funding_spread }),
  pfe: (exposure_paths: number[][], counterparty_spread: number, recovery_rate = 0.4) =>
    apiCall('/quantlib/risk/xva/pfe', { exposure_paths, counterparty_spread, recovery_rate }),
};

// ── Correlation Stress (1) ─────────────────────────────────────
export const correlationApi = {
  stress: (correlation_matrix: number[][], stress_type = 'crisis', volatilities?: number[], weights?: number[]) =>
    apiCall('/quantlib/risk/correlation-stress', { correlation_matrix, stress_type, volatilities, weights }),
};

// ── Sensitivities (6) ──────────────────────────────────────────
export const sensitivityApi = {
  greeks: (S = 100, K = 100, T = 1, r = 0.05, sigma = 0.2, q = 0) =>
    apiCall('/quantlib/risk/sensitivities/greeks', { S, K, T, r, sigma, q }),
  bucketDelta: (positions: any[], bump_size = 0.0001) =>
    apiCall('/quantlib/risk/sensitivities/bucket-delta', { positions, bump_size }),
  crossGamma: (S = 100, K = 100, T = 1, r = 0.05, sigma = 0.2, bump = 0.01) =>
    apiCall('/quantlib/risk/sensitivities/cross-gamma', { S, K, T, r, sigma, bump }),
  keyRateDuration: (cashflows: number[], times: number[], curve_rates: number[], bump_size = 0.0001) =>
    apiCall('/quantlib/risk/sensitivities/key-rate-duration', { cashflows, times, curve_rates, bump_size }),
  parallelShift: (cashflows: number[], times: number[], curve_rates: number[], bump_size = 0.0001) =>
    apiCall('/quantlib/risk/sensitivities/parallel-shift', { cashflows, times, curve_rates, bump_size }),
  twist: (cashflows: number[], times: number[], curve_rates: number[], bump_size = 0.0001) =>
    apiCall('/quantlib/risk/sensitivities/twist', { cashflows, times, curve_rates, bump_size }),
};

// ── Portfolio Risk (3) ─────────────────────────────────────────
export const portfolioRiskApi = {
  optimalHedge: (portfolio_delta: number, hedge_delta: number, portfolio_gamma = 0, hedge_gamma = 0, target_delta = 0, target_gamma = 0) =>
    apiCall('/quantlib/risk/portfolio-risk/optimal-hedge', { portfolio_delta, hedge_delta, portfolio_gamma, hedge_gamma, target_delta, target_gamma }),
  exposureProfile: (paths: number[][], confidence = 0.95) =>
    apiCall('/quantlib/risk/portfolio-risk/exposure-profile', { paths, confidence }),
};
