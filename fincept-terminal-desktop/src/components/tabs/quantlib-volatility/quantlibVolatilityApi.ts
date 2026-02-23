// File: src/components/tabs/quantlib-volatility/quantlibVolatilityApi.ts
// API service for QuantLib Volatility endpoints

const BASE_URL = 'https://api.fincept.in';

let _apiKey: string | null = null;
export function setVolatilityApiKey(key: string | null) { _apiKey = key; }

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

// === SURFACE ===
export const surfaceApi = {
  flat: (volatility: number, query_expiry: number, query_strike: number) =>
    apiCall('/quantlib/volatility/surface/flat', { volatility, query_expiry, query_strike }),
  termStructure: (expiries: number[], volatilities: number[], query_expiry: number) =>
    apiCall('/quantlib/volatility/surface/term-structure', { expiries, volatilities, query_expiry }),
  grid: (expiries: number[], strikes: number[], vol_matrix: number[][], query_expiry: number, query_strike: number) =>
    apiCall('/quantlib/volatility/surface/grid', { expiries, strikes, vol_matrix, query_expiry, query_strike }),
  smile: (expiries: number[], strikes: number[], vol_matrix: number[][], query_expiry: number, query_strikes: number[]) =>
    apiCall('/quantlib/volatility/surface/smile', { expiries, strikes, vol_matrix, query_expiry, query_strikes }),
  totalVariance: (expiries: number[], strikes: number[], vol_matrix: number[][], query_expiry: number, query_strike: number) =>
    apiCall('/quantlib/volatility/surface/total-variance', { expiries, strikes, vol_matrix, query_expiry, query_strike }),
  fromPoints: (tenors: number[], strikes: number[], vols: number[], forwards?: number[], expiry_grid?: number[], strike_grid?: number[]) =>
    apiCall('/quantlib/volatility/surface/from-points', { tenors, strikes, vols, forwards, expiry_grid, strike_grid }),
};

// === SABR ===
export const sabrApi = {
  impliedVol: (forward: number, strike: number, expiry: number, alpha: number, beta: number, rho: number, nu: number) =>
    apiCall('/quantlib/volatility/sabr/implied-vol', { forward, strike, expiry, alpha, beta, rho, nu }),
  normalVol: (forward: number, strike: number, expiry: number, alpha: number, beta: number, rho: number, nu: number) =>
    apiCall('/quantlib/volatility/sabr/normal-vol', { forward, strike, expiry, alpha, beta, rho, nu }),
  smile: (forward: number, expiry: number, alpha: number, beta: number, rho: number, nu: number, strikes: number[]) =>
    apiCall('/quantlib/volatility/sabr/smile', { forward, expiry, alpha, beta, rho, nu, strikes }),
  calibrate: (forward: number, expiry: number, strikes: number[], market_vols: number[], beta?: number, weights?: number[], method?: string) =>
    apiCall('/quantlib/volatility/sabr/calibrate', { forward, expiry, strikes, market_vols, beta, weights, method }),
  density: (forward: number, expiry: number, alpha: number, beta: number, rho: number, nu: number, strikes: number[]) =>
    apiCall('/quantlib/volatility/sabr/density', { forward, expiry, alpha, beta, rho, nu, strikes }),
  dynamics: (forward: number, expiry: number, alpha: number, beta: number, rho: number, nu: number, strike: number, forward_shift?: number) =>
    apiCall('/quantlib/volatility/sabr/dynamics', { forward, expiry, alpha, beta, rho, nu, strike, forward_shift }),
};

// === LOCAL VOL ===
export const localVolApi = {
  constant: (volatility: number, spot: number, time: number) =>
    apiCall('/quantlib/volatility/local-vol/constant', { volatility, spot, time }),
  impliedToLocal: (spot: number, rate: number, strike: number, expiry: number, implied_vol: number, dk?: number, dt?: number) =>
    apiCall('/quantlib/volatility/local-vol/implied-to-local', { spot, rate, strike, expiry, implied_vol, dk, dt }),
};
