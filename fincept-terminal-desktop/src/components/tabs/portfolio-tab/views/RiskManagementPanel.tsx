/**
 * RiskManagementPanel — Comprehensive risk analytics using Fortitudo + portfolio.rs Python commands.
 * Sub-tabs: RISK OVERVIEW | STRESS TEST | FORTITUDO ADVANCED | EFFICIENT FRONTIERS
 */
import React, { useState, useCallback, useEffect, useMemo } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { Shield, AlertTriangle, Zap, RefreshCw, Target } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS } from '../finceptStyles';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { cacheService } from '../../../../services/cache/cacheService';

interface RiskManagementPanelProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
}

type SubTab = 'overview' | 'stresstest' | 'fortitudo' | 'frontiers';

const SUB_TABS: { id: SubTab; label: string; icon: React.ElementType }[] = [
  { id: 'overview',   label: 'RISK OVERVIEW',       icon: Shield },
  { id: 'stresstest', label: 'STRESS & DECAY',       icon: AlertTriangle },
  { id: 'fortitudo',  label: 'FORTITUDO ADVANCED',   icon: Zap },
  { id: 'frontiers',  label: 'EFFICIENT FRONTIERS',  icon: Target },
];

function safeParse(raw: string): any {
  try { return JSON.parse(raw); } catch { return raw; }
}

function fmt(v: any): string {
  if (v === null || v === undefined) return '—';
  if (typeof v === 'boolean') return v ? 'Yes' : 'No';
  if (typeof v === 'number') {
    const abs = Math.abs(v);
    if (abs === 0) return '0';
    if (abs < 0.0001) return v.toExponential(3);
    if (abs < 1) return v.toFixed(4);
    if (abs < 10000) return v.toFixed(3);
    return v.toLocaleString(undefined, { maximumFractionDigits: 0 });
  }
  return String(v);
}

