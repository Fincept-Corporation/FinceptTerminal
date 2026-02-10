// File: src/components/tabs/quantlib-solver/quantlibSolverApi.ts
// 25 endpoints — bond analytics, rates, implied vol, spreads, basis, carry, cashflows

const BASE_URL = 'https://finceptbackend.share.zrok.io';
let _apiKey: string | null = null;
export function setSolverApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Bond Analytics (4) ─────────────────────────────────────────
export const bondApi = {
  yield: (price: number, coupon: number, maturity: number, face = 100, frequency = 2, guess = 0.05) =>
    apiCall('/quantlib/solver/finance/bond-yield', { price, coupon, maturity, face, frequency, guess }),
  duration: (coupon: number, maturity: number, ytm: number, face = 100, frequency = 2) =>
    apiCall('/quantlib/solver/finance/duration', { coupon, maturity, ytm, face, frequency }),
  modifiedDuration: (coupon: number, maturity: number, ytm: number, face = 100, frequency = 2) =>
    apiCall('/quantlib/solver/finance/modified-duration', { coupon, maturity, ytm, face, frequency }),
  convexity: (coupon: number, maturity: number, ytm: number, face = 100, frequency = 2) =>
    apiCall('/quantlib/solver/finance/convexity', { coupon, maturity, ytm, face, frequency }),
};

// ── Convexity Adjustment (1) ───────────────────────────────────
export const convexityAdjApi = {
  adjust: (forward_rate: number, volatility: number, t1: number, t2: number, model = 'simple', mean_reversion = 0.03) =>
    apiCall('/quantlib/solver/finance/convexity-adjustment', { forward_rate, volatility, t1, t2, model, mean_reversion }),
};

// ── Rates (4) ──────────────────────────────────────────────────
export const rateApi = {
  discountFactor: (rate: number, t: number, compounding = 'continuous') =>
    apiCall('/quantlib/solver/finance/discount-factor', { rate, t, compounding }),
  zeroRate: (discount_factor: number, t: number, compounding = 'continuous') =>
    apiCall('/quantlib/solver/finance/zero-rate', { discount_factor, t, compounding }),
  forwardRate: (df1: number, df2: number, t1: number, t2: number, compounding = 'simple') =>
    apiCall('/quantlib/solver/finance/forward-rate', { df1, df2, t1, t2, compounding }),
  parRate: (discount_factors: number[], times: number[], frequency = 2) =>
    apiCall('/quantlib/solver/finance/par-rate', { discount_factors, times, frequency }),
};

// ── PV01, IRR, XIRR (3) ───────────────────────────────────────
export const cashflowApi = {
  pv01: (notional: number, discount_factors: number[], times: number[], frequency = 2) =>
    apiCall('/quantlib/solver/finance/pv01', { notional, discount_factors, times, frequency }),
  irr: (cashflows: number[], times?: number[], guess = 0.1) =>
    apiCall('/quantlib/solver/finance/irr', { cashflows, times, guess }),
  xirr: (cashflows: number[], dates: string[], guess = 0.1) =>
    apiCall('/quantlib/solver/finance/xirr', { cashflows, dates, guess }),
};

// ── Implied Vol (2) ────────────────────────────────────────────
export const impliedVolApi = {
  bs: (price: number, spot: number, strike: number, time: number, rate: number, option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/solver/finance/implied-vol', { price, spot, strike, time, rate, option_type, dividend_yield }),
  black76: (price: number, forward: number, strike: number, time: number, rate: number, option_type = 'call') =>
    apiCall('/quantlib/solver/finance/implied-vol-black76', { price, forward, strike, time, rate, option_type }),
};

// ── Spreads (3) ────────────────────────────────────────────────
export const spreadApi = {
  gSpread: (bond_yield: number, govt_yield: number) =>
    apiCall('/quantlib/solver/finance/g-spread', { bond_yield, govt_yield }),
  iSpread: (bond_yield: number, swap_rate: number) =>
    apiCall('/quantlib/solver/finance/i-spread', { bond_yield, swap_rate }),
  zSpread: (price: number, cashflows: number[], times: number[], base_rates: number[]) =>
    apiCall('/quantlib/solver/finance/z-spread', { price, cashflows, times, base_rates }),
};

// ── Basis & Carry (3) ──────────────────────────────────────────
export const basisApi = {
  basis: (spot: number, futures: number) =>
    apiCall('/quantlib/solver/finance/basis', { spot, futures }),
  carry: (spot: number, futures: number, time_to_expiry: number) =>
    apiCall('/quantlib/solver/finance/carry', { spot, futures, time_to_expiry }),
  impliedRepo: (spot: number, futures: number, time_to_expiry: number, income = 0) =>
    apiCall('/quantlib/solver/finance/implied-repo-rate', { spot, futures, time_to_expiry, income }),
};

// ── Cashflow Analysis (5) ──────────────────────────────────────
export const analysisApi = {
  npv: (cashflows: number[], rate: number, times?: number[]) =>
    apiCall('/quantlib/solver/finance/npv', { cashflows, rate, times }),
  loan: (principal: number, annual_rate: number, n_periods: number, frequency = 12) =>
    apiCall('/quantlib/solver/finance/loan-amortization', { principal, annual_rate, n_periods, frequency }),
  annuity: (payment: number, rate: number, n_periods: number) =>
    apiCall('/quantlib/solver/finance/annuity-pv', { payment, rate, n_periods }),
  perpetuity: (payment: number, rate: number, growth = 0) =>
    apiCall('/quantlib/solver/finance/perpetuity-pv', { payment, rate, growth }),
  fv: (pv: number, rate: number, n_periods: number) =>
    apiCall('/quantlib/solver/finance/future-value', { pv, rate, n_periods }),
};
