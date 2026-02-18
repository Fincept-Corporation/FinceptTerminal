/**
 * Advanced Analytics Panel - Monte Carlo Simulation & Regression Valuation
 *
 * Bloomberg-grade UI with distribution histograms, CDF curves, scatter plots,
 * and residual analysis. Uses shared MA component library for consistent styling.
 */

import React, { useState, useMemo } from 'react';
import { Activity, PlayCircle, Loader2, TrendingUp, Target, Plus, Trash2, BarChart3, ScatterChart as ScatterIcon } from 'lucide-react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Cell, AreaChart, Area, ScatterChart, Scatter, Line, LineChart, ReferenceLine, Legend, ResponsiveContainer } from 'recharts';
import { ChevronDown, ChevronUp } from 'lucide-react';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MAExportButton } from '../components';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES, EFFECTS } from '../../portfolio-tab/finceptStyles';
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

const ACCENT = MA_COLORS.advanced; // #FF3B8E pink

// ---------- Histogram / CDF generation utilities ----------

const generateHistogramBins = (mean: number, std: number, p5: number, p95: number) => {
  const bins = 20;
  const range = p95 - p5;
  const binWidth = range / bins;
  return Array.from({ length: bins }, (_, i) => {
    const x = p5 + i * binWidth;
    const z = (x - mean) / std;
    const freq = Math.exp(-0.5 * z * z);
    let fill: string;
    if (z < -1) fill = '#FF3B3B';
    else if (z < 0) fill = '#FFD700';
    else if (z < 1) fill = '#00D66F';
    else fill = '#00E5FF';
    return {
      x: x / 1e6 >= 1 ? x / 1e6 : x,
      xRaw: x,
      freq: parseFloat(freq.toFixed(4)),
      fill,
      label: x / 1e6 >= 1 ? `${(x / 1e6).toFixed(1)}B` : `${x.toFixed(0)}M`,
    };
  });
};

const generateCdfPoints = (result: any) => {
  const points = [
    { percentile: 5, value: result.p5 || 0 },
    { percentile: 25, value: result.p25 || 0 },
    { percentile: 50, value: result.median || 0 },
    { percentile: 75, value: result.p75 || 0 },
    { percentile: 95, value: result.p95 || 0 },
  ];
  return points.map(p => ({
    ...p,
    label: `P${p.percentile}`,
    displayValue: `$${p.value.toFixed(0)}M`,
  }));
};

const formatValuation = (val: number): string => {
  if (Math.abs(val) >= 1e6) return `$${(val / 1e6).toFixed(1)}B`;
  if (Math.abs(val) >= 1e3) return `$${(val / 1e3).toFixed(1)}B`;
  return `$${val.toFixed(0)}M`;
};

const formatAxisTick = (val: number): string => {
  if (Math.abs(val) >= 1e6) return `${(val / 1e6).toFixed(1)}B`;
  if (Math.abs(val) >= 1e3) return `${(val / 1e3).toFixed(0)}K`;
  return `${val.toFixed(0)}`;
};

// ---------- Collapsible section wrapper ----------

const CollapsibleSection: React.FC<{
  title: string;
  icon?: React.ReactNode;
  defaultOpen?: boolean;
  children: React.ReactNode;
}> = ({ title, icon, defaultOpen = true, children }) => {
  const [open, setOpen] = useState(defaultOpen);
  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
    }}>
      <div
        onClick={() => setOpen(!open)}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: open ? `1px solid ${FINCEPT.BORDER}` : 'none',
          cursor: 'pointer',
          userSelect: 'none',
        }}
      >
        {icon && <span style={{ color: ACCENT, display: 'flex' }}>{icon}</span>}
        <span style={{
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: ACCENT,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase',
        }}>
          {title}
        </span>
        <div style={{ flex: 1 }} />
        {open
          ? <ChevronUp size={12} color={FINCEPT.GRAY} />
          : <ChevronDown size={12} color={FINCEPT.GRAY} />
        }
      </div>
      {open && (
        <div style={{ padding: '12px' }}>
          {children}
        </div>
      )}
    </div>
  );
};

// ---------- Custom chart tooltip ----------

