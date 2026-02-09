/**
 * Fairness Opinion Panel - Board-Level M&A Analysis
 *
 * Tools: Valuation Framework, Premium Analysis, Process Quality Assessment
 */

import React, { useState } from 'react';
import { Scale, PlayCircle, Loader2, TrendingUp, CheckCircle, AlertTriangle, FileText } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

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

export const FairnessOpinion: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('fairness');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);

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

  const renderInputs = () => {
    switch (analysisType) {
      case 'fairness':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Offer Price ($/share)</label>
              <input
                type="number"
                step="0.01"
                value={fairnessInputs.offerPrice}
                onChange={(e) => setFairnessInputs({ ...fairnessInputs, offerPrice: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>

            <div style={{ ...COMMON_STYLES.dataLabel, marginTop: SPACING.SMALL }}>
              VALUATION METHODS
            </div>
            {fairnessInputs.valuationMethods.map((method, idx) => (
              <div key={idx} style={{
                padding: SPACING.SMALL,
                backgroundColor: FINCEPT.PANEL_BG,
                borderRadius: '4px',
                border: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.ORANGE, marginBottom: '4px' }}>
                  {method.method}
                </div>
                <div style={{ display: 'flex', gap: SPACING.SMALL }}>
                  <div style={{ flex: 1 }}>
                    <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Value</label>
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
                    <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Min</label>
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
                    <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Max</label>
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

            <div style={{ ...COMMON_STYLES.dataLabel, marginTop: SPACING.SMALL }}>
              QUALITATIVE FACTORS
            </div>
            <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
              {QUALITATIVE_FACTORS.map(factor => (
                <div
                  key={factor}
                  onClick={() => toggleFactor(factor)}
                  style={{
                    padding: '6px',
                    marginBottom: '2px',
                    backgroundColor: fairnessInputs.selectedFactors.has(factor) ? `${FINCEPT.GREEN}20` : FINCEPT.PANEL_BG,
                    borderRadius: '4px',
                    border: `1px solid ${fairnessInputs.selectedFactors.has(factor) ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: SPACING.SMALL,
                  }}
                >
                  <CheckCircle
                    size={12}
                    color={fairnessInputs.selectedFactors.has(factor) ? FINCEPT.GREEN : FINCEPT.MUTED}
                  />
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{factor}</span>
                </div>
              ))}
            </div>
          </div>
        );

      case 'premium':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Offer Price ($/share)</label>
              <input
                type="number"
                step="0.01"
                value={premiumInputs.offerPrice}
                onChange={(e) => setPremiumInputs({ ...premiumInputs, offerPrice: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                Daily Prices (comma-separated, most recent last)
              </label>
              <textarea
                value={premiumInputs.dailyPrices}
                onChange={(e) => setPremiumInputs({ ...premiumInputs, dailyPrices: e.target.value })}
                rows={4}
                style={{ ...COMMON_STYLES.inputField, width: '100%', resize: 'vertical' }}
              />
            </div>
            <div>
              <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                Announcement Date (day index, 0-based)
              </label>
              <input
                type="number"
                value={premiumInputs.announcementDate}
                onChange={(e) => setPremiumInputs({ ...premiumInputs, announcementDate: Number(e.target.value) })}
                style={{ ...COMMON_STYLES.inputField, width: '100%' }}
              />
            </div>
            <div style={{
              padding: SPACING.SMALL,
              backgroundColor: `${FINCEPT.CYAN}15`,
              borderRadius: '4px',
              fontSize: TYPOGRAPHY.TINY,
              color: FINCEPT.CYAN,
            }}>
              Calculates premiums vs 1-day, 1-week, 1-month, and 52-week prices
            </div>
          </div>
        );

      case 'process':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            <div style={{ ...COMMON_STYLES.dataLabel }}>
              PROCESS QUALITY ASSESSMENT (1-5)
            </div>
            {PROCESS_FACTORS.map(factor => (
              <div key={factor.key} style={{
                padding: SPACING.SMALL,
                backgroundColor: FINCEPT.PANEL_BG,
                borderRadius: '4px',
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div>
                    <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.WHITE }}>{factor.label}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{factor.desc}</div>
                  </div>
                  <select
                    value={processInputs[factor.key]}
                    onChange={(e) => setProcessInputs({ ...processInputs, [factor.key]: Number(e.target.value) })}
                    style={{ ...COMMON_STYLES.inputField, width: '60px', fontSize: TYPOGRAPHY.TINY }}
                  >
                    <option value={1}>1 - Poor</option>
                    <option value={2}>2</option>
                    <option value={3}>3 - Average</option>
                    <option value={4}>4</option>
                    <option value={5}>5 - Excellent</option>
                  </select>
                </div>
              </div>
            ))}
          </div>
        );
    }
  };

  const renderResults = () => {
    if (!result) {
      return (
        <div style={{ ...COMMON_STYLES.emptyState }}>
          <Scale size={32} style={{ opacity: 0.3, marginBottom: SPACING.SMALL }} />
          <span style={{ color: FINCEPT.GRAY }}>Run analysis to see results</span>
        </div>
      );
    }

    return (
      <div style={{ padding: SPACING.DEFAULT }}>
        {/* Fairness Opinion Result */}
        {result.is_fair !== undefined && (
          <>
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: result.is_fair ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
              borderRadius: '4px',
              border: `2px solid ${result.is_fair ? FINCEPT.GREEN : FINCEPT.RED}`,
              textAlign: 'center',
              marginBottom: SPACING.DEFAULT,
            }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>FAIRNESS OPINION</div>
              <div style={{
                fontSize: '24px',
                color: result.is_fair ? FINCEPT.GREEN : FINCEPT.RED,
                fontWeight: TYPOGRAPHY.BOLD,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: SPACING.SMALL,
              }}>
                {result.is_fair ? <CheckCircle size={24} /> : <AlertTriangle size={24} />}
                {result.is_fair ? 'FAIR' : 'NOT FAIR'}
              </div>
            </div>

            {result.valuation_summary && (
              <div style={{ marginBottom: SPACING.DEFAULT }}>
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>VALUATION SUMMARY</div>
                <div style={{ display: 'flex', gap: SPACING.SMALL }}>
                  <div style={{ flex: 1, padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '4px' }}>
                    <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>OFFER</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.ORANGE }}>
                      {formatCurrency(result.offer_price || fairnessInputs.offerPrice)}
                    </div>
                  </div>
                  <div style={{ flex: 1, padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '4px' }}>
                    <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>AVG VALUATION</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.GREEN }}>
                      {formatCurrency(result.valuation_summary.average || 0)}
                    </div>
                  </div>
                </div>
              </div>
            )}
          </>
        )}

        {/* Premium Analysis Result */}
        {result.premium_1day !== undefined && (
          <>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>PREMIUM ANALYSIS</div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
              {[
                { label: '1-DAY', value: result.premium_1day },
                { label: '1-WEEK', value: result.premium_1week },
                { label: '1-MONTH', value: result.premium_1month },
                { label: '52-WEEK HIGH', value: result.premium_52week_high },
              ].map(item => (
                <div key={item.label} style={{
                  padding: SPACING.SMALL,
                  backgroundColor: FINCEPT.PANEL_BG,
                  borderRadius: '4px',
                  textAlign: 'center',
                }}>
                  <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{item.label}</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.HEADING,
                    color: (item.value || 0) >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                    fontWeight: TYPOGRAPHY.BOLD,
                  }}>
                    {item.value !== undefined ? `${item.value.toFixed(1)}%` : 'N/A'}
                  </div>
                </div>
              ))}
            </div>
          </>
        )}

        {/* Process Quality Result */}
        {result.overall_score !== undefined && (
          <>
            <div style={{
              padding: SPACING.DEFAULT,
              backgroundColor: FINCEPT.PANEL_BG,
              borderRadius: '4px',
              border: `1px solid ${FINCEPT.ORANGE}`,
              textAlign: 'center',
              marginBottom: SPACING.DEFAULT,
            }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>PROCESS QUALITY SCORE</div>
              <div style={{ fontSize: '32px', color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
                {result.overall_score.toFixed(1)} / 5
              </div>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>
                {result.overall_score >= 4 ? 'Strong Process' :
                 result.overall_score >= 3 ? 'Adequate Process' : 'Process Concerns'}
              </div>
            </div>

            {result.factor_scores && (
              <div>
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>FACTOR BREAKDOWN</div>
                {Object.entries(result.factor_scores).map(([key, score]: [string, any]) => (
                  <div key={key} style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    padding: '4px 0',
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  }}>
                    <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                      {key.replace(/_/g, ' ').toUpperCase()}
                    </span>
                    <span style={{
                      fontSize: TYPOGRAPHY.SMALL,
                      color: score >= 4 ? FINCEPT.GREEN : score >= 3 ? FINCEPT.ORANGE : FINCEPT.RED,
                      fontWeight: TYPOGRAPHY.BOLD,
                    }}>
                      {score}/5
                    </span>
                  </div>
                ))}
              </div>
            )}
          </>
        )}

        {/* Raw JSON */}
        <details style={{ marginTop: SPACING.DEFAULT }}>
          <summary style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, cursor: 'pointer' }}>
            Raw Output
          </summary>
          <pre style={{
            fontSize: '9px',
            color: FINCEPT.GRAY,
            backgroundColor: FINCEPT.PANEL_BG,
            padding: SPACING.SMALL,
            borderRadius: '4px',
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
      {/* LEFT - Analysis Type & Inputs */}
      <div style={{
        width: '380px',
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {/* Analysis Type Tabs */}
        <div style={{
          display: 'flex',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          {[
            { id: 'fairness' as const, label: 'FAIRNESS OPINION', icon: Scale },
            { id: 'premium' as const, label: 'PREMIUM', icon: TrendingUp },
            { id: 'process' as const, label: 'PROCESS', icon: FileText },
          ].map(tab => (
            <button
              key={tab.id}
              onClick={() => { setAnalysisType(tab.id); setResult(null); }}
              style={{
                flex: 1,
                padding: SPACING.SMALL,
                backgroundColor: 'transparent',
                border: 'none',
                borderBottom: analysisType === tab.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: analysisType === tab.id ? FINCEPT.ORANGE : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '4px',
              }}
            >
              <tab.icon size={12} />
              {tab.label}
            </button>
          ))}
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
              borderRadius: '4px',
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
            {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
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
