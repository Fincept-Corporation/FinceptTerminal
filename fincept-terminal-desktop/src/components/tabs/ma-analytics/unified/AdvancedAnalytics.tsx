/**
 * Advanced Analytics Panel - Monte Carlo Simulation & Regression Valuation
 *
 * Tools: Monte Carlo simulation for probability distributions, Regression-based valuation
 */

import React, { useState } from 'react';
import { Activity, PlayCircle, Loader2, TrendingUp, Target, Plus, Trash2 } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

type AnalysisType = 'montecarlo' | 'regression';

interface CompanyData {
  name: string;
  ev: number;
  revenue: number;
  ebitda: number;
  growth: number;
}

const DEFAULT_COMPS: CompanyData[] = [
  { name: 'Comp A', ev: 5000, revenue: 1200, ebitda: 350, growth: 15 },
  { name: 'Comp B', ev: 3200, revenue: 850, ebitda: 220, growth: 12 },
  { name: 'Comp C', ev: 7500, revenue: 1800, ebitda: 520, growth: 18 },
  { name: 'Comp D', ev: 4100, revenue: 950, ebitda: 280, growth: 10 },
];

export const AdvancedAnalytics: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('montecarlo');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);

  // Monte Carlo inputs
  const [mcInputs, setMcInputs] = useState({
    baseValuation: 1000,
    revenueGrowthMean: 15,
    revenueGrowthStd: 5,
    marginMean: 25,
    marginStd: 3,
    discountRate: 10,
    simulations: 10000,
  });

  // Regression inputs
  const [regressionType, setRegressionType] = useState<'ols' | 'multiple'>('ols');
  const [compData, setCompData] = useState<CompanyData[]>(DEFAULT_COMPS);
  const [subjectMetrics, setSubjectMetrics] = useState({
    revenue: 1000,
    ebitda: 300,
    growth: 14,
  });

  const runAnalysis = async () => {
    setLoading(true);
    setResult(null);
    try {
      let data;
      if (analysisType === 'montecarlo') {
        data = await MAAnalyticsService.AdvancedAnalytics.runMonteCarloValuation(
          mcInputs.baseValuation,
          mcInputs.revenueGrowthMean,
          mcInputs.revenueGrowthStd,
          mcInputs.marginMean,
          mcInputs.marginStd,
          mcInputs.discountRate,
          mcInputs.simulations
        );
      } else {
        data = await MAAnalyticsService.AdvancedAnalytics.runRegressionValuation(
          compData,
          subjectMetrics,
          regressionType
        );
      }
      setResult(data);
      showSuccess(`${analysisType === 'montecarlo' ? 'Monte Carlo' : 'Regression'} analysis completed`);
    } catch (error) {
      showError(`Analysis failed: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const addComp = () => {
    setCompData([...compData, { name: `Comp ${String.fromCharCode(65 + compData.length)}`, ev: 0, revenue: 0, ebitda: 0, growth: 0 }]);
  };

  const removeComp = (idx: number) => {
    setCompData(compData.filter((_, i) => i !== idx));
  };

  const updateComp = (idx: number, field: keyof CompanyData, value: string | number) => {
    const updated = [...compData];
    updated[idx] = { ...updated[idx], [field]: field === 'name' ? value : parseFloat(String(value)) || 0 };
    setCompData(updated);
  };

  const inputStyle: React.CSSProperties = { ...COMMON_STYLES.inputField };
  const smallInputStyle: React.CSSProperties = { ...COMMON_STYLES.inputField, padding: '4px 8px', fontSize: TYPOGRAPHY.TINY };

  const renderMonteCarloInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
        <div>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>BASE VALUATION ($M)</label>
          <input
            type="number"
            value={mcInputs.baseValuation}
            onChange={e => setMcInputs({ ...mcInputs, baseValuation: parseFloat(e.target.value) || 0 })}
            style={inputStyle}
          />
        </div>
        <div>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>DISCOUNT RATE (%)</label>
          <input
            type="number"
            value={mcInputs.discountRate}
            onChange={e => setMcInputs({ ...mcInputs, discountRate: parseFloat(e.target.value) || 0 })}
            style={inputStyle}
          />
        </div>
      </div>

      <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>REVENUE GROWTH DISTRIBUTION</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>MEAN (%)</label>
            <input
              type="number"
              value={mcInputs.revenueGrowthMean}
              onChange={e => setMcInputs({ ...mcInputs, revenueGrowthMean: parseFloat(e.target.value) || 0 })}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>STD DEV (%)</label>
            <input
              type="number"
              value={mcInputs.revenueGrowthStd}
              onChange={e => setMcInputs({ ...mcInputs, revenueGrowthStd: parseFloat(e.target.value) || 0 })}
              style={inputStyle}
            />
          </div>
        </div>
      </div>

      <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>MARGIN DISTRIBUTION</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>MEAN (%)</label>
            <input
              type="number"
              value={mcInputs.marginMean}
              onChange={e => setMcInputs({ ...mcInputs, marginMean: parseFloat(e.target.value) || 0 })}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>STD DEV (%)</label>
            <input
              type="number"
              value={mcInputs.marginStd}
              onChange={e => setMcInputs({ ...mcInputs, marginStd: parseFloat(e.target.value) || 0 })}
              style={inputStyle}
            />
          </div>
        </div>
      </div>

      <div>
        <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>NUMBER OF SIMULATIONS</label>
        <select
          value={mcInputs.simulations}
          onChange={e => setMcInputs({ ...mcInputs, simulations: parseInt(e.target.value) })}
          style={inputStyle}
        >
          <option value={1000}>1,000</option>
          <option value={5000}>5,000</option>
          <option value={10000}>10,000</option>
          <option value={50000}>50,000</option>
          <option value={100000}>100,000</option>
        </select>
      </div>
    </div>
  );

  const renderRegressionInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      <div>
        <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>REGRESSION TYPE</label>
        <div style={{ display: 'flex', gap: SPACING.SMALL }}>
          <button
            onClick={() => setRegressionType('ols')}
            style={COMMON_STYLES.tabButton(regressionType === 'ols')}
          >
            OLS (SIMPLE)
          </button>
          <button
            onClick={() => setRegressionType('multiple')}
            style={COMMON_STYLES.tabButton(regressionType === 'multiple')}
          >
            MULTIPLE REGRESSION
          </button>
        </div>
      </div>

      <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: SPACING.SMALL }}>
          <span style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE }}>COMPARABLE COMPANIES</span>
          <button
            onClick={addComp}
            style={{ ...COMMON_STYLES.buttonSecondary, display: 'flex', alignItems: 'center', gap: SPACING.TINY, padding: '4px 8px', color: FINCEPT.ORANGE }}
          >
            <Plus size={10} /> ADD
          </button>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, maxHeight: 200, overflowY: 'auto' }}>
          {compData.map((comp, idx) => (
            <div key={idx} style={{ display: 'grid', gridTemplateColumns: '1.5fr 1fr 1fr 1fr 1fr auto', gap: SPACING.TINY, alignItems: 'center' }}>
              <input type="text" value={comp.name} onChange={e => updateComp(idx, 'name', e.target.value)} style={smallInputStyle} placeholder="Name" />
              <input type="number" value={comp.ev} onChange={e => updateComp(idx, 'ev', e.target.value)} style={smallInputStyle} placeholder="EV" />
              <input type="number" value={comp.revenue} onChange={e => updateComp(idx, 'revenue', e.target.value)} style={smallInputStyle} placeholder="Revenue" />
              <input type="number" value={comp.ebitda} onChange={e => updateComp(idx, 'ebitda', e.target.value)} style={smallInputStyle} placeholder="EBITDA" />
              <input type="number" value={comp.growth} onChange={e => updateComp(idx, 'growth', e.target.value)} style={smallInputStyle} placeholder="Growth %" />
              <button
                onClick={() => removeComp(idx)}
                disabled={compData.length <= 2}
                style={{ background: 'none', border: 'none', cursor: compData.length <= 2 ? 'not-allowed' : 'pointer', padding: '4px' }}
              >
                <Trash2 size={12} color={compData.length <= 2 ? FINCEPT.MUTED : FINCEPT.RED} />
              </button>
            </div>
          ))}
        </div>
        <div style={{ marginTop: SPACING.SMALL, display: 'grid', gridTemplateColumns: '1.5fr 1fr 1fr 1fr 1fr auto', gap: SPACING.TINY }}>
          {['Name', 'EV ($M)', 'Rev ($M)', 'EBITDA', 'Growth %', ''].map((h, i) => (
            <span key={i} style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>{h}</span>
          ))}
        </div>
      </div>

      <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>SUBJECT COMPANY METRICS</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.SMALL }}>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>REVENUE ($M)</label>
            <input type="number" value={subjectMetrics.revenue} onChange={e => setSubjectMetrics({ ...subjectMetrics, revenue: parseFloat(e.target.value) || 0 })} style={inputStyle} />
          </div>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>EBITDA ($M)</label>
            <input type="number" value={subjectMetrics.ebitda} onChange={e => setSubjectMetrics({ ...subjectMetrics, ebitda: parseFloat(e.target.value) || 0 })} style={inputStyle} />
          </div>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>GROWTH (%)</label>
            <input type="number" value={subjectMetrics.growth} onChange={e => setSubjectMetrics({ ...subjectMetrics, growth: parseFloat(e.target.value) || 0 })} style={inputStyle} />
          </div>
        </div>
      </div>
    </div>
  );

  const renderMonteCarloResults = () => {
    if (!result) return null;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {/* Key Statistics */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
          {[
            { label: 'MEAN VALUATION', value: `$${(result.mean || 0).toFixed(0)}M` },
            { label: 'MEDIAN VALUATION', value: `$${(result.median || 0).toFixed(0)}M` },
            { label: 'STD DEVIATION', value: `$${(result.std || 0).toFixed(0)}M` },
            { label: 'CV', value: `${((result.std / result.mean) * 100 || 0).toFixed(1)}%` },
          ].map(item => (
            <div key={item.label} style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
              <div style={COMMON_STYLES.dataLabel}>{item.label}</div>
              <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>{item.value}</div>
            </div>
          ))}
        </div>

        {/* Percentiles */}
        <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>VALUATION DISTRIBUTION</div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            {[
              { label: '5th Percentile', key: 'p5', color: '#ef4444' },
              { label: '25th Percentile', key: 'p25', color: '#f97316' },
              { label: '50th Percentile (Median)', key: 'median', color: FINCEPT.ORANGE },
              { label: '75th Percentile', key: 'p75', color: '#22c55e' },
              { label: '95th Percentile', key: 'p95', color: '#10b981' },
            ].map(item => (
              <div key={item.key} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{item.label}</span>
                <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                  <div style={{ width: '96px', height: '6px', borderRadius: '2px', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
                    <div style={{
                      height: '100%',
                      borderRadius: '2px',
                      backgroundColor: item.color,
                      width: `${Math.min(100, ((result[item.key] || 0) / (result.p95 || 1)) * 100)}%`,
                    }} />
                  </div>
                  <span style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, width: '80px', textAlign: 'right' }}>
                    ${(result[item.key] || 0).toFixed(0)}M
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Range */}
        <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>90% CONFIDENCE INTERVAL</div>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.DEFAULT }}>
            <span style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.ORANGE }}>
              ${(result.p5 || 0).toFixed(0)}M
            </span>
            <span style={{ color: FINCEPT.MUTED }}>—</span>
            <span style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.ORANGE }}>
              ${(result.p95 || 0).toFixed(0)}M
            </span>
          </div>
        </div>
      </div>
    );
  };

  const renderRegressionResults = () => {
    if (!result) return null;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {/* Implied Valuation */}
        <div style={{ padding: SPACING.DEFAULT, borderRadius: '2px', textAlign: 'center', backgroundColor: `${FINCEPT.ORANGE}20`, border: `1px solid ${FINCEPT.ORANGE}` }}>
          <div style={COMMON_STYLES.dataLabel}>IMPLIED ENTERPRISE VALUE</div>
          <div style={{ fontSize: '24px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.ORANGE, marginTop: SPACING.TINY }}>
            ${(result.implied_ev || 0).toFixed(0)}M
          </div>
        </div>

        {/* Regression Stats */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
            <div style={COMMON_STYLES.dataLabel}>R-SQUARED</div>
            <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
              {((result.r_squared || 0) * 100).toFixed(1)}%
            </div>
          </div>
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
            <div style={COMMON_STYLES.dataLabel}>ADJ. R-SQUARED</div>
            <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
              {((result.adj_r_squared || 0) * 100).toFixed(1)}%
            </div>
          </div>
        </div>

        {/* Coefficients */}
        {result.coefficients && (
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>REGRESSION COEFFICIENTS</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              {Object.entries(result.coefficients).map(([key, value]) => (
                <div key={key} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{key.replace(/_/g, ' ')}</span>
                  <span style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>{(value as number).toFixed(4)}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Implied Multiples */}
        {result.implied_multiples && (
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>IMPLIED MULTIPLES</div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
              {Object.entries(result.implied_multiples).map(([key, value]) => (
                <div key={key} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '2px' }}>
                  <span style={COMMON_STYLES.dataLabel}>{key.toUpperCase()}</span>
                  <span style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.ORANGE }}>{(value as number).toFixed(1)}x</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    );
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG, fontFamily: TYPOGRAPHY.MONO }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: SPACING.DEFAULT,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <Activity size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>ADVANCED ANALYTICS</span>
          <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>MONTE CARLO & REGRESSION</span>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{ ...COMMON_STYLES.buttonPrimary, display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}
        >
          {loading ? <Loader2 size={10} className="animate-spin" /> : <PlayCircle size={10} />}
          RUN ANALYSIS
        </button>
      </div>

      {/* Analysis Type Tabs */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.SMALL,
        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <button
          onClick={() => { setAnalysisType('montecarlo'); setResult(null); }}
          style={{ ...COMMON_STYLES.tabButton(analysisType === 'montecarlo'), display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}
        >
          <Target size={12} /> MONTE CARLO
        </button>
        <button
          onClick={() => { setAnalysisType('regression'); setResult(null); }}
          style={{ ...COMMON_STYLES.tabButton(analysisType === 'regression'), display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}
        >
          <TrendingUp size={12} /> REGRESSION
        </button>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
          {/* Input Panel */}
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
              {analysisType === 'montecarlo' ? 'MONTE CARLO PARAMETERS' : 'REGRESSION INPUTS'}
            </div>
            {analysisType === 'montecarlo' ? renderMonteCarloInputs() : renderRegressionInputs()}
          </div>

          {/* Results Panel */}
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>ANALYSIS RESULTS</div>
            {loading ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '200px' }}>
                <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
              </div>
            ) : result ? (
              analysisType === 'montecarlo' ? renderMonteCarloResults() : renderRegressionResults()
            ) : (
              <div style={{ ...COMMON_STYLES.emptyState, height: '200px' }}>
                <Activity size={32} style={{ opacity: 0.3, marginBottom: SPACING.SMALL }} />
                <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>Click "RUN ANALYSIS" to start</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default AdvancedAnalytics;
