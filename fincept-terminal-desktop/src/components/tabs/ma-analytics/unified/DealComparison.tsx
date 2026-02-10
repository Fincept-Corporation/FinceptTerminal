/**
 * Deal Comparison Panel - Compare, Rank, and Benchmark M&A Deals
 *
 * Tools: Side-by-side comparison, deal ranking, premium benchmarking, payment structure analysis, industry analysis
 */

import React, { useState } from 'react';
import { GitCompare, PlayCircle, Loader2, BarChart3, Award, CreditCard, Building2, Plus, Trash2, RefreshCw } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, MADeal } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

type AnalysisType = 'compare' | 'rank' | 'benchmark' | 'payment' | 'industry';

type RankCriteria = 'premium' | 'deal_value' | 'ev_revenue' | 'ev_ebitda' | 'synergies';

interface ComparisonDeal {
  deal_id: string;
  acquirer_name: string;
  target_name: string;
  deal_value: number;
  announced_date: string;
  status: string;
  premium_1day: number;
  ev_revenue?: number;
  ev_ebitda?: number;
  synergies?: number;
  payment_cash_pct: number;
  payment_stock_pct: number;
  industry: string;
}

const DEFAULT_DEALS: ComparisonDeal[] = [
  {
    deal_id: '1', acquirer_name: 'TechCorp Inc', target_name: 'StartupA', deal_value: 2500,
    announced_date: '2024-01-15', status: 'completed', premium_1day: 35, ev_revenue: 8.5,
    ev_ebitda: 18.2, synergies: 150, payment_cash_pct: 100, payment_stock_pct: 0, industry: 'Technology',
  },
  {
    deal_id: '2', acquirer_name: 'MegaCorp', target_name: 'GrowthCo', deal_value: 4200,
    announced_date: '2024-02-20', status: 'completed', premium_1day: 28, ev_revenue: 6.2,
    ev_ebitda: 14.5, synergies: 280, payment_cash_pct: 50, payment_stock_pct: 50, industry: 'Technology',
  },
  {
    deal_id: '3', acquirer_name: 'GlobalTech', target_name: 'InnovateCo', deal_value: 1800,
    announced_date: '2024-03-10', status: 'pending', premium_1day: 42, ev_revenue: 10.1,
    ev_ebitda: 22.0, synergies: 95, payment_cash_pct: 0, payment_stock_pct: 100, industry: 'Healthcare',
  },
];

const RANK_CRITERIA_OPTIONS: { value: RankCriteria; label: string; desc: string }[] = [
  { value: 'premium', label: 'Premium Paid', desc: 'Rank by 1-day premium to target shareholders' },
  { value: 'deal_value', label: 'Deal Value', desc: 'Rank by total transaction value' },
  { value: 'ev_revenue', label: 'EV/Revenue', desc: 'Rank by enterprise value to revenue multiple' },
  { value: 'ev_ebitda', label: 'EV/EBITDA', desc: 'Rank by enterprise value to EBITDA multiple' },
  { value: 'synergies', label: 'Synergies', desc: 'Rank by estimated synergy value' },
];

