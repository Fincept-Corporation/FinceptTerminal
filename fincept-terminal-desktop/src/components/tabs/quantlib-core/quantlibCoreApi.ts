// File: src/components/tabs/quantlib-core/quantlibCoreApi.ts
// API service for QuantLib Core endpoints

const BASE_URL = 'https://api.fincept.in';

let _apiKey: string | null = null;
export function setQuantLibApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, method: 'GET' | 'POST' = 'POST', body?: any, queryParams?: Record<string, any>): Promise<T> {
  let url = `${BASE_URL}${path}`;
  if (queryParams) {
    const params = new URLSearchParams();
    for (const [k, v] of Object.entries(queryParams)) {
      if (v !== undefined && v !== null) params.append(k, String(v));
    }
    const qs = params.toString();
    if (qs) url += `?${qs}`;
  }

  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;

  const opts: RequestInit = { method, headers };
  if (method === 'POST' && body) {
    opts.body = JSON.stringify(body);
  }

  const res = await fetch(url, opts);
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`API ${res.status}: ${text}`);
  }
  return res.json();
}

// === TYPES ===
export const typesApi = {
  tenorAddToDate: (tenor: string, start_date: string) =>
    apiCall('/quantlib/core/types/tenor/add-to-date', 'POST', { tenor, start_date }),
  rateConvert: (value: number, unit?: string) =>
    apiCall('/quantlib/core/types/rate/convert', 'POST', { value, unit }),
  moneyCreate: (amount: number, currency?: string) =>
    apiCall('/quantlib/core/types/money/create', 'POST', { amount, currency }),
  moneyConvert: (amount: number, from_currency: string, to_currency: string, rate: number) =>
    apiCall('/quantlib/core/types/money/convert', 'POST', { amount, from_currency, to_currency, rate }),
  spreadFromBps: (bps: number) =>
    apiCall('/quantlib/core/types/spread/from-bps', 'POST', undefined, { bps }),
  notionalSchedule: (schedule_type: string, notional: number, periods: number, rate?: number) =>
    apiCall('/quantlib/core/types/notional-schedule', 'POST', { schedule_type, notional, periods, rate }),
  listCurrencies: () =>
    apiCall('/quantlib/core/types/currencies', 'GET'),
  listFrequencies: () =>
    apiCall('/quantlib/core/types/frequencies', 'GET'),
};

// === CONVENTIONS ===
export const conventionsApi = {
  formatDate: (date_str: string, format?: string) =>
    apiCall('/quantlib/core/conventions/format-date', 'POST', { date_str, format }),
  parseDate: (date_string: string) =>
    apiCall('/quantlib/core/conventions/parse-date', 'POST', { date_string }),
  daysToYears: (value: number, convention?: string) =>
    apiCall('/quantlib/core/conventions/days-to-years', 'POST', { value, convention }),
  yearsToDays: (value: number, convention?: string) =>
    apiCall('/quantlib/core/conventions/years-to-days', 'POST', { value, convention }),
  normalizeRate: (value: number, unit?: string) =>
    apiCall('/quantlib/core/conventions/normalize-rate', 'POST', { value, unit }),
  normalizeVol: (value: number, vol_type?: string, is_percent?: boolean) =>
    apiCall('/quantlib/core/conventions/normalize-volatility', 'POST', { value, vol_type, is_percent }),
};

// === AUTODIFF ===
export const autodiffApi = {
  dualEval: (func_name: string, x: number, power_n?: number) =>
    apiCall('/quantlib/core/autodiff/dual-eval', 'POST', { func_name, x, power_n }),
  gradient: (func_name: string, x: number[]) =>
    apiCall('/quantlib/core/autodiff/gradient', 'POST', { func_name, x }),
  taylorExpand: (func_name: string, x0: number, order?: number) =>
    apiCall('/quantlib/core/autodiff/taylor-expand', 'POST', { func_name, x0, order }),
};

// === DISTRIBUTIONS ===
export const distributionsApi = {
  normalCdf: (x: number) =>
    apiCall('/quantlib/core/distributions/normal/cdf', 'POST', { x }),
  normalPdf: (x: number) =>
    apiCall('/quantlib/core/distributions/normal/pdf', 'POST', { x }),
  normalPpf: (p: number) =>
    apiCall('/quantlib/core/distributions/normal/ppf', 'POST', { p }),
  tCdf: (x: number, df: number) =>
    apiCall('/quantlib/core/distributions/t/cdf', 'POST', { x, df }),
  tPdf: (x: number, df: number) =>
    apiCall('/quantlib/core/distributions/t/pdf', 'POST', { x, df }),
  tPpf: (p: number, df: number) =>
    apiCall('/quantlib/core/distributions/t/ppf', 'POST', { p, df }),
  chi2Cdf: (x: number, df: number) =>
    apiCall('/quantlib/core/distributions/chi2/cdf', 'POST', { x, df }),
  chi2Pdf: (x: number, df: number) =>
    apiCall('/quantlib/core/distributions/chi2/pdf', 'POST', { x, df }),
  gammaCdf: (x: number, alpha: number, beta?: number) =>
    apiCall('/quantlib/core/distributions/gamma/cdf', 'POST', { x, alpha, beta }),
  gammaPdf: (x: number, alpha: number, beta?: number) =>
    apiCall('/quantlib/core/distributions/gamma/pdf', 'POST', { x, alpha, beta }),
  expCdf: (x: number, rate: number) =>
    apiCall('/quantlib/core/distributions/exponential/cdf', 'POST', { x, rate }),
  expPdf: (x: number, rate: number) =>
    apiCall('/quantlib/core/distributions/exponential/pdf', 'POST', { x, rate }),
  expPpf: (p: number, rate: number) =>
    apiCall('/quantlib/core/distributions/exponential/ppf', 'POST', { p, rate }),
  bivariateNormalCdf: (x: number, y: number, rho: number) =>
    apiCall('/quantlib/core/distributions/bivariate-normal/cdf', 'POST', { x, y, rho }),
};

