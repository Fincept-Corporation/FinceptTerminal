// File: src/components/tabs/quantlib-curves/quantlibCurvesApi.ts
// 31 endpoints — yield curves, interpolation, shifts, Nelson-Siegel, inflation, multicurve

const BASE_URL = 'https://api.fincept.in';
let _apiKey: string | null = null;
export function setCurvesApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Curve Building & Querying (6) ──────────────────────────────
export const curveApi = {
  build: (reference_date: string, tenors: number[], values: number[], curve_type = 'zero', interpolation = 'linear') =>
    apiCall('/quantlib/curves/build', { reference_date, tenors, values, curve_type, interpolation }),
  zeroRate: (reference_date: string, tenors: number[], values: number[], query_tenor: number, curve_type = 'zero', interpolation = 'linear') =>
    apiCall('/quantlib/curves/zero-rate', { reference_date, tenors, values, query_tenor, curve_type, interpolation }),
  discountFactor: (reference_date: string, tenors: number[], values: number[], query_tenor: number, curve_type = 'zero', interpolation = 'linear') =>
    apiCall('/quantlib/curves/discount-factor', { reference_date, tenors, values, query_tenor, curve_type, interpolation }),
  forwardRate: (reference_date: string, tenors: number[], values: number[], start_tenor: number, end_tenor: number, curve_type = 'zero', interpolation = 'linear') =>
    apiCall('/quantlib/curves/forward-rate', { reference_date, tenors, values, start_tenor, end_tenor, curve_type, interpolation }),
  instantaneousForward: (reference_date: string, tenors: number[], values: number[], query_tenor: number, curve_type = 'zero', interpolation = 'linear') =>
    apiCall('/quantlib/curves/instantaneous-forward', { reference_date, tenors, values, query_tenor, curve_type, interpolation }),
  curvePoints: (reference_date: string, tenors: number[], values: number[], query_tenors: number[], curve_type = 'zero', interpolation = 'linear') =>
    apiCall('/quantlib/curves/curve-points', { reference_date, tenors, values, query_tenors, curve_type, interpolation }),
};

// ── Interpolation (3) ──────────────────────────────────────────
export const interpolationApi = {
  interpolate: (x: number[], y: number[], query_x: number[], method = 'linear') =>
    apiCall('/quantlib/curves/interpolate', { x, y, method, query_x }),
  derivative: (x: number[], y: number[], query_x: number, method = 'linear') =>
    apiCall('/quantlib/curves/interpolate-derivative', { x, y, method, query_x }),
  monotonicityCheck: (x: number[], y: number[], query_x: number[], method = 'linear') =>
    apiCall('/quantlib/curves/monotonicity-check', { x, y, method, query_x }),
};

// ── Curve Transformations (6) ──────────────────────────────────
export const transformApi = {
  parallelShift: (reference_date: string, tenors: number[], values: number[], shift_bps: number, space = 'zero_rate') =>
    apiCall('/quantlib/curves/parallel-shift', { reference_date, tenors, values, shift_bps, space }),
  twist: (reference_date: string, tenors: number[], values: number[], pivot_years: number, short_shift_bps: number, long_shift_bps: number) =>
    apiCall('/quantlib/curves/twist', { reference_date, tenors, values, pivot_years, short_shift_bps, long_shift_bps }),
  butterfly: (reference_date: string, tenors: number[], values: number[], belly_years: number, wing_shift_bps: number, belly_shift_bps: number) =>
    apiCall('/quantlib/curves/butterfly', { reference_date, tenors, values, belly_years, wing_shift_bps, belly_shift_bps }),
  keyRateShift: (reference_date: string, tenors: number[], values: number[], tenor_years: number, shift_bps: number, width_years = 2) =>
    apiCall('/quantlib/curves/key-rate-shift', { reference_date, tenors, values, tenor_years, shift_bps, width_years }),
  roll: (reference_date: string, tenors: number[], values: number[], days: number, preserve_shape = true) =>
    apiCall('/quantlib/curves/roll', { reference_date, tenors, values, days, preserve_shape }),
  scale: (reference_date: string, tenors: number[], values: number[], scale_factor: number) =>
    apiCall('/quantlib/curves/scale', { reference_date, tenors, values, scale_factor }),
};

