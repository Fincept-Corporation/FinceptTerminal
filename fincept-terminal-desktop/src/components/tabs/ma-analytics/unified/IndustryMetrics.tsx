/**
 * Industry Metrics Panel - Sector-Specific Valuation Metrics
 *
 * Sectors: Technology (SaaS, Marketplace, Semiconductor), Healthcare (Pharma, Biotech, Devices, Services),
 * Financial Services (Banking, Insurance, Asset Management)
 */

import React, { useState, useMemo } from 'react';
import { Cpu, HeartPulse, Building2, PlayCircle, Loader2, TrendingUp, BarChart3, ChevronDown, ChevronUp, Lightbulb, Target } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';
import { MAMetricCard } from '../components/MAMetricCard';
import { MAChartPanel } from '../components/MAChartPanel';
import { MASectionHeader } from '../components/MASectionHeader';
import { MAEmptyState } from '../components/MAEmptyState';
import { MADataTable } from '../components/MADataTable';
import { MAExportButton } from '../components/MAExportButton';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, Cell,
  RadarChart, Radar, PolarGrid, PolarAngleAxis, PolarRadiusAxis,
} from 'recharts';

type IndustryType = 'tech' | 'healthcare' | 'financial';

const TECH_SECTORS = [
  { value: 'saas', label: 'SaaS / Cloud' },
  { value: 'marketplace', label: 'Marketplace / Platform' },
  { value: 'semiconductor', label: 'Semiconductor' },
] as const;

const HEALTHCARE_SECTORS = [
  { value: 'pharma', label: 'Pharmaceuticals' },
  { value: 'biotech', label: 'Biotechnology' },
  { value: 'devices', label: 'Medical Devices' },
  { value: 'services', label: 'Healthcare Services' },
] as const;

const FINANCIAL_SECTORS = [
  { value: 'banking', label: 'Banking' },
  { value: 'insurance', label: 'Insurance' },
  { value: 'asset_management', label: 'Asset Management' },
] as const;

const TECH_FIELDS = {
  saas: [
    { key: 'arr', label: 'Annual Recurring Revenue ($M)', type: 'number' },
    { key: 'arr_growth', label: 'ARR Growth Rate (%)', type: 'number' },
    { key: 'net_retention', label: 'Net Revenue Retention (%)', type: 'number' },
    { key: 'gross_margin', label: 'Gross Margin (%)', type: 'number' },
    { key: 'cac_payback', label: 'CAC Payback (months)', type: 'number' },
    { key: 'ltv_cac', label: 'LTV/CAC Ratio', type: 'number' },
    { key: 'rule_of_40', label: 'Rule of 40 Score', type: 'number' },
  ],
  marketplace: [
    { key: 'gmv', label: 'Gross Merchandise Value ($M)', type: 'number' },
    { key: 'take_rate', label: 'Take Rate (%)', type: 'number' },
    { key: 'gmv_growth', label: 'GMV Growth Rate (%)', type: 'number' },
    { key: 'active_buyers', label: 'Active Buyers (000s)', type: 'number' },
    { key: 'active_sellers', label: 'Active Sellers (000s)', type: 'number' },
    { key: 'repeat_rate', label: 'Repeat Purchase Rate (%)', type: 'number' },
  ],
  semiconductor: [
    { key: 'revenue', label: 'Revenue ($M)', type: 'number' },
    { key: 'revenue_growth', label: 'Revenue Growth (%)', type: 'number' },
    { key: 'gross_margin', label: 'Gross Margin (%)', type: 'number' },
    { key: 'r_and_d_intensity', label: 'R&D Intensity (%)', type: 'number' },
    { key: 'design_wins', label: 'Design Wins', type: 'number' },
    { key: 'book_to_bill', label: 'Book-to-Bill Ratio', type: 'number' },
  ],
};

