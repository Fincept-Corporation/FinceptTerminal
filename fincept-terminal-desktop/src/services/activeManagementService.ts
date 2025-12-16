// Active Management Service - Portfolio Active Management Analytics
import { invoke } from '@tauri-apps/api/core';

// ==================== TYPES ====================

export interface ActiveValueAddedResult {
  active_return_daily: number;
  active_return_annualized: number;
  tracking_error_daily: number;
  tracking_error_annualized: number;
  information_ratio_daily: number;
  information_ratio_annualized: number;
  hit_rate: number;
  value_added_decomposition: {
    total_active_return: number;
    estimated_allocation_effect: number;
    estimated_selection_effect: number;
    interaction_effect: number;
    note: string;
  };
  statistical_significance: {
    t_statistic: number;
    p_value: number;
    is_significant: boolean;
    confidence_level: number;
  };
}

export interface InformationRatioResult {
  information_ratio_annualized: number;
  tracking_error_annualized: number;
  active_return_annualized: number;
  [key: string]: any;
}

export interface TrackingRiskResult {
  tracking_error: number;
  tracking_error_annualized: number;
  tracking_risk_stability: {
    stability_level: string;
    [key: string]: any;
  };
  [key: string]: any;
}

export interface ComprehensiveAnalysisResult {
  value_added_analysis: ActiveValueAddedResult;
  information_ratio_analysis: InformationRatioResult;
  fundamental_law_analysis: any;
  risk_decomposition: any;
  tracking_risk_analysis: TrackingRiskResult;
  active_management_assessment: {
    quality_score: number;
    quality_rating: string;
    key_strengths: string[];
    areas_for_improvement: string[];
  };
  improvement_recommendations: string[];
}

export interface ManagerCandidate {
  manager_id?: string;
  returns: number[];
  benchmark_returns: number[];
  track_record_years?: number;
  aum_billions?: number;
}

export interface ManagerSelectionResult {
  manager_analysis: {
    [managerId: string]: {
      information_ratio: number;
      tracking_error: number;
      active_return: number;
      overall_score: number;
      strengths: string[];
      concerns: string[];
    };
  };
  manager_ranking: string[];
  top_manager: string | null;
  selection_criteria: any;
  due_diligence_framework: any;
}

// ==================== SERVICE CLASS ====================

class ActiveManagementService {
  /**
   * Calculate value added by active management
   */
  async calculateValueAdded(
    portfolioReturns: number[],
    benchmarkReturns: number[],
    portfolioWeights?: number[]
  ): Promise<ActiveValueAddedResult> {
    try {
      const result = await invoke<string>('calculate_active_value_added', {
        portfolioReturns: JSON.stringify(portfolioReturns),
        benchmarkReturns: JSON.stringify(benchmarkReturns),
        portfolioWeights: portfolioWeights ? JSON.stringify(portfolioWeights) : null,
      });

      const parsed = JSON.parse(result);
      if (parsed.error) {
        throw new Error(parsed.error);
      }
      return parsed;
    } catch (error) {
      console.error('[ActiveManagementService] Value added calculation error:', error);
      throw error;
    }
  }

  /**
   * Calculate information ratio analysis
   */
  async calculateInformationRatio(
    portfolioReturns: number[],
    benchmarkReturns: number[]
  ): Promise<InformationRatioResult> {
    try {
      const result = await invoke<string>('calculate_active_information_ratio', {
        portfolioReturns: JSON.stringify(portfolioReturns),
        benchmarkReturns: JSON.stringify(benchmarkReturns),
      });

      const parsed = JSON.parse(result);
      if (parsed.error) {
        throw new Error(parsed.error);
      }
      return parsed;
    } catch (error) {
      console.error('[ActiveManagementService] Information ratio calculation error:', error);
      throw error;
    }
  }

  /**
   * Calculate tracking risk analysis
   */
  async calculateTrackingRisk(
    portfolioReturns: number[],
    benchmarkReturns: number[]
  ): Promise<TrackingRiskResult> {
    try {
      const result = await invoke<string>('calculate_active_tracking_risk', {
        portfolioReturns: JSON.stringify(portfolioReturns),
        benchmarkReturns: JSON.stringify(benchmarkReturns),
      });

      const parsed = JSON.parse(result);
      if (parsed.error) {
        throw new Error(parsed.error);
      }
      return parsed;
    } catch (error) {
      console.error('[ActiveManagementService] Tracking risk calculation error:', error);
      throw error;
    }
  }

  /**
   * Comprehensive active management analysis
   */
  async comprehensiveAnalysis(
    portfolioData: {
      returns: number[];
      weights?: number[];
      [key: string]: any;
    },
    benchmarkData: {
      returns: number[];
      weights?: number[];
      [key: string]: any;
    }
  ): Promise<ComprehensiveAnalysisResult> {
    try {
      const result = await invoke<string>('comprehensive_active_analysis', {
        portfolioData: JSON.stringify(portfolioData),
        benchmarkData: JSON.stringify(benchmarkData),
      });

      const parsed = JSON.parse(result);
      if (parsed.error) {
        throw new Error(parsed.error);
      }
      return parsed;
    } catch (error) {
      console.error('[ActiveManagementService] Comprehensive analysis error:', error);
      throw error;
    }
  }

  /**
   * Manager selection analysis
   */
  async analyzeManagerSelection(
    managerCandidates: ManagerCandidate[]
  ): Promise<ManagerSelectionResult> {
    try {
      const result = await invoke<string>('analyze_manager_selection', {
        managerCandidates: JSON.stringify(managerCandidates),
      });

      const parsed = JSON.parse(result);
      if (parsed.error) {
        throw new Error(parsed.error);
      }
      return parsed;
    } catch (error) {
      console.error('[ActiveManagementService] Manager selection error:', error);
      throw error;
    }
  }

  /**
   * Helper: Calculate portfolio returns from holdings
   */
  calculatePortfolioReturns(holdings: Array<{symbol: string, prices: number[], quantities: number[]}>): number[] {
    // This would calculate daily returns based on price changes
    // Simplified implementation - actual would need proper date alignment
    if (holdings.length === 0) return [];

    const maxLength = Math.max(...holdings.map(h => h.prices.length));
    const returns: number[] = [];

    for (let i = 1; i < maxLength; i++) {
      let portfolioReturn = 0;
      let totalWeight = 0;

      holdings.forEach(holding => {
        if (i < holding.prices.length) {
          const returnPct = (holding.prices[i] - holding.prices[i - 1]) / holding.prices[i - 1];
          const weight = holding.quantities[i] * holding.prices[i];
          portfolioReturn += returnPct * weight;
          totalWeight += weight;
        }
      });

      returns.push(totalWeight > 0 ? portfolioReturn / totalWeight : 0);
    }

    return returns;
  }

  /**
   * Helper: Fetch benchmark returns (e.g., S&P 500)
   */
  async fetchBenchmarkReturns(benchmarkSymbol: string = '^GSPC', days: number = 252): Promise<number[]> {
    // This would call yfinance or another data service
    // For now, return placeholder
    // TODO: Integrate with yfinance service
    console.warn('[ActiveManagementService] Benchmark fetching not yet implemented');
    return [];
  }
}

// Export singleton instance
export const activeManagementService = new ActiveManagementService();
