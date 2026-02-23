/**
 * M&A Analytics Service - Bloomberg FA Replacement System
 *
 * Complete M&A and Financial Advisory toolkit covering:
 * - Deal Database & SEC Filing Scanner
 * - Valuation Analysis (DCF, Comps, Precedent Transactions)
 * - Merger Models (Accretion/Dilution, Pro Forma)
 * - LBO Models
 * - Startup Valuation (5 methods)
 * - Deal Structure (Payment, Earnout, CVR, Collar)
 * - Synergies (Revenue & Cost)
 * - Fairness Opinions
 * - Industry-Specific Metrics (Tech, Healthcare, Financial Services)
 * - Advanced Analytics (Monte Carlo, Regression)
 * - Deal Comparison & Benchmarking
 */

import { invoke } from '@tauri-apps/api/core';

// =============================================================================
// TYPES
// =============================================================================

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

// =============================================================================
// DEAL DATABASE OPERATIONS
// =============================================================================

export class DealDatabaseService {
  /**
   * Scan SEC filings for M&A activity
   */
  static async scanFilings(
    daysBack: number = 30,
    filingTypes?: string[]
  ): Promise<{
    filings_found: number;
    deals_parsed: number;
    deals_created: number;
    filings: any[];
    deals: any[];
    deals_count: number;
  }> {
    const result = await invoke('scan_ma_filings', {
      daysBack,
      filingTypes: filingTypes ? JSON.stringify(filingTypes) : undefined,
    });
    const parsed = JSON.parse(result as string);
    if (parsed.success) {
      return parsed.data || { filings_found: 0, deals_parsed: 0, deals_created: 0, filings: [], deals: [], deals_count: 0 };
    } else {
      throw new Error(parsed.error || 'Failed to scan filings');
    }
  }

  /**
   * Parse a specific SEC filing for deal details
   */
  static async parseFiling(
    filingUrl: string,
    filingType: string
  ): Promise<any> {
    const result = await invoke('parse_ma_filing', {
      filingUrl,
      filingType,
    });
    const parsed = JSON.parse(result as string);
    if (parsed.success) {
      return parsed.data;
    } else {
      throw new Error(parsed.error || 'Failed to parse filing');
    }
  }

  /**
   * Create a new M&A deal in database
   */
  static async createDeal(dealData: Partial<MADeal>): Promise<any> {
    return invoke('create_ma_deal', {
      dealData: JSON.stringify(dealData),
    });
  }

  /**
   * Get all M&A deals with optional filters
   */
  static async getAllDeals(filters?: Record<string, any>): Promise<MADeal[]> {
    const result = await invoke('get_all_ma_deals', {
      filters: filters ? JSON.stringify(filters) : undefined,
    });
    const parsed = JSON.parse(result as string);
    if (parsed.success) {
      // Map DB column names to MADeal interface
      return (parsed.data || []).map((d: any) => {
        const paymentMethod: string = d.payment_method || '';
        // Derive cash/stock percentages from stored columns first, then fall back to payment_method string
        let cashPct: number = d.cash_percentage ?? d.payment_cash_pct ?? null;
        let stockPct: number = d.stock_percentage ?? d.payment_stock_pct ?? null;
        if (cashPct === null && stockPct === null) {
          if (paymentMethod === 'Cash') { cashPct = 100; stockPct = 0; }
          else if (paymentMethod === 'Stock') { cashPct = 0; stockPct = 100; }
          else if (paymentMethod === 'Mixed') { cashPct = 50; stockPct = 50; }
          else { cashPct = 0; stockPct = 0; }
        }
        return {
          deal_id: d.deal_id || '',
          target_name: d.target_name || '',
          acquirer_name: d.acquirer_name || '',
          deal_value: d.deal_value || 0,
          offer_price_per_share: d.offer_price_per_share ?? undefined,
          premium_1day: d.premium_1day || 0,
          payment_method: paymentMethod,
          payment_cash_pct: cashPct ?? 0,
          payment_stock_pct: stockPct ?? 0,
          ev_revenue: d.ev_revenue ?? undefined,
          ev_ebitda: d.ev_ebitda ?? undefined,
          synergies: d.synergies_disclosed ?? d.synergies ?? undefined,
          status: d.deal_status || d.status || 'Unknown',
          deal_type: d.deal_type || undefined,
          industry: d.industry || 'General',
          announced_date: d.announcement_date || d.announced_date || '',
          expected_close_date: d.expected_close_date || undefined,
          breakup_fee: d.breakup_fee ?? undefined,
          hostile_flag: d.hostile_flag ?? undefined,
          tender_offer_flag: d.tender_offer_flag ?? undefined,
          cross_border_flag: d.cross_border_flag ?? undefined,
          acquirer_country: d.acquirer_country || undefined,
          target_country: d.target_country || undefined,
        };
      });
    } else {
      throw new Error(parsed.error || 'Failed to get deals');
    }
  }

