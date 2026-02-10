/**
 * Merger Analysis - Unified Accretion/Dilution, Synergies, Deal Structure, Pro Forma
 *
 * Complete merger analysis toolkit with advanced deal structures
 */

import React, { useState } from 'react';
import { GitMerge, Zap, DollarSign, PlayCircle, TrendingUp, TrendingDown, Target, Percent, Shield, FileText, PieChart, Layers } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';
import { showSuccess, showError } from '@/utils/notifications';

type AnalysisType = 'accretion' | 'synergies' | 'structure' | 'proforma';
type StructureSubTab = 'payment' | 'earnout' | 'exchange' | 'collar' | 'cvr';
type ProFormaSubTab = 'proforma' | 'sources_uses' | 'contribution';

export const MergerAnalysis: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('accretion');
  const [structureSubTab, setStructureSubTab] = useState<StructureSubTab>('payment');
  const [proformaSubTab, setProformaSubTab] = useState<ProFormaSubTab>('proforma');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);

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
    targetRevenue: 3000,
    targetEbitda: 600,
    targetNetIncome: 300,
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
        { label: 'EXPECTED VALUE', value: formatCurrency(res.data?.expected_value || 0) }
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
        { label: 'RATIO', value: `${res.data?.exchange_ratio?.toFixed(4)}x` }
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
        { label: 'EXPECTED VALUE', value: `$${res.data?.expected_value?.toFixed(2)}` }
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
      };

      const targetData = {
        revenue: proformaInputs.targetRevenue * 1000000,
        ebitda: proformaInputs.targetEbitda * 1000000,
        net_income: proformaInputs.targetNetIncome * 1000000,
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

  const renderInputField = (
    label: string,
    value: number | string,
    onChange: (val: string) => void,
    placeholder?: string
  ) => (
    <div>
      <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>{label}</label>
      <input
        type="text"
        inputMode="decimal"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={COMMON_STYLES.inputField}
      />
    </div>
  );

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT - Analysis Type & Inputs */}
      <div style={{
        width: '420px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {/* Analysis Type Selector */}
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
            ANALYSIS TYPE
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.TINY }}>
            {[
              { id: 'accretion', label: 'ACCRETION/DILUTION', icon: GitMerge },
              { id: 'synergies', label: 'SYNERGIES', icon: Zap },
              { id: 'structure', label: 'DEAL STRUCTURE', icon: DollarSign },
              { id: 'proforma', label: 'PRO FORMA', icon: FileText },
            ].map((type) => {
              const Icon = type.icon;
              return (
                <button
                  key={type.id}
                  onClick={() => { setAnalysisType(type.id as AnalysisType); setResult(null); }}
                  style={{
                    ...COMMON_STYLES.tabButton(analysisType === type.id),
                    justifyContent: 'flex-start',
                    padding: '8px',
                  }}
                >
                  <Icon size={12} style={{ marginRight: SPACING.TINY }} />
                  {type.label}
                </button>
              );
            })}
          </div>

          {/* Structure Sub-tabs */}
          {analysisType === 'structure' && (
            <div style={{ marginTop: SPACING.DEFAULT }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>STRUCTURE TYPE</div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.TINY }}>
                {[
                  { id: 'payment', label: 'Payment', icon: DollarSign },
                  { id: 'earnout', label: 'Earnout', icon: Target },
                  { id: 'exchange', label: 'Exchange', icon: Percent },
                  { id: 'collar', label: 'Collar', icon: Shield },
                  { id: 'cvr', label: 'CVR', icon: FileText },
                ].map((t) => {
                  const Icon = t.icon;
                  return (
                    <button
                      key={t.id}
                      onClick={() => { setStructureSubTab(t.id as StructureSubTab); setResult(null); }}
                      style={{
                        ...COMMON_STYLES.tabButton(structureSubTab === t.id),
                        padding: '4px 8px',
                        fontSize: TYPOGRAPHY.TINY,
                      }}
                    >
                      <Icon size={10} style={{ marginRight: '4px' }} />
                      {t.label}
                    </button>
                  );
                })}
              </div>
            </div>
          )}

          {/* Pro Forma Sub-tabs */}
          {analysisType === 'proforma' && (
            <div style={{ marginTop: SPACING.DEFAULT }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>PRO FORMA TYPE</div>
              <div style={{ display: 'flex', gap: SPACING.TINY }}>
                {[
                  { id: 'proforma', label: 'Pro Forma', icon: FileText },
                  { id: 'sources_uses', label: 'Sources/Uses', icon: Layers },
                  { id: 'contribution', label: 'Contribution', icon: PieChart },
                ].map((t) => {
                  const Icon = t.icon;
                  return (
                    <button
                      key={t.id}
                      onClick={() => { setProformaSubTab(t.id as ProFormaSubTab); setResult(null); }}
                      style={{
                        ...COMMON_STYLES.tabButton(proformaSubTab === t.id),
                        padding: '6px 10px',
                        fontSize: TYPOGRAPHY.TINY,
                      }}
                    >
                      <Icon size={10} style={{ marginRight: '4px' }} />
                      {t.label}
                    </button>
                  );
                })}
              </div>
            </div>
          )}
        </div>

        {/* Input Forms */}
        <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
          {/* Accretion/Dilution */}
          {analysisType === 'accretion' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>ACQUIRER ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Revenue', accretionInputs.acquirerRevenue, (v) => setAccretionInputs({ ...accretionInputs, acquirerRevenue: parseFloat(v) || 0 }))}
                {renderInputField('Net Income', accretionInputs.acquirerNetIncome, (v) => setAccretionInputs({ ...accretionInputs, acquirerNetIncome: parseFloat(v) || 0 }))}
                {renderInputField('Shares (M)', accretionInputs.acquirerShares, (v) => setAccretionInputs({ ...accretionInputs, acquirerShares: parseFloat(v) || 0 }))}
                {renderInputField('EPS', accretionInputs.acquirerEPS, (v) => setAccretionInputs({ ...accretionInputs, acquirerEPS: parseFloat(v) || 0 }))}
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>TARGET ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Revenue', accretionInputs.targetRevenue, (v) => setAccretionInputs({ ...accretionInputs, targetRevenue: parseFloat(v) || 0 }))}
                {renderInputField('Net Income', accretionInputs.targetNetIncome, (v) => setAccretionInputs({ ...accretionInputs, targetNetIncome: parseFloat(v) || 0 }))}
                {renderInputField('Purchase Price', accretionInputs.purchasePrice, (v) => setAccretionInputs({ ...accretionInputs, purchasePrice: parseFloat(v) || 0 }))}
                {renderInputField('Cash %', accretionInputs.cashPct, (v) => { const cash = parseFloat(v) || 0; setAccretionInputs({ ...accretionInputs, cashPct: cash, stockPct: 100 - cash }); })}
              </div>
            </>
          )}

          {/* Synergies */}
          {analysisType === 'synergies' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>SYNERGY TYPE</div>
              <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                <button
                  onClick={() => setSynergiesInputs({ ...synergiesInputs, synergyType: 'revenue' })}
                  style={{ ...COMMON_STYLES.tabButton(synergiesInputs.synergyType === 'revenue'), flex: 1 }}
                >
                  REVENUE
                </button>
                <button
                  onClick={() => setSynergiesInputs({ ...synergiesInputs, synergyType: 'cost' })}
                  style={{ ...COMMON_STYLES.tabButton(synergiesInputs.synergyType === 'cost'), flex: 1 }}
                >
                  COST
                </button>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Base Revenue ($M)', synergiesInputs.baseRevenue, (v) => setSynergiesInputs({ ...synergiesInputs, baseRevenue: parseFloat(v) || 0 }))}
                {renderInputField('Cross-Sell Rate', synergiesInputs.crossSellRate, (v) => setSynergiesInputs({ ...synergiesInputs, crossSellRate: parseFloat(v) || 0 }))}
                {renderInputField('Projection Years', synergiesInputs.years, (v) => setSynergiesInputs({ ...synergiesInputs, years: parseInt(v) || 0 }))}
              </div>
            </>
          )}

          {/* Deal Structure - Payment */}
          {analysisType === 'structure' && structureSubTab === 'payment' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>PAYMENT STRUCTURE ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Purchase Price', structureInputs.purchasePrice, (v) => setStructureInputs({ ...structureInputs, purchasePrice: parseFloat(v) || 0 }))}
                {renderInputField('Cash %', structureInputs.cashPct, (v) => setStructureInputs({ ...structureInputs, cashPct: parseFloat(v) || 0 }))}
                {renderInputField('Acquirer Cash Available', structureInputs.acquirerCash, (v) => setStructureInputs({ ...structureInputs, acquirerCash: parseFloat(v) || 0 }))}
                {renderInputField('Debt Capacity', structureInputs.debtCapacity, (v) => setStructureInputs({ ...structureInputs, debtCapacity: parseFloat(v) || 0 }))}
              </div>
            </>
          )}

          {/* Deal Structure - Earnout */}
          {analysisType === 'structure' && structureSubTab === 'earnout' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>EARNOUT TERMS ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Max Earnout', earnoutInputs.maxEarnout, (v) => setEarnoutInputs({ ...earnoutInputs, maxEarnout: parseFloat(v) || 0 }))}
                {renderInputField('Revenue Target', earnoutInputs.revenueTarget, (v) => setEarnoutInputs({ ...earnoutInputs, revenueTarget: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA Target', earnoutInputs.ebitdaTarget, (v) => setEarnoutInputs({ ...earnoutInputs, ebitdaTarget: parseFloat(v) || 0 }))}
                {renderInputField('Earnout Period (Years)', earnoutInputs.earnoutPeriod, (v) => setEarnoutInputs({ ...earnoutInputs, earnoutPeriod: parseInt(v) || 0 }))}
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>PROJECTIONS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Base Revenue ($M)', earnoutInputs.baseRevenue, (v) => setEarnoutInputs({ ...earnoutInputs, baseRevenue: parseFloat(v) || 0 }))}
                {renderInputField('Revenue Growth Rate', earnoutInputs.revenueGrowth, (v) => setEarnoutInputs({ ...earnoutInputs, revenueGrowth: parseFloat(v) || 0 }))}
                {renderInputField('Base EBITDA ($M)', earnoutInputs.baseEbitda, (v) => setEarnoutInputs({ ...earnoutInputs, baseEbitda: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA Growth Rate', earnoutInputs.ebitdaGrowth, (v) => setEarnoutInputs({ ...earnoutInputs, ebitdaGrowth: parseFloat(v) || 0 }))}
              </div>
            </>
          )}

          {/* Deal Structure - Exchange Ratio */}
          {analysisType === 'structure' && structureSubTab === 'exchange' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>EXCHANGE RATIO INPUTS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Acquirer Stock Price', exchangeInputs.acquirerPrice, (v) => setExchangeInputs({ ...exchangeInputs, acquirerPrice: parseFloat(v) || 0 }))}
                {renderInputField('Target Stock Price', exchangeInputs.targetPrice, (v) => setExchangeInputs({ ...exchangeInputs, targetPrice: parseFloat(v) || 0 }))}
                {renderInputField('Offer Premium', exchangeInputs.offerPremium, (v) => setExchangeInputs({ ...exchangeInputs, offerPremium: parseFloat(v) || 0 }))}
              </div>
            </>
          )}

          {/* Deal Structure - Collar */}
          {analysisType === 'structure' && structureSubTab === 'collar' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>COLLAR MECHANISM</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Base Exchange Ratio', collarInputs.baseRatio, (v) => setCollarInputs({ ...collarInputs, baseRatio: parseFloat(v) || 0 }))}
                {renderInputField('Floor Price', collarInputs.floorPrice, (v) => setCollarInputs({ ...collarInputs, floorPrice: parseFloat(v) || 0 }))}
                {renderInputField('Cap Price', collarInputs.capPrice, (v) => setCollarInputs({ ...collarInputs, capPrice: parseFloat(v) || 0 }))}
                {renderInputField('Target Shares (M)', collarInputs.targetShares, (v) => setCollarInputs({ ...collarInputs, targetShares: parseFloat(v) || 0 }))}
                {renderInputField('Price Scenarios (comma-sep)', collarInputs.priceScenarios, (v) => setCollarInputs({ ...collarInputs, priceScenarios: v }), '40,45,50,55,60')}
              </div>
            </>
          )}

          {/* Deal Structure - CVR */}
          {analysisType === 'structure' && structureSubTab === 'cvr' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>CVR TYPE</div>
              <div style={{ display: 'flex', gap: SPACING.TINY, marginBottom: SPACING.DEFAULT }}>
                {['milestone', 'earnout', 'litigation'].map((type) => (
                  <button
                    key={type}
                    onClick={() => setCvrInputs({ ...cvrInputs, cvrType: type as any })}
                    style={{
                      ...COMMON_STYLES.tabButton(cvrInputs.cvrType === type),
                      flex: 1,
                      textTransform: 'uppercase',
                    }}
                  >
                    {type}
                  </button>
                ))}
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>CVR PARAMETERS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Milestone', cvrInputs.milestone, (v) => setCvrInputs({ ...cvrInputs, milestone: v }), 'FDA_APPROVAL')}
                {renderInputField('Probability', cvrInputs.probability, (v) => setCvrInputs({ ...cvrInputs, probability: parseFloat(v) || 0 }))}
                {renderInputField('Payout Amount ($)', cvrInputs.payoutAmount, (v) => setCvrInputs({ ...cvrInputs, payoutAmount: parseFloat(v) || 0 }))}
                {renderInputField('Expected Date', cvrInputs.expectedDate, (v) => setCvrInputs({ ...cvrInputs, expectedDate: v }), 'YYYY-MM-DD')}
                {renderInputField('Discount Rate', cvrInputs.discountRate, (v) => setCvrInputs({ ...cvrInputs, discountRate: parseFloat(v) || 0 }))}
              </div>
            </>
          )}

          {/* Pro Forma */}
          {analysisType === 'proforma' && proformaSubTab === 'proforma' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>ACQUIRER ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Revenue', proformaInputs.acquirerRevenue, (v) => setProformaInputs({ ...proformaInputs, acquirerRevenue: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA', proformaInputs.acquirerEbitda, (v) => setProformaInputs({ ...proformaInputs, acquirerEbitda: parseFloat(v) || 0 }))}
                {renderInputField('Net Income', proformaInputs.acquirerNetIncome, (v) => setProformaInputs({ ...proformaInputs, acquirerNetIncome: parseFloat(v) || 0 }))}
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>TARGET ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Revenue', proformaInputs.targetRevenue, (v) => setProformaInputs({ ...proformaInputs, targetRevenue: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA', proformaInputs.targetEbitda, (v) => setProformaInputs({ ...proformaInputs, targetEbitda: parseFloat(v) || 0 }))}
                {renderInputField('Net Income', proformaInputs.targetNetIncome, (v) => setProformaInputs({ ...proformaInputs, targetNetIncome: parseFloat(v) || 0 }))}
              </div>
              {renderInputField('Projection Year', proformaInputs.projectionYear, (v) => setProformaInputs({ ...proformaInputs, projectionYear: parseInt(v) || 1 }))}
            </>
          )}

          {/* Sources & Uses */}
          {analysisType === 'proforma' && proformaSubTab === 'sources_uses' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>USES OF FUNDS ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Purchase Price', sourcesUsesInputs.purchasePrice, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, purchasePrice: parseFloat(v) || 0 }))}
                {renderInputField('Target Debt Refinance', sourcesUsesInputs.targetDebt, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, targetDebt: parseFloat(v) || 0 }))}
                {renderInputField('Transaction Fees', sourcesUsesInputs.transactionFees, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, transactionFees: parseFloat(v) || 0 }))}
                {renderInputField('Financing Fees', sourcesUsesInputs.financingFees, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, financingFees: parseFloat(v) || 0 }))}
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>SOURCES OF FUNDS ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Cash on Hand', sourcesUsesInputs.cashOnHand, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, cashOnHand: parseFloat(v) || 0 }))}
                {renderInputField('New Debt', sourcesUsesInputs.newDebt, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, newDebt: parseFloat(v) || 0 }))}
                {renderInputField('Stock Issuance', sourcesUsesInputs.stockIssuance, (v) => setSourcesUsesInputs({ ...sourcesUsesInputs, stockIssuance: parseFloat(v) || 0 }))}
              </div>
            </>
          )}

          {/* Contribution Analysis */}
          {analysisType === 'proforma' && proformaSubTab === 'contribution' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>ACQUIRER ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Revenue', contributionInputs.acquirerRevenue, (v) => setContributionInputs({ ...contributionInputs, acquirerRevenue: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA', contributionInputs.acquirerEbitda, (v) => setContributionInputs({ ...contributionInputs, acquirerEbitda: parseFloat(v) || 0 }))}
                {renderInputField('Net Income', contributionInputs.acquirerNetIncome, (v) => setContributionInputs({ ...contributionInputs, acquirerNetIncome: parseFloat(v) || 0 }))}
                {renderInputField('Total Assets', contributionInputs.acquirerAssets, (v) => setContributionInputs({ ...contributionInputs, acquirerAssets: parseFloat(v) || 0 }))}
              </div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>TARGET ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Revenue', contributionInputs.targetRevenue, (v) => setContributionInputs({ ...contributionInputs, targetRevenue: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA', contributionInputs.targetEbitda, (v) => setContributionInputs({ ...contributionInputs, targetEbitda: parseFloat(v) || 0 }))}
                {renderInputField('Net Income', contributionInputs.targetNetIncome, (v) => setContributionInputs({ ...contributionInputs, targetNetIncome: parseFloat(v) || 0 }))}
                {renderInputField('Total Assets', contributionInputs.targetAssets, (v) => setContributionInputs({ ...contributionInputs, targetAssets: parseFloat(v) || 0 }))}
              </div>
              {renderInputField('Acquirer Ownership %', contributionInputs.ownershipSplit, (v) => setContributionInputs({ ...contributionInputs, ownershipSplit: parseFloat(v) || 0 }))}
            </>
          )}
        </div>

        {/* Run Button */}
        <div style={{ padding: SPACING.DEFAULT, borderTop: `1px solid ${FINCEPT.BORDER}` }}>
          <button
            onClick={handleRun}
            disabled={loading}
            style={{ ...COMMON_STYLES.buttonPrimary, width: '100%' }}
          >
            <PlayCircle size={14} style={{ marginRight: SPACING.SMALL }} />
            {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
          </button>
        </div>
      </div>

      {/* RIGHT - Results */}
      <div style={{
        flex: 1,
        backgroundColor: FINCEPT.DARK_BG,
        padding: SPACING.LARGE,
        overflow: 'auto',
      }}>
        {result ? (
          <div>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
              {analysisType === 'accretion' && 'ACCRETION / DILUTION ANALYSIS'}
              {analysisType === 'synergies' && `${synergiesInputs.synergyType.toUpperCase()} SYNERGIES`}
              {analysisType === 'structure' && structureSubTab === 'payment' && 'PAYMENT STRUCTURE'}
              {analysisType === 'structure' && structureSubTab === 'earnout' && 'EARNOUT VALUATION'}
              {analysisType === 'structure' && structureSubTab === 'exchange' && 'EXCHANGE RATIO'}
              {analysisType === 'structure' && structureSubTab === 'collar' && 'COLLAR MECHANISM'}
              {analysisType === 'structure' && structureSubTab === 'cvr' && 'CVR VALUATION'}
              {analysisType === 'proforma' && proformaSubTab === 'proforma' && 'PRO FORMA FINANCIALS'}
              {analysisType === 'proforma' && proformaSubTab === 'sources_uses' && 'SOURCES & USES'}
              {analysisType === 'proforma' && proformaSubTab === 'contribution' && 'CONTRIBUTION ANALYSIS'}
            </div>

            {/* Accretion/Dilution Results */}
            {analysisType === 'accretion' && result.pro_forma_eps && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{
                  ...COMMON_STYLES.metricCard,
                  backgroundColor: result.eps_accretion_pct > 0 ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
                  borderLeft: `3px solid ${result.eps_accretion_pct > 0 ? FINCEPT.GREEN : FINCEPT.RED}`,
                  marginBottom: SPACING.DEFAULT,
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div>
                      <div style={COMMON_STYLES.dataLabel}>PRO FORMA EPS</div>
                      <div style={{ fontSize: '32px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                        ${result.pro_forma_eps.toFixed(2)}
                      </div>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ fontSize: TYPOGRAPHY.SMALL, color: result.eps_accretion_pct > 0 ? FINCEPT.GREEN : FINCEPT.RED, display: 'flex', alignItems: 'center', gap: SPACING.TINY }}>
                        {result.eps_accretion_pct > 0 ? <TrendingUp size={16} /> : <TrendingDown size={16} />}
                        {result.eps_accretion_pct > 0 ? 'ACCRETIVE' : 'DILUTIVE'}
                      </div>
                      <div style={{ fontSize: '24px', fontWeight: TYPOGRAPHY.BOLD, color: result.eps_accretion_pct > 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
                        {result.eps_accretion_pct > 0 ? '+' : ''}{result.eps_accretion_pct.toFixed(1)}%
                      </div>
                    </div>
                  </div>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PRO FORMA REVENUE</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>{formatCurrency(result.pro_forma_revenue || 0)}</div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PRO FORMA SHARES</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>{((result.pro_forma_shares || 0) / 1000000).toFixed(1)}M</div>
                  </div>
                </div>
              </div>
            )}

            {/* Exchange Ratio Results */}
            {analysisType === 'structure' && structureSubTab === 'exchange' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.CYAN}15` }}>
                    <div style={COMMON_STYLES.dataLabel}>EXCHANGE RATIO</div>
                    <div style={{ fontSize: '28px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                      {result.data.exchange_ratio?.toFixed(4)}x
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>OFFER PRICE</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      ${result.data.offer_price?.toFixed(2)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PREMIUM</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                      {(result.data.premium * 100)?.toFixed(1)}%
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Earnout Results */}
            {analysisType === 'structure' && structureSubTab === 'earnout' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.GREEN}15` }}>
                    <div style={COMMON_STYLES.dataLabel}>EXPECTED VALUE</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.data.expected_value || 0)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>ACHIEVEMENT PROB</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                      {((result.data.achievement_probability || 0) * 100).toFixed(0)}%
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>MAX EARNOUT</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.data.max_earnout || 0)}
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* CVR Results */}
            {analysisType === 'structure' && structureSubTab === 'cvr' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.PURPLE}15` }}>
                    <div style={COMMON_STYLES.dataLabel}>CVR VALUE</div>
                    <div style={{ fontSize: '28px', color: FINCEPT.PURPLE, marginTop: SPACING.TINY }}>
                      ${result.data.expected_value?.toFixed(2)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PRESENT VALUE</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      ${result.data.present_value?.toFixed(2)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PROBABILITY</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                      {((result.data.probability || 0) * 100).toFixed(0)}%
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Collar Results */}
            {analysisType === 'structure' && structureSubTab === 'collar' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                {result.data.scenarios && Array.isArray(result.data.scenarios) && (
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    borderRadius: '2px',
                    overflow: 'auto',
                  }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                      <thead>
                        <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                          <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Acquirer Price</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Effective Ratio</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Shares Issued</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Deal Value</th>
                        </tr>
                      </thead>
                      <tbody>
                        {result.data.scenarios.map((row: any, idx: number) => (
                          <tr key={idx}>
                            <td style={{ padding: '8px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>${row.acquirer_price}</td>
                            <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.CYAN, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{row.effective_ratio?.toFixed(4)}x</td>
                            <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{((row.shares_issued || 0) / 1000000).toFixed(1)}M</td>
                            <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GREEN, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{formatCurrency(row.deal_value || 0)}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            )}

            {/* Sources & Uses Results */}
            {analysisType === 'proforma' && proformaSubTab === 'sources_uses' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL, color: FINCEPT.GREEN }}>SOURCES</div>
                    {result.data.sources && Object.entries(result.data.sources).map(([key, val]) => (
                      <div key={key} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: SPACING.TINY }}>
                        <span style={{ color: FINCEPT.GRAY, textTransform: 'uppercase', fontSize: TYPOGRAPHY.TINY }}>{key.replace(/_/g, ' ')}</span>
                        <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(val as number)}</span>
                      </div>
                    ))}
                    <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: SPACING.SMALL, marginTop: SPACING.SMALL, display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: FINCEPT.GREEN, fontWeight: TYPOGRAPHY.BOLD }}>TOTAL</span>
                      <span style={{ color: FINCEPT.GREEN, fontWeight: TYPOGRAPHY.BOLD }}>{formatCurrency(result.data.total_sources || 0)}</span>
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL, color: FINCEPT.RED }}>USES</div>
                    {result.data.uses && Object.entries(result.data.uses).map(([key, val]) => (
                      <div key={key} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: SPACING.TINY }}>
                        <span style={{ color: FINCEPT.GRAY, textTransform: 'uppercase', fontSize: TYPOGRAPHY.TINY }}>{key.replace(/_/g, ' ')}</span>
                        <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(val as number)}</span>
                      </div>
                    ))}
                    <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: SPACING.SMALL, marginTop: SPACING.SMALL, display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: FINCEPT.RED, fontWeight: TYPOGRAPHY.BOLD }}>TOTAL</span>
                      <span style={{ color: FINCEPT.RED, fontWeight: TYPOGRAPHY.BOLD }}>{formatCurrency(result.data.total_uses || 0)}</span>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Contribution Analysis Results */}
            {analysisType === 'proforma' && proformaSubTab === 'contribution' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                {result.data.contributions && (
                  <div style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    borderRadius: '2px',
                    overflow: 'auto',
                    marginBottom: SPACING.DEFAULT,
                  }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                      <thead>
                        <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                          <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Metric</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Acquirer %</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Target %</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Ownership</th>
                        </tr>
                      </thead>
                      <tbody>
                        {Object.entries(result.data.contributions).map(([metric, data]: [string, any]) => (
                          <tr key={metric}>
                            <td style={{ padding: '8px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}`, textTransform: 'uppercase' }}>{metric.replace(/_/g, ' ')}</td>
                            <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.CYAN, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{(data.acquirer_pct * 100).toFixed(1)}%</td>
                            <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.ORANGE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{(data.target_pct * 100).toFixed(1)}%</td>
                            <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{(contributionInputs.ownershipSplit * 100).toFixed(0)}%</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
                {result.data.fairness_assessment && (
                  <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: result.data.fairness_assessment.is_fair ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15` }}>
                    <div style={COMMON_STYLES.dataLabel}>FAIRNESS ASSESSMENT</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: result.data.fairness_assessment.is_fair ? FINCEPT.GREEN : FINCEPT.RED, marginTop: SPACING.TINY }}>
                      {result.data.fairness_assessment.is_fair ? 'FAIR' : 'POTENTIALLY UNFAIR'}
                    </div>
                    <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: SPACING.TINY }}>
                      {result.data.fairness_assessment.commentary}
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* Pro Forma Results */}
            {analysisType === 'proforma' && proformaSubTab === 'proforma' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>COMBINED REVENUE</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.data.combined_revenue || 0)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>COMBINED EBITDA</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.data.combined_ebitda || 0)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>COMBINED NET INCOME</div>
                    <div style={{ fontSize: '24px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.data.combined_net_income || 0)}
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Full JSON Results */}
            <div style={COMMON_STYLES.dataLabel}>DETAILED RESULTS</div>
            <pre style={{
              fontSize: TYPOGRAPHY.TINY,
              color: FINCEPT.GRAY,
              backgroundColor: FINCEPT.PANEL_BG,
              padding: SPACING.DEFAULT,
              borderRadius: '2px',
              overflow: 'auto',
              maxHeight: '300px',
            }}>
              {JSON.stringify(result, null, 2)}
            </pre>
          </div>
        ) : (
          <div style={COMMON_STYLES.emptyState}>
            <GitMerge size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
            <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>
              Select an analysis type and configure inputs
            </div>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, marginTop: SPACING.SMALL }}>
              Accretion/Dilution • Synergies • Deal Structure • Pro Forma
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
