import { InvestmentVerdict } from '@/services/alternativeInvestments/api/types';

/**
 * Maps Python CLI response to InvestmentVerdict interface
 * Python returns: { analysis_category, analysis_recommendation, key_problems }
 * UI expects: { category, rating, analysis_summary, key_findings, recommendation }
 */
export function mapPythonResponseToVerdict(metrics: any): InvestmentVerdict {
  return {
    category: `THE ${metrics.analysis_category}` as any,
    rating: metrics.rating || 'N/A',
    analysis_summary: metrics.analysis_recommendation || '',
    key_findings: metrics.key_problems || metrics.key_findings || [],
    recommendation: metrics.analysis_recommendation || metrics.recommendation || '',
    when_acceptable: metrics.when_acceptable,
    better_alternatives: metrics.better_alternatives,
  };
}