  /**
   * Search M&A deals by query
   */
  static async searchDeals(
    query: string,
    searchType?: 'target' | 'acquirer' | 'industry'
  ): Promise<MADeal[]> {
    const result = await invoke('search_ma_deals', {
      query,
      searchType,
    });
    const parsed = JSON.parse(result as string);
    if (parsed.success) {
      return parsed.data || [];
    } else {
      throw new Error(parsed.error || 'Failed to search deals');
    }
  }

  /**
   * Update an existing deal
   */
  static async updateDeal(
    dealId: string,
    updates: Partial<MADeal>
  ): Promise<any> {
    return invoke('update_ma_deal', {
      dealId,
      updates: JSON.stringify(updates),
    });
  }
}

// =============================================================================
// VALUATION ANALYSIS
// =============================================================================

export class ValuationService {
  /**
   * Calculate valuation using precedent transactions
   */
  static async calculatePrecedentTransactions(
    targetData: Record<string, any>,
    compDeals: MADeal[]
  ): Promise<any> {
    const result = await invoke('calculate_precedent_transactions', {
      targetData: JSON.stringify(targetData),
      compDeals: JSON.stringify(compDeals),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Precedent transactions failed');
    return parsed;
  }

  /**
   * Calculate trading comps valuation
   */
  static async calculateTradingComps(
    targetTicker: string,
    compTickers: string[]
  ): Promise<any> {
    const result = await invoke('calculate_trading_comps', {
      targetTicker,
      compTickers: JSON.stringify(compTickers),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Trading comps failed');
    return parsed;
  }

  /**
   * Run comprehensive DCF valuation
   */
  static async calculateDCF(inputs: DCFInputs): Promise<any> {
    const result = await invoke('calculate_ma_dcf', {
      waccInputs: JSON.stringify(inputs.wacc_inputs),
      fcfInputs: JSON.stringify(inputs.fcf_inputs),
      growthRates: JSON.stringify(inputs.growth_rates),
      terminalGrowth: inputs.terminal_growth,
      balanceSheet: JSON.stringify(inputs.balance_sheet),
      sharesOutstanding: inputs.shares_outstanding,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'DCF calculation failed');
    const d = parsed.data || {};
    // Flatten nested Python response to match frontend expectations
    return {
      equity_value_per_share: d.valuation_per_share?.price_per_share ?? 0,
      enterprise_value: d.enterprise_value?.enterprise_value ?? 0,
      equity_value: d.equity_value?.equity_value ?? 0,
      wacc: d.wacc?.wacc ?? 0,
      terminal_value_contribution: d.enterprise_value?.terminal_value_contribution,
      fcf_details: d.enterprise_value?.fcf_details,
      shares_outstanding: d.valuation_per_share?.shares_used,
      summary: d.summary,
    };
  }

  /**
   * Calculate DCF sensitivity analysis
   */
  static async calculateDCFSensitivity(
    baseFcf: number,
    growthRates: number[],
    terminalGrowthScenarios: number[],
    waccScenarios: number[],
    balanceSheet: Record<string, number>,
    sharesOutstanding: number
  ): Promise<any> {
    const result = await invoke('calculate_dcf_sensitivity', {
      baseFcf,
      growthRates: JSON.stringify(growthRates),
      terminalGrowthScenarios: JSON.stringify(terminalGrowthScenarios),
      waccScenarios: JSON.stringify(waccScenarios),
      balanceSheet: JSON.stringify(balanceSheet),
      sharesOutstanding,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'DCF sensitivity failed');
    return parsed;
  }

  /**
   * Generate football field chart
   */
  static async generateFootballField(
    valuationMethods: ValuationResult[],
    sharesOutstanding: number
  ): Promise<any> {
    const result = await invoke('generate_football_field', {
      valuationMethods: JSON.stringify(valuationMethods),
      sharesOutstanding,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Football field generation failed');
    return parsed;
  }
}

// =============================================================================
// MERGER MODELS
// =============================================================================

export class MergerModelService {
  /**
   * Build complete merger model
   */
  static async buildMergerModel(inputs: MergerModelInputs): Promise<any> {
    const result = await invoke('build_merger_model', {
      acquirerData: JSON.stringify(inputs.acquirer_data),
      targetData: JSON.stringify(inputs.target_data),
      dealStructure: JSON.stringify(inputs.deal_structure),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Merger model failed');
    const d = parsed.data;
    // Flatten for UI: accretion_dilution fields + pro_forma fields at top level
    const ad = d.accretion_dilution || {};
    const pf = d.pro_forma_year1 || {};
    return {
      ...d,
      // Accretion/Dilution fields at top level for UI
      eps_accretion_pct: ad.accretion_dilution_pct ?? 0,
      pro_forma_eps: ad.pro_forma_eps ?? pf.eps ?? 0,
      standalone_eps: ad.standalone_eps ?? 0,
      is_accretive: ad.is_accretive ?? false,
      // Pro Forma fields at top level for UI
      pro_forma_revenue: pf.revenue ?? 0,
      pro_forma_shares: pf.shares_outstanding ?? 0,
      pro_forma_net_income: pf.net_income ?? 0,
      pro_forma_ebitda: pf.ebitda ?? 0,
    };
  }

  /**
   * Calculate accretion/dilution analysis
   */
  static async calculateAccretionDilution(
    mergerModelData: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_accretion_dilution', {
      mergerModelData: JSON.stringify(mergerModelData),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Accretion/Dilution failed');
    return parsed.data;
  }

  /**
   * Build pro forma financials
   */
  static async buildProForma(
    acquirerData: Record<string, any>,
    targetData: Record<string, any>,
    year: number
  ): Promise<any> {
    const result = await invoke('build_pro_forma', {
      acquirerData: JSON.stringify(acquirerData),
      targetData: JSON.stringify(targetData),
      year,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Pro forma failed');
    const d = parsed.data;
    const pf = d.pro_forma || {};
    // Flatten pro_forma fields with combined_ prefix for UI
    return {
      data: {
        combined_revenue: pf.revenue ?? 0,
        combined_ebitda: pf.ebitda ?? 0,
        combined_net_income: pf.net_income ?? 0,
        combined_eps: pf.eps ?? 0,
        combined_shares: pf.shares_outstanding ?? 0,
        pro_forma: pf,
        standalone_vs_proforma: d.standalone_vs_proforma || {},
        year: d.year,
      },
    };
  }

  /**
   * Calculate sources and uses of funds
   */
  static async calculateSourcesUses(
    dealStructure: Record<string, any>,
    financingStructure: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_sources_uses', {
      dealStructure: JSON.stringify(dealStructure),
      financingStructure: JSON.stringify(financingStructure),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Sources & Uses failed');
    return { data: parsed.data };
  }

  /**
   * Analyze contribution analysis
   */
  static async analyzeContribution(
    acquirerData: Record<string, any>,
    targetData: Record<string, any>,
    ownershipSplit: number
  ): Promise<any> {
    const result = await invoke('analyze_contribution', {
      acquirerData: JSON.stringify(acquirerData),
      targetData: JSON.stringify(targetData),
      ownershipSplit,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Contribution analysis failed');
    const d = parsed.data;
    // Restructure for UI: { contributions: { revenue: {acquirer_pct, target_pct}, ... }, fairness_assessment }
    const { ownership, ...metrics } = d;
    const contributions: Record<string, any> = {};
    for (const [key, val] of Object.entries(metrics)) {
      contributions[key.replace('_contribution', '')] = val;
    }
    // Fairness assessment: check if ownership % roughly matches contribution %
    const avgAcqContrib = Object.values(contributions).reduce(
      (sum: number, v: any) => sum + (v.acquirer_pct || 0), 0
    ) / Math.max(Object.keys(contributions).length, 1);
    const ownershipDelta = Math.abs((ownership?.acquirer_pct || 0) - avgAcqContrib);
    return {
      data: {
        contributions,
        ownership,
        fairness_assessment: {
          is_fair: ownershipDelta < 15,
          commentary: ownershipDelta < 5
            ? 'Ownership closely reflects financial contributions. Deal structure appears fair.'
            : ownershipDelta < 15
            ? `Moderate gap (${ownershipDelta.toFixed(1)}pp) between ownership and contribution. Review recommended.`
            : `Significant gap (${ownershipDelta.toFixed(1)}pp) between ownership and contribution. Deal may disadvantage one party.`,
        },
      },
    };
  }
}

// =============================================================================
// LBO MODELS
// =============================================================================

export class LBOModelService {
  /**
   * Build complete LBO model
   */
  static async buildLBOModel(inputs: LBOInputs): Promise<any> {
    const result = await invoke('build_lbo_model', {
      companyData: JSON.stringify(inputs.company_data),
      transactionAssumptions: JSON.stringify(inputs.transaction_assumptions),
      projectionYears: inputs.projection_years,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'LBO model failed');
    return parsed;
  }

  /**
   * Calculate LBO returns (IRR, MOIC)
   */
  static async calculateLBOReturns(
    entryValuation: number,
    exitValuation: number,
    equityInvested: number,
    holdingPeriod: number
  ): Promise<any> {
    const result = await invoke('calculate_lbo_returns', {
      entryValuation,
      exitValuation,
      equityInvested,
      holdingPeriod,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'LBO returns calculation failed');
    const d = parsed.data || {};
    return {
      irr: d.annualized_return || d.irr || 0,
      moic: d.moic || d.cash_on_cash_multiple || 0,
      absolute_gain: d.absolute_gain || 0,
      absolute_gain_pct: d.absolute_gain_pct || 0,
      initial_equity: d.initial_equity_investment || 0,
      exit_equity: d.exit_equity_value || 0,
      holding_period: d.holding_period_years || 0,
    };
  }

  /**
   * Analyze LBO debt schedule
   */
  static async analyzeDebtSchedule(
    debtStructure: Record<string, any>,
    cashFlows: Record<string, any>,
    sweepPercentage: number
  ): Promise<any> {
    const result = await invoke('analyze_lbo_debt_schedule', {
      debtStructure: JSON.stringify(debtStructure),
      cashFlows: JSON.stringify(cashFlows),
      sweepPercentage,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Debt schedule analysis failed');
    return parsed;
  }

  /**
   * Calculate LBO sensitivity analysis
   */
  static async calculateLBOSensitivity(
    baseCase: Record<string, any>,
    revenueGrowthScenarios: number[],
    exitMultipleScenarios: number[]
  ): Promise<any> {
    const result = await invoke('calculate_lbo_sensitivity', {
      baseCase: JSON.stringify(baseCase),
      revenueGrowthScenarios: JSON.stringify(revenueGrowthScenarios),
      exitMultipleScenarios: JSON.stringify(exitMultipleScenarios),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'LBO sensitivity analysis failed');
    return parsed;
  }
}

// =============================================================================
// STARTUP VALUATION
// =============================================================================

export class StartupValuationService {
  /**
   * Calculate Berkus Method valuation
   */
  static async calculateBerkusValuation(
    factorScores: Record<string, number>
  ): Promise<any> {
    const result = await invoke('calculate_berkus_valuation', {
      factorScores: JSON.stringify(factorScores),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Berkus valuation failed');
    const d = parsed.data || {};
    // Normalize: Python returns total_valuation, frontend expects valuation
    return { ...d, valuation: d.total_valuation ?? d.valuation ?? 0 };
  }

  /**
   * Calculate Scorecard Method valuation
   */
  static async calculateScorecardValuation(
    stage: string,
    region: string,
    factorAssessments: Record<string, number>
  ): Promise<any> {
    const result = await invoke('calculate_scorecard_valuation', {
      stage,
      region,
      factorAssessments: JSON.stringify(factorAssessments),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Scorecard valuation failed');
    const d = parsed.data || {};
    // Normalize: Python returns final_valuation, frontend expects valuation
    return { ...d, valuation: d.final_valuation ?? d.valuation ?? 0 };
  }

  /**
   * Calculate VC Method valuation
   */
  static async calculateVCMethodValuation(
    exitYearMetric: number,
    exitMultiple: number,
    yearsToExit: number,
    investmentAmount: number,
    stage: string
  ): Promise<any> {
    const result = await invoke('calculate_vc_method_valuation', {
      exitYearMetric,
      exitMultiple,
      yearsToExit,
      investmentAmount,
      stage,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'VC method valuation failed');
    const d = parsed.data || {};
    // Normalize: Python returns investor_ownership_pct (0-100), frontend expects ownership_percentage (0-1)
    return {
      ...d,
      ownership_percentage: (d.investor_ownership_pct ?? 0) / 100,
    };
  }

  /**
   * Calculate First Chicago Method valuation
   */
  static async calculateFirstChicagoValuation(
    scenarios: Array<{
      name: string;
      probability: number;
      exit_year: number;
      exit_value: number;
      description: string;
    }>
  ): Promise<any> {
    const result = await invoke('calculate_first_chicago_valuation', {
      scenarios: JSON.stringify(scenarios),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'First Chicago valuation failed');
    const d = parsed.data || {};
    // Normalize: Python returns expected_present_value, frontend checks weighted_valuation
    // Also extract scenario_values array for display
    return {
      ...d,
      weighted_valuation: d.expected_present_value ?? d.weighted_valuation ?? d.valuation ?? 0,
      scenario_values: Array.isArray(d.scenarios)
        ? d.scenarios.map((s: any) => s.present_value ?? s.exit_value ?? 0)
        : [],
    };
  }

  /**
   * Calculate Risk Factor Summation valuation
   */
  static async calculateRiskFactorValuation(
    baseValuation: number,
    riskAssessments: Record<string, number>
  ): Promise<any> {
    const result = await invoke('calculate_risk_factor_valuation', {
      baseValuation,
      riskAssessments: JSON.stringify(riskAssessments),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Risk factor valuation failed');
    const d = parsed.data || {};
    // Normalize: Python returns final_valuation and base_valuation, frontend expects adjusted_valuation
    return {
      ...d,
      adjusted_valuation: d.final_valuation ?? d.adjusted_valuation ?? 0,
      base_valuation: d.base_valuation ?? 0,
    };
  }

  /**
   * Comprehensive startup valuation using all methods
   */
  static async comprehensiveStartupValuation(
    inputs: StartupValuationInputs
  ): Promise<any> {
    const result = await invoke('comprehensive_startup_valuation', {
      startupName: inputs.startup_name,
      berkusScores: inputs.berkus_scores ? JSON.stringify(inputs.berkus_scores) : undefined,
      scorecardInputs: inputs.scorecard_inputs ? JSON.stringify(inputs.scorecard_inputs) : undefined,
      vcInputs: inputs.vc_inputs ? JSON.stringify(inputs.vc_inputs) : undefined,
      firstChicagoScenarios: inputs.first_chicago_scenarios
        ? JSON.stringify(inputs.first_chicago_scenarios)
        : undefined,
      riskFactorAssessments: inputs.risk_factor_assessments
        ? JSON.stringify(inputs.risk_factor_assessments)
        : undefined,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Comprehensive startup valuation failed');
    const d = parsed.data || {};
    // Normalize: Python returns valuations_by_method, frontend expects methods
    // Each method entry has {valuation, method, applicability, details}
    return {
      ...d,
      methods: d.valuations_by_method ?? d.methods ?? {},
    };
  }

  /**
   * Quick pre-revenue valuation
   */
  static async quickPreRevenueValuation(
    ideaQuality: number,
    teamQuality: number,
    prototypeStatus: number,
    marketSize: number
  ): Promise<any> {
    const result = await invoke('quick_pre_revenue_valuation', {
      ideaQuality,
      teamQuality,
      prototypeStatus,
      marketSize,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Quick pre-revenue valuation failed');
    return parsed.data;
  }
}

// =============================================================================
// DEAL STRUCTURE
// =============================================================================

export class DealStructureService {
  /**
   * Analyze payment structure
   */
  static async analyzePaymentStructure(
    purchasePrice: number,
    cashPercentage: number,
    acquirerCash: number,
    debtCapacity: number
  ): Promise<any> {
    const result = await invoke('analyze_payment_structure', {
      purchasePrice,
      cashPercentage,
      acquirerCash,
      debtCapacity,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Payment structure analysis failed');
    return parsed.data;
  }

  /**
   * Value earnout provision
   */
  static async valueEarnout(
    earnoutParams: Record<string, any>,
    financialProjections: Record<string, any>
  ): Promise<any> {
    const result = await invoke('value_earnout', {
      earnoutParams: JSON.stringify(earnoutParams),
      financialProjections: JSON.stringify(financialProjections),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Earnout valuation failed');
    return parsed.data;
  }

  /**
   * Calculate exchange ratio
   */
  static async calculateExchangeRatio(
    acquirerPrice: number,
    targetPrice: number,
    offerPremium: number
  ): Promise<any> {
    const result = await invoke('calculate_exchange_ratio', {
      acquirerPrice,
      targetPrice,
      offerPremium,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Exchange ratio calculation failed');
    return parsed.data;
  }

  /**
   * Analyze collar mechanism
   */
  static async analyzeCollarMechanism(
    collarParams: Record<string, any>,
    priceScenarios: Record<string, any>
  ): Promise<any> {
    const result = await invoke('analyze_collar_mechanism', {
      collarParams: JSON.stringify(collarParams),
      priceScenarios: JSON.stringify(priceScenarios),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Collar mechanism analysis failed');
    return parsed.data;
  }

  /**
   * Value Contingent Value Right (CVR)
   */
  static async valueCVR(
    cvrType: string,
    cvrParams: Record<string, any>
  ): Promise<any> {
    const result = await invoke('value_cvr', {
      cvrType,
      cvrParams: JSON.stringify(cvrParams),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'CVR valuation failed');
    return parsed.data;
  }
}

// =============================================================================
// SYNERGIES
// =============================================================================

export class SynergiesService {
  /**
   * Calculate revenue synergies
   */
  static async calculateRevenueSynergies(
    synergyType: string,
    synergyParams: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_revenue_synergies', {
      synergyType,
      synergyParams: JSON.stringify(synergyParams),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Revenue synergy calculation failed');
    return parsed.data;
  }

  /**
   * Calculate cost synergies
   */
  static async calculateCostSynergies(
    synergyType: string,
    synergyParams: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_cost_synergies', {
      synergyType,
      synergyParams: JSON.stringify(synergyParams),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Cost synergy calculation failed');
    return parsed.data;
  }

  /**
   * Estimate integration costs
   */
  static async estimateIntegrationCosts(
    dealSize: number,
    complexityLevel: string,
    integrationPlan: Record<string, any>
  ): Promise<any> {
    const result = await invoke('estimate_integration_costs', {
      dealSize,
      complexityLevel,
      integrationPlan: JSON.stringify(integrationPlan),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Integration cost estimation failed');
    return parsed.data;
  }

  /**
   * Value synergies using DCF
   */
  static async valueSynergiesDCF(
    revenueSynergies: Record<string, any>,
    costSynergies: Record<string, any>,
    integrationCosts: Record<string, any>,
    discountRate: number
  ): Promise<any> {
    const result = await invoke('value_synergies_dcf', {
      revenueSynergies: JSON.stringify(revenueSynergies),
      costSynergies: JSON.stringify(costSynergies),
      integrationCosts: JSON.stringify(integrationCosts),
      discountRate,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Synergy DCF valuation failed');
    return parsed.data;
  }
}

// =============================================================================
// FAIRNESS OPINION
// =============================================================================

export class FairnessOpinionService {
  /**
   * Generate fairness opinion
   */
  static async generateFairnessOpinion(
    inputs: FairnessOpinionInputs
  ): Promise<any> {
    const result = await invoke('generate_fairness_opinion', {
      valuationMethods: JSON.stringify(inputs.valuation_methods),
      offerPrice: inputs.offer_price,
      qualitativeFactors: JSON.stringify(inputs.qualitative_factors),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Fairness opinion generation failed');
    const d = parsed.data || {};
    const wvr = d.weighted_valuation_range || {};
    // Normalize: build valuation_summary that the frontend expects
    return {
      ...d,
      valuation_summary: {
        average: wvr.midpoint ?? 0,
        median: wvr.midpoint ?? 0,
        range_low: wvr.low ?? 0,
        range_high: wvr.high ?? 0,
        ...(d.valuation_summary || {}),
      },
    };
  }

  /**
   * Analyze premium fairness
   */
  static async analyzePremiumFairness(
    dailyPrices: number[],
    offerPrice: number,
    announcementDate?: number
  ): Promise<any> {
    const result = await invoke('analyze_premium_fairness', {
      dailyPrices: JSON.stringify(dailyPrices),
      offerPrice,
      announcementDate,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Premium fairness analysis failed');
    const d = parsed.data || {};
    // Python returns premiums_to_unaffected.{1_day,5_day,20_day,60_day}
    // Frontend checks premium_1day, premium_1week, premium_1month, premium_52week_high
    const pu = d.premiums_to_unaffected || {};
    // Also handle calculate_premiums format: premiums.{1_day,1_week,1_month}.premium_pct
    const cp = d.premiums || {};
    const getPremium = (puKey: string, cpKey: string) =>
      pu[puKey] ?? cp[cpKey]?.premium_pct ?? undefined;
    return {
      ...d,
      premium_1day: getPremium('1_day', '1_day'),
      premium_1week: getPremium('5_day', '1_week'),
      premium_1month: getPremium('20_day', '1_month'),
      premium_52week_high: getPremium('60_day', '52_week'),
    };
  }

  /**
   * Assess process quality
   */
  static async assessProcessQuality(
    processFactors: Record<string, any>
  ): Promise<any> {
    const result = await invoke('assess_process_quality', {
      processFactors: JSON.stringify(processFactors),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Process quality assessment failed');
    const d = parsed.data || {};
    // For flat input mode, overall_score is already 1-5. For nested, we computed it.
    // For nested without comprehensive (missing data), compute from available component scores.
    const comprehensiveScore = d.comprehensive?.overall_process_score;
    return {
      ...d,
      overall_score: d.overall_score ?? (comprehensiveScore != null ? comprehensiveScore / 20 : undefined),
      factor_scores: d.factor_scores ?? {},
    };
  }
}

// =============================================================================
// INDUSTRY METRICS
// =============================================================================

export class IndustryMetricsService {
  /**
   * Calculate technology sector metrics
   */
  static async calculateTechMetrics(
    sector: 'saas' | 'marketplace' | 'semiconductor',
    companyData: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_tech_metrics', {
      sector,
      companyData: JSON.stringify(companyData),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Tech metrics calculation failed');
    const d = parsed.data || {};
    // Ensure standard output fields are always present
    return {
      ...d,
      valuation_metrics: d.valuation_metrics ?? {},
      benchmarks: d.benchmarks ?? {},
      insights: Array.isArray(d.insights) ? d.insights : [],
    };
  }

  /**
   * Calculate healthcare sector metrics
   */
  static async calculateHealthcareMetrics(
    sector: 'pharma' | 'biotech' | 'devices' | 'services',
    companyData: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_healthcare_metrics', {
      sector,
      companyData: JSON.stringify(companyData),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Healthcare metrics calculation failed');
    const d = parsed.data || {};
    return {
      ...d,
      valuation_metrics: d.valuation_metrics ?? {},
      benchmarks: d.benchmarks ?? {},
      insights: Array.isArray(d.insights) ? d.insights : [],
    };
  }

  /**
   * Calculate financial services metrics
   */
  static async calculateFinancialServicesMetrics(
    sector: 'banking' | 'insurance' | 'asset_management',
    institutionData: Record<string, any>
  ): Promise<any> {
    const result = await invoke('calculate_financial_services_metrics', {
      sector,
      institutionData: JSON.stringify(institutionData),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Financial services metrics calculation failed');
    const d = parsed.data || {};
    return {
      ...d,
      valuation_metrics: d.valuation_metrics ?? {},
      benchmarks: d.benchmarks ?? {},
      insights: Array.isArray(d.insights) ? d.insights : [],
    };
  }
}

// =============================================================================
// ADVANCED ANALYTICS
// =============================================================================

export class AdvancedAnalyticsService {
  /**
   * Run Monte Carlo valuation simulation
   */
  static async runMonteCarloValuation(
    baseValuation: number,
    revenueGrowthMean: number,
    revenueGrowthStd: number,
    marginMean: number,
    marginStd: number,
    discountRate: number,
    simulations: number = 10000
  ): Promise<any> {
    const result = await invoke('run_monte_carlo_valuation', {
      baseValuation,
      revenueGrowthMean,
      revenueGrowthStd,
      marginMean,
      marginStd,
      discountRate,
      simulations,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Monte Carlo valuation failed');
    const d = parsed.data || {};
    // Ensure flat top-level fields frontend expects: mean, median, std, p5, p25, p75, p95
    return {
      ...d,
      mean: d.mean ?? d.valuation_statistics?.mean ?? 0,
      median: d.median ?? d.valuation_statistics?.median ?? 0,
      std: d.std ?? d.valuation_statistics?.std ?? 0,
      p5: d.p5 ?? d.valuation_statistics?.percentile_5 ?? 0,
      p25: d.p25 ?? d.valuation_statistics?.percentile_25 ?? 0,
      p75: d.p75 ?? d.valuation_statistics?.percentile_75 ?? 0,
      p95: d.p95 ?? d.valuation_statistics?.percentile_95 ?? 0,
    };
  }

  /**
   * Run regression-based valuation
   */
  static async runRegressionValuation(
    compData: Record<string, any>[],
    subjectMetrics: Record<string, any>,
    regressionType: 'ols' | 'multiple'
  ): Promise<any> {
    const result = await invoke('run_regression_valuation', {
      compData: JSON.stringify(compData),
      subjectMetrics: JSON.stringify(subjectMetrics),
      regressionType,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Regression valuation failed');
    const d = parsed.data || {};
    // Normalize output to flat fields the frontend expects
    const rs = (d.regression_statistics || {}) as Record<string, number>;
    return {
      ...d,
      implied_ev: d.implied_ev ?? d.predicted_value ?? 0,
      r_squared: d.r_squared ?? rs.r_squared ?? 0,
      adj_r_squared: d.adj_r_squared ?? rs.adjusted_r_squared ?? 0,
      // coefficients: flatten to {feature: number} if Python returned nested objects
      coefficients: d.coefficients && typeof Object.values(d.coefficients)[0] === 'object'
        ? Object.fromEntries(
            Object.entries(d.coefficients).map(([k, v]: [string, any]) => [k, v?.coefficient ?? v ?? 0])
          )
        : (d.coefficients ?? {}),
      implied_multiples: d.implied_multiples ?? {},
    };
  }
}

// =============================================================================
// DEAL COMPARISON
// =============================================================================

export class DealComparisonService {
  /**
   * Compare multiple deals side-by-side
   * Expected: { comparison: {metric: [v1,v2,...]}, summary: {metric: value} }
   */
  static async compareDeals(deals: MADeal[]): Promise<any> {
    const result = await invoke('compare_deals', {
      deals: JSON.stringify(deals),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Deal comparison failed');
    const d = parsed.data || {};
    return {
      ...d,
      comparison: d.comparison ?? {},
      summary: d.summary ?? {},
      num_deals: d.num_deals ?? deals.length,
    };
  }

  /**
   * Rank deals by criteria
   * Expected: { rankings: [{target_name, acquirer_name, premium_1day|deal_value|ev_revenue|ev_ebitda|synergies}] }
   */
  static async rankDeals(
    deals: MADeal[],
    criteria: 'premium' | 'deal_value' | 'ev_revenue' | 'ev_ebitda' | 'synergies'
  ): Promise<any> {
    const result = await invoke('rank_deals', {
      deals: JSON.stringify(deals),
      criteria,
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Deal ranking failed');
    const d = parsed.data || {};
    return {
      ...d,
      rankings: Array.isArray(d.rankings) ? d.rankings : [],
      ranking_criteria: d.ranking_criteria ?? criteria,
    };
  }

  /**
   * Benchmark deal premium against comparables
   * Expected: { premium_comparison: {target, median, percentile}, insight: string }
   */
  static async benchmarkDealPremium(
    targetDeal: MADeal,
    comparableDeals: MADeal[]
  ): Promise<any> {
    const result = await invoke('benchmark_deal_premium', {
      targetDeal: JSON.stringify(targetDeal),
      comparableDeals: JSON.stringify(comparableDeals),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Deal premium benchmark failed');
    const d = parsed.data || {};
    return {
      ...d,
      premium_comparison: d.premium_comparison ?? null,
      insight: d.insight ?? '',
      above_median: Boolean(d.above_median),
    };
  }

  /**
   * Analyze payment structures across deals
   * Expected: { distribution: {all_cash: count, all_stock: count, mixed: count}, stats_by_type: {...} }
   */
  static async analyzePaymentStructures(deals: MADeal[]): Promise<any> {
    const result = await invoke('analyze_payment_structures', {
      deals: JSON.stringify(deals),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Payment structure analysis failed');
    const d = parsed.data || {};
    return {
      ...d,
      distribution: d.distribution ?? {},
      stats_by_type: d.stats_by_type ?? {},
      total_deals: d.total_deals ?? deals.length,
    };
  }

  /**
   * Analyze deals by industry
   * Expected: { by_industry: {IndustryName: {count, avg_premium, avg_ev_revenue, avg_ev_ebitda, total_value}} }
   */
  static async analyzeIndustryDeals(deals: MADeal[]): Promise<any> {
    const result = await invoke('analyze_industry_deals', {
      deals: JSON.stringify(deals),
    });
    const parsed = JSON.parse(result as string);
    if (!parsed.success) throw new Error(parsed.error || 'Industry deal analysis failed');
    const d = parsed.data || {};
    return {
      ...d,
      by_industry: d.by_industry ?? {},
      industries: d.industries ?? [],
      total_deals: d.total_deals ?? deals.length,
    };
  }
}

// =============================================================================
// UNIFIED SERVICE EXPORT
// =============================================================================

export const MAAnalyticsService = {
  DealDatabase: DealDatabaseService,
  Valuation: ValuationService,
  MergerModel: MergerModelService,
  LBO: LBOModelService,
  StartupValuation: StartupValuationService,
  DealStructure: DealStructureService,
  Synergies: SynergiesService,
  FairnessOpinion: FairnessOpinionService,
  IndustryMetrics: IndustryMetricsService,
  AdvancedAnalytics: AdvancedAnalyticsService,
  DealComparison: DealComparisonService,
};

export default MAAnalyticsService;
