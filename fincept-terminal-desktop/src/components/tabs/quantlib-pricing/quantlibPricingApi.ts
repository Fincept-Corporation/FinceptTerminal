// File: src/components/tabs/quantlib-pricing/quantlibPricingApi.ts
// 29 endpoints — Black-Scholes, Black76, Bachelier, Kirk, Binomial, Monte Carlo, PDE

const BASE_URL = 'https://finceptbackend.share.zrok.io';
let _apiKey: string | null = null;
export function setPricingApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Black-Scholes (8) ──────────────────────────────────────────
export const bsApi = {
  price: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/price', { spot, strike, risk_free_rate, volatility, time_to_maturity, option_type, dividend_yield }),
  greeks: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/greeks', { spot, strike, risk_free_rate, volatility, time_to_maturity, option_type, dividend_yield }),
  impliedVol: (market_price: number, spot: number, strike: number, risk_free_rate: number, time_to_maturity: number, option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/implied-vol', { market_price, spot, strike, risk_free_rate, time_to_maturity, option_type, dividend_yield }),
  greeksFull: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/greeks-full', { spot, strike, risk_free_rate, volatility, time_to_maturity, option_type, dividend_yield }),
  digitalCall: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/digital-call', { spot, strike, risk_free_rate, volatility, time_to_maturity, dividend_yield }),
  digitalPut: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/digital-put', { spot, strike, risk_free_rate, volatility, time_to_maturity, dividend_yield }),
  assetOrNothingCall: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/asset-or-nothing-call', { spot, strike, risk_free_rate, volatility, time_to_maturity, dividend_yield }),
  assetOrNothingPut: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, dividend_yield = 0) =>
    apiCall('/quantlib/pricing/bs/asset-or-nothing-put', { spot, strike, risk_free_rate, volatility, time_to_maturity, dividend_yield }),
};

// ── Black76 (7) ────────────────────────────────────────────────
export const black76Api = {
  price: (forward: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/black76/price', { forward, strike, risk_free_rate, volatility, time_to_maturity, option_type }),
  greeks: (forward: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/black76/greeks', { forward, strike, risk_free_rate, volatility, time_to_maturity, option_type }),
  greeksFull: (forward: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/black76/greeks-full', { forward, strike, risk_free_rate, volatility, time_to_maturity, option_type }),
  impliedVol: (market_price: number, forward: number, strike: number, risk_free_rate: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/black76/implied-vol', { market_price, forward, strike, risk_free_rate, time_to_maturity, option_type }),
  caplet: (forward_rate: number, strike: number, discount_factor: number, volatility: number, t_start: number, t_end: number, notional = 1) =>
    apiCall('/quantlib/pricing/black76/caplet', { forward_rate, strike, discount_factor, volatility, t_start, t_end, notional }),
  floorlet: (forward_rate: number, strike: number, discount_factor: number, volatility: number, t_start: number, t_end: number, notional = 1) =>
    apiCall('/quantlib/pricing/black76/floorlet', { forward_rate, strike, discount_factor, volatility, t_start, t_end, notional }),
  swaption: (forward_swap_rate: number, strike: number, annuity: number, volatility: number, t_expiry: number, is_payer = true, notional = 1) =>
    apiCall('/quantlib/pricing/black76/swaption', { forward_swap_rate, strike, annuity, volatility, t_expiry, is_payer, notional }),
};

// ── Bachelier (6) ──────────────────────────────────────────────
export const bachelierApi = {
  price: (forward: number, strike: number, risk_free_rate: number, normal_volatility: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/bachelier/price', { forward, strike, risk_free_rate, normal_volatility, time_to_maturity, option_type }),
  greeks: (forward: number, strike: number, risk_free_rate: number, normal_volatility: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/bachelier/greeks', { forward, strike, risk_free_rate, normal_volatility, time_to_maturity, option_type }),
  greeksFull: (forward: number, strike: number, risk_free_rate: number, normal_volatility: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/bachelier/greeks-full', { forward, strike, risk_free_rate, normal_volatility, time_to_maturity, option_type }),
  impliedVol: (market_price: number, forward: number, strike: number, risk_free_rate: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/bachelier/implied-vol', { market_price, forward, strike, risk_free_rate, time_to_maturity, option_type }),
  volConversion: (volatility: number, forward: number, strike: number, time_to_maturity: number, direction = 'lognormal_to_normal') =>
    apiCall('/quantlib/pricing/bachelier/vol-conversion', { volatility, forward, strike, time_to_maturity, direction }),
  shiftedLognormal: (forward: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, shift = 0, option_type = 'call') =>
    apiCall('/quantlib/pricing/bachelier/shifted-lognormal', { forward, strike, risk_free_rate, volatility, time_to_maturity, shift, option_type }),
};

// ── Kirk Spread (1) ────────────────────────────────────────────
export const kirkApi = {
  spreadPrice: (F1: number, F2: number, strike: number, risk_free_rate: number, sigma1: number, sigma2: number, rho: number, time_to_maturity: number, option_type = 'call') =>
    apiCall('/quantlib/pricing/kirk/spread-price', { F1, F2, strike, risk_free_rate, sigma1, sigma2, rho, time_to_maturity, option_type }),
};

// ── Binomial Tree (3) ──────────────────────────────────────────
export const binomialApi = {
  price: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, steps: number, option_type = 'call', exercise = 'european', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/binomial/price', { spot, strike, risk_free_rate, volatility, time_to_maturity, steps, option_type, exercise, dividend_yield }),
  greeks: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, steps: number, option_type = 'call', exercise = 'european', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/binomial/greeks', { spot, strike, risk_free_rate, volatility, time_to_maturity, steps, option_type, exercise, dividend_yield }),
  convergence: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, max_steps: number, option_type = 'call', exercise = 'european', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/binomial/convergence', { spot, strike, risk_free_rate, volatility, time_to_maturity, max_steps, option_type, exercise, dividend_yield }),
};

// ── Monte Carlo (2) ────────────────────────────────────────────
export const monteCarloApi = {
  price: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, n_simulations: number, option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/monte-carlo/price', { spot, strike, risk_free_rate, volatility, time_to_maturity, n_simulations, option_type, dividend_yield }),
  barrier: (spot: number, strike: number, barrier: number, risk_free_rate: number, volatility: number, time_to_maturity: number, n_simulations: number, n_steps: number, barrier_type = 'down-and-out', option_type = 'call', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/monte-carlo/barrier', { spot, strike, barrier, risk_free_rate, volatility, time_to_maturity, n_simulations, n_steps, barrier_type, option_type, dividend_yield }),
};

// ── PDE / Finite Difference (2) ────────────────────────────────
export const pdeApi = {
  price: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, n_spot: number, n_time: number, option_type = 'call', exercise = 'european', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/pde/price', { spot, strike, risk_free_rate, volatility, time_to_maturity, n_spot, n_time, option_type, exercise, dividend_yield }),
  greeks: (spot: number, strike: number, risk_free_rate: number, volatility: number, time_to_maturity: number, n_spot: number, n_time: number, option_type = 'call', exercise = 'european', dividend_yield = 0) =>
    apiCall('/quantlib/pricing/pde/greeks', { spot, strike, risk_free_rate, volatility, time_to_maturity, n_spot, n_time, option_type, exercise, dividend_yield }),
};
