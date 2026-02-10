// File: src/components/tabs/quantlib-regulatory/quantlibRegulatoryApi.ts
// API service for QuantLib Regulatory endpoints

const BASE_URL = 'https://finceptbackend.share.zrok.io';

let _apiKey: string | null = null;
export function setRegulatoryApiKey(key: string | null) { _apiKey = key; }

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

// === BASEL ===
export const baselApi = {
  capitalRatios: (cet1_capital: number, tier1_capital: number, total_capital: number, risk_weighted_assets: number, total_exposure?: number) =>
    apiCall('/quantlib/regulatory/basel/capital-ratios', { cet1_capital, tier1_capital, total_capital, risk_weighted_assets, total_exposure }),
  creditRwa: (exposures: Array<Record<string, any>>, method?: string) =>
    apiCall('/quantlib/regulatory/basel/credit-rwa', { exposures, method }),
  operationalRwa: (gross_income_3y: number[]) =>
    apiCall('/quantlib/regulatory/basel/operational-rwa', { gross_income_3y }),
};

// === SA-CCR ===
export const saccrApi = {
  ead: (mtm_value: number, addon: number, collateral?: number, alpha?: number) =>
    apiCall('/quantlib/regulatory/saccr/ead', { mtm_value, addon, collateral, alpha }),
};

// === IFRS9 ===
export const ifrs9Api = {
  stageAssessment: (days_past_due: number, rating_downgrade: number, pd_increase_ratio: number, is_credit_impaired?: boolean) =>
    apiCall('/quantlib/regulatory/ifrs9/stage-assessment', { days_past_due, rating_downgrade, pd_increase_ratio, is_credit_impaired }),
  ecl12m: (pd_12m: number, lgd: number, ead: number, discount_rate?: number) =>
    apiCall('/quantlib/regulatory/ifrs9/ecl-12m', { pd_12m, lgd, ead, discount_rate }),
  eclLifetime: (pd_curve: number[], lgd: number, ead_curve: number[], discount_rates: number[]) =>
    apiCall('/quantlib/regulatory/ifrs9/ecl-lifetime', { pd_curve, lgd, ead_curve, discount_rates }),
  sicr: (origination_pd: number, current_pd: number, threshold_absolute?: number, threshold_relative?: number) =>
    apiCall('/quantlib/regulatory/ifrs9/sicr', { origination_pd, current_pd, threshold_absolute, threshold_relative }),
};

// === LIQUIDITY ===
export const liquidityApi = {
  lcr: (hqla_level1: number, hqla_level2a?: number, hqla_level2b?: number,
    retail_deposits_stable?: number, retail_deposits_less_stable?: number,
    unsecured_wholesale?: number, secured_funding?: number, other_outflows?: number, inflows?: number) =>
    apiCall('/quantlib/regulatory/liquidity/lcr', {
      hqla_level1, hqla_level2a, hqla_level2b,
      retail_deposits_stable, retail_deposits_less_stable,
      unsecured_wholesale, secured_funding, other_outflows, inflows,
    }),
  nsfr: (capital: number, stable_deposits_retail?: number, less_stable_deposits_retail?: number,
    wholesale_funding?: number, cash_and_reserves?: number, securities_level1?: number,
    loans_retail?: number, loans_corporate?: number, other_assets?: number) =>
    apiCall('/quantlib/regulatory/liquidity/nsfr', {
      capital, stable_deposits_retail, less_stable_deposits_retail,
      wholesale_funding, cash_and_reserves, securities_level1,
      loans_retail, loans_corporate, other_assets,
    }),
};

// === STRESS ===
export const stressApi = {
  capitalProjection: (initial_capital: number, initial_rwa: number, earnings: number[], losses: number[], rwa_changes: number[]) =>
    apiCall('/quantlib/regulatory/stress/capital-projection', { initial_capital, initial_rwa, earnings, losses, rwa_changes }),
};