export const DealComparison: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('compare');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [deals, setDeals] = useState<ComparisonDeal[]>(DEFAULT_DEALS);
  const [rankCriteria, setRankCriteria] = useState<RankCriteria>('premium');
  const [targetDealIdx, setTargetDealIdx] = useState(0);
  const [dbDeals, setDbDeals] = useState<MADeal[]>([]);
  const [loadingDb, setLoadingDb] = useState(false);

  const loadFromDatabase = async () => {
    setLoadingDb(true);
    try {
      const data = await MAAnalyticsService.DealDatabase.getAllDeals();
      if (data && data.length > 0) {
        setDbDeals(data);
        showSuccess(`Loaded ${data.length} deals from database`);
      } else {
        showError('No deals found in database. Add deals manually or scan SEC filings.');
      }
    } catch (error) {
      showError(`Failed to load deals: ${error}`);
    } finally {
      setLoadingDb(false);
    }
  };

  const runAnalysis = async () => {
    if (deals.length < 2) { showError('Add at least 2 deals to compare'); return; }
    setLoading(true);
    setResult(null);
    try {
      let data;
      const dealData = deals as MADeal[];
      switch (analysisType) {
        case 'compare': data = await MAAnalyticsService.DealComparison.compareDeals(dealData); break;
        case 'rank': data = await MAAnalyticsService.DealComparison.rankDeals(dealData, rankCriteria); break;
        case 'benchmark': {
          const targetDeal = dealData[targetDealIdx];
          const comparables = dealData.filter((_, i) => i !== targetDealIdx);
          data = await MAAnalyticsService.DealComparison.benchmarkDealPremium(targetDeal, comparables);
          break;
        }
        case 'payment': data = await MAAnalyticsService.DealComparison.analyzePaymentStructures(dealData); break;
        case 'industry': data = await MAAnalyticsService.DealComparison.analyzeIndustryDeals(dealData); break;
      }
      setResult(data);
      showSuccess('Analysis completed');
    } catch (error) {
      showError(`Analysis failed: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const addDeal = () => {
    setDeals([...deals, {
      deal_id: String(Date.now()), acquirer_name: '', target_name: '', deal_value: 0,
      announced_date: new Date().toISOString().split('T')[0], status: 'pending', premium_1day: 0,
      ev_revenue: 0, ev_ebitda: 0, synergies: 0, payment_cash_pct: 100, payment_stock_pct: 0, industry: 'Technology',
    }]);
  };

  const removeDeal = (idx: number) => {
    if (deals.length <= 2) return;
    setDeals(deals.filter((_, i) => i !== idx));
    if (targetDealIdx >= deals.length - 1) setTargetDealIdx(Math.max(0, deals.length - 2));
  };

  const updateDeal = (idx: number, field: keyof ComparisonDeal, value: any) => {
    const updated = [...deals];
    updated[idx] = { ...updated[idx], [field]: value };
    setDeals(updated);
  };

  const importFromDb = (dbDeal: MADeal) => {
    const converted: ComparisonDeal = {
      deal_id: dbDeal.deal_id, acquirer_name: dbDeal.acquirer_name, target_name: dbDeal.target_name,
      deal_value: dbDeal.deal_value, announced_date: dbDeal.announced_date, status: dbDeal.status,
      premium_1day: dbDeal.premium_1day, ev_revenue: dbDeal.ev_revenue, ev_ebitda: dbDeal.ev_ebitda,
      synergies: dbDeal.synergies, payment_cash_pct: dbDeal.payment_cash_pct,
      payment_stock_pct: dbDeal.payment_stock_pct, industry: dbDeal.industry,
    };
    setDeals([...deals, converted]);
  };

  const smallInputStyle: React.CSSProperties = { ...COMMON_STYLES.inputField, padding: '4px 8px', fontSize: TYPOGRAPHY.TINY };

  const renderDealInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <span style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE }}>DEALS ({deals.length})</span>
        <div style={{ display: 'flex', gap: SPACING.SMALL }}>
          <button onClick={loadFromDatabase} disabled={loadingDb} style={{ ...COMMON_STYLES.buttonSecondary, display: 'flex', alignItems: 'center', gap: SPACING.TINY, padding: '4px 8px', color: FINCEPT.ORANGE }}>
            {loadingDb ? <Loader2 size={10} className="animate-spin" /> : <RefreshCw size={10} />}
            LOAD FROM DB
          </button>
          <button onClick={addDeal} style={{ ...COMMON_STYLES.buttonSecondary, display: 'flex', alignItems: 'center', gap: SPACING.TINY, padding: '4px 8px', color: FINCEPT.ORANGE }}>
            <Plus size={10} /> ADD DEAL
          </button>
        </div>
      </div>

      {/* Database deals quick import */}
      {dbDeals.length > 0 && (
        <div style={{ padding: SPACING.SMALL, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: SPACING.SMALL }}>QUICK IMPORT FROM DATABASE:</div>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.TINY }}>
            {dbDeals.slice(0, 5).map(d => (
              <button key={d.deal_id} onClick={() => importFromDb(d)} style={{ ...COMMON_STYLES.buttonSecondary, padding: '3px 8px', color: FINCEPT.ORANGE }}>
                {d.target_name}
              </button>
            ))}
            {dbDeals.length > 5 && (
              <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, padding: '3px 8px' }}>+{dbDeals.length - 5} more</span>
            )}
          </div>
        </div>
      )}

      {/* Deal list */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, maxHeight: 400, overflowY: 'auto' }}>
        {deals.map((deal, idx) => (
          <div key={deal.deal_id} style={{ padding: SPACING.SMALL, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: SPACING.SMALL }}>
              <span style={{ fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>DEAL #{idx + 1}</span>
              <button onClick={() => removeDeal(idx)} disabled={deals.length <= 2} style={{ background: 'none', border: 'none', cursor: deals.length <= 2 ? 'not-allowed' : 'pointer', padding: '2px' }}>
                <Trash2 size={12} color={deals.length <= 2 ? FINCEPT.MUTED : FINCEPT.RED} />
              </button>
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.TINY }}>
              <input type="text" value={deal.acquirer_name || ''} onChange={e => updateDeal(idx, 'acquirer_name', e.target.value)} placeholder="Acquirer" style={smallInputStyle} />
              <input type="text" value={deal.target_name || ''} onChange={e => updateDeal(idx, 'target_name', e.target.value)} placeholder="Target" style={smallInputStyle} />
              <input type="number" value={deal.deal_value || 0} onChange={e => updateDeal(idx, 'deal_value', parseFloat(e.target.value) || 0)} placeholder="Value ($M)" style={smallInputStyle} />
              <input type="number" value={deal.premium_1day || 0} onChange={e => updateDeal(idx, 'premium_1day', parseFloat(e.target.value) || 0)} placeholder="Premium %" style={smallInputStyle} />
              <input type="number" value={deal.ev_revenue || 0} onChange={e => updateDeal(idx, 'ev_revenue', parseFloat(e.target.value) || 0)} placeholder="EV/Rev" style={smallInputStyle} />
              <input type="number" value={deal.ev_ebitda || 0} onChange={e => updateDeal(idx, 'ev_ebitda', parseFloat(e.target.value) || 0)} placeholder="EV/EBITDA" style={smallInputStyle} />
              <input type="number" value={deal.payment_cash_pct || 0} onChange={e => { const cashPct = parseFloat(e.target.value) || 0; updateDeal(idx, 'payment_cash_pct', cashPct); updateDeal(idx, 'payment_stock_pct', 100 - cashPct); }} placeholder="Cash %" style={smallInputStyle} />
              <select value={deal.industry || 'Technology'} onChange={e => updateDeal(idx, 'industry', e.target.value)} style={smallInputStyle}>
                <option value="Technology">Technology</option>
                <option value="Healthcare">Healthcare</option>
                <option value="Financial Services">Financial Services</option>
                <option value="Consumer">Consumer</option>
                <option value="Industrial">Industrial</option>
                <option value="Energy">Energy</option>
              </select>
            </div>
          </div>
        ))}
      </div>

      {/* Analysis-specific options */}
      {analysisType === 'rank' && (
        <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>RANKING CRITERIA</div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.TINY }}>
            {RANK_CRITERIA_OPTIONS.map(opt => (
              <button
                key={opt.value}
                onClick={() => setRankCriteria(opt.value)}
                style={{
                  ...COMMON_STYLES.tabButton(rankCriteria === opt.value),
                  width: '100%',
                  textAlign: 'left',
                  padding: '6px 10px',
                }}
              >
                <div style={{ fontWeight: TYPOGRAPHY.BOLD }}>{opt.label}</div>
                <div style={{ fontSize: TYPOGRAPHY.TINY, opacity: 0.7, marginTop: SPACING.TINY }}>{opt.desc}</div>
              </button>
            ))}
          </div>
        </div>
      )}

      {analysisType === 'benchmark' && (
        <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>TARGET DEAL TO BENCHMARK</label>
          <select value={targetDealIdx} onChange={e => setTargetDealIdx(parseInt(e.target.value))} style={COMMON_STYLES.inputField}>
            {deals.map((d, i) => (
              <option key={i} value={i}>{d.target_name || `Deal #${i + 1}`}</option>
            ))}
          </select>
        </div>
      )}
    </div>
  );

  const renderCompareResults = () => {
    if (!result) return null;
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {result.comparison && (
          <div style={{ overflowX: 'auto' }}>
            <table style={{ width: '100%', fontSize: TYPOGRAPHY.TINY, borderCollapse: 'collapse' }}>
              <thead>
                <tr>
                  <th style={COMMON_STYLES.tableHeader}>METRIC</th>
                  {deals.map((d, i) => (
                    <th key={i} style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>{d.target_name || `Deal ${i + 1}`}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {Object.entries(result.comparison).map(([metric, values]: [string, any]) => (
                  <tr key={metric} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    <td style={{ padding: SPACING.SMALL, color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>{metric.replace(/_/g, ' ')}</td>
                    {(values as any[]).map((v, i) => (
                      <td key={i} style={{ padding: SPACING.SMALL, textAlign: 'right', color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD, fontSize: TYPOGRAPHY.TINY }}>
                        {typeof v === 'number' ? v.toFixed(2) : v}
                      </td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}

        {result.summary && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
            {Object.entries(result.summary).map(([key, value]) => (
              <div key={key} style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
                <div style={COMMON_STYLES.dataLabel}>{key.replace(/_/g, ' ').toUpperCase()}</div>
                <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                  {typeof value === 'number' ? value.toFixed(1) : String(value)}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    );
  };

  const renderRankResults = () => {
    if (!result || !result.rankings) return null;
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
          Ranked by: <span style={{ color: FINCEPT.ORANGE }}>{RANK_CRITERIA_OPTIONS.find(o => o.value === rankCriteria)?.label}</span>
        </div>
        {result.rankings.map((deal: any, idx: number) => (
          <div key={idx} style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              width: '28px', height: '28px', borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center',
              fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD,
              backgroundColor: idx === 0 ? '#FFD700' : idx === 1 ? '#C0C0C0' : idx === 2 ? '#CD7F32' : FINCEPT.PANEL_BG,
              color: idx < 3 ? '#000' : FINCEPT.WHITE,
            }}>
              {idx + 1}
            </div>
            <div style={{ flex: 1 }}>
              <div style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>{deal.target_name}</div>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Acquired by {deal.acquirer_name}</div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.ORANGE }}>
                {rankCriteria === 'premium' && `${deal.premium_1day?.toFixed(1)}%`}
                {rankCriteria === 'deal_value' && `$${deal.deal_value?.toFixed(0)}M`}
                {rankCriteria === 'ev_revenue' && `${deal.ev_revenue?.toFixed(1)}x`}
                {rankCriteria === 'ev_ebitda' && `${deal.ev_ebitda?.toFixed(1)}x`}
                {rankCriteria === 'synergies' && `$${deal.synergy_value?.toFixed(0)}M`}
              </div>
            </div>
          </div>
        ))}
      </div>
    );
  };

  const renderBenchmarkResults = () => {
    if (!result) return null;
    const targetDeal = deals[targetDealIdx];
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        <div style={{ padding: SPACING.DEFAULT, borderRadius: '2px', textAlign: 'center', backgroundColor: `${FINCEPT.ORANGE}20`, border: `1px solid ${FINCEPT.ORANGE}` }}>
          <div style={COMMON_STYLES.dataLabel}>BENCHMARKING</div>
          <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.ORANGE, marginTop: SPACING.TINY }}>
            {targetDeal?.target_name}
          </div>
        </div>

        {result.premium_comparison && (
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>PREMIUM VS COMPARABLES</div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.SMALL }}>
              {[
                { label: 'TARGET PREMIUM', value: `${result.premium_comparison.target?.toFixed(1)}%`, color: FINCEPT.WHITE },
                { label: 'MEDIAN PREMIUM', value: `${result.premium_comparison.median?.toFixed(1)}%`, color: FINCEPT.WHITE },
                { label: 'PERCENTILE', value: `${result.premium_comparison.percentile?.toFixed(0)}%`, color: FINCEPT.ORANGE },
              ].map(item => (
                <div key={item.label} style={{ textAlign: 'center', padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '2px' }}>
                  <div style={COMMON_STYLES.dataLabel}>{item.label}</div>
                  <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: item.color, marginTop: SPACING.TINY }}>{item.value}</div>
                </div>
              ))}
            </div>
          </div>
        )}

        {result.insight && (
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, border: `1px solid ${FINCEPT.BORDER}` }}>
            {result.insight}
          </div>
        )}
      </div>
    );
  };

  const renderPaymentResults = () => {
    if (!result) return null;
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {result.distribution && (
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>PAYMENT STRUCTURE DISTRIBUTION</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              {Object.entries(result.distribution).map(([type, count]: [string, any]) => (
                <div key={type} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{type.replace(/_/g, ' ')}</span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                    <div style={{ width: '96px', height: '6px', borderRadius: '2px', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
                      <div style={{ height: '100%', borderRadius: '2px', backgroundColor: FINCEPT.ORANGE, width: `${(count / deals.length) * 100}%` }} />
                    </div>
                    <span style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>{count}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {result.stats_by_type && (
          <div style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>METRICS BY PAYMENT TYPE</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              {Object.entries(result.stats_by_type).map(([type, stats]: [string, any]) => (
                <div key={type} style={{ padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '2px' }}>
                  <div style={{ fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginBottom: SPACING.SMALL }}>{type.replace(/_/g, ' ')}</div>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY }}>
                    <div><span style={{ color: FINCEPT.GRAY }}>Avg Premium: </span><span style={{ color: FINCEPT.WHITE }}>{stats.avg_premium?.toFixed(1)}%</span></div>
                    <div><span style={{ color: FINCEPT.GRAY }}>Avg Value: </span><span style={{ color: FINCEPT.WHITE }}>${stats.avg_value?.toFixed(0)}M</span></div>
                    <div><span style={{ color: FINCEPT.GRAY }}>Count: </span><span style={{ color: FINCEPT.WHITE }}>{stats.count}</span></div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    );
  };

  const renderIndustryResults = () => {
    if (!result) return null;
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {result.by_industry && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
            {Object.entries(result.by_industry).map(([industry, data]: [string, any]) => (
              <div key={industry} style={{ padding: SPACING.DEFAULT, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL, marginBottom: SPACING.SMALL }}>
                  <Building2 size={14} color={FINCEPT.ORANGE} />
                  <span style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>{industry}</span>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>({data.count} deals)</span>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: SPACING.SMALL }}>
                  {[
                    { label: 'AVG PREMIUM', value: `${data.avg_premium?.toFixed(1)}%` },
                    { label: 'AVG EV/REV', value: `${data.avg_ev_revenue?.toFixed(1)}x` },
                    { label: 'AVG EV/EBITDA', value: `${data.avg_ev_ebitda?.toFixed(1)}x` },
                    { label: 'TOTAL VALUE', value: `$${data.total_value?.toFixed(0)}M` },
                  ].map(item => (
                    <div key={item.label} style={{ textAlign: 'center', padding: SPACING.SMALL, backgroundColor: FINCEPT.PANEL_BG, borderRadius: '2px' }}>
                      <div style={COMMON_STYLES.dataLabel}>{item.label}</div>
                      <div style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>{item.value}</div>
                    </div>
                  ))}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    );
  };

  const renderResults = () => {
    if (loading) {
      return (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '200px' }}>
          <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
        </div>
      );
    }
    if (!result) {
      return (
        <div style={{ ...COMMON_STYLES.emptyState, height: '200px' }}>
          <GitCompare size={32} style={{ opacity: 0.3, marginBottom: SPACING.SMALL }} />
          <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>Add deals and run analysis</span>
        </div>
      );
    }
    switch (analysisType) {
      case 'compare': return renderCompareResults();
      case 'rank': return renderRankResults();
      case 'benchmark': return renderBenchmarkResults();
      case 'payment': return renderPaymentResults();
      case 'industry': return renderIndustryResults();
      default: return null;
    }
  };

  const ANALYSIS_TABS: { id: AnalysisType; label: string; icon: React.ElementType }[] = [
    { id: 'compare', label: 'COMPARE', icon: GitCompare },
    { id: 'rank', label: 'RANK', icon: Award },
    { id: 'benchmark', label: 'BENCHMARK', icon: BarChart3 },
    { id: 'payment', label: 'PAYMENT', icon: CreditCard },
    { id: 'industry', label: 'INDUSTRY', icon: Building2 },
  ];

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
          <GitCompare size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>DEAL COMPARISON</span>
          <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>COMPARE & BENCHMARK</span>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading || deals.length < 2}
          style={{ ...COMMON_STYLES.buttonPrimary, display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}
        >
          {loading ? <Loader2 size={10} className="animate-spin" /> : <PlayCircle size={10} />}
          ANALYZE
        </button>
      </div>

      {/* Analysis Type Tabs */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.TINY,
        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        overflowX: 'auto',
      }}>
        {ANALYSIS_TABS.map(tab => {
          const Icon = tab.icon;
          return (
            <button
              key={tab.id}
              onClick={() => { setAnalysisType(tab.id); setResult(null); }}
              style={{
                ...COMMON_STYLES.tabButton(analysisType === tab.id),
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.TINY,
                whiteSpace: 'nowrap',
              }}
            >
              <Icon size={10} /> {tab.label}
            </button>
          );
        })}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
          {/* Input Panel */}
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>DEAL DATA</div>
            {renderDealInputs()}
          </div>

          {/* Results Panel */}
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
              {analysisType === 'compare' && 'COMPARISON RESULTS'}
              {analysisType === 'rank' && 'RANKINGS'}
              {analysisType === 'benchmark' && 'BENCHMARK RESULTS'}
              {analysisType === 'payment' && 'PAYMENT ANALYSIS'}
              {analysisType === 'industry' && 'INDUSTRY ANALYSIS'}
            </div>
            {renderResults()}
          </div>
        </div>
      </div>
    </div>
  );
};

export default DealComparison;