// ── Curve Construction (3) ─────────────────────────────────────
export const constructionApi = {
  composite: (reference_date: string, base_tenors: number[], base_values: number[], forecast_tenors: number[], forecast_values: number[]) =>
    apiCall('/quantlib/curves/composite', { reference_date, base_tenors, base_values, forecast_tenors, forecast_values }),
  proxy: (reference_date: string, base_tenors: number[], base_values: number[], spread_bps: number) =>
    apiCall('/quantlib/curves/proxy', { reference_date, base_tenors, base_values, spread_bps }),
  timeShift: (reference_date: string, tenors: number[], values: number[], shift_days: number, query_tenors: number[], preserve_shape = true) =>
    apiCall('/quantlib/curves/time-shift', { reference_date, tenors, values, shift_days, preserve_shape, query_tenors }),
};

// ── Smoothness (2) ─────────────────────────────────────────────
export const smoothApi = {
  constrainedFit: (tenors: number[], rates: number[]) =>
    apiCall('/quantlib/curves/constrained-fit', { tenors, rates }),
  smoothnessPenalty: (x: number[], y: number[], query_x: number[], method = 'linear') =>
    apiCall('/quantlib/curves/smoothness-penalty', { x, y, method, query_x }),
};

// ── Nelson-Siegel / NSS (4) ───────────────────────────────────
export const nelsonSiegelApi = {
  fit: (tenors: number[], rates: number[]) =>
    apiCall('/quantlib/curves/nelson-siegel/fit', { tenors, rates }),
  evaluate: (beta0: number, beta1: number, beta2: number, tau: number, query_tenors: number[]) =>
    apiCall('/quantlib/curves/nelson-siegel/evaluate', { beta0, beta1, beta2, tau, query_tenors }),
  nssFit: (tenors: number[], rates: number[]) =>
    apiCall('/quantlib/curves/nss/fit', { tenors, rates }),
  nssEvaluate: (beta0: number, beta1: number, beta2: number, beta3: number, tau1: number, tau2: number, query_tenors: number[]) =>
    apiCall('/quantlib/curves/nss/evaluate', { beta0, beta1, beta2, beta3, tau1, tau2, query_tenors }),
};

// ── Inflation (3) ──────────────────────────────────────────────
export const inflationApi = {
  build: (reference_date: string, base_cpi: number, tenors: number[], inflation_rates: number[]) =>
    apiCall('/quantlib/curves/inflation/build', { reference_date, base_cpi, tenors, inflation_rates }),
  bootstrap: (reference_date: string, base_cpi: number, swap_tenors: number[], swap_rates: number[], nominal_tenors: number[], nominal_rates: number[]) =>
    apiCall('/quantlib/curves/inflation/bootstrap', { reference_date, base_cpi, swap_tenors, swap_rates, nominal_tenors, nominal_rates }),
  seasonality: (monthly_factors: number[], cpi_values: number[], months: number[]) =>
    apiCall('/quantlib/curves/inflation/seasonality', { monthly_factors, cpi_values, months }),
};

// ── Multicurve (2) ─────────────────────────────────────────────
export const multicurveApi = {
  setup: (reference_date: string, ois_tenors: number[], ois_rates: number[], ibor_tenors: number[], ibor_rates: number[], query_tenors: number[]) =>
    apiCall('/quantlib/curves/multicurve/setup', { reference_date, ois_tenors, ois_rates, ibor_tenors, ibor_rates, query_tenors }),
  basisSpread: (reference_date: string, ois_tenors: number[], ois_rates: number[], ibor_tenors: number[], ibor_rates: number[], query_tenor: number) =>
    apiCall('/quantlib/curves/multicurve/basis-spread', { reference_date, ois_tenors, ois_rates, ibor_tenors, ibor_rates, query_tenor }),
};

// ── Cross-currency / Real Rate (2) ────────────────────────────
export const crossCurrencyApi = {
  basis: (reference_date: string, domestic_currency: string, foreign_currency: string, tenors: number[], basis_spreads_bps: number[], query_tenors: number[]) =>
    apiCall('/quantlib/curves/cross-currency-basis', { reference_date, domestic_currency, foreign_currency, tenors, basis_spreads_bps, query_tenors }),
  realRate: (reference_date: string, nominal_tenors: number[], nominal_rates: number[], base_cpi: number, inflation_tenors: number[], inflation_rates: number[], query_tenors: number[]) =>
    apiCall('/quantlib/curves/real-rate', { reference_date, nominal_tenors, nominal_rates, base_cpi, inflation_tenors, inflation_rates, query_tenors }),
};