// ─── Generic flat key-value TABLE ───────────────────────────────────────────
const KVTable: React.FC<{ title: string; data: Record<string, any>; color: string }> = ({ title, data, color }) => {
  const rows = Object.entries(data).filter(([, v]) => v !== null && v !== undefined && typeof v !== 'object');
  if (rows.length === 0) return null;
  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{ color, fontSize: '10px', fontWeight: 700, letterSpacing: '0.8px', marginBottom: '8px', paddingBottom: '4px', borderBottom: `1px solid ${color}` }}>
        {title}
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '11px', fontFamily: TYPOGRAPHY.MONO }}>
        <tbody>
          {rows.map(([k, v], i) => (
            <tr key={k} style={{ backgroundColor: i % 2 === 0 ? FINCEPT.PANEL_BG : 'transparent' }}>
              <td style={{ padding: '4px 8px', color: FINCEPT.GRAY, fontWeight: 600, width: '55%', borderRight: `1px solid ${FINCEPT.BORDER}` }}>
                {k.replace(/_/g, ' ').toUpperCase()}
              </td>
              <td style={{ padding: '4px 8px', color: typeof v === 'number' && v < 0 ? FINCEPT.RED : color, fontWeight: 700, textAlign: 'right' }}>
                {fmt(v)}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};

// ─── MATRIX HEATMAP TABLE (covariance / correlation) ─────────────────────────
const MatrixTable: React.FC<{ title: string; flatData: Record<string, number>; color: string }> = ({ title, flatData, color }) => {
  // Derive asset names from keys like "AAPL / MSFT"
  const assets = Array.from(new Set(
    Object.keys(flatData).flatMap(k => k.split(' / ').map(s => s.trim()))
  ));
  if (assets.length === 0) return null;

  const get = (r: string, c: string): number => flatData[`${r} / ${c}`] ?? flatData[`${c} / ${r}`] ?? 0;

  const allVals = assets.flatMap(r => assets.map(c => get(r, c)));
  const maxAbs = Math.max(...allVals.map(Math.abs), 0.0001);

  const cellBg = (v: number): string => {
    const intensity = Math.min(Math.abs(v) / maxAbs, 1);
    const isCorr = maxAbs <= 1.01; // correlation matrix
    if (isCorr) {
      if (v > 0) return `rgba(0,180,80,${intensity * 0.55})`;
      return `rgba(220,50,50,${intensity * 0.55})`;
    }
    return `rgba(0,180,220,${intensity * 0.55})`;
  };

  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{ color, fontSize: '10px', fontWeight: 700, letterSpacing: '0.8px', marginBottom: '8px', paddingBottom: '4px', borderBottom: `1px solid ${color}` }}>
        {title}
      </div>
      <div style={{ overflowX: 'auto' }}>
        <table style={{ borderCollapse: 'collapse', fontSize: '10px', fontFamily: TYPOGRAPHY.MONO }}>
          <thead>
            <tr>
              <th style={{ padding: '4px 8px', color: FINCEPT.MUTED, textAlign: 'left', borderBottom: `1px solid ${FINCEPT.BORDER}` }} />
              {assets.map(a => (
                <th key={a} style={{ padding: '4px 10px', color, fontWeight: 700, textAlign: 'center', borderBottom: `1px solid ${FINCEPT.BORDER}`, whiteSpace: 'nowrap' }}>{a}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {assets.map((row, ri) => (
              <tr key={row} style={{ backgroundColor: ri % 2 === 0 ? FINCEPT.PANEL_BG : 'transparent' }}>
                <td style={{ padding: '4px 8px', color, fontWeight: 700, borderRight: `1px solid ${FINCEPT.BORDER}`, whiteSpace: 'nowrap' }}>{row}</td>
                {assets.map(col => {
                  const v = get(row, col);
                  return (
                    <td key={col} style={{ padding: '5px 10px', textAlign: 'right', backgroundColor: cellBg(v), color: FINCEPT.WHITE, fontWeight: row === col ? 800 : 500 }}>
                      {fmt(v)}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

// ─── MOMENTS TABLE (asset × metric) ──────────────────────────────────────────
const MomentsTable: React.FC<{ title: string; flatData: Record<string, number>; color: string }> = ({ title, flatData, color }) => {
  // keys like "AAPL Mean", "AAPL Volatility"
  const metrics = Array.from(new Set(Object.keys(flatData).map(k => k.split(' ').slice(1).join(' '))));
  const assets  = Array.from(new Set(Object.keys(flatData).map(k => k.split(' ')[0])));
  if (assets.length === 0 || metrics.length === 0) return null;

  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{ color, fontSize: '10px', fontWeight: 700, letterSpacing: '0.8px', marginBottom: '8px', paddingBottom: '4px', borderBottom: `1px solid ${color}` }}>
        {title}
      </div>
      <div style={{ overflowX: 'auto' }}>
        <table style={{ borderCollapse: 'collapse', fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, width: '100%' }}>
          <thead>
            <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <th style={{ padding: '4px 8px', color: FINCEPT.MUTED, textAlign: 'left' }}>ASSET</th>
              {metrics.map(m => (
                <th key={m} style={{ padding: '4px 12px', color, textAlign: 'right', fontWeight: 700, whiteSpace: 'nowrap' }}>{m.toUpperCase()}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {assets.map((asset, i) => (
              <tr key={asset} style={{ backgroundColor: i % 2 === 0 ? FINCEPT.PANEL_BG : 'transparent' }}>
                <td style={{ padding: '5px 8px', color, fontWeight: 700, borderRight: `1px solid ${FINCEPT.BORDER}` }}>{asset}</td>
                {metrics.map(m => {
                  const v = flatData[`${asset} ${m}`] ?? 0;
                  return (
                    <td key={m} style={{ padding: '5px 12px', textAlign: 'right', color: v < 0 ? FINCEPT.RED : FINCEPT.WHITE, fontWeight: 500 }}>
                      {fmt(v)}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

// ─── WEIGHTS BAR CHART ────────────────────────────────────────────────────────
const WeightsChart: React.FC<{ title: string; weights: Record<string, number>; color: string }> = ({ title, weights, color }) => {
  const entries = Object.entries(weights).sort((a, b) => b[1] - a[1]);
  const max = Math.max(...entries.map(([, v]) => v), 0.001);
  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{ color, fontSize: '10px', fontWeight: 700, letterSpacing: '0.8px', marginBottom: '8px', paddingBottom: '4px', borderBottom: `1px solid ${color}` }}>
        {title}
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px', fontFamily: TYPOGRAPHY.MONO }}>
        <tbody>
          {entries.map(([ticker, w]) => (
            <tr key={ticker}>
              <td style={{ padding: '3px 8px', color: FINCEPT.GRAY, width: '80px', fontWeight: 600 }}>{ticker}</td>
              <td style={{ padding: '3px 8px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <div style={{ flex: 1, height: '12px', backgroundColor: FINCEPT.PANEL_BG, borderRadius: '1px', overflow: 'hidden' }}>
                    <div style={{ width: `${(w / max) * 100}%`, height: '100%', backgroundColor: color, opacity: 0.8 }} />
                  </div>
                  <span style={{ color, fontWeight: 700, minWidth: '48px', textAlign: 'right' }}>{(w * 100).toFixed(1)}%</span>
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};

// ─── FRONTIER TABLE ───────────────────────────────────────────────────────────
const FrontierTable: React.FC<{ title: string; frontier: any[]; kind: 'mv' | 'cvar'; color: string }> = ({ title, frontier, kind, color }) => {
  if (!frontier || frontier.length === 0) return null;
  const retKey   = 'expected_return';
  const riskKey  = kind === 'mv' ? 'volatility' : 'cvar';
  const riskLabel = kind === 'mv' ? 'VOLATILITY' : 'CVaR (95%)';
  const sharpeKey = kind === 'mv' ? 'sharpe_ratio' : null;

  const minRisk = Math.min(...frontier.map(p => p[riskKey] ?? 0));
  const maxRisk = Math.max(...frontier.map(p => p[riskKey] ?? 0), 0.001);

  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{ color, fontSize: '10px', fontWeight: 700, letterSpacing: '0.8px', marginBottom: '8px', paddingBottom: '4px', borderBottom: `1px solid ${color}` }}>
        {title}
      </div>
      <div style={{ overflowX: 'auto' }}>
        <table style={{ borderCollapse: 'collapse', fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, width: '100%' }}>
          <thead>
            <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <th style={{ padding: '4px 8px', color: FINCEPT.MUTED, textAlign: 'left' }}>#</th>
              <th style={{ padding: '4px 12px', color, textAlign: 'right' }}>EXP RETURN</th>
              <th style={{ padding: '4px 12px', color, textAlign: 'right' }}>{riskLabel}</th>
              {sharpeKey && <th style={{ padding: '4px 12px', color, textAlign: 'right' }}>SHARPE</th>}
              <th style={{ padding: '4px 12px', color, textAlign: 'left' }}>RISK BAR</th>
            </tr>
          </thead>
          <tbody>
            {frontier.map((pt, i) => {
              const ret  = pt[retKey] ?? 0;
              const risk = pt[riskKey] ?? 0;
              const barPct = ((risk - minRisk) / (maxRisk - minRisk + 0.0001)) * 100;
              return (
                <tr key={i} style={{ backgroundColor: i % 2 === 0 ? FINCEPT.PANEL_BG : 'transparent' }}>
                  <td style={{ padding: '4px 8px', color: FINCEPT.MUTED }}>{i + 1}</td>
                  <td style={{ padding: '4px 12px', textAlign: 'right', color: ret >= 0 ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: 600 }}>{fmt(ret)}</td>
                  <td style={{ padding: '4px 12px', textAlign: 'right', color: FINCEPT.WHITE }}>{fmt(risk)}</td>
                  {sharpeKey && <td style={{ padding: '4px 12px', textAlign: 'right', color: FINCEPT.CYAN }}>{fmt(pt[sharpeKey])}</td>}
                  <td style={{ padding: '4px 12px', minWidth: '80px' }}>
                    <div style={{ height: '8px', backgroundColor: FINCEPT.PANEL_BG, borderRadius: '1px', overflow: 'hidden' }}>
                      <div style={{ width: `${barPct}%`, height: '100%', backgroundColor: color, opacity: 0.75 }} />
                    </div>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
};

// ─── NESTED SECTION DISPATCHER ────────────────────────────────────────────────
// Detects data shape and picks the right renderer
const SmartSection: React.FC<{ title: string; data: any; color: string }> = ({ title, data, color }) => {
  if (!data || typeof data !== 'object') return null;

  // Frontier data: has "frontier" array
  if (Array.isArray(data.frontier)) {
    const isCvar = 'cvar' in (data.frontier[0] ?? {});
    return (
      <div>
        <FrontierTable title={title} frontier={data.frontier} kind={isCvar ? 'cvar' : 'mv'} color={color} />
        {data.min_variance_portfolio && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
            <WeightsChart title="MIN VARIANCE WEIGHTS" weights={data.min_variance_portfolio.weights ?? {}} color={FINCEPT.CYAN} />
            <KVTable title="MIN VARIANCE METRICS" data={{
              expected_return: data.min_variance_portfolio.expected_return,
              volatility: data.min_variance_portfolio.volatility,
              sharpe_ratio: data.min_variance_portfolio.sharpe_ratio,
            }} color={FINCEPT.CYAN} />
          </div>
        )}
        {data.min_cvar_portfolio && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
            <WeightsChart title="MIN CVaR WEIGHTS" weights={data.min_cvar_portfolio.weights ?? {}} color={FINCEPT.YELLOW} />
            <KVTable title="MIN CVaR METRICS" data={{
              expected_return: data.min_cvar_portfolio.expected_return,
              cvar: data.min_cvar_portfolio.cvar,
              var: data.min_cvar_portfolio.var,
              alpha: data.min_cvar_portfolio.alpha,
            }} color={FINCEPT.YELLOW} />
          </div>
        )}
      </div>
    );
  }

  // Optimal portfolio: has "weights" dict + scalar metrics
  if (data.weights && typeof data.weights === 'object' && !Array.isArray(data.weights)) {
    const metrics: Record<string, any> = {};
    for (const [k, v] of Object.entries(data)) {
      if (k !== 'weights' && typeof v !== 'object') metrics[k] = v;
    }
    return (
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
        <WeightsChart title={`${title} — WEIGHTS`} weights={data.weights as Record<string,number>} color={color} />
        <KVTable title={`${title} — METRICS`} data={metrics} color={color} />
      </div>
    );
  }

  // Covariance analysis: has covariance_matrix, correlation_matrix, moments sub-keys
  if (data.covariance_matrix || data.correlation_matrix || data.moments) {
    return (
      <div>
        {data.covariance_matrix && (
          <MatrixTable title="COVARIANCE MATRIX" flatData={data.covariance_matrix} color={FINCEPT.CYAN} />
        )}
        {data.correlation_matrix && (
          <MatrixTable title="CORRELATION MATRIX" flatData={data.correlation_matrix} color={FINCEPT.GREEN} />
        )}
        {data.moments && (
          <MomentsTable title="SIMULATION MOMENTS" flatData={data.moments} color={FINCEPT.YELLOW} />
        )}
        {/* scalar top-level fields */}
        {(() => {
          const scalars: Record<string, any> = {};
          for (const [k, v] of Object.entries(data)) {
            if (!['covariance_matrix','correlation_matrix','moments','assets'].includes(k) && typeof v !== 'object')
              scalars[k] = v;
          }
          return Object.keys(scalars).length > 0
            ? <KVTable title="SUMMARY" data={scalars} color={FINCEPT.MUTED} />
            : null;
        })()}
      </div>
    );
  }

  // Full analysis: has analysis sub-object
  if (data.analysis && typeof data.analysis === 'object') {
    return <SmartSection title={title} data={data.analysis} color={color} />;
  }

  // Exp decay: has probabilities array + scalar fields
  if (data.probabilities !== undefined) {
    const scalars: Record<string, any> = {};
    for (const [k, v] of Object.entries(data)) {
      if (k !== 'probabilities' && typeof v !== 'object') scalars[k] = v;
    }
    return <KVTable title={title} data={scalars} color={color} />;
  }

  // Entropy pooling
  if (data.prior_probabilities !== undefined || data.posterior_probabilities !== undefined) {
    const scalars: Record<string, any> = {};
    for (const [k, v] of Object.entries(data)) {
      if (!['prior_probabilities','posterior_probabilities'].includes(k) && typeof v !== 'object')
        scalars[k] = v;
    }
    return <KVTable title={title} data={scalars} color={color} />;
  }

  // Exposure stacking
  if (Array.isArray(data.stacked_weights)) {
    const assets = (data.stacked_weights as number[]).map((_: number, i: number) => `Asset ${i + 1}`);
    const weights: Record<string, number> = {};
    (data.stacked_weights as number[]).forEach((w: number, i: number) => { weights[`Asset ${i + 1}`] = w; });
    const scalars: Record<string, any> = {};
    for (const [k, v] of Object.entries(data)) {
      if (!['stacked_weights','mean_sample_weights','std_sample_weights'].includes(k) && typeof v !== 'object')
        scalars[k] = v;
    }
    return (
      <div>
        <WeightsChart title={`${title} — STACKED WEIGHTS`} weights={weights} color={color} />
        {data.mean_sample_weights && (
          <WeightsChart title="MEAN SAMPLE WEIGHTS" weights={Object.fromEntries(assets.map((a: string, i: number) => [a, (data.mean_sample_weights as number[])[i]]))} color={FINCEPT.MUTED} />
        )}
        <KVTable title="STACKING SUMMARY" data={scalars} color={color} />
      </div>
    );
  }

  // Fallback: flat key-value table
  const scalars: Record<string, any> = {};
  for (const [k, v] of Object.entries(data)) {
    if (typeof v !== 'object' && !Array.isArray(v)) scalars[k] = v;
  }
  return <KVTable title={title} data={scalars} color={color} />;
};

// ─── MAIN COMPONENT ───────────────────────────────────────────────────────────
const RiskManagementPanel: React.FC<RiskManagementPanelProps> = ({ portfolioSummary, currency }) => {
  const holdings = portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
  const [subTab, setSubTab] = useState<SubTab>('overview');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const [comprehensiveRisk, setComprehensiveRisk] = useState<any>(null);
  const [riskMetrics, setRiskMetrics]             = useState<any>(null);
  const [fullAnalysis, setFullAnalysis]           = useState<any>(null);
  const [expDecay, setExpDecay]                   = useState<any>(null);
  const [covAnalysis, setCovAnalysis]             = useState<any>(null);
  const [entropyPooling, setEntropyPooling]       = useState<any>(null);
  const [exposureStacking, setExposureStacking]   = useState<any>(null);
  const [mvFrontier, setMvFrontier]               = useState<any>(null);
  const [cvarFrontier, setCvarFrontier]           = useState<any>(null);
  const [mvOpt, setMvOpt]                         = useState<any>(null);
  const [cvarOpt, setCvarOpt]                     = useState<any>(null);

  const buildReturnsJson  = useCallback(() => JSON.stringify(holdings.map(h => h.symbol).join(',')),  [holdings]);
  const buildWeightsJson  = useCallback(() => JSON.stringify(holdings.map(h => h.weight / 100)),      [holdings]);
  const portfolioValue    = portfolioSummary.total_market_value || 0;

  // ── Cache key: unique per portfolio composition ──────────────────────────────
  const cachePrefix = useMemo(() => {
    const symbols = holdings.map(h => h.symbol).sort().join(',');
    const weights = holdings.map(h => Math.round(h.weight * 10)).join(',');
    return `risk-mgmt:${symbols}:${weights}`;
  }, [holdings]);

  // ── Restore cached results on mount (or when portfolio changes) ───────────────
  useEffect(() => {
    let cancelled = false;
    const restore = async () => {
      const keys: [string, (v: any) => void][] = [
        [`${cachePrefix}:comprehensive_risk`,  setComprehensiveRisk],
        [`${cachePrefix}:risk_metrics`,        setRiskMetrics],
        [`${cachePrefix}:full_analysis`,       setFullAnalysis],
        [`${cachePrefix}:exp_decay`,           setExpDecay],
        [`${cachePrefix}:cov_analysis`,        setCovAnalysis],
        [`${cachePrefix}:entropy_pooling`,     setEntropyPooling],
        [`${cachePrefix}:exposure_stacking`,   setExposureStacking],
        [`${cachePrefix}:mv_frontier`,         setMvFrontier],
        [`${cachePrefix}:cvar_frontier`,       setCvarFrontier],
        [`${cachePrefix}:mv_opt`,              setMvOpt],
        [`${cachePrefix}:cvar_opt`,            setCvarOpt],
      ];
      await Promise.all(keys.map(async ([cKey, setter]) => {
        const cached = await cacheService.get<any>(cKey);
        if (!cancelled && cached) setter(cached.data);
      }));
    };
    restore();
    return () => { cancelled = true; };
  }, [cachePrefix]);

  const fetchOverview = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const returnsData = buildReturnsJson();
      const weights     = buildWeightsJson();
      const results = await Promise.allSettled([
        invoke<string>('comprehensive_risk_analysis', { returnsData, weights, portfolioValue }),
        invoke<string>('calculate_risk_metrics',      { returnsData, weights }),
        invoke<string>('fortitudo_full_analysis',     { returnsJson: returnsData, weightsJson: weights, alpha: 0.05 }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setComprehensiveRisk(d);
        cacheService.set(`${cachePrefix}:comprehensive_risk`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setRiskMetrics(d);
        cacheService.set(`${cachePrefix}:risk_metrics`, d, 'api-response', '1h');
      }
      if (results[2].status === 'fulfilled') {
        const d = safeParse(results[2].value);
        setFullAnalysis(d);
        cacheService.set(`${cachePrefix}:full_analysis`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [buildReturnsJson, buildWeightsJson, portfolioValue, cachePrefix]);

  const fetchStress = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const returnsJson = buildReturnsJson();
      const results = await Promise.allSettled([
        invoke<string>('fortitudo_exp_decay_weighting',  { returnsJson, halfLife: 252 }),
        invoke<string>('fortitudo_covariance_analysis',  { returnsJson }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setExpDecay(d);
        cacheService.set(`${cachePrefix}:exp_decay`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setCovAnalysis(d);
        cacheService.set(`${cachePrefix}:cov_analysis`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [buildReturnsJson, cachePrefix]);

  const fetchFortitudo = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const results = await Promise.allSettled([
        invoke<string>('fortitudo_entropy_pooling',    { nScenarios: 1000 }),
        invoke<string>('fortitudo_exposure_stacking',  { samplePortfoliosJson: buildWeightsJson() }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setEntropyPooling(d);
        cacheService.set(`${cachePrefix}:entropy_pooling`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setExposureStacking(d);
        cacheService.set(`${cachePrefix}:exposure_stacking`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [buildWeightsJson, cachePrefix]);

  const fetchFrontiers = useCallback(async () => {
    setLoading(true); setError(null);
    try {
      const returnsJson = buildReturnsJson();
      const results = await Promise.allSettled([
        invoke<string>('fortitudo_efficient_frontier_mv',   { returnsJson, nPoints: 20 }),
        invoke<string>('fortitudo_efficient_frontier_cvar', { returnsJson, nPoints: 20, alpha: 0.05 }),
        invoke<string>('fortitudo_optimize_mean_variance',  { returnsJson, objective: 'min_variance' }),
        invoke<string>('fortitudo_optimize_mean_cvar',      { returnsJson, objective: 'min_cvar', alpha: 0.05 }),
      ]);
      if (results[0].status === 'fulfilled') {
        const d = safeParse(results[0].value);
        setMvFrontier(d);
        cacheService.set(`${cachePrefix}:mv_frontier`, d, 'api-response', '1h');
      }
      if (results[1].status === 'fulfilled') {
        const d = safeParse(results[1].value);
        setCvarFrontier(d);
        cacheService.set(`${cachePrefix}:cvar_frontier`, d, 'api-response', '1h');
      }
      if (results[2].status === 'fulfilled') {
        const d = safeParse(results[2].value);
        setMvOpt(d);
        cacheService.set(`${cachePrefix}:mv_opt`, d, 'api-response', '1h');
      }
      if (results[3].status === 'fulfilled') {
        const d = safeParse(results[3].value);
        setCvarOpt(d);
        cacheService.set(`${cachePrefix}:cvar_opt`, d, 'api-response', '1h');
      }
    } catch (e: any) { setError(e?.message || String(e)); }
    finally { setLoading(false); }
  }, [buildReturnsJson, cachePrefix]);

  const handleRun = () => {
    if (subTab === 'overview')   fetchOverview();
    else if (subTab === 'stresstest') fetchStress();
    else if (subTab === 'fortitudo')  fetchFortitudo();
    else if (subTab === 'frontiers')  fetchFrontiers();
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG }}>

      {/* ── Header / tab bar ── */}
      <div style={{ padding: '10px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.PANEL_BG, display: 'flex', alignItems: 'center', gap: '10px', flexShrink: 0, flexWrap: 'wrap' }}>
        <Shield size={16} color={FINCEPT.RED} />
        <span style={{ color: FINCEPT.RED, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: 'monospace' }}>RISK MANAGEMENT</span>
        <div style={{ flex: 1 }} />
        {SUB_TABS.map(t => {
          const Icon = t.icon;
          const active = subTab === t.id;
          return (
            <button key={t.id} onClick={() => setSubTab(t.id)} style={{
              padding: '4px 10px', backgroundColor: active ? `${FINCEPT.RED}30` : 'transparent',
              border: `1px solid ${active ? FINCEPT.RED : FINCEPT.BORDER}`, color: active ? FINCEPT.RED : FINCEPT.GRAY,
              fontSize: '9px', fontWeight: 700, cursor: 'pointer', fontFamily: 'monospace',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <Icon size={10} /> {t.label}
            </button>
          );
        })}
        <button onClick={handleRun} disabled={loading} style={{
          padding: '6px 14px', backgroundColor: loading ? FINCEPT.MUTED : FINCEPT.RED,
          border: 'none', color: FINCEPT.WHITE, fontSize: '10px', fontWeight: 700,
          cursor: loading ? 'not-allowed' : 'pointer', fontFamily: 'monospace',
          display: 'flex', alignItems: 'center', gap: '4px',
        }}>
          {loading ? <RefreshCw size={10} className="animate-spin" /> : <Zap size={10} />}
          {loading ? 'RUNNING...' : 'RUN'}
        </button>
      </div>

      {/* ── Error bar ── */}
      {error && (
        <div style={{ padding: '8px 16px', backgroundColor: `${FINCEPT.RED}15`, borderBottom: `1px solid ${FINCEPT.RED}`, display: 'flex', alignItems: 'center', gap: '8px', flexShrink: 0 }}>
          <AlertTriangle size={12} color={FINCEPT.RED} />
          <span style={{ color: FINCEPT.RED, fontSize: '10px', fontFamily: 'monospace' }}>{error}</span>
        </div>
      )}

      {/* ── Content ── */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>

        {subTab === 'overview' && (
          <div>
            {!comprehensiveRisk && !riskMetrics && !fullAnalysis && !loading && (
              <EmptyState text="Click RUN to compute comprehensive risk analysis, risk metrics, and Fortitudo full analysis" />
            )}
            {comprehensiveRisk && <SmartSection title="COMPREHENSIVE RISK ANALYSIS" data={comprehensiveRisk} color={FINCEPT.RED} />}
            {riskMetrics       && <SmartSection title="RISK METRICS"                data={riskMetrics}       color={FINCEPT.ORANGE} />}
            {fullAnalysis      && <SmartSection title="FORTITUDO FULL ANALYSIS"     data={fullAnalysis}      color={FINCEPT.CYAN} />}
          </div>
        )}

        {subTab === 'stresstest' && (
          <div>
            {!expDecay && !covAnalysis && !loading && (
              <EmptyState text="Click RUN to compute exponential decay weighting and covariance analysis" />
            )}
            {expDecay    && <SmartSection title="EXPONENTIAL DECAY WEIGHTING (Half-Life: 252)" data={expDecay}    color={FINCEPT.YELLOW} />}
            {covAnalysis && <SmartSection title="COVARIANCE / CORRELATION MATRIX"              data={covAnalysis} color={FINCEPT.CYAN} />}
          </div>
        )}

        {subTab === 'fortitudo' && (
          <div>
            {!entropyPooling && !exposureStacking && !loading && (
              <EmptyState text="Click RUN to compute entropy pooling and exposure stacking" />
            )}
            {entropyPooling   && <SmartSection title="ENTROPY POOLING (1000 scenarios)" data={entropyPooling}   color={FINCEPT.GREEN} />}
            {exposureStacking && <SmartSection title="EXPOSURE STACKING"                data={exposureStacking} color={FINCEPT.ORANGE} />}
          </div>
        )}

        {subTab === 'frontiers' && (
          <div>
            {!mvFrontier && !cvarFrontier && !mvOpt && !cvarOpt && !loading && (
              <EmptyState text="Click RUN to compute Mean-Variance and Mean-CVaR efficient frontiers + optimized portfolios" />
            )}
            {mvFrontier   && <SmartSection title="MV EFFICIENT FRONTIER (20 points)"   data={mvFrontier}   color={FINCEPT.GREEN} />}
            {cvarFrontier && <SmartSection title="CVaR EFFICIENT FRONTIER (20 points)" data={cvarFrontier} color={FINCEPT.RED} />}
            {(mvOpt || cvarOpt) && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
                {mvOpt   && <SmartSection title="MIN VARIANCE PORTFOLIO" data={mvOpt}   color={FINCEPT.CYAN} />}
                {cvarOpt && <SmartSection title="MIN CVaR PORTFOLIO"     data={cvarOpt} color={FINCEPT.YELLOW} />}
              </div>
            )}
          </div>
        )}

      </div>
    </div>
  );
};

const EmptyState: React.FC<{ text: string }> = ({ text }) => (
  <div style={{ textAlign: 'center', padding: '60px 0' }}>
    <Shield size={36} color={FINCEPT.MUTED} style={{ opacity: 0.3, marginBottom: '12px' }} />
    <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '6px' }}>RISK MANAGEMENT ENGINE</div>
    <div style={{ color: FINCEPT.MUTED, fontSize: '10px', maxWidth: '500px', margin: '0 auto' }}>{text}</div>
  </div>
);

export default RiskManagementPanel;
