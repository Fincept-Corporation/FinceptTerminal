// File: src/components/tabs/quantlib-stochastic/quantlibStochasticApi.ts
// API service for QuantLib Stochastic endpoints (36 endpoints)

const BASE_URL = 'https://api.fincept.in';

let _apiKey: string | null = null;
export function setStochasticApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;
  const res = await fetch(`${BASE_URL}${path}`, { method: 'POST', headers, body: JSON.stringify(body) });
  if (!res.ok) { const text = await res.text(); throw new Error(`API ${res.status}: ${text}`); }
  return res.json();
}

// === PROCESSES ===
export const processesApi = {
  gbmSimulate: (S0: number, mu: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/gbm/simulate', { S0, mu, sigma, T, n_steps, n_paths, seed }),
  gbmProperties: (S0: number, mu: number, sigma: number, T: number, n_steps?: number, n_paths?: number) =>
    apiCall('/quantlib/stochastic/gbm/properties', { S0, mu, sigma, T, n_steps, n_paths }),
  ouSimulate: (X0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/ou/simulate', { X0, kappa, theta, sigma, T, n_steps, n_paths, seed }),
  cirSimulate: (r0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/cir/simulate', { r0, kappa, theta, sigma, T, n_steps, n_paths, seed }),
  cirBondPrice: (r0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number) =>
    apiCall('/quantlib/stochastic/cir/bond-price', { r0, kappa, theta, sigma, T, n_steps, n_paths }),
  vasicekSimulate: (r0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/vasicek/simulate', { r0, kappa, theta, sigma, T, n_steps, n_paths, seed }),
  vasicekBondPrice: (r0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number) =>
    apiCall('/quantlib/stochastic/vasicek/bond-price', { r0, kappa, theta, sigma, T, n_steps, n_paths }),
  hestonSimulate: (S0: number, v0: number, r: number, kappa: number, theta: number, sigma_v: number, rho: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/heston/simulate', { S0, v0, r, kappa, theta, sigma_v, rho, T, n_steps, n_paths, seed }),
  mertonSimulate: (S0: number, mu: number, sigma: number, lam: number, jump_mean: number, jump_std: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/merton/simulate', { S0, mu, sigma, lam, jump_mean, jump_std, T, n_steps, n_paths, seed }),
  poissonSimulate: (lam: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/poisson/simulate', { lam, T, n_steps, n_paths, seed }),
  wienerSimulate: (T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/wiener/simulate', { T, n_steps, n_paths, seed }),
  brownianBridge: (T: number, a?: number, b?: number, n_steps?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/brownian-bridge/simulate', { T, a, b, n_steps, seed }),
  correlatedBm: (n_assets: number, correlation_matrix: number[][], T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/correlated-bm/simulate', { n_assets, correlation_matrix, T, n_steps, n_paths, seed }),
  vgSimulate: (S0: number, r: number, sigma: number, theta_vg: number, nu: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/variance-gamma/simulate', { S0, r, sigma, theta_vg, nu, T, n_steps, n_paths, seed }),
};

// === EXACT ===
export const exactApi = {
  gbm: (S0: number, mu: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/exact/gbm', { S0, mu, sigma, T, n_steps, n_paths, seed }),
  cir: (r0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/exact/cir', { r0, kappa, theta, sigma, T, n_steps, n_paths, seed }),
  ou: (X0: number, kappa: number, theta: number, sigma: number, T: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/exact/ou', { X0, kappa, theta, sigma, T, n_steps, n_paths, seed }),
  heston: (S0?: number, v0?: number, kappa?: number, theta?: number, xi?: number, rho?: number, r?: number, T?: number, n_steps?: number, n_paths?: number) =>
    apiCall('/quantlib/stochastic/exact/heston', { S0, v0, kappa, theta, xi, rho, r, T, n_steps, n_paths }),
};

// === SIMULATION ===
export const simulationApi = {
  eulerMaruyama: (x0?: number, T?: number, n_steps?: number, n_paths?: number, mu?: number, sigma?: number, seed?: number) =>
    apiCall('/quantlib/stochastic/simulation/euler-maruyama', { x0, T, n_steps, n_paths, mu, sigma, seed }),
  eulerMaruyamaNd: (x0: number[], T?: number, n_steps?: number, mu?: number[], sigma?: number[]) =>
    apiCall('/quantlib/stochastic/simulation/euler-maruyama-nd', { x0, T, n_steps, mu, sigma }),
  milstein: (x0?: number, T?: number, n_steps?: number, n_paths?: number, mu?: number, sigma?: number) =>
    apiCall('/quantlib/stochastic/simulation/milstein', { x0, T, n_steps, n_paths, mu, sigma }),
  milsteinNd: (x0: number[], T?: number, n_steps?: number, mu?: number[], sigma?: number[]) =>
    apiCall('/quantlib/stochastic/simulation/milstein-nd', { x0, T, n_steps, mu, sigma }),
  multilevelMc: (x0?: number, T?: number, mu?: number, sigma?: number, n_levels?: number, n_samples_per_level?: number) =>
    apiCall('/quantlib/stochastic/simulation/multilevel-mc', { x0, T, mu, sigma, n_levels, n_samples_per_level }),
};

// === SAMPLING ===
export const samplingApi = {
  sobol: (dim: number, n: number, skip?: number) =>
    apiCall('/quantlib/stochastic/sampling/sobol', { dim, n, skip }),
  antithetic: (n: number, seed?: number) =>
    apiCall('/quantlib/stochastic/sampling/antithetic', { n, seed }),
  correlatedNormals: (rho: number, n: number, seed?: number) =>
    apiCall('/quantlib/stochastic/sampling/correlated-normals', { rho, n, seed }),
  multivariateNormal: (mean: number[], cov: number[][]) =>
    apiCall('/quantlib/stochastic/sampling/multivariate-normal', { mean, cov }),
  jump: (n?: number, intensity?: number, jump_mean?: number, jump_std?: number, distribution?: string) =>
    apiCall('/quantlib/stochastic/sampling/jump', { n, intensity, jump_mean, jump_std, distribution }),
  distribution: (n?: number, distribution?: string, params?: Record<string, any>) =>
    apiCall('/quantlib/stochastic/sampling/distribution', { n, distribution, params }),
};

// === THEORY ===
export const theoryApi = {
  quadraticVariation: (path: number[], times: number[]) =>
    apiCall('/quantlib/stochastic/theory/quadratic-variation', { path, times }),
  covariation: (path_X: number[], path_Y: number[], times: number[]) =>
    apiCall('/quantlib/stochastic/theory/covariation', { path_X, path_Y, times }),
  martingaleTest: (paths: number[][], times: number[], confidence?: number) =>
    apiCall('/quantlib/stochastic/theory/martingale-test', { paths, times, confidence }),
  riskNeutralDrift: (mu: number, r: number, sigma: number) =>
    apiCall('/quantlib/stochastic/theory/girsanov/risk-neutral-drift', { mu, r, sigma }),
  measureChange: (paths: number[][], times: number[], mu?: number, r?: number, sigma?: number) =>
    apiCall('/quantlib/stochastic/theory/girsanov/measure-change', { paths, times, mu, r, sigma }),
  itoLemma: (path: number[], times: number[]) =>
    apiCall('/quantlib/stochastic/theory/ito-lemma', { path, times }),
  itoProductRule: (path_X: number[], path_Y: number[], times: number[]) =>
    apiCall('/quantlib/stochastic/theory/ito-product-rule', { path_X, path_Y, times }),
};
