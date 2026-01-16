import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Target, Activity, Award, AlertTriangle } from 'lucide-react';
import { activeManagementService, type ComprehensiveAnalysisResult } from '@/services/portfolio/activeManagementService';
import { formatPercent, formatNumber } from './utils';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../bloombergStyles';

interface ActiveManagementViewProps {
  portfolioId: string;
  portfolioData?: {
    returns: number[];
    weights?: number[];
  };
}

const ActiveManagementView: React.FC<ActiveManagementViewProps> = ({ portfolioId, portfolioData }) => {
  const [loading, setLoading] = useState(false);
  const [analysis, setAnalysis] = useState<ComprehensiveAnalysisResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [benchmarkSymbol, setBenchmarkSymbol] = useState('^GSPC'); // S&P 500 default

  useEffect(() => {
    if (portfolioData?.returns && portfolioData.returns.length > 0) {
      runAnalysis();
    }
  }, [portfolioData]);

  const runAnalysis = async () => {
    if (!portfolioData?.returns || portfolioData.returns.length === 0) {
      setError('No portfolio return data available');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      // Fetch benchmark returns using the service
      const benchmarkReturns = await activeManagementService.fetchBenchmarkReturns(
        benchmarkSymbol === '^GSPC' ? 'SPY' : benchmarkSymbol,
        portfolioData.returns.length
      );

      // Require real benchmark data - no fallback to random data
      if (benchmarkReturns.length === 0) {
        setError('Unable to fetch benchmark returns. Please check your internet connection and try again.');
        return;
      }

      const result = await activeManagementService.comprehensiveAnalysis(
        {
          returns: portfolioData.returns,
          weights: portfolioData.weights,
        },
        {
          returns: benchmarkReturns,
        }
      );

      setAnalysis(result);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Analysis failed');
      console.error('[ActiveManagementView] Analysis error:', err);
    } finally {
      setLoading(false);
    }
  };

  if (loading) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG.DARK_BG,
        padding: SPACING.XLARGE,
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{ color: BLOOMBERG.ORANGE, fontSize: TYPOGRAPHY.SUBHEADING, marginBottom: SPACING.MEDIUM }}>
          Running Active Management Analysis...
        </div>
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-orange-500 mx-auto"></div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG.DARK_BG,
        padding: SPACING.XLARGE,
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: 'rgba(255,59,59,0.1)',
          border: BORDERS.RED,
          color: BLOOMBERG.RED,
          fontSize: TYPOGRAPHY.DEFAULT
        }}>
          <AlertTriangle size={16} style={{ display: 'inline-block', marginRight: SPACING.SMALL }} />
          {error}
        </div>
      </div>
    );
  }

  if (!analysis) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG.DARK_BG,
        padding: SPACING.XLARGE,
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <Activity size={48} color={BLOOMBERG.GRAY} style={{ margin: `0 auto ${SPACING.MEDIUM}` }} />
        <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.DEFAULT }}>No analysis data yet</div>
        <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.BODY, color: BLOOMBERG.GRAY }}>
          Portfolio needs return data to perform active management analysis
        </div>
      </div>
    );
  }

  const { value_added_analysis, information_ratio_analysis, active_management_assessment, improvement_recommendations } = analysis;

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG.PANEL_BG,
        borderBottom: BORDERS.ORANGE,
        padding: SPACING.MEDIUM,
        position: 'sticky',
        top: 0,
        zIndex: 10
      }}>
        <div style={{
          color: BLOOMBERG.ORANGE,
          fontWeight: TYPOGRAPHY.BOLD,
          fontSize: TYPOGRAPHY.SUBHEADING,
          marginBottom: SPACING.SMALL
        }}>
          ACTIVE MANAGEMENT ANALYTICS
        </div>
        <div style={{ display: 'flex', gap: SPACING.XLARGE, fontSize: TYPOGRAPHY.BODY }}>
          <div>
            <span style={{ color: BLOOMBERG.GRAY }}>BENCHMARK: </span>
            <span style={{ color: BLOOMBERG.CYAN }}>{benchmarkSymbol}</span>
          </div>
          <div>
            <span style={{ color: BLOOMBERG.GRAY }}>QUALITY RATING: </span>
            <span style={{
              color: active_management_assessment.quality_rating === 'Excellent' ? BLOOMBERG.GREEN :
                active_management_assessment.quality_rating === 'Good' ? BLOOMBERG.CYAN :
                  active_management_assessment.quality_rating === 'Average' ? BLOOMBERG.YELLOW : BLOOMBERG.RED,
              fontWeight: TYPOGRAPHY.BOLD
            }}>
              {active_management_assessment.quality_rating}
            </span>
          </div>
          <div>
            <span style={{ color: BLOOMBERG.GRAY }}>QUALITY SCORE: </span>
            <span style={{ color: BLOOMBERG.WHITE }}>{active_management_assessment.quality_score}/100</span>
          </div>
        </div>
      </div>

      {/* Key Metrics Grid */}
      <div style={{ padding: SPACING.MEDIUM, display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: SPACING.MEDIUM }}>
        {/* Active Return */}
        <MetricCard
          title="ACTIVE RETURN"
          value={formatPercent(value_added_analysis.active_return_annualized)}
          icon={<TrendingUp size={16} />}
          color={value_added_analysis.active_return_annualized >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
          subtitle="Annualized"
        />

        {/* Information Ratio */}
        <MetricCard
          title="INFORMATION RATIO"
          value={formatNumber(information_ratio_analysis.information_ratio_annualized, 2)}
          icon={<Target size={16} />}
          color={information_ratio_analysis.information_ratio_annualized > 0.5 ? BLOOMBERG.GREEN : BLOOMBERG.YELLOW}
          subtitle="Risk-Adjusted Performance"
        />

        {/* Tracking Error */}
        <MetricCard
          title="TRACKING ERROR"
          value={formatPercent(information_ratio_analysis.tracking_error_annualized)}
          icon={<Activity size={16} />}
          color={BLOOMBERG.CYAN}
          subtitle="Annualized"
        />

        {/* Hit Rate */}
        <MetricCard
          title="HIT RATE"
          value={formatPercent(value_added_analysis.hit_rate)}
          icon={<Award size={16} />}
          color={value_added_analysis.hit_rate > 0.55 ? BLOOMBERG.GREEN : BLOOMBERG.YELLOW}
          subtitle="Positive Periods"
        />

        {/* Statistical Significance */}
        <MetricCard
          title="SIGNIFICANCE"
          value={value_added_analysis.statistical_significance.is_significant ? 'YES' : 'NO'}
          icon={<TrendingUp size={16} />}
          color={value_added_analysis.statistical_significance.is_significant ? BLOOMBERG.GREEN : BLOOMBERG.RED}
          subtitle={`p-value: ${value_added_analysis.statistical_significance.p_value.toFixed(3)}`}
        />

        {/* T-Statistic */}
        <MetricCard
          title="T-STATISTIC"
          value={formatNumber(value_added_analysis.statistical_significance.t_statistic, 2)}
          icon={<Activity size={16} />}
          color={Math.abs(value_added_analysis.statistical_significance.t_statistic) > 2 ? BLOOMBERG.GREEN : BLOOMBERG.YELLOW}
          subtitle="Statistical Measure"
        />
      </div>

      {/* Value Decomposition */}
      <div style={{ padding: `0 ${SPACING.MEDIUM} ${SPACING.MEDIUM}` }}>
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: BORDERS.STANDARD,
          padding: SPACING.MEDIUM
        }}>
          <div style={{
            color: BLOOMBERG.ORANGE,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.MEDIUM
          }}>
            VALUE ADDED DECOMPOSITION
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: SPACING.MEDIUM, fontSize: TYPOGRAPHY.BODY }}>
            <div>
              <div style={{ color: BLOOMBERG.GRAY, marginBottom: SPACING.TINY }}>ALLOCATION EFFECT</div>
              <div style={{ color: BLOOMBERG.WHITE, fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD }}>
                {formatPercent(value_added_analysis.value_added_decomposition.estimated_allocation_effect)}
              </div>
            </div>
            <div>
              <div style={{ color: BLOOMBERG.GRAY, marginBottom: SPACING.TINY }}>SELECTION EFFECT</div>
              <div style={{ color: BLOOMBERG.WHITE, fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD }}>
                {formatPercent(value_added_analysis.value_added_decomposition.estimated_selection_effect)}
              </div>
            </div>
          </div>
          {value_added_analysis.value_added_decomposition.note && (
            <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.SMALL, marginTop: SPACING.SMALL, fontStyle: 'italic' }}>
              Note: {value_added_analysis.value_added_decomposition.note}
            </div>
          )}
        </div>
      </div>

      {/* Strengths & Improvements */}
      <div style={{ padding: `0 ${SPACING.MEDIUM} ${SPACING.MEDIUM}`, display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM }}>
        {/* Strengths */}
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: BORDERS.GREEN,
          padding: SPACING.MEDIUM
        }}>
          <div style={{
            color: BLOOMBERG.GREEN,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.SMALL
          }}>
            KEY STRENGTHS
          </div>
          <ul style={{ margin: 0, paddingLeft: SPACING.DEFAULT, fontSize: TYPOGRAPHY.BODY, color: BLOOMBERG.WHITE }}>
            {active_management_assessment.key_strengths.map((strength, idx) => (
              <li key={idx} style={{ marginBottom: SPACING.TINY }}>{strength}</li>
            ))}
          </ul>
        </div>

        {/* Areas for Improvement */}
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.YELLOW}`,
          padding: SPACING.MEDIUM
        }}>
          <div style={{
            color: BLOOMBERG.YELLOW,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.SMALL
          }}>
            AREAS FOR IMPROVEMENT
          </div>
          <ul style={{ margin: 0, paddingLeft: SPACING.DEFAULT, fontSize: TYPOGRAPHY.BODY, color: BLOOMBERG.WHITE }}>
            {active_management_assessment.areas_for_improvement.map((area, idx) => (
              <li key={idx} style={{ marginBottom: SPACING.TINY }}>{area}</li>
            ))}
          </ul>
        </div>
      </div>

      {/* Recommendations */}
      {improvement_recommendations && improvement_recommendations.length > 0 && (
        <div style={{ padding: `0 ${SPACING.MEDIUM} ${SPACING.MEDIUM}` }}>
          <div style={{
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: BORDERS.CYAN,
            padding: SPACING.MEDIUM
          }}>
            <div style={{
              color: BLOOMBERG.CYAN,
              fontSize: TYPOGRAPHY.DEFAULT,
              fontWeight: TYPOGRAPHY.BOLD,
              marginBottom: SPACING.SMALL
            }}>
              IMPROVEMENT RECOMMENDATIONS
            </div>
            <ul style={{ margin: 0, paddingLeft: SPACING.DEFAULT, fontSize: TYPOGRAPHY.BODY, color: BLOOMBERG.WHITE }}>
              {improvement_recommendations.map((rec, idx) => (
                <li key={idx} style={{ marginBottom: SPACING.TINY }}>{rec}</li>
              ))}
            </ul>
          </div>
        </div>
      )}
    </div>
  );
};

// Metric Card Component
interface MetricCardProps {
  title: string;
  value: string;
  icon: React.ReactNode;
  color: string;
  subtitle?: string;
}

const MetricCard: React.FC<MetricCardProps> = ({ title, value, icon, color, subtitle }) => {
  return (
    <div style={{
      backgroundColor: BLOOMBERG.PANEL_BG,
      border: BORDERS.STANDARD,
      padding: SPACING.MEDIUM
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: SPACING.SMALL
      }}>
        <div style={{
          color: BLOOMBERG.GRAY,
          fontSize: TYPOGRAPHY.SMALL,
          fontWeight: TYPOGRAPHY.BOLD
        }}>
          {title}
        </div>
        <div style={{ color }}>{icon}</div>
      </div>
      <div style={{
        color,
        fontSize: TYPOGRAPHY.HEADING,
        fontWeight: TYPOGRAPHY.BOLD,
        marginBottom: SPACING.TINY
      }}>
        {value}
      </div>
      {subtitle && (
        <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.SMALL }}>{subtitle}</div>
      )}
    </div>
  );
};

export default ActiveManagementView;
