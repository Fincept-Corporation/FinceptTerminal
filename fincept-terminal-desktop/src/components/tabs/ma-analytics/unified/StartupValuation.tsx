/**
 * Startup Valuation Panel - 5 Pre-Revenue Valuation Methods
 *
 * Methods: Berkus, Scorecard, VC Method, First Chicago, Risk Factor Summation
 */

import React, { useState } from 'react';
import { Rocket, PlayCircle, Loader2, TrendingUp, AlertTriangle, Target, Users, Lightbulb, Shield, ChevronDown, ChevronUp, BarChart3, PieChart as PieChartIcon, Radar as RadarIcon, DollarSign } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MAExportButton } from '../components';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Cell,
  PieChart, Pie, Legend,
  RadarChart, Radar, PolarGrid, PolarAngleAxis, PolarRadiusAxis,
  ResponsiveContainer,
} from 'recharts';

type ValuationMethod = 'berkus' | 'scorecard' | 'vc' | 'first_chicago' | 'risk_factor' | 'comprehensive';

const METHODS = [
  { id: 'berkus' as const, label: 'BERKUS', desc: 'Pre-revenue milestone-based' },
  { id: 'scorecard' as const, label: 'SCORECARD', desc: 'Comparative assessment' },
  { id: 'vc' as const, label: 'VC METHOD', desc: 'Expected return approach' },
  { id: 'first_chicago' as const, label: 'FIRST CHICAGO', desc: 'Scenario-weighted' },
  { id: 'risk_factor' as const, label: 'RISK FACTOR', desc: 'Risk adjustment method' },
  { id: 'comprehensive' as const, label: 'ALL METHODS', desc: 'Run all 5 methods' },
];

// Berkus factor descriptions
const BERKUS_FACTORS = [
  { key: 'sound_idea', label: 'Sound Idea', desc: 'Basic value of the concept', max: 500000 },
  { key: 'prototype', label: 'Prototype', desc: 'Reduces technology risk', max: 500000 },
  { key: 'quality_team', label: 'Quality Team', desc: 'Reduces execution risk', max: 500000 },
  { key: 'strategic_relationships', label: 'Strategic Relationships', desc: 'Reduces market risk', max: 500000 },
  { key: 'product_rollout', label: 'Product Rollout/Sales', desc: 'Reduces production risk', max: 500000 },
];

// Scorecard factors
const SCORECARD_FACTORS = [
  { key: 'team', label: 'Management Team', weight: 0.30 },
  { key: 'market_size', label: 'Market Size', weight: 0.25 },
  { key: 'product', label: 'Product/Technology', weight: 0.15 },
  { key: 'competitive', label: 'Competitive Environment', weight: 0.10 },
  { key: 'marketing', label: 'Marketing/Sales', weight: 0.10 },
  { key: 'need_for_funding', label: 'Need for Additional Funding', weight: 0.05 },
  { key: 'other', label: 'Other Factors', weight: 0.05 },
];

// Risk factors for Risk Factor Summation
const RISK_FACTORS = [
  { key: 'management', label: 'Management Risk' },
  { key: 'stage', label: 'Stage of Business' },
  { key: 'legislation', label: 'Legislation/Political Risk' },
  { key: 'manufacturing', label: 'Manufacturing Risk' },
  { key: 'sales_marketing', label: 'Sales & Marketing Risk' },
  { key: 'funding', label: 'Funding/Capital Raising Risk' },
  { key: 'competition', label: 'Competition Risk' },
  { key: 'technology', label: 'Technology Risk' },
  { key: 'litigation', label: 'Litigation Risk' },
  { key: 'international', label: 'International Risk' },
  { key: 'reputation', label: 'Reputation Risk' },
  { key: 'exit', label: 'Potential Lucrative Exit' },
];

const ACCENT = MA_COLORS.startup;

