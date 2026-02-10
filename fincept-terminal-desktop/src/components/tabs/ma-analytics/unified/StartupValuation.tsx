/**
 * Startup Valuation Panel - 5 Pre-Revenue Valuation Methods
 *
 * Methods: Berkus, Scorecard, VC Method, First Chicago, Risk Factor Summation
 */

import React, { useState } from 'react';
import { Rocket, PlayCircle, Loader2, TrendingUp, AlertTriangle, Target, Users, Lightbulb, Shield } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

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

export const StartupValuation: React.FC = () => {
  const [method, setMethod] = useState<ValuationMethod>('berkus');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);

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

  const renderInputs = () => {
    switch (method) {
      case 'berkus':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.TINY }}>
              BERKUS FACTORS (0-100%)
            </div>
            {BERKUS_FACTORS.map(factor => (
              <div key={factor.key}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{factor.label}</span>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.ORANGE }}>
                    {berkusScores[factor.key]}% (${((berkusScores[factor.key] / 100) * factor.max / 1000).toFixed(0)}K)
                  </span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={berkusScores[factor.key]}
                  onChange={(e) => setBerkusScores({ ...berkusScores, [factor.key]: Number(e.target.value) })}
                  style={{ width: '100%', accentColor: FINCEPT.ORANGE }}
                />
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{factor.desc}</div>
              </div>
            ))}
          </div>
        );

      case 'scorecard':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div style={{ display: 'flex', gap: SPACING.SMALL }}>
              <div style={{ flex: 1 }}>
                <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Stage</label>
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
              <div style={{ flex: 1 }}>
                <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Region</label>
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
              <div key={factor.key} style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                <span style={{ flex: 1, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
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
                  style={{ ...COMMON_STYLES.inputField, width: '60px', fontSize: TYPOGRAPHY.TINY }}
                />
              </div>
            ))}
          </div>
        );

      case 'vc':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Exit Year Revenue ($M)</label>
              <input
                type="number"
                value={vcInputs.exitYearMetric}
                onChange={(e) => setVcInputs({ ...vcInputs, exitYearMetric: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Exit Multiple (x Revenue)</label>
              <input
                type="number"
                step="0.5"
                value={vcInputs.exitMultiple}
                onChange={(e) => setVcInputs({ ...vcInputs, exitMultiple: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Years to Exit</label>
              <input
                type="number"
                value={vcInputs.yearsToExit}
                onChange={(e) => setVcInputs({ ...vcInputs, yearsToExit: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Investment Amount ($M)</label>
              <input
                type="number"
                value={vcInputs.investmentAmount}
                onChange={(e) => setVcInputs({ ...vcInputs, investmentAmount: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Stage</label>
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
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div style={{ ...COMMON_STYLES.dataLabel }}>SCENARIO ANALYSIS</div>
            {chicagoScenarios.map((scenario, idx) => (
              <div key={idx} style={{
                padding: SPACING.SMALL,
                backgroundColor: FINCEPT.PANEL_BG,
                borderRadius: '2px',
                border: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <div style={{ fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginBottom: SPACING.TINY }}>
                  {scenario.name}
                </div>
                <div style={{ display: 'flex', gap: SPACING.SMALL }}>
                  <div style={{ flex: 1 }}>
                    <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Probability %</label>
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
                  <div style={{ flex: 1 }}>
                    <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Exit Value ($M)</label>
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
              </div>
            ))}
          </div>
        );

      case 'risk_factor':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Base Valuation ($M)</label>
              <input
                type="number"
                value={riskInputs.baseValuation}
                onChange={(e) => setRiskInputs({ ...riskInputs, baseValuation: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div style={{ ...COMMON_STYLES.dataLabel, marginTop: SPACING.SMALL }}>
              RISK ADJUSTMENTS (-2 to +2)
            </div>
            <div style={{ maxHeight: '300px', overflowY: 'auto' }}>
              {RISK_FACTORS.map(factor => (
                <div key={factor.key} style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL, marginBottom: '4px' }}>
                  <span style={{ flex: 1, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{factor.label}</span>
                  <select
                    value={riskInputs.assessments[factor.key]}
                    onChange={(e) => setRiskInputs({
                      ...riskInputs,
                      assessments: { ...riskInputs.assessments, [factor.key]: Number(e.target.value) }
                    })}
                    style={{ ...COMMON_STYLES.inputField, width: '80px', fontSize: TYPOGRAPHY.TINY }}
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
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Startup Name</label>
              <input
                type="text"
                value={startupName}
                onChange={(e) => setStartupName(e.target.value)}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div style={{
              padding: SPACING.SMALL,
              backgroundColor: `${FINCEPT.ORANGE}15`,
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.ORANGE}40`,
            }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
                COMPREHENSIVE ANALYSIS
              </div>
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px' }}>
                Runs all 5 valuation methods using inputs from each tab. Configure each method first, then run comprehensive for a complete valuation range.
              </div>
            </div>
          </div>
        );
    }
  };

  const renderResults = () => {
    if (!result) {
      return (
        <div style={{ ...COMMON_STYLES.emptyState }}>
          <Rocket size={32} style={{ opacity: 0.3, marginBottom: SPACING.SMALL }} />
          <span style={{ color: FINCEPT.GRAY }}>Run a valuation to see results</span>
        </div>
      );
    }

    return (
      <div style={{ padding: SPACING.DEFAULT }}>
        <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
          VALUATION RESULTS
        </div>

        {/* Main valuation */}
        {result.valuation && (
          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.PANEL_BG,
            borderRadius: '2px',
            border: `1px solid ${FINCEPT.ORANGE}`,
            marginBottom: SPACING.DEFAULT,
            textAlign: 'center',
          }}>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>ESTIMATED VALUATION</div>
            <div style={{ fontSize: '24px', color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
              {formatCurrency(result.valuation)}
            </div>
          </div>
        )}

        {/* VC Method specific */}
        {result.pre_money_valuation && (
          <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
            <div style={{ flex: 1, padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>PRE-MONEY</div>
              <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.GREEN }}>
                {formatCurrency(result.pre_money_valuation)}
              </div>
            </div>
            <div style={{ flex: 1, padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>POST-MONEY</div>
              <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.CYAN }}>
                {formatCurrency(result.post_money_valuation)}
              </div>
            </div>
          </div>
        )}

        {/* First Chicago specific */}
        {result.weighted_valuation && (
          <div style={{ marginBottom: SPACING.DEFAULT }}>
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.PANEL_BG,
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.ORANGE}`,
              textAlign: 'center',
            }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>PROBABILITY-WEIGHTED VALUE</div>
              <div style={{ fontSize: '24px', color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
                {formatCurrency(result.weighted_valuation)}
              </div>
            </div>
          </div>
        )}

        {/* Risk Factor specific */}
        {result.adjusted_valuation && (
          <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
            <div style={{ flex: 1, padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>BASE VALUE</div>
              <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.GRAY }}>
                {formatCurrency(result.base_valuation || riskInputs.baseValuation * 1e6)}
              </div>
            </div>
            <div style={{ flex: 1, padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '4px' }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>ADJUSTED VALUE</div>
              <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.GREEN }}>
                {formatCurrency(result.adjusted_valuation)}
              </div>
            </div>
          </div>
        )}

        {/* Comprehensive results */}
        {result.methods && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            {Object.entries(result.methods).map(([methodName, methodResult]: [string, any]) => (
              <div key={methodName} style={{
                padding: SPACING.SMALL,
                backgroundColor: FINCEPT.PANEL_BG,
                borderRadius: '2px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}>
                <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL }}>
                  {methodName.replace(/_/g, ' ').toUpperCase()}
                </span>
                <span style={{ color: FINCEPT.GREEN, fontWeight: TYPOGRAPHY.BOLD }}>
                  {formatCurrency(methodResult.valuation || methodResult.weighted_valuation || methodResult.adjusted_valuation || 0)}
                </span>
              </div>
            ))}
          </div>
        )}

        {/* Raw JSON for debugging */}
        <details style={{ marginTop: SPACING.DEFAULT }}>
          <summary style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, cursor: 'pointer' }}>
            Raw Output
          </summary>
          <pre style={{
            fontSize: '9px',
            color: FINCEPT.GRAY,
            backgroundColor: FINCEPT.PANEL_BG,
            padding: SPACING.SMALL,
            borderRadius: '2px',
            overflow: 'auto',
            maxHeight: '200px',
          }}>
            {JSON.stringify(result, null, 2)}
          </pre>
        </details>
      </div>
    );
  };

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT - Method Selector & Inputs */}
      <div style={{
        width: '380px',
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {/* Method Tabs */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(3, 1fr)',
          gap: '1px',
          backgroundColor: FINCEPT.BORDER,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          {METHODS.map(m => (
            <button
              key={m.id}
              onClick={() => { setMethod(m.id); setResult(null); }}
              style={{
                padding: SPACING.SMALL,
                backgroundColor: method === m.id ? FINCEPT.PANEL_BG : FINCEPT.DARK_BG,
                border: 'none',
                borderBottom: method === m.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: method === m.id ? FINCEPT.ORANGE : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {m.label}
            </button>
          ))}
        </div>

        {/* Method Description */}
        <div style={{
          padding: SPACING.SMALL,
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>
            {METHODS.find(m => m.id === method)?.desc}
          </div>
        </div>

        {/* Inputs */}
        <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
          {renderInputs()}
        </div>

        {/* Run Button */}
        <div style={{ padding: SPACING.DEFAULT, borderTop: `1px solid ${FINCEPT.BORDER}` }}>
          <button
            onClick={handleRun}
            disabled={loading}
            style={{
              width: '100%',
              padding: SPACING.SMALL,
              backgroundColor: loading ? FINCEPT.PANEL_BG : FINCEPT.ORANGE,
              border: 'none',
              borderRadius: '2px',
              color: loading ? FINCEPT.GRAY : FINCEPT.DARK_BG,
              cursor: loading ? 'not-allowed' : 'pointer',
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
              fontFamily: TYPOGRAPHY.MONO,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: SPACING.SMALL,
            }}
          >
            {loading ? <Loader2 size={14} className="animate-spin" /> : <PlayCircle size={14} />}
            {loading ? 'CALCULATING...' : `RUN ${METHODS.find(m => m.id === method)?.label}`}
          </button>
        </div>
      </div>

      {/* RIGHT - Results */}
      <div style={{ flex: 1, overflow: 'auto', backgroundColor: FINCEPT.DARK_BG }}>
        {renderResults()}
      </div>
    </div>
  );
};
