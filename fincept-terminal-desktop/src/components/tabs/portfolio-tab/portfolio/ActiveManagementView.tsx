import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Target, Activity, Award, AlertTriangle } from 'lucide-react';
import { activeManagementService, type ComprehensiveAnalysisResult } from '@/services/portfolio/activeManagementService';
import { formatPercent, formatNumber } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';

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
        backgroundColor: FINCEPT.DARK_BG,
        padding: '24px',
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: '13px',
          fontWeight: 700,
          marginBottom: '8px',
          letterSpacing: '0.5px',
          textTransform: 'uppercase'
        }}>
          Running Active Management Analysis...
        </div>
        <div
          className="animate-spin"
          style={{
            width: '32px',
            height: '32px',
            borderRadius: '50%',
            border: '2px solid transparent',
            borderBottomColor: FINCEPT.ORANGE,
            margin: '0 auto'
          }}
        />
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        padding: '24px',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(255,59,59,0.1)',
          border: BORDERS.RED,
          borderRadius: '2px',
          color: FINCEPT.RED,
          fontSize: '10px'
        }}>
          <AlertTriangle size={16} style={{ display: 'inline-block', marginRight: '4px' }} />
          {error}
        </div>
      </div>
    );
  }

  if (!analysis) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        padding: '24px',
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <Activity size={48} color={FINCEPT.GRAY} style={{ margin: '0 auto 8px' }} />
        <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>No analysis data yet</div>
        <div style={{ marginTop: '4px', fontSize: '11px', color: FINCEPT.GRAY }}>
          Portfolio needs return data to perform active management analysis
        </div>
      </div>
    );
  }

  const { value_added_analysis, information_ratio_analysis, active_management_assessment, improvement_recommendations } = analysis;

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      {/* Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        borderBottom: BORDERS.ORANGE,
        padding: '8px',
        position: 'sticky',
        top: 0,
        zIndex: 10,
        marginBottom: '0px'
      }}>
        <div style={{
          color: FINCEPT.ORANGE,
          fontWeight: 700,
          fontSize: '11px',
          marginBottom: '4px',
          letterSpacing: '0.5px',
          textTransform: 'uppercase'
        }}>
          ACTIVE MANAGEMENT ANALYTICS
        </div>
        <div style={{ display: 'flex', gap: '24px', fontSize: '10px' }}>
          <div>
            <span style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>BENCHMARK: </span>
            <span style={{ color: FINCEPT.CYAN, fontSize: '10px' }}>{benchmarkSymbol}</span>
          </div>
          <div>
            <span style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>QUALITY RATING: </span>
            <span style={{
              color: active_management_assessment.quality_rating === 'Excellent' ? FINCEPT.GREEN :
                active_management_assessment.quality_rating === 'Good' ? FINCEPT.CYAN :
                  active_management_assessment.quality_rating === 'Average' ? FINCEPT.YELLOW : FINCEPT.RED,
              fontWeight: 700,
              fontSize: '10px'
            }}>
              {active_management_assessment.quality_rating}
            </span>
          </div>
          <div>
            <span style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>QUALITY SCORE: </span>
            <span style={{ color: FINCEPT.CYAN, fontSize: '10px' }}>{active_management_assessment.quality_score}/100</span>
          </div>
        </div>
      </div>

      {/* Key Metrics Grid */}
      <div style={{ padding: '8px', display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
        {/* Active Return */}
        <MetricCard
          title="ACTIVE RETURN"
          value={formatPercent(value_added_analysis.active_return_annualized)}
          icon={<TrendingUp size={16} />}
          color={value_added_analysis.active_return_annualized >= 0 ? FINCEPT.GREEN : FINCEPT.RED}
          subtitle="Annualized"
        />

        {/* Information Ratio */}
        <MetricCard
          title="INFORMATION RATIO"
          value={formatNumber(information_ratio_analysis.information_ratio_annualized, 2)}
          icon={<Target size={16} />}
          color={information_ratio_analysis.information_ratio_annualized > 0.5 ? FINCEPT.GREEN : FINCEPT.YELLOW}
          subtitle="Risk-Adjusted Performance"
        />

        {/* Tracking Error */}
        <MetricCard
          title="TRACKING ERROR"
          value={formatPercent(information_ratio_analysis.tracking_error_annualized)}
          icon={<Activity size={16} />}
          color={FINCEPT.CYAN}
          subtitle="Annualized"
        />

        {/* Hit Rate */}
        <MetricCard
          title="HIT RATE"
          value={formatPercent(value_added_analysis.hit_rate)}
          icon={<Award size={16} />}
          color={value_added_analysis.hit_rate > 0.55 ? FINCEPT.GREEN : FINCEPT.YELLOW}
          subtitle="Positive Periods"
        />

        {/* Statistical Significance */}
        <MetricCard
          title="SIGNIFICANCE"
          value={value_added_analysis.statistical_significance.is_significant ? 'YES' : 'NO'}
          icon={<TrendingUp size={16} />}
          color={value_added_analysis.statistical_significance.is_significant ? FINCEPT.GREEN : FINCEPT.RED}
          subtitle={`p-value: ${value_added_analysis.statistical_significance.p_value.toFixed(3)}`}
        />

        {/* T-Statistic */}
        <MetricCard
          title="T-STATISTIC"
          value={formatNumber(value_added_analysis.statistical_significance.t_statistic, 2)}
          icon={<Activity size={16} />}
          color={Math.abs(value_added_analysis.statistical_significance.t_statistic) > 2 ? FINCEPT.GREEN : FINCEPT.YELLOW}
          subtitle="Statistical Measure"
        />
      </div>

      {/* Value Decomposition */}
      <div style={{ padding: '0 8px 8px' }}>
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.STANDARD,
          borderRadius: '2px',
          padding: '8px'
        }}>
          <div style={{
            ...COMMON_STYLES.sectionHeader,
            marginBottom: '8px',
            padding: '8px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            VALUE ADDED DECOMPOSITION
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px', fontSize: '11px' }}>
            <div>
              <div style={{
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '2px'
              }}>ALLOCATION EFFECT</div>
              <div style={{ color: FINCEPT.CYAN, fontSize: '13px', fontWeight: 700 }}>
                {formatPercent(value_added_analysis.value_added_decomposition.estimated_allocation_effect)}
              </div>
            </div>
            <div>
              <div style={{
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '2px'
              }}>SELECTION EFFECT</div>
              <div style={{ color: FINCEPT.CYAN, fontSize: '13px', fontWeight: 700 }}>
                {formatPercent(value_added_analysis.value_added_decomposition.estimated_selection_effect)}
              </div>
            </div>
          </div>
          {value_added_analysis.value_added_decomposition.note && (
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '4px', fontStyle: 'italic' }}>
              Note: {value_added_analysis.value_added_decomposition.note}
            </div>
          )}
        </div>
      </div>

      {/* Strengths & Improvements */}
      <div style={{ padding: '0 8px 8px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
        {/* Strengths */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.GREEN,
          borderRadius: '2px',
          padding: '8px'
        }}>
          <div style={{
            color: FINCEPT.GREEN,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '4px'
          }}>
            KEY STRENGTHS
          </div>
          <ul style={{ margin: 0, paddingLeft: '12px', fontSize: '10px', color: FINCEPT.WHITE }}>
            {active_management_assessment.key_strengths.map((strength, idx) => (
              <li key={idx} style={{ marginBottom: '2px' }}>{strength}</li>
            ))}
          </ul>
        </div>

        {/* Areas for Improvement */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.YELLOW}`,
          borderRadius: '2px',
          padding: '8px'
        }}>
          <div style={{
            color: FINCEPT.YELLOW,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '4px'
          }}>
            AREAS FOR IMPROVEMENT
          </div>
          <ul style={{ margin: 0, paddingLeft: '12px', fontSize: '10px', color: FINCEPT.WHITE }}>
            {active_management_assessment.areas_for_improvement.map((area, idx) => (
              <li key={idx} style={{ marginBottom: '2px' }}>{area}</li>
            ))}
          </ul>
        </div>
      </div>

      {/* Recommendations */}
      {improvement_recommendations && improvement_recommendations.length > 0 && (
        <div style={{ padding: '0 8px 8px' }}>
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.CYAN,
            borderRadius: '2px',
            padding: '8px'
          }}>
            <div style={{
              color: FINCEPT.CYAN,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '4px'
            }}>
              IMPROVEMENT RECOMMENDATIONS
            </div>
            <ul style={{ margin: 0, paddingLeft: '12px', fontSize: '10px', color: FINCEPT.WHITE }}>
              {improvement_recommendations.map((rec, idx) => (
                <li key={idx} style={{ marginBottom: '2px' }}>{rec}</li>
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
  const [hovered, setHovered] = useState(false);

  return (
    <div
      style={{
        backgroundColor: hovered ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
        border: BORDERS.STANDARD,
        borderRadius: '2px',
        padding: '12px',
        transition: EFFECTS.TRANSITION_STANDARD,
        cursor: 'default'
      }}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
    >
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '4px'
      }}>
        <div style={{
          color: FINCEPT.GRAY,
          fontSize: '9px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          textTransform: 'uppercase'
        }}>
          {title}
        </div>
        <div style={{ color }}>{icon}</div>
      </div>
      <div style={{
        color: FINCEPT.CYAN,
        fontSize: '15px',
        fontWeight: 700,
        marginBottom: '2px'
      }}>
        {value}
      </div>
      {subtitle && (
        <div style={{
          color: FINCEPT.GRAY,
          fontSize: '9px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          textTransform: 'uppercase'
        }}>{subtitle}</div>
      )}
    </div>
  );
};

export default ActiveManagementView;
