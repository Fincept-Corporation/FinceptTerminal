// File: src/components/tabs/quantlib-statistics/quantlibStatisticsApi.ts
// API service for QuantLib Statistics endpoints (52 endpoints)

const BASE_URL = 'https://api.fincept.in';

let _apiKey: string | null = null;
export function setStatisticsApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;
  const res = await fetch(`${BASE_URL}${path}`, { method: 'POST', headers, body: JSON.stringify(body) });
  if (!res.ok) { const text = await res.text(); throw new Error(`API ${res.status}: ${text}`); }
  return res.json();
}

// === CONTINUOUS DISTRIBUTIONS ===

// Normal
export const normalApi = {
  properties: (mu?: number, sigma?: number) =>
    apiCall('/quantlib/statistics/distributions/normal/properties', { mu, sigma }),
  pdf: (mu?: number, sigma?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/normal/pdf', { mu, sigma, x }),
  cdf: (mu?: number, sigma?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/normal/cdf', { mu, sigma, x }),
  ppf: (mu?: number, sigma?: number, p?: number) =>
    apiCall('/quantlib/statistics/distributions/normal/ppf', { mu, sigma, p }),
};

// Lognormal
export const lognormalApi = {
  properties: (mu?: number, sigma?: number) =>
    apiCall('/quantlib/statistics/distributions/lognormal/properties', { mu, sigma }),
  pdf: (mu?: number, sigma?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/lognormal/pdf', { mu, sigma, x }),
  cdf: (mu?: number, sigma?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/lognormal/cdf', { mu, sigma, x }),
  ppf: (mu?: number, sigma?: number, p?: number) =>
    apiCall('/quantlib/statistics/distributions/lognormal/ppf', { mu, sigma, p }),
};

// Student-t
export const studentTApi = {
  properties: (df: number, mu?: number, sigma?: number) =>
    apiCall('/quantlib/statistics/distributions/student-t/properties', { df, mu, sigma }),
  pdf: (df: number, mu?: number, sigma?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/student-t/pdf', { df, mu, sigma, x }),
  cdf: (df: number, mu?: number, sigma?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/student-t/cdf', { df, mu, sigma, x }),
};

// Chi-Squared
export const chiSquaredApi = {
  properties: (df: number) =>
    apiCall('/quantlib/statistics/distributions/chi-squared/properties', { df }),
  pdf: (df: number, x: number) =>
    apiCall('/quantlib/statistics/distributions/chi-squared/pdf', { df, x }),
  cdf: (df: number, x: number) =>
    apiCall('/quantlib/statistics/distributions/chi-squared/cdf', { df, x }),
};

// Gamma
export const gammaApi = {
  properties: (alpha: number, beta?: number) =>
    apiCall('/quantlib/statistics/distributions/gamma/properties', { alpha, beta }),
  pdf: (alpha: number, beta?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/gamma/pdf', { alpha, beta, x }),
  cdf: (alpha: number, beta?: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/gamma/cdf', { alpha, beta, x }),
};

// Beta
export const betaApi = {
  properties: (alpha: number, beta: number) =>
    apiCall('/quantlib/statistics/distributions/beta/properties', { alpha, beta }),
  pdf: (alpha: number, beta: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/beta/pdf', { alpha, beta, x }),
  cdf: (alpha: number, beta: number, x?: number) =>
    apiCall('/quantlib/statistics/distributions/beta/cdf', { alpha, beta, x }),
};

// Exponential
export const exponentialApi = {
  properties: (rate: number) =>
    apiCall('/quantlib/statistics/distributions/exponential/properties', { rate }),
  pdf: (rate: number, x: number) =>
    apiCall('/quantlib/statistics/distributions/exponential/pdf', { rate, x }),
  cdf: (rate: number, x: number) =>
    apiCall('/quantlib/statistics/distributions/exponential/cdf', { rate, x }),
  ppf: (rate: number, p: number) =>
    apiCall('/quantlib/statistics/distributions/exponential/ppf', { rate, p }),
};

// F Distribution
export const fDistApi = {
  properties: (df1: number, df2: number) =>
    apiCall('/quantlib/statistics/distributions/f/properties', { df1, df2 }),
  pdf: (df1: number, df2: number, x: number) =>
    apiCall('/quantlib/statistics/distributions/f/pdf', { df1, df2, x }),
};

// === DISCRETE DISTRIBUTIONS ===

// Binomial
export const binomialApi = {
  properties: (n: number, p: number) =>
    apiCall('/quantlib/statistics/distributions/binomial/properties', { n, p }),
  pmf: (n: number, p: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/binomial/pmf', { n, p, k }),
  cdf: (n: number, p: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/binomial/cdf', { n, p, k }),
};

// Poisson
export const poissonApi = {
  properties: (lam: number) =>
    apiCall('/quantlib/statistics/distributions/poisson/properties', { lam }),
  pmf: (lam: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/poisson/pmf', { lam, k }),
  cdf: (lam: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/poisson/cdf', { lam, k }),
};

// Geometric
export const geometricApi = {
  properties: (p: number) =>
    apiCall('/quantlib/statistics/distributions/geometric/properties', { p }),
  pmf: (p: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/geometric/pmf', { p, k }),
  cdf: (p: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/geometric/cdf', { p, k }),
  ppf: (p: number, q: number) =>
    apiCall('/quantlib/statistics/distributions/geometric/ppf', { p, q }),
};

// Hypergeometric
export const hypergeometricApi = {
  properties: (N: number, K: number, n: number) =>
    apiCall('/quantlib/statistics/distributions/hypergeometric/properties', { N, K, n }),
  pmf: (N: number, K: number, n: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/hypergeometric/pmf', { N, K, n, k }),
  cdf: (N: number, K: number, n: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/hypergeometric/cdf', { N, K, n, k }),
};

// Negative Binomial
export const negBinomialApi = {
  properties: (r: number, p: number) =>
    apiCall('/quantlib/statistics/distributions/negative-binomial/properties', { r, p }),
  pmf: (r: number, p: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/negative-binomial/pmf', { r, p, k }),
  cdf: (r: number, p: number, k: number) =>
    apiCall('/quantlib/statistics/distributions/negative-binomial/cdf', { r, p, k }),
};

// PGF
export const pgfApi = {
  evaluate: (distribution: string, z: number, n?: number, p?: number, lam?: number) =>
    apiCall('/quantlib/statistics/distributions/pgf', { distribution, z, n, p, lam }),
};

// === TIME SERIES ===
export const timeseriesApi = {
  arFit: (data: number[], order?: number) =>
    apiCall('/quantlib/statistics/timeseries/ar/fit', { data, order }),
  arForecast: (data: number[], order?: number, steps?: number) =>
    apiCall('/quantlib/statistics/timeseries/ar/forecast', { data, order, steps }),
  maFit: (data: number[], order?: number) =>
    apiCall('/quantlib/statistics/timeseries/ma/fit', { data, order }),
  arimaFit: (data: number[], p?: number, d?: number, q?: number) =>
    apiCall('/quantlib/statistics/timeseries/arima/fit', { data, p, d, q }),
  arimaForecast: (data: number[], p?: number, d?: number, q?: number, steps?: number) =>
    apiCall('/quantlib/statistics/timeseries/arima/forecast', { data, p, d, q, steps }),
  garchFit: (returns: number[], p?: number, q?: number) =>
    apiCall('/quantlib/statistics/timeseries/garch/fit', { returns, p, q }),
  garchForecast: (returns: number[], p?: number, q?: number, steps?: number) =>
    apiCall('/quantlib/statistics/timeseries/garch/forecast', { returns, p, q, steps }),
  egarchFit: (returns: number[], p?: number, q?: number) =>
    apiCall('/quantlib/statistics/timeseries/egarch/fit', { returns, p, q }),
  gjrGarchFit: (returns: number[], p?: number, q?: number) =>
    apiCall('/quantlib/statistics/timeseries/gjr-garch/fit', { returns, p, q }),
};
