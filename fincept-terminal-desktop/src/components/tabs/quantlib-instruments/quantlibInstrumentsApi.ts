// File: src/components/tabs/quantlib-instruments/quantlibInstrumentsApi.ts
// 26 endpoints — bonds, swaps, FRA, money market, OIS, equity, commodity, FX, CDS, futures

const BASE_URL = 'https://api.fincept.in';
let _apiKey: string | null = null;
export function setInstrumentsApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Fixed Bond (4) ──────────────────────────────────────────────
export const fixedBondApi = {
  cashflows: (issue_date: string, maturity_date: string, coupon_rate: number, face_value = 100, frequency = 2, day_count = 'ACT/365', settlement_date?: string) =>
    apiCall('/quantlib/instruments/bond/fixed/cashflows', { issue_date, maturity_date, coupon_rate, face_value, frequency, day_count, settlement_date }),
  price: (issue_date: string, maturity_date: string, coupon_rate: number, yield_rate: number, face_value = 100, frequency = 2, day_count = 'ACT/365', settlement_date?: string) =>
    apiCall('/quantlib/instruments/bond/fixed/price', { issue_date, maturity_date, coupon_rate, face_value, frequency, day_count, settlement_date, yield_rate }),
  yieldCalc: (issue_date: string, maturity_date: string, coupon_rate: number, clean_price: number, face_value = 100, frequency = 2, day_count = 'ACT/365', settlement_date?: string) =>
    apiCall('/quantlib/instruments/bond/fixed/yield', { issue_date, maturity_date, coupon_rate, face_value, frequency, day_count, settlement_date, clean_price }),
  analytics: (issue_date: string, maturity_date: string, coupon_rate: number, yield_rate: number, face_value = 100, frequency = 2, day_count = 'ACT/365', settlement_date?: string) =>
    apiCall('/quantlib/instruments/bond/fixed/analytics', { issue_date, maturity_date, coupon_rate, face_value, frequency, day_count, settlement_date, yield_rate }),
};

// ── Zero Coupon (1) ─────────────────────────────────────────────
export const zeroBondApi = {
  price: (issue_date: string, maturity_date: string, face_value = 100, day_count = 'ACT/365', settlement_date?: string) =>
    apiCall('/quantlib/instruments/bond/zero-coupon/price', { issue_date, maturity_date, face_value, day_count, settlement_date }),
};

// ── Inflation-Linked (1) ────────────────────────────────────────
export const inflationBondApi = {
  calc: (issue_date: string, maturity_date: string, coupon_rate: number, base_cpi: number, current_cpi: number, face_value = 100, frequency = 2) =>
    apiCall('/quantlib/instruments/bond/inflation-linked', { issue_date, maturity_date, coupon_rate, face_value, frequency, base_cpi, current_cpi }),
};

// ── IRS (3) ─────────────────────────────────────────────────────
export const irsApi = {
  value: (effective_date: string, maturity_date: string, notional: number, fixed_rate: number, curve_tenors: number[], curve_rates: number[], reference_date: string, frequency = 2, day_count = 'ACT/365', swap_type = 'payer') =>
    apiCall('/quantlib/instruments/swap/irs/value', { effective_date, maturity_date, notional, fixed_rate, frequency, day_count, curve_tenors, curve_rates, reference_date, swap_type }),
  parRate: (effective_date: string, maturity_date: string, notional: number, fixed_rate: number, curve_tenors: number[], curve_rates: number[], reference_date: string, frequency = 2, day_count = 'ACT/365', swap_type = 'payer') =>
    apiCall('/quantlib/instruments/swap/irs/par-rate', { effective_date, maturity_date, notional, fixed_rate, frequency, day_count, curve_tenors, curve_rates, reference_date, swap_type }),
  dv01: (effective_date: string, maturity_date: string, notional: number, fixed_rate: number, curve_tenors: number[], curve_rates: number[], reference_date: string, frequency = 2, day_count = 'ACT/365', swap_type = 'payer') =>
    apiCall('/quantlib/instruments/swap/irs/dv01', { effective_date, maturity_date, notional, fixed_rate, frequency, day_count, curve_tenors, curve_rates, reference_date, swap_type }),
};

// ── FRA (2) ─────────────────────────────────────────────────────
export const fraApi = {
  value: (trade_date: string, start_months: number, end_months: number, notional: number, fixed_rate: number, curve_tenors: number[], curve_rates: number[], reference_date: string) =>
    apiCall('/quantlib/instruments/fra/value', { trade_date, start_months, end_months, notional, fixed_rate, curve_tenors, curve_rates, reference_date }),
  breakEven: (trade_date: string, start_months: number, end_months: number, notional: number, fixed_rate: number, curve_tenors: number[], curve_rates: number[], reference_date: string) =>
    apiCall('/quantlib/instruments/fra/break-even', { trade_date, start_months, end_months, notional, fixed_rate, curve_tenors, curve_rates, reference_date }),
};

