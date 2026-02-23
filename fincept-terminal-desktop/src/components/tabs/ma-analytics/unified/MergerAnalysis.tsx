/**
 * Merger Analysis - Unified Accretion/Dilution, Synergies, Deal Structure, Pro Forma
 *
 * Complete merger analysis toolkit with advanced deal structures
 * Bloomberg-grade UI/UX with horizontal tab selectors, collapsible dense input grids, and rich chart visualizations
 */

import React, { useState } from 'react';
import { GitMerge, Zap, DollarSign, PlayCircle, TrendingUp, TrendingDown, Target, Percent, Shield, FileText, Layers } from 'lucide-react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Cell, PieChart, Pie, Legend, ResponsiveContainer } from 'recharts';
import { ChevronDown, ChevronUp } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MAExportButton } from '../components';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';

type AnalysisType = 'accretion' | 'synergies' | 'structure' | 'proforma';
type StructureSubTab = 'payment' | 'earnout' | 'exchange' | 'collar' | 'cvr';
type ProFormaSubTab = 'proforma' | 'sources_uses' | 'contribution';

export const MergerAnalysis: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('accretion');
  const [structureSubTab, setStructureSubTab] = useState<StructureSubTab>('payment');
  const [proformaSubTab, setProformaSubTab] = useState<ProFormaSubTab>('proforma');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [inputsCollapsed, setInputsCollapsed] = useState(false);

  // Accretion/Dilution Inputs
  const [accretionInputs, setAccretionInputs] = useState({
    acquirerRevenue: 10000,
    acquirerEBITDA: 2000,
    acquirerNetIncome: 1000,
    acquirerShares: 100,
    acquirerEPS: 10,
    targetRevenue: 3000,
    targetEBITDA: 600,
    targetNetIncome: 300,
    purchasePrice: 5000,
    cashPct: 50,
    stockPct: 50,
    costSynergies: 0,
    revenueSynergies: 0,
    integrationCosts: 0,
    taxRate: 21,
  });

  // Synergies Inputs
  const [synergiesInputs, setSynergiesInputs] = useState({
    synergyType: 'revenue' as 'revenue' | 'cost',
    baseRevenue: 5000,
    crossSellRate: 0.15,
    years: 5,
  });

  // Payment Structure Inputs
  const [structureInputs, setStructureInputs] = useState({
    purchasePrice: 1000,
    cashPct: 60,
    acquirerCash: 500,
    debtCapacity: 400,
  });

  // Earnout Inputs
  const [earnoutInputs, setEarnoutInputs] = useState({
    maxEarnout: 200,
    revenueTarget: 500,
    ebitdaTarget: 100,
    earnoutPeriod: 3,
    baseRevenue: 400,
    revenueGrowth: 0.10,
    baseEbitda: 80,
    ebitdaGrowth: 0.08,
  });

  // Exchange Ratio Inputs
  const [exchangeInputs, setExchangeInputs] = useState({
    acquirerPrice: 50,
    targetPrice: 30,
    offerPremium: 0.25,
  });

  // Collar Inputs
  const [collarInputs, setCollarInputs] = useState({
    baseRatio: 0.8,
    floorPrice: 45,
    capPrice: 55,
    targetShares: 50,
    priceScenarios: '40,45,50,55,60,65',
  });

  // CVR Inputs
  const [cvrInputs, setCvrInputs] = useState({
    cvrType: 'milestone' as 'milestone' | 'earnout' | 'litigation',
    milestone: 'FDA_APPROVAL',
    probability: 0.60,
    payoutAmount: 5.00,
    expectedDate: '2025-12-31',
    discountRate: 0.12,
  });

  // Pro Forma Inputs
  const [proformaInputs, setProformaInputs] = useState({
    acquirerRevenue: 10000,
    acquirerEbitda: 2000,
    acquirerNetIncome: 1000,
    acquirerShares: 500,
    targetRevenue: 3000,
    targetEbitda: 600,
    targetNetIncome: 300,
    targetEnterpriseValue: 4800,
    projectionYear: 1,
  });

  // Sources & Uses Inputs
  const [sourcesUsesInputs, setSourcesUsesInputs] = useState({
    purchasePrice: 5000,
    targetDebt: 500,
    transactionFees: 100,
    financingFees: 50,
    cashOnHand: 1000,
    newDebt: 2000,
    stockIssuance: 2650,
  });

  // Contribution Analysis Inputs
  const [contributionInputs, setContributionInputs] = useState({
    acquirerRevenue: 10000,
    acquirerEbitda: 2000,
    acquirerNetIncome: 1000,
    acquirerAssets: 15000,
    targetRevenue: 3000,
    targetEbitda: 600,
    targetNetIncome: 300,
    targetAssets: 4000,
    ownershipSplit: 0.77,
  });

  const handleRunAccretion = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.MergerModel.buildMergerModel({
        acquirer_data: {
          revenue: accretionInputs.acquirerRevenue * 1000000,
          ebitda: accretionInputs.acquirerEBITDA * 1000000,
          ebit: accretionInputs.acquirerEBITDA * 1000000 * 0.8,
          net_income: accretionInputs.acquirerNetIncome * 1000000,
          shares_outstanding: accretionInputs.acquirerShares * 1000000,
          eps: accretionInputs.acquirerEPS,
          market_cap: accretionInputs.acquirerShares * 1000000 * (accretionInputs.acquirerEPS * 15),
        },
        target_data: {
          revenue: accretionInputs.targetRevenue * 1000000,
          ebitda: accretionInputs.targetEBITDA * 1000000,
          ebit: accretionInputs.targetEBITDA * 1000000 * 0.8,
          net_income: accretionInputs.targetNetIncome * 1000000,
          shares_outstanding: 30000000,
          eps: accretionInputs.targetNetIncome / 30,
          market_cap: accretionInputs.purchasePrice * 1000000,
        },
        deal_structure: {
          purchase_price: accretionInputs.purchasePrice * 1000000,
          payment_cash_pct: accretionInputs.cashPct,
          payment_stock_pct: accretionInputs.stockPct,
          cost_synergies: accretionInputs.costSynergies * 1000000,
          revenue_synergies: accretionInputs.revenueSynergies * 1000000,
          integration_costs: accretionInputs.integrationCosts * 1000000,
          tax_rate: accretionInputs.taxRate / 100,
        },
      });
      setResult(res);
      showSuccess('Merger Analysis Complete', [
        { label: 'TYPE', value: res.eps_accretion_pct > 0 ? 'ACCRETIVE' : 'DILUTIVE' },
        { label: 'EPS ACCRETION', value: `${res.eps_accretion_pct?.toFixed(1)}%` }
      ]);
    } catch (error) {
      showError('Analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunSynergies = async () => {
    setLoading(true);
    try {
      const params = {
        base_revenue: synergiesInputs.baseRevenue * 1000000,
        cross_sell_rate: synergiesInputs.crossSellRate,
        years: synergiesInputs.years,
      };

      const res = synergiesInputs.synergyType === 'revenue'
        ? await MAAnalyticsService.Synergies.calculateRevenueSynergies('cross_selling', params)
        : await MAAnalyticsService.Synergies.calculateCostSynergies('headcount_reduction', params);

      setResult(res);
      showSuccess(`${synergiesInputs.synergyType === 'revenue' ? 'Revenue' : 'Cost'} Synergies Calculated`);
    } catch (error) {
      showError('Synergies calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunPaymentStructure = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.DealStructure.analyzePaymentStructure(
        structureInputs.purchasePrice * 1000000,
        structureInputs.cashPct,
        structureInputs.acquirerCash * 1000000,
        structureInputs.debtCapacity * 1000000
      );
      setResult(res);
      showSuccess('Payment Structure Analyzed');
    } catch (error) {
      showError('Structure analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunEarnout = async () => {
    setLoading(true);
    try {
      const earnoutParams = {
        max_earnout: earnoutInputs.maxEarnout * 1000000,
        revenue_target: earnoutInputs.revenueTarget * 1000000,
        ebitda_target: earnoutInputs.ebitdaTarget * 1000000,
        earnout_period: earnoutInputs.earnoutPeriod,
        payment_schedule: 'annual',
      };

      const financialProjections = {
        revenue: Array.from({ length: earnoutInputs.earnoutPeriod }, (_, i) =>
          earnoutInputs.baseRevenue * Math.pow(1 + earnoutInputs.revenueGrowth, i + 1) * 1000000
        ),
        ebitda: Array.from({ length: earnoutInputs.earnoutPeriod }, (_, i) =>
          earnoutInputs.baseEbitda * Math.pow(1 + earnoutInputs.ebitdaGrowth, i + 1) * 1000000
        ),
      };

      const res = await MAAnalyticsService.DealStructure.valueEarnout(earnoutParams, financialProjections);
      setResult(res);
      showSuccess('Earnout Valued', [
        { label: 'EXPECTED VALUE', value: formatCurrency(res?.total_earnout_expected_value || 0) }
      ]);
    } catch (error) {
      showError('Earnout valuation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunExchangeRatio = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.DealStructure.calculateExchangeRatio(
        exchangeInputs.acquirerPrice,
        exchangeInputs.targetPrice,
        exchangeInputs.offerPremium
      );
      setResult(res);
      showSuccess('Exchange Ratio Calculated', [
        { label: 'RATIO', value: `${res?.exchange_ratio?.toFixed(4)}x` }
      ]);
    } catch (error) {
      showError('Exchange ratio calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunCollar = async () => {
    setLoading(true);
    try {
      const collarParams = {
        base_exchange_ratio: collarInputs.baseRatio,
        floor_price: collarInputs.floorPrice,
        cap_price: collarInputs.capPrice,
        target_shares: collarInputs.targetShares * 1000000,
        collar_type: 'fixed_value',
      };

      const priceScenarios = {
        acquirer_prices: collarInputs.priceScenarios.split(',').map(v => parseFloat(v.trim())),
      };

      const res = await MAAnalyticsService.DealStructure.analyzeCollarMechanism(collarParams, priceScenarios);
      setResult(res);
      showSuccess('Collar Analysis Complete');
    } catch (error) {
      showError('Collar analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunCVR = async () => {
    setLoading(true);
    try {
      const cvrParams = {
        milestone: cvrInputs.milestone,
        probability: cvrInputs.probability,
        payout_amount: cvrInputs.payoutAmount,
        expected_date: cvrInputs.expectedDate,
        discount_rate: cvrInputs.discountRate,
      };

      const res = await MAAnalyticsService.DealStructure.valueCVR(cvrInputs.cvrType, cvrParams);
      setResult(res);
      showSuccess('CVR Valued', [
        { label: 'EXPECTED VALUE', value: `$${res?.total_expected_value?.toFixed(2)}` }
      ]);
    } catch (error) {
      showError('CVR valuation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunProForma = async () => {
    setLoading(true);
    try {
      const acquirerData = {
        revenue: proformaInputs.acquirerRevenue * 1000000,
        ebitda: proformaInputs.acquirerEbitda * 1000000,
        net_income: proformaInputs.acquirerNetIncome * 1000000,
        shares_outstanding: proformaInputs.acquirerShares * 1000000,
      };

      const targetData = {
        revenue: proformaInputs.targetRevenue * 1000000,
        ebitda: proformaInputs.targetEbitda * 1000000,
        net_income: proformaInputs.targetNetIncome * 1000000,
        enterprise_value: proformaInputs.targetEnterpriseValue * 1000000,
      };

      const res = await MAAnalyticsService.MergerModel.buildProForma(acquirerData, targetData, proformaInputs.projectionYear);
      setResult(res);
      showSuccess('Pro Forma Built', [
        { label: 'COMBINED REVENUE', value: formatCurrency(res.data?.combined_revenue || 0) }
      ]);
    } catch (error) {
      showError('Pro Forma failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunSourcesUses = async () => {
    setLoading(true);
    try {
      const dealStructure = {
        purchase_price: sourcesUsesInputs.purchasePrice * 1000000,
        target_debt: sourcesUsesInputs.targetDebt * 1000000,
        transaction_fees: sourcesUsesInputs.transactionFees * 1000000,
        financing_fees: sourcesUsesInputs.financingFees * 1000000,
      };

      const financingStructure = {
        cash_on_hand: sourcesUsesInputs.cashOnHand * 1000000,
        new_debt: sourcesUsesInputs.newDebt * 1000000,
        stock_issuance: sourcesUsesInputs.stockIssuance * 1000000,
      };

      const res = await MAAnalyticsService.MergerModel.calculateSourcesUses(dealStructure, financingStructure);
      setResult(res);
      showSuccess('Sources & Uses Calculated');
    } catch (error) {
      showError('Sources & Uses failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunContribution = async () => {
    setLoading(true);
    try {
      const acquirerData = {
        revenue: contributionInputs.acquirerRevenue * 1000000,
        ebitda: contributionInputs.acquirerEbitda * 1000000,
        net_income: contributionInputs.acquirerNetIncome * 1000000,
        total_assets: contributionInputs.acquirerAssets * 1000000,
      };

      const targetData = {
        revenue: contributionInputs.targetRevenue * 1000000,
        ebitda: contributionInputs.targetEbitda * 1000000,
        net_income: contributionInputs.targetNetIncome * 1000000,
        total_assets: contributionInputs.targetAssets * 1000000,
      };

      const res = await MAAnalyticsService.MergerModel.analyzeContribution(
        acquirerData,
        targetData,
        contributionInputs.ownershipSplit
      );
      setResult(res);
      showSuccess('Contribution Analysis Complete');
    } catch (error) {
      showError('Contribution analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    if (analysisType === 'structure') {
      switch (structureSubTab) {
        case 'payment': return handleRunPaymentStructure();
        case 'earnout': return handleRunEarnout();
        case 'exchange': return handleRunExchangeRatio();
        case 'collar': return handleRunCollar();
        case 'cvr': return handleRunCVR();
      }
    }
    if (analysisType === 'proforma') {
      switch (proformaSubTab) {
        case 'proforma': return handleRunProForma();
        case 'sources_uses': return handleRunSourcesUses();
        case 'contribution': return handleRunContribution();
      }
    }
    switch (analysisType) {
      case 'accretion': return handleRunAccretion();
      case 'synergies': return handleRunSynergies();
    }
  };

  const formatCurrency = (value: number) => {
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  // ---------------------------------------------------------------------------
  //  Input field renderer -- dense 3-column grid compatible
  // ---------------------------------------------------------------------------
  const renderInputField = (
    label: string,
    value: number | string,
    onChange: (val: string) => void,
    placeholder?: string
  ) => (
    <div style={{ minWidth: 0 }}>
      <label style={{
        display: 'block',
        fontSize: '8px',
        fontFamily: TYPOGRAPHY.MONO,
        fontWeight: TYPOGRAPHY.BOLD,
        color: FINCEPT.MUTED,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '2px',
        whiteSpace: 'nowrap' as const,
        overflow: 'hidden',
        textOverflow: 'ellipsis',
      }}>
        {label}
      </label>
      <input
        type="text"
        inputMode="decimal"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          ...COMMON_STYLES.inputField,
          padding: '5px 8px',
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
        }}
        onFocus={(e) => { e.currentTarget.style.borderColor = MA_COLORS.merger; }}
        onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
      />
    </div>
  );

  // ---------------------------------------------------------------------------
  //  Active analysis title
  // ---------------------------------------------------------------------------
  const getActiveTitle = (): string => {
    if (analysisType === 'accretion') return 'ACCRETION / DILUTION ANALYSIS';
    if (analysisType === 'synergies') return `${synergiesInputs.synergyType.toUpperCase()} SYNERGIES`;
    if (analysisType === 'structure') {
      const map: Record<StructureSubTab, string> = {
        payment: 'PAYMENT STRUCTURE', earnout: 'EARNOUT VALUATION',
        exchange: 'EXCHANGE RATIO', collar: 'COLLAR MECHANISM', cvr: 'CVR VALUATION',
      };
      return map[structureSubTab];
    }
    const pfMap: Record<ProFormaSubTab, string> = {
      proforma: 'PRO FORMA FINANCIALS', sources_uses: 'SOURCES & USES', contribution: 'CONTRIBUTION ANALYSIS',
    };
    return pfMap[proformaSubTab];
  };

  // ---------------------------------------------------------------------------
  //  ANALYSIS TYPE TAB DATA
  // ---------------------------------------------------------------------------
  const analysisTypeTabs: { id: AnalysisType; label: string; icon: React.ElementType }[] = [
    { id: 'accretion', label: 'ACCRETION/DILUTION', icon: GitMerge },
    { id: 'synergies', label: 'SYNERGIES', icon: Zap },
    { id: 'structure', label: 'DEAL STRUCTURE', icon: DollarSign },
    { id: 'proforma', label: 'PRO FORMA', icon: FileText },
  ];

  const structureSubTabs: { id: StructureSubTab; label: string; icon: React.ElementType }[] = [
    { id: 'payment', label: 'PAYMENT', icon: DollarSign },
    { id: 'earnout', label: 'EARNOUT', icon: Target },
    { id: 'exchange', label: 'EXCHANGE', icon: Percent },
    { id: 'collar', label: 'COLLAR', icon: Shield },
    { id: 'cvr', label: 'CVR', icon: FileText },
  ];

  const proformaSubTabs: { id: ProFormaSubTab; label: string; icon: React.ElementType }[] = [
    { id: 'proforma', label: 'PRO FORMA', icon: FileText },
    { id: 'sources_uses', label: 'SOURCES/USES', icon: Layers },
    { id: 'contribution', label: 'CONTRIBUTION', icon: GitMerge },
  ];

  // ---------------------------------------------------------------------------
  //  TAB BUTTON STYLE (uses merger cyan accent)
  // ---------------------------------------------------------------------------
  const tabBtn = (active: boolean, accent: string = MA_COLORS.merger) => ({
    display: 'flex' as const,
    alignItems: 'center' as const,
    gap: '5px',
    padding: '6px 14px',
    backgroundColor: active ? accent : 'transparent',
    color: active ? FINCEPT.DARK_BG : FINCEPT.GRAY,
    border: active ? 'none' : `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '9px',
    fontFamily: TYPOGRAPHY.MONO,
    fontWeight: TYPOGRAPHY.BOLD,
    letterSpacing: '0.5px',
    cursor: 'pointer' as const,
    transition: 'all 0.15s ease',
    textTransform: 'uppercase' as const,
    whiteSpace: 'nowrap' as const,
  });

  const subTabBtn = (active: boolean) => ({
    ...tabBtn(active, MA_COLORS.merger),
    padding: '4px 10px',
    fontSize: '8px',
    backgroundColor: active ? `${MA_COLORS.merger}25` : 'transparent',
    color: active ? MA_COLORS.merger : FINCEPT.MUTED,
    border: active ? `1px solid ${MA_COLORS.merger}` : `1px solid transparent`,
  });

  // ---------------------------------------------------------------------------
  //  RENDER INPUT FORMS
  // ---------------------------------------------------------------------------
  const renderInputForms = () => {
    // Accretion/Dilution
    if (analysisType === 'accretion') return (
      <>
        <MASectionHeader title="Acquirer ($M)" accentColor={MA_COLORS.merger} icon={<TrendingUp size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Revenue', accretionInputs.acquirerRevenue, (v) => setAccretionInputs({ ...accretionInputs, acquirerRevenue: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA', accretionInputs.acquirerEBITDA, (v) => setAccretionInputs({ ...accretionInputs, acquirerEBITDA: parseFloat(v) || 0 }))}
          {renderInputField('Net Income', accretionInputs.acquirerNetIncome, (v) => setAccretionInputs({ ...accretionInputs, acquirerNetIncome: parseFloat(v) || 0 }))}
          {renderInputField('Shares (M)', accretionInputs.acquirerShares, (v) => setAccretionInputs({ ...accretionInputs, acquirerShares: parseFloat(v) || 0 }))}
          {renderInputField('EPS', accretionInputs.acquirerEPS, (v) => setAccretionInputs({ ...accretionInputs, acquirerEPS: parseFloat(v) || 0 }))}
        </div>
        <MASectionHeader title="Target ($M)" accentColor={CHART_PALETTE[2]} icon={<Target size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Revenue', accretionInputs.targetRevenue, (v) => setAccretionInputs({ ...accretionInputs, targetRevenue: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA', accretionInputs.targetEBITDA, (v) => setAccretionInputs({ ...accretionInputs, targetEBITDA: parseFloat(v) || 0 }))}
          {renderInputField('Net Income', accretionInputs.targetNetIncome, (v) => setAccretionInputs({ ...accretionInputs, targetNetIncome: parseFloat(v) || 0 }))}
          {renderInputField('Purchase Price', accretionInputs.purchasePrice, (v) => setAccretionInputs({ ...accretionInputs, purchasePrice: parseFloat(v) || 0 }))}
          {renderInputField('Cash %', accretionInputs.cashPct, (v) => { const cash = parseFloat(v) || 0; setAccretionInputs({ ...accretionInputs, cashPct: cash, stockPct: 100 - cash }); })}
          {renderInputField('Stock %', accretionInputs.stockPct, () => {}, `${accretionInputs.stockPct}`)}
        </div>
        <MASectionHeader title="Deal Terms" accentColor={CHART_PALETTE[3]} icon={<Zap size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Cost Synergies ($M)', accretionInputs.costSynergies, (v) => setAccretionInputs({ ...accretionInputs, costSynergies: parseFloat(v) || 0 }))}
          {renderInputField('Rev Synergies ($M)', accretionInputs.revenueSynergies, (v) => setAccretionInputs({ ...accretionInputs, revenueSynergies: parseFloat(v) || 0 }))}
          {renderInputField('Integration Costs ($M)', accretionInputs.integrationCosts, (v) => setAccretionInputs({ ...accretionInputs, integrationCosts: parseFloat(v) || 0 }))}
          {renderInputField('Tax Rate (%)', accretionInputs.taxRate, (v) => setAccretionInputs({ ...accretionInputs, taxRate: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    // Synergies
    if (analysisType === 'synergies') return (
      <>
        <MASectionHeader title="Synergy Configuration" accentColor={MA_COLORS.merger} icon={<Zap size={12} />} />
        <div style={{ display: 'flex', gap: '6px', marginBottom: '12px' }}>
          <button
            onClick={() => setSynergiesInputs({ ...synergiesInputs, synergyType: 'revenue' })}
            style={subTabBtn(synergiesInputs.synergyType === 'revenue')}
          >REVENUE</button>
          <button
            onClick={() => setSynergiesInputs({ ...synergiesInputs, synergyType: 'cost' })}
            style={subTabBtn(synergiesInputs.synergyType === 'cost')}
          >COST</button>
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Base Revenue ($M)', synergiesInputs.baseRevenue, (v) => setSynergiesInputs({ ...synergiesInputs, baseRevenue: parseFloat(v) || 0 }))}
          {renderInputField('Cross-Sell Rate', synergiesInputs.crossSellRate, (v) => setSynergiesInputs({ ...synergiesInputs, crossSellRate: parseFloat(v) || 0 }))}
          {renderInputField('Projection Years', synergiesInputs.years, (v) => setSynergiesInputs({ ...synergiesInputs, years: parseInt(v) || 0 }))}
        </div>
      </>
    );

    // Structure - Payment
    if (analysisType === 'structure' && structureSubTab === 'payment') return (
      <>
        <MASectionHeader title="Payment Structure ($M)" accentColor={MA_COLORS.merger} icon={<DollarSign size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInputField('Purchase Price', structureInputs.purchasePrice, (v) => setStructureInputs({ ...structureInputs, purchasePrice: parseFloat(v) || 0 }))}
          {renderInputField('Cash %', structureInputs.cashPct, (v) => setStructureInputs({ ...structureInputs, cashPct: parseFloat(v) || 0 }))}
          {renderInputField('Acquirer Cash Available', structureInputs.acquirerCash, (v) => setStructureInputs({ ...structureInputs, acquirerCash: parseFloat(v) || 0 }))}
          {renderInputField('Debt Capacity', structureInputs.debtCapacity, (v) => setStructureInputs({ ...structureInputs, debtCapacity: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    // Structure - Earnout
    if (analysisType === 'structure' && structureSubTab === 'earnout') return (
      <>
        <MASectionHeader title="Earnout Terms ($M)" accentColor={MA_COLORS.merger} icon={<Target size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Max Earnout', earnoutInputs.maxEarnout, (v) => setEarnoutInputs({ ...earnoutInputs, maxEarnout: parseFloat(v) || 0 }))}
          {renderInputField('Revenue Target', earnoutInputs.revenueTarget, (v) => setEarnoutInputs({ ...earnoutInputs, revenueTarget: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA Target', earnoutInputs.ebitdaTarget, (v) => setEarnoutInputs({ ...earnoutInputs, ebitdaTarget: parseFloat(v) || 0 }))}
          {renderInputField('Earnout Period (Yrs)', earnoutInputs.earnoutPeriod, (v) => setEarnoutInputs({ ...earnoutInputs, earnoutPeriod: parseInt(v) || 0 }))}
        </div>
        <MASectionHeader title="Projections" accentColor={CHART_PALETTE[3]} icon={<TrendingUp size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInputField('Base Revenue ($M)', earnoutInputs.baseRevenue, (v) => setEarnoutInputs({ ...earnoutInputs, baseRevenue: parseFloat(v) || 0 }))}
          {renderInputField('Revenue Growth Rate', earnoutInputs.revenueGrowth, (v) => setEarnoutInputs({ ...earnoutInputs, revenueGrowth: parseFloat(v) || 0 }))}
          {renderInputField('Base EBITDA ($M)', earnoutInputs.baseEbitda, (v) => setEarnoutInputs({ ...earnoutInputs, baseEbitda: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA Growth Rate', earnoutInputs.ebitdaGrowth, (v) => setEarnoutInputs({ ...earnoutInputs, ebitdaGrowth: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    // Structure - Exchange Ratio
    if (analysisType === 'structure' && structureSubTab === 'exchange') return (
      <>
        <MASectionHeader title="Exchange Ratio Inputs" accentColor={MA_COLORS.merger} icon={<Percent size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Acquirer Price', exchangeInputs.acquirerPrice, (v) => setExchangeInputs({ ...exchangeInputs, acquirerPrice: parseFloat(v) || 0 }))}
          {renderInputField('Target Price', exchangeInputs.targetPrice, (v) => setExchangeInputs({ ...exchangeInputs, targetPrice: parseFloat(v) || 0 }))}
          {renderInputField('Offer Premium', exchangeInputs.offerPremium, (v) => setExchangeInputs({ ...exchangeInputs, offerPremium: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    // Structure - Collar
    if (analysisType === 'structure' && structureSubTab === 'collar') return (
      <>
        <MASectionHeader title="Collar Mechanism" accentColor={MA_COLORS.merger} icon={<Shield size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Base Exchange Ratio', collarInputs.baseRatio, (v) => setCollarInputs({ ...collarInputs, baseRatio: parseFloat(v) || 0 }))}
          {renderInputField('Floor Price', collarInputs.floorPrice, (v) => setCollarInputs({ ...collarInputs, floorPrice: parseFloat(v) || 0 }))}
          {renderInputField('Cap Price', collarInputs.capPrice, (v) => setCollarInputs({ ...collarInputs, capPrice: parseFloat(v) || 0 }))}
          {renderInputField('Target Shares (M)', collarInputs.targetShares, (v) => setCollarInputs({ ...collarInputs, targetShares: parseFloat(v) || 0 }))}
          {renderInputField('Price Scenarios', collarInputs.priceScenarios, (v) => setCollarInputs({ ...collarInputs, priceScenarios: v }), '40,45,50,55,60')}
        </div>
      </>
    );

    // Structure - CVR
    if (analysisType === 'structure' && structureSubTab === 'cvr') return (
      <>
        <MASectionHeader title="CVR Type" accentColor={MA_COLORS.merger} icon={<FileText size={12} />} />
        <div style={{ display: 'flex', gap: '6px', marginBottom: '12px' }}>
          {(['milestone', 'earnout', 'litigation'] as const).map((type) => (
            <button
              key={type}
              onClick={() => setCvrInputs({ ...cvrInputs, cvrType: type })}
              style={subTabBtn(cvrInputs.cvrType === type)}
            >
              {type.toUpperCase()}
            </button>
          ))}
        </div>
        <MASectionHeader title="CVR Parameters" accentColor={CHART_PALETTE[2]} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Milestone', cvrInputs.milestone, (v) => setCvrInputs({ ...cvrInputs, milestone: v }), 'FDA_APPROVAL')}
          {renderInputField('Probability', cvrInputs.probability, (v) => setCvrInputs({ ...cvrInputs, probability: parseFloat(v) || 0 }))}
          {renderInputField('Payout Amount ($)', cvrInputs.payoutAmount, (v) => setCvrInputs({ ...cvrInputs, payoutAmount: parseFloat(v) || 0 }))}
          {renderInputField('Expected Date', cvrInputs.expectedDate, (v) => setCvrInputs({ ...cvrInputs, expectedDate: v }), 'YYYY-MM-DD')}
          {renderInputField('Discount Rate', cvrInputs.discountRate, (v) => setCvrInputs({ ...cvrInputs, discountRate: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    // Pro Forma
    if (analysisType === 'proforma' && proformaSubTab === 'proforma') return (
      <>
        <MASectionHeader title="Acquirer ($M)" accentColor={MA_COLORS.merger} icon={<TrendingUp size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Revenue', proformaInputs.acquirerRevenue, (v) => setProformaInputs({ ...proformaInputs, acquirerRevenue: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA', proformaInputs.acquirerEbitda, (v) => setProformaInputs({ ...proformaInputs, acquirerEbitda: parseFloat(v) || 0 }))}
          {renderInputField('Net Income', proformaInputs.acquirerNetIncome, (v) => setProformaInputs({ ...proformaInputs, acquirerNetIncome: parseFloat(v) || 0 }))}
          {renderInputField('Shares (M)', proformaInputs.acquirerShares, (v) => setProformaInputs({ ...proformaInputs, acquirerShares: parseFloat(v) || 0 }))}
        </div>
        <MASectionHeader title="Target ($M)" accentColor={CHART_PALETTE[2]} icon={<Target size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Revenue', proformaInputs.targetRevenue, (v) => setProformaInputs({ ...proformaInputs, targetRevenue: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA', proformaInputs.targetEbitda, (v) => setProformaInputs({ ...proformaInputs, targetEbitda: parseFloat(v) || 0 }))}
          {renderInputField('Net Income', proformaInputs.targetNetIncome, (v) => setProformaInputs({ ...proformaInputs, targetNetIncome: parseFloat(v) || 0 }))}
          {renderInputField('Enterprise Value', proformaInputs.targetEnterpriseValue, (v) => setProformaInputs({ ...proformaInputs, targetEnterpriseValue: parseFloat(v) || 0 }))}
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Projection Year', proformaInputs.projectionYear, (v) => setProformaInputs({ ...proformaInputs, projectionYear: parseInt(v) || 1 }))}
        </div>
      </>
    );

    // Sources & Uses
    if (analysisType === 'proforma' && proformaSubTab === 'sources_uses') return (
      <>
        <MASectionHeader title="Uses of Funds ($M)" accentColor={FINCEPT.RED} icon={<TrendingDown size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Purchase Price', sourcesUsesInputs.purchasePrice, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, purchasePrice: parseFloat(v) || 0 }))}
          {renderInputField('Target Debt Refinance', sourcesUsesInputs.targetDebt, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, targetDebt: parseFloat(v) || 0 }))}
          {renderInputField('Transaction Fees', sourcesUsesInputs.transactionFees, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, transactionFees: parseFloat(v) || 0 }))}
          {renderInputField('Financing Fees', sourcesUsesInputs.financingFees, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, financingFees: parseFloat(v) || 0 }))}
        </div>
        <MASectionHeader title="Sources of Funds ($M)" accentColor={FINCEPT.GREEN} icon={<TrendingUp size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Cash on Hand', sourcesUsesInputs.cashOnHand, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, cashOnHand: parseFloat(v) || 0 }))}
          {renderInputField('New Debt', sourcesUsesInputs.newDebt, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, newDebt: parseFloat(v) || 0 }))}
          {renderInputField('Stock Issuance', sourcesUsesInputs.stockIssuance, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, stockIssuance: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    // Contribution Analysis
    if (analysisType === 'proforma' && proformaSubTab === 'contribution') return (
      <>
        <MASectionHeader title="Acquirer ($M)" accentColor={MA_COLORS.merger} icon={<TrendingUp size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Revenue', contributionInputs.acquirerRevenue, (v) => setContributionInputs({ ...contributionInputs, acquirerRevenue: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA', contributionInputs.acquirerEbitda, (v) => setContributionInputs({ ...contributionInputs, acquirerEbitda: parseFloat(v) || 0 }))}
          {renderInputField('Net Income', contributionInputs.acquirerNetIncome, (v) => setContributionInputs({ ...contributionInputs, acquirerNetIncome: parseFloat(v) || 0 }))}
          {renderInputField('Total Assets', contributionInputs.acquirerAssets, (v) => setContributionInputs({ ...contributionInputs, acquirerAssets: parseFloat(v) || 0 }))}
        </div>
        <MASectionHeader title="Target ($M)" accentColor={CHART_PALETTE[2]} icon={<Target size={12} />} />
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '14px' }}>
          {renderInputField('Revenue', contributionInputs.targetRevenue, (v) => setContributionInputs({ ...contributionInputs, targetRevenue: parseFloat(v) || 0 }))}
          {renderInputField('EBITDA', contributionInputs.targetEbitda, (v) => setContributionInputs({ ...contributionInputs, targetEbitda: parseFloat(v) || 0 }))}
          {renderInputField('Net Income', contributionInputs.targetNetIncome, (v) => setContributionInputs({ ...contributionInputs, targetNetIncome: parseFloat(v) || 0 }))}
          {renderInputField('Total Assets', contributionInputs.targetAssets, (v) => setContributionInputs({ ...contributionInputs, targetAssets: parseFloat(v) || 0 }))}
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInputField('Acquirer Ownership %', contributionInputs.ownershipSplit, (v) => setContributionInputs({ ...contributionInputs, ownershipSplit: parseFloat(v) || 0 }))}
        </div>
      </>
    );

    return null;
  };

  // ---------------------------------------------------------------------------
  //  RENDER RESULTS
  // ---------------------------------------------------------------------------
  const renderResults = () => {
    if (!result) return null;

    // Accretion / Dilution Results
    if (analysisType === 'accretion' && result.pro_forma_eps !== undefined) {
      const isAccretive = result.eps_accretion_pct > 0;
      const epsChange = result.pro_forma_eps - (result.standalone_eps || accretionInputs.acquirerEPS);
      const waterfallData = [
        { name: 'Acquirer EPS', value: result.standalone_eps || accretionInputs.acquirerEPS, fill: FINCEPT.CYAN },
        { name: 'EPS Change', value: epsChange, fill: epsChange > 0 ? FINCEPT.GREEN : FINCEPT.RED },
        { name: 'Pro Forma EPS', value: result.pro_forma_eps, fill: MA_COLORS.merger },
      ];

      const dealOverview = result.deal_overview || {};
      // goodwill from Python is a raw float, not an object
      const goodwillAmount = typeof result.goodwill === 'number' ? result.goodwill : (result.goodwill?.goodwill_amount || result.goodwill?.goodwill || 0);
      // breakeven_synergies from Python is a raw float, not an object
      const breakevenAmount = typeof result.breakeven_synergies === 'number' ? result.breakeven_synergies : (result.breakeven_synergies?.required_synergies || result.breakeven_synergies?.breakeven_amount || 0);
      const contribAnalysis = result.contribution_analysis || {};

      // Pro forma comparison chart
      const proFormaCompData = [
        {
          name: 'Revenue',
          Acquirer: accretionInputs.acquirerRevenue,
          Target: accretionInputs.targetRevenue,
          Combined: (result.pro_forma_revenue || 0) / 1e6,
        },
        {
          name: 'EBITDA',
          Acquirer: accretionInputs.acquirerEBITDA,
          Target: accretionInputs.targetEBITDA,
          Combined: (result.pro_forma_ebitda || 0) / 1e6,
        },
        {
          name: 'Net Income',
          Acquirer: accretionInputs.acquirerNetIncome,
          Target: accretionInputs.targetNetIncome,
          Combined: (result.pro_forma_net_income || 0) / 1e6,
        },
      ];

      // Payment mix chart
      const paymentMix = dealOverview.payment_mix || {};
      const paymentData = [
        { name: 'Cash', value: paymentMix.cash_pct || accretionInputs.cashPct, fill: FINCEPT.GREEN },
        { name: 'Stock', value: paymentMix.stock_pct || accretionInputs.stockPct, fill: MA_COLORS.merger },
      ].filter(d => d.value > 0);

      return (
        <>
          {/* Hero metric: EPS impact */}
          <div style={{
            padding: '16px 20px',
            backgroundColor: isAccretive ? `${FINCEPT.GREEN}10` : `${FINCEPT.RED}10`,
            borderLeft: `3px solid ${isAccretive ? FINCEPT.GREEN : FINCEPT.RED}`,
            borderRadius: '2px',
            marginBottom: '16px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}>
            <div>
              <div style={{
                fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.MUTED, letterSpacing: '0.5px', textTransform: 'uppercase' as const, marginBottom: '4px',
              }}>PRO FORMA EPS</div>
              <div style={{ fontSize: '28px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, lineHeight: 1 }}>
                ${result.pro_forma_eps.toFixed(2)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                display: 'flex', alignItems: 'center', gap: '4px', justifyContent: 'flex-end',
                fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: isAccretive ? FINCEPT.GREEN : FINCEPT.RED, marginBottom: '4px',
              }}>
                {isAccretive ? <TrendingUp size={14} /> : <TrendingDown size={14} />}
                {isAccretive ? 'ACCRETIVE' : 'DILUTIVE'}
              </div>
              <div style={{
                fontSize: '22px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: isAccretive ? FINCEPT.GREEN : FINCEPT.RED,
              }}>
                {isAccretive ? '+' : ''}{result.eps_accretion_pct.toFixed(1)}%
              </div>
            </div>
          </div>

          {/* Metric cards row 1 */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '10px', marginBottom: '10px' }}>
            <MAMetricCard
              label="Pro Forma Revenue"
              value={formatCurrency(result.pro_forma_revenue || 0)}
              accentColor={FINCEPT.WHITE}
            />
            <MAMetricCard
              label="Pro Forma EBITDA"
              value={formatCurrency(result.pro_forma_ebitda || 0)}
              accentColor={CHART_PALETTE[3]}
            />
            <MAMetricCard
              label="Pro Forma Net Income"
              value={formatCurrency(result.pro_forma_net_income || 0)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Pro Forma Shares"
              value={`${((result.pro_forma_shares || 0) / 1000000).toFixed(1)}M`}
              accentColor={MA_COLORS.merger}
            />
          </div>

          {/* Metric cards row 2 - Deal terms */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="Purchase Price"
              value={formatCurrency(dealOverview.purchase_price || accretionInputs.purchasePrice * 1e6)}
              accentColor={FINCEPT.WHITE}
            />
            <MAMetricCard
              label="Goodwill Created"
              value={formatCurrency(goodwillAmount)}
              accentColor={CHART_PALETTE[2]}
            />
            <MAMetricCard
              label="Breakeven Synergies"
              value={formatCurrency(breakevenAmount)}
              accentColor={FINCEPT.CYAN}
            />
            <MAMetricCard
              label="New Shares Issued"
              value={`${((result.new_shares_issued || 0) / 1e6).toFixed(1)}M`}
              accentColor={MA_COLORS.merger}
            />
          </div>

          {/* Charts: EPS waterfall + Payment mix side by side */}
          <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: '12px', marginBottom: '16px' }}>
            <MAChartPanel
              title="EPS Waterfall: Accretion / Dilution Impact"
              accentColor={MA_COLORS.merger}
              icon={<GitMerge size={12} />}
              height={220}
              actions={<MAExportButton data={waterfallData} filename="accretion_waterfall" accentColor={MA_COLORS.merger} />}
            >
              <BarChart data={waterfallData} margin={{ top: 10, right: 20, bottom: 10, left: 20 }}>
                <CartesianGrid {...CHART_STYLE.grid} />
                <XAxis dataKey="name" tick={CHART_STYLE.axis} />
                <YAxis tick={CHART_STYLE.axis} tickFormatter={(v: number) => `$${v.toFixed(1)}`} />
                <Tooltip
                  contentStyle={CHART_STYLE.tooltip}
                  formatter={(val: number) => [`$${val.toFixed(2)}`, 'EPS']}
                />
                <Bar dataKey="value" radius={[2, 2, 0, 0]}>
                  {waterfallData.map((entry, i) => (
                    <Cell key={i} fill={entry.fill} />
                  ))}
                </Bar>
              </BarChart>
            </MAChartPanel>
            <MAChartPanel
              title="Payment Mix"
              accentColor={MA_COLORS.merger}
              icon={<DollarSign size={12} />}
              height={220}
            >
              <PieChart>
                <Pie data={paymentData} dataKey="value" nameKey="name" cx="50%" cy="50%" innerRadius={40} outerRadius={70} paddingAngle={2}>
                  {paymentData.map((entry, i) => (
                    <Cell key={i} fill={entry.fill} />
                  ))}
                </Pie>
                <Tooltip contentStyle={CHART_STYLE.tooltip} formatter={(val: number) => [`${val.toFixed(1)}%`, '']} />
                <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
              </PieChart>
            </MAChartPanel>
          </div>

          {/* Pro Forma Comparison bar chart */}
          <MAChartPanel
            title="Pro Forma Financials: Acquirer vs Target vs Combined ($M)"
            accentColor={MA_COLORS.merger}
            icon={<FileText size={12} />}
            height={220}
            actions={<MAExportButton data={proFormaCompData} filename="proforma_comparison" accentColor={MA_COLORS.merger} />}
          >
            <BarChart data={proFormaCompData} margin={{ top: 10, right: 20, bottom: 10, left: 20 }}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis dataKey="name" tick={CHART_STYLE.axis} />
              <YAxis tick={CHART_STYLE.axis} tickFormatter={(v: number) => `$${v.toLocaleString()}`} />
              <Tooltip contentStyle={CHART_STYLE.tooltip} formatter={(val: number) => [`$${val.toLocaleString()}M`, '']} />
              <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
              <Bar dataKey="Acquirer" fill={FINCEPT.CYAN} radius={[2, 2, 0, 0]} />
              <Bar dataKey="Target" fill={CHART_PALETTE[2]} radius={[2, 2, 0, 0]} />
              <Bar dataKey="Combined" fill={FINCEPT.GREEN} radius={[2, 2, 0, 0]} />
            </BarChart>
          </MAChartPanel>

          {/* Contribution analysis from build result */}
          {contribAnalysis.revenue_contribution && (
            <div style={{ marginTop: '16px' }}>
              <MASectionHeader title="Contribution Analysis" accentColor={MA_COLORS.merger} icon={<GitMerge size={12} />} />
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '10px' }}>
                {Object.entries(contribAnalysis).filter(([k]) => k !== 'ownership').map(([key, val]: [string, any]) => (
                  <MAMetricCard
                    key={key}
                    label={key.replace(/_contribution/g, '').replace(/_/g, ' ').toUpperCase()}
                    value={`ACQ ${val.acquirer_pct?.toFixed(1)}%`}
                    subtitle={`TGT ${val.target_pct?.toFixed(1)}%`}
                    accentColor={MA_COLORS.merger}
                  />
                ))}
              </div>
            </div>
          )}
        </>
      );
    }

    // Synergies Results
    if (analysisType === 'synergies' && result) {
      const synergyData = result.data || result;
      return (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="Total Synergies"
              value={formatCurrency(synergyData.total_synergies || synergyData.total_value || 0)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Year 1 Impact"
              value={formatCurrency(synergyData.year_1_value || synergyData.first_year || 0)}
              accentColor={MA_COLORS.merger}
            />
            <MAMetricCard
              label="Run-Rate"
              value={formatCurrency(synergyData.run_rate || synergyData.steady_state || 0)}
              accentColor={CHART_PALETTE[2]}
            />
          </div>
        </>
      );
    }

    // Payment Structure Results
    if (analysisType === 'structure' && structureSubTab === 'payment' && result) {
      const payData = result.data || result;
      return (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="Cash Component"
              value={formatCurrency(payData.cash_amount || payData.cash_component || 0)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Stock Component"
              value={formatCurrency(payData.stock_amount || payData.stock_component || 0)}
              accentColor={MA_COLORS.merger}
            />
            <MAMetricCard
              label="Debt Required"
              value={formatCurrency(payData.debt_required || payData.financing_gap || 0)}
              accentColor={FINCEPT.RED}
            />
          </div>
        </>
      );
    }

    // Exchange Ratio Results
    if (analysisType === 'structure' && structureSubTab === 'exchange' && result.data) {
      return (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="Exchange Ratio"
              value={`${result.data.exchange_ratio?.toFixed(4)}x`}
              accentColor={MA_COLORS.merger}
            />
            <MAMetricCard
              label="Offer Price"
              value={`$${result.data.offer_price?.toFixed(2)}`}
              accentColor={FINCEPT.WHITE}
            />
            <MAMetricCard
              label="Premium"
              value={`${(result.data.premium * 100)?.toFixed(1)}%`}
              accentColor={FINCEPT.GREEN}
              trend="up"
              trendValue={`${(result.data.premium * 100)?.toFixed(1)}%`}
            />
          </div>
        </>
      );
    }

    // Earnout Results
    if (analysisType === 'structure' && structureSubTab === 'earnout' && result.data) {
      return (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="Expected Value"
              value={formatCurrency(result.data.expected_value || 0)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Achievement Prob"
              value={`${((result.data.achievement_probability || 0) * 100).toFixed(0)}%`}
              accentColor={MA_COLORS.merger}
            />
            <MAMetricCard
              label="Max Earnout"
              value={formatCurrency(result.data.max_earnout || 0)}
              accentColor={FINCEPT.WHITE}
            />
          </div>
        </>
      );
    }

    // CVR Results
    if (analysisType === 'structure' && structureSubTab === 'cvr' && result.data) {
      return (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="CVR Value"
              value={`$${result.data.expected_value?.toFixed(2)}`}
              accentColor={CHART_PALETTE[2]}
            />
            <MAMetricCard
              label="Present Value"
              value={`$${result.data.present_value?.toFixed(2)}`}
              accentColor={FINCEPT.WHITE}
            />
            <MAMetricCard
              label="Probability"
              value={`${((result.data.probability || 0) * 100).toFixed(0)}%`}
              accentColor={MA_COLORS.merger}
            />
          </div>
        </>
      );
    }

    // Collar Results
    if (analysisType === 'structure' && structureSubTab === 'collar' && result.data) {
      const scenarios = result.data.scenarios;
      return (
        <>
          {scenarios && Array.isArray(scenarios) && (
            <>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                overflow: 'hidden',
                marginBottom: '16px',
              }}>
                <div style={{
                  display: 'flex', alignItems: 'center', gap: '8px',
                  padding: '8px 12px', backgroundColor: FINCEPT.HEADER_BG,
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  <Shield size={12} color={MA_COLORS.merger} />
                  <span style={{
                    fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                    color: MA_COLORS.merger, letterSpacing: '0.5px', textTransform: 'uppercase' as const,
                  }}>COLLAR SCENARIO TABLE</span>
                  <div style={{ flex: 1 }} />
                  <MAExportButton
                    data={scenarios.map((r: any) => ({
                      'Acquirer Price': `$${r.acquirer_price}`,
                      'Effective Ratio': r.effective_ratio?.toFixed(4),
                      'Shares Issued': ((r.shares_issued || 0) / 1e6).toFixed(1) + 'M',
                      'Deal Value': formatCurrency(r.deal_value || 0),
                    }))}
                    filename="collar_scenarios"
                    accentColor={MA_COLORS.merger}
                  />
                </div>
                <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: TYPOGRAPHY.MONO }}>
                  <thead>
                    <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                      {['ACQUIRER PRICE', 'EFFECTIVE RATIO', 'SHARES ISSUED', 'DEAL VALUE'].map((h) => (
                        <th key={h} style={{
                          padding: '7px 12px',
                          textAlign: h === 'ACQUIRER PRICE' ? 'left' as const : 'right' as const,
                          fontSize: '8px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.MUTED,
                          letterSpacing: '0.5px', borderBottom: `1px solid ${FINCEPT.BORDER}`,
                          textTransform: 'uppercase' as const,
                        }}>{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {scenarios.map((row: any, idx: number) => (
                      <tr key={idx} style={{ backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)' }}>
                        <td style={{ padding: '6px 12px', fontSize: '10px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          ${row.acquirer_price}
                        </td>
                        <td style={{ padding: '6px 12px', fontSize: '10px', color: MA_COLORS.merger, borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'right' }}>
                          {row.effective_ratio?.toFixed(4)}x
                        </td>
                        <td style={{ padding: '6px 12px', fontSize: '10px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'right' }}>
                          {((row.shares_issued || 0) / 1000000).toFixed(1)}M
                        </td>
                        <td style={{ padding: '6px 12px', fontSize: '10px', color: FINCEPT.GREEN, borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'right' }}>
                          {formatCurrency(row.deal_value || 0)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>

              {/* Collar chart */}
              <MAChartPanel
                title="Collar: Effective Exchange Ratio by Price"
                accentColor={MA_COLORS.merger}
                icon={<Shield size={12} />}
                height={200}
              >
                <BarChart data={scenarios.map((s: any) => ({ price: `$${s.acquirer_price}`, ratio: s.effective_ratio }))} margin={{ top: 10, right: 20, bottom: 10, left: 20 }}>
                  <CartesianGrid {...CHART_STYLE.grid} />
                  <XAxis dataKey="price" tick={CHART_STYLE.axis} />
                  <YAxis tick={CHART_STYLE.axis} domain={['auto', 'auto']} tickFormatter={(v: number) => `${v.toFixed(2)}x`} />
                  <Tooltip contentStyle={CHART_STYLE.tooltip} formatter={(val: number) => [`${val.toFixed(4)}x`, 'Ratio']} />
                  <Bar dataKey="ratio" fill={MA_COLORS.merger} radius={[2, 2, 0, 0]} />
                </BarChart>
              </MAChartPanel>
            </>
          )}
        </>
      );
    }

    // Pro Forma Results
    if (analysisType === 'proforma' && proformaSubTab === 'proforma' && result.data) {
      const proFormaChartData = [
        {
          name: 'Revenue',
          Acquirer: proformaInputs.acquirerRevenue,
          Target: proformaInputs.targetRevenue,
          Combined: (result.data.combined_revenue || 0) / 1e6,
        },
        {
          name: 'EBITDA',
          Acquirer: proformaInputs.acquirerEbitda,
          Target: proformaInputs.targetEbitda,
          Combined: (result.data.combined_ebitda || 0) / 1e6,
        },
        {
          name: 'Net Income',
          Acquirer: proformaInputs.acquirerNetIncome,
          Target: proformaInputs.targetNetIncome,
          Combined: (result.data.combined_net_income || 0) / 1e6,
        },
      ];

      return (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            <MAMetricCard
              label="Combined Revenue"
              value={formatCurrency(result.data.combined_revenue || 0)}
              accentColor={FINCEPT.WHITE}
            />
            <MAMetricCard
              label="Combined EBITDA"
              value={formatCurrency(result.data.combined_ebitda || 0)}
              accentColor={MA_COLORS.merger}
            />
            <MAMetricCard
              label="Combined Net Income"
              value={formatCurrency(result.data.combined_net_income || 0)}
              accentColor={FINCEPT.GREEN}
            />
            <MAMetricCard
              label="Pro Forma EPS"
              value={`$${(result.data.combined_eps || 0).toFixed(2)}`}
              accentColor={FINCEPT.YELLOW}
            />
            <MAMetricCard
              label="Combined Shares"
              value={`${((result.data.combined_shares || 0) / 1e6).toFixed(1)}M`}
              accentColor={FINCEPT.CYAN}
            />
          </div>

          {/* Grouped bar chart: Acquirer vs Target vs Combined */}
          <MAChartPanel
            title="Pro Forma Financials: Acquirer vs Target vs Combined ($M)"
            accentColor={MA_COLORS.merger}
            icon={<FileText size={12} />}
            height={260}
            actions={<MAExportButton data={proFormaChartData} filename="proforma_comparison" accentColor={MA_COLORS.merger} />}
          >
            <BarChart data={proFormaChartData} margin={{ top: 10, right: 20, bottom: 10, left: 20 }}>
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis dataKey="name" tick={CHART_STYLE.axis} />
              <YAxis tick={CHART_STYLE.axis} tickFormatter={(v: number) => `$${v.toLocaleString()}`} />
              <Tooltip
                contentStyle={CHART_STYLE.tooltip}
                formatter={(val: number) => [`$${val.toLocaleString()}M`, '']}
              />
              <Legend
                wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
              />
              <Bar dataKey="Acquirer" fill={MA_COLORS.merger} radius={[2, 2, 0, 0]} />
              <Bar dataKey="Target" fill={CHART_PALETTE[2]} radius={[2, 2, 0, 0]} />
              <Bar dataKey="Combined" fill={FINCEPT.GREEN} radius={[2, 2, 0, 0]} />
            </BarChart>
          </MAChartPanel>
        </>
      );
    }

    // Sources & Uses Results
    if (analysisType === 'proforma' && proformaSubTab === 'sources_uses' && result.data) {
      // Build chart data for horizontal stacked bar
      const sourcesEntries = result.data.sources ? Object.entries(result.data.sources) : [];
      const usesEntries = result.data.uses ? Object.entries(result.data.uses) : [];

      const sourcesChartData = sourcesEntries.map(([key, val], i) => ({
        name: key.replace(/_/g, ' ').toUpperCase(),
        value: (val as number) / 1e6,
        fill: CHART_PALETTE[i % CHART_PALETTE.length],
      }));
      const usesChartData = usesEntries.map(([key, val], i) => ({
        name: key.replace(/_/g, ' ').toUpperCase(),
        value: (val as number) / 1e6,
        fill: CHART_PALETTE[(i + 4) % CHART_PALETTE.length],
      }));

      return (
        <>
          {/* Two-column sources vs uses metric display */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '16px' }}>
            {/* Sources */}
            <div style={{
              padding: '12px 14px', backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`, borderLeft: `3px solid ${FINCEPT.GREEN}`, borderRadius: '2px',
            }}>
              <div style={{
                fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.GREEN, letterSpacing: '0.5px', textTransform: 'uppercase' as const, marginBottom: '10px',
              }}>SOURCES</div>
              {sourcesEntries.map(([key, val]) => (
                <div key={key} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY, textTransform: 'uppercase' as const }}>{key.replace(/_/g, ' ')}</span>
                  <span style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.WHITE }}>{formatCurrency(val as number)}</span>
                </div>
              ))}
              <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: '6px', marginTop: '6px', display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.GREEN }}>TOTAL</span>
                <span style={{ fontSize: '11px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.GREEN }}>{formatCurrency(result.data.total_sources || 0)}</span>
              </div>
            </div>

            {/* Uses */}
            <div style={{
              padding: '12px 14px', backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`, borderLeft: `3px solid ${FINCEPT.RED}`, borderRadius: '2px',
            }}>
              <div style={{
                fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.RED, letterSpacing: '0.5px', textTransform: 'uppercase' as const, marginBottom: '10px',
              }}>USES</div>
              {usesEntries.map(([key, val]) => (
                <div key={key} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY, textTransform: 'uppercase' as const }}>{key.replace(/_/g, ' ')}</span>
                  <span style={{ fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.WHITE }}>{formatCurrency(val as number)}</span>
                </div>
              ))}
              <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: '6px', marginTop: '6px', display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.RED }}>TOTAL</span>
                <span style={{ fontSize: '11px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.RED }}>{formatCurrency(result.data.total_uses || 0)}</span>
              </div>
            </div>
          </div>

          {/* Horizontal stacked bar: Sources composition */}
          <MAChartPanel
            title="Sources & Uses Composition ($M)"
            accentColor={MA_COLORS.merger}
            icon={<Layers size={12} />}
            height={200}
            actions={<MAExportButton data={[...sourcesChartData, ...usesChartData]} filename="sources_uses" accentColor={MA_COLORS.merger} />}
          >
            <BarChart
              layout="vertical"
              data={[
                {
                  name: 'Sources',
                  ...Object.fromEntries(sourcesEntries.map(([k, v]) => [k.replace(/_/g, ' '), (v as number) / 1e6])),
                },
                {
                  name: 'Uses',
                  ...Object.fromEntries(usesEntries.map(([k, v]) => [k.replace(/_/g, ' '), (v as number) / 1e6])),
                },
              ]}
              margin={{ top: 10, right: 20, bottom: 10, left: 60 }}
            >
              <CartesianGrid {...CHART_STYLE.grid} />
              <XAxis type="number" tick={CHART_STYLE.axis} tickFormatter={(v: number) => `$${v.toLocaleString()}`} />
              <YAxis type="category" dataKey="name" tick={CHART_STYLE.axis} width={50} />
              <Tooltip
                contentStyle={CHART_STYLE.tooltip}
                formatter={(val: number) => [`$${val.toLocaleString()}M`, '']}
              />
              <Legend wrapperStyle={{ fontSize: '8px', fontFamily: 'var(--ft-font-family, monospace)' }} />
              {sourcesEntries.map(([k], i) => (
                <Bar key={k} dataKey={k.replace(/_/g, ' ')} stackId="a" fill={CHART_PALETTE[i % CHART_PALETTE.length]} radius={i === sourcesEntries.length - 1 ? [0, 2, 2, 0] : [0, 0, 0, 0]} />
              ))}
              {usesEntries.map(([k], i) => (
                <Bar key={k} dataKey={k.replace(/_/g, ' ')} stackId="a" fill={CHART_PALETTE[(i + 4) % CHART_PALETTE.length]} radius={i === usesEntries.length - 1 ? [0, 2, 2, 0] : [0, 0, 0, 0]} />
              ))}
            </BarChart>
          </MAChartPanel>
        </>
      );
    }

    // Contribution Analysis Results
    if (analysisType === 'proforma' && proformaSubTab === 'contribution' && result.data) {
      const contributions = result.data.contributions;
      const contributionChartData = contributions
        ? Object.entries(contributions).map(([metric, data]: [string, any]) => ({
            name: metric.replace(/_/g, ' ').toUpperCase(),
            Acquirer: parseFloat(((data.acquirer_pct || 0) * 100).toFixed(1)),
            Target: parseFloat(((data.target_pct || 0) * 100).toFixed(1)),
          }))
        : [];

      return (
        <>
          {/* Contribution table */}
          {contributions && (
            <div style={{
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              overflow: 'hidden',
              marginBottom: '16px',
            }}>
              <div style={{
                display: 'flex', alignItems: 'center', gap: '8px',
                padding: '8px 12px', backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <GitMerge size={12} color={MA_COLORS.merger} />
                <span style={{
                  fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                  color: MA_COLORS.merger, letterSpacing: '0.5px', textTransform: 'uppercase' as const,
                }}>CONTRIBUTION BREAKDOWN</span>
                <div style={{ flex: 1 }} />
                <MAExportButton
                  data={Object.entries(contributions).map(([m, d]: [string, any]) => ({
                    Metric: m.replace(/_/g, ' '),
                    'Acquirer %': `${(d.acquirer_pct * 100).toFixed(1)}%`,
                    'Target %': `${(d.target_pct * 100).toFixed(1)}%`,
                    'Ownership': `${(contributionInputs.ownershipSplit * 100).toFixed(0)}%`,
                  }))}
                  filename="contribution_analysis"
                  accentColor={MA_COLORS.merger}
                />
              </div>
              <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: TYPOGRAPHY.MONO }}>
                <thead>
                  <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                    {['METRIC', 'ACQUIRER %', 'TARGET %', 'OWNERSHIP'].map((h, i) => (
                      <th key={h} style={{
                        padding: '7px 12px',
                        textAlign: i === 0 ? 'left' as const : 'right' as const,
                        fontSize: '8px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.MUTED,
                        letterSpacing: '0.5px', borderBottom: `1px solid ${FINCEPT.BORDER}`,
                        textTransform: 'uppercase' as const,
                      }}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {Object.entries(contributions).map(([metric, data]: [string, any], idx: number) => (
                    <tr key={metric} style={{ backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)' }}>
                      <td style={{ padding: '6px 12px', fontSize: '10px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}`, textTransform: 'uppercase' as const }}>
                        {metric.replace(/_/g, ' ')}
                      </td>
                      <td style={{ padding: '6px 12px', fontSize: '10px', color: MA_COLORS.merger, borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'right' }}>
                        {(data.acquirer_pct * 100).toFixed(1)}%
                      </td>
                      <td style={{ padding: '6px 12px', fontSize: '10px', color: CHART_PALETTE[0], borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'right' }}>
                        {(data.target_pct * 100).toFixed(1)}%
                      </td>
                      <td style={{ padding: '6px 12px', fontSize: '10px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'right' }}>
                        {(contributionInputs.ownershipSplit * 100).toFixed(0)}%
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}

          {/* Fairness Assessment */}
          {result.data.fairness_assessment && (
            <div style={{
              padding: '12px 14px',
              backgroundColor: result.data.fairness_assessment.is_fair ? `${FINCEPT.GREEN}10` : `${FINCEPT.RED}10`,
              borderLeft: `3px solid ${result.data.fairness_assessment.is_fair ? FINCEPT.GREEN : FINCEPT.RED}`,
              borderRadius: '2px',
              marginBottom: '16px',
            }}>
              <div style={{
                fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.MUTED, letterSpacing: '0.5px', textTransform: 'uppercase' as const, marginBottom: '4px',
              }}>FAIRNESS ASSESSMENT</div>
              <div style={{
                fontSize: '14px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                color: result.data.fairness_assessment.is_fair ? FINCEPT.GREEN : FINCEPT.RED, marginBottom: '4px',
              }}>
                {result.data.fairness_assessment.is_fair ? 'FAIR' : 'POTENTIALLY UNFAIR'}
              </div>
              <div style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY, lineHeight: 1.5 }}>
                {result.data.fairness_assessment.commentary}
              </div>
            </div>
          )}

          {/* Grouped bar chart: Acquirer % vs Target % */}
          {contributionChartData.length > 0 && (
            <MAChartPanel
              title="Contribution: Acquirer % vs Target % by Metric"
              accentColor={MA_COLORS.merger}
              icon={<GitMerge size={12} />}
              height={240}
            >
              <BarChart data={contributionChartData} margin={{ top: 10, right: 20, bottom: 10, left: 20 }}>
                <CartesianGrid {...CHART_STYLE.grid} />
                <XAxis dataKey="name" tick={{ ...CHART_STYLE.axis, fontSize: 7 }} interval={0} />
                <YAxis tick={CHART_STYLE.axis} domain={[0, 100]} tickFormatter={(v: number) => `${v}%`} />
                <Tooltip
                  contentStyle={CHART_STYLE.tooltip}
                  formatter={(val: number) => [`${val.toFixed(1)}%`, '']}
                />
                <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                <Bar dataKey="Acquirer" fill={MA_COLORS.merger} radius={[2, 2, 0, 0]} />
                <Bar dataKey="Target" fill={CHART_PALETTE[0]} radius={[2, 2, 0, 0]} />
              </BarChart>
            </MAChartPanel>
          )}
        </>
      );
    }

    // Fallback: generic result display
    return null;
  };

  // ===========================================================================
  //  MAIN RENDER
  // ===========================================================================
  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      overflow: 'hidden',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* ----------------------------------------------------------------- */}
      {/*  TOP BAR: Analysis type selector (horizontal)                     */}
      {/* ----------------------------------------------------------------- */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        padding: '8px 14px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        flexShrink: 0,
      }}>
        <GitMerge size={13} color={MA_COLORS.merger} />
        <span style={{
          fontSize: '10px', fontWeight: TYPOGRAPHY.BOLD, color: MA_COLORS.merger,
          letterSpacing: '0.5px', textTransform: 'uppercase' as const, marginRight: '10px',
        }}>MERGER</span>
        <div style={{ width: '1px', height: '18px', backgroundColor: FINCEPT.BORDER, marginRight: '6px' }} />
        {analysisTypeTabs.map((t) => {
          const Icon = t.icon;
          return (
            <button
              key={t.id}
              onClick={() => { setAnalysisType(t.id); setResult(null); }}
              style={tabBtn(analysisType === t.id)}
            >
              <Icon size={11} />
              {t.label}
            </button>
          );
        })}
        <div style={{ flex: 1 }} />
        {/* Run button in header for quick access */}
        <button
          onClick={handleRun}
          disabled={loading}
          style={{
            display: 'flex', alignItems: 'center', gap: '5px',
            padding: '6px 16px',
            backgroundColor: loading ? FINCEPT.MUTED : MA_COLORS.merger,
            color: FINCEPT.DARK_BG,
            border: 'none', borderRadius: '2px',
            fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: '0.5px', cursor: loading ? 'wait' : 'pointer',
            textTransform: 'uppercase' as const, transition: 'all 0.15s',
          }}
        >
          <PlayCircle size={12} />
          {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
        </button>
      </div>

      {/* ----------------------------------------------------------------- */}
      {/*  SUB-TAB BAR: Structure / ProForma sub-tabs                      */}
      {/* ----------------------------------------------------------------- */}
      {(analysisType === 'structure' || analysisType === 'proforma') && (
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          padding: '6px 14px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
        }}>
          <span style={{
            fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.MUTED, letterSpacing: '0.5px', textTransform: 'uppercase' as const, marginRight: '8px',
          }}>
            {analysisType === 'structure' ? 'STRUCTURE TYPE' : 'PRO FORMA TYPE'}
          </span>
          {(analysisType === 'structure' ? structureSubTabs : proformaSubTabs).map((t) => {
            const Icon = t.icon;
            const isActive = analysisType === 'structure'
              ? structureSubTab === t.id
              : proformaSubTab === (t.id as ProFormaSubTab);
            return (
              <button
                key={t.id}
                onClick={() => {
                  if (analysisType === 'structure') { setStructureSubTab(t.id as StructureSubTab); }
                  else { setProformaSubTab(t.id as ProFormaSubTab); }
                  setResult(null);
                }}
                style={subTabBtn(isActive)}
              >
                <Icon size={10} />
                {t.label}
              </button>
            );
          })}
        </div>
      )}

      {/* ----------------------------------------------------------------- */}
      {/*  SCROLLABLE CONTENT: inputs (collapsible) + results              */}
      {/* ----------------------------------------------------------------- */}
      <div style={{ flex: 1, overflow: 'auto', padding: '14px' }}>
        {/* Collapsible Input Section */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: '16px',
          overflow: 'hidden',
        }}>
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
            <span style={{
              fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
              color: MA_COLORS.merger, letterSpacing: '0.5px', textTransform: 'uppercase' as const,
            }}>
              {getActiveTitle()} -- INPUTS
            </span>
            <div style={{ flex: 1 }} />
            <span style={{ fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>
              {inputsCollapsed ? 'EXPAND' : 'COLLAPSE'}
            </span>
            {inputsCollapsed ? <ChevronDown size={12} color={FINCEPT.GRAY} /> : <ChevronUp size={12} color={FINCEPT.GRAY} />}
          </div>
          {!inputsCollapsed && (
            <div style={{ padding: '12px 14px' }}>
              {renderInputForms()}
            </div>
          )}
        </div>

        {/* Results Section */}
        {result ? (
          <div>
            <MASectionHeader
              title={getActiveTitle()}
              subtitle="RESULTS"
              accentColor={MA_COLORS.merger}
              icon={<GitMerge size={12} />}
            />
            {renderResults()}

            {/* Detailed JSON results */}
            <div style={{
              marginTop: '16px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              overflow: 'hidden',
            }}>
              <div style={{
                display: 'flex', alignItems: 'center', gap: '8px',
                padding: '8px 12px', backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <span style={{
                  fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.MUTED, letterSpacing: '0.5px', textTransform: 'uppercase' as const,
                }}>DETAILED RESULTS (JSON)</span>
                <div style={{ flex: 1 }} />
                <MAExportButton data={JSON.stringify(result, null, 2)} filename={`merger_${analysisType}_raw`} accentColor={MA_COLORS.merger} />
              </div>
              <pre style={{
                fontSize: '9px',
                fontFamily: TYPOGRAPHY.MONO,
                color: FINCEPT.GRAY,
                padding: '12px 14px',
                overflow: 'auto',
                maxHeight: '240px',
                margin: 0,
                lineHeight: 1.6,
              }}>
                {JSON.stringify(result, null, 2)}
              </pre>
            </div>
          </div>
        ) : (
          <MAEmptyState
            icon={<GitMerge size={48} />}
            title="Configure inputs and run analysis"
            description="Accretion/Dilution -- Synergies -- Deal Structure -- Pro Forma"
            accentColor={MA_COLORS.merger}
            actionLabel="RUN ANALYSIS"
            onAction={handleRun}
          />
        )}
      </div>
    </div>
  );
};
