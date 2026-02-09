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
  { value: 'saas', label: 'SaaS / Cloud', icon: '☁️' },
  { value: 'marketplace', label: 'Marketplace / Platform', icon: '🏪' },
  { value: 'semiconductor', label: 'Semiconductor', icon: '🔌' },
] as const;

const HEALTHCARE_SECTORS = [
  { value: 'pharma', label: 'Pharmaceuticals', icon: '💊' },
  { value: 'biotech', label: 'Biotechnology', icon: '🧬' },
  { value: 'devices', label: 'Medical Devices', icon: '🩺' },
  { value: 'services', label: 'Healthcare Services', icon: '🏥' },
] as const;

const FINANCIAL_SECTORS = [
  { value: 'banking', label: 'Banking', icon: '🏦' },
  { value: 'insurance', label: 'Insurance', icon: '🛡️' },
  { value: 'asset_management', label: 'Asset Management', icon: '📈' },
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

  const getCurrentSector = () => {
    if (industryType === 'tech') return techSector;
    if (industryType === 'healthcare') return healthcareSector;
    return financialSector;
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
    if (industryType === 'tech') {
      return TECH_SECTORS.map(s => (
        <button
          key={s.value}
          onClick={() => setTechSector(s.value as any)}
          className={`px-3 py-2 text-xs font-medium transition-all ${
            techSector === s.value
              ? 'text-white border-b-2'
              : 'text-gray-400 hover:text-white'
          }`}
          style={{ borderColor: techSector === s.value ? FINCEPT.ORANGE : 'transparent' }}
        >
          {s.icon} {s.label}
        </button>
      ));
    }
    if (industryType === 'healthcare') {
      return HEALTHCARE_SECTORS.map(s => (
        <button
          key={s.value}
          onClick={() => setHealthcareSector(s.value as any)}
          className={`px-3 py-2 text-xs font-medium transition-all ${
            healthcareSector === s.value
              ? 'text-white border-b-2'
              : 'text-gray-400 hover:text-white'
          }`}
          style={{ borderColor: healthcareSector === s.value ? FINCEPT.ORANGE : 'transparent' }}
        >
          {s.icon} {s.label}
        </button>
      ));
    }
    return FINANCIAL_SECTORS.map(s => (
      <button
        key={s.value}
        onClick={() => setFinancialSector(s.value as any)}
        className={`px-3 py-2 text-xs font-medium transition-all ${
          financialSector === s.value
            ? 'text-white border-b-2'
            : 'text-gray-400 hover:text-white'
        }`}
        style={{ borderColor: financialSector === s.value ? FINCEPT.ORANGE : 'transparent' }}
      >
        {s.icon} {s.label}
      </button>
    ));
  };

  const renderResults = () => {
    if (!result) return null;

    return (
      <div className="space-y-4">
        {/* Valuation Metrics */}
        {result.valuation_metrics && (
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h4 className="text-sm font-medium text-white mb-3 flex items-center gap-2">
              <TrendingUp size={14} style={{ color: FINCEPT.ORANGE }} />
              Valuation Metrics
            </h4>
            <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
              {Object.entries(result.valuation_metrics).map(([key, value]) => (
                <div key={key} className="p-3 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
                  <div className="text-xs text-gray-400">{key.replace(/_/g, ' ').toUpperCase()}</div>
                  <div className="text-lg font-bold text-white">
                    {typeof value === 'number' ? value.toFixed(2) : String(value)}
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Benchmarks */}
        {result.benchmarks && (
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h4 className="text-sm font-medium text-white mb-3 flex items-center gap-2">
              <BarChart3 size={14} style={{ color: FINCEPT.ORANGE }} />
              Industry Benchmarks
            </h4>
            <div className="space-y-2">
              {Object.entries(result.benchmarks).map(([key, benchmark]: [string, any]) => (
                <div key={key} className="flex items-center justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
                  <span className="text-xs text-gray-400">{key.replace(/_/g, ' ')}</span>
                  <div className="flex items-center gap-4">
                    <span className="text-xs text-gray-500">25th: {benchmark.p25?.toFixed(1) ?? '-'}</span>
                    <span className="text-xs text-gray-400">Median: {benchmark.median?.toFixed(1) ?? '-'}</span>
                    <span className="text-xs text-gray-500">75th: {benchmark.p75?.toFixed(1) ?? '-'}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Key Insights */}
        {result.insights && result.insights.length > 0 && (
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h4 className="text-sm font-medium text-white mb-3">Key Insights</h4>
            <ul className="space-y-2">
              {result.insights.map((insight: string, idx: number) => (
                <li key={idx} className="text-xs text-gray-300 flex items-start gap-2">
                  <span style={{ color: FINCEPT.ORANGE }}>•</span>
                  {insight}
                </li>
              ))}
            </ul>
          </div>
        )}

        {/* Raw Data */}
        <details className="text-xs">
          <summary className="text-gray-500 cursor-pointer hover:text-gray-400">View raw data</summary>
          <pre className="mt-2 p-3 rounded overflow-auto text-gray-400" style={{ backgroundColor: FINCEPT.CHARCOAL, maxHeight: 200 }}>
            {JSON.stringify(result, null, 2)}
          </pre>
        </details>
      </div>
    );
  };

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: FINCEPT.CHARCOAL }}>
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b" style={{ borderColor: FINCEPT.PANEL_BG }}>
        <div className="flex items-center gap-3">
          <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.ORANGE + '20' }}>
            <BarChart3 size={20} style={{ color: FINCEPT.ORANGE }} />
          </div>
          <div>
            <h2 className={TYPOGRAPHY.HEADING}>Industry Metrics</h2>
            <p className="text-xs text-gray-400">Sector-specific valuation metrics and benchmarks</p>
          </div>
        </div>
        <button
          onClick={runAnalysis}
          disabled={loading}
          className="flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all hover:opacity-90 disabled:opacity-50"
          style={{ backgroundColor: FINCEPT.ORANGE, color: '#000' }}
        >
          {loading ? <Loader2 size={16} className="animate-spin" /> : <PlayCircle size={16} />}
          Calculate Metrics
        </button>
      </div>

      {/* Industry Type Tabs */}
      <div className="flex items-center gap-2 px-4 py-2 border-b" style={{ borderColor: FINCEPT.PANEL_BG }}>
        <button
          onClick={() => setIndustryType('tech')}
          className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
            industryType === 'tech' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: industryType === 'tech' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <Cpu size={16} /> Technology
        </button>
        <button
          onClick={() => setIndustryType('healthcare')}
          className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
            industryType === 'healthcare' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: industryType === 'healthcare' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <HeartPulse size={16} /> Healthcare
        </button>
        <button
          onClick={() => setIndustryType('financial')}
          className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
            industryType === 'financial' ? 'text-white' : 'text-gray-400 hover:text-white'
          }`}
          style={{ backgroundColor: industryType === 'financial' ? FINCEPT.PANEL_BG : 'transparent' }}
        >
          <Building2 size={16} /> Financial Services
        </button>
      </div>

      {/* Sector Sub-tabs */}
      <div className="flex items-center gap-1 px-4 py-1 border-b overflow-x-auto" style={{ borderColor: FINCEPT.PANEL_BG }}>
        {renderSectorTabs()}
      </div>

      {/* Content */}
      <div className="flex-1 overflow-auto p-4">
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
          {/* Input Panel */}
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h3 className="text-sm font-medium text-white mb-4">
              {industryType === 'tech' && TECH_SECTORS.find(s => s.value === techSector)?.label}
              {industryType === 'healthcare' && HEALTHCARE_SECTORS.find(s => s.value === healthcareSector)?.label}
              {industryType === 'financial' && FINANCIAL_SECTORS.find(s => s.value === financialSector)?.label}
              {' '}Inputs
            </h3>
            <div className="space-y-3">
              {getCurrentFields().map(field => (
                <div key={field.key}>
                  <label className="text-xs text-gray-400 mb-1 block">{field.label}</label>
                  <input
                    type="number"
                    value={getCurrentInputs()[field.key] || 0}
                    onChange={e => updateInput(field.key, parseFloat(e.target.value) || 0)}
                    className="w-full px-3 py-2 rounded text-sm text-white"
                    style={{ backgroundColor: FINCEPT.CHARCOAL, border: `1px solid ${FINCEPT.CHARCOAL}` }}
                  />
                </div>
              ))}
            </div>
          </div>

          {/* Results Panel */}
          <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
            <h3 className="text-sm font-medium text-white mb-4">Analysis Results</h3>
            {loading ? (
              <div className="flex items-center justify-center h-48">
                <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
              </div>
            ) : result ? (
              renderResults()
            ) : (
              <div className="flex flex-col items-center justify-center h-48 text-gray-500 text-sm">
                <BarChart3 size={32} className="mb-2 opacity-50" />
                <p>Click "Calculate Metrics" to analyze</p>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default IndustryMetrics;
