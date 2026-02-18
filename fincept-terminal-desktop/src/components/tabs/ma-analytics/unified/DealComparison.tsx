/**
 * Deal Comparison Panel - Compare, Rank, and Benchmark M&A Deals
 *
 * Tools: Side-by-side comparison, deal ranking, premium benchmarking, payment structure analysis, industry analysis
 */

import React, { useState } from 'react';
import { GitCompare, PlayCircle, Loader2, BarChart3, Award, CreditCard, Building2, Plus, Trash2, RefreshCw, ChevronDown, ChevronUp } from 'lucide-react';
import {
  RadarChart, Radar, PolarGrid, PolarAngleAxis, PolarRadiusAxis, Legend, Tooltip,
  BarChart, Bar, XAxis, YAxis, CartesianGrid, ReferenceLine, Cell,
} from 'recharts';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, MADeal } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MADataTable, MAExportButton } from '../components';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';

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
  const [inputsCollapsed, setInputsCollapsed] = useState(false);

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

  // --- Helper: input field renderer ---
  const renderInputField = (label: string, value: string | number, onChange: (v: string) => void, type: string = 'text') => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
      <label style={{
        fontSize: '8px',
        fontFamily: TYPOGRAPHY.MONO,
        fontWeight: TYPOGRAPHY.BOLD,
        color: FINCEPT.MUTED,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
      }}>
        {label}
      </label>
      <input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        style={{
          padding: '5px 8px',
          backgroundColor: FINCEPT.DARK_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          outline: 'none',
          width: '100%',
        }}
      />
    </div>
  );

  // --- Radar chart data for compare mode ---
  const buildRadarData = (): { metric: string; fullMark: number; [key: string]: any }[] => {
    if (!deals.length) return [];
    const metrics = [
      { key: 'premium_1day', label: 'Premium' },
      { key: 'ev_revenue', label: 'EV/Rev' },
      { key: 'ev_ebitda', label: 'EV/EBITDA' },
      { key: 'deal_value', label: 'Value ($B)' },
      { key: 'synergies', label: 'Synergies' },
    ];
    const maxVals: Record<string, number> = {};
    metrics.forEach(m => {
      const vals = deals.map(d => Number((d as any)[m.key]) || 0);
      maxVals[m.key] = Math.max(...vals, 1);
    });
    return metrics.map(m => {
      const row: any = { metric: m.label, fullMark: 100 };
      deals.forEach(d => {
        const raw = Number((d as any)[m.key]) || 0;
        row[d.target_name || d.deal_id] = Math.round((raw / maxVals[m.key]) * 100);
      });
      return row;
    });
  };

  // --- Rankings horizontal bar data ---
  const buildRankBarData = (): any[] => {
    if (!result?.rankings) return [];
    return result.rankings.map((deal: any) => ({
      target_name: deal.target_name || 'N/A',
      [rankCriteria]: Number(deal[rankCriteria] ?? deal.premium_1day ?? deal.synergy_value ?? 0),
    }));
  };

  // --- Benchmark bar data ---
  const buildBenchmarkBarData = (): { name: string; premium: number; isTarget: boolean }[] => {
    const targetDeal = deals[targetDealIdx];
    return deals.map(d => ({
      name: d.target_name || d.deal_id,
      premium: d.premium_1day || 0,
      isTarget: d.deal_id === targetDeal?.deal_id,
    }));
  };

  // --- Industry grouped bar data ---
  const buildIndustryBarData = (): any[] => {
    if (!result?.by_industry) return [];
    return Object.entries(result.by_industry).map(([industry, data]: [string, any]) => ({
      industry,
      avg_premium: data.avg_premium ?? 0,
      avg_ev_revenue: data.avg_ev_revenue ?? 0,
      avg_ev_ebitda: data.avg_ev_ebitda ?? 0,
    }));
  };

  // --- Analysis type tab definitions ---
  const ANALYSIS_TABS: { id: AnalysisType; label: string; icon: React.ElementType }[] = [
    { id: 'compare', label: 'COMPARE', icon: GitCompare },
    { id: 'rank', label: 'RANK', icon: Award },
    { id: 'benchmark', label: 'BENCHMARK', icon: BarChart3 },
    { id: 'payment', label: 'PAYMENT', icon: CreditCard },
    { id: 'industry', label: 'INDUSTRY', icon: Building2 },
  ];

  // --- Result title helper ---
  const getResultTitle = (): string => {
    switch (analysisType) {
      case 'compare': return 'COMPARISON RESULTS';
      case 'rank': return 'RANKINGS';
      case 'benchmark': return 'BENCHMARK RESULTS';
      case 'payment': return 'PAYMENT ANALYSIS';
      case 'industry': return 'INDUSTRY ANALYSIS';
      default: return 'RESULTS';
    }
  };

  // --- Compare mode: table columns ---
  const buildCompareTableColumns = (): { key: string; label: string; align?: 'left' | 'center' | 'right'; format?: (v: any, row: any) => React.ReactNode }[] => {
    const cols: any[] = [
      { key: 'metric', label: 'Metric', align: 'left' as const },
    ];
    deals.forEach((d, i) => {
      cols.push({
        key: `deal_${i}`,
        label: d.target_name || `Deal ${i + 1}`,
        align: 'right' as const,
        format: (v: any) => (
          <span style={{ fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>
            {typeof v === 'number' ? v.toFixed(2) : v ?? '-'}
          </span>
        ),
      });
    });
    return cols;
  };

  const buildCompareTableData = (): Record<string, any>[] => {
    if (!result?.comparison) return [];
    return Object.entries(result.comparison).map(([metric, values]: [string, any]) => {
      const row: any = { metric: metric.replace(/_/g, ' ').toUpperCase() };
      (values as any[]).forEach((v, i) => {
        row[`deal_${i}`] = v;
      });
      return row;
    });
  };

  // --- Ranking table columns ---
  const buildRankTableColumns = () => [
    {
      key: 'rank',
      label: '#',
      width: '50px',
      align: 'center' as const,
      format: (v: any, row: any) => {
        const idx = row._idx;
        const bgColor = idx === 0 ? '#FFD700' : idx === 1 ? '#C0C0C0' : idx === 2 ? '#CD7F32' : FINCEPT.PANEL_BG;
        const textColor = idx < 3 ? '#000' : FINCEPT.WHITE;
        return (
          <span style={{
            display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
            width: '22px', height: '22px', borderRadius: '2px',
            backgroundColor: bgColor, color: textColor,
            fontSize: '9px', fontWeight: TYPOGRAPHY.BOLD,
          }}>
            {idx + 1}
          </span>
        );
      },
    },
    { key: 'target_name', label: 'Target', align: 'left' as const },
    { key: 'acquirer_name', label: 'Acquirer', align: 'left' as const },
    {
      key: 'value',
      label: RANK_CRITERIA_OPTIONS.find(o => o.value === rankCriteria)?.label || 'Value',
      align: 'right' as const,
      format: (v: any) => (
        <span style={{ color: MA_COLORS.comparison, fontWeight: TYPOGRAPHY.BOLD }}>
          {v}
        </span>
      ),
    },
  ];

  const buildRankTableData = (): Record<string, any>[] => {
    if (!result?.rankings) return [];
    return result.rankings.map((deal: any, idx: number) => {
      let displayValue = '-';
      if (rankCriteria === 'premium') displayValue = `${deal.premium_1day?.toFixed(1)}%`;
      else if (rankCriteria === 'deal_value') displayValue = `$${deal.deal_value?.toFixed(0)}M`;
      else if (rankCriteria === 'ev_revenue') displayValue = `${deal.ev_revenue?.toFixed(1)}x`;
      else if (rankCriteria === 'ev_ebitda') displayValue = `${deal.ev_ebitda?.toFixed(1)}x`;
      else if (rankCriteria === 'synergies') displayValue = `$${deal.synergy_value?.toFixed(0)}M`;
      return {
        _idx: idx,
        rank: idx + 1,
        target_name: deal.target_name,
        acquirer_name: deal.acquirer_name,
        value: displayValue,
      };
    });
  };

  // --- Render: Compare Results ---
  const renderCompareResults = () => {
    if (!result) return null;
    const radarData = buildRadarData();
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Summary Metrics */}
        {result.summary && (
          <>
            <MASectionHeader
              title="Summary Metrics"
              icon={<GitCompare size={13} />}
              accentColor={MA_COLORS.comparison}
              action={
                <MAExportButton
                  data={Object.entries(result.summary).map(([k, v]) => ({ metric: k, value: v }))}
                  filename="deal_comparison_summary"
                  accentColor={MA_COLORS.comparison}
                />
              }
            />
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(140px, 1fr))', gap: '8px' }}>
              {Object.entries(result.summary).map(([key, value]) => (
                <MAMetricCard
                  key={key}
                  label={key.replace(/_/g, ' ')}
                  value={typeof value === 'number' ? value.toFixed(1) : String(value)}
                  accentColor={MA_COLORS.comparison}
                  compact
                />
              ))}
            </div>
          </>
        )}

        {/* Radar Chart: Deal Comparison */}
        {radarData.length > 0 && (
          <MAChartPanel
            title="Deal Comparison Radar"
            icon={<GitCompare size={12} />}
            accentColor={MA_COLORS.comparison}
            height={300}
          >
            <RadarChart data={radarData} outerRadius={90}>
              <PolarGrid stroke={FINCEPT.BORDER} />
              <PolarAngleAxis dataKey="metric" tick={CHART_STYLE.axis} />
              <PolarRadiusAxis tick={CHART_STYLE.axis} domain={[0, 100]} />
              {deals.map((d, i) => (
                <Radar
                  key={d.deal_id}
                  name={d.target_name || `Deal ${i + 1}`}
                  dataKey={d.target_name || d.deal_id}
                  stroke={CHART_PALETTE[i % CHART_PALETTE.length]}
                  fill={CHART_PALETTE[i % CHART_PALETTE.length]}
                  fillOpacity={0.15}
                />
              ))}
              <Tooltip contentStyle={CHART_STYLE.tooltip} />
              <Legend
                wrapperStyle={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }}
              />
            </RadarChart>
          </MAChartPanel>
        )}

        {/* Comparison Table */}
        {result.comparison && (
          <>
            <MASectionHeader
              title="Detailed Comparison"
              accentColor={MA_COLORS.comparison}
            />
            <MADataTable
              columns={buildCompareTableColumns()}
              data={buildCompareTableData()}
              accentColor={MA_COLORS.comparison}
              compact
            />
          </>
        )}
      </div>
    );
  };

  // --- Render: Rank Results ---
  const renderRankResults = () => {
    if (!result || !result.rankings) return null;
    const rankBarData = buildRankBarData();
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <MASectionHeader
          title="Rankings"
          subtitle={`Ranked by: ${RANK_CRITERIA_OPTIONS.find(o => o.value === rankCriteria)?.label}`}
          icon={<Award size={13} />}
          accentColor={MA_COLORS.comparison}
          action={
            <MAExportButton
              data={buildRankTableData()}
              filename="deal_rankings"
              accentColor={MA_COLORS.comparison}
            />
          }
        />

        {/* Horizontal Bar Chart */}
        {rankBarData.length > 0 && (
          <MAChartPanel
            title="Rankings Horizontal Bar"
            icon={<Award size={12} />}
            accentColor={MA_COLORS.comparison}
            height={Math.max(180, rankBarData.length * 50)}
          >
            <BarChart layout="vertical" data={rankBarData}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis type="number" tick={CHART_STYLE.axis} />
              <YAxis type="category" dataKey="target_name" width={100} tick={CHART_STYLE.axis} />
              <Tooltip contentStyle={CHART_STYLE.tooltip} />
              <Bar dataKey={rankCriteria} radius={[0, 2, 2, 0]}>
                {rankBarData.map((_: any, idx: number) => (
                  <Cell
                    key={idx}
                    fill={idx === 0 ? '#FFD700' : idx === 1 ? '#C0C0C0' : idx === 2 ? '#CD7F32' : FINCEPT.MUTED}
                  />
                ))}
              </Bar>
            </BarChart>
          </MAChartPanel>
        )}

        {/* Ranking Table */}
        <MADataTable
          columns={buildRankTableColumns()}
          data={buildRankTableData()}
          accentColor={MA_COLORS.comparison}
          compact
        />
      </div>
    );
  };

  // --- Render: Benchmark Results ---
  const renderBenchmarkResults = () => {
    if (!result) return null;
    const targetDeal = deals[targetDealIdx];
    const benchmarkBarData = buildBenchmarkBarData();
    const medianPremium = result.premium_comparison?.median ?? null;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <MASectionHeader
          title={`Benchmarking: ${targetDeal?.target_name || 'Target'}`}
          icon={<BarChart3 size={13} />}
          accentColor={MA_COLORS.comparison}
          action={
            result.premium_comparison ? (
              <MAExportButton
                data={benchmarkBarData}
                filename="deal_benchmark"
                accentColor={MA_COLORS.comparison}
              />
            ) : undefined
          }
        />

        {/* Premium Benchmark Metrics */}
        {result.premium_comparison && (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
            <MAMetricCard
              label="Target Premium"
              value={`${result.premium_comparison.target?.toFixed(1)}%`}
              accentColor={FINCEPT.WHITE}
              compact
            />
            <MAMetricCard
              label="Median Premium"
              value={`${result.premium_comparison.median?.toFixed(1)}%`}
              accentColor={FINCEPT.WHITE}
              compact
            />
            <MAMetricCard
              label="Percentile"
              value={`${result.premium_comparison.percentile?.toFixed(0)}%`}
              accentColor={MA_COLORS.comparison}
              compact
            />
          </div>
        )}

        {/* Premium Benchmark Distribution Chart */}
        {benchmarkBarData.length > 0 && (
          <MAChartPanel
            title="Premium Benchmark Distribution"
            icon={<BarChart3 size={12} />}
            accentColor={MA_COLORS.comparison}
            height={250}
          >
            <BarChart data={benchmarkBarData}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis dataKey="name" tick={CHART_STYLE.axis} />
              <YAxis tick={CHART_STYLE.axis} />
              <Tooltip contentStyle={CHART_STYLE.tooltip} />
              <Bar dataKey="premium" radius={[2, 2, 0, 0]}>
                {benchmarkBarData.map((entry, idx) => (
                  <Cell
                    key={idx}
                    fill={entry.isTarget ? MA_COLORS.comparison : FINCEPT.MUTED}
                  />
                ))}
              </Bar>
              {medianPremium !== null && (
                <ReferenceLine
                  y={medianPremium}
                  stroke={MA_COLORS.comparison}
                  strokeDasharray="4 4"
                  label={{
                    value: 'Median',
                    position: 'right',
                    fill: MA_COLORS.comparison,
                    fontSize: 9,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}
                />
              )}
            </BarChart>
          </MAChartPanel>
        )}

        {/* Insight */}
        {result.insight && (
          <div style={{
            padding: '10px 14px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderLeft: `3px solid ${MA_COLORS.comparison}`,
            borderRadius: '2px',
            fontSize: TYPOGRAPHY.SMALL,
            fontFamily: TYPOGRAPHY.MONO,
            color: FINCEPT.GRAY,
            lineHeight: 1.6,
          }}>
            {result.insight}
          </div>
        )}
      </div>
    );
  };

  // --- Render: Payment Results ---
  const renderPaymentResults = () => {
    if (!result) return null;
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <MASectionHeader
          title="Payment Structure Analysis"
          icon={<CreditCard size={13} />}
          accentColor={MA_COLORS.comparison}
          action={
            <MAExportButton
              data={deals.map(d => ({
                target: d.target_name, acquirer: d.acquirer_name,
                cash_pct: d.payment_cash_pct, stock_pct: d.payment_stock_pct,
              }))}
              filename="payment_structures"
              accentColor={MA_COLORS.comparison}
            />
          }
        />

        {/* Payment Structure Stacked Bar Chart */}
        <MAChartPanel
          title="Cash vs Stock Payment Structure"
          icon={<CreditCard size={12} />}
          accentColor={MA_COLORS.comparison}
          height={250}
        >
          <BarChart data={deals.map(d => ({
            name: d.target_name || d.deal_id,
            payment_cash_pct: d.payment_cash_pct,
            payment_stock_pct: d.payment_stock_pct,
          }))}>
            <CartesianGrid {...CHART_STYLE.grid} />
            <XAxis dataKey="name" tick={CHART_STYLE.axis} />
            <YAxis tick={CHART_STYLE.axis} domain={[0, 100]} />
            <Tooltip contentStyle={CHART_STYLE.tooltip} />
            <Legend wrapperStyle={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }} />
            <Bar dataKey="payment_cash_pct" stackId="a" fill="#00BCD4" name="Cash %" radius={[0, 0, 0, 0]} />
            <Bar dataKey="payment_stock_pct" stackId="a" fill="#FF6B35" name="Stock %" radius={[2, 2, 0, 0]} />
          </BarChart>
        </MAChartPanel>

        {/* Distribution */}
        {result.distribution && (
          <>
            <MASectionHeader title="Payment Distribution" accentColor={MA_COLORS.comparison} />
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(140px, 1fr))', gap: '8px' }}>
              {Object.entries(result.distribution).map(([type, count]: [string, any]) => (
                <MAMetricCard
                  key={type}
                  label={type.replace(/_/g, ' ')}
                  value={count}
                  subtitle={`${((count / deals.length) * 100).toFixed(0)}% of deals`}
                  accentColor={MA_COLORS.comparison}
                  compact
                />
              ))}
            </div>
          </>
        )}

        {/* Stats by Type */}
        {result.stats_by_type && (
          <>
            <MASectionHeader title="Metrics by Payment Type" accentColor={MA_COLORS.comparison} />
            {Object.entries(result.stats_by_type).map(([type, stats]: [string, any]) => (
              <div key={type} style={{
                padding: '10px 14px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: `3px solid ${MA_COLORS.comparison}`,
                borderRadius: '2px',
              }}>
                <div style={{
                  fontSize: '10px',
                  fontFamily: TYPOGRAPHY.MONO,
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.WHITE,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase' as const,
                  marginBottom: '8px',
                }}>
                  {type.replace(/_/g, ' ')}
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
                  <MAMetricCard label="Avg Premium" value={`${stats.avg_premium?.toFixed(1)}%`} accentColor={MA_COLORS.comparison} compact />
                  <MAMetricCard label="Avg Value" value={`$${stats.avg_value?.toFixed(0)}M`} accentColor={MA_COLORS.comparison} compact />
                  <MAMetricCard label="Count" value={stats.count} accentColor={MA_COLORS.comparison} compact />
                </div>
              </div>
            ))}
          </>
        )}
      </div>
    );
  };

  // --- Render: Industry Results ---
  const renderIndustryResults = () => {
    if (!result) return null;
    const industryBarData = buildIndustryBarData();
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <MASectionHeader
          title="Industry Analysis"
          icon={<Building2 size={13} />}
          accentColor={MA_COLORS.comparison}
          action={
            industryBarData.length > 0 ? (
              <MAExportButton
                data={industryBarData}
                filename="industry_analysis"
                accentColor={MA_COLORS.comparison}
              />
            ) : undefined
          }
        />

        {/* Industry Multiples Grouped Bar Chart */}
        {industryBarData.length > 0 && (
          <MAChartPanel
            title="Industry Multiples Comparison"
            icon={<Building2 size={12} />}
            accentColor={MA_COLORS.comparison}
            height={280}
          >
            <BarChart data={industryBarData}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis dataKey="industry" tick={CHART_STYLE.axis} />
              <YAxis tick={CHART_STYLE.axis} />
              <Tooltip contentStyle={CHART_STYLE.tooltip} />
              <Legend wrapperStyle={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }} />
              <Bar dataKey="avg_premium" fill={CHART_PALETTE[0]} name="Avg Premium" radius={[2, 2, 0, 0]} />
              <Bar dataKey="avg_ev_revenue" fill={CHART_PALETTE[1]} name="Avg EV/Rev" radius={[2, 2, 0, 0]} />
              <Bar dataKey="avg_ev_ebitda" fill={CHART_PALETTE[2]} name="Avg EV/EBITDA" radius={[2, 2, 0, 0]} />
            </BarChart>
          </MAChartPanel>
        )}

        {/* Industry Detail Cards */}
        {result.by_industry && (
          <>
            <MASectionHeader title="Industry Breakdown" accentColor={MA_COLORS.comparison} />
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              {Object.entries(result.by_industry).map(([industry, data]: [string, any]) => (
                <div key={industry} style={{
                  padding: '12px 14px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderLeft: `3px solid ${MA_COLORS.comparison}`,
                  borderRadius: '2px',
                }}>
                  <div style={{
                    display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '10px',
                  }}>
                    <Building2 size={13} color={MA_COLORS.comparison} />
                    <span style={{
                      fontSize: '10px',
                      fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.WHITE,
                      letterSpacing: '0.5px',
                      textTransform: 'uppercase' as const,
                    }}>
                      {industry}
                    </span>
                    <span style={{
                      fontSize: '9px',
                      fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.GRAY,
                    }}>
                      ({data.count} deals)
                    </span>
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '8px' }}>
                    <MAMetricCard label="Avg Premium" value={`${data.avg_premium?.toFixed(1)}%`} accentColor={MA_COLORS.comparison} compact />
                    <MAMetricCard label="Avg EV/Rev" value={`${data.avg_ev_revenue?.toFixed(1)}x`} accentColor={MA_COLORS.comparison} compact />
                    <MAMetricCard label="Avg EV/EBITDA" value={`${data.avg_ev_ebitda?.toFixed(1)}x`} accentColor={MA_COLORS.comparison} compact />
                    <MAMetricCard label="Total Value" value={`$${data.total_value?.toFixed(0)}M`} accentColor={MA_COLORS.comparison} compact />
                  </div>
                </div>
              ))}
            </div>
          </>
        )}
      </div>
    );
  };

  // --- Render: Results dispatcher ---
  const renderResults = () => {
    if (loading) {
      return (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '300px', gap: '10px' }}>
          <Loader2 size={20} className="animate-spin" style={{ color: MA_COLORS.comparison }} />
          <span style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY }}>RUNNING ANALYSIS...</span>
        </div>
      );
    }
    if (!result) {
      return (
        <MAEmptyState
          icon={<GitCompare size={32} />}
          title="Deal Comparison"
          description="Add deals and select an analysis type to begin"
          accentColor={MA_COLORS.comparison}
        />
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

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG, fontFamily: TYPOGRAPHY.MONO }}>
      {/* ===== HEADER ===== */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${MA_COLORS.comparison}`,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <GitCompare size={14} color={MA_COLORS.comparison} />
          <span style={{ fontSize: '11px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            DEAL COMPARISON
          </span>
          <span style={{
            fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED,
            padding: '2px 6px', backgroundColor: `${MA_COLORS.comparison}15`, borderRadius: '2px',
          }}>
            {deals.length} DEALS
          </span>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading || deals.length < 2}
          style={{
            padding: '5px 16px',
            backgroundColor: MA_COLORS.comparison,
            color: FINCEPT.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '9px',
            fontFamily: TYPOGRAPHY.MONO,
            fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: '0.5px',
            cursor: loading || deals.length < 2 ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '5px',
            opacity: loading || deals.length < 2 ? 0.5 : 1,
            textTransform: 'uppercase' as const,
          }}
        >
          {loading ? <Loader2 size={11} className="animate-spin" /> : <PlayCircle size={11} />}
          ANALYZE
        </button>
      </div>

      {/* ===== ANALYSIS TYPE TAB BAR ===== */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '2px',
        padding: '6px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        overflowX: 'auto',
        flexShrink: 0,
      }}>
        {ANALYSIS_TABS.map(tab => {
          const Icon = tab.icon;
          const isActive = analysisType === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => { setAnalysisType(tab.id); setResult(null); }}
              style={{
                padding: '5px 14px',
                backgroundColor: isActive ? MA_COLORS.comparison : 'transparent',
                color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '5px',
                whiteSpace: 'nowrap' as const,
                textTransform: 'uppercase' as const,
                transition: 'all 0.15s ease',
              }}
            >
              <Icon size={10} /> {tab.label}
            </button>
          );
        })}
      </div>

      {/* ===== SCROLLABLE CONTENT: Collapsible Inputs + Results ===== */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px 16px' }}>

        {/* --- Collapsible Deal Input Section --- */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: '16px',
          overflow: 'hidden',
        }}>
          {/* Input Section Header (collapsible) */}
          <div
            onClick={() => setInputsCollapsed(!inputsCollapsed)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: inputsCollapsed ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer',
              userSelect: 'none' as const,
            }}
          >
            <GitCompare size={12} color={MA_COLORS.comparison} />
            <span style={{
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              color: MA_COLORS.comparison,
              letterSpacing: '0.5px',
              textTransform: 'uppercase' as const,
            }}>
              DEAL DATA ({deals.length} DEALS)
            </span>
            <div style={{ flex: 1 }} />

            {/* Action buttons in header */}
            <button
              onClick={(e) => { e.stopPropagation(); loadFromDatabase(); }}
              disabled={loadingDb}
              style={{
                padding: '3px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: MA_COLORS.comparison,
                fontSize: '8px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: '0.5px',
                cursor: loadingDb ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px',
                textTransform: 'uppercase' as const,
              }}
            >
              {loadingDb ? <Loader2 size={9} className="animate-spin" /> : <RefreshCw size={9} />}
              LOAD DB
            </button>
            <button
              onClick={(e) => { e.stopPropagation(); addDeal(); }}
              style={{
                padding: '3px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: MA_COLORS.comparison,
                fontSize: '8px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px',
                textTransform: 'uppercase' as const,
              }}
            >
              <Plus size={9} /> ADD DEAL
            </button>
            {/* Run button in header */}
            <button
              onClick={(e) => { e.stopPropagation(); runAnalysis(); }}
              disabled={loading || deals.length < 2}
              style={{
                padding: '3px 14px',
                backgroundColor: MA_COLORS.comparison,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '8px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: '0.5px',
                cursor: loading || deals.length < 2 ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: loading || deals.length < 2 ? 0.5 : 1,
                textTransform: 'uppercase' as const,
              }}
            >
              <PlayCircle size={9} />
              {loading ? 'RUNNING...' : 'RUN'}
            </button>
            {inputsCollapsed
              ? <ChevronDown size={12} color={FINCEPT.GRAY} />
              : <ChevronUp size={12} color={FINCEPT.GRAY} />
            }
          </div>

          {/* Input Fields (collapsible body) */}
          {!inputsCollapsed && (
            <div style={{ padding: '12px' }}>

              {/* Database deals quick import */}
              {dbDeals.length > 0 && (
                <div style={{
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  borderRadius: '2px',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  marginBottom: '10px',
                }}>
                  <div style={{
                    fontSize: '8px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.MUTED,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '6px',
                  }}>
                    QUICK IMPORT FROM DATABASE:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {dbDeals.slice(0, 5).map(d => (
                      <button
                        key={d.deal_id}
                        onClick={() => importFromDb(d)}
                        style={{
                          padding: '3px 8px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.BORDER}`,
                          color: MA_COLORS.comparison,
                          fontSize: '9px',
                          fontFamily: TYPOGRAPHY.MONO,
                          fontWeight: TYPOGRAPHY.BOLD,
                          cursor: 'pointer',
                          borderRadius: '2px',
                          transition: 'all 0.15s',
                        }}
                      >
                        {d.target_name}
                      </button>
                    ))}
                    {dbDeals.length > 5 && (
                      <span style={{
                        fontSize: '9px',
                        fontFamily: TYPOGRAPHY.MONO,
                        color: FINCEPT.MUTED,
                        padding: '3px 8px',
                        display: 'flex',
                        alignItems: 'center',
                      }}>
                        +{dbDeals.length - 5} more
                      </span>
                    )}
                  </div>
                </div>
              )}

              {/* Deal grid */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', maxHeight: '380px', overflowY: 'auto' }}>
                {deals.map((deal, idx) => (
                  <div key={deal.deal_id} style={{
                    padding: '10px 12px',
                    backgroundColor: FINCEPT.DARK_BG,
                    borderRadius: '2px',
                    border: `1px solid ${FINCEPT.BORDER}`,
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                      <span style={{
                        fontSize: '9px',
                        fontFamily: TYPOGRAPHY.MONO,
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: MA_COLORS.comparison,
                        letterSpacing: '0.5px',
                      }}>
                        DEAL #{idx + 1}
                      </span>
                      <button
                        onClick={() => removeDeal(idx)}
                        disabled={deals.length <= 2}
                        style={{
                          background: 'none',
                          border: 'none',
                          cursor: deals.length <= 2 ? 'not-allowed' : 'pointer',
                          padding: '2px',
                          display: 'flex',
                          alignItems: 'center',
                          opacity: deals.length <= 2 ? 0.3 : 1,
                        }}
                      >
                        <Trash2 size={11} color={deals.length <= 2 ? FINCEPT.MUTED : FINCEPT.RED} />
                      </button>
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
                      {renderInputField('Acquirer', deal.acquirer_name || '', (v) => updateDeal(idx, 'acquirer_name', v))}
                      {renderInputField('Target', deal.target_name || '', (v) => updateDeal(idx, 'target_name', v))}
                      {renderInputField('Value ($M)', String(deal.deal_value || 0), (v) => updateDeal(idx, 'deal_value', parseFloat(v) || 0), 'number')}
                      {renderInputField('Premium %', String(deal.premium_1day || 0), (v) => updateDeal(idx, 'premium_1day', parseFloat(v) || 0), 'number')}
                      {renderInputField('EV/Rev', String(deal.ev_revenue || 0), (v) => updateDeal(idx, 'ev_revenue', parseFloat(v) || 0), 'number')}
                      {renderInputField('EV/EBITDA', String(deal.ev_ebitda || 0), (v) => updateDeal(idx, 'ev_ebitda', parseFloat(v) || 0), 'number')}
                      {renderInputField('Cash %', String(deal.payment_cash_pct || 0), (v) => { const cashPct = parseFloat(v) || 0; updateDeal(idx, 'payment_cash_pct', cashPct); updateDeal(idx, 'payment_stock_pct', 100 - cashPct); }, 'number')}
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
                        <label style={{
                          fontSize: '8px',
                          fontFamily: TYPOGRAPHY.MONO,
                          fontWeight: TYPOGRAPHY.BOLD,
                          color: FINCEPT.MUTED,
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase' as const,
                        }}>
                          Industry
                        </label>
                        <select
                          value={deal.industry || 'Technology'}
                          onChange={(e) => updateDeal(idx, 'industry', e.target.value)}
                          style={{
                            padding: '5px 8px',
                            backgroundColor: FINCEPT.DARK_BG,
                            color: FINCEPT.WHITE,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            borderRadius: '2px',
                            fontSize: '10px',
                            fontFamily: TYPOGRAPHY.MONO,
                            outline: 'none',
                            width: '100%',
                          }}
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
                  </div>
                ))}
              </div>

              {/* Analysis-specific options */}
              {analysisType === 'rank' && (
                <div style={{
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  borderRadius: '2px',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  marginTop: '10px',
                }}>
                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.MUTED,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    RANKING CRITERIA
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '4px' }}>
                    {RANK_CRITERIA_OPTIONS.map(opt => (
                      <button
                        key={opt.value}
                        onClick={() => setRankCriteria(opt.value)}
                        style={{
                          padding: '6px 8px',
                          backgroundColor: rankCriteria === opt.value ? MA_COLORS.comparison : 'transparent',
                          color: rankCriteria === opt.value ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                          border: rankCriteria === opt.value ? 'none' : `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '8px',
                          fontFamily: TYPOGRAPHY.MONO,
                          fontWeight: TYPOGRAPHY.BOLD,
                          letterSpacing: '0.3px',
                          cursor: 'pointer',
                          textTransform: 'uppercase' as const,
                          textAlign: 'center' as const,
                          transition: 'all 0.15s ease',
                        }}
                        title={opt.desc}
                      >
                        {opt.label}
                      </button>
                    ))}
                  </div>
                </div>
              )}

              {analysisType === 'benchmark' && (
                <div style={{
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  borderRadius: '2px',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  marginTop: '10px',
                }}>
                  <label style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.MUTED,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    display: 'block',
                    marginBottom: '6px',
                  }}>
                    TARGET DEAL TO BENCHMARK
                  </label>
                  <select
                    value={targetDealIdx}
                    onChange={(e) => setTargetDealIdx(parseInt(e.target.value))}
                    style={{
                      ...COMMON_STYLES.inputField,
                      padding: '5px 8px',
                      fontSize: '10px',
                    }}
                  >
                    {deals.map((d, i) => (
                      <option key={i} value={i}>{d.target_name || `Deal #${i + 1}`}</option>
                    ))}
                  </select>
                </div>
              )}
            </div>
          )}
        </div>

        {/* --- Results Section --- */}
        <MASectionHeader
          title={getResultTitle()}
          accentColor={MA_COLORS.comparison}
        />
        {renderResults()}
      </div>
    </div>
  );
};

export default DealComparison;
