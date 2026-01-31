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
  payment_cash_pct: number;
  payment_stock_pct: number;
  ev_revenue?: number;
  ev_ebitda?: number;
  synergies?: number;
  status: string;
  industry: string;
  announced_date: string;
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
  };
}

export interface LBOInputs {
  company_data: {
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
    debt_percent: number;
    equity_percent: number;
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
  ): Promise<any> {
    const result = await invoke('scan_ma_filings', {
      daysBack,
      filingTypes: filingTypes ? JSON.stringify(filingTypes) : undefined,
    });
    const parsed = JSON.parse(result as string);
    if (parsed.success) {
      return parsed.data || [];
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
      return parsed.data || [];
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
  ): Promise<ValuationResult> {
    const result = await invoke('calculate_precedent_transactions', {
      targetData: JSON.stringify(targetData),
      compDeals: JSON.stringify(compDeals),
    });
    return JSON.parse(result as string);
  }

  /**
   * Calculate trading comps valuation
   */
  static async calculateTradingComps(
    targetTicker: string,
    compTickers: string[]
  ): Promise<ValuationResult> {
    const result = await invoke('calculate_trading_comps', {
      targetTicker,
      compTickers: JSON.stringify(compTickers),
    });
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
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
    return JSON.parse(result as string);
  }
}

// =============================================================================
// DEAL COMPARISON
// =============================================================================

export class DealComparisonService {
  /**
   * Compare multiple deals side-by-side
   */
  static async compareDeals(deals: MADeal[]): Promise<any> {
    const result = await invoke('compare_deals', {
      deals: JSON.stringify(deals),
    });
    return JSON.parse(result as string);
  }

  /**
   * Rank deals by criteria
   */
  static async rankDeals(
    deals: MADeal[],
    criteria: 'premium' | 'deal_value' | 'ev_revenue' | 'ev_ebitda' | 'synergies'
  ): Promise<any> {
    const result = await invoke('rank_deals', {
      deals: JSON.stringify(deals),
      criteria,
    });
    return JSON.parse(result as string);
  }

  /**
   * Benchmark deal premium against comparables
   */
  static async benchmarkDealPremium(
    targetDeal: MADeal,
    comparableDeals: MADeal[]
  ): Promise<any> {
    const result = await invoke('benchmark_deal_premium', {
      targetDeal: JSON.stringify(targetDeal),
      comparableDeals: JSON.stringify(comparableDeals),
    });
    return JSON.parse(result as string);
  }

  /**
   * Analyze payment structures across deals
   */
  static async analyzePaymentStructures(deals: MADeal[]): Promise<any> {
    const result = await invoke('analyze_payment_structures', {
      deals: JSON.stringify(deals),
    });
    return JSON.parse(result as string);
  }

  /**
   * Analyze deals by industry
   */
  static async analyzeIndustryDeals(deals: MADeal[]): Promise<any> {
    const result = await invoke('analyze_industry_deals', {
      deals: JSON.stringify(deals),
    });
    return JSON.parse(result as string);
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
