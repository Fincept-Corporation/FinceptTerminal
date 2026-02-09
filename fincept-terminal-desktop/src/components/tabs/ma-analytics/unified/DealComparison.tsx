/**
 * Deal Comparison Panel - Compare, Rank, and Benchmark M&A Deals
 *
 * Tools: Side-by-side comparison, deal ranking, premium benchmarking, payment structure analysis, industry analysis
 */

import React, { useState, useEffect } from 'react';
import { GitCompare, PlayCircle, Loader2, TrendingUp, BarChart3, Award, CreditCard, Building2, Plus, Trash2, RefreshCw } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, MADeal } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

type AnalysisType = 'compare' | 'rank' | 'benchmark' | 'payment' | 'industry';

type RankCriteria = 'premium' | 'deal_value' | 'ev_revenue' | 'ev_ebitda' | 'synergies';

// Extended deal type for local comparison (superset of MADeal)
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
    deal_id: '1',
    acquirer_name: 'TechCorp Inc',
    target_name: 'StartupA',
    deal_value: 2500,
    announced_date: '2024-01-15',
    status: 'completed',
    premium_1day: 35,
    ev_revenue: 8.5,
    ev_ebitda: 18.2,
    synergies: 150,
    payment_cash_pct: 100,
    payment_stock_pct: 0,
    industry: 'Technology',
  },
  {
    deal_id: '2',
    acquirer_name: 'MegaCorp',
    target_name: 'GrowthCo',
    deal_value: 4200,
    announced_date: '2024-02-20',
    status: 'completed',
    premium_1day: 28,
    ev_revenue: 6.2,
    ev_ebitda: 14.5,
    synergies: 280,
    payment_cash_pct: 50,
    payment_stock_pct: 50,
    industry: 'Technology',
  },
  {
    deal_id: '3',
    acquirer_name: 'GlobalTech',
    target_name: 'InnovateCo',
    deal_value: 1800,
    announced_date: '2024-03-10',
    status: 'pending',
    premium_1day: 42,
    ev_revenue: 10.1,
    ev_ebitda: 22.0,
    synergies: 95,
    payment_cash_pct: 0,
    payment_stock_pct: 100,
    industry: 'Healthcare',
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

  // Load deals from database
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
    if (deals.length < 2) {
      showError('Add at least 2 deals to compare');
      return;
    }

    setLoading(true);
    setResult(null);
    try {
      let data;
      const dealData = deals as MADeal[];

      switch (analysisType) {
        case 'compare':
          data = await MAAnalyticsService.DealComparison.compareDeals(dealData);
          break;
        case 'rank':
          data = await MAAnalyticsService.DealComparison.rankDeals(dealData, rankCriteria);
          break;
        case 'benchmark':
          const targetDeal = dealData[targetDealIdx];
          const comparables = dealData.filter((_, i) => i !== targetDealIdx);
          data = await MAAnalyticsService.DealComparison.benchmarkDealPremium(targetDeal, comparables);
          break;
        case 'payment':
          data = await MAAnalyticsService.DealComparison.analyzePaymentStructures(dealData);
          break;
        case 'industry':
          data = await MAAnalyticsService.DealComparison.analyzeIndustryDeals(dealData);
          break;
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
    setDeals([
      ...deals,
      {
        deal_id: String(Date.now()),
        acquirer_name: '',
        target_name: '',
        deal_value: 0,
        announced_date: new Date().toISOString().split('T')[0],
        status: 'pending',
        premium_1day: 0,
        ev_revenue: 0,
        ev_ebitda: 0,
        synergies: 0,
        payment_cash_pct: 100,
        payment_stock_pct: 0,
        industry: 'Technology',
      },
    ]);
  };

  const removeDeal = (idx: number) => {
    if (deals.length <= 2) return;
    setDeals(deals.filter((_, i) => i !== idx));
    if (targetDealIdx >= deals.length - 1) {
      setTargetDealIdx(Math.max(0, deals.length - 2));
    }
  };

  const updateDeal = (idx: number, field: keyof ComparisonDeal, value: any) => {
    const updated = [...deals];
    updated[idx] = { ...updated[idx], [field]: value };
    setDeals(updated);
  };

  const importFromDb = (dbDeal: MADeal) => {
    // Convert MADeal to ComparisonDeal
    const converted: ComparisonDeal = {
      deal_id: dbDeal.deal_id,
      acquirer_name: dbDeal.acquirer_name,
      target_name: dbDeal.target_name,
      deal_value: dbDeal.deal_value,
      announced_date: dbDeal.announced_date,
      status: dbDeal.status,
      premium_1day: dbDeal.premium_1day,
      ev_revenue: dbDeal.ev_revenue,
      ev_ebitda: dbDeal.ev_ebitda,
      synergies: dbDeal.synergies,
      payment_cash_pct: dbDeal.payment_cash_pct,
      payment_stock_pct: dbDeal.payment_stock_pct,
      industry: dbDeal.industry,
    };
    setDeals([...deals, converted]);
  };

  const renderDealInputs = () => (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <h4 className="text-sm font-medium text-white">Deals ({deals.length})</h4>
        <div className="flex gap-2">
          <button
            onClick={loadFromDatabase}
            disabled={loadingDb}
            className="flex items-center gap-1 px-2 py-1 rounded text-xs"
            style={{ backgroundColor: FINCEPT.CHARCOAL, color: FINCEPT.ORANGE }}
          >
            {loadingDb ? <Loader2 size={12} className="animate-spin" /> : <RefreshCw size={12} />}
            Load from DB
          </button>
          <button
            onClick={addDeal}
            className="flex items-center gap-1 px-2 py-1 rounded text-xs"
            style={{ backgroundColor: FINCEPT.ORANGE + '20', color: FINCEPT.ORANGE }}
          >
            <Plus size={12} /> Add Deal
          </button>
        </div>
      </div>

      {/* Database deals quick import */}
      {dbDeals.length > 0 && (
        <div className="p-2 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
          <div className="text-xs text-gray-400 mb-2">Quick Import from Database:</div>
          <div className="flex flex-wrap gap-1">
            {dbDeals.slice(0, 5).map(d => (
              <button
                key={d.deal_id}
                onClick={() => importFromDb(d)}
                className="px-2 py-1 rounded text-xs hover:opacity-80"
                style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.ORANGE }}
              >
                {d.target_name}
              </button>
            ))}
            {dbDeals.length > 5 && (
              <span className="text-xs text-gray-500 px-2 py-1">+{dbDeals.length - 5} more</span>
            )}
          </div>
        </div>
      )}

      {/* Deal list */}
      <div className="space-y-3 max-h-96 overflow-y-auto">
        {deals.map((deal, idx) => (
          <div key={deal.deal_id} className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <div className="flex items-center justify-between mb-2">
              <span className="text-xs font-medium text-white">Deal #{idx + 1}</span>
              <button
                onClick={() => removeDeal(idx)}
                disabled={deals.length <= 2}
                className="p-1 rounded hover:bg-red-500/20"
              >
                <Trash2 size={12} className={deals.length <= 2 ? 'text-gray-600' : 'text-red-400'} />
              </button>
            </div>
            <div className="grid grid-cols-2 gap-2">
              <input
                type="text"
                value={deal.acquirer_name || ''}
                onChange={e => updateDeal(idx, 'acquirer_name', e.target.value)}
                placeholder="Acquirer"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <input
                type="text"
                value={deal.target_name || ''}
                onChange={e => updateDeal(idx, 'target_name', e.target.value)}
                placeholder="Target"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <input
                type="number"
                value={deal.deal_value || 0}
                onChange={e => updateDeal(idx, 'deal_value', parseFloat(e.target.value) || 0)}
                placeholder="Value ($M)"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <input
                type="number"
                value={deal.premium_1day || 0}
                onChange={e => updateDeal(idx, 'premium_1day', parseFloat(e.target.value) || 0)}
                placeholder="Premium %"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <input
                type="number"
                value={deal.ev_revenue || 0}
                onChange={e => updateDeal(idx, 'ev_revenue', parseFloat(e.target.value) || 0)}
                placeholder="EV/Rev"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <input
                type="number"
                value={deal.ev_ebitda || 0}
                onChange={e => updateDeal(idx, 'ev_ebitda', parseFloat(e.target.value) || 0)}
                placeholder="EV/EBITDA"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <input
                type="number"
                value={deal.payment_cash_pct || 0}
                onChange={e => {
                  const cashPct = parseFloat(e.target.value) || 0;
                  updateDeal(idx, 'payment_cash_pct', cashPct);
                  updateDeal(idx, 'payment_stock_pct', 100 - cashPct);
                }}
                placeholder="Cash %"
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              />
              <select
                value={deal.industry || 'Technology'}
                onChange={e => updateDeal(idx, 'industry', e.target.value)}
                className="px-2 py-1 rounded text-xs text-white"
                style={{ backgroundColor: FINCEPT.PANEL_BG }}
              >
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
        <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
          <label className="text-xs text-gray-400 mb-2 block">Ranking Criteria</label>
          <div className="space-y-1">
            {RANK_CRITERIA_OPTIONS.map(opt => (
              <button
                key={opt.value}
                onClick={() => setRankCriteria(opt.value)}
                className={`w-full text-left px-3 py-2 rounded text-xs transition-all ${
                  rankCriteria === opt.value ? 'text-white' : 'text-gray-400'
                }`}
                style={{ backgroundColor: rankCriteria === opt.value ? FINCEPT.ORANGE + '30' : FINCEPT.PANEL_BG }}
              >
                <div className="font-medium">{opt.label}</div>
                <div className="text-xs opacity-70">{opt.desc}</div>
              </button>
            ))}
          </div>
        </div>
      )}

      {analysisType === 'benchmark' && (
        <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
          <label className="text-xs text-gray-400 mb-2 block">Target Deal to Benchmark</label>
          <select
            value={targetDealIdx}
            onChange={e => setTargetDealIdx(parseInt(e.target.value))}
            className="w-full px-3 py-2 rounded text-sm text-white"
            style={{ backgroundColor: FINCEPT.PANEL_BG }}
          >
            {deals.map((d, i) => (
              <option key={i} value={i}>
                {d.target_name || `Deal #${i + 1}`}
              </option>
            ))}
          </select>
        </div>
      )}
    </div>
  );

  const renderCompareResults = () => {
    if (!result) return null;

    return (
      <div className="space-y-4">
        {/* Comparison Table */}
        {result.comparison && (
          <div className="overflow-x-auto">
            <table className="w-full text-xs">
              <thead>
                <tr style={{ backgroundColor: FINCEPT.CHARCOAL }}>
                  <th className="text-left p-2 text-gray-400">Metric</th>
                  {deals.map((d, i) => (
                    <th key={i} className="text-right p-2 text-gray-400">{d.target_name || `Deal ${i + 1}`}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {Object.entries(result.comparison).map(([metric, values]: [string, any]) => (
                  <tr key={metric} className="border-t" style={{ borderColor: FINCEPT.PANEL_BG }}>
                    <td className="p-2 text-gray-300">{metric.replace(/_/g, ' ')}</td>
                    {(values as any[]).map((v, i) => (
                      <td key={i} className="p-2 text-right text-white font-medium">
                        {typeof v === 'number' ? v.toFixed(2) : v}
                      </td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}

        {/* Summary Stats */}
        {result.summary && (
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
            {Object.entries(result.summary).map(([key, value]) => (
              <div key={key} className="p-3 rounded text-center" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
                <div className="text-xs text-gray-400">{key.replace(/_/g, ' ')}</div>
                <div className="text-lg font-bold text-white">
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
      <div className="space-y-4">
        <div className="text-xs text-gray-400 mb-2">
          Ranked by: <span style={{ color: FINCEPT.ORANGE }}>{RANK_CRITERIA_OPTIONS.find(o => o.value === rankCriteria)?.label}</span>
        </div>
        {result.rankings.map((deal: any, idx: number) => (
          <div
            key={idx}
            className="flex items-center gap-3 p-3 rounded"
            style={{ backgroundColor: FINCEPT.CHARCOAL }}
          >
            <div
              className="w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold"
              style={{
                backgroundColor: idx === 0 ? '#FFD700' : idx === 1 ? '#C0C0C0' : idx === 2 ? '#CD7F32' : FINCEPT.PANEL_BG,
                color: idx < 3 ? '#000' : '#fff',
              }}
            >
              {idx + 1}
            </div>
            <div className="flex-1">
              <div className="text-sm font-medium text-white">{deal.target_name}</div>
              <div className="text-xs text-gray-400">Acquired by {deal.acquirer_name}</div>
            </div>
            <div className="text-right">
              <div className="text-lg font-bold" style={{ color: FINCEPT.ORANGE }}>
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
      <div className="space-y-4">
        {/* Target Deal Summary */}
        <div className="p-4 rounded text-center" style={{ backgroundColor: FINCEPT.ORANGE + '20' }}>
          <div className="text-xs text-gray-400 mb-1">Benchmarking</div>
          <div className="text-xl font-bold" style={{ color: FINCEPT.ORANGE }}>
            {targetDeal?.target_name}
          </div>
        </div>

        {/* Premium Comparison */}
        {result.premium_comparison && (
          <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <h4 className="text-xs font-medium text-white mb-3">Premium vs Comparables</h4>
            <div className="grid grid-cols-3 gap-3">
              <div className="text-center p-2 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                <div className="text-xs text-gray-400">Target Premium</div>
                <div className="text-lg font-bold text-white">{result.premium_comparison.target?.toFixed(1)}%</div>
              </div>
              <div className="text-center p-2 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                <div className="text-xs text-gray-400">Median Premium</div>
                <div className="text-lg font-bold text-white">{result.premium_comparison.median?.toFixed(1)}%</div>
              </div>
              <div className="text-center p-2 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                <div className="text-xs text-gray-400">Percentile</div>
                <div className="text-lg font-bold" style={{ color: FINCEPT.ORANGE }}>
                  {result.premium_comparison.percentile?.toFixed(0)}%
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Insight */}
        {result.insight && (
          <div className="p-3 rounded text-sm text-gray-300" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            {result.insight}
          </div>
        )}
      </div>
    );
  };

  const renderPaymentResults = () => {
    if (!result) return null;

    return (
      <div className="space-y-4">
        {/* Payment Type Distribution */}
        {result.distribution && (
          <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <h4 className="text-xs font-medium text-white mb-3">Payment Structure Distribution</h4>
            <div className="space-y-2">
              {Object.entries(result.distribution).map(([type, count]: [string, any]) => (
                <div key={type} className="flex items-center justify-between">
                  <span className="text-xs text-gray-400">{type.replace(/_/g, ' ')}</span>
                  <div className="flex items-center gap-2">
                    <div className="w-24 h-2 rounded-full overflow-hidden" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                      <div
                        className="h-full rounded-full"
                        style={{
                          backgroundColor: FINCEPT.ORANGE,
                          width: `${(count / deals.length) * 100}%`,
                        }}
                      />
                    </div>
                    <span className="text-sm font-medium text-white">{count}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* By Payment Type Stats */}
        {result.stats_by_type && (
          <div className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
            <h4 className="text-xs font-medium text-white mb-3">Metrics by Payment Type</h4>
            <div className="space-y-3">
              {Object.entries(result.stats_by_type).map(([type, stats]: [string, any]) => (
                <div key={type} className="p-2 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                  <div className="text-xs font-medium text-white mb-2">{type.replace(/_/g, ' ')}</div>
                  <div className="grid grid-cols-3 gap-2 text-xs">
                    <div>
                      <span className="text-gray-400">Avg Premium: </span>
                      <span className="text-white">{stats.avg_premium?.toFixed(1)}%</span>
                    </div>
                    <div>
                      <span className="text-gray-400">Avg Value: </span>
                      <span className="text-white">${stats.avg_value?.toFixed(0)}M</span>
                    </div>
                    <div>
                      <span className="text-gray-400">Count: </span>
                      <span className="text-white">{stats.count}</span>
                    </div>
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
      <div className="space-y-4">
        {/* Industry Distribution */}
        {result.by_industry && (
          <div className="space-y-3">
            {Object.entries(result.by_industry).map(([industry, data]: [string, any]) => (
              <div key={industry} className="p-4 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
                <div className="flex items-center gap-2 mb-3">
                  <Building2 size={14} style={{ color: FINCEPT.ORANGE }} />
                  <h4 className="text-sm font-medium text-white">{industry}</h4>
                  <span className="text-xs text-gray-400">({data.count} deals)</span>
                </div>
                <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
                  <div className="p-2 rounded text-center" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                    <div className="text-xs text-gray-400">Avg Premium</div>
                    <div className="text-sm font-bold text-white">{data.avg_premium?.toFixed(1)}%</div>
                  </div>
                  <div className="p-2 rounded text-center" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                    <div className="text-xs text-gray-400">Avg EV/Rev</div>
                    <div className="text-sm font-bold text-white">{data.avg_ev_revenue?.toFixed(1)}x</div>
                  </div>
                  <div className="p-2 rounded text-center" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                    <div className="text-xs text-gray-400">Avg EV/EBITDA</div>
                    <div className="text-sm font-bold text-white">{data.avg_ev_ebitda?.toFixed(1)}x</div>
                  </div>
                  <div className="p-2 rounded text-center" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                    <div className="text-xs text-gray-400">Total Value</div>
                    <div className="text-sm font-bold text-white">${data.total_value?.toFixed(0)}M</div>
                  </div>
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
        <div className="flex items-center justify-center h-48">
          <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
        </div>
      );
    }

    if (!result) {
      return (
        <div className="flex flex-col items-center justify-center h-48 text-gray-500 text-sm">
          <GitCompare size={32} className="mb-2 opacity-50" />
          <p>Add deals and run analysis</p>
        </div>
      );
    }

    switch (analysisType) {
      case 'compare':
        return renderCompareResults();
      case 'rank':
        return renderRankResults();
      case 'benchmark':
        return renderBenchmarkResults();
      case 'payment':
        return renderPaymentResults();
      case 'industry':
        return renderIndustryResults();
      default:
        return null;
    }
  };

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b" style={{ borderColor: FINCEPT.PANEL_BG }}>
        <div className="flex items-center gap-3">
          <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.ORANGE + '20' }}>
            <GitCompare size={20} style={{ color: FINCEPT.ORANGE }} />
          </div>
          <div>
            <h2 className={TYPOGRAPHY.HEADING}>Deal Comparison</h2>
            <p className="text-xs text-gray-400">Compare, rank, and benchmark M&A transactions</p>
          </div>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading || deals.length < 2}
          className="flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all hover:opacity-90 disabled:opacity-50"
          style={{ backgroundColor: FINCEPT.ORANGE, color: '#000' }}
        >
          {loading ? <Loader2 size={16} className="animate-spin" /> : <PlayCircle size={16} />}
          Analyze
        </button>
      </div>

      {/* Analysis Type Tabs */}
      <div className="flex items-center gap-1 px-4 py-2 border-b overflow-x-auto" style={{ borderColor: FINCEPT.PANEL_BG }}>
        <button
          onClick={() => { setAnalysisType('compare'); setResult(null); }}
          className={`flex items-center gap-2 px-3 py-2 rounded-lg text-xs font-medium transition-all whitespace-nowrap ${
            analysisType === 'compare' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'compare' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <GitCompare size={14} /> Compare
        </button>
        <button
          onClick={() => { setAnalysisType('rank'); setResult(null); }}
          className={`flex items-center gap-2 px-3 py-2 rounded-lg text-xs font-medium transition-all whitespace-nowrap ${
            analysisType === 'rank' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'rank' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <Award size={14} /> Rank
        </button>
        <button
          onClick={() => { setAnalysisType('benchmark'); setResult(null); }}
          className={`flex items-center gap-2 px-3 py-2 rounded-lg text-xs font-medium transition-all whitespace-nowrap ${
            analysisType === 'benchmark' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'benchmark' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <BarChart3 size={14} /> Benchmark
        </button>
        <button
          onClick={() => { setAnalysisType('payment'); setResult(null); }}
          className={`flex items-center gap-2 px-3 py-2 rounded-lg text-xs font-medium transition-all whitespace-nowrap ${
            analysisType === 'payment' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'payment' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <CreditCard size={14} /> Payment
        </button>
        <button
          onClick={() => { setAnalysisType('industry'); setResult(null); }}
          className={`flex items-center gap-2 px-3 py-2 rounded-lg text-xs font-medium transition-all whitespace-nowrap ${
            analysisType === 'industry' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: analysisType === 'industry' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <Building2 size={14} /> Industry
        </button>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-auto p-4">
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
          {/* Input Panel */}
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h3 className="text-sm font-medium text-white mb-4">Deal Data</h3>
            {renderDealInputs()}
          </div>

          {/* Results Panel */}
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h3 className="text-sm font-medium text-white mb-4">
              {analysisType === 'compare' && 'Comparison Results'}
              {analysisType === 'rank' && 'Rankings'}
              {analysisType === 'benchmark' && 'Benchmark Results'}
              {analysisType === 'payment' && 'Payment Analysis'}
              {analysisType === 'industry' && 'Industry Analysis'}
            </h3>
            {renderResults()}
          </div>
        </div>
      </div>
    </div>
  );
};

export default DealComparison;
