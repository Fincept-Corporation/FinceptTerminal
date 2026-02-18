/**
 * EconomicsPanel — Macro economics, business cycle, equity risk premium, active management.
 * Sub-tabs: BUSINESS CYCLE | EQUITY RISK PREMIUM | PORTFOLIO OVERVIEW | ACTIVE MANAGEMENT
 */
import React, { useState, useCallback, useEffect, useMemo } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { TrendingUp, Activity, Shield, Zap, RefreshCw, AlertCircle, BarChart3 } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { cacheService } from '../../../../services/cache/cacheService';

interface EconomicsPanelProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
}

type SubTab = 'cycle' | 'erp' | 'overview' | 'active';

const SUB_TABS: { id: SubTab; label: string; icon: React.ElementType }[] = [
  { id: 'cycle', label: 'BUSINESS CYCLE', icon: Activity },
  { id: 'erp', label: 'EQUITY RISK PREMIUM', icon: TrendingUp },
  { id: 'overview', label: 'PORTFOLIO OVERVIEW', icon: BarChart3 },
  { id: 'active', label: 'ACTIVE MGMT', icon: Shield },
];

const CYCLE_PHASES = ['expansion', 'peak', 'contraction', 'trough', 'recovery'];

function safeParse(raw: string): any {
  try { return JSON.parse(raw); } catch { return raw; }
}

