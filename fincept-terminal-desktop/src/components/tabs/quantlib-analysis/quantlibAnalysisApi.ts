// File: src/components/tabs/quantlib-analysis/quantlibAnalysisApi.ts
// 122 endpoints — fundamentals(11), ratios(77), valuation(30), industry(4)

const BASE_URL = 'https://api.fincept.in';
let _apiKey: string | null = null;
export function setAnalysisApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Fundamentals (11) ───────────────────────────────────────────
export const fundamentalsApi = {
  profitability: (income: any, balance: any, prior_balance?: any, employees?: any) =>
    apiCall('/quantlib/analysis/fundamentals/profitability', { income, balance, prior_balance, employees }),
  liquidity: (income: any, balance: any, cash_flow?: any, prior_balance?: any) =>
    apiCall('/quantlib/analysis/fundamentals/liquidity', { income, balance, cash_flow, prior_balance }),
  efficiency: (income: any, balance: any, cash_flow?: any, prior_balance?: any) =>
    apiCall('/quantlib/analysis/fundamentals/efficiency', { income, balance, cash_flow, prior_balance }),
  growth: (current_income: any, prior_income: any, current_balance: any, prior_balance: any, current_cash_flow?: any, prior_cash_flow?: any) =>
    apiCall('/quantlib/analysis/fundamentals/growth', { current_income, prior_income, current_balance, prior_balance, current_cash_flow, prior_cash_flow }),
  solvency: (income: any, balance: any, cash_flow?: any) =>
    apiCall('/quantlib/analysis/fundamentals/solvency', { income, balance, cash_flow }),
  quality: (income: any, balance: any, cash_flow: any, prior_income?: any, prior_balance?: any) =>
    apiCall('/quantlib/analysis/fundamentals/quality', { income, balance, cash_flow, prior_income, prior_balance }),
  dupont: (income: any, balance: any, prior_balance?: any) =>
    apiCall('/quantlib/analysis/fundamentals/dupont', { income, balance, prior_balance }),
  cashflow: (income: any, balance: any, cash_flow: any, prior_balance?: any) =>
    apiCall('/quantlib/analysis/fundamentals/cashflow', { income, balance, cash_flow, prior_balance }),
  comprehensive: (income: any, balance: any, cash_flow: any, prior_income?: any, prior_balance?: any, market?: any) =>
    apiCall('/quantlib/analysis/fundamentals/comprehensive', { income, balance, cash_flow, prior_income, prior_balance, market }),
  wacc: (debt: number, equity: number, tax_rate = 0.25, cost_of_debt = 0.05, cost_of_equity = 0.1, ebitda = 0, beta_levered?: number) =>
    apiCall('/quantlib/analysis/fundamentals/capital-structure/wacc', { debt, equity, tax_rate, cost_of_debt, cost_of_equity, ebitda, beta_levered }),
  optimalCapital: (debt: number, equity: number, tax_rate = 0.25, cost_of_debt = 0.05, cost_of_equity = 0.1, ebitda = 0, beta_levered?: number) =>
    apiCall('/quantlib/analysis/fundamentals/capital-structure/optimal', { debt, equity, tax_rate, cost_of_debt, cost_of_equity, ebitda, beta_levered }),
};

// ── Industry (4) ────────────────────────────────────────────────
export const industryApi = {
  banking: (data: any) => apiCall('/quantlib/analysis/industry/banking', data),
  insurance: (data: any) => apiCall('/quantlib/analysis/industry/insurance', data),
  reits: (data: any) => apiCall('/quantlib/analysis/industry/reits', data),
  utilities: (data: any) => apiCall('/quantlib/analysis/industry/utilities', data),
};

// ── Ratios: Profitability (12) ──────────────────────────────────
export const profitRatioApi = {
  grossMargin: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/gross-margin', d),
  operatingMargin: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/operating-margin', d),
  netMargin: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/net-margin', d),
  ebitdaMargin: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/ebitda-margin', d),
  roa: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/roa', d),
  roe: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/roe', d),
  roic: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/roic', d),
  roce: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/roce', d),
  basicEarningPower: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/basic-earning-power', d),
  cashRoa: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/cash-roa', d),
  cashRoe: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/cash-roe', d),
  cashRoic: (d: any) => apiCall('/quantlib/analysis/ratios/profitability/cash-roic', d),
};

// ── Ratios: Liquidity (7) ───────────────────────────────────────
export const liquidityRatioApi = {
  currentRatio: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/current-ratio', d),
  quickRatio: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/quick-ratio', d),
  cashRatio: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/cash-ratio', d),
  absoluteLiquidity: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/absolute-liquidity', d),
  defensiveInterval: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/defensive-interval', d),
  workingCapital: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/working-capital', d),
  workingCapitalRatio: (d: any) => apiCall('/quantlib/analysis/ratios/liquidity/working-capital-ratio', d),
};

