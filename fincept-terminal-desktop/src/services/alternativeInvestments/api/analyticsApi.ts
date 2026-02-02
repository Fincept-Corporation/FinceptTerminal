/**
 * Alternative Investments Analytics API
 * Integrates with Python backend CLI
 */

import { invoke } from '@tauri-apps/api/core';
import {
  AnalysisResult,
  HighYieldBondParams,
  EmergingMarketBondParams,
  ConvertibleBondParams,
  PreferredStockParams,
  RealEstateParams,
  REITParams,
  HedgeFundParams,
  ManagedFuturesParams,
  MarketNeutralParams,
  CommodityParams,
  PreciousMetalsParams,
  NaturalResourceParams,
  PrivateEquityParams,
  FixedAnnuityParams,
  VariableAnnuityParams,
  EquityIndexedAnnuityParams,
  StructuredProductParams,
  LeveragedFundParams,
  TIPSParams,
  IBondParams,
  InflationProtectedParams,
  StableValueParams,
  CoveredCallParams,
  AssetLocationParams,
  SRIFundParams,
  DigitalAssetParams,
} from './types';

export class AlternativeInvestmentApi {
  private static readonly SCRIPT_PATH = 'Analytics/alternateInvestment/cli.py';

  /**
   * Execute Python CLI command
   */
  private static async executePythonAnalysis(
    command: string,
    data: any,
    method: string = 'verdict'
  ): Promise<AnalysisResult> {
    try {
      const args = [
        command,
        '--data',
        JSON.stringify(data),
        '--method',
        method,
      ];

      const result = await invoke<string>('execute_python_command', {
        script: this.SCRIPT_PATH,
        args,
      });

      return JSON.parse(result) as AnalysisResult;
    } catch (error) {
      console.error(`Error executing ${command} analysis:`, error);
      throw new Error(`Failed to analyze ${command}: ${error}`);
    }
  }

  // ============================================================================
  // Bonds & Fixed Income
  // ============================================================================

  static async analyzeHighYieldBond(
    params: HighYieldBondParams,
    method: 'yield_spread' | 'default_risk' | 'equity_risk' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('high-yield', params, method);
  }

  static async analyzeEmergingMarketBond(
    params: EmergingMarketBondParams,
    method: 'yield_spread' | 'default_risk' | 'currency_risk' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('em-bonds', params, method);
  }

  static async analyzeConvertibleBond(
    params: ConvertibleBondParams,
    method: 'participation' | 'credit_risk' | 'comparison' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('convertible-bonds', params, method);
  }

  static async analyzePreferredStock(
    params: PreferredStockParams,
    method: 'yield_analysis' | 'call_risk' | 'dividend_safety' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('preferred-stocks', params, method);
  }

  // ============================================================================
  // Real Estate
  // ============================================================================

  static async analyzeRealEstate(
    params: RealEstateParams,
    method: 'noi' | 'caprate' | 'dcf' | 'metrics' = 'metrics'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('real-estate', params, method);
  }

  static async analyzeREIT(
    params: REITParams,
    method: 'diversification' | 'expense' | 'regional' | 'metrics' = 'metrics'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('intl-reit', params, method);
  }

  // ============================================================================
  // Hedge Funds
  // ============================================================================

  static async analyzeHedgeFund(
    params: HedgeFundParams,
    method: 'metrics' | 'performance' | 'fees' | 'verdict' = 'metrics'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('hedge-funds', params, method);
  }

  static async analyzeManagedFutures(
    params: ManagedFuturesParams,
    method: 'crisis_performance' | 'correlation' | 'drawdowns' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('managed-futures', params, method);
  }

  static async analyzeMarketNeutral(
    params: HedgeFundParams,
    method: 'beta_analysis' | 'factor_exposure' | 'leverage_risk' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('market-neutral', params, method);
  }

  // ============================================================================
  // Commodities
  // ============================================================================

  static async analyzeCommodity(
    params: CommodityParams,
    method: 'basis' | 'contango' | 'futures' | 'metrics' = 'metrics'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('natural-resources', params, method);
  }

  static async analyzePreciousMetals(
    params: PreciousMetalsParams,
    method: 'correlation' | 'drawdowns' | 'crisis_performance' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('pme', params, method);
  }

  static async analyzeNaturalResources(
    params: NaturalResourceParams,
    method: 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('natural-resources', params, method);
  }

  // ============================================================================
  // Private Capital
  // ============================================================================