const ChartTooltipContent: React.FC<any> = ({ active, payload, label, formatter }: any) => {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      ...CHART_STYLE.tooltip,
      padding: '8px 12px',
    }}>
      {label !== undefined && (
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: TYPOGRAPHY.MONO }}>
          {label}
        </div>
      )}
      {payload.map((entry: any, i: number) => (
        <div key={i} style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          color: entry.color || FINCEPT.WHITE,
        }}>
          <div style={{ width: 6, height: 6, borderRadius: '1px', backgroundColor: entry.color || ACCENT }} />
          <span style={{ color: FINCEPT.GRAY }}>{entry.name}:</span>
          <span style={{ fontWeight: 700, color: FINCEPT.WHITE }}>
            {formatter ? formatter(entry.value) : entry.value}
          </span>
        </div>
      ))}
    </div>
  );
};

// ================================================================
// MAIN COMPONENT
// ================================================================

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

  // Collapse states
  const [inputsOpen, setInputsOpen] = useState(true);

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

  // ---------- Derived chart data ----------

  const histogramData = useMemo(() => {
    if (!result || analysisType !== 'montecarlo') return [];
    const mean = result.mean || 0;
    const std = result.std || 1;
    const p5 = result.p5 || mean - 2 * std;
    const p95 = result.p95 || mean + 2 * std;
    return generateHistogramBins(mean, std, p5, p95);
  }, [result, analysisType]);

  const cdfData = useMemo(() => {
    if (!result || analysisType !== 'montecarlo') return [];
    return generateCdfPoints(result);
  }, [result, analysisType]);

  const regressionScatterData = useMemo(() => {
    if (!result || analysisType !== 'regression') return [];
    return compData.map(c => ({
      name: c.name,
      ev: c.ev,
      revenue: c.revenue,
      ebitda: c.ebitda,
      isSubject: false,
    }));
  }, [result, compData, analysisType]);

  const subjectPoint = useMemo(() => {
    if (!result || analysisType !== 'regression') return null;
    return {
      name: 'Subject',
      ev: result.implied_ev || 0,
      revenue: subjectMetrics.revenue,
      ebitda: subjectMetrics.ebitda,
      isSubject: true,
    };
  }, [result, subjectMetrics, analysisType]);

  const regressionLineData = useMemo(() => {
    if (!result || analysisType !== 'regression' || !compData.length) return [];
    const sorted = [...compData].sort((a, b) => a.revenue - b.revenue);
    const minRev = sorted[0].revenue;
    const maxRev = sorted[sorted.length - 1].revenue;
    // Simple linear regression fit for overlay
    const n = compData.length;
    const sumX = compData.reduce((s, c) => s + c.revenue, 0);
    const sumY = compData.reduce((s, c) => s + c.ev, 0);
    const sumXY = compData.reduce((s, c) => s + c.revenue * c.ev, 0);
    const sumXX = compData.reduce((s, c) => s + c.revenue * c.revenue, 0);
    const slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX || 1);
    const intercept = (sumY - slope * sumX) / n;
    const steps = 20;
    const stepSize = (maxRev - minRev) / steps;
    return Array.from({ length: steps + 1 }, (_, i) => {
      const x = minRev + i * stepSize;
      return { revenue: x, ev: slope * x + intercept };
    });
  }, [result, compData, analysisType]);

  const residualData = useMemo(() => {
    if (!result || analysisType !== 'regression' || !compData.length) return [];
    // Compute residuals using simple OLS
    const n = compData.length;
    const sumX = compData.reduce((s, c) => s + c.revenue, 0);
    const sumY = compData.reduce((s, c) => s + c.ev, 0);
    const sumXY = compData.reduce((s, c) => s + c.revenue * c.ev, 0);
    const sumXX = compData.reduce((s, c) => s + c.revenue * c.revenue, 0);
    const slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX || 1);
    const intercept = (sumY - slope * sumX) / n;
    return compData.map(c => ({
      name: c.name,
      predicted: slope * c.revenue + intercept,
      actual: c.ev,
      residual: c.ev - (slope * c.revenue + intercept),
    }));
  }, [result, compData, analysisType]);

  // Export data
  const exportData = useMemo(() => {
    if (!result) return [];
    if (analysisType === 'montecarlo') {
      return [
        { metric: 'Mean', value: result.mean || 0 },
        { metric: 'Median', value: result.median || 0 },
        { metric: 'Std Dev', value: result.std || 0 },
        { metric: 'P5', value: result.p5 || 0 },
        { metric: 'P25', value: result.p25 || 0 },
        { metric: 'P50', value: result.median || 0 },
        { metric: 'P75', value: result.p75 || 0 },
        { metric: 'P95', value: result.p95 || 0 },
      ];
    }
    const rows: Record<string, any>[] = [
      { metric: 'Implied EV', value: result.implied_ev || 0 },
      { metric: 'R-Squared', value: result.r_squared || 0 },
      { metric: 'Adj R-Squared', value: result.adj_r_squared || 0 },
    ];
    if (result.coefficients) {
      Object.entries(result.coefficients).forEach(([k, v]) => {
        rows.push({ metric: `Coeff: ${k}`, value: v as number });
      });
    }
    return rows;
  }, [result, analysisType]);

  // ---------- Shared input style ----------

  const inputStyle: React.CSSProperties = { ...COMMON_STYLES.inputField };
  const smallInputStyle: React.CSSProperties = {
    ...COMMON_STYLES.inputField,
    padding: '4px 8px',
    fontSize: TYPOGRAPHY.TINY,
  };

  // ================================================================
  // RENDER - Monte Carlo Inputs
  // ================================================================

  const renderMonteCarloInputs = () => (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
      {/* Row 1 */}
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
      <div>
        <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.TINY }}>SIMULATIONS</label>
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

      {/* Revenue Growth Distribution */}
      <div style={{
        gridColumn: '1 / 3',
        padding: '10px 12px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '2px',
        border: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ ...COMMON_STYLES.dataLabel, color: ACCENT, marginBottom: SPACING.SMALL, letterSpacing: '1px' }}>
          REVENUE GROWTH DISTRIBUTION
        </div>
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

      {/* Margin Distribution */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '2px',
        border: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ ...COMMON_STYLES.dataLabel, color: ACCENT, marginBottom: SPACING.SMALL, letterSpacing: '1px' }}>
          MARGIN DISTRIBUTION
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
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
    </div>
  );

  // ================================================================
  // RENDER - Regression Inputs
  // ================================================================

  const renderRegressionInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Regression type toggle */}
      <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
        <label style={{ ...COMMON_STYLES.dataLabel, marginRight: '8px' }}>TYPE</label>
        <button
          onClick={() => setRegressionType('ols')}
          style={{
            ...COMMON_STYLES.tabButton(regressionType === 'ols'),
            ...(regressionType === 'ols' ? { backgroundColor: ACCENT, color: '#000' } : {}),
          }}
        >
          OLS (SIMPLE)
        </button>
        <button
          onClick={() => setRegressionType('multiple')}
          style={{
            ...COMMON_STYLES.tabButton(regressionType === 'multiple'),
            ...(regressionType === 'multiple' ? { backgroundColor: ACCENT, color: '#000' } : {}),
          }}
        >
          MULTIPLE REGRESSION
        </button>
      </div>

      {/* Subject company metrics in 3-col grid */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '2px',
        border: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ ...COMMON_STYLES.dataLabel, color: ACCENT, marginBottom: SPACING.SMALL, letterSpacing: '1px' }}>
          SUBJECT COMPANY METRICS
        </div>
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

      {/* Comparable companies table */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '2px',
        border: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: SPACING.SMALL }}>
          <span style={{ ...COMMON_STYLES.dataLabel, color: ACCENT, letterSpacing: '1px' }}>COMPARABLE COMPANIES</span>
          <button
            onClick={addComp}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.TINY,
              padding: '4px 8px',
              color: ACCENT,
              borderColor: ACCENT,
            }}
          >
            <Plus size={10} /> ADD
          </button>
        </div>

        {/* Column headers */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1.5fr 1fr 1fr 1fr 1fr 28px',
          gap: SPACING.TINY,
          marginBottom: '4px',
          paddingBottom: '4px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          {['NAME', 'EV ($M)', 'REV ($M)', 'EBITDA', 'GROWTH %', ''].map((h, i) => (
            <span key={i} style={{ fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>{h}</span>
          ))}
        </div>

        {/* Data rows */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '3px', maxHeight: 180, overflowY: 'auto' }}>
          {compData.map((comp, idx) => (
            <div
              key={idx}
              style={{
                display: 'grid',
                gridTemplateColumns: '1.5fr 1fr 1fr 1fr 1fr 28px',
                gap: SPACING.TINY,
                alignItems: 'center',
                padding: '2px 0',
                backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
              }}
            >
              <input type="text" value={comp.name} onChange={e => updateComp(idx, 'name', e.target.value)} style={smallInputStyle} placeholder="Name" />
              <input type="number" value={comp.ev} onChange={e => updateComp(idx, 'ev', e.target.value)} style={smallInputStyle} placeholder="EV" />
              <input type="number" value={comp.revenue} onChange={e => updateComp(idx, 'revenue', e.target.value)} style={smallInputStyle} placeholder="Revenue" />
              <input type="number" value={comp.ebitda} onChange={e => updateComp(idx, 'ebitda', e.target.value)} style={smallInputStyle} placeholder="EBITDA" />
              <input type="number" value={comp.growth} onChange={e => updateComp(idx, 'growth', e.target.value)} style={smallInputStyle} placeholder="Growth %" />
              <button
                onClick={() => removeComp(idx)}
                disabled={compData.length <= 2}
                style={{
                  background: 'none',
                  border: 'none',
                  cursor: compData.length <= 2 ? 'not-allowed' : 'pointer',
                  padding: '4px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                }}
              >
                <Trash2 size={12} color={compData.length <= 2 ? FINCEPT.MUTED : '#FF3B3B'} />
              </button>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  // ================================================================
  // RENDER - Monte Carlo Results
  // ================================================================

  const renderMonteCarloResults = () => {
    if (!result) return null;

    const mean = result.mean || 0;
    const median = result.median || 0;
    const std = result.std || 0;
    const cv = std && mean ? ((std / mean) * 100) : 0;
    const p5 = result.p5 || 0;
    const p25 = result.p25 || 0;
    const p75 = result.p75 || 0;
    const p95 = result.p95 || 0;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Key Metrics Row */}
        <MASectionHeader
          title="Key Statistics"
          subtitle={`${mcInputs.simulations.toLocaleString()} simulations`}
          icon={<Target size={12} />}
          accentColor={ACCENT}
          action={<MAExportButton data={exportData} filename="monte_carlo_results" accentColor={ACCENT} />}
        />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '8px' }}>
          <MAMetricCard label="Mean Valuation" value={`$${mean.toFixed(0)}M`} accentColor={ACCENT} />
          <MAMetricCard label="Median Valuation" value={`$${median.toFixed(0)}M`} accentColor={CHART_PALETTE[1]} />
          <MAMetricCard label="Std Deviation" value={`$${std.toFixed(0)}M`} accentColor={CHART_PALETTE[2]} />
          <MAMetricCard
            label="Coefficient of Var"
            value={`${cv.toFixed(1)}%`}
            accentColor={CHART_PALETTE[3]}
            subtitle={cv < 20 ? 'Low dispersion' : cv < 40 ? 'Moderate dispersion' : 'High dispersion'}
          />
        </div>

        {/* 90% Confidence Interval banner */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '16px',
          padding: '12px 16px',
          backgroundColor: `${ACCENT}10`,
          border: `1px solid ${ACCENT}40`,
          borderRadius: '2px',
        }}>
          <span style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.GRAY, letterSpacing: '1px' }}>
            90% CONFIDENCE INTERVAL
          </span>
          <span style={{ fontSize: '16px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: ACCENT }}>
            ${p5.toFixed(0)}M
          </span>
          <span style={{ color: FINCEPT.MUTED, fontSize: '14px' }}>---</span>
          <span style={{ fontSize: '16px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: ACCENT }}>
            ${p95.toFixed(0)}M
          </span>
        </div>

        {/* Distribution Histogram */}
        {histogramData.length > 0 && (
          <MAChartPanel
            title="Valuation Distribution (Histogram)"
            height={260}
            icon={<BarChart3 size={12} />}
            accentColor={ACCENT}
          >
            <BarChart data={histogramData} margin={{ top: 10, right: 20, left: 10, bottom: 5 }}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis
                dataKey="x"
                tick={{ ...CHART_STYLE.axis }}
                tickFormatter={(v: number) => `${v.toFixed(0)}`}
                label={{ value: 'Valuation ($M)', position: 'insideBottom', offset: -2, style: CHART_STYLE.label }}
              />
              <YAxis
                tick={{ ...CHART_STYLE.axis }}
                label={{ value: 'Probability Density', angle: -90, position: 'insideLeft', offset: 5, style: CHART_STYLE.label }}
              />
              <Tooltip
                content={<ChartTooltipContent formatter={(v: number) => v.toFixed(4)} />}
                cursor={{ fill: 'rgba(255,255,255,0.03)' }}
              />
              <ReferenceLine
                x={histogramData.length > 0 ? histogramData[Math.floor(histogramData.length / 2)]?.x : 0}
                stroke={ACCENT}
                strokeDasharray="4 4"
                strokeWidth={1.5}
                label={{
                  value: 'MEDIAN',
                  position: 'top',
                  style: { fontSize: '8px', fontFamily: 'monospace', fill: ACCENT, fontWeight: 700 },
                }}
              />
              <Bar dataKey="freq" name="Density" radius={[2, 2, 0, 0]}>
                {histogramData.map((entry, idx) => (
                  <Cell key={idx} fill={entry.fill} fillOpacity={0.85} />
                ))}
              </Bar>
            </BarChart>
          </MAChartPanel>
        )}

        {/* CDF Chart */}
        {cdfData.length > 0 && (
          <MAChartPanel
            title="Cumulative Distribution Function (CDF)"
            height={240}
            icon={<TrendingUp size={12} />}
            accentColor={ACCENT}
          >
            <AreaChart data={cdfData} margin={{ top: 10, right: 30, left: 10, bottom: 5 }}>
              <defs>
                <linearGradient id="cdfGradient" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="5%" stopColor={ACCENT} stopOpacity={0.4} />
                  <stop offset="95%" stopColor={ACCENT} stopOpacity={0.05} />
                </linearGradient>
              </defs>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis
                dataKey="value"
                tick={{ ...CHART_STYLE.axis }}
                tickFormatter={(v: number) => `$${v.toFixed(0)}M`}
                label={{ value: 'Enterprise Value', position: 'insideBottom', offset: -2, style: CHART_STYLE.label }}
              />
              <YAxis
                dataKey="percentile"
                tick={{ ...CHART_STYLE.axis }}
                tickFormatter={(v: number) => `${v}%`}
                domain={[0, 100]}
                label={{ value: 'Cumulative %', angle: -90, position: 'insideLeft', offset: 5, style: CHART_STYLE.label }}
              />
              <Tooltip
                content={({ active, payload }) => {
                  if (!active || !payload?.length) return null;
                  const d = payload[0]?.payload;
                  return (
                    <div style={{ ...CHART_STYLE.tooltip, padding: '8px 12px' }}>
                      <div style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: ACCENT, fontWeight: 700 }}>
                        {d?.label}
                      </div>
                      <div style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.WHITE, marginTop: '2px' }}>
                        {d?.displayValue} at {d?.percentile}%
                      </div>
                    </div>
                  );
                }}
              />
              <ReferenceLine
                y={50}
                stroke={FINCEPT.GRAY}
                strokeDasharray="3 3"
                label={{
                  value: '50%',
                  position: 'right',
                  style: { fontSize: '8px', fontFamily: 'monospace', fill: FINCEPT.GRAY },
                }}
              />
              <Area
                type="monotone"
                dataKey="percentile"
                stroke={ACCENT}
                strokeWidth={2}
                fill="url(#cdfGradient)"
                dot={{
                  r: 5,
                  fill: FINCEPT.DARK_BG,
                  stroke: ACCENT,
                  strokeWidth: 2,
                }}
                activeDot={{
                  r: 7,
                  fill: ACCENT,
                  stroke: FINCEPT.WHITE,
                  strokeWidth: 2,
                }}
              />
            </AreaChart>
          </MAChartPanel>
        )}

        {/* Percentile Breakdown */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr',
          gap: '6px',
        }}>
          {[
            { label: 'P5 (Bear)', value: p5, color: '#FF3B3B' },
            { label: 'P25', value: p25, color: '#FF6B35' },
            { label: 'P50 (Median)', value: median, color: ACCENT },
            { label: 'P75', value: p75, color: '#00D66F' },
            { label: 'P95 (Bull)', value: p95, color: '#00E5FF' },
          ].map(item => (
            <MAMetricCard
              key={item.label}
              label={item.label}
              value={`$${item.value.toFixed(0)}M`}
              accentColor={item.color}
              compact
            />
          ))}
        </div>
      </div>
    );
  };

  // ================================================================
  // RENDER - Regression Results
  // ================================================================

  const renderRegressionResults = () => {
    if (!result) return null;

    const impliedEv = result.implied_ev || 0;
    const rSquared = result.r_squared || 0;
    const adjRSquared = result.adj_r_squared || 0;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Header metrics */}
        <MASectionHeader
          title="Regression Results"
          subtitle={regressionType === 'ols' ? 'Ordinary Least Squares' : 'Multiple Regression'}
          icon={<TrendingUp size={12} />}
          accentColor={ACCENT}
          action={<MAExportButton data={exportData} filename="regression_results" accentColor={ACCENT} />}
        />

        {/* Implied EV hero card */}
        <div style={{
          padding: '16px 20px',
          borderRadius: '2px',
          textAlign: 'center',
          backgroundColor: `${ACCENT}12`,
          border: `1px solid ${ACCENT}50`,
          position: 'relative',
          overflow: 'hidden',
        }}>
          <div style={{
            position: 'absolute',
            top: 0,
            left: 0,
            right: 0,
            height: '2px',
            background: `linear-gradient(90deg, transparent, ${ACCENT}, transparent)`,
          }} />
          <div style={{ ...COMMON_STYLES.dataLabel, letterSpacing: '1px' }}>IMPLIED ENTERPRISE VALUE</div>
          <div style={{
            fontSize: '28px',
            fontFamily: TYPOGRAPHY.MONO,
            fontWeight: TYPOGRAPHY.BOLD,
            color: ACCENT,
            marginTop: '6px',
            textShadow: `0 0 20px ${ACCENT}40`,
          }}>
            ${impliedEv.toFixed(0)}M
          </div>
        </div>

        {/* Regression stats */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <MAMetricCard
            label="R-Squared"
            value={`${(rSquared * 100).toFixed(1)}%`}
            accentColor={rSquared > 0.7 ? '#00D66F' : rSquared > 0.4 ? '#FFD700' : '#FF3B3B'}
            subtitle={rSquared > 0.7 ? 'Strong fit' : rSquared > 0.4 ? 'Moderate fit' : 'Weak fit'}
          />
          <MAMetricCard
            label="Adj. R-Squared"
            value={`${(adjRSquared * 100).toFixed(1)}%`}
            accentColor={adjRSquared > 0.7 ? '#00D66F' : adjRSquared > 0.4 ? '#FFD700' : '#FF3B3B'}
            subtitle="Adjusted for predictors"
          />
        </div>

        {/* Scatter plot with regression line */}
        {regressionScatterData.length > 0 && (
          <MAChartPanel
            title="EV vs Revenue - Regression Fit"
            height={280}
            icon={<ScatterIcon size={12} />}
            accentColor={ACCENT}
            actions={
              <span style={{
                fontSize: '9px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                color: rSquared > 0.7 ? '#00D66F' : '#FFD700',
                padding: '2px 6px',
                backgroundColor: `${rSquared > 0.7 ? '#00D66F' : '#FFD700'}20`,
                borderRadius: '2px',
              }}>
                R2 = {(rSquared * 100).toFixed(1)}%
              </span>
            }
          >
            <ScatterChart margin={{ top: 10, right: 30, left: 10, bottom: 5 }}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis
                dataKey="revenue"
                type="number"
                tick={{ ...CHART_STYLE.axis }}
                name="Revenue"
                label={{ value: 'Revenue ($M)', position: 'insideBottom', offset: -2, style: CHART_STYLE.label }}
              />
              <YAxis
                dataKey="ev"
                type="number"
                tick={{ ...CHART_STYLE.axis }}
                name="EV"
                label={{ value: 'Enterprise Value ($M)', angle: -90, position: 'insideLeft', offset: 5, style: CHART_STYLE.label }}
              />
              <Tooltip
                content={({ active, payload }) => {
                  if (!active || !payload?.length) return null;
                  const d = payload[0]?.payload;
                  return (
                    <div style={{ ...CHART_STYLE.tooltip, padding: '8px 12px' }}>
                      <div style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: d?.isSubject ? ACCENT : CHART_PALETTE[1], fontWeight: 700 }}>
                        {d?.name} {d?.isSubject ? '(Subject)' : ''}
                      </div>
                      <div style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY, marginTop: '3px' }}>
                        Rev: ${d?.revenue?.toFixed(0)}M | EV: ${d?.ev?.toFixed(0)}M
                      </div>
                    </div>
                  );
                }}
              />
              <Legend
                wrapperStyle={{ fontSize: '9px', fontFamily: 'monospace' }}
              />
              {/* Comparables */}
              <Scatter
                name="Comparables"
                data={regressionScatterData}
                fill={CHART_PALETTE[1]}
                shape="circle"
                legendType="circle"
              />
              {/* Subject company */}
              {subjectPoint && (
                <Scatter
                  name="Subject Company"
                  data={[subjectPoint]}
                  fill={ACCENT}
                  shape="diamond"
                  legendType="diamond"
                >
                  <Cell fill={ACCENT} />
                </Scatter>
              )}
            </ScatterChart>
          </MAChartPanel>
        )}

        {/* Regression line overlay -- rendered as a separate LineChart since ScatterChart can't overlay lines cleanly */}
        {regressionLineData.length > 0 && (
          <MAChartPanel
            title="Regression Trend Line"
            height={220}
            icon={<TrendingUp size={12} />}
            accentColor={ACCENT}
          >
            <LineChart data={regressionLineData} margin={{ top: 10, right: 30, left: 10, bottom: 5 }}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis
                dataKey="revenue"
                tick={{ ...CHART_STYLE.axis }}
                tickFormatter={(v: number) => `$${v.toFixed(0)}M`}
                label={{ value: 'Revenue ($M)', position: 'insideBottom', offset: -2, style: CHART_STYLE.label }}
              />
              <YAxis
                tick={{ ...CHART_STYLE.axis }}
                tickFormatter={(v: number) => `$${v.toFixed(0)}M`}
                label={{ value: 'Predicted EV ($M)', angle: -90, position: 'insideLeft', offset: 5, style: CHART_STYLE.label }}
              />
              <Tooltip
                content={<ChartTooltipContent formatter={(v: number) => `$${v.toFixed(0)}M`} />}
              />
              <Line
                type="linear"
                dataKey="ev"
                stroke={ACCENT}
                strokeWidth={2}
                dot={false}
                name="Regression Line"
              />
              {subjectPoint && (
                <ReferenceLine
                  x={subjectMetrics.revenue}
                  stroke={CHART_PALETTE[3]}
                  strokeDasharray="4 4"
                  label={{
                    value: `Subject: $${subjectMetrics.revenue}M`,
                    position: 'top',
                    style: { fontSize: '8px', fontFamily: 'monospace', fill: CHART_PALETTE[3], fontWeight: 700 },
                  }}
                />
              )}
            </LineChart>
          </MAChartPanel>
        )}

        {/* Residuals plot */}
        {residualData.length > 0 && (
          <MAChartPanel
            title="Residuals Analysis (Actual - Predicted)"
            height={220}
            icon={<Activity size={12} />}
            accentColor={ACCENT}
          >
            <ScatterChart margin={{ top: 10, right: 30, left: 10, bottom: 5 }}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis
                dataKey="predicted"
                type="number"
                tick={{ ...CHART_STYLE.axis }}
                name="Predicted"
                tickFormatter={(v: number) => `$${v.toFixed(0)}`}
                label={{ value: 'Predicted EV ($M)', position: 'insideBottom', offset: -2, style: CHART_STYLE.label }}
              />
              <YAxis
                dataKey="residual"
                type="number"
                tick={{ ...CHART_STYLE.axis }}
                name="Residual"
                tickFormatter={(v: number) => `${v.toFixed(0)}`}
                label={{ value: 'Residual ($M)', angle: -90, position: 'insideLeft', offset: 5, style: CHART_STYLE.label }}
              />
              <Tooltip
                content={({ active, payload }) => {
                  if (!active || !payload?.length) return null;
                  const d = payload[0]?.payload;
                  return (
                    <div style={{ ...CHART_STYLE.tooltip, padding: '8px 12px' }}>
                      <div style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: ACCENT, fontWeight: 700 }}>
                        {d?.name}
                      </div>
                      <div style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY, marginTop: '3px' }}>
                        Predicted: ${d?.predicted?.toFixed(0)}M
                      </div>
                      <div style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY }}>
                        Actual: ${d?.actual?.toFixed(0)}M
                      </div>
                      <div style={{
                        fontSize: '9px',
                        fontFamily: TYPOGRAPHY.MONO,
                        color: d?.residual >= 0 ? '#00D66F' : '#FF3B3B',
                        fontWeight: 700,
                      }}>
                        Residual: {d?.residual >= 0 ? '+' : ''}{d?.residual?.toFixed(0)}M
                      </div>
                    </div>
                  );
                }}
              />
              <ReferenceLine
                y={0}
                stroke={FINCEPT.GRAY}
                strokeDasharray="4 4"
                strokeWidth={1}
              />
              <Scatter name="Residuals" data={residualData}>
                {residualData.map((entry, idx) => (
                  <Cell
                    key={idx}
                    fill={entry.residual >= 0 ? '#00D66F' : '#FF3B3B'}
                    fillOpacity={0.85}
                  />
                ))}
              </Scatter>
            </ScatterChart>
          </MAChartPanel>
        )}

        {/* Coefficients */}
        {result.coefficients && (
          <CollapsibleSection title="Regression Coefficients" icon={<Activity size={11} />} defaultOpen={true}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {Object.entries(result.coefficients).map(([key, value]) => (
                <div key={key} style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.DARK_BG,
                  borderRadius: '2px',
                }}>
                  <span style={{
                    fontSize: TYPOGRAPHY.TINY,
                    fontFamily: TYPOGRAPHY.MONO,
                    color: FINCEPT.GRAY,
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                  }}>
                    {key.replace(/_/g, ' ')}
                  </span>
                  <span style={{
                    fontSize: TYPOGRAPHY.SMALL,
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                  }}>
                    {(value as number).toFixed(4)}
                  </span>
                </div>
              ))}
            </div>
          </CollapsibleSection>
        )}

        {/* Implied Multiples */}
        {result.implied_multiples && (
          <>
            <MASectionHeader
              title="Implied Multiples"
              icon={<BarChart3 size={12} />}
              accentColor={ACCENT}
            />
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(140px, 1fr))', gap: '8px' }}>
              {Object.entries(result.implied_multiples).map(([key, value], idx) => (
                <MAMetricCard
                  key={key}
                  label={key.toUpperCase().replace(/_/g, ' ')}
                  value={`${(value as number).toFixed(1)}x`}
                  accentColor={CHART_PALETTE[idx % CHART_PALETTE.length]}
                  compact
                />
              ))}
            </div>
          </>
        )}
      </div>
    );
  };

  // ================================================================
  // MAIN RENDER
  // ================================================================

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* ---- Top bar: Header + Analysis type selector + Run button ---- */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${ACCENT}40`,
        flexShrink: 0,
      }}>
        {/* Title */}
        <Activity size={14} color={ACCENT} />
        <span style={{
          fontSize: '11px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          letterSpacing: '0.5px',
          fontFamily: TYPOGRAPHY.MONO,
        }}>
          ADVANCED ANALYTICS
        </span>

        {/* Separator */}
        <div style={{ width: '1px', height: '18px', backgroundColor: FINCEPT.BORDER }} />

        {/* Analysis type tabs */}
        <button
          onClick={() => { setAnalysisType('montecarlo'); setResult(null); }}
          style={{
            ...COMMON_STYLES.tabButton(analysisType === 'montecarlo'),
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            ...(analysisType === 'montecarlo' ? { backgroundColor: ACCENT, color: '#000' } : {}),
          }}
        >
          <Target size={11} /> MONTE CARLO
        </button>
        <button
          onClick={() => { setAnalysisType('regression'); setResult(null); }}
          style={{
            ...COMMON_STYLES.tabButton(analysisType === 'regression'),
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            ...(analysisType === 'regression' ? { backgroundColor: ACCENT, color: '#000' } : {}),
          }}
        >
          <TrendingUp size={11} /> REGRESSION
        </button>

        {/* Spacer */}
        <div style={{ flex: 1 }} />

        {/* Run button */}
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            backgroundColor: ACCENT,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            opacity: loading ? 0.7 : 1,
          }}
        >
          {loading ? <Loader2 size={10} className="animate-spin" /> : <PlayCircle size={10} />}
          RUN ANALYSIS
        </button>
      </div>

      {/* ---- Scrollable content ---- */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: SPACING.DEFAULT,
      }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', maxWidth: '1400px' }}>
          {/* Collapsible Inputs Section */}
          <CollapsibleSection
            title={analysisType === 'montecarlo' ? 'Monte Carlo Parameters' : 'Regression Inputs'}
            icon={analysisType === 'montecarlo' ? <Target size={11} /> : <TrendingUp size={11} />}
            defaultOpen={!result}
          >
            {analysisType === 'montecarlo' ? renderMonteCarloInputs() : renderRegressionInputs()}
          </CollapsibleSection>

          {/* Results Section */}
          {loading ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '300px',
              gap: '12px',
            }}>
              <Loader2 size={28} className="animate-spin" style={{ color: ACCENT }} />
              <span style={{
                fontSize: TYPOGRAPHY.SMALL,
                fontFamily: TYPOGRAPHY.MONO,
                color: FINCEPT.GRAY,
                letterSpacing: '1px',
              }}>
                RUNNING {analysisType === 'montecarlo' ? `${mcInputs.simulations.toLocaleString()} SIMULATIONS` : 'REGRESSION ANALYSIS'}...
              </span>
            </div>
          ) : result ? (
            analysisType === 'montecarlo' ? renderMonteCarloResults() : renderRegressionResults()
          ) : (
            <MAEmptyState
              icon={<Activity size={36} />}
              title="No Analysis Results"
              description={
                analysisType === 'montecarlo'
                  ? 'Configure parameters above and run Monte Carlo simulation to generate probability distributions and valuation ranges.'
                  : 'Add comparable companies and subject metrics, then run regression analysis to compute implied valuations.'
              }
              actionLabel="Run Analysis"
              onAction={runAnalysis}
              accentColor={ACCENT}
            />
          )}
        </div>
      </div>
    </div>
  );
};

export default AdvancedAnalytics;