// ── Money Market (3) ────────────────────────────────────────────
export const mmApi = {
  deposit: (start_date: string, maturity_date: string, notional: number, rate: number, day_count = 'ACT/360') =>
    apiCall('/quantlib/instruments/money-market/deposit', { start_date, maturity_date, notional, rate, day_count }),
  tbill: (settlement_date: string, maturity_date: string, face_value = 100, discount_rate?: number, price?: number) =>
    apiCall('/quantlib/instruments/money-market/tbill', { settlement_date, maturity_date, face_value, discount_rate, price }),
  repo: (start_date: string, end_date: string, collateral_value: number, repo_rate: number, haircut = 0.02) =>
    apiCall('/quantlib/instruments/money-market/repo', { start_date, end_date, collateral_value, haircut, repo_rate }),
};

// ── OIS (2) ─────────────────────────────────────────────────────
export const oisApi = {
  value: (reference_date: string, effective_date: string, maturity_date: string, notional: number, fixed_rate: number, curve_tenors: number[], curve_rates: number[], index = 'SOFR') =>
    apiCall('/quantlib/instruments/ois/value', { reference_date, effective_date, maturity_date, notional, fixed_rate, curve_tenors, curve_rates, index }),
  buildCurve: (reference_date: string, ois_rates: number[]) =>
    apiCall('/quantlib/instruments/ois/build-curve', { reference_date, ois_rates }),
};

// ── Equity (2) ──────────────────────────────────────────────────
export const equityApi = {
  varianceSwap: (strike_variance: number, realized_returns: number[], notional: number, annualization_factor = 252) =>
    apiCall('/quantlib/instruments/equity/variance-swap', { strike_variance, realized_returns, notional, annualization_factor }),
  volatilitySwap: (strike_vol: number, realized_returns: number[], notional: number, annualization_factor = 252) =>
    apiCall('/quantlib/instruments/equity/volatility-swap', { strike_vol, realized_returns, notional, annualization_factor }),
};

// ── Commodity (1) ───────────────────────────────────────────────
export const commodityApi = {
  future: (commodity_type: string, spot_price: number, delivery_date: string, trade_date: string, futures_price: number, contract_size = 1) =>
    apiCall('/quantlib/instruments/commodity/future', { commodity_type, spot_price, delivery_date, trade_date, futures_price, contract_size }),
};

// ── FX (2) ──────────────────────────────────────────────────────
export const fxApi = {
  forward: (spot_rate: number, base_rate: number, quote_rate: number, time_to_maturity: number, notional: number, pair = 'EURUSD') =>
    apiCall('/quantlib/instruments/fx/forward', { spot_rate, base_rate, quote_rate, time_to_maturity, notional, pair }),
  garmanKohlhagen: (spot: number, strike: number, domestic_rate: number, foreign_rate: number, volatility: number, time_to_expiry: number, option_type = 'call') =>
    apiCall('/quantlib/instruments/fx/garman-kohlhagen', { spot, strike, domestic_rate, foreign_rate, volatility, time_to_expiry, option_type }),
};

// ── CDS (3) ─────────────────────────────────────────────────────
export const cdsApi = {
  value: (reference_date: string, effective_date: string, maturity_date: string, notional: number, spread_bps: number, curve_tenors: number[], curve_rates: number[], recovery_rate = 0.4, frequency = 4) =>
    apiCall('/quantlib/instruments/cds/value', { reference_date, effective_date, maturity_date, notional, spread_bps, recovery_rate, curve_tenors, curve_rates, frequency }),
  hazardRate: (spread_bps: number, recovery_rate = 0.4) =>
    apiCall('/quantlib/instruments/cds/hazard-rate', { spread_bps, recovery_rate }),
  survivalProbability: (hazard_rate: number, time_horizon: number, time_points?: number[]) =>
    apiCall('/quantlib/instruments/cds/survival-probability', { hazard_rate, time_horizon, time_points }),
};

// ── Futures (2) ─────────────────────────────────────────────────
export const futuresApi = {
  stir: (price: number, contract_date: string, reference_date: string, notional = 1000000, tick_value = 25) =>
    apiCall('/quantlib/instruments/futures/stir', { price, contract_date, reference_date, notional, tick_value }),
  bondCtd: (futures_price: number, delivery_date: string, reference_date: string, deliverables: any[]) =>
    apiCall('/quantlib/instruments/futures/bond/ctd', { futures_price, delivery_date, reference_date, deliverables }),
};
