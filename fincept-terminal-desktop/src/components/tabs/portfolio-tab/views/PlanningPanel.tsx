/**
 * PlanningPanel — Financial planning, retirement, behavioral analysis, and ETF costs.
 * Sub-tabs: ASSET ALLOCATION | RETIREMENT | BEHAVIORAL | ETF ANALYSIS
 */
import React, { useState, useCallback, useEffect, useMemo } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { Target, Users, Brain, TrendingUp, Zap, RefreshCw, AlertCircle } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { cacheService } from '../../../../services/cache/cacheService';

interface PlanningPanelProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
}

type SubTab = 'allocation' | 'retirement' | 'behavioral' | 'etf';

const SUB_TABS: { id: SubTab; label: string; icon: React.ElementType }[] = [
  { id: 'allocation', label: 'ASSET ALLOCATION', icon: Target },
  { id: 'retirement', label: 'RETIREMENT', icon: TrendingUp },
  { id: 'behavioral', label: 'BEHAVIORAL', icon: Brain },
  { id: 'etf', label: 'ETF ANALYSIS', icon: Users },
];

function safeParse(raw: string): any {
  try { return JSON.parse(raw); } catch { return raw; }
}

const PlanningPanel: React.FC<PlanningPanelProps> = ({ portfolioSummary, currency }) => {
  const holdings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
  const [subTab, setSubTab] = useState<SubTab>('allocation');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Inputs
  const [age, setAge] = useState(30);
  const [riskTolerance, setRiskTolerance] = useState<'conservative' | 'moderate' | 'aggressive'>('moderate');
  const [yearsToRetirement, setYearsToRetirement] = useState(30);
  const [retirementAge, setRetirementAge] = useState(60);
  const [currentSavings, setCurrentSavings] = useState(50000);
  const [annualContribution, setAnnualContribution] = useState(12000);

  // Data
  const [allocationResult, setAllocationResult] = useState<any>(null);
  const [strategicResult, setStrategicResult] = useState<any>(null);
  const [retirementResult, setRetirementResult] = useState<any>(null);
  const [retirementAdvResult, setRetirementAdvResult] = useState<any>(null);
  const [behavioralBiases, setBehavioralBiases] = useState<any>(null);
  const [behavioralFinance, setBehavioralFinance] = useState<any>(null);
  const [etfCosts, setEtfCosts] = useState<any>(null);
  const [etfAnalysis, setEtfAnalysis] = useState<any>(null);

  const buildPortfolioData = useCallback(() => {
    return JSON.stringify({
      holdings: holdings.map(h => ({ symbol: h.symbol, weight: h.weight, quantity: h.quantity, market_value: h.market_value })),
      total_value: portfolioSummary.total_market_value,
    });
  }, [holdings, portfolioSummary.total_market_value]);

  // ── Cache prefix: per portfolio composition ──────────────────────────────────
  const portfolioCacheKey = useMemo(() => {
    const symbols = holdings.map(h => h.symbol).sort().join(',');
    const weights = holdings.map(h => Math.round(h.weight * 10)).join(',');
    return `planning:${symbols}:${weights}`;
  }, [holdings]);

  // ── Restore cached results on mount ──────────────────────────────────────────
  useEffect(() => {
    let cancelled = false;
    const restore = async () => {
      const keys: [string, (v: any) => void][] = [
        [`planning:alloc:${age}:${riskTolerance}:${yearsToRetirement}`,                    setAllocationResult],
        [`planning:strategic:${age}:${riskTolerance}:${yearsToRetirement}`,                setStrategicResult],
        [`planning:retire:${age}:${retirementAge}:${currentSavings}:${annualContribution}`,setRetirementResult],
        [`planning:retire_adv:${age}:${retirementAge}:${currentSavings}:${annualContribution}`, setRetirementAdvResult],
        [`${portfolioCacheKey}:behavioral_biases`,   setBehavioralBiases],
        [`${portfolioCacheKey}:behavioral_finance`,  setBehavioralFinance],
        [`${portfolioCacheKey}:etf_costs`,           setEtfCosts],
        [`${portfolioCacheKey}:etf_analysis`,        setEtfAnalysis],
      ];
      await Promise.all(keys.map(async ([cKey, setter]) => {
        const cached = await cacheService.get<any>(cKey);
        if (!cancelled && cached) setter(cached.data);
      }));
    };
    restore();
    return () => { cancelled = true; };
  }, [portfolioCacheKey]); // eslint-disable-line react-hooks/exhaustive-deps

  // ── Asset Allocation ──
  const fetchAllocation = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const results = await Promise.allSettled([
        invoke<string>('generate_asset_allocation', { age, riskTolerance, yearsToRetirement }),
        invoke<string>('strategic_asset_allocation', { age, riskTolerance, timeHorizon: yearsToRetirement }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setAllocationResult(d);
        cacheService.set(`planning:alloc:${age}:${riskTolerance}:${yearsToRetirement}`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setStrategicResult(d);
        cacheService.set(`planning:strategic:${age}:${riskTolerance}:${yearsToRetirement}`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [age, riskTolerance, yearsToRetirement]);

  // ── Retirement ──
  const fetchRetirement = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const results = await Promise.allSettled([
        invoke<string>('calculate_retirement_plan', { currentAge: age, retirementAge, currentSavings, annualContribution }),
        invoke<string>('calculate_retirement_planning', { currentAge: age, retirementAge, currentSavings, annualContribution }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setRetirementResult(d);
        cacheService.set(`planning:retire:${age}:${retirementAge}:${currentSavings}:${annualContribution}`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setRetirementAdvResult(d);
        cacheService.set(`planning:retire_adv:${age}:${retirementAge}:${currentSavings}:${annualContribution}`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [age, retirementAge, currentSavings, annualContribution]);

  // ── Behavioral ──
  const fetchBehavioral = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const portfolioData = buildPortfolioData();
      const investorData = JSON.stringify({ age, risk_tolerance: riskTolerance, portfolio: portfolioData });
      const results = await Promise.allSettled([
        invoke<string>('analyze_behavioral_biases', { portfolioData }),
        invoke<string>('analyze_behavioral_finance', { investorData }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setBehavioralBiases(d);
        cacheService.set(`${portfolioCacheKey}:behavioral_biases`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setBehavioralFinance(d);
        cacheService.set(`${portfolioCacheKey}:behavioral_finance`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [buildPortfolioData, age, riskTolerance, portfolioCacheKey]);

  // ── ETF Analysis ──
  const fetchETF = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const symbols = holdings.map(h => h.symbol).join(',');
      const expenseRatios = JSON.stringify(holdings.map(() => 0.003)); // default 0.3% ER
      const etfData = JSON.stringify({ symbols: holdings.map(h => h.symbol) });
      const results = await Promise.allSettled([
        invoke<string>('analyze_etf_costs', { symbols, expenseRatios }),
        invoke<string>('analyze_etf', { etfData }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setEtfCosts(d);
        cacheService.set(`${portfolioCacheKey}:etf_costs`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setEtfAnalysis(d);
        cacheService.set(`${portfolioCacheKey}:etf_analysis`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [holdings, portfolioCacheKey]);

  const handleRun = () => {
    if (subTab === 'allocation') fetchAllocation();
    else if (subTab === 'retirement') fetchRetirement();
    else if (subTab === 'behavioral') fetchBehavioral();
    else if (subTab === 'etf') fetchETF();
  };

  const inputStyle: React.CSSProperties = {
    ...COMMON_STYLES.inputField,
    width: '100%',
    fontSize: TYPOGRAPHY.SMALL,
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG }}>
      {/* Header */}
      <div style={{ padding: '10px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.PANEL_BG, display: 'flex', alignItems: 'center', gap: '10px', flexShrink: 0 }}>
        <Target size={16} color={FINCEPT.GREEN} />
        <span style={{ color: FINCEPT.GREEN, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: 'monospace' }}>PLANNING & BEHAVIORAL</span>
        <div style={{ flex: 1 }} />
        {SUB_TABS.map(t => {
          const Icon = t.icon;
          const active = subTab === t.id;
          return (
            <button key={t.id} onClick={() => setSubTab(t.id)} style={{
              padding: '4px 10px', backgroundColor: active ? `${FINCEPT.GREEN}30` : 'transparent',
              border: `1px solid ${active ? FINCEPT.GREEN : FINCEPT.BORDER}`, color: active ? FINCEPT.GREEN : FINCEPT.GRAY,
              fontSize: '9px', fontWeight: 700, cursor: 'pointer', fontFamily: 'monospace',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <Icon size={10} /> {t.label}
            </button>
          );
        })}
        <button onClick={handleRun} disabled={loading} style={{
          padding: '6px 14px', backgroundColor: loading ? FINCEPT.MUTED : FINCEPT.GREEN,
          border: 'none', color: FINCEPT.DARK_BG, fontSize: '10px', fontWeight: 700,
          cursor: loading ? 'not-allowed' : 'pointer', fontFamily: 'monospace',
          display: 'flex', alignItems: 'center', gap: '4px',
        }}>
          {loading ? <RefreshCw size={10} className="animate-spin" /> : <Zap size={10} />}
          {loading ? 'RUNNING...' : 'RUN'}
        </button>
      </div>

      {error && (
        <div style={{ padding: '8px 16px', backgroundColor: `${FINCEPT.RED}15`, borderBottom: `1px solid ${FINCEPT.RED}`, display: 'flex', alignItems: 'center', gap: '8px' }}>
          <AlertCircle size={12} color={FINCEPT.RED} />
          <span style={{ color: FINCEPT.RED, fontSize: '10px', fontFamily: 'monospace' }}>{error}</span>
        </div>
      )}

      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {/* ASSET ALLOCATION */}
        {subTab === 'allocation' && (
          <div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 2fr', gap: SPACING.XLARGE }}>
              <div>
                <div style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700, marginBottom: SPACING.LARGE, borderBottom: `1px solid ${FINCEPT.GREEN}`, paddingBottom: SPACING.SMALL }}>INPUT PARAMETERS</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>AGE</label>
                    <input type="number" value={age} onChange={(e) => setAge(Number(e.target.value))} style={inputStyle} />
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>RISK TOLERANCE</label>
                    <select value={riskTolerance} onChange={(e) => setRiskTolerance(e.target.value as any)} style={inputStyle}>
                      <option value="conservative">Conservative</option>
                      <option value="moderate">Moderate</option>
                      <option value="aggressive">Aggressive</option>
                    </select>
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>YEARS TO RETIREMENT</label>
                    <input type="number" value={yearsToRetirement} onChange={(e) => setYearsToRetirement(Number(e.target.value))} style={inputStyle} />
                  </div>
                </div>
              </div>
              <div>
                {allocationResult && <ResultSection title="ASSET ALLOCATION" data={allocationResult} color={FINCEPT.GREEN} />}
                {strategicResult && <ResultSection title="STRATEGIC ASSET ALLOCATION" data={strategicResult} color={FINCEPT.CYAN} />}
                {!allocationResult && !strategicResult && !loading && (
                  <EmptyState text="Configure your parameters and click RUN to generate asset allocation recommendations" />
                )}
              </div>
            </div>
          </div>
        )}

        {/* RETIREMENT */}
        {subTab === 'retirement' && (
          <div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 2fr', gap: SPACING.XLARGE }}>
              <div>
                <div style={{ color: FINCEPT.ORANGE, fontSize: '10px', fontWeight: 700, marginBottom: SPACING.LARGE, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.SMALL }}>RETIREMENT INPUTS</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>CURRENT AGE</label>
                    <input type="number" value={age} onChange={(e) => setAge(Number(e.target.value))} style={inputStyle} />
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>RETIREMENT AGE</label>
                    <input type="number" value={retirementAge} onChange={(e) => setRetirementAge(Number(e.target.value))} style={inputStyle} />
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>CURRENT SAVINGS ({currency})</label>
                    <input type="number" value={currentSavings} onChange={(e) => setCurrentSavings(Number(e.target.value))} style={inputStyle} />
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>ANNUAL CONTRIBUTION ({currency})</label>
                    <input type="number" value={annualContribution} onChange={(e) => setAnnualContribution(Number(e.target.value))} style={inputStyle} />
                  </div>
                </div>
              </div>
              <div>
                {retirementResult && <ResultSection title="RETIREMENT PLAN" data={retirementResult} color={FINCEPT.ORANGE} />}
                {retirementAdvResult && <ResultSection title="ADVANCED RETIREMENT PLANNING" data={retirementAdvResult} color={FINCEPT.YELLOW} />}
                {!retirementResult && !retirementAdvResult && !loading && (
                  <EmptyState text="Enter your retirement parameters and click RUN" />
                )}
              </div>
            </div>
          </div>
        )}

        {/* BEHAVIORAL */}
        {subTab === 'behavioral' && (
          <div>
            {!behavioralBiases && !behavioralFinance && !loading && (
              <EmptyState text="Click RUN to detect behavioral biases in your portfolio" />
            )}
            {behavioralBiases && <ResultSection title="BEHAVIORAL BIASES (Overconfidence, Loss Aversion, etc.)" data={behavioralBiases} color={FINCEPT.YELLOW} />}
            {behavioralFinance && <ResultSection title="BEHAVIORAL FINANCE ANALYSIS" data={behavioralFinance} color={FINCEPT.CYAN} />}
          </div>
        )}

        {/* ETF ANALYSIS */}
        {subTab === 'etf' && (
          <div>
            {!etfCosts && !etfAnalysis && !loading && (
              <EmptyState text="Click RUN to analyze ETF costs and characteristics for your holdings" />
            )}
            {etfCosts && <ResultSection title="ETF EXPENSE RATIO IMPACT" data={etfCosts} color={FINCEPT.RED} />}
            {etfAnalysis && <ResultSection title="ETF CHARACTERISTICS" data={etfAnalysis} color={FINCEPT.GREEN} />}
          </div>
        )}
      </div>
    </div>
  );
};

const EmptyState: React.FC<{ text: string }> = ({ text }) => (
  <div style={{ textAlign: 'center', padding: '60px 0' }}>
    <Target size={36} color={FINCEPT.MUTED} style={{ opacity: 0.3, marginBottom: '12px' }} />
    <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '6px' }}>PLANNING ENGINE</div>
    <div style={{ color: FINCEPT.MUTED, fontSize: '10px', maxWidth: '500px', margin: '0 auto' }}>{text}</div>
  </div>
);

const ResultSection: React.FC<{ title: string; data: any; color: string }> = ({ title, data, color }) => {
  const flat = typeof data === 'object' && data !== null
    ? Object.entries(data).filter(([, v]) => v !== null && v !== undefined && typeof v !== 'object')
    : [];
  const nested = typeof data === 'object' && data !== null
    ? Object.entries(data).filter(([, v]) => typeof v === 'object' && v !== null && !Array.isArray(v))
    : [];
  return (
    <div style={{ marginBottom: SPACING.XLARGE }}>
      <div style={{ color, fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: SPACING.MEDIUM, paddingBottom: SPACING.SMALL, borderBottom: `1px solid ${color}` }}>{title}</div>
      {flat.length > 0 && (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: SPACING.MEDIUM, marginBottom: nested.length > 0 ? SPACING.LARGE : 0 }}>
          {flat.map(([k, v]) => (
            <div key={k} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${color}30` }}>
              <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{k.replace(/_/g, ' ').toUpperCase()}</div>
              <div style={{ color, fontSize: '12px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                {typeof v === 'number' ? (Math.abs(v as number) < 1 ? (v as number).toFixed(4) : (v as number).toFixed(2)) : String(v)}
              </div>
            </div>
          ))}
        </div>
      )}
      {nested.map(([sKey, sData]) => (
        <div key={sKey} style={{ marginBottom: SPACING.LARGE }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, marginBottom: SPACING.SMALL }}>{sKey.replace(/_/g, ' ').toUpperCase()}</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: SPACING.MEDIUM }}>
            {Object.entries(sData as Record<string, any>).filter(([, v]) => v !== null && typeof v !== 'object').map(([k, v]) => (
              <div key={k} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${color}20` }}>
                <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{k.replace(/_/g, ' ').toUpperCase()}</div>
                <div style={{ color, fontSize: '11px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                  {typeof v === 'number' ? (Math.abs(v as number) < 1 ? (v as number).toFixed(4) : (v as number).toFixed(2)) : String(v)}
                </div>
              </div>
            ))}
          </div>
        </div>
      ))}
      {flat.length === 0 && nested.length === 0 && (
        <div style={{ color: FINCEPT.MUTED, fontSize: '10px', fontFamily: 'monospace', padding: SPACING.MEDIUM, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, whiteSpace: 'pre-wrap', maxHeight: '250px', overflow: 'auto' }}>
          {typeof data === 'string' ? data : JSON.stringify(data, null, 2)}
        </div>
      )}
    </div>
  );
};

export default PlanningPanel;