// === MATH ===
export const mathApi = {
  eval: (func_name: string, x: number) =>
    apiCall('/quantlib/core/math/eval', 'POST', { func_name, x }),
  twoArg: (func_name: string, x: number, y: number) =>
    apiCall('/quantlib/core/math/two-arg', 'POST', { func_name, x, y }),
};

// === OPERATIONS ===
export const opsApi = {
  discountCashflows: (cashflows: number[], times: number[], discount_factors: number[]) =>
    apiCall('/quantlib/core/ops/discount-cashflows', 'POST', { cashflows, times, discount_factors }),
  blackScholes: (option_type: string, spot: number, strike: number, rate: number, time: number, volatility: number) =>
    apiCall('/quantlib/core/ops/black-scholes', 'POST', { option_type, spot, strike, rate, time, volatility }),
  black76: (option_type: string, forward: number, strike: number, volatility: number, time: number, discount_factor: number) =>
    apiCall('/quantlib/core/ops/black76', 'POST', { option_type, forward, strike, volatility, time, discount_factor }),
  var: (method: string, returns?: number[], portfolio_value?: number, portfolio_std?: number, confidence?: number, holding_period?: number) =>
    apiCall('/quantlib/core/ops/var', 'POST', { method, returns, portfolio_value, portfolio_std, confidence, holding_period }),
  interpolate: (method: string, x: number, x_data: number[], y_data: number[]) =>
    apiCall('/quantlib/core/ops/interpolate', 'POST', { method, x, x_data, y_data }),
  forwardRate: (df1: number, df2: number, t1: number, t2: number) =>
    apiCall('/quantlib/core/ops/forward-rate', 'POST', { df1, df2, t1, t2 }),
  zeroRateConvert: (direction: string, value: number, t: number) =>
    apiCall('/quantlib/core/ops/zero-rate-convert', 'POST', { direction, value, t }),
  gbmPaths: (spot: number, drift: number, volatility: number, time: number, n_paths?: number, n_steps?: number, seed?: number, antithetic?: boolean) =>
    apiCall('/quantlib/core/ops/gbm-paths', 'POST', { spot, drift, volatility, time, n_paths, n_steps, seed, antithetic }),
  cholesky: (matrix: number[][]) =>
    apiCall('/quantlib/core/ops/cholesky', 'POST', { matrix }),
  covarianceMatrix: (returns: number[][]) =>
    apiCall('/quantlib/core/ops/covariance-matrix', 'POST', { returns }),
  statistics: (values: number[], ddof?: number) =>
    apiCall('/quantlib/core/ops/statistics', 'POST', { values, ddof }),
  percentile: (values: number[], p: number) =>
    apiCall('/quantlib/core/ops/percentile', 'POST', { values, p }),
};

// === LEGS ===
export const legsApi = {
  fixed: (notional: number, rate: number, start_date: string, end_date: string, frequency?: string, day_count?: string, currency?: string) =>
    apiCall('/quantlib/core/legs/fixed', 'POST', { notional, rate, start_date, end_date, frequency, day_count, currency }),
  float: (notional: number, start_date: string, end_date: string, spread?: number, frequency?: string, day_count?: string, index_name?: string) =>
    apiCall('/quantlib/core/legs/float', 'POST', { notional, spread, start_date, end_date, frequency, day_count, index_name }),
  zeroCoupon: (notional: number, rate: number, start_date: string, end_date: string, day_count?: string) =>
    apiCall('/quantlib/core/legs/zero-coupon', 'POST', { notional, rate, start_date, end_date, day_count }),
};

// === PERIODS ===
export const periodsApi = {
  fixedCoupon: (notional: number, rate: number, start_date: string, end_date: string, day_count?: string) =>
    apiCall('/quantlib/core/periods/fixed-coupon', 'POST', { notional, rate, start_date, end_date, day_count }),
  floatCoupon: (notional: number, start_date: string, end_date: string, spread?: number, day_count?: string, fixing_rate?: number) =>
    apiCall('/quantlib/core/periods/float-coupon', 'POST', { notional, spread, start_date, end_date, day_count, fixing_rate }),
  dayCountFraction: (start_date: string, end_date: string, convention?: string) =>
    apiCall('/quantlib/core/periods/day-count-fraction', 'POST', { start_date, end_date, convention }),
};