// ── Ratios: Efficiency (12) ─────────────────────────────────────
export const efficiencyRatioApi = {
  assetTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/asset-turnover', d),
  fixedAssetTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/fixed-asset-turnover', d),
  inventoryTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/inventory-turnover', d),
  receivablesTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/receivables-turnover', d),
  payablesTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/payables-turnover', d),
  dso: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/days-sales-outstanding', d),
  dio: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/days-inventory-outstanding', d),
  dpo: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/days-payables-outstanding', d),
  ccc: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/cash-conversion-cycle', d),
  operatingCycle: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/operating-cycle', d),
  wcTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/working-capital-turnover', d),
  equityTurnover: (d: any) => apiCall('/quantlib/analysis/ratios/efficiency/equity-turnover', d),
};

// ── Ratios: Leverage (13) ───────────────────────────────────────
export const leverageRatioApi = {
  debtToEquity: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/debt-to-equity', d),
  debtToAssets: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/debt-to-assets', d),
  debtToCapital: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/debt-to-capital', d),
  equityRatio: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/equity-ratio', d),
  equityMultiplier: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/equity-multiplier', d),
  interestCoverage: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/interest-coverage', d),
  ebitdaInterestCoverage: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/ebitda-interest-coverage', d),
  cashCoverage: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/cash-coverage', d),
  netDebtToEbitda: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/net-debt-to-ebitda', d),
  ltdToEquity: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/long-term-debt-to-equity', d),
  financialLeverage: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/financial-leverage', d),
  capitalization: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/capitalization-ratio', d),
  dscr: (d: any) => apiCall('/quantlib/analysis/ratios/leverage/debt-service-coverage', d),
};

// ── Ratios: Growth (4) ──────────────────────────────────────────
export const growthRatioApi = {
  yoy: (current_value: number, previous_value: number, periods = 1) =>
    apiCall('/quantlib/analysis/ratios/growth/yoy', { current_value, previous_value, periods }),
  cagr: (current_value: number, previous_value: number, periods = 1) =>
    apiCall('/quantlib/analysis/ratios/growth/cagr', { current_value, previous_value, periods }),
  sustainable: (retention_ratio: number, roe: number) =>
    apiCall('/quantlib/analysis/ratios/growth/sustainable-growth', { retention_ratio, roe }),
  internal: (retention_ratio: number, roa: number) =>
    apiCall('/quantlib/analysis/ratios/growth/internal-growth', { retention_ratio, roa }),
};

// ── Ratios: Cashflow (10) ───────────────────────────────────────
export const cfRatioApi = {
  ocfToDebt: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/ocf-to-debt', d),
  ocfRatio: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/ocf-ratio', d),
  ocfMargin: (operating_cash_flow: number, revenue: number) =>
    apiCall('/quantlib/analysis/ratios/cashflow/ocf-margin', { operating_cash_flow, revenue }),
  fcf: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/fcf', d),
  fcfMargin: (operating_cash_flow: number, capital_expenditures: number, revenue: number) =>
    apiCall('/quantlib/analysis/ratios/cashflow/fcf-margin', { operating_cash_flow, capital_expenditures, revenue }),
  cashConversionQuality: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/cash-conversion-quality', d),
  capexToOcf: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/capex-to-ocf', d),
  capexToDepreciation: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/capex-to-depreciation', d),
  cashDividendCoverage: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/cash-coverage-of-dividends', d),
  reinvestment: (d: any) => apiCall('/quantlib/analysis/ratios/cashflow/reinvestment-rate', d),
};

// ── Ratios: Valuation (13) ──────────────────────────────────────
export const valRatioApi = {
  pe: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/pe-ratio', d),
  forwardPe: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/forward-pe', d),
  peg: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/peg-ratio', d),
  priceToBook: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/price-to-book', d),
  priceToSales: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/price-to-sales', d),
  priceToCashFlow: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/price-to-cash-flow', d),
  evToEbitda: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/ev-to-ebitda', d),
  evToSales: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/ev-to-sales', d),
  evToFcf: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/ev-to-fcf', d),
  dividendYield: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/dividend-yield', d),
  earningsYield: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/earnings-yield', d),
  fcfYield: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/fcf-yield', d),
  priceToTangible: (d: any) => apiCall('/quantlib/analysis/ratios/valuation/price-to-tangible-book', d),
};