const HEALTHCARE_FIELDS = {
  pharma: [
    { key: 'revenue', label: 'Revenue ($M)', type: 'number' },
    { key: 'pipeline_value', label: 'Pipeline NPV ($M)', type: 'number' },
    { key: 'phase3_count', label: 'Phase 3 Drug Candidates', type: 'number' },
    { key: 'patent_expiry_revenue', label: 'Revenue Facing Patent Expiry (%)', type: 'number' },
    { key: 'r_and_d_spend', label: 'R&D Spend ($M)', type: 'number' },
    { key: 'ebitda_margin', label: 'EBITDA Margin (%)', type: 'number' },
  ],
  biotech: [
    { key: 'cash_position', label: 'Cash Position ($M)', type: 'number' },
    { key: 'burn_rate', label: 'Monthly Burn Rate ($M)', type: 'number' },
    { key: 'pipeline_value', label: 'Pipeline NPV ($M)', type: 'number' },
    { key: 'lead_program_phase', label: 'Lead Program Phase (1-3)', type: 'number' },
    { key: 'platform_value', label: 'Platform Value ($M)', type: 'number' },
    { key: 'partnership_revenue', label: 'Partnership Revenue ($M)', type: 'number' },
  ],
  devices: [
    { key: 'revenue', label: 'Revenue ($M)', type: 'number' },
    { key: 'revenue_growth', label: 'Revenue Growth (%)', type: 'number' },
    { key: 'gross_margin', label: 'Gross Margin (%)', type: 'number' },
    { key: 'fda_clearances', label: 'FDA Clearances (past 2 yrs)', type: 'number' },
    { key: 'recurring_revenue_pct', label: 'Recurring Revenue (%)', type: 'number' },
    { key: 'international_revenue_pct', label: 'International Revenue (%)', type: 'number' },
  ],
  services: [
    { key: 'revenue', label: 'Revenue ($M)', type: 'number' },
    { key: 'organic_growth', label: 'Organic Growth (%)', type: 'number' },
    { key: 'ebitda_margin', label: 'EBITDA Margin (%)', type: 'number' },
    { key: 'patient_volume_growth', label: 'Patient Volume Growth (%)', type: 'number' },
    { key: 'payor_mix_commercial', label: 'Commercial Payor Mix (%)', type: 'number' },
    { key: 'same_store_growth', label: 'Same-Store Growth (%)', type: 'number' },
  ],
};

const FINANCIAL_FIELDS = {
  banking: [
    { key: 'total_assets', label: 'Total Assets ($M)', type: 'number' },
    { key: 'total_deposits', label: 'Total Deposits ($M)', type: 'number' },
    { key: 'net_interest_margin', label: 'Net Interest Margin (%)', type: 'number' },
    { key: 'efficiency_ratio', label: 'Efficiency Ratio (%)', type: 'number' },
    { key: 'roa', label: 'Return on Assets (%)', type: 'number' },
    { key: 'roe', label: 'Return on Equity (%)', type: 'number' },
    { key: 'cet1_ratio', label: 'CET1 Capital Ratio (%)', type: 'number' },
    { key: 'npl_ratio', label: 'NPL Ratio (%)', type: 'number' },
  ],
  insurance: [
    { key: 'gross_premiums', label: 'Gross Written Premiums ($M)', type: 'number' },
    { key: 'combined_ratio', label: 'Combined Ratio (%)', type: 'number' },
    { key: 'loss_ratio', label: 'Loss Ratio (%)', type: 'number' },
    { key: 'expense_ratio', label: 'Expense Ratio (%)', type: 'number' },
    { key: 'investment_yield', label: 'Investment Yield (%)', type: 'number' },
    { key: 'book_value', label: 'Book Value ($M)', type: 'number' },
    { key: 'roe', label: 'Return on Equity (%)', type: 'number' },
  ],
  asset_management: [
    { key: 'aum', label: 'Assets Under Management ($B)', type: 'number' },
    { key: 'net_flows', label: 'Net Flows ($B)', type: 'number' },
    { key: 'fee_rate', label: 'Average Fee Rate (bps)', type: 'number' },
    { key: 'operating_margin', label: 'Operating Margin (%)', type: 'number' },
    { key: 'active_vs_passive', label: 'Active AUM (%)', type: 'number' },
    { key: 'alpha_generation', label: 'Avg Alpha vs Benchmark (bps)', type: 'number' },
  ],
};