const EconomicsPanel: React.FC<EconomicsPanelProps> = ({ portfolioSummary, currency }) => {
  const holdings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
  const [subTab, setSubTab] = useState<SubTab>('cycle');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Inputs
  const [cyclePhase, setCyclePhase] = useState('expansion');
  const [riskFreeRate, setRiskFreeRate] = useState(0.04);
  const [marketRiskPremium, setMarketRiskPremium] = useState(0.06);

  // Data
  const [businessCycle, setBusinessCycle] = useState<any>(null);
  const [comprehensiveEcon, setComprehensiveEcon] = useState<any>(null);
  const [erpResult, setErpResult] = useState<any>(null);
  const [portfolioOverview, setPortfolioOverview] = useState<any>(null);
  const [activeManagement, setActiveManagement] = useState<any>(null);
  const [managerSelection, setManagerSelection] = useState<any>(null);

  const buildReturnsJson = useCallback(() => {
    return JSON.stringify(holdings.map(h => h.symbol).join(','));
  }, [holdings]);

  const buildWeightsJson = useCallback(() => {
    return JSON.stringify(holdings.map(h => h.weight / 100));
  }, [holdings]);

  // ── Cache prefix: per portfolio composition ──────────────────────────────────
  const portfolioCacheKey = useMemo(() => {
    const symbols = holdings.map(h => h.symbol).sort().join(',');
    const weights = holdings.map(h => Math.round(h.weight * 10)).join(',');
    return `econ:${symbols}:${weights}`;
  }, [holdings]);

  // ── Restore cached results on mount ──────────────────────────────────────────
  useEffect(() => {
    let cancelled = false;
    const restore = async () => {
      // Portfolio-keyed results
      const portfolioKeys: [string, (v: any) => void][] = [
        [`${portfolioCacheKey}:overview`,         setPortfolioOverview],
        [`${portfolioCacheKey}:active_mgmt`,      setActiveManagement],
        [`${portfolioCacheKey}:manager_selection`,setManagerSelection],
      ];
      // Input-keyed results (cycle / ERP depend on user inputs, restore last used)
      const genericKeys: [string, (v: any) => void][] = [
        [`econ:cycle:${cyclePhase}`,                                    setBusinessCycle],
        [`econ:econ:${cyclePhase}`,                                     setComprehensiveEcon],
        [`econ:erp:${riskFreeRate}:${marketRiskPremium}`,               setErpResult],
      ];
      await Promise.all([...portfolioKeys, ...genericKeys].map(async ([cKey, setter]) => {
        const cached = await cacheService.get<any>(cKey);
        if (!cancelled && cached) setter(cached.data);
      }));
    };
    restore();
    return () => { cancelled = true; };
  }, [portfolioCacheKey]); // eslint-disable-line react-hooks/exhaustive-deps

  // ── Business Cycle ──
  const fetchCycle = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const results = await Promise.allSettled([
        invoke<string>('analyze_business_cycle', { cyclePhase }),
        invoke<string>('comprehensive_economics_analysis', { cyclePhase }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setBusinessCycle(d);
        cacheService.set(`econ:cycle:${cyclePhase}`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setComprehensiveEcon(d);
        cacheService.set(`econ:econ:${cyclePhase}`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [cyclePhase]);

  // ── Equity Risk Premium ──
  const fetchERP = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const result = await invoke<string>('analyze_equity_risk_premium', { riskFreeRate, marketRiskPremium });
      const d = safeParse(result);
      setErpResult(d);
      cacheService.set(`econ:erp:${riskFreeRate}:${marketRiskPremium}`, d, 'api-response', '1h');
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [riskFreeRate, marketRiskPremium]);

  // ── Portfolio Overview ──
  const fetchOverview = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const returnsData = buildReturnsJson();
      const weights = buildWeightsJson();
      const result = await invoke<string>('get_portfolio_overview', { returnsData, weights });
      const d = safeParse(result);
      setPortfolioOverview(d);
      cacheService.set(`${portfolioCacheKey}:overview`, d, 'api-response', '1h');
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [buildReturnsJson, buildWeightsJson, portfolioCacheKey]);

  // ── Active Management ──
  const fetchActive = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const fundData = JSON.stringify({
        holdings: holdings.map(h => ({ symbol: h.symbol, weight: h.weight })),
        total_value: portfolioSummary.total_market_value,
      });
      const managerCandidates = JSON.stringify(holdings.map(h => h.symbol));
      const results = await Promise.allSettled([
        invoke<string>('analyze_active_management', { fundData }),
        invoke<string>('analyze_manager_selection', { managerCandidates }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setActiveManagement(d);
        cacheService.set(`${portfolioCacheKey}:active_mgmt`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setManagerSelection(d);
        cacheService.set(`${portfolioCacheKey}:manager_selection`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [holdings, portfolioSummary.total_market_value, portfolioCacheKey]);

  const handleRun = () => {
    if (subTab === 'cycle') fetchCycle();
    else if (subTab === 'erp') fetchERP();
    else if (subTab === 'overview') fetchOverview();
    else if (subTab === 'active') fetchActive();
  };

  const inputStyle: React.CSSProperties = { ...COMMON_STYLES.inputField, width: '100%', fontSize: TYPOGRAPHY.SMALL };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG }}>
      {/* Header */}
      <div style={{ padding: '10px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.PANEL_BG, display: 'flex', alignItems: 'center', gap: '10px', flexShrink: 0 }}>
        <TrendingUp size={16} color={FINCEPT.YELLOW} />
        <span style={{ color: FINCEPT.YELLOW, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: 'monospace' }}>ECONOMICS & MARKETS</span>
        <div style={{ flex: 1 }} />
        {SUB_TABS.map(t => {
          const Icon = t.icon;
          const active = subTab === t.id;
          return (
            <button key={t.id} onClick={() => setSubTab(t.id)} style={{
              padding: '4px 10px', backgroundColor: active ? `${FINCEPT.YELLOW}30` : 'transparent',
              border: `1px solid ${active ? FINCEPT.YELLOW : FINCEPT.BORDER}`, color: active ? FINCEPT.YELLOW : FINCEPT.GRAY,
              fontSize: '9px', fontWeight: 700, cursor: 'pointer', fontFamily: 'monospace',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <Icon size={10} /> {t.label}
            </button>
          );
        })}
        <button onClick={handleRun} disabled={loading} style={{
          padding: '6px 14px', backgroundColor: loading ? FINCEPT.MUTED : FINCEPT.YELLOW,
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
        {/* BUSINESS CYCLE */}
        {subTab === 'cycle' && (
          <div>
            <div style={{ display: 'grid', gridTemplateColumns: '200px 1fr', gap: SPACING.XLARGE }}>
              <div>
                <div style={{ color: FINCEPT.YELLOW, fontSize: '10px', fontWeight: 700, marginBottom: SPACING.LARGE, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.SMALL }}>CYCLE PHASE</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                  {CYCLE_PHASES.map(phase => (
                    <button key={phase} onClick={() => setCyclePhase(phase)} style={{
                      padding: '6px 10px', textAlign: 'left',
                      backgroundColor: cyclePhase === phase ? `${FINCEPT.YELLOW}30` : FINCEPT.PANEL_BG,
                      border: `1px solid ${cyclePhase === phase ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
                      color: cyclePhase === phase ? FINCEPT.YELLOW : FINCEPT.GRAY,
                      fontSize: '10px', fontWeight: 700, cursor: 'pointer', fontFamily: 'monospace',
                      textTransform: 'uppercase',
                    }}>
                      {phase}
                    </button>
                  ))}
                </div>
              </div>
              <div>
                {businessCycle && <ResultSection title="BUSINESS CYCLE ANALYSIS" data={businessCycle} color={FINCEPT.YELLOW} />}
                {comprehensiveEcon && <ResultSection title="COMPREHENSIVE ECONOMICS" data={comprehensiveEcon} color={FINCEPT.CYAN} />}
                {!businessCycle && !comprehensiveEcon && !loading && (
                  <EmptyState text="Select a business cycle phase and click RUN" />
                )}
              </div>
            </div>
          </div>
        )}

        {/* EQUITY RISK PREMIUM */}
        {subTab === 'erp' && (
          <div>
            <div style={{ display: 'grid', gridTemplateColumns: '250px 1fr', gap: SPACING.XLARGE }}>
              <div>
                <div style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700, marginBottom: SPACING.LARGE, borderBottom: `1px solid ${FINCEPT.GREEN}`, paddingBottom: SPACING.SMALL }}>ERP PARAMETERS</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>RISK-FREE RATE</label>
                    <input type="number" step="0.01" value={riskFreeRate} onChange={(e) => setRiskFreeRate(Number(e.target.value))} style={inputStyle} />
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>MARKET RISK PREMIUM</label>
                    <input type="number" step="0.01" value={marketRiskPremium} onChange={(e) => setMarketRiskPremium(Number(e.target.value))} style={inputStyle} />
                  </div>
                </div>
              </div>
              <div>
                {erpResult && <ResultSection title="EQUITY RISK PREMIUM ANALYSIS" data={erpResult} color={FINCEPT.GREEN} />}
                {!erpResult && !loading && <EmptyState text="Configure parameters and click RUN for ERP analysis" />}
              </div>
            </div>
          </div>
        )}

        {/* PORTFOLIO OVERVIEW */}
        {subTab === 'overview' && (
          <div>
            {portfolioOverview && <ResultSection title="PORTFOLIO OVERVIEW (Python)" data={portfolioOverview} color={FINCEPT.ORANGE} />}
            {!portfolioOverview && !loading && <EmptyState text="Click RUN to get a comprehensive portfolio overview with optimized comparison" />}
          </div>
        )}

        {/* ACTIVE MANAGEMENT */}
        {subTab === 'active' && (
          <div>
            {!activeManagement && !managerSelection && !loading && (
              <EmptyState text="Click RUN to analyze active management and manager selection" />
            )}
            {activeManagement && <ResultSection title="ACTIVE MANAGEMENT ANALYSIS" data={activeManagement} color={FINCEPT.CYAN} />}
            {managerSelection && <ResultSection title="MANAGER SELECTION" data={managerSelection} color={FINCEPT.ORANGE} />}
          </div>
        )}
      </div>
    </div>
  );
};

const EmptyState: React.FC<{ text: string }> = ({ text }) => (
  <div style={{ textAlign: 'center', padding: '60px 0' }}>
    <TrendingUp size={36} color={FINCEPT.MUTED} style={{ opacity: 0.3, marginBottom: '12px' }} />
    <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '6px' }}>ECONOMICS ENGINE</div>
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

export default EconomicsPanel;
