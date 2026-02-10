/**
 * Industry Metrics Panel - Sector-Specific Valuation Metrics
 *
 * Sectors: Technology (SaaS, Marketplace, Semiconductor), Healthcare (Pharma, Biotech, Devices, Services),
 * Financial Services (Banking, Insurance, Asset Management)
 */

import React, { useState } from 'react';
import { Cpu, HeartPulse, Building2, PlayCircle, Loader2, TrendingUp, BarChart3 } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

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
          ...COMMON_STYLES.tabButton(currentSector === s.value),
          padding: '4px 10px',
          fontSize: TYPOGRAPHY.TINY,
        }}
      >
        {s.label}
      </button>
    ));
  };

  const renderResults = () => {
    if (!result) return null;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {/* Valuation Metrics */}
        {result.valuation_metrics && (
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
              <TrendingUp size={14} color={FINCEPT.ORANGE} />
              <span style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE }}>VALUATION METRICS</span>
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
              {Object.entries(result.valuation_metrics).map(([key, value]) => (
                <div key={key} style={{ padding: SPACING.SMALL, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px' }}>
                  <div style={COMMON_STYLES.dataLabel}>{key.replace(/_/g, ' ').toUpperCase()}</div>
                  <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                    {typeof value === 'number' ? value.toFixed(2) : String(value)}
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Benchmarks */}
        {result.benchmarks && (
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
              <BarChart3 size={14} color={FINCEPT.ORANGE} />
              <span style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE }}>INDUSTRY BENCHMARKS</span>
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              {Object.entries(result.benchmarks).map(([key, benchmark]: [string, any]) => (
                <div key={key} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: SPACING.SMALL, backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px' }}>
                  <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{key.replace(/_/g, ' ')}</span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT }}>
                    <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>25th: {benchmark.p25?.toFixed(1) ?? '-'}</span>
                    <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Median: {benchmark.median?.toFixed(1) ?? '-'}</span>
                    <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>75th: {benchmark.p75?.toFixed(1) ?? '-'}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Key Insights */}
        {result.insights && result.insights.length > 0 && (
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, color: FINCEPT.WHITE, marginBottom: SPACING.DEFAULT }}>KEY INSIGHTS</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              {result.insights.map((insight: string, idx: number) => (
                <div key={idx} style={{ display: 'flex', alignItems: 'flex-start', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                  <span style={{ color: FINCEPT.ORANGE }}>•</span>
                  {insight}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Raw Data */}
        <details>
          <summary style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, cursor: 'pointer' }}>View raw data</summary>
          <pre style={{
            marginTop: SPACING.SMALL,
            padding: SPACING.DEFAULT,
            borderRadius: '2px',
            overflow: 'auto',
            fontSize: TYPOGRAPHY.TINY,
            color: FINCEPT.GRAY,
            backgroundColor: FINCEPT.DARK_BG,
            maxHeight: 200,
          }}>
            {JSON.stringify(result, null, 2)}
          </pre>
        </details>
      </div>
    );
  };

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
          <BarChart3 size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>INDUSTRY METRICS</span>
          <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED }}>SECTOR-SPECIFIC VALUATION</span>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{ ...COMMON_STYLES.buttonPrimary, display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}
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
      }}>
        {([
          { id: 'tech', label: 'TECHNOLOGY', icon: Cpu },
          { id: 'healthcare', label: 'HEALTHCARE', icon: HeartPulse },
          { id: 'financial', label: 'FINANCIAL SERVICES', icon: Building2 },
        ] as const).map(item => {
          const Icon = item.icon;
          return (
            <button
              key={item.id}
              onClick={() => setIndustryType(item.id)}
              style={{
                ...COMMON_STYLES.tabButton(industryType === item.id),
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
      }}>
        {renderSectorTabs()}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
          {/* Input Panel */}
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
              {industryType === 'tech' && TECH_SECTORS.find(s => s.value === techSector)?.label}
              {industryType === 'healthcare' && HEALTHCARE_SECTORS.find(s => s.value === healthcareSector)?.label}
              {industryType === 'financial' && FINANCIAL_SECTORS.find(s => s.value === financialSector)?.label}
              {' '}INPUTS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
              {getCurrentFields().map(field => (
                <div key={field.key}>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, display: 'block', marginBottom: SPACING.TINY }}>{field.label}</label>
                  <input
                    type="number"
                    value={getCurrentInputs()[field.key] || 0}
                    onChange={e => updateInput(field.key, parseFloat(e.target.value) || 0)}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
              ))}
            </div>
          </div>

          {/* Results Panel */}
          <div style={{ ...COMMON_STYLES.metricCard }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>ANALYSIS RESULTS</div>
            {loading ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '200px' }}>
                <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
              </div>
            ) : result ? (
              renderResults()
            ) : (
              <div style={{ ...COMMON_STYLES.emptyState, height: '200px' }}>
                <BarChart3 size={32} style={{ opacity: 0.3, marginBottom: SPACING.SMALL }} />
                <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>Click "CALCULATE METRICS" to analyze</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default IndustryMetrics;