export const IndustryMetrics: React.FC = () => {
  const [industryType, setIndustryType] = useState<IndustryType>('tech');
  const [techSector, setTechSector] = useState<'saas' | 'marketplace' | 'semiconductor'>('saas');
  const [healthcareSector, setHealthcareSector] = useState<'pharma' | 'biotech' | 'devices' | 'services'>('pharma');
  const [financialSector, setFinancialSector] = useState<'banking' | 'insurance' | 'asset_management'>('banking');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [inputsCollapsed, setInputsCollapsed] = useState(false);

  const [techInputs, setTechInputs] = useState<Record<string, Record<string, number>>>({
    saas: { arr: 100, arr_growth: 40, net_retention: 115, gross_margin: 75, cac_payback: 18, ltv_cac: 3.5, rule_of_40: 55 },
    marketplace: { gmv: 500, take_rate: 15, gmv_growth: 35, active_buyers: 150, active_sellers: 25, repeat_rate: 55 },
    semiconductor: { revenue: 200, revenue_growth: 20, gross_margin: 55, r_and_d_intensity: 18, design_wins: 12, book_to_bill: 1.15 },
  });

  const [healthcareInputs, setHealthcareInputs] = useState<Record<string, Record<string, number>>>({
    pharma: { revenue: 5000, pipeline_value: 3000, phase3_count: 5, patent_expiry_revenue: 25, r_and_d_spend: 800, ebitda_margin: 35 },
    biotech: { cash_position: 200, burn_rate: 15, pipeline_value: 800, lead_program_phase: 2, platform_value: 300, partnership_revenue: 25 },
    devices: { revenue: 300, revenue_growth: 12, gross_margin: 65, fda_clearances: 8, recurring_revenue_pct: 35, international_revenue_pct: 40 },
    services: { revenue: 800, organic_growth: 8, ebitda_margin: 18, patient_volume_growth: 5, payor_mix_commercial: 55, same_store_growth: 4 },
  });

  const [financialInputs, setFinancialInputs] = useState<Record<string, Record<string, number>>>({
    banking: { total_assets: 50000, total_deposits: 40000, net_interest_margin: 3.2, efficiency_ratio: 58, roa: 1.1, roe: 12, cet1_ratio: 12.5, npl_ratio: 0.8 },
    insurance: { gross_premiums: 2000, combined_ratio: 95, loss_ratio: 65, expense_ratio: 30, investment_yield: 4.5, book_value: 800, roe: 14 },
    asset_management: { aum: 150, net_flows: 8, fee_rate: 45, operating_margin: 35, active_vs_passive: 60, alpha_generation: 75 },
  });

  const runAnalysis = async () => {
    setLoading(true);
    setResult(null);
    try {
      let data;
      if (industryType === 'tech') {
        data = await MAAnalyticsService.IndustryMetrics.calculateTechMetrics(techSector, techInputs[techSector]);
      } else if (industryType === 'healthcare') {
        data = await MAAnalyticsService.IndustryMetrics.calculateHealthcareMetrics(healthcareSector, healthcareInputs[healthcareSector]);
      } else {
        data = await MAAnalyticsService.IndustryMetrics.calculateFinancialServicesMetrics(financialSector, financialInputs[financialSector]);
      }
      setResult(data);
      showSuccess('Industry metrics calculated successfully');
    } catch (error) {
      showError(`Failed to calculate metrics: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const getCurrentFields = () => {
    if (industryType === 'tech') return TECH_FIELDS[techSector];
    if (industryType === 'healthcare') return HEALTHCARE_FIELDS[healthcareSector];
    return FINANCIAL_FIELDS[financialSector];
  };

  const getCurrentInputs = () => {
    if (industryType === 'tech') return techInputs[techSector];
    if (industryType === 'healthcare') return healthcareInputs[healthcareSector];
    return financialInputs[financialSector];
  };

  const updateInput = (key: string, value: number) => {
    if (industryType === 'tech') {
      setTechInputs(prev => ({
        ...prev,
        [techSector]: { ...prev[techSector], [key]: value },
      }));
    } else if (industryType === 'healthcare') {
      setHealthcareInputs(prev => ({
        ...prev,
        [healthcareSector]: { ...prev[healthcareSector], [key]: value },
      }));
    } else {
      setFinancialInputs(prev => ({
        ...prev,
        [financialSector]: { ...prev[financialSector], [key]: value },
      }));
    }
  };

  const renderSectorTabs = () => {
    const sectors = industryType === 'tech' ? TECH_SECTORS
      : industryType === 'healthcare' ? HEALTHCARE_SECTORS
      : FINANCIAL_SECTORS;

    const currentSector = industryType === 'tech' ? techSector
      : industryType === 'healthcare' ? healthcareSector
      : financialSector;

    const setSector = (val: string) => {
      if (industryType === 'tech') setTechSector(val as any);
      else if (industryType === 'healthcare') setHealthcareSector(val as any);
      else setFinancialSector(val as any);
    };

    return sectors.map(s => (
      <button
        key={s.value}
        onClick={() => setSector(s.value)}
        style={{
          padding: '4px 10px',
          fontSize: TYPOGRAPHY.TINY,
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
          backgroundColor: currentSector === s.value ? MA_COLORS.industry : 'transparent',
          color: currentSector === s.value ? FINCEPT.DARK_BG : FINCEPT.GRAY,
          border: currentSector === s.value ? `1px solid ${MA_COLORS.industry}` : `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          cursor: 'pointer',
          transition: 'all 0.15s',
        }}
      >
        {s.label}
      </button>
    ));
  };

  const currentSectorLabel = useMemo(() => {
    if (industryType === 'tech') return TECH_SECTORS.find(s => s.value === techSector)?.label ?? '';
    if (industryType === 'healthcare') return HEALTHCARE_SECTORS.find(s => s.value === healthcareSector)?.label ?? '';
    return FINANCIAL_SECTORS.find(s => s.value === financialSector)?.label ?? '';
  }, [industryType, techSector, healthcareSector, financialSector]);

  // Build benchmark chart data
  const benchmarkChartData = useMemo(() => {
    if (!result?.benchmarks) return [];
    const inputs = getCurrentInputs();
    return Object.entries(result.benchmarks).map(([key, benchmark]: [string, any]) => {
      const inputValue = inputs[key] || 0;
      return {
        metric: key.replace(/_/g, ' '),
        company: inputValue,
        p25: benchmark.p25,
        median: benchmark.median,
        p75: benchmark.p75,
      };
    });
  }, [result, industryType, techSector, healthcareSector, financialSector]);

  // Build radar chart data from valuation metrics
  const radarChartData = useMemo(() => {
    if (!result?.valuation_metrics) return [];
    return Object.entries(result.valuation_metrics).slice(0, 8).map(([key, value]: [string, any]) => ({
      metric: key.replace(/_/g, ' '),
      value: typeof value === 'number' ? Math.min(value, 100) : 0,
      fullMark: 100,
    }));
  }, [result]);

  // Build benchmark table data
  const benchmarkTableData = useMemo(() => {
    if (!result?.benchmarks) return [];
    const inputs = getCurrentInputs();
    return Object.entries(result.benchmarks).map(([key, benchmark]: [string, any]) => ({
      metric: key.replace(/_/g, ' ').toUpperCase(),
      company: inputs[key] ?? '-',
      p25: benchmark.p25?.toFixed(1) ?? '-',
      median: benchmark.median?.toFixed(1) ?? '-',
      p75: benchmark.p75?.toFixed(1) ?? '-',
    }));
  }, [result, industryType, techSector, healthcareSector, financialSector]);

  // Build export data
  const exportData = useMemo(() => {
    if (!result) return [];
    const rows: Record<string, any>[] = [];
    if (result.valuation_metrics) {
      Object.entries(result.valuation_metrics).forEach(([key, value]) => {
        rows.push({ section: 'Valuation', metric: key.replace(/_/g, ' '), value: typeof value === 'number' ? value.toFixed(4) : String(value) });
      });
    }
    if (result.benchmarks) {
      const inputs = getCurrentInputs();
      Object.entries(result.benchmarks).forEach(([key, benchmark]: [string, any]) => {
        rows.push({
          section: 'Benchmark',
          metric: key.replace(/_/g, ' '),
          company: inputs[key] ?? '',
          p25: benchmark.p25?.toFixed(2) ?? '',
          median: benchmark.median?.toFixed(2) ?? '',
          p75: benchmark.p75?.toFixed(2) ?? '',
        });
      });
    }
    if (result.insights) {
      result.insights.forEach((insight: string, idx: number) => {
        rows.push({ section: 'Insight', metric: `#${idx + 1}`, value: insight });
      });
    }
    return rows;
  }, [result, industryType, techSector, healthcareSector, financialSector]);

  const benchmarkTableColumns = [
    { key: 'metric', label: 'Metric', width: '35%' },
    { key: 'company', label: 'Company', align: 'right' as const, format: (v: any) => <span style={{ color: MA_COLORS.industry, fontWeight: TYPOGRAPHY.BOLD }}>{v}</span> },
    { key: 'p25', label: 'P25', align: 'right' as const },
    { key: 'median', label: 'Median', align: 'right' as const, format: (v: any) => <span style={{ color: FINCEPT.WHITE }}>{v}</span> },
    { key: 'p75', label: 'P75', align: 'right' as const },
  ];

  const renderResults = () => {
    if (!result) return null;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.LARGE }}>
        {/* Valuation Metrics Cards */}
        {result.valuation_metrics && (
          <div>
            <MASectionHeader
              title="Valuation Metrics"
              subtitle={currentSectorLabel}
              icon={<TrendingUp size={14} />}
              accentColor={MA_COLORS.industry}
              action={
                <MAExportButton
                  data={exportData}
                  filename={`industry_metrics_${industryType}`}
                  accentColor={MA_COLORS.industry}
                />
              }
            />
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(180px, 1fr))',
              gap: SPACING.MEDIUM,
            }}>
              {Object.entries(result.valuation_metrics).map(([key, value]) => (
                <MAMetricCard
                  key={key}
                  label={key.replace(/_/g, ' ')}
                  value={typeof value === 'number' ? value.toFixed(2) : String(value)}
                  accentColor={MA_COLORS.industry}
                  compact
                />
              ))}
            </div>
          </div>
        )}

        {/* Sector Profile Radar Chart */}
        {result.valuation_metrics && radarChartData.length > 0 && (
          <MAChartPanel
            title="Sector Profile Radar"
            icon={<Target size={14} />}
            accentColor={MA_COLORS.industry}
            height={300}
          >
            <RadarChart data={radarChartData}>
              <PolarGrid stroke={FINCEPT.BORDER} />
              <PolarAngleAxis dataKey="metric" tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: 'var(--ft-font-family, monospace)' }} />
              <PolarRadiusAxis tick={CHART_STYLE.axis} />
              <Radar dataKey="value" stroke={MA_COLORS.industry} fill={MA_COLORS.industry} fillOpacity={0.3} />
              <Tooltip contentStyle={CHART_STYLE.tooltip} />
            </RadarChart>
          </MAChartPanel>
        )}

        {/* Industry Benchmark Bar Chart */}
        {result.benchmarks && benchmarkChartData.length > 0 && (
          <MAChartPanel
            title="Industry Benchmark Comparison"
            icon={<BarChart3 size={14} />}
            accentColor={MA_COLORS.industry}
            height={300}
          >
            <BarChart data={benchmarkChartData}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis
                dataKey="metric"
                tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: 'var(--ft-font-family, monospace)' }}
                angle={-45}
                textAnchor="end"
                height={60}
              />
              <YAxis tick={CHART_STYLE.axis} />
              <Tooltip contentStyle={CHART_STYLE.tooltip} />
              <Legend
                wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
              />
              <Bar dataKey="company" fill={MA_COLORS.industry} name="Company" />
              <Bar dataKey="median" fill={FINCEPT.GRAY} name="Industry Median" fillOpacity={0.5} />
            </BarChart>
          </MAChartPanel>
        )}

        {/* Benchmark Data Table */}
        {result.benchmarks && benchmarkTableData.length > 0 && (
          <div>
            <MASectionHeader
              title="Benchmark Details"
              subtitle="Percentile Distribution"
              icon={<BarChart3 size={14} />}
              accentColor={MA_COLORS.industry}
            />
            <MADataTable
              columns={benchmarkTableColumns}
              data={benchmarkTableData}
              accentColor={MA_COLORS.industry}
              compact
              maxHeight="300px"
            />
          </div>
        )}

        {/* Key Insights */}
        {result.insights && result.insights.length > 0 && (
          <div>
            <MASectionHeader
              title="Key Insights"
              icon={<Lightbulb size={14} />}
              accentColor={MA_COLORS.industry}
            />
            <div style={{
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              padding: SPACING.DEFAULT,
              display: 'flex',
              flexDirection: 'column',
              gap: SPACING.MEDIUM,
            }}>
              {result.insights.map((insight: string, idx: number) => (
                <div key={idx} style={{
                  display: 'flex',
                  alignItems: 'flex-start',
                  gap: SPACING.MEDIUM,
                  padding: SPACING.MEDIUM,
                  backgroundColor: idx % 2 === 0 ? 'transparent' : `${FINCEPT.CHARCOAL}40`,
                  borderRadius: '2px',
                }}>
                  <span style={{
                    flexShrink: 0,
                    width: '18px',
                    height: '18px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    backgroundColor: `${MA_COLORS.industry}20`,
                    color: MA_COLORS.industry,
                    fontSize: '8px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    borderRadius: '2px',
                  }}>
                    {idx + 1}
                  </span>
                  <span style={{
                    fontSize: TYPOGRAPHY.SMALL,
                    fontFamily: TYPOGRAPHY.MONO,
                    color: FINCEPT.GRAY,
                    lineHeight: 1.5,
                  }}>
                    {insight}
                  </span>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Raw Data (collapsed by default) */}
        <details>
          <summary style={{
            fontSize: TYPOGRAPHY.TINY,
            fontFamily: TYPOGRAPHY.MONO,
            color: FINCEPT.MUTED,
            cursor: 'pointer',
            padding: `${SPACING.SMALL} 0`,
          }}>
            View raw data
          </summary>
          <pre style={{
            marginTop: SPACING.SMALL,
            padding: SPACING.DEFAULT,
            borderRadius: '2px',
            overflow: 'auto',
            fontSize: TYPOGRAPHY.TINY,
            fontFamily: TYPOGRAPHY.MONO,
            color: FINCEPT.GRAY,
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            maxHeight: 200,
          }}>
            {JSON.stringify(result, null, 2)}
          </pre>
        </details>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${MA_COLORS.industry}`,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
          <BarChart3 size={14} color={MA_COLORS.industry} />
          <span style={{
            fontSize: '11px',
            fontWeight: TYPOGRAPHY.BOLD,
            color: MA_COLORS.industry,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase' as const,
          }}>
            INDUSTRY METRICS
          </span>
          <span style={{
            fontSize: TYPOGRAPHY.TINY,
            color: FINCEPT.MUTED,
            fontFamily: TYPOGRAPHY.MONO,
            letterSpacing: TYPOGRAPHY.WIDE,
          }}>
            SECTOR-SPECIFIC VALUATION
          </span>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            backgroundColor: MA_COLORS.industry,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            opacity: loading ? 0.7 : 1,
          }}
        >
          {loading ? <Loader2 size={10} className="animate-spin" /> : <PlayCircle size={10} />}
          CALCULATE METRICS
        </button>
      </div>

      {/* Industry Type Tabs */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.SMALL,
        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        flexShrink: 0,
      }}>
        {([
          { id: 'tech', label: 'TECHNOLOGY', icon: Cpu },
          { id: 'healthcare', label: 'HEALTHCARE', icon: HeartPulse },
          { id: 'financial', label: 'FINANCIAL SERVICES', icon: Building2 },
        ] as const).map(item => {
          const Icon = item.icon;
          const isActive = industryType === item.id;
          return (
            <button
              key={item.id}
              onClick={() => setIndustryType(item.id)}
              style={{
                padding: '6px 12px',
                fontSize: '9px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase' as const,
                backgroundColor: isActive ? `${MA_COLORS.industry}20` : 'transparent',
                color: isActive ? MA_COLORS.industry : FINCEPT.GRAY,
                border: isActive ? `1px solid ${MA_COLORS.industry}` : `1px solid transparent`,
                borderRadius: '2px',
                cursor: 'pointer',
                transition: 'all 0.15s',
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.SMALL,
              }}
            >
              <Icon size={12} />
              {item.label}
            </button>
          );
        })}
      </div>

      {/* Sector Sub-tabs */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.TINY,
        padding: `${SPACING.TINY} ${SPACING.DEFAULT}`,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        overflowX: 'auto',
        flexShrink: 0,
      }}>
        {renderSectorTabs()}
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
          {/* Input section header - clickable to collapse */}
          <div
            onClick={() => setInputsCollapsed(!inputsCollapsed)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.MEDIUM,
              padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: inputsCollapsed ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer',
              userSelect: 'none' as const,
            }}
          >
            <span style={{
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              color: MA_COLORS.industry,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase' as const,
            }}>
              {currentSectorLabel} INPUTS
            </span>
            <span style={{
              fontSize: TYPOGRAPHY.TINY,
              fontFamily: TYPOGRAPHY.MONO,
              color: FINCEPT.MUTED,
            }}>
              {getCurrentFields().length} parameters
            </span>
            <div style={{ flex: 1 }} />
            {inputsCollapsed
              ? <ChevronDown size={12} color={FINCEPT.GRAY} />
              : <ChevronUp size={12} color={FINCEPT.GRAY} />
            }
          </div>

          {/* 3-column input grid */}
          {!inputsCollapsed && (
            <div style={{
              padding: SPACING.DEFAULT,
              display: 'grid',
              gridTemplateColumns: 'repeat(3, 1fr)',
              gap: SPACING.MEDIUM,
            }}>
              {getCurrentFields().map(field => (
                <div key={field.key}>
                  <label style={{
                    fontSize: TYPOGRAPHY.TINY,
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: TYPOGRAPHY.WIDE,
                    textTransform: 'uppercase' as const,
                    display: 'block',
                    marginBottom: SPACING.TINY,
                  }}>
                    {field.label}
                  </label>
                  <input
                    type="number"
                    value={getCurrentInputs()[field.key] || 0}
                    onChange={e => updateInput(field.key, parseFloat(e.target.value) || 0)}
                    style={{
                      ...COMMON_STYLES.inputField,
                      borderColor: FINCEPT.BORDER,
                      padding: '6px 8px',
                      fontSize: TYPOGRAPHY.SMALL,
                    }}
                    onFocus={e => { e.currentTarget.style.borderColor = MA_COLORS.industry; }}
                    onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                  />
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Full-Width Results Area */}
        {loading ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '300px',
            gap: SPACING.DEFAULT,
          }}>
            <Loader2 size={28} className="animate-spin" style={{ color: MA_COLORS.industry }} />
            <span style={{
              fontSize: TYPOGRAPHY.SMALL,
              fontFamily: TYPOGRAPHY.MONO,
              color: FINCEPT.MUTED,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase' as const,
            }}>
              Calculating industry metrics...
            </span>
          </div>
        ) : result ? (
          renderResults()
        ) : (
          <MAEmptyState
            icon={<BarChart3 size={36} />}
            title="Industry Metrics"
            description="Select a sector and configure inputs above, then click CALCULATE METRICS to analyze"
            accentColor={MA_COLORS.industry}
            actionLabel="Calculate"
            onAction={runAnalysis}
          />
        )}
      </div>
    </div>
  );
};

export default IndustryMetrics;
