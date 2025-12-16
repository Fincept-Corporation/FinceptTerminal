import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Target, Activity, Award, AlertTriangle } from 'lucide-react';
import { activeManagementService, type ComprehensiveAnalysisResult } from '@/services/activeManagementService';
import { getBloombergColors, formatPercent, formatNumber } from './utils';

interface ActiveManagementViewProps {
  portfolioId: string;
  portfolioData?: {
    returns: number[];
    weights?: number[];
  };
}

const ActiveManagementView: React.FC<ActiveManagementViewProps> = ({ portfolioId, portfolioData }) => {
  const BLOOMBERG_COLORS = getBloombergColors();
  const { ORANGE, WHITE, RED, GREEN, GRAY, DARK_BG, PANEL_BG, CYAN, YELLOW } = BLOOMBERG_COLORS;

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
      // Fetch benchmark returns (placeholder for now)
      // TODO: Integrate with yfinance service
      const benchmarkReturns = portfolioData.returns.map(() => Math.random() * 0.02 - 0.01);

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
      <div style={{ padding: '24px', textAlign: 'center' }}>
        <div style={{ color: ORANGE, fontSize: '12px', marginBottom: '12px' }}>
          Running Active Management Analysis...
        </div>
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-orange-500 mx-auto"></div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        padding: '24px',
        backgroundColor: 'rgba(255,0,0,0.1)',
        border: `1px solid ${RED}`,
        color: RED,
        fontSize: '11px'
      }}>
        <AlertTriangle size={16} style={{ display: 'inline-block', marginRight: '8px' }} />
        {error}
      </div>
    );
  }

  if (!analysis) {
    return (
      <div style={{ padding: '24px', textAlign: 'center', color: GRAY, fontSize: '11px' }}>
        <Activity size={48} color={GRAY} style={{ margin: '0 auto 12px' }} />
        <div>No analysis data yet</div>
        <div style={{ marginTop: '8px', fontSize: '10px' }}>
          Portfolio needs return data to perform active management analysis
        </div>
      </div>
    );
  }

  const { value_added_analysis, information_ratio_analysis, active_management_assessment, improvement_recommendations } = analysis;

  return (
    <div style={{ height: '100%', overflow: 'auto' }}>
      {/* Header */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
        padding: '12px',
        position: 'sticky',
        top: 0,
        zIndex: 10
      }}>
        <div style={{ color: ORANGE, fontWeight: 'bold', fontSize: '12px', marginBottom: '8px' }}>
          ACTIVE MANAGEMENT ANALYTICS
        </div>
        <div style={{ display: 'flex', gap: '24px', fontSize: '10px' }}>
          <div>
            <span style={{ color: GRAY }}>BENCHMARK: </span>
            <span style={{ color: CYAN }}>{benchmarkSymbol}</span>
          </div>
          <div>
            <span style={{ color: GRAY }}>QUALITY RATING: </span>
            <span style={{
              color: active_management_assessment.quality_rating === 'Excellent' ? GREEN :
                active_management_assessment.quality_rating === 'Good' ? CYAN :
                  active_management_assessment.quality_rating === 'Average' ? YELLOW : RED,
              fontWeight: 'bold'
            }}>
              {active_management_assessment.quality_rating}
            </span>
          </div>
          <div>
            <span style={{ color: GRAY }}>QUALITY SCORE: </span>
            <span style={{ color: WHITE }}>{active_management_assessment.quality_score}/100</span>
          </div>
        </div>
      </div>

      {/* Key Metrics Grid */}
      <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
        {/* Active Return */}
        <MetricCard
          title="ACTIVE RETURN"
          value={formatPercent(value_added_analysis.active_return_annualized)}
          icon={<TrendingUp size={16} />}
          color={value_added_analysis.active_return_annualized >= 0 ? GREEN : RED}
          subtitle="Annualized"
        />

        {/* Information Ratio */}
        <MetricCard
          title="INFORMATION RATIO"
          value={formatNumber(information_ratio_analysis.information_ratio_annualized, 2)}
          icon={<Target size={16} />}
          color={information_ratio_analysis.information_ratio_annualized > 0.5 ? GREEN : YELLOW}
          subtitle="Risk-Adjusted Performance"
        />

        {/* Tracking Error */}
        <MetricCard
          title="TRACKING ERROR"
          value={formatPercent(information_ratio_analysis.tracking_error_annualized)}
          icon={<Activity size={16} />}
          color={CYAN}
          subtitle="Annualized"
        />

        {/* Hit Rate */}
        <MetricCard
          title="HIT RATE"
          value={formatPercent(value_added_analysis.hit_rate)}
          icon={<Award size={16} />}
          color={value_added_analysis.hit_rate > 0.55 ? GREEN : YELLOW}
          subtitle="Positive Periods"
        />

        {/* Statistical Significance */}
        <MetricCard
          title="SIGNIFICANCE"
          value={value_added_analysis.statistical_significance.is_significant ? 'YES' : 'NO'}
          icon={<TrendingUp size={16} />}
          color={value_added_analysis.statistical_significance.is_significant ? GREEN : RED}
          subtitle={`p-value: ${value_added_analysis.statistical_significance.p_value.toFixed(3)}`}
        />

        {/* T-Statistic */}
        <MetricCard
          title="T-STATISTIC"
          value={formatNumber(value_added_analysis.statistical_significance.t_statistic, 2)}
          icon={<Activity size={16} />}
          color={Math.abs(value_added_analysis.statistical_significance.t_statistic) > 2 ? GREEN : YELLOW}
          subtitle="Statistical Measure"
        />
      </div>

      {/* Value Decomposition */}
      <div style={{ padding: '0 12px 12px' }}>
        <div style={{
          backgroundColor: PANEL_BG,
          border: `1px solid ${GRAY}`,
          padding: '12px'
        }}>
          <div style={{ color: ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '12px' }}>
            VALUE ADDED DECOMPOSITION
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px', fontSize: '10px' }}>
            <div>
              <div style={{ color: GRAY, marginBottom: '4px' }}>ALLOCATION EFFECT</div>
              <div style={{ color: WHITE, fontSize: '12px', fontWeight: 'bold' }}>
                {formatPercent(value_added_analysis.value_added_decomposition.estimated_allocation_effect)}
              </div>
            </div>
            <div>
              <div style={{ color: GRAY, marginBottom: '4px' }}>SELECTION EFFECT</div>
              <div style={{ color: WHITE, fontSize: '12px', fontWeight: 'bold' }}>
                {formatPercent(value_added_analysis.value_added_decomposition.estimated_selection_effect)}
              </div>
            </div>
          </div>
          {value_added_analysis.value_added_decomposition.note && (
            <div style={{ color: GRAY, fontSize: '9px', marginTop: '8px', fontStyle: 'italic' }}>
              Note: {value_added_analysis.value_added_decomposition.note}
            </div>
          )}
        </div>
      </div>

      {/* Strengths & Improvements */}
      <div style={{ padding: '0 12px 12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        {/* Strengths */}
        <div style={{
          backgroundColor: PANEL_BG,
          border: `1px solid ${GREEN}`,
          padding: '12px'
        }}>
          <div style={{ color: GREEN, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
            KEY STRENGTHS
          </div>
          <ul style={{ margin: 0, paddingLeft: '16px', fontSize: '10px', color: WHITE }}>
            {active_management_assessment.key_strengths.map((strength, idx) => (
              <li key={idx} style={{ marginBottom: '4px' }}>{strength}</li>
            ))}
          </ul>
        </div>

        {/* Areas for Improvement */}
        <div style={{
          backgroundColor: PANEL_BG,
          border: `1px solid ${YELLOW}`,
          padding: '12px'
        }}>
          <div style={{ color: YELLOW, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
            AREAS FOR IMPROVEMENT
          </div>
          <ul style={{ margin: 0, paddingLeft: '16px', fontSize: '10px', color: WHITE }}>
            {active_management_assessment.areas_for_improvement.map((area, idx) => (
              <li key={idx} style={{ marginBottom: '4px' }}>{area}</li>
            ))}
          </ul>
        </div>
      </div>

      {/* Recommendations */}
      {improvement_recommendations && improvement_recommendations.length > 0 && (
        <div style={{ padding: '0 12px 12px' }}>
          <div style={{
            backgroundColor: PANEL_BG,
            border: `1px solid ${CYAN}`,
            padding: '12px'
          }}>
            <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              IMPROVEMENT RECOMMENDATIONS
            </div>
            <ul style={{ margin: 0, paddingLeft: '16px', fontSize: '10px', color: WHITE }}>
              {improvement_recommendations.map((rec, idx) => (
                <li key={idx} style={{ marginBottom: '6px' }}>{rec}</li>
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
  const { PANEL_BG, GRAY, WHITE } = getBloombergColors();

  return (
    <div style={{
      backgroundColor: PANEL_BG,
      border: `1px solid ${GRAY}`,
      padding: '12px'
    }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
        <div style={{ color: GRAY, fontSize: '9px', fontWeight: 'bold' }}>{title}</div>
        <div style={{ color }}>{icon}</div>
      </div>
      <div style={{ color, fontSize: '16px', fontWeight: 'bold', marginBottom: '4px' }}>
        {value}
      </div>
      {subtitle && (
        <div style={{ color: GRAY, fontSize: '9px' }}>{subtitle}</div>
      )}
    </div>
  );
};

export default ActiveManagementView;
