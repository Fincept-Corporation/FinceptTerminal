/**
 * M&A Analytics Service - Type Definitions
 */

export interface MADeal {
  deal_id: string;
  target_name: string;
  acquirer_name: string;
  deal_value: number;
  offer_price_per_share?: number;
  premium_1day: number;
  payment_method?: string;
  payment_cash_pct: number;
  payment_stock_pct: number;
  ev_revenue?: number;
  ev_ebitda?: number;
  synergies?: number;
  status: string;
  deal_type?: string;
  industry: string;
  announced_date: string;
  expected_close_date?: string;
  breakup_fee?: number;
  hostile_flag?: number;
  tender_offer_flag?: number;
  cross_border_flag?: number;
  acquirer_country?: string;
  target_country?: string;
}

export interface ValuationResult {
  method: string;
  valuation: number;
  range?: { min: number; max: number };
  assumptions: Record<string, any>;
  sensitivity?: any;
}

export interface DCFInputs {
  wacc_inputs: {
    risk_free_rate: number;
    market_risk_premium: number;
    beta: number;
    cost_of_debt: number;
    tax_rate: number;
    market_value_equity: number;
    market_value_debt: number;
  };
  fcf_inputs: {
    ebit: number;
    tax_rate: number;
    depreciation: number;
    capex: number;
    change_in_nwc: number;
  };
  growth_rates: number[];
  terminal_growth: number;
  balance_sheet: {
    cash: number;
    debt: number;
    minority_interest?: number;
    preferred_stock?: number;
  };
  shares_outstanding: number;
}

export interface MergerModelInputs {
  acquirer_data: {
    revenue: number;
    ebitda: number;
    ebit: number;
    net_income: number;
    shares_outstanding: number;
    eps: number;
    market_cap: number;
  };
  target_data: {
    revenue: number;
    ebitda: number;
    ebit: number;
    net_income: number;
    shares_outstanding: number;
    eps: number;
    market_cap: number;
  };
  deal_structure: {
    purchase_price: number;
    payment_cash_pct: number;
    payment_stock_pct: number;
    exchange_ratio?: number;
    cost_synergies?: number;
    revenue_synergies?: number;
    integration_costs?: number;
    tax_rate?: number;
  };
}

export interface LBOInputs {
  company_data: {
    company_name?: string;
    revenue: number;
    ebitda: number;
    ebit: number;
    capex: number;
    nwc: number;
  };
  transaction_assumptions: {
    purchase_price: number;
    entry_multiple: number;
    exit_multiple: number;
    exit_multiples?: number[];
    debt_percent: number;
    equity_percent: number;
    target_leverage?: number;
    revenue_growth?: number;
    ebitda_margin?: number;
    capex_pct_revenue?: number;
    nwc_pct_revenue?: number;
    tax_rate?: number;
    hurdle_irr?: number;
  };
  projection_years: number;
}

export interface StartupValuationInputs {
  startup_name: string;
  berkus_scores?: Record<string, number>;
  scorecard_inputs?: {
    stage: string;
    region: string;
    assessments: Record<string, number>;
  };
  vc_inputs?: {
    exit_year_metric: number;
    exit_multiple: number;
    years_to_exit: number;
    investment_amount: number;
    stage: string;
  };
  first_chicago_scenarios?: Array<{
    name: string;
    probability: number;
    exit_year: number;
    exit_value: number;
    description: string;
  }>;
  risk_factor_assessments?: Record<string, number>;
}

export interface SynergyInputs {
  revenue_synergies?: {
    type: string;
    params: Record<string, any>;
  };
  cost_synergies?: {
    type: string;
    params: Record<string, any>;
  };
  integration_costs?: {
    deal_size: number;
    complexity_level: string;
    integration_plan: Record<string, any>;
  };
}

export interface FairnessOpinionInputs {
  valuation_methods: ValuationResult[];
  offer_price: number;
  qualitative_factors: string[];
  daily_prices?: number[];
  announcement_date?: number;
}
