/**
 * Fairness Opinion Panel - Board-Level M&A Analysis
 *
 * Tools: Valuation Framework, Premium Analysis, Process Quality Assessment
 */

import React, { useState } from 'react';
import { Scale, PlayCircle, Loader2, TrendingUp, CheckCircle, AlertTriangle, FileText, ChevronDown, ChevronUp } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MAExportButton } from '../components';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ReferenceLine, Cell,
  RadarChart, Radar, PolarGrid, PolarAngleAxis, PolarRadiusAxis,
} from 'recharts';

type AnalysisType = 'fairness' | 'premium' | 'process';

const QUALITATIVE_FACTORS = [
  'Strategic rationale clearly articulated',
  'Arm\'s length negotiation process',
  'Board formed special committee',
  'Independent financial advisor engaged',
  'Adequate time for due diligence',
  'No conflicts of interest identified',
  'Market check or go-shop provision',
  'Fiduciary out provisions included',
  'Shareholder vote required',
  'Material adverse change clause present',
];

const PROCESS_FACTORS = [
  { key: 'board_independence', label: 'Board Independence', desc: 'Independent directors led process' },
  { key: 'special_committee', label: 'Special Committee', desc: 'Formed to evaluate transaction' },
  { key: 'independent_advisor', label: 'Independent Advisor', desc: 'Engaged qualified financial advisor' },
  { key: 'market_check', label: 'Market Check', desc: 'Conducted auction or market check' },
  { key: 'negotiation_process', label: 'Negotiation Process', desc: 'Arm\'s length negotiations' },
  { key: 'due_diligence', label: 'Due Diligence', desc: 'Comprehensive review completed' },
  { key: 'disclosure', label: 'Disclosure Quality', desc: 'Full disclosure to shareholders' },
  { key: 'timing', label: 'Timing Adequacy', desc: 'Sufficient time for evaluation' },
];

const ANALYSIS_TABS: { id: AnalysisType; label: string; icon: React.FC<{ size?: number }> }[] = [
  { id: 'fairness', label: 'FAIRNESS OPINION', icon: Scale },
  { id: 'premium', label: 'PREMIUM ANALYSIS', icon: TrendingUp },
  { id: 'process', label: 'PROCESS QUALITY', icon: FileText },
];