export const StartupValuation: React.FC = () => {
  const [method, setMethod] = useState<ValuationMethod>('berkus');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [inputsExpanded, setInputsExpanded] = useState(true);

  // Berkus inputs (0-100% of max $500K per factor)
  const [berkusScores, setBerkusScores] = useState<Record<string, number>>({
    sound_idea: 70,
    prototype: 50,
    quality_team: 80,
    strategic_relationships: 40,
    product_rollout: 30,
  });

  // Scorecard inputs
  const [scorecardInputs, setScorecardInputs] = useState({
    stage: 'seed',
    region: 'us',
    assessments: {
      team: 1.2,
      market_size: 1.1,
      product: 1.0,
      competitive: 0.9,
      marketing: 1.0,
      need_for_funding: 0.8,
      other: 1.0,
    } as Record<string, number>,
  });

  // VC Method inputs
  const [vcInputs, setVcInputs] = useState({
    exitYearMetric: 50, // $50M revenue at exit
    exitMultiple: 8,    // 8x revenue multiple
    yearsToExit: 5,
    investmentAmount: 5, // $5M investment
    stage: 'seed',
  });

  // First Chicago inputs (3 scenarios)
  const [chicagoScenarios, setChicagoScenarios] = useState([
    { name: 'Bull Case', probability: 25, exit_year: 5, exit_value: 100, description: 'Exceeds targets' },
    { name: 'Base Case', probability: 50, exit_year: 5, exit_value: 40, description: 'Meets targets' },
    { name: 'Bear Case', probability: 25, exit_year: 5, exit_value: 10, description: 'Below targets' },
  ]);

  // Risk Factor inputs (-2 to +2 scale)
  const [riskInputs, setRiskInputs] = useState({
    baseValuation: 3, // $3M base
    assessments: Object.fromEntries(RISK_FACTORS.map(f => [f.key, 0])) as Record<string, number>,
  });

  // Startup name for comprehensive
  const [startupName, setStartupName] = useState('My Startup');

  const formatCurrency = (value: number) => {
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    if (value >= 1e3) return `$${(value / 1e3).toFixed(0)}K`;
    return `$${value.toLocaleString()}`;
  };

  const handleRunBerkus = async () => {
    setLoading(true);
    try {
      const scores = Object.fromEntries(
        Object.entries(berkusScores).map(([k, v]) => [k, v / 100])
      );
      const res = await MAAnalyticsService.StartupValuation.calculateBerkusValuation(scores);
      setResult(res);
      showSuccess('Berkus Valuation Complete', [
        { label: 'VALUATION', value: formatCurrency(res.valuation || 0) }
      ]);
    } catch (error) {
      showError('Berkus calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunScorecard = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.StartupValuation.calculateScorecardValuation(
        scorecardInputs.stage,
        scorecardInputs.region,
        scorecardInputs.assessments
      );
      setResult(res);
      showSuccess('Scorecard Valuation Complete', [
        { label: 'VALUATION', value: formatCurrency(res.valuation || 0) }
      ]);
    } catch (error) {
      showError('Scorecard calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunVC = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.StartupValuation.calculateVCMethodValuation(
        vcInputs.exitYearMetric * 1e6,
        vcInputs.exitMultiple,
        vcInputs.yearsToExit,
        vcInputs.investmentAmount * 1e6,
        vcInputs.stage
      );
      setResult(res);
      showSuccess('VC Method Complete', [
        { label: 'PRE-MONEY', value: formatCurrency(res.pre_money_valuation || 0) },
        { label: 'POST-MONEY', value: formatCurrency(res.post_money_valuation || 0) }
      ]);
    } catch (error) {
      showError('VC Method calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunFirstChicago = async () => {
    setLoading(true);
    try {
      const scenarios = chicagoScenarios.map(s => ({
        ...s,
        probability: s.probability / 100,
        exit_value: s.exit_value * 1e6,
      }));
      const res = await MAAnalyticsService.StartupValuation.calculateFirstChicagoValuation(scenarios);
      setResult(res);
      showSuccess('First Chicago Complete', [
        { label: 'WEIGHTED VALUE', value: formatCurrency(res.weighted_valuation || 0) }
      ]);
    } catch (error) {
      showError('First Chicago calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunRiskFactor = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.StartupValuation.calculateRiskFactorValuation(
        riskInputs.baseValuation * 1e6,
        riskInputs.assessments
      );
      setResult(res);
      showSuccess('Risk Factor Complete', [
        { label: 'ADJUSTED VALUE', value: formatCurrency(res.adjusted_valuation || 0) }
      ]);
    } catch (error) {
      showError('Risk Factor calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunComprehensive = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.StartupValuation.comprehensiveStartupValuation({
        startup_name: startupName,
        berkus_scores: Object.fromEntries(
          Object.entries(berkusScores).map(([k, v]) => [k, v / 100])
        ),
        scorecard_inputs: scorecardInputs,
        vc_inputs: {
          exit_year_metric: vcInputs.exitYearMetric * 1e6,
          exit_multiple: vcInputs.exitMultiple,
          years_to_exit: vcInputs.yearsToExit,
          investment_amount: vcInputs.investmentAmount * 1e6,
          stage: vcInputs.stage,
        },
        first_chicago_scenarios: chicagoScenarios.map(s => ({
          ...s,
          probability: s.probability / 100,
          exit_value: s.exit_value * 1e6,
        })),
        risk_factor_assessments: riskInputs.assessments,
      });
      setResult(res);
      showSuccess('Comprehensive Valuation Complete');
    } catch (error) {
      showError('Comprehensive valuation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    switch (method) {
      case 'berkus': return handleRunBerkus();
      case 'scorecard': return handleRunScorecard();
      case 'vc': return handleRunVC();
      case 'first_chicago': return handleRunFirstChicago();
      case 'risk_factor': return handleRunRiskFactor();
      case 'comprehensive': return handleRunComprehensive();
    }
  };

  // ── Build export data from result ──
  const buildExportData = (): Record<string, any>[] => {
    if (!result) return [];
    if (result.methods) {
      return Object.entries(result.methods).map(([name, res]: [string, any]) => ({
        method: name.replace(/_/g, ' '),
        valuation: res.valuation || res.weighted_valuation || res.adjusted_valuation || 0,
      }));
    }
    return [{ method, ...result }];
  };

  // ── Render method-specific inputs ──
  const renderInputs = () => {
    switch (method) {
      case 'berkus':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.TINY }}>
              BERKUS FACTORS (0-100%)
            </div>
            {BERKUS_FACTORS.map(factor => (
              <div key={factor.key}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>{factor.label}</span>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: ACCENT, fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD }}>
                    {berkusScores[factor.key]}% (${((berkusScores[factor.key] / 100) * factor.max / 1000).toFixed(0)}K)
                  </span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={berkusScores[factor.key]}
                  onChange={(e) => setBerkusScores({ ...berkusScores, [factor.key]: Number(e.target.value) })}
                  style={{ width: '100%', accentColor: ACCENT }}
                />
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontFamily: TYPOGRAPHY.MONO }}>{factor.desc}</div>
              </div>
            ))}
          </div>
        );

      case 'scorecard':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM }}>
              <div>
                <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Stage</label>
                <select
                  value={scorecardInputs.stage}
                  onChange={(e) => setScorecardInputs({ ...scorecardInputs, stage: e.target.value })}
                  style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                >
                  <option value="pre_seed">Pre-Seed</option>
                  <option value="seed">Seed</option>
                  <option value="series_a">Series A</option>
                </select>
              </div>
              <div>
                <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Region</label>
                <select
                  value={scorecardInputs.region}
                  onChange={(e) => setScorecardInputs({ ...scorecardInputs, region: e.target.value })}
                  style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                >
                  <option value="us">United States</option>
                  <option value="europe">Europe</option>
                  <option value="asia">Asia</option>
                  <option value="other">Other</option>
                </select>
              </div>
            </div>
            <div style={{ ...COMMON_STYLES.dataLabel, marginTop: SPACING.SMALL }}>
              FACTOR SCORES (0.5 = Below Avg, 1.0 = Avg, 1.5 = Above)
            </div>
            {SCORECARD_FACTORS.map(factor => (
              <div key={factor.key} style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                <span style={{ flex: 1, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
                  {factor.label} ({(factor.weight * 100).toFixed(0)}%)
                </span>
                <input
                  type="number"
                  step="0.1"
                  min="0.5"
                  max="1.5"
                  value={scorecardInputs.assessments[factor.key]}
                  onChange={(e) => setScorecardInputs({
                    ...scorecardInputs,
                    assessments: { ...scorecardInputs.assessments, [factor.key]: Number(e.target.value) }
                  })}
                  style={{ ...COMMON_STYLES.inputField, width: '70px', fontSize: TYPOGRAPHY.TINY }}
                />
              </div>
            ))}
          </div>
        );

      case 'vc':
        return (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Exit Year Revenue ($M)</label>
              <input
                type="number"
                value={vcInputs.exitYearMetric}
                onChange={(e) => setVcInputs({ ...vcInputs, exitYearMetric: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Exit Multiple (x Revenue)</label>
              <input
                type="number"
                step="0.5"
                value={vcInputs.exitMultiple}
                onChange={(e) => setVcInputs({ ...vcInputs, exitMultiple: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Years to Exit</label>
              <input
                type="number"
                value={vcInputs.yearsToExit}
                onChange={(e) => setVcInputs({ ...vcInputs, yearsToExit: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Investment Amount ($M)</label>
              <input
                type="number"
                value={vcInputs.investmentAmount}
                onChange={(e) => setVcInputs({ ...vcInputs, investmentAmount: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div style={{ gridColumn: '1 / -1' }}>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Stage</label>
              <select
                value={vcInputs.stage}
                onChange={(e) => setVcInputs({ ...vcInputs, stage: e.target.value })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              >
                <option value="seed">Seed (50-70% IRR)</option>
                <option value="series_a">Series A (40-60% IRR)</option>
                <option value="series_b">Series B (30-50% IRR)</option>
                <option value="growth">Growth (20-35% IRR)</option>
              </select>
            </div>
          </div>
        );

      case 'first_chicago':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
            <div style={{ ...COMMON_STYLES.dataLabel }}>SCENARIO ANALYSIS</div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: SPACING.MEDIUM }}>
              {chicagoScenarios.map((scenario, idx) => {
                const scenarioColors = ['#00D66F', '#FFC400', '#FF3B3B'];
                return (
                  <div key={idx} style={{
                    padding: SPACING.DEFAULT,
                    backgroundColor: FINCEPT.PANEL_BG,
                    borderRadius: '2px',
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderTop: `2px solid ${scenarioColors[idx]}`,
                  }}>
                    <div style={{
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: scenarioColors[idx],
                      marginBottom: SPACING.MEDIUM,
                      fontSize: TYPOGRAPHY.TINY,
                      fontFamily: TYPOGRAPHY.MONO,
                      letterSpacing: TYPOGRAPHY.WIDE,
                      textTransform: 'uppercase' as const,
                    }}>
                      {scenario.name}
                    </div>
                    <div style={{ marginBottom: SPACING.SMALL }}>
                      <label style={{ fontSize: '9px', color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '2px' }}>Probability %</label>
                      <input
                        type="number"
                        value={scenario.probability}
                        onChange={(e) => {
                          const updated = [...chicagoScenarios];
                          updated[idx].probability = Number(e.target.value);
                          setChicagoScenarios(updated);
                        }}
                        style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                      />
                    </div>
                    <div>
                      <label style={{ fontSize: '9px', color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '2px' }}>Exit Value ($M)</label>
                      <input
                        type="number"
                        value={scenario.exit_value}
                        onChange={(e) => {
                          const updated = [...chicagoScenarios];
                          updated[idx].exit_value = Number(e.target.value);
                          setChicagoScenarios(updated);
                        }}
                        style={{ ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.TINY }}
                      />
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        );

      case 'risk_factor':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: SPACING.MEDIUM, alignItems: 'end' }}>
              <div>
                <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Base Valuation ($M)</label>
                <input
                  type="number"
                  value={riskInputs.baseValuation}
                  onChange={(e) => setRiskInputs({ ...riskInputs, baseValuation: Number(e.target.value) })}
                  style={{ ...COMMON_STYLES.inputField, width: '180px' }}
                />
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel }}>
                RISK ADJUSTMENTS (-2 to +2)
              </div>
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: `${SPACING.SMALL} ${SPACING.LARGE}` }}>
              {RISK_FACTORS.map(factor => (
                <div key={factor.key} style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                  <span style={{ flex: 1, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>{factor.label}</span>
                  <select
                    value={riskInputs.assessments[factor.key]}
                    onChange={(e) => setRiskInputs({
                      ...riskInputs,
                      assessments: { ...riskInputs.assessments, [factor.key]: Number(e.target.value) }
                    })}
                    style={{ ...COMMON_STYLES.inputField, width: '90px', fontSize: TYPOGRAPHY.TINY }}
                  >
                    <option value={-2}>-2 (High Risk)</option>
                    <option value={-1}>-1</option>
                    <option value={0}>0 (Neutral)</option>
                    <option value={1}>+1</option>
                    <option value={2}>+2 (Low Risk)</option>
                  </select>
                </div>
              ))}
            </div>
          </div>
        );

      case 'comprehensive':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO, display: 'block', marginBottom: '3px' }}>Startup Name</label>
              <input
                type="text"
                value={startupName}
                onChange={(e) => setStartupName(e.target.value)}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: `${ACCENT}10`,
              borderRadius: '2px',
              border: `1px solid ${ACCENT}30`,
              borderLeft: `3px solid ${ACCENT}`,
            }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: ACCENT, fontWeight: TYPOGRAPHY.BOLD, fontFamily: TYPOGRAPHY.MONO, letterSpacing: TYPOGRAPHY.WIDE }}>
                COMPREHENSIVE ANALYSIS
              </div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px', fontFamily: TYPOGRAPHY.MONO, lineHeight: 1.5 }}>
                Runs all 5 valuation methods using inputs from each tab. Configure each method first, then run comprehensive for a complete valuation range.
              </div>
            </div>
          </div>
        );
    }
  };

  // ── Berkus Factor Breakdown chart data ──
  const buildBerkusChartData = () =>
    BERKUS_FACTORS.map(f => ({
      factor: f.label,
      value: (berkusScores[f.key] / 100) * f.max / 1000,
      pct: berkusScores[f.key],
    }));

  // ── Risk Factor Radar chart data ──
  const buildRiskRadarData = () =>
    RISK_FACTORS.map(f => ({
      factor: f.label.replace(' Risk', ''),
      value: riskInputs.assessments[f.key] + 2,
      fullMark: 4,
    }));

  // ── First Chicago scenario pie data ──
  const buildChicagoPieData = () =>
    chicagoScenarios.map(s => ({
      name: s.name,
      value: s.probability,
      exitValue: s.exit_value,
    }));

  // ── Methods comparison data ──
  const buildMethodsComparisonData = () => {
    if (!result?.methods) return [];
    return Object.entries(result.methods).map(([name, res]: [string, any]) => ({
      name: name.replace(/_/g, ' '),
      valuation: (res.valuation || res.weighted_valuation || res.adjusted_valuation || 0) / 1e6,
    }));
  };

  const PIE_COLORS = ['#00D66F', '#FFC400', '#FF3B3B'];

  // ── Render results section ──
  const renderResults = () => {
    if (!result) return null;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.LARGE }}>

        {/* ── Main valuation metric cards ── */}
        <MASectionHeader
          title="Valuation Results"
          icon={<DollarSign size={14} />}
          accentColor={ACCENT}
          action={<MAExportButton data={buildExportData()} filename="startup_valuation" accentColor={ACCENT} />}
        />

        {/* Berkus / Scorecard / Comprehensive single valuation */}
        {result.valuation && !result.pre_money_valuation && !result.methods && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: SPACING.DEFAULT }}>
            <MAMetricCard
              label="Estimated Valuation"
              value={formatCurrency(result.valuation)}
              accentColor={ACCENT}
            />
          </div>
        )}

        {/* VC Method: pre-money / post-money */}
        {result.pre_money_valuation && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
            <MAMetricCard
              label="Pre-Money Valuation"
              value={formatCurrency(result.pre_money_valuation)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Post-Money Valuation"
              value={formatCurrency(result.post_money_valuation)}
              accentColor={FINCEPT.CYAN}
            />
            {result.ownership_percentage != null && (
              <MAMetricCard
                label="Investor Ownership"
                value={`${(result.ownership_percentage * 100).toFixed(1)}%`}
                accentColor={ACCENT}
              />
            )}
          </div>
        )}

        {/* First Chicago: weighted valuation */}
        {result.weighted_valuation && !result.methods && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
            <MAMetricCard
              label="Probability-Weighted Value"
              value={formatCurrency(result.weighted_valuation)}
              accentColor={ACCENT}
            />
            {result.scenario_values && result.scenario_values.map((sv: any, i: number) => (
              <MAMetricCard
                key={i}
                label={chicagoScenarios[i]?.name || `Scenario ${i + 1}`}
                value={formatCurrency(sv || 0)}
                accentColor={PIE_COLORS[i]}
                compact
              />
            ))}
          </div>
        )}

        {/* Risk Factor: base vs adjusted */}
        {result.adjusted_valuation && !result.methods && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
            <MAMetricCard
              label="Base Valuation"
              value={formatCurrency(result.base_valuation || riskInputs.baseValuation * 1e6)}
              accentColor={FINCEPT.GRAY}
            />
            <MAMetricCard
              label="Risk-Adjusted Value"
              value={formatCurrency(result.adjusted_valuation)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Total Adjustment"
              value={formatCurrency((result.adjusted_valuation || 0) - (result.base_valuation || riskInputs.baseValuation * 1e6))}
              accentColor={
                ((result.adjusted_valuation || 0) - (result.base_valuation || riskInputs.baseValuation * 1e6)) >= 0
                  ? FINCEPT.GREEN
                  : FINCEPT.RED
              }
              trend={
                ((result.adjusted_valuation || 0) - (result.base_valuation || riskInputs.baseValuation * 1e6)) >= 0
                  ? 'up'
                  : 'down'
              }
            />
          </div>
        )}

        {/* Comprehensive: method-level cards */}
        {result.methods && (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: SPACING.DEFAULT }}>
            {Object.entries(result.methods).map(([methodName, methodResult]: [string, any], idx: number) => (
              <MAMetricCard
                key={methodName}
                label={methodName.replace(/_/g, ' ')}
                value={formatCurrency(methodResult.valuation || methodResult.weighted_valuation || methodResult.adjusted_valuation || 0)}
                accentColor={CHART_PALETTE[idx % CHART_PALETTE.length]}
                compact
              />
            ))}
          </div>
        )}

        {/* ── Charts ── */}

        {/* Chart 1: Methods Comparison Bar (comprehensive) */}
        {result.methods && (
          <MAChartPanel
            title="Methods Comparison"
            icon={<BarChart3 size={12} />}
            accentColor={ACCENT}
            height={260}
          >
            <BarChart data={buildMethodsComparisonData()}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis dataKey="name" tick={CHART_STYLE.axis} interval={0} angle={-15} textAnchor="end" height={50} />
              <YAxis tick={CHART_STYLE.axis} label={{ value: '$M', ...CHART_STYLE.label, angle: -90, position: 'insideLeft' }} />
              <Tooltip
                contentStyle={CHART_STYLE.tooltip}
                formatter={(value: number) => [`$${value.toFixed(2)}M`, 'Valuation']}
              />
              <Bar dataKey="valuation" fill={ACCENT} radius={[2, 2, 0, 0]}>
                {buildMethodsComparisonData().map((_, i) => (
                  <Cell key={i} fill={CHART_PALETTE[i % CHART_PALETTE.length]} />
                ))}
              </Bar>
            </BarChart>
          </MAChartPanel>
        )}

        {/* Chart 2: First Chicago Scenario Pie */}
        {result.weighted_valuation && !result.methods && method === 'first_chicago' && (
          <MAChartPanel
            title="Scenario Probability Distribution"
            icon={<PieChartIcon size={12} />}
            accentColor={ACCENT}
            height={280}
          >
            <PieChart>
              <Pie
                data={buildChicagoPieData()}
                dataKey="value"
                nameKey="name"
                cx="50%"
                cy="50%"
                outerRadius={80}
                innerRadius={35}
                label={({ name, value }: any) => `${name}: ${value}%`}
                labelLine={{ stroke: FINCEPT.MUTED }}
              >
                {buildChicagoPieData().map((_, i) => (
                  <Cell key={i} fill={PIE_COLORS[i]} />
                ))}
              </Pie>
              <Legend
                wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
              />
              <Tooltip
                contentStyle={CHART_STYLE.tooltip}
                formatter={(value: number, _name: string, props: any) => [
                  `${value}% - Exit: $${props.payload.exitValue}M`,
                  props.payload.name,
                ]}
              />
            </PieChart>
          </MAChartPanel>
        )}

        {/* Chart 3: Berkus Factor Breakdown */}
        {method === 'berkus' && result.valuation && (
          <MAChartPanel
            title="Berkus Factor Breakdown"
            icon={<BarChart3 size={12} />}
            accentColor={ACCENT}
            height={240}
          >
            <BarChart layout="vertical" data={buildBerkusChartData()}>
              <YAxis type="category" dataKey="factor" width={130} tick={CHART_STYLE.axis} />
              <XAxis type="number" tick={CHART_STYLE.axis} unit="K" />
              <CartesianGrid {...CHART_STYLE.grid} />
              <Bar dataKey="value" fill={ACCENT} radius={[0, 2, 2, 0]}>
                {buildBerkusChartData().map((_, i) => (
                  <Cell key={i} fill={CHART_PALETTE[i % CHART_PALETTE.length]} />
                ))}
              </Bar>
              <Tooltip
                contentStyle={CHART_STYLE.tooltip}
                formatter={(value: number, _name: string, props: any) => [
                  `$${value.toFixed(0)}K (${props.payload.pct}%)`,
                  'Value',
                ]}
              />
            </BarChart>
          </MAChartPanel>
        )}

        {/* Chart 4: Risk Factor Radar */}
        {method === 'risk_factor' && result.adjusted_valuation && (
          <MAChartPanel
            title="Risk Factor Profile"
            icon={<RadarIcon size={12} />}
            accentColor={ACCENT}
            height={320}
          >
            <RadarChart data={buildRiskRadarData()} cx="50%" cy="50%" outerRadius="70%">
              <PolarGrid stroke={FINCEPT.BORDER} />
              <PolarAngleAxis dataKey="factor" tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: 'var(--ft-font-family, monospace)' }} />
              <PolarRadiusAxis domain={[0, 4]} tick={{ ...CHART_STYLE.axis, fontSize: 7 }} tickCount={5} />
              <Radar dataKey="value" stroke={ACCENT} fill={ACCENT} fillOpacity={0.25} strokeWidth={1.5} />
              <Tooltip
                contentStyle={CHART_STYLE.tooltip}
                formatter={(value: number) => [`${(value - 2).toFixed(0)} (raw: ${value.toFixed(0)}/4)`, 'Risk Score']}
              />
            </RadarChart>
          </MAChartPanel>
        )}

        {/* Raw JSON details */}
        <details style={{ marginTop: SPACING.SMALL }}>
          <summary style={{
            fontSize: TYPOGRAPHY.TINY,
            color: FINCEPT.MUTED,
            cursor: 'pointer',
            fontFamily: TYPOGRAPHY.MONO,
            padding: SPACING.SMALL,
          }}>
            Raw Output
          </summary>
          <pre style={{
            fontSize: '9px',
            color: FINCEPT.GRAY,
            backgroundColor: FINCEPT.PANEL_BG,
            padding: SPACING.DEFAULT,
            borderRadius: '2px',
            border: `1px solid ${FINCEPT.BORDER}`,
            overflow: 'auto',
            maxHeight: '200px',
            fontFamily: TYPOGRAPHY.MONO,
          }}>
            {JSON.stringify(result, null, 2)}
          </pre>
        </details>
      </div>
    );
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      overflow: 'hidden',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: TYPOGRAPHY.MONO,
    }}>

      {/* ── Horizontal Method Tab Bar ── */}
      <div style={{
        display: 'flex',
        alignItems: 'stretch',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
        flexShrink: 0,
      }}>
        {METHODS.map(m => {
          const isActive = method === m.id;
          return (
            <button
              key={m.id}
              onClick={() => { setMethod(m.id); setResult(null); }}
              style={{
                flex: 1,
                padding: '8px 6px 6px',
                backgroundColor: 'transparent',
                border: 'none',
                borderBottom: isActive ? `2px solid ${ACCENT}` : '2px solid transparent',
                color: isActive ? ACCENT : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase' as const,
                transition: 'all 0.15s',
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '2px',
              }}
              onMouseEnter={(e) => {
                if (!isActive) e.currentTarget.style.color = FINCEPT.WHITE;
              }}
              onMouseLeave={(e) => {
                if (!isActive) e.currentTarget.style.color = FINCEPT.GRAY;
              }}
            >
              <span>{m.label}</span>
              <span style={{ fontSize: '7px', color: isActive ? `${ACCENT}99` : FINCEPT.MUTED, fontWeight: 400 }}>
                {m.desc}
              </span>
            </button>
          );
        })}
      </div>

      {/* ── Scrollable Content ── */}
      <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>

        {/* ── Collapsible Inputs Section ── */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: SPACING.LARGE,
        }}>
          {/* Input section header with collapse toggle */}
          <div
            onClick={() => setInputsExpanded(!inputsExpanded)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.MEDIUM,
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              cursor: 'pointer',
              userSelect: 'none' as const,
              borderBottom: inputsExpanded ? `1px solid ${FINCEPT.BORDER}` : 'none',
            }}
          >
            <Rocket size={12} style={{ color: ACCENT }} />
            <span style={{
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
              color: ACCENT,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase' as const,
              fontFamily: TYPOGRAPHY.MONO,
            }}>
              {METHODS.find(m => m.id === method)?.label} Configuration
            </span>
            <div style={{ flex: 1 }} />
            {inputsExpanded
              ? <ChevronUp size={12} color={FINCEPT.GRAY} />
              : <ChevronDown size={12} color={FINCEPT.GRAY} />
            }
          </div>

          {/* Input fields */}
          {inputsExpanded && (
            <div style={{ padding: SPACING.DEFAULT }}>
              {renderInputs()}
            </div>
          )}

          {/* Run Button - always visible at bottom of config panel */}
          <div style={{
            padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
            borderTop: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <button
              onClick={handleRun}
              disabled={loading}
              style={{
                width: '100%',
                padding: '8px 16px',
                backgroundColor: loading ? FINCEPT.PANEL_BG : ACCENT,
                border: loading ? `1px solid ${FINCEPT.BORDER}` : 'none',
                borderRadius: '2px',
                color: loading ? FINCEPT.GRAY : '#000000',
                cursor: loading ? 'not-allowed' : 'pointer',
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase' as const,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: SPACING.MEDIUM,
                transition: 'all 0.15s',
              }}
            >
              {loading ? <Loader2 size={14} className="animate-spin" /> : <PlayCircle size={14} />}
              {loading ? 'CALCULATING...' : `RUN ${METHODS.find(m => m.id === method)?.label}`}
            </button>
          </div>
        </div>

        {/* ── Results Section ── */}
        {!result ? (
          <MAEmptyState
            icon={<Rocket size={36} />}
            title="Startup Valuation"
            description="Select a method and configure inputs"
            accentColor={ACCENT}
          />
        ) : (
          renderResults()
        )}
      </div>
    </div>
  );
};