// ── Ratios: Quality (6) ─────────────────────────────────────────
export const qualityRatioApi = {
  accruals: (d: any) => apiCall('/quantlib/analysis/ratios/quality/accruals-ratio', d),
  sloan: (d: any) => apiCall('/quantlib/analysis/ratios/quality/sloan-ratio', d),
  noa: (d: any) => apiCall('/quantlib/analysis/ratios/quality/net-operating-assets', d),
  earningsPersistence: (d: any) => apiCall('/quantlib/analysis/ratios/quality/earnings-persistence', d),
  earningsSmooth: (d: any) => apiCall('/quantlib/analysis/ratios/quality/earnings-smoothness', d),
  cashEarnings: (d: any) => apiCall('/quantlib/analysis/ratios/quality/cash-earnings', d),
};

// ── Valuation: DCF (7) ──────────────────────────────────────────
export const dcfApi = {
  fcff: (fcff: number[], wacc: number, terminal_growth = 0.02, shares_outstanding = 1, net_debt = 0) =>
    apiCall('/quantlib/analysis/valuation/dcf/fcff', { fcff, wacc, terminal_growth, shares_outstanding, net_debt }),
  gordonGrowth: (dividend: number, required_return: number, growth_rate: number) =>
    apiCall('/quantlib/analysis/valuation/dcf/gordon-growth', { dividend, required_return, growth_rate }),
  twoStage: (fcff: number[], wacc: number, terminal_growth = 0.02) =>
    apiCall('/quantlib/analysis/valuation/dcf/two-stage', { fcff, wacc, terminal_growth }),
  ddm: (d: any) => apiCall('/quantlib/analysis/valuation/dcf/ddm', d),
  wacc: (d: any) => apiCall('/quantlib/analysis/valuation/dcf/wacc', d),
  costOfEquity: (d: any) => apiCall('/quantlib/analysis/valuation/dcf/cost-of-equity', d),
  terminalValue: (d: any) => apiCall('/quantlib/analysis/valuation/dcf/terminal-value', d),
};

// ── Valuation: Credit (5) ───────────────────────────────────────
export const creditApi = {
  mertonModel: (d: any) => apiCall('/quantlib/analysis/valuation/credit/merton-model', d),
  spreadFromPd: (d: any) => apiCall('/quantlib/analysis/valuation/credit/spread-from-pd', d),
  distanceToDefault: (d: any) => apiCall('/quantlib/analysis/valuation/credit/distance-to-default', d),
  expectedLoss: (d: any) => apiCall('/quantlib/analysis/valuation/credit/expected-loss', d),
  ratingPd: (d: any) => apiCall('/quantlib/analysis/valuation/credit/rating-pd', d),
};

// ── Valuation: Predictive (6) ───────────────────────────────────
export const predictiveApi = {
  altmanZ: (d: any) => apiCall('/quantlib/analysis/valuation/predictive/altman-z', d),
  piotroskiF: (d: any) => apiCall('/quantlib/analysis/valuation/predictive/piotroski-f', d),
  beneishM: (d: any) => apiCall('/quantlib/analysis/valuation/predictive/beneish-m', d),
  ohlsonO: (d: any) => apiCall('/quantlib/analysis/valuation/predictive/ohlson-o', d),
  springate: (d: any) => apiCall('/quantlib/analysis/valuation/predictive/springate', d),
  zmijewski: (d: any) => apiCall('/quantlib/analysis/valuation/predictive/zmijewski', d),
};

// ── Valuation: Residual Income (3) ──────────────────────────────
export const residualIncomeApi = {
  eva: (d: any) => apiCall('/quantlib/analysis/valuation/residual-income/eva', d),
  ri: (d: any) => apiCall('/quantlib/analysis/valuation/residual-income/ri', d),
  riValuation: (d: any) => apiCall('/quantlib/analysis/valuation/residual-income/valuation', d),
};

// ── Valuation: Startup (4) ──────────────────────────────────────
export const startupApi = {
  vcMethod: (d: any) => apiCall('/quantlib/analysis/valuation/startup/vc-method', d),
  berkus: (d: any) => apiCall('/quantlib/analysis/valuation/startup/berkus', d),
  firstChicago: (d: any) => apiCall('/quantlib/analysis/valuation/startup/first-chicago', d),
  dilution: (d: any) => apiCall('/quantlib/analysis/valuation/startup/dilution', d),
};

// ── Valuation: Other (5) ────────────────────────────────────────
export const valuationOtherApi = {
  factorModels: (d: any) => apiCall('/quantlib/analysis/valuation/factor-models', d),
  proforma: (d: any) => apiCall('/quantlib/analysis/valuation/proforma/adjustments', d),
  sotp: (d: any) => apiCall('/quantlib/analysis/valuation/segment/sotp', d),
  comparable: (d: any) => apiCall('/quantlib/analysis/valuation/comparable', d),
  screen: (d: any) => apiCall('/quantlib/analysis/valuation/screen', d),
};