  static async analyzePrivateEquity(
    params: PrivateEquityParams,
    method: 'metrics' | 'irr' | 'moic' | 'verdict' = 'metrics'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('private-capital', params, method);
  }

  // ============================================================================
  // Annuities
  // ============================================================================

  static async analyzeFixedAnnuity(
    params: FixedAnnuityParams,
    method: 'surrender_charges' | 'mortality_credits' | 'alternatives' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('annuities', params, method);
  }

  static async analyzeVariableAnnuity(
    params: VariableAnnuityParams,
    method: 'fees' | 'tax' | 'alternatives' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('variable-annuities', params, method);
  }

  static async analyzeEquityIndexedAnnuity(
    params: EquityIndexedAnnuityParams,
    method: 'participation' | 'caps' | 'surrender' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('eia', params, method);
  }

  // ============================================================================
  // Structured Products
  // ============================================================================

  static async analyzeStructuredProduct(
    params: StructuredProductParams,
    method: 'complexity' | 'costs' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('structured-products', params, method);
  }

  static async analyzeLeveragedFund(
    params: LeveragedFundParams,
    method: 'decay' | 'volatility' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('leveraged-funds', params, method);
  }

  // ============================================================================
  // Inflation Protected
  // ============================================================================

  static async analyzeTIPS(
    params: TIPSParams,
    method: 'real_yield' | 'inflation_scenarios' | 'tax_efficiency' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('tips', params, method);
  }

  static async analyzeIBond(
    params: IBondParams,
    method: 'composite_rate' | 'liquidity' | 'purchase_limit' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('ibonds', params, method);
  }

  static async analyzeInflationProtected(
    params: InflationProtectedParams,
    method: 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('inflation-protected', params, method);
  }

  static async analyzeStableValue(
    params: StableValueParams,
    method: 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('stable-value', params, method);
  }

  // ============================================================================
  // Strategies
  // ============================================================================

  static async analyzeCoveredCall(
    params: CoveredCallParams,
    method: 'tax' | 'opportunity_cost' | 'alternative' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('covered-calls', params, method);
  }

  static async analyzeAssetLocation(
    params: AssetLocationParams,
    method: 'tax_efficiency' | 'optimal_location' | 'value_added' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('asset-location', params, method);
  }

  static async analyzeSRIFund(
    params: SRIFundParams,
    method: 'performance' | 'screening' | 'expenses' | 'approaches' | 'verdict' = 'verdict'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('sri', params, method);
  }

  // ============================================================================
  // Digital Assets
  // ============================================================================

  static async analyzeDigitalAsset(
    params: DigitalAssetParams,
    method: 'fundamental' | 'volatility' | 'onchain' | 'metrics' = 'fundamental'
  ): Promise<AnalysisResult> {
    return this.executePythonAnalysis('digital-assets', params, method);
  }

  // ============================================================================
  // Batch Analysis
  // ============================================================================

  /**
   * Analyze multiple investments for comparison
   */
  static async batchAnalyze(
    analyses: Array<{ type: string; params: any; method?: string }>
  ): Promise<AnalysisResult[]> {
    const promises = analyses.map((analysis) => {
      const method = analysis.method || 'verdict';
      return this.executePythonAnalysis(analysis.type, analysis.params, method);
    });

    return Promise.all(promises);
  }

  /**
   * Get available analysis methods for a given analyzer
   */
  static getAvailableMethods(analyzerType: string): string[] {
    const methodsMap: Record<string, string[]> = {
      'high-yield': ['yield_spread', 'default_risk', 'equity_risk', 'verdict'],
      'em-bonds': ['yield_spread', 'default_risk', 'currency_risk', 'verdict'],
      'convertible-bonds': ['participation', 'credit_risk', 'comparison', 'verdict'],
      'preferred-stocks': ['yield_analysis', 'call_risk', 'dividend_safety', 'verdict'],
      'hedge-funds': ['metrics', 'performance', 'fees', 'verdict'],
      'managed-futures': ['crisis_performance', 'correlation', 'drawdowns', 'verdict'],
      'tips': ['real_yield', 'inflation_scenarios', 'tax_efficiency', 'verdict'],
      'ibonds': ['composite_rate', 'liquidity', 'purchase_limit', 'verdict'],
      'covered-calls': ['tax', 'opportunity_cost', 'alternative', 'verdict'],
      'asset-location': ['tax_efficiency', 'optimal_location', 'value_added', 'verdict'],
      'sri': ['performance', 'screening', 'expenses', 'approaches', 'verdict'],
    };

    return methodsMap[analyzerType] || ['verdict'];
  }
}
