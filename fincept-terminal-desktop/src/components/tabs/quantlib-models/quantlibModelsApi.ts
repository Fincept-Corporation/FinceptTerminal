// File: src/components/tabs/quantlib-models/quantlibModelsApi.ts
// API service for QuantLib Models endpoints

const BASE_URL = 'https://finceptbackend.share.zrok.io';

let _apiKey: string | null = null;
export function setModelsApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;

  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers,
    body: JSON.stringify(body),
  });
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`API ${res.status}: ${text}`);
  }
  return res.json();
}

// === SHORT RATE ===
export const shortRateApi = {
  bondPrice: (model?: string, kappa?: number, theta?: number, sigma?: number, r0?: number, maturities?: number[]) =>
    apiCall('/quantlib/models/short-rate/bond-price', { model, kappa, theta, sigma, r0, maturities }),
  yieldCurve: (model?: string, kappa?: number, theta?: number, sigma?: number, r0?: number, maturities?: number[]) =>
    apiCall('/quantlib/models/short-rate/yield-curve', { model, kappa, theta, sigma, r0, maturities }),
  monteCarlo: (model?: string, kappa?: number, theta?: number, sigma?: number, r0?: number, T?: number, n_steps?: number, n_paths?: number, seed?: number) =>
    apiCall('/quantlib/models/short-rate/monte-carlo', { model, kappa, theta, sigma, r0, T, n_steps, n_paths, seed }),
  bondOption: (model?: string, kappa?: number, theta?: number, sigma?: number, r0?: number, T?: number) =>
    apiCall('/quantlib/models/short-rate/bond-option', { model, kappa, theta, sigma, r0, T }),
};

// === HULL-WHITE ===
export const hullWhiteApi = {
  calibrate: (kappa?: number, sigma?: number, r0?: number, market_tenors?: number[], market_rates?: number[]) =>
    apiCall('/quantlib/models/hull-white/calibrate', { kappa, sigma, r0, market_tenors, market_rates }),
};

// === HESTON ===
export const hestonApi = {
  price: (S0: number, v0: number, r: number, kappa: number, theta: number, sigma_v: number, rho: number, strike: number, T: number, option_type?: string) =>
    apiCall('/quantlib/models/heston/price', { S0, v0, r, kappa, theta, sigma_v, rho, strike, T, option_type }),
  impliedVol: (S0: number, v0: number, r: number, kappa: number, theta: number, sigma_v: number, rho: number, strike: number, T: number, option_type?: string) =>
    apiCall('/quantlib/models/heston/implied-vol', { S0, v0, r, kappa, theta, sigma_v, rho, strike, T, option_type }),
  monteCarlo: (S0: number, v0: number, r: number, kappa: number, theta: number, sigma_v: number, rho: number, strike: number, T: number, option_type?: string, n_paths?: number, n_steps?: number, seed?: number) =>
    apiCall('/quantlib/models/heston/monte-carlo', { S0, v0, r, kappa, theta, sigma_v, rho, strike, T, option_type, n_paths, n_steps, seed }),
};

// === MERTON ===
export const mertonApi = {
  price: (S: number, K: number, T: number, r: number, sigma: number, lambda_jump: number, mu_jump: number, sigma_jump: number, q?: number) =>
    apiCall('/quantlib/models/merton/price', { S, K, T, r, sigma, lambda_jump, mu_jump, sigma_jump, q }),
  fft: (S: number, K: number, T: number, r: number, sigma: number, lambda_jump: number, mu_jump: number, sigma_jump: number, q?: number) =>
    apiCall('/quantlib/models/merton/fft', { S, K, T, r, sigma, lambda_jump, mu_jump, sigma_jump, q }),
};

// === KOU ===
export const kouApi = {
  price: (S: number, K: number, T: number, r: number, sigma: number, lambda_jump: number, p: number, eta1: number, eta2: number, q?: number) =>
    apiCall('/quantlib/models/kou/price', { S, K, T, r, sigma, lambda_jump, p, eta1, eta2, q }),
};

// === VARIANCE GAMMA ===
export const varianceGammaApi = {
  price: (S: number, K: number, T: number, r: number, sigma: number, theta_vg: number, nu: number, q?: number) =>
    apiCall('/quantlib/models/variance-gamma/price', { S, K, T, r, sigma, theta_vg, nu, q }),
};

// === DUPIRE ===
export const dupireApi = {
  price: (spot: number, r?: number, q?: number, sigma?: number, strike?: number, T?: number, option_type?: string) =>
    apiCall('/quantlib/models/dupire/price', { spot, r, q, sigma, strike, T, option_type }),
};

// === SVI ===
export const sviApi = {
  calibrate: (strikes: number[], market_vols: number[], forward: number, T: number) =>
    apiCall('/quantlib/models/svi/calibrate', { strikes, market_vols, forward, T }),
};