export const FairnessOpinion: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('fairness');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [inputsCollapsed, setInputsCollapsed] = useState(false);

  // Fairness Opinion inputs
  const [fairnessInputs, setFairnessInputs] = useState({
    offerPrice: 50,
    valuationMethods: [
      { method: 'DCF', valuation: 45, range: { min: 40, max: 55 } },
      { method: 'Trading Comps', valuation: 48, range: { min: 42, max: 54 } },
      { method: 'Precedent Transactions', valuation: 52, range: { min: 46, max: 58 } },
    ],
    selectedFactors: new Set<string>(['Strategic rationale clearly articulated', 'Board formed special committee']),
  });

  // Premium Analysis inputs
  const [premiumInputs, setPremiumInputs] = useState({
    offerPrice: 50,
    dailyPrices: '42,43,44,43,45,44,46,45,47,46,48,47,49,48,50,49,51,50,52,51', // Last 20 days
    announcementDate: 15, // Day index
  });

  // Process Quality inputs
  const [processInputs, setProcessInputs] = useState<Record<string, number>>(
    Object.fromEntries(PROCESS_FACTORS.map(f => [f.key, 3])) // 1-5 scale
  );

  const formatCurrency = (value: number) => `$${value.toFixed(2)}`;

  const handleRunFairness = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.FairnessOpinion.generateFairnessOpinion({
        valuation_methods: fairnessInputs.valuationMethods.map(v => ({
          method: v.method,
          valuation: v.valuation,
          range: v.range,
          assumptions: {},
        })),
        offer_price: fairnessInputs.offerPrice,
        qualitative_factors: Array.from(fairnessInputs.selectedFactors),
      });
      setResult(res);
      showSuccess('Fairness Opinion Generated', [
        { label: 'CONCLUSION', value: res.is_fair ? 'FAIR' : 'NOT FAIR' }
      ]);
    } catch (error) {
      showError('Fairness opinion failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunPremium = async () => {
    setLoading(true);
    try {
      const prices = premiumInputs.dailyPrices.split(',').map(p => parseFloat(p.trim()));
      const res = await MAAnalyticsService.FairnessOpinion.analyzePremiumFairness(
        prices,
        premiumInputs.offerPrice,
        premiumInputs.announcementDate
      );
      setResult(res);
      showSuccess('Premium Analysis Complete', [
        { label: '1-DAY PREMIUM', value: `${res.premium_1day?.toFixed(1)}%` }
      ]);
    } catch (error) {
      showError('Premium analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunProcess = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.FairnessOpinion.assessProcessQuality(processInputs);
      setResult(res);
      showSuccess('Process Assessment Complete', [
        { label: 'QUALITY SCORE', value: `${res.overall_score?.toFixed(1)}/5` }
      ]);
    } catch (error) {
      showError('Process assessment failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    switch (analysisType) {
      case 'fairness': return handleRunFairness();
      case 'premium': return handleRunPremium();
      case 'process': return handleRunProcess();
    }
  };

  const toggleFactor = (factor: string) => {
    const newSet = new Set(fairnessInputs.selectedFactors);
    if (newSet.has(factor)) {
      newSet.delete(factor);
    } else {
      newSet.add(factor);
    }
    setFairnessInputs({ ...fairnessInputs, selectedFactors: newSet });
  };

  // -- Build chart data helpers --

  const buildFootballFieldData = () =>
    fairnessInputs.valuationMethods.map(m => ({
      method: m.method,
      min: m.range.min,
      range: m.range.max - m.range.min,
      valuation: m.valuation,
    }));

  const buildPremiumData = () => [
    { name: '1-Day', value: result?.premium_1day ?? 0 },
    { name: '1-Week', value: result?.premium_1week ?? 0 },
    { name: '1-Month', value: result?.premium_1month ?? 0 },
    { name: '52W High', value: result?.premium_52week_high ?? 0 },
  ];

  const buildProcessRadarData = () =>
    result?.factor_scores
      ? Object.entries(result.factor_scores).map(([key, score]) => ({
          factor: key.replace(/_/g, ' '),
          score: score as number,
          fullMark: 5,
        }))
      : [];

  const buildExportData = (): Record<string, any>[] => {
    if (!result) return [];
    if (result.is_fair !== undefined) {
      return [
        { metric: 'Conclusion', value: result.is_fair ? 'FAIR' : 'NOT FAIR' },
        { metric: 'Offer Price', value: result.offer_price || fairnessInputs.offerPrice },
        { metric: 'Average Valuation', value: result.valuation_summary?.average || 'N/A' },
      ];
    }
    if (result.premium_1day !== undefined) {
      return [
        { metric: '1-Day Premium', value: `${result.premium_1day?.toFixed(1)}%` },
        { metric: '1-Week Premium', value: `${result.premium_1week?.toFixed(1)}%` },
        { metric: '1-Month Premium', value: `${result.premium_1month?.toFixed(1)}%` },
        { metric: '52-Week High Premium', value: `${result.premium_52week_high?.toFixed(1)}%` },
      ];
    }
    if (result.overall_score !== undefined) {
      const rows: Record<string, any>[] = [
        { metric: 'Overall Score', value: `${result.overall_score.toFixed(1)}/5` },
      ];
      if (result.factor_scores) {
        Object.entries(result.factor_scores).forEach(([k, v]) => {
          rows.push({ metric: k.replace(/_/g, ' '), value: `${v}/5` });
        });
      }
      return rows;
    }
    return [result];
  };

  // -- Input renderers --

  const renderFairnessInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
      {/* Offer Price */}
      <div>
        <label style={{
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: FINCEPT.MUTED,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          marginBottom: '4px',
          display: 'block',
        }}>
          Offer Price ($/share)
        </label>
        <input
          type="number"
          step="0.01"
          value={fairnessInputs.offerPrice}
          onChange={(e) => setFairnessInputs({ ...fairnessInputs, offerPrice: Number(e.target.value) })}
          style={{ ...COMMON_STYLES.inputField, width: '100%' }}
        />
      </div>

      {/* Valuation Methods */}
      <div>
        <div style={{
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: MA_COLORS.fairness,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          marginBottom: SPACING.SMALL,
        }}>
          VALUATION METHODS
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: SPACING.MEDIUM }}>
          {fairnessInputs.valuationMethods.map((method, idx) => (
            <div key={idx} style={{
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.PANEL_BG,
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.BORDER}`,
              borderLeft: `3px solid ${CHART_PALETTE[idx % CHART_PALETTE.length]}`,
            }}>
              <div style={{
                fontSize: TYPOGRAPHY.SMALL,
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                color: CHART_PALETTE[idx % CHART_PALETTE.length],
                marginBottom: SPACING.SMALL,
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                {method.method}
              </div>
              <div style={{ display: 'flex', gap: SPACING.SMALL }}>
                <div style={{ flex: 1 }}>
                  <label style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>Value</label>
                  <input
                    type="number"
                    step="0.01"
                    value={method.valuation}
                    onChange={(e) => {
                      const updated = [...fairnessInputs.valuationMethods];
                      updated[idx].valuation = Number(e.target.value);
                      setFairnessInputs({ ...fairnessInputs, valuationMethods: updated });
                    }}
                    style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                  />
                </div>
                <div style={{ flex: 1 }}>
                  <label style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>Min</label>
                  <input
                    type="number"
                    step="0.01"
                    value={method.range.min}
                    onChange={(e) => {
                      const updated = [...fairnessInputs.valuationMethods];
                      updated[idx].range.min = Number(e.target.value);
                      setFairnessInputs({ ...fairnessInputs, valuationMethods: updated });
                    }}
                    style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                  />
                </div>
                <div style={{ flex: 1 }}>
                  <label style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>Max</label>
                  <input
                    type="number"
                    step="0.01"
                    value={method.range.max}
                    onChange={(e) => {
                      const updated = [...fairnessInputs.valuationMethods];
                      updated[idx].range.max = Number(e.target.value);
                      setFairnessInputs({ ...fairnessInputs, valuationMethods: updated });
                    }}
                    style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                  />
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Qualitative Factors */}
      <div>
        <div style={{
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: MA_COLORS.fairness,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          marginBottom: SPACING.SMALL,
        }}>
          QUALITATIVE FACTORS ({fairnessInputs.selectedFactors.size}/{QUALITATIVE_FACTORS.length} selected)
        </div>
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
          gap: '3px',
        }}>
          {QUALITATIVE_FACTORS.map(factor => (
            <div
              key={factor}
              onClick={() => toggleFactor(factor)}
              style={{
                padding: '8px 10px',
                backgroundColor: fairnessInputs.selectedFactors.has(factor) ? `${MA_COLORS.fairness}15` : FINCEPT.PANEL_BG,
                borderRadius: '2px',
                border: `1px solid ${fairnessInputs.selectedFactors.has(factor) ? MA_COLORS.fairness : FINCEPT.BORDER}`,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.MEDIUM,
                transition: 'all 0.15s',
              }}
            >
              <CheckCircle
                size={12}
                color={fairnessInputs.selectedFactors.has(factor) ? MA_COLORS.fairness : FINCEPT.MUTED}
              />
              <span style={{
                fontSize: TYPOGRAPHY.TINY,
                fontFamily: TYPOGRAPHY.MONO,
                color: fairnessInputs.selectedFactors.has(factor) ? FINCEPT.WHITE : FINCEPT.GRAY,
              }}>
                {factor}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  const renderPremiumInputs = () => (
    <div style={{
      display: 'grid',
      gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
      gap: SPACING.DEFAULT,
      alignItems: 'start',
    }}>
      <div>
        <label style={{
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: FINCEPT.MUTED,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          marginBottom: '4px',
          display: 'block',
        }}>
          Offer Price ($/share)
        </label>
        <input
          type="number"
          step="0.01"
          value={premiumInputs.offerPrice}
          onChange={(e) => setPremiumInputs({ ...premiumInputs, offerPrice: Number(e.target.value) })}
          style={{ ...COMMON_STYLES.inputField, width: '100%' }}
        />
      </div>
      <div>
        <label style={{
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: FINCEPT.MUTED,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          marginBottom: '4px',
          display: 'block',
        }}>
          Daily Prices (comma-separated, most recent last)
        </label>
        <textarea
          value={premiumInputs.dailyPrices}
          onChange={(e) => setPremiumInputs({ ...premiumInputs, dailyPrices: e.target.value })}
          rows={3}
          style={{ ...COMMON_STYLES.inputField, width: '100%', resize: 'vertical' }}
        />
      </div>
      <div>
        <label style={{
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: FINCEPT.MUTED,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          marginBottom: '4px',
          display: 'block',
        }}>
          Announcement Date (day index, 0-based)
        </label>
        <input
          type="number"
          value={premiumInputs.announcementDate}
          onChange={(e) => setPremiumInputs({ ...premiumInputs, announcementDate: Number(e.target.value) })}
          style={{ ...COMMON_STYLES.inputField, width: '100%' }}
        />
        <div style={{
          marginTop: SPACING.SMALL,
          padding: '6px 10px',
          backgroundColor: `${MA_COLORS.fairness}10`,
          borderRadius: '2px',
          borderLeft: `2px solid ${MA_COLORS.fairness}`,
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          color: FINCEPT.GRAY,
        }}>
          Calculates premiums vs 1-day, 1-week, 1-month, and 52-week prices
        </div>
      </div>
    </div>
  );

  const renderProcessInputs = () => (
    <div>
      <div style={{
        fontSize: TYPOGRAPHY.TINY,
        fontFamily: TYPOGRAPHY.MONO,
        fontWeight: TYPOGRAPHY.BOLD,
        color: MA_COLORS.fairness,
        letterSpacing: TYPOGRAPHY.WIDE,
        textTransform: 'uppercase' as const,
        marginBottom: SPACING.MEDIUM,
      }}>
        PROCESS QUALITY ASSESSMENT (1-5 Scale)
      </div>
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
        gap: SPACING.SMALL,
      }}>
        {PROCESS_FACTORS.map(factor => (
          <div key={factor.key} style={{
            padding: '8px 12px',
            backgroundColor: FINCEPT.PANEL_BG,
            borderRadius: '2px',
            border: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            gap: SPACING.DEFAULT,
          }}>
            <div style={{ flex: 1, minWidth: 0 }}>
              <div style={{
                fontSize: TYPOGRAPHY.SMALL,
                fontFamily: TYPOGRAPHY.MONO,
                color: FINCEPT.WHITE,
                fontWeight: TYPOGRAPHY.BOLD,
              }}>
                {factor.label}
              </div>
              <div style={{
                fontSize: '9px',
                fontFamily: TYPOGRAPHY.MONO,
                color: FINCEPT.MUTED,
                marginTop: '2px',
              }}>
                {factor.desc}
              </div>
            </div>
            <select
              value={processInputs[factor.key]}
              onChange={(e) => setProcessInputs({ ...processInputs, [factor.key]: Number(e.target.value) })}
              style={{
                ...COMMON_STYLES.inputField,
                width: '100px',
                fontSize: TYPOGRAPHY.TINY,
                flexShrink: 0,
              }}
            >
              <option value={1}>1 - Poor</option>
              <option value={2}>2</option>
              <option value={3}>3 - Average</option>
              <option value={4}>4</option>
              <option value={5}>5 - Excellent</option>
            </select>
          </div>
        ))}
      </div>
    </div>
  );

  const renderInputs = () => {
    switch (analysisType) {
      case 'fairness': return renderFairnessInputs();
      case 'premium': return renderPremiumInputs();
      case 'process': return renderProcessInputs();
    }
  };

  // -- Result renderers --

  const renderFairnessResults = () => {
    if (result.is_fair === undefined) return null;

    const footballFieldData = buildFootballFieldData();

    return (
      <>
        {/* FAIR / NOT FAIR Banner */}
        <div style={{
          padding: SPACING.LARGE,
          backgroundColor: result.is_fair ? `${FINCEPT.GREEN}12` : `${FINCEPT.RED}12`,
          borderRadius: '2px',
          border: `2px solid ${result.is_fair ? FINCEPT.GREEN : FINCEPT.RED}`,
          textAlign: 'center',
          marginBottom: SPACING.LARGE,
        }}>
          <div style={{
            fontSize: TYPOGRAPHY.TINY,
            fontFamily: TYPOGRAPHY.MONO,
            fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.MUTED,
            letterSpacing: TYPOGRAPHY.WIDE,
            marginBottom: SPACING.SMALL,
          }}>
            FAIRNESS OPINION
          </div>
          <div style={{
            fontSize: '28px',
            fontFamily: TYPOGRAPHY.MONO,
            color: result.is_fair ? FINCEPT.GREEN : FINCEPT.RED,
            fontWeight: TYPOGRAPHY.BOLD,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: SPACING.MEDIUM,
          }}>
            {result.is_fair ? <CheckCircle size={28} /> : <AlertTriangle size={28} />}
            {result.is_fair ? 'FAIR' : 'NOT FAIR'}
          </div>
        </div>

        {/* Valuation Summary Metrics */}
        {result.valuation_summary && (
          <>
            <MASectionHeader
              title="Valuation Summary"
              icon={<Scale size={14} />}
              accentColor={MA_COLORS.fairness}
            />
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fit, minmax(160px, 1fr))',
              gap: SPACING.DEFAULT,
              marginBottom: SPACING.LARGE,
            }}>
              <MAMetricCard
                label="Offer Price"
                value={formatCurrency(result.offer_price || fairnessInputs.offerPrice)}
                accentColor="#FF6B35"
              />
              <MAMetricCard
                label="Avg Valuation"
                value={formatCurrency(result.valuation_summary.average || 0)}
                accentColor={MA_COLORS.fairness}
              />
              {result.valuation_summary.median !== undefined && (
                <MAMetricCard
                  label="Median Valuation"
                  value={formatCurrency(result.valuation_summary.median)}
                  accentColor={CHART_PALETTE[2]}
                />
              )}
              {result.valuation_summary.range_low !== undefined && (
                <MAMetricCard
                  label="Range"
                  value={`${formatCurrency(result.valuation_summary.range_low)} - ${formatCurrency(result.valuation_summary.range_high)}`}
                  accentColor={FINCEPT.CYAN}
                  compact
                />
              )}
            </div>
          </>
        )}

        {/* Football Field Chart */}
        <MAChartPanel
          title="Valuation Range (Football Field)"
          height={220}
          icon={<Scale size={14} />}
          accentColor={MA_COLORS.fairness}
        >
          <BarChart layout="vertical" data={footballFieldData}>
            <YAxis type="category" dataKey="method" width={120} tick={CHART_STYLE.axis} />
            <XAxis type="number" tick={CHART_STYLE.axis} />
            <CartesianGrid {...CHART_STYLE.grid} />
            <Bar dataKey="min" stackId="a" fill="transparent" />
            <Bar dataKey="range" stackId="a" fill={MA_COLORS.fairness} fillOpacity={0.4} />
            <ReferenceLine x={fairnessInputs.offerPrice} stroke="#FF6B35" strokeWidth={2} label={{ value: 'Offer', fill: '#FF6B35', fontSize: 9, fontFamily: 'var(--ft-font-family, monospace)' }} />
            <Tooltip
              contentStyle={CHART_STYLE.tooltip}
              formatter={(value: number, name: string) => {
                if (name === 'min') return [null, null];
                return [`$${value.toFixed(2)}`, 'Range Width'];
              }}
              labelFormatter={(label: string) => {
                const item = footballFieldData.find(d => d.method === label);
                return item ? `${label}: $${(item.min).toFixed(2)} - $${(item.min + item.range).toFixed(2)} | Point: $${item.valuation.toFixed(2)}` : label;
              }}
            />
          </BarChart>
        </MAChartPanel>
      </>
    );
  };

  const renderPremiumResults = () => {
    if (result.premium_1day === undefined) return null;

    const premiumData = buildPremiumData();

    return (
      <>
        <MASectionHeader
          title="Premium Analysis"
          icon={<TrendingUp size={14} />}
          accentColor={MA_COLORS.fairness}
        />

        {/* Premium Metric Cards */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(140px, 1fr))',
          gap: SPACING.DEFAULT,
          marginBottom: SPACING.LARGE,
        }}>
          {[
            { label: '1-Day Premium', value: result.premium_1day },
            { label: '1-Week Premium', value: result.premium_1week },
            { label: '1-Month Premium', value: result.premium_1month },
            { label: '52-Week High', value: result.premium_52week_high },
          ].map(item => (
            <MAMetricCard
              key={item.label}
              label={item.label}
              value={item.value !== undefined ? `${item.value.toFixed(1)}%` : 'N/A'}
              accentColor={MA_COLORS.fairness}
              trend={item.value !== undefined ? (item.value >= 0 ? 'up' : 'down') : undefined}
            />
          ))}
        </div>

        {/* Premium Waterfall Chart */}
        <MAChartPanel
          title="Premium Waterfall"
          height={260}
          icon={<TrendingUp size={14} />}
          accentColor={MA_COLORS.fairness}
        >
          <BarChart data={premiumData}>
            <XAxis dataKey="name" tick={CHART_STYLE.axis} />
            <YAxis tick={CHART_STYLE.axis} tickFormatter={(v: number) => `${v.toFixed(0)}%`} />
            <CartesianGrid {...CHART_STYLE.grid} />
            <Bar dataKey="value" radius={[2, 2, 0, 0]}>
              {premiumData.map((entry, i) => (
                <Cell key={i} fill={entry.value >= 0 ? FINCEPT.GREEN : FINCEPT.RED} fillOpacity={0.8} />
              ))}
            </Bar>
            <Tooltip
              contentStyle={CHART_STYLE.tooltip}
              formatter={(value: number) => [`${value.toFixed(1)}%`, 'Premium']}
            />
          </BarChart>
        </MAChartPanel>
      </>
    );
  };

  const renderProcessResults = () => {
    if (result.overall_score === undefined) return null;

    const processRadarData = buildProcessRadarData();
    const scoreLabel = result.overall_score >= 4 ? 'Strong Process' :
                       result.overall_score >= 3 ? 'Adequate Process' : 'Process Concerns';
    const scoreColor = result.overall_score >= 4 ? FINCEPT.GREEN :
                       result.overall_score >= 3 ? MA_COLORS.fairness : FINCEPT.RED;

    return (
      <>
        {/* Overall Score Banner */}
        <div style={{
          padding: SPACING.LARGE,
          backgroundColor: FINCEPT.PANEL_BG,
          borderRadius: '2px',
          border: `2px solid ${scoreColor}`,
          textAlign: 'center',
          marginBottom: SPACING.LARGE,
        }}>
          <div style={{
            fontSize: TYPOGRAPHY.TINY,
            fontFamily: TYPOGRAPHY.MONO,
            fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.MUTED,
            letterSpacing: TYPOGRAPHY.WIDE,
            marginBottom: SPACING.SMALL,
          }}>
            PROCESS QUALITY SCORE
          </div>
          <div style={{
            fontSize: '36px',
            fontFamily: TYPOGRAPHY.MONO,
            color: scoreColor,
            fontWeight: TYPOGRAPHY.BOLD,
          }}>
            {result.overall_score.toFixed(1)} / 5
          </div>
          <div style={{
            fontSize: TYPOGRAPHY.SMALL,
            fontFamily: TYPOGRAPHY.MONO,
            color: FINCEPT.GRAY,
            marginTop: SPACING.SMALL,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase' as const,
          }}>
            {scoreLabel}
          </div>
        </div>

        {/* Factor Breakdown Metrics */}
        {result.factor_scores && (
          <>
            <MASectionHeader
              title="Factor Breakdown"
              icon={<FileText size={14} />}
              accentColor={MA_COLORS.fairness}
            />

            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fit, minmax(160px, 1fr))',
              gap: SPACING.SMALL,
              marginBottom: SPACING.LARGE,
            }}>
              {Object.entries(result.factor_scores).map(([key, score]: [string, any]) => (
                <MAMetricCard
                  key={key}
                  label={key.replace(/_/g, ' ')}
                  value={`${score}/5`}
                  accentColor={score >= 4 ? FINCEPT.GREEN : score >= 3 ? MA_COLORS.fairness : FINCEPT.RED}
                  compact
                />
              ))}
            </div>

            {/* Process Quality Radar Chart */}
            {processRadarData.length > 0 && (
              <MAChartPanel
                title="Process Quality Radar"
                height={320}
                icon={<FileText size={14} />}
                accentColor={MA_COLORS.fairness}
              >
                <RadarChart data={processRadarData}>
                  <PolarGrid stroke={FINCEPT.BORDER} />
                  <PolarAngleAxis
                    dataKey="factor"
                    tick={{
                      ...CHART_STYLE.axis,
                      fontSize: 8,
                    }}
                  />
                  <PolarRadiusAxis
                    domain={[0, 5]}
                    tick={CHART_STYLE.axis}
                  />
                  <Radar
                    dataKey="score"
                    stroke={MA_COLORS.fairness}
                    fill={MA_COLORS.fairness}
                    fillOpacity={0.3}
                    strokeWidth={2}
                  />
                </RadarChart>
              </MAChartPanel>
            )}
          </>
        )}
      </>
    );
  };

  const renderResults = () => {
    if (!result) {
      return (
        <MAEmptyState
          icon={<Scale size={36} />}
          title="Fairness Opinion"
          description="Configure valuation methods and run analysis"
          accentColor={MA_COLORS.fairness}
        />
      );
    }

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {renderFairnessResults()}
        {renderPremiumResults()}
        {renderProcessResults()}
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* Top Tab Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        flexShrink: 0,
      }}>
        {ANALYSIS_TABS.map(tab => {
          const isActive = analysisType === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => { setAnalysisType(tab.id); setResult(null); }}
              style={{
                padding: '10px 20px',
                backgroundColor: 'transparent',
                border: 'none',
                borderBottom: isActive ? `2px solid ${MA_COLORS.fairness}` : '2px solid transparent',
                color: isActive ? MA_COLORS.fairness : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: TYPOGRAPHY.WIDE,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                transition: 'all 0.15s',
                textTransform: 'uppercase' as const,
              }}
            >
              <tab.icon size={12} />
              {tab.label}
            </button>
          );
        })}

        <div style={{ flex: 1 }} />

        {/* Run Button in header */}
        <button
          onClick={handleRun}
          disabled={loading}
          style={{
            margin: '0 12px',
            padding: '6px 18px',
            backgroundColor: loading ? FINCEPT.PANEL_BG : MA_COLORS.fairness,
            border: 'none',
            borderRadius: '2px',
            color: loading ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            cursor: loading ? 'not-allowed' : 'pointer',
            fontSize: TYPOGRAPHY.TINY,
            fontWeight: TYPOGRAPHY.BOLD,
            fontFamily: TYPOGRAPHY.MONO,
            letterSpacing: TYPOGRAPHY.WIDE,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            transition: 'all 0.15s',
          }}
        >
          {loading ? <Loader2 size={12} className="animate-spin" /> : <PlayCircle size={12} />}
          {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
        </button>

        {result && (
          <MAExportButton
            data={buildExportData()}
            filename={`fairness_${analysisType}`}
            accentColor={MA_COLORS.fairness}
          />
        )}
      </div>

      {/* Scrollable Content Area */}
      <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
        {/* Collapsible Input Section */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: SPACING.LARGE,
          overflow: 'hidden',
        }}>
          {/* Input Section Header (clickable to collapse) */}
          <div
            onClick={() => setInputsCollapsed(!inputsCollapsed)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.MEDIUM,
              padding: '10px 14px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: inputsCollapsed ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer',
              userSelect: 'none' as const,
            }}
          >
            <Scale size={14} color={MA_COLORS.fairness} />
            <span style={{
              fontSize: TYPOGRAPHY.SMALL,
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              color: MA_COLORS.fairness,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase' as const,
            }}>
              {analysisType === 'fairness' ? 'Valuation Configuration' :
               analysisType === 'premium' ? 'Premium Configuration' :
               'Process Assessment Configuration'}
            </span>
            <div style={{ flex: 1 }} />
            <span style={{
              fontSize: '9px',
              fontFamily: TYPOGRAPHY.MONO,
              color: FINCEPT.MUTED,
            }}>
              {inputsCollapsed ? 'EXPAND' : 'COLLAPSE'}
            </span>
            {inputsCollapsed
              ? <ChevronDown size={14} color={FINCEPT.GRAY} />
              : <ChevronUp size={14} color={FINCEPT.GRAY} />
            }
          </div>

          {/* Input Content */}
          {!inputsCollapsed && (
            <div style={{ padding: SPACING.DEFAULT }}>
              {renderInputs()}
            </div>
          )}
        </div>

        {/* Results Section */}
        {renderResults()}
      </div>
    </div>
  );
};
