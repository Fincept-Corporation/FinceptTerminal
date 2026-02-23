/**
 * Valuation Toolkit - Unified DCF, LBO, Trading Comps, Precedent Transactions
 *
 * Complete valuation toolkit with full LBO modeling, debt schedules, and sensitivity analysis
 * Bloomberg-grade UI/UX with horizontal method selector, dense input grids, and rich chart visualizations
 */

import React, { useState } from 'react';
import { Calculator, TrendingUp, Building2, BarChart3, PlayCircle, Layers, Grid3X3, DollarSign, Calendar, Plus, Minus, ChevronDown, ChevronUp } from 'lucide-react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Cell, AreaChart, Area, ScatterChart, Scatter, ReferenceLine, Legend } from 'recharts';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, LBOInputs } from '@/services/maAnalyticsService';
import { showSuccess, showError, showWarning } from '@/utils/notifications';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MADataTable, MAExportButton } from '../components';
import { MASensitivityHeatmap } from '../components';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE, formatDealValue } from '../constants';

type ValuationMethod = 'dcf' | 'lbo' | 'lbo_model' | 'debt_schedule' | 'lbo_sensitivity' | 'comps' | 'precedent';
type LBOSubTab = 'returns' | 'full_model' | 'debt_schedule' | 'sensitivity';

export const ValuationToolkit: React.FC = () => {
  const [method, setMethod] = useState<ValuationMethod>('dcf');
  const [lboSubTab, setLboSubTab] = useState<LBOSubTab>('returns');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [inputsCollapsed, setInputsCollapsed] = useState(false);

  // DCF Inputs
  const [dcfInputs, setDcfInputs] = useState({
    ebit: 500,
    taxRate: 0.21,
    riskFreeRate: 0.045,
    beta: 1.2,
    terminalGrowth: 0.025,
    shares: 100,
    marketCapM: 5000,
    debtM: 1000,
    costOfDebt: 0.05,
    cash: 500,
    depreciation: 100,
    capex: 150,
    changeInNwc: 20,
  });

  // LBO Returns Inputs (simple)
  const [lboInputs, setLboInputs] = useState({
    entryValuation: 1000,
    exitValuation: 1500,
    equityInvested: 300,
    holdingPeriod: 5,
  });

  // Full LBO Model Inputs
  const [lboModelInputs, setLboModelInputs] = useState<LBOInputs>({
    company_data: {
      company_name: 'Target',
      revenue: 500,
      ebitda: 100,
      ebit: 80,
      capex: 25,
      nwc: 50,
    },
    transaction_assumptions: {
      purchase_price: 800,
      entry_multiple: 8.0,
      exit_multiple: 9.0,
      debt_percent: 0.60,
      equity_percent: 0.40,
      revenue_growth: 0.05,
      ebitda_margin: 0.20,
      capex_pct_revenue: 0.03,
      nwc_pct_revenue: 0.05,
      tax_rate: 0.21,
      hurdle_irr: 0.20,
    },
    projection_years: 5,
  });

  // Debt Schedule Inputs
  const [debtScheduleInputs, setDebtScheduleInputs] = useState({
    seniorDebt: 300,
    seniorRate: 0.06,
    seniorTerm: 7,
    subDebt: 100,
    subRate: 0.10,
    subTerm: 8,
    revolver: 50,
    revolverRate: 0.055,
    ebitdaYears: '100,110,121,133,146',
    capexYears: '25,27,30,33,36',
    sweepPct: 0.75,
  });

  // Sensitivity Inputs
  const [sensitivityInputs, setSensitivityInputs] = useState({
    baseRevenue: 500,
    baseEbitdaMargin: 0.20,
    baseExitMultiple: 9.0,
    baseDebtPct: 0.60,
    holdingPeriod: 5,
    revenueGrowthScenarios: '0.02,0.05,0.08,0.10,0.12',
    exitMultipleScenarios: '7.0,8.0,9.0,10.0,11.0',
  });

  // Comps Inputs
  const [compsInputs, setCompsInputs] = useState({
    targetTicker: '',
    compTickers: 'AAPL,MSFT,GOOGL',
  });

  // Precedent Transaction Inputs
  const [precedentInputs, setPrecedentInputs] = useState({
    targetRevenue: 500,
    targetEbitda: 100,
    transactions: [
      { name: 'Deal A', ev: 900, revenue: 400, ebitda: 80 },
      { name: 'Deal B', ev: 1200, revenue: 500, ebitda: 120 },
      { name: 'Deal C', ev: 750, revenue: 350, ebitda: 70 },
    ] as Array<{ name: string; ev: number; revenue: number; ebitda: number }>,
  });

  const handleRunDCF = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.Valuation.calculateDCF({
        wacc_inputs: {
          risk_free_rate: dcfInputs.riskFreeRate,
          market_risk_premium: 0.06,
          beta: dcfInputs.beta,
          cost_of_debt: dcfInputs.costOfDebt,
          tax_rate: dcfInputs.taxRate,
          market_value_equity: dcfInputs.marketCapM * 1000000,
          market_value_debt: dcfInputs.debtM * 1000000,
        },
        fcf_inputs: {
          ebit: dcfInputs.ebit * 1000000,
          tax_rate: dcfInputs.taxRate,
          depreciation: dcfInputs.depreciation * 1000000,
          capex: dcfInputs.capex * 1000000,
          change_in_nwc: dcfInputs.changeInNwc * 1000000,
        },
        growth_rates: [0.15, 0.12, 0.10, 0.08, 0.06],
        terminal_growth: dcfInputs.terminalGrowth,
        balance_sheet: {
          cash: dcfInputs.cash * 1000000,
          debt: dcfInputs.debtM * 1000000,
          minority_interest: 0,
          preferred_stock: 0,
        },
        shares_outstanding: dcfInputs.shares * 1000000,
      });
      setResult(res);
      showSuccess('DCF Complete', [
        { label: 'EQUITY VALUE/SHARE', value: `$${res.equity_value_per_share?.toFixed(2)}` }
      ]);
    } catch (error) {
      showError('DCF calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunLBOReturns = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.LBO.calculateLBOReturns(
        lboInputs.entryValuation * 1000000,
        lboInputs.exitValuation * 1000000,
        lboInputs.equityInvested * 1000000,
        lboInputs.holdingPeriod
      );
      setResult(res);
      showSuccess('LBO Returns Complete', [
        { label: 'IRR', value: `${res.irr?.toFixed(1)}%` },
        { label: 'MOIC', value: `${res.moic?.toFixed(2)}x` }
      ]);
    } catch (error) {
      showError('LBO Returns calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunLBOModel = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.LBO.buildLBOModel(lboModelInputs);
      setResult(res);
      showSuccess('LBO Model Complete', [
        { label: 'IRR', value: res.data?.returns?.irr ? `${(res.data.returns.irr * 100).toFixed(1)}%` : 'N/A' },
        { label: 'MOIC', value: res.data?.returns?.moic ? `${res.data.returns.moic.toFixed(2)}x` : 'N/A' }
      ]);
    } catch (error) {
      showError('LBO Model failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunDebtSchedule = async () => {
    setLoading(true);
    try {
      const ebitdaArr = debtScheduleInputs.ebitdaYears.split(',').map(v => parseFloat(v.trim()) * 1000000);
      const capexArr = debtScheduleInputs.capexYears.split(',').map(v => parseFloat(v.trim()) * 1000000);

      const debtStructure = {
        senior: {
          amount: debtScheduleInputs.seniorDebt * 1000000,
          rate: debtScheduleInputs.seniorRate,
          term: debtScheduleInputs.seniorTerm,
          amortization: 'straight_line',
        },
        subordinated: {
          amount: debtScheduleInputs.subDebt * 1000000,
          rate: debtScheduleInputs.subRate,
          term: debtScheduleInputs.subTerm,
          amortization: 'bullet',
        },
        revolver: {
          amount: debtScheduleInputs.revolver * 1000000,
          rate: debtScheduleInputs.revolverRate,
          commitment: debtScheduleInputs.revolver * 1000000 * 1.5,
        },
      };

      const cashFlows = {
        ebitda: ebitdaArr,
        capex: capexArr,
        interest_paid: ebitdaArr.map((_, i) => (debtScheduleInputs.seniorDebt * debtScheduleInputs.seniorRate + debtScheduleInputs.subDebt * debtScheduleInputs.subRate) * 1000000 / (i + 1)),
        taxes: ebitdaArr.map(e => e * 0.21),
      };

      const res = await MAAnalyticsService.LBO.analyzeDebtSchedule(
        debtStructure,
        cashFlows,
        debtScheduleInputs.sweepPct
      );
      setResult(res);
      showSuccess('Debt Schedule Complete');
    } catch (error) {
      showError('Debt Schedule analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunSensitivity = async () => {
    setLoading(true);
    try {
      const revenueGrowths = sensitivityInputs.revenueGrowthScenarios.split(',').map(v => parseFloat(v.trim()));
      const exitMultiples = sensitivityInputs.exitMultipleScenarios.split(',').map(v => parseFloat(v.trim()));

      const baseCase = {
        revenue: sensitivityInputs.baseRevenue * 1000000,
        ebitda_margin: sensitivityInputs.baseEbitdaMargin,
        exit_multiple: sensitivityInputs.baseExitMultiple,
        debt_percent: sensitivityInputs.baseDebtPct,
        holding_period: sensitivityInputs.holdingPeriod,
      };

      const res = await MAAnalyticsService.LBO.calculateLBOSensitivity(
        baseCase,
        revenueGrowths,
        exitMultiples
      );
      setResult({ ...res, revenueGrowths, exitMultiples });
      showSuccess('Sensitivity Analysis Complete');
    } catch (error) {
      showError('Sensitivity analysis failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunComps = async () => {
    if (!compsInputs.targetTicker.trim()) {
      showWarning('Please enter a target ticker');
      return;
    }
    setLoading(true);
    try {
      const res = await MAAnalyticsService.Valuation.calculateTradingComps(
        compsInputs.targetTicker,
        compsInputs.compTickers.split(',').map(t => t.trim())
      );
      setResult(res);
      showSuccess('Trading Comps Complete');
    } catch (error) {
      showError('Trading Comps calculation failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRunPrecedent = async () => {
    setLoading(true);
    try {
      const targetData = {
        revenue: precedentInputs.targetRevenue * 1000000,
        ebitda: precedentInputs.targetEbitda * 1000000,
      };

      // Create MADeal-compatible structures from precedent transactions
      const compDeals = precedentInputs.transactions.map((t, idx) => ({
        deal_id: `precedent_${idx}`,
        target_name: t.name,
        acquirer_name: 'Acquirer',
        deal_value: t.ev * 1000000,
        premium_1day: 0.25,
        payment_cash_pct: 50,
        payment_stock_pct: 50,
        ev_revenue: t.revenue > 0 ? (t.ev / t.revenue) : undefined,
        ev_ebitda: t.ebitda > 0 ? (t.ev / t.ebitda) : undefined,
        status: 'completed',
        industry: 'General',
        announced_date: new Date().toISOString().split('T')[0],
      }));

      const res = await MAAnalyticsService.Valuation.calculatePrecedentTransactions(targetData, compDeals);
      setResult(res);
      const impliedEV = (res as any)?.data?.target_valuation?.valuations?.blended_median || 0;
      showSuccess('Precedent Transactions Complete', [
        { label: 'IMPLIED EV', value: formatCurrency(impliedEV) }
      ]);
    } catch (error) {
      showError('Precedent Transactions failed', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    if (method === 'lbo') {
      switch (lboSubTab) {
        case 'returns': return handleRunLBOReturns();
        case 'full_model': return handleRunLBOModel();
        case 'debt_schedule': return handleRunDebtSchedule();
        case 'sensitivity': return handleRunSensitivity();
      }
    }
    switch (method) {
      case 'dcf': return handleRunDCF();
      case 'comps': return handleRunComps();
      case 'precedent': return handleRunPrecedent();
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
      <label style={{
        fontSize: '9px',
        fontFamily: TYPOGRAPHY.MONO,
        fontWeight: TYPOGRAPHY.BOLD,
        color: FINCEPT.MUTED,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        display: 'block',
        marginBottom: '3px',
      }}>{label}</label>
      <input
        type="text"
        inputMode="decimal"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          ...COMMON_STYLES.inputField,
          padding: '6px 8px',
          fontSize: '10px',
        }}
      />
    </div>
  );

  const addPrecedentTransaction = () => {
    setPrecedentInputs({
      ...precedentInputs,
      transactions: [...precedentInputs.transactions, { name: `Deal ${String.fromCharCode(65 + precedentInputs.transactions.length)}`, ev: 0, revenue: 0, ebitda: 0 }],
    });
  };

  const removePrecedentTransaction = (index: number) => {
    setPrecedentInputs({
      ...precedentInputs,
      transactions: precedentInputs.transactions.filter((_, i) => i !== index),
    });
  };

  // Method definitions for the top bar
  const methods = [
    { id: 'dcf' as const, label: 'DCF MODEL', icon: Calculator },
    { id: 'lbo' as const, label: 'LBO ANALYSIS', icon: Building2 },
    { id: 'comps' as const, label: 'TRADING COMPS', icon: BarChart3 },
    { id: 'precedent' as const, label: 'PRECEDENT TXNS', icon: Layers },
  ];

  const lboSubTabs = [
    { id: 'returns' as const, label: 'RETURNS', icon: TrendingUp },
    { id: 'full_model' as const, label: 'FULL MODEL', icon: Layers },
    { id: 'debt_schedule' as const, label: 'DEBT SCHEDULE', icon: Calendar },
    { id: 'sensitivity' as const, label: 'SENSITIVITY', icon: Grid3X3 },
  ];

  const tooltipStyle = CHART_STYLE.tooltip;
  const axisStyle = CHART_STYLE.axis;
  const gridStyle = CHART_STYLE.grid;

  // Get the current method title for the input section header
  const getInputSectionTitle = (): string => {
    if (method === 'dcf') return 'DCF INPUTS';
    if (method === 'lbo') {
      if (lboSubTab === 'returns') return 'LBO RETURNS INPUTS ($M)';
      if (lboSubTab === 'full_model') return 'LBO MODEL INPUTS';
      if (lboSubTab === 'debt_schedule') return 'DEBT SCHEDULE INPUTS';
      if (lboSubTab === 'sensitivity') return 'SENSITIVITY INPUTS';
    }
    if (method === 'comps') return 'TRADING COMPS INPUTS';
    if (method === 'precedent') return 'PRECEDENT TRANSACTIONS INPUTS';
    return 'INPUTS';
  };

  // ==================== RENDER ====================

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* TOP BAR - Method Selector */}
      <div style={{
        display: 'flex',
        gap: '2px',
        padding: '8px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        flexShrink: 0,
      }}>
        {methods.map((m) => {
          const Icon = m.icon;
          const isActive = method === m.id || (method === 'lbo' && m.id === 'lbo');
          return (
            <button
              key={m.id}
              onClick={() => { setMethod(m.id as ValuationMethod); setResult(null); }}
              style={{
                padding: '6px 14px',
                backgroundColor: isActive ? `${MA_COLORS.valuation}15` : 'transparent',
                border: 'none',
                borderBottom: isActive ? `2px solid ${MA_COLORS.valuation}` : '2px solid transparent',
                color: isActive ? MA_COLORS.valuation : FINCEPT.GRAY,
                fontSize: '10px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: isActive ? 700 : 400,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                transition: 'all 0.15s',
                textTransform: 'uppercase' as const,
              }}
            >
              <Icon size={12} />
              {m.label}
            </button>
          );
        })}
      </div>

      {/* LBO Sub-tabs (only when LBO is active) */}
      {method === 'lbo' && (
        <div style={{
          display: 'flex',
          gap: '2px',
          padding: '4px 12px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
        }}>
          {lboSubTabs.map((t) => {
            const Icon = t.icon;
            const isActive = lboSubTab === t.id;
            return (
              <button
                key={t.id}
                onClick={() => { setLboSubTab(t.id); setResult(null); }}
                style={{
                  padding: '4px 10px',
                  backgroundColor: isActive ? `${FINCEPT.CYAN}15` : 'transparent',
                  border: 'none',
                  borderBottom: isActive ? `2px solid ${FINCEPT.CYAN}` : '2px solid transparent',
                  color: isActive ? FINCEPT.CYAN : FINCEPT.MUTED,
                  fontSize: '9px',
                  fontFamily: TYPOGRAPHY.MONO,
                  fontWeight: isActive ? 700 : 400,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  transition: 'all 0.15s',
                  textTransform: 'uppercase' as const,
                }}
              >
                <Icon size={10} />
                {t.label}
              </button>
            );
          })}
        </div>
      )}

      {/* SCROLLABLE CONTENT: Inputs + Results */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px 16px' }}>
        {/* Collapsible Input Section */}
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
            <Calculator size={12} color={MA_COLORS.valuation} />
            <span style={{
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              color: MA_COLORS.valuation,
              letterSpacing: '0.5px',
              textTransform: 'uppercase' as const,
            }}>
              {getInputSectionTitle()}
            </span>
            <div style={{ flex: 1 }} />
            {/* Run button in header */}
            <button
              onClick={(e) => { e.stopPropagation(); handleRun(); }}
              disabled={loading}
              style={{
                padding: '4px 14px',
                backgroundColor: MA_COLORS.valuation,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: '0.5px',
                cursor: loading ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: loading ? 0.6 : 1,
                textTransform: 'uppercase' as const,
              }}
            >
              <PlayCircle size={11} />
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
              {/* DCF Inputs */}
              {method === 'dcf' && (
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                  {renderInputField('EBIT ($M)', dcfInputs.ebit, (v) => setDcfInputs({ ...dcfInputs, ebit: parseFloat(v) || 0 }))}
                  {renderInputField('DEPRECIATION ($M)', dcfInputs.depreciation, (v) => setDcfInputs({ ...dcfInputs, depreciation: parseFloat(v) || 0 }))}
                  {renderInputField('CAPEX ($M)', dcfInputs.capex, (v) => setDcfInputs({ ...dcfInputs, capex: parseFloat(v) || 0 }))}
                  {renderInputField('CHANGE IN NWC ($M)', dcfInputs.changeInNwc, (v) => setDcfInputs({ ...dcfInputs, changeInNwc: parseFloat(v) || 0 }))}
                  {renderInputField('MARKET CAP ($M)', dcfInputs.marketCapM, (v) => setDcfInputs({ ...dcfInputs, marketCapM: parseFloat(v) || 0 }))}
                  {renderInputField('TOTAL DEBT ($M)', dcfInputs.debtM, (v) => setDcfInputs({ ...dcfInputs, debtM: parseFloat(v) || 0 }))}
                  {renderInputField('CASH ($M)', dcfInputs.cash, (v) => setDcfInputs({ ...dcfInputs, cash: parseFloat(v) || 0 }))}
                  {renderInputField('COST OF DEBT', dcfInputs.costOfDebt, (v) => setDcfInputs({ ...dcfInputs, costOfDebt: parseFloat(v) || 0 }))}
                  {renderInputField('TAX RATE', dcfInputs.taxRate, (v) => setDcfInputs({ ...dcfInputs, taxRate: parseFloat(v) || 0 }))}
                  {renderInputField('RISK-FREE RATE', dcfInputs.riskFreeRate, (v) => setDcfInputs({ ...dcfInputs, riskFreeRate: parseFloat(v) || 0 }))}
                  {renderInputField('BETA', dcfInputs.beta, (v) => setDcfInputs({ ...dcfInputs, beta: parseFloat(v) || 0 }))}
                  {renderInputField('TERMINAL GROWTH', dcfInputs.terminalGrowth, (v) => setDcfInputs({ ...dcfInputs, terminalGrowth: parseFloat(v) || 0 }))}
                  {renderInputField('SHARES OUTSTANDING (M)', dcfInputs.shares, (v) => setDcfInputs({ ...dcfInputs, shares: parseFloat(v) || 0 }))}
                </div>
              )}

              {/* LBO Returns Inputs */}
              {method === 'lbo' && lboSubTab === 'returns' && (
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                  {renderInputField('ENTRY VALUATION ($M)', lboInputs.entryValuation, (v) => setLboInputs({ ...lboInputs, entryValuation: parseFloat(v) || 0 }))}
                  {renderInputField('EXIT VALUATION ($M)', lboInputs.exitValuation, (v) => setLboInputs({ ...lboInputs, exitValuation: parseFloat(v) || 0 }))}
                  {renderInputField('EQUITY INVESTED ($M)', lboInputs.equityInvested, (v) => setLboInputs({ ...lboInputs, equityInvested: parseFloat(v) || 0 }))}
                  {renderInputField('HOLDING PERIOD (YRS)', lboInputs.holdingPeriod, (v) => setLboInputs({ ...lboInputs, holdingPeriod: parseInt(v) || 0 }))}
                </div>
              )}

              {/* Full LBO Model Inputs */}
              {method === 'lbo' && lboSubTab === 'full_model' && (
                <>
                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    COMPANY DATA ($M)
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '14px' }}>
                    {renderInputField('REVENUE', lboModelInputs.company_data.revenue, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      company_data: { ...lboModelInputs.company_data, revenue: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('EBITDA', lboModelInputs.company_data.ebitda, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      company_data: { ...lboModelInputs.company_data, ebitda: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('EBIT', lboModelInputs.company_data.ebit, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      company_data: { ...lboModelInputs.company_data, ebit: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('CAPEX', lboModelInputs.company_data.capex, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      company_data: { ...lboModelInputs.company_data, capex: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('NET WORKING CAPITAL', lboModelInputs.company_data.nwc, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      company_data: { ...lboModelInputs.company_data, nwc: parseFloat(v) || 0 }
                    }))}
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    TRANSACTION ASSUMPTIONS
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                    {renderInputField('PURCHASE PRICE ($M)', lboModelInputs.transaction_assumptions.purchase_price, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, purchase_price: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('ENTRY MULTIPLE', lboModelInputs.transaction_assumptions.entry_multiple, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, entry_multiple: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('EXIT MULTIPLE', lboModelInputs.transaction_assumptions.exit_multiple, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, exit_multiple: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('DEBT %', lboModelInputs.transaction_assumptions.debt_percent, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, debt_percent: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('EQUITY %', lboModelInputs.transaction_assumptions.equity_percent, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, equity_percent: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('PROJECTION YEARS', lboModelInputs.projection_years, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      projection_years: parseInt(v) || 5
                    }))}
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                    marginTop: '14px',
                  }}>
                    OPERATING ASSUMPTIONS
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                    {renderInputField('REVENUE GROWTH', lboModelInputs.transaction_assumptions.revenue_growth ?? 0.05, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, revenue_growth: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('EBITDA MARGIN', lboModelInputs.transaction_assumptions.ebitda_margin ?? 0.20, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, ebitda_margin: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('CAPEX % REV', lboModelInputs.transaction_assumptions.capex_pct_revenue ?? 0.03, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, capex_pct_revenue: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('NWC % REV', lboModelInputs.transaction_assumptions.nwc_pct_revenue ?? 0.05, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, nwc_pct_revenue: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('TAX RATE', lboModelInputs.transaction_assumptions.tax_rate ?? 0.21, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, tax_rate: parseFloat(v) || 0 }
                    }))}
                    {renderInputField('HURDLE IRR', lboModelInputs.transaction_assumptions.hurdle_irr ?? 0.20, (v) => setLboModelInputs({
                      ...lboModelInputs,
                      transaction_assumptions: { ...lboModelInputs.transaction_assumptions, hurdle_irr: parseFloat(v) || 0 }
                    }))}
                  </div>
                </>
              )}

              {/* Debt Schedule Inputs */}
              {method === 'lbo' && lboSubTab === 'debt_schedule' && (
                <>
                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    SENIOR DEBT
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '14px' }}>
                    {renderInputField('AMOUNT ($M)', debtScheduleInputs.seniorDebt, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, seniorDebt: parseFloat(v) || 0 }))}
                    {renderInputField('INTEREST RATE', debtScheduleInputs.seniorRate, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, seniorRate: parseFloat(v) || 0 }))}
                    {renderInputField('TERM (YEARS)', debtScheduleInputs.seniorTerm, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, seniorTerm: parseInt(v) || 0 }))}
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    SUBORDINATED DEBT
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '14px' }}>
                    {renderInputField('AMOUNT ($M)', debtScheduleInputs.subDebt, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, subDebt: parseFloat(v) || 0 }))}
                    {renderInputField('INTEREST RATE', debtScheduleInputs.subRate, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, subRate: parseFloat(v) || 0 }))}
                    {renderInputField('TERM (YEARS)', debtScheduleInputs.subTerm, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, subTerm: parseInt(v) || 0 }))}
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    REVOLVER & CASH FLOWS
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                    {renderInputField('REVOLVER ($M)', debtScheduleInputs.revolver, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, revolver: parseFloat(v) || 0 }))}
                    {renderInputField('REVOLVER RATE', debtScheduleInputs.revolverRate, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, revolverRate: parseFloat(v) || 0 }))}
                    {renderInputField('CASH SWEEP %', debtScheduleInputs.sweepPct, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, sweepPct: parseFloat(v) || 0 }))}
                    {renderInputField('EBITDA BY YEAR ($M)', debtScheduleInputs.ebitdaYears, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, ebitdaYears: v }), '100,110,121,...')}
                    {renderInputField('CAPEX BY YEAR ($M)', debtScheduleInputs.capexYears, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, capexYears: v }), '25,27,30,...')}
                  </div>
                </>
              )}

              {/* Sensitivity Inputs */}
              {method === 'lbo' && lboSubTab === 'sensitivity' && (
                <>
                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    BASE CASE
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '14px' }}>
                    {renderInputField('BASE REVENUE ($M)', sensitivityInputs.baseRevenue, (v) => setSensitivityInputs({ ...sensitivityInputs, baseRevenue: parseFloat(v) || 0 }))}
                    {renderInputField('EBITDA MARGIN', sensitivityInputs.baseEbitdaMargin, (v) => setSensitivityInputs({ ...sensitivityInputs, baseEbitdaMargin: parseFloat(v) || 0 }))}
                    {renderInputField('EXIT MULTIPLE', sensitivityInputs.baseExitMultiple, (v) => setSensitivityInputs({ ...sensitivityInputs, baseExitMultiple: parseFloat(v) || 0 }))}
                    {renderInputField('DEBT %', sensitivityInputs.baseDebtPct, (v) => setSensitivityInputs({ ...sensitivityInputs, baseDebtPct: parseFloat(v) || 0 }))}
                    {renderInputField('HOLDING PERIOD', sensitivityInputs.holdingPeriod, (v) => setSensitivityInputs({ ...sensitivityInputs, holdingPeriod: parseInt(v) || 0 }))}
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    SENSITIVITY SCENARIOS
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '10px' }}>
                    {renderInputField('REVENUE GROWTH RATES (COMMA-SEP)', sensitivityInputs.revenueGrowthScenarios, (v) => setSensitivityInputs({ ...sensitivityInputs, revenueGrowthScenarios: v }), '0.02,0.05,0.08,...')}
                    {renderInputField('EXIT MULTIPLES (COMMA-SEP)', sensitivityInputs.exitMultipleScenarios, (v) => setSensitivityInputs({ ...sensitivityInputs, exitMultipleScenarios: v }), '7.0,8.0,9.0,...')}
                  </div>
                </>
              )}

              {/* Comps Inputs */}
              {method === 'comps' && (
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '10px' }}>
                  {renderInputField('TARGET TICKER', compsInputs.targetTicker, (v) => setCompsInputs({ ...compsInputs, targetTicker: v.toUpperCase() }), 'AAPL')}
                  {renderInputField('COMPARABLE TICKERS (COMMA-SEP)', compsInputs.compTickers, (v) => setCompsInputs({ ...compsInputs, compTickers: v.toUpperCase() }), 'MSFT,GOOGL,META')}
                </div>
              )}

              {/* Precedent Transaction Inputs */}
              {method === 'precedent' && (
                <>
                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                  }}>
                    TARGET COMPANY ($M)
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '14px' }}>
                    {renderInputField('TARGET REVENUE', precedentInputs.targetRevenue, (v) => setPrecedentInputs({ ...precedentInputs, targetRevenue: parseFloat(v) || 0 }))}
                    {renderInputField('TARGET EBITDA', precedentInputs.targetEbitda, (v) => setPrecedentInputs({ ...precedentInputs, targetEbitda: parseFloat(v) || 0 }))}
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase' as const,
                    marginBottom: '8px',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                  }}>
                    <span>PRECEDENT TRANSACTIONS ($M)</span>
                    <button
                      onClick={addPrecedentTransaction}
                      style={{
                        padding: '3px 8px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.BORDER}`,
                        color: FINCEPT.GRAY,
                        fontSize: '9px',
                        fontFamily: TYPOGRAPHY.MONO,
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '3px',
                        borderRadius: '2px',
                      }}
                    >
                      <Plus size={10} /> ADD
                    </button>
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    {precedentInputs.transactions.map((txn, idx) => (
                      <div key={idx} style={{
                        backgroundColor: FINCEPT.HEADER_BG,
                        padding: '8px 10px',
                        borderRadius: '2px',
                        border: `1px solid ${FINCEPT.BORDER}`,
                      }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                          <input
                            type="text"
                            value={txn.name}
                            onChange={(e) => {
                              const updated = [...precedentInputs.transactions];
                              updated[idx].name = e.target.value;
                              setPrecedentInputs({ ...precedentInputs, transactions: updated });
                            }}
                            style={{ ...COMMON_STYLES.inputField, width: '120px', padding: '4px 6px', marginBottom: 0 }}
                          />
                          <button
                            onClick={() => removePrecedentTransaction(idx)}
                            style={{
                              padding: '3px',
                              backgroundColor: 'transparent',
                              border: `1px solid ${FINCEPT.BORDER}`,
                              color: FINCEPT.RED,
                              cursor: 'pointer',
                              display: 'flex',
                              alignItems: 'center',
                              borderRadius: '2px',
                            }}
                          >
                            <Minus size={10} />
                          </button>
                        </div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
                          <div>
                            <label style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED, textTransform: 'uppercase' as const, letterSpacing: '0.5px' }}>EV</label>
                            <input
                              type="text"
                              value={txn.ev}
                              onChange={(e) => {
                                const updated = [...precedentInputs.transactions];
                                updated[idx].ev = parseFloat(e.target.value) || 0;
                                setPrecedentInputs({ ...precedentInputs, transactions: updated });
                              }}
                              style={{ ...COMMON_STYLES.inputField, padding: '4px 6px' }}
                            />
                          </div>
                          <div>
                            <label style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED, textTransform: 'uppercase' as const, letterSpacing: '0.5px' }}>REV</label>
                            <input
                              type="text"
                              value={txn.revenue}
                              onChange={(e) => {
                                const updated = [...precedentInputs.transactions];
                                updated[idx].revenue = parseFloat(e.target.value) || 0;
                                setPrecedentInputs({ ...precedentInputs, transactions: updated });
                              }}
                              style={{ ...COMMON_STYLES.inputField, padding: '4px 6px' }}
                            />
                          </div>
                          <div>
                            <label style={{ fontSize: '9px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED, textTransform: 'uppercase' as const, letterSpacing: '0.5px' }}>EBITDA</label>
                            <input
                              type="text"
                              value={txn.ebitda}
                              onChange={(e) => {
                                const updated = [...precedentInputs.transactions];
                                updated[idx].ebitda = parseFloat(e.target.value) || 0;
                                setPrecedentInputs({ ...precedentInputs, transactions: updated });
                              }}
                              style={{ ...COMMON_STYLES.inputField, padding: '4px 6px' }}
                            />
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </>
              )}
            </div>
          )}
        </div>

        {/* RESULTS SECTION */}
        {result ? (
          <div>
            {/* ==================== DCF RESULTS ==================== */}
            {method === 'dcf' && result.equity_value_per_share && (
              <div>
                <MASectionHeader
                  title="DCF VALUATION RESULTS"
                  icon={<Calculator size={14} />}
                  accentColor={MA_COLORS.valuation}
                  action={
                    <MAExportButton
                      data={[{
                        'Enterprise Value': formatCurrency(result.enterprise_value || 0),
                        'Equity Value': formatCurrency(result.equity_value || 0),
                        'WACC': `${(result.wacc * 100).toFixed(2)}%`,
                        'Value/Share': `$${result.equity_value_per_share.toFixed(2)}`,
                      }]}
                      filename="dcf_valuation"
                      accentColor={MA_COLORS.valuation}
                    />
                  }
                />
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px', marginBottom: '16px' }}>
                  <MAMetricCard label="ENTERPRISE VALUE" value={formatCurrency(result.enterprise_value || 0)} accentColor={FINCEPT.CYAN} />
                  <MAMetricCard label="EQUITY VALUE" value={formatCurrency(result.equity_value || 0)} accentColor={MA_COLORS.valuation} />
                  <MAMetricCard label="WACC" value={`${(result.wacc * 100).toFixed(2)}%`} accentColor={FINCEPT.ORANGE} />
                  <MAMetricCard label="VALUE / SHARE" value={`$${result.equity_value_per_share.toFixed(2)}`} accentColor={FINCEPT.GREEN} />
                </div>
              </div>
            )}

            {/* ==================== LBO RETURNS RESULTS ==================== */}
            {method === 'lbo' && lboSubTab === 'returns' && result.irr && (() => {
              const equityInvested = lboInputs.equityInvested;
              const absoluteGain = (result.absolute_gain || 0) / 1e6;
              const exitEquity = equityInvested + absoluteGain;
              const waterfallData = [
                { name: 'Entry Equity', value: equityInvested, fill: FINCEPT.CYAN },
                { name: 'Gain', value: absoluteGain, fill: FINCEPT.GREEN },
                { name: 'Exit Equity', value: exitEquity, fill: MA_COLORS.valuation },
              ];

              return (
                <div>
                  <MASectionHeader
                    title="LBO RETURNS"
                    icon={<TrendingUp size={14} />}
                    accentColor={FINCEPT.GREEN}
                    action={
                      <MAExportButton
                        data={[{
                          'IRR': `${result.irr.toFixed(1)}%`,
                          'MOIC': `${result.moic.toFixed(2)}x`,
                          'Absolute Gain': formatCurrency(result.absolute_gain || 0),
                          'Gain %': `${(result.absolute_gain_pct || 0).toFixed(1)}%`,
                          'Holding Period': `${result.holding_period || 0} yrs`,
                          'Initial Equity': formatCurrency(result.initial_equity || 0),
                          'Exit Equity': formatCurrency(result.exit_equity || 0),
                        }]}
                        filename="lbo_returns"
                        accentColor={FINCEPT.GREEN}
                      />
                    }
                  />
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px', marginBottom: '16px' }}>
                    <MAMetricCard label="IRR" value={`${result.irr.toFixed(1)}%`} accentColor={FINCEPT.GREEN} />
                    <MAMetricCard label="MOIC" value={`${result.moic.toFixed(2)}x`} accentColor={FINCEPT.CYAN} />
                    <MAMetricCard label="ABSOLUTE GAIN" value={formatCurrency(result.absolute_gain || 0)} accentColor={MA_COLORS.valuation} />
                    <MAMetricCard label="GAIN %" value={`${(result.absolute_gain_pct || 0).toFixed(1)}%`} accentColor={FINCEPT.ORANGE} />
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '16px' }}>
                    <MAMetricCard label="INITIAL EQUITY" value={formatCurrency(result.initial_equity || 0)} accentColor={FINCEPT.GRAY} />
                    <MAMetricCard label="EXIT EQUITY" value={formatCurrency(result.exit_equity || 0)} accentColor={FINCEPT.WHITE} />
                    <MAMetricCard label="HOLDING PERIOD" value={`${result.holding_period || 0} yrs`} accentColor={FINCEPT.MUTED} />
                  </div>

                  {/* Waterfall Chart */}
                  <MAChartPanel
                    title="RETURNS WATERFALL ($M)"
                    icon={<BarChart3 size={12} />}
                    accentColor={MA_COLORS.valuation}
                    height={240}
                  >
                    <BarChart data={waterfallData}>
                      <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                      <XAxis dataKey="name" tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} />
                      <YAxis tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} tickFormatter={(v: number) => `$${v.toFixed(0)}M`} />
                      <Tooltip contentStyle={tooltipStyle} formatter={(value: number) => [`$${value.toFixed(1)}M`, '']} />
                      <Bar dataKey="value" radius={[2, 2, 0, 0]}>
                        {waterfallData.map((entry, index) => (
                          <Cell key={`cell-${index}`} fill={entry.fill} />
                        ))}
                      </Bar>
                    </BarChart>
                  </MAChartPanel>
                </div>
              );
            })()}

            {/* ==================== FULL LBO MODEL RESULTS ==================== */}
            {method === 'lbo' && lboSubTab === 'full_model' && result.data && (() => {
              const d = result.data;
              const txnSummary = d.transaction_summary || {};
              const capStruct = d.capital_structure || {};
              const leverage = capStruct.leverage_metrics || {};
              const projections = d.financial_projections || {};
              const creditMetrics = d.credit_metrics || {};
              const exitScenarios = d.exit_scenarios || {};
              const returnsAnalysis = d.returns_analysis || {};
              const debtSummary = d.debt_summary || {};

              // Build financial projections array
              const projYears = Object.keys(projections).sort((a, b) => Number(a) - Number(b));
              const projArray = projYears.map(y => ({ year: `Y${y}`, ...projections[y] }));

              // Build credit metrics array
              const coverageData = Object.entries(creditMetrics.interest_coverage || {}).map(([y, v]) => ({
                year: `Y${y}`, coverage: v as number,
                leverage: (creditMetrics.leverage_progression || {})[y] || 0,
              }));

              // Build exit scenarios array
              const exitArray = Object.entries(exitScenarios).map(([mult, scenario]: [string, any]) => {
                const ret = returnsAnalysis[mult] || {};
                return {
                  multiple: `${parseFloat(mult).toFixed(1)}x`,
                  exit_ev: scenario.exit_enterprise_value || 0,
                  exit_equity: scenario.exit_equity_value || 0,
                  irr: ret.annualized_return || 0,
                  moic: ret.moic || 0,
                  meets_hurdle: ret.hurdle_metrics?.meets_hurdle,
                };
              });

              const kvRow = (label: string, value: string, color: string = FINCEPT.WHITE) => (
                <div key={label} style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, padding: '2px 0' }}>
                  <span style={{ color: FINCEPT.GRAY }}>{label}</span>
                  <span style={{ color }}>{value}</span>
                </div>
              );

              return (
                <div>
                  <MASectionHeader title="FULL LBO MODEL" icon={<Layers size={14} />} accentColor={FINCEPT.CYAN}
                    action={<MAExportButton data={projArray.length ? projArray : [txnSummary]} filename="lbo_model" accentColor={FINCEPT.CYAN} />}
                  />

                  {/* Returns Summary */}
                  {d.returns && (
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px', marginBottom: '16px' }}>
                      <MAMetricCard label="IRR" value={`${(d.returns.irr * 100).toFixed(1)}%`} accentColor={FINCEPT.GREEN} />
                      <MAMetricCard label="MOIC" value={`${d.returns.moic.toFixed(2)}x`} accentColor={FINCEPT.CYAN} />
                      <MAMetricCard label="ENTRY EQUITY" value={formatCurrency(d.returns.entry_equity || 0)} accentColor={MA_COLORS.valuation} />
                      <MAMetricCard label="EXIT EQUITY" value={formatCurrency(d.returns.exit_equity || 0)} accentColor={FINCEPT.PURPLE} />
                    </div>
                  )}

                  {/* Transaction Summary */}
                  {txnSummary.entry_ebitda && (
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>TRANSACTION SUMMARY</div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px' }}>
                        <MAMetricCard label="TARGET" value={txnSummary.target_company || 'N/A'} accentColor={FINCEPT.WHITE} />
                        <MAMetricCard label="ENTRY EV" value={formatCurrency(txnSummary.entry_enterprise_value || 0)} accentColor={FINCEPT.CYAN} />
                        <MAMetricCard label="ENTRY MULTIPLE" value={`${(txnSummary.entry_multiple || 0).toFixed(1)}x`} accentColor={MA_COLORS.valuation} />
                        <MAMetricCard label="ENTRY EBITDA" value={formatCurrency(txnSummary.entry_ebitda || 0)} accentColor={FINCEPT.GREEN} />
                        <MAMetricCard label="HOLDING PERIOD" value={`${txnSummary.holding_period || 0} yrs`} accentColor={FINCEPT.ORANGE} />
                        <MAMetricCard label="EXIT YEAR" value={`${txnSummary.exit_year || 0}`} accentColor={FINCEPT.GRAY} />
                      </div>
                    </div>
                  )}

                  {/* Capital Structure */}
                  {leverage.total_debt != null && (
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>CAPITAL STRUCTURE</div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px', marginBottom: '10px' }}>
                        <MAMetricCard label="TOTAL DEBT" value={formatCurrency(leverage.total_debt || 0)} accentColor={FINCEPT.RED} />
                        <MAMetricCard label="TOTAL EQUITY" value={formatCurrency(leverage.total_equity || 0)} accentColor={FINCEPT.GREEN} />
                        <MAMetricCard label="DEBT/EBITDA" value={`${(leverage.debt_to_ebitda || 0).toFixed(1)}x`} accentColor={FINCEPT.ORANGE} />
                        <MAMetricCard label="DEBT/CAPITAL" value={`${(leverage.total_debt_to_capital || 0).toFixed(1)}%`} accentColor={FINCEPT.CYAN} />
                      </div>
                      {/* Debt Tranches Table */}
                      {capStruct.debt_tranches?.length > 0 && (
                        <MADataTable
                          columns={[
                            { key: 'name', label: 'Tranche', width: '100px', align: 'left' },
                            { key: 'type', label: 'Type', width: '90px', align: 'left' },
                            { key: 'amount', label: 'Amount', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(v)}</span> },
                            { key: 'percentage_of_total', label: '% Total', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.CYAN }}>{`${(v || 0).toFixed(1)}%`}</span> },
                            { key: 'interest_rate', label: 'Rate', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.ORANGE }}>{`${(v || 0).toFixed(1)}%`}</span> },
                          ]}
                          data={capStruct.debt_tranches}
                          accentColor={FINCEPT.RED}
                          compact
                        />
                      )}
                    </div>
                  )}

                  {/* Sources & Uses */}
                  {d.sources_uses && (
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>SOURCES & USES</div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
                        <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '10px 14px' }}>
                          <div style={{ fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.GREEN, marginBottom: '6px', letterSpacing: '0.5px' }}>SOURCES</div>
                          {Object.entries(d.sources_uses.sources || {}).map(([key, val]) => kvRow(key.replace(/_/g, ' ').toUpperCase(), formatCurrency(val as number), FINCEPT.GREEN))}
                        </div>
                        <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '10px 14px' }}>
                          <div style={{ fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.RED, marginBottom: '6px', letterSpacing: '0.5px' }}>USES</div>
                          {Object.entries(d.sources_uses.uses || {}).map(([key, val]) => kvRow(key.replace(/_/g, ' ').toUpperCase(), formatCurrency(val as number), FINCEPT.RED))}
                        </div>
                      </div>
                    </div>
                  )}

                  {/* Financial Projections */}
                  {projArray.length > 0 && (
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>FINANCIAL PROJECTIONS</div>
                      <MADataTable
                        columns={[
                          { key: 'year', label: 'Year', width: '50px', align: 'left' },
                          { key: 'revenue', label: 'Revenue', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(v)}</span> },
                          { key: 'revenue_growth', label: 'Growth', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.GREEN }}>{`${(v || 0).toFixed(1)}%`}</span> },
                          { key: 'ebitda', label: 'EBITDA', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.CYAN }}>{formatCurrency(v)}</span> },
                          { key: 'ebit', label: 'EBIT', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(v)}</span> },
                          { key: 'net_income', label: 'Net Inc', align: 'right', format: (v: number) => <span style={{ color: v >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>{formatCurrency(v)}</span> },
                          { key: 'free_cash_flow', label: 'FCF', align: 'right', format: (v: number) => <span style={{ color: v >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>{formatCurrency(v)}</span> },
                        ]}
                        data={projArray}
                        accentColor={FINCEPT.CYAN}
                        compact
                      />

                      {/* Revenue & EBITDA Chart */}
                      <div style={{ marginTop: '12px' }}>
                        <MAChartPanel title="REVENUE & EBITDA PROJECTION" icon={<TrendingUp size={12} />} accentColor={FINCEPT.CYAN} height={220}>
                          <BarChart data={projArray}>
                            <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                            <XAxis dataKey="year" tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} />
                            <YAxis tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} tickFormatter={(v: number) => `$${(v / 1e6).toFixed(0)}M`} />
                            <Tooltip contentStyle={tooltipStyle} formatter={(value: number) => [`$${(value / 1e6).toFixed(1)}M`, '']} />
                            <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                            <Bar dataKey="revenue" fill={`${FINCEPT.CYAN}80`} name="Revenue" radius={[2, 2, 0, 0]} />
                            <Bar dataKey="ebitda" fill={`${FINCEPT.GREEN}80`} name="EBITDA" radius={[2, 2, 0, 0]} />
                            <Bar dataKey="free_cash_flow" fill={`${MA_COLORS.valuation}80`} name="FCF" radius={[2, 2, 0, 0]} />
                          </BarChart>
                        </MAChartPanel>
                      </div>
                    </div>
                  )}

                  {/* Credit Metrics */}
                  {coverageData.length > 0 && (
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>CREDIT METRICS</div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '10px' }}>
                        <MAMetricCard label="INITIAL DEBT" value={formatCurrency(debtSummary.initial_debt || 0)} accentColor={FINCEPT.RED} />
                        <MAMetricCard label="TOTAL PAYDOWN" value={formatCurrency(debtSummary.total_paydown || 0)} accentColor={FINCEPT.GREEN} />
                        <MAMetricCard label="PAYDOWN %" value={`${(debtSummary.paydown_percentage || 0).toFixed(1)}%`} accentColor={FINCEPT.CYAN} />
                      </div>
                      <MAChartPanel title="LEVERAGE & COVERAGE PROGRESSION" icon={<TrendingUp size={12} />} accentColor={FINCEPT.ORANGE} height={220}>
                        <BarChart data={coverageData}>
                          <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                          <XAxis dataKey="year" tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} />
                          <YAxis tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} tickFormatter={(v: number) => `${v.toFixed(1)}x`} />
                          <Tooltip contentStyle={tooltipStyle} formatter={(value: number, name: string) => [`${(value as number).toFixed(2)}x`, name]} />
                          <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                          <Bar dataKey="coverage" fill={`${FINCEPT.GREEN}80`} name="Interest Coverage" radius={[2, 2, 0, 0]} />
                          <Bar dataKey="leverage" fill={`${FINCEPT.ORANGE}80`} name="Debt/EBITDA" radius={[2, 2, 0, 0]} />
                        </BarChart>
                      </MAChartPanel>
                    </div>
                  )}

                  {/* Exit Scenarios */}
                  {exitArray.length > 0 && (
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>EXIT SCENARIOS</div>
                      <MADataTable
                        columns={[
                          { key: 'multiple', label: 'Exit Mult', width: '70px', align: 'left' },
                          { key: 'exit_ev', label: 'Exit EV', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(v)}</span> },
                          { key: 'exit_equity', label: 'Exit Equity', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.GREEN }}>{formatCurrency(v)}</span> },
                          { key: 'irr', label: 'IRR', align: 'right', format: (v: number) => <span style={{ color: v >= 20 ? FINCEPT.GREEN : v >= 10 ? FINCEPT.ORANGE : FINCEPT.RED }}>{`${(v || 0).toFixed(1)}%`}</span> },
                          { key: 'moic', label: 'MOIC', align: 'right', format: (v: number) => <span style={{ color: v >= 2 ? FINCEPT.GREEN : v >= 1.5 ? FINCEPT.CYAN : FINCEPT.ORANGE }}>{`${(v || 0).toFixed(2)}x`}</span> },
                          { key: 'meets_hurdle', label: 'Hurdle', align: 'center', format: (v: boolean) => <span style={{ color: v ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: TYPOGRAPHY.BOLD }}>{v ? 'PASS' : 'FAIL'}</span> },
                        ]}
                        data={exitArray}
                        accentColor={MA_COLORS.valuation}
                        compact
                      />

                      {/* Exit IRR Bar Chart */}
                      <div style={{ marginTop: '12px' }}>
                        <MAChartPanel title="EXIT SCENARIO RETURNS" icon={<BarChart3 size={12} />} accentColor={FINCEPT.GREEN} height={200}>
                          <BarChart data={exitArray}>
                            <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                            <XAxis dataKey="multiple" tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} />
                            <YAxis tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} tickFormatter={(v: number) => `${v.toFixed(0)}%`} />
                            <Tooltip contentStyle={tooltipStyle} formatter={(value: number, name: string) => [name === 'irr' ? `${value.toFixed(1)}%` : `${value.toFixed(2)}x`, name.toUpperCase()]} />
                            <ReferenceLine y={20} stroke={FINCEPT.ORANGE} strokeDasharray="3 3" label={{ value: 'Hurdle', fill: FINCEPT.ORANGE, fontSize: 9 }} />
                            <Bar dataKey="irr" name="irr" radius={[2, 2, 0, 0]}>
                              {exitArray.map((entry, index) => (
                                <Cell key={`exit-${index}`} fill={entry.irr >= 20 ? FINCEPT.GREEN : entry.irr >= 10 ? FINCEPT.ORANGE : FINCEPT.RED} />
                              ))}
                            </Bar>
                          </BarChart>
                        </MAChartPanel>
                      </div>
                    </div>
                  )}
                </div>
              );
            })()}

            {/* ==================== DEBT SCHEDULE RESULTS ==================== */}
            {method === 'lbo' && lboSubTab === 'debt_schedule' && result.data && (
              <div>
                <MASectionHeader
                  title="DEBT SCHEDULE ANALYSIS"
                  icon={<Calendar size={14} />}
                  accentColor={FINCEPT.ORANGE}
                  action={
                    result.data.schedule && Array.isArray(result.data.schedule) ? (
                      <MAExportButton
                        data={result.data.schedule.map((row: any) => ({
                          Year: row.year,
                          Opening: formatCurrency(row.opening || 0),
                          Interest: formatCurrency(row.interest || 0),
                          Paydown: formatCurrency(row.paydown || 0),
                          Closing: formatCurrency(row.closing || 0),
                        }))}
                        filename="debt_schedule"
                        accentColor={FINCEPT.ORANGE}
                      />
                    ) : undefined
                  }
                />

                {result.data.summary && (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px', marginBottom: '16px' }}>
                    <MAMetricCard label="TOTAL DEBT" value={formatCurrency(result.data.summary.total_debt || 0)} accentColor={FINCEPT.WHITE} />
                    <MAMetricCard label="TOTAL INTEREST" value={formatCurrency(result.data.summary.total_interest || 0)} accentColor={FINCEPT.RED} />
                    <MAMetricCard label="TOTAL PAYDOWN" value={formatCurrency(result.data.summary.total_paydown || 0)} accentColor={FINCEPT.GREEN} />
                    <MAMetricCard label="EXIT DEBT" value={formatCurrency(result.data.summary.exit_debt || 0)} accentColor={FINCEPT.ORANGE} />
                  </div>
                )}

                {/* Debt Schedule Area Chart */}
                {result.data.schedule && Array.isArray(result.data.schedule) && result.data.schedule.length > 0 && (() => {
                  const debtChartData = result.data.schedule.map((row: any) => ({
                    year: `Y${row.year}`,
                    opening: (row.opening || 0) / 1e6,
                    interest: (row.interest || 0) / 1e6,
                    paydown: (row.paydown || 0) / 1e6,
                    closing: (row.closing || 0) / 1e6,
                  }));

                  return (
                    <div style={{ marginBottom: '16px' }}>
                      <MAChartPanel
                        title="DEBT PROFILE OVER TIME ($M)"
                        icon={<TrendingUp size={12} />}
                        accentColor={FINCEPT.ORANGE}
                        height={260}
                      >
                        <AreaChart data={debtChartData}>
                          <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                          <XAxis dataKey="year" tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} />
                          <YAxis tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} tickFormatter={(v: number) => `$${v.toFixed(0)}M`} />
                          <Tooltip contentStyle={tooltipStyle} formatter={(value: number) => [`$${value.toFixed(1)}M`, '']} />
                          <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                          <Area type="monotone" dataKey="opening" stroke={FINCEPT.CYAN} fill={`${FINCEPT.CYAN}20`} name="Opening Balance" />
                          <Area type="monotone" dataKey="paydown" stroke={FINCEPT.GREEN} fill={`${FINCEPT.GREEN}20`} name="Paydown" />
                        </AreaChart>
                      </MAChartPanel>
                    </div>
                  );
                })()}

                {/* Debt Schedule Table */}
                {result.data.schedule && Array.isArray(result.data.schedule) && (
                  <MADataTable
                    columns={[
                      { key: 'year', label: 'Year', width: '60px', align: 'left' },
                      { key: 'opening', label: 'Opening', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(v || 0)}</span> },
                      { key: 'interest', label: 'Interest', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.RED }}>{formatCurrency(v || 0)}</span> },
                      { key: 'paydown', label: 'Paydown', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.GREEN }}>{formatCurrency(v || 0)}</span> },
                      { key: 'closing', label: 'Closing', align: 'right', format: (v: number) => <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(v || 0)}</span> },
                    ]}
                    data={result.data.schedule}
                    accentColor={FINCEPT.ORANGE}
                    compact
                  />
                )}
              </div>
            )}

            {/* ==================== SENSITIVITY RESULTS ==================== */}
            {method === 'lbo' && lboSubTab === 'sensitivity' && result.data && (
              <div>
                <MASectionHeader
                  title="SENSITIVITY ANALYSIS"
                  icon={<Grid3X3 size={14} />}
                  accentColor={MA_COLORS.valuation}
                />

                {/* IRR Heatmap */}
                <div style={{ marginBottom: '24px' }}>
                  <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: '10px' }}>IRR SENSITIVITY TABLE</div>
                  <MASensitivityHeatmap
                    data={result.data?.irr_matrix || []}
                    rowLabels={result.revenueGrowths?.map((g: number) => `${(g * 100).toFixed(0)}%`) || []}
                    colLabels={result.exitMultiples?.map((m: number) => `${m.toFixed(1)}x`) || []}
                    rowHeader="Rev Growth"
                    colHeader="Exit Multiple"
                    formatValue={(v: number) => `${(v * 100).toFixed(1)}%`}
                    accentColor={FINCEPT.GREEN}
                  />
                </div>

                {/* MOIC Heatmap */}
                <div style={{ marginBottom: '16px' }}>
                  <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: '10px' }}>MOIC SENSITIVITY TABLE</div>
                  <MASensitivityHeatmap
                    data={result.data?.moic_matrix || []}
                    rowLabels={result.revenueGrowths?.map((g: number) => `${(g * 100).toFixed(0)}%`) || []}
                    colLabels={result.exitMultiples?.map((m: number) => `${m.toFixed(1)}x`) || []}
                    rowHeader="Rev Growth"
                    colHeader="Exit Multiple"
                    formatValue={(v: number) => `${v.toFixed(2)}x`}
                    accentColor={FINCEPT.CYAN}
                  />
                </div>
              </div>
            )}

            {/* ==================== TRADING COMPS RESULTS ==================== */}
            {method === 'comps' && result?.data?.comparables && (
              <div>
                <MASectionHeader
                  title="TRADING COMPS RESULTS"
                  icon={<BarChart3 size={14} />}
                  accentColor={FINCEPT.CYAN}
                  action={
                    <MAExportButton
                      data={result.data.comparables}
                      filename="trading_comps"
                      accentColor={FINCEPT.CYAN}
                    />
                  }
                />

                {/* Target & Summary Header */}
                {result.data.target_ticker && (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '16px' }}>
                    <MAMetricCard label="TARGET" value={result.data.target_ticker} accentColor={FINCEPT.CYAN} />
                    <MAMetricCard label="COMPARABLES" value={result.data.comp_count} accentColor={MA_COLORS.valuation} />
                    <MAMetricCard label="AS OF" value={result.data.as_of_date || '--'} accentColor={FINCEPT.GRAY} />
                  </div>
                )}

                {/* Implied Valuation from Comps */}
                {result.data.target_valuation?.valuations && (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '16px' }}>
                    <MAMetricCard label="EV/REVENUE (MEDIAN)" value={formatCurrency(result.data.target_valuation.valuations.ev_revenue_median || 0)} accentColor={FINCEPT.WHITE} />
                    <MAMetricCard label="EV/EBITDA (MEDIAN)" value={formatCurrency(result.data.target_valuation.valuations.ev_ebitda_median || 0)} accentColor={FINCEPT.WHITE} />
                    <MAMetricCard label="BLENDED MEDIAN" value={formatCurrency(result.data.target_valuation.valuations.blended_median || 0)} accentColor={FINCEPT.GREEN} />
                  </div>
                )}

                {/* Comps Multiples Bar Chart */}
                {result.data.comparables.length > 0 && (() => {
                  const compsChartData = result.data.comparables.map((row: any) => ({
                    name: row['Ticker'] || '',
                    'EV/Revenue': row['EV/Revenue'] || 0,
                    'EV/EBITDA': row['EV/EBITDA'] || 0,
                    'P/E': row['P/E'] || 0,
                  }));

                  return (
                    <div style={{ marginBottom: '16px' }}>
                      <MAChartPanel
                        title="COMPARABLE MULTIPLES"
                        icon={<BarChart3 size={12} />}
                        accentColor={FINCEPT.CYAN}
                        height={280}
                      >
                        <BarChart data={compsChartData}>
                          <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                          <XAxis dataKey="name" tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} />
                          <YAxis tick={axisStyle} axisLine={{ stroke: FINCEPT.BORDER }} tickFormatter={(v: number) => `${v.toFixed(1)}x`} />
                          <Tooltip contentStyle={tooltipStyle} formatter={(value: number) => [`${value.toFixed(1)}x`, '']} />
                          <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                          <Bar dataKey="EV/Revenue" fill={CHART_PALETTE[0]} radius={[2, 2, 0, 0]} />
                          <Bar dataKey="EV/EBITDA" fill={CHART_PALETTE[1]} radius={[2, 2, 0, 0]} />
                          <Bar dataKey="P/E" fill={CHART_PALETTE[2]} radius={[2, 2, 0, 0]} />
                        </BarChart>
                      </MAChartPanel>
                    </div>
                  );
                })()}

                {/* Comp Table */}
                <MADataTable
                  columns={[
                    { key: 'Ticker', label: 'Ticker', width: '80px', align: 'left', format: (v: string) => <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{v}</span> },
                    { key: 'Company', label: 'Company', width: '140px', align: 'left' },
                    { key: 'Market Cap ($M)', label: 'Mkt Cap ($M)', align: 'right', format: (v: number) => (v || 0).toLocaleString(undefined, { maximumFractionDigits: 0 }) },
                    { key: 'EV ($M)', label: 'EV ($M)', align: 'right', format: (v: number) => (v || 0).toLocaleString(undefined, { maximumFractionDigits: 0 }) },
                    { key: 'EV/Revenue', label: 'EV/Rev', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}x` },
                    { key: 'EV/EBITDA', label: 'EV/EBITDA', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}x` },
                    { key: 'EV/EBIT', label: 'EV/EBIT', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}x` },
                    { key: 'P/E', label: 'P/E', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}x` },
                    { key: 'Rev Growth (%)', label: 'Rev Gr.%', align: 'right', format: (v: number, row: any) => <span style={{ color: (row['Rev Growth (%)'] || 0) >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>{(v || 0).toFixed(1)}%</span> },
                    { key: 'EBITDA Margin (%)', label: 'EBITDA Mg.%', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}%` },
                    { key: 'Net Margin (%)', label: 'Net Mg.%', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}%` },
                    { key: 'ROE (%)', label: 'ROE%', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}%` },
                  ]}
                  data={[
                    ...result.data.comparables,
                    ...(result.data.summary_statistics?.median ? [{
                      'Ticker': 'MEDIAN',
                      'Company': '--',
                      'Market Cap ($M)': result.data.summary_statistics.median['Market Cap ($M)'] || 0,
                      'EV ($M)': result.data.summary_statistics.median['EV ($M)'] || 0,
                      'EV/Revenue': result.data.summary_statistics.median['EV/Revenue'] || 0,
                      'EV/EBITDA': result.data.summary_statistics.median['EV/EBITDA'] || 0,
                      'EV/EBIT': result.data.summary_statistics.median['EV/EBIT'] || 0,
                      'P/E': result.data.summary_statistics.median['P/E'] || 0,
                      'Rev Growth (%)': result.data.summary_statistics.median['Rev Growth (%)'] || 0,
                      'EBITDA Margin (%)': result.data.summary_statistics.median['EBITDA Margin (%)'] || 0,
                      'Net Margin (%)': result.data.summary_statistics.median['Net Margin (%)'] || 0,
                      'ROE (%)': result.data.summary_statistics.median['ROE (%)'] || 0,
                    }] : []),
                  ]}
                  accentColor={FINCEPT.CYAN}
                  compact
                  maxHeight="350px"
                />
              </div>
            )}

            {/* ==================== PRECEDENT TRANSACTIONS RESULTS ==================== */}
            {method === 'precedent' && result?.data?.comparables && (
              <div>
                <MASectionHeader
                  title="PRECEDENT TRANSACTIONS"
                  icon={<Layers size={14} />}
                  accentColor={MA_COLORS.valuation}
                  action={
                    <MAExportButton
                      data={result.data.comparables}
                      filename="precedent_transactions"
                      accentColor={MA_COLORS.valuation}
                    />
                  }
                />

                {/* Implied Valuation Summary */}
                {result.data.target_valuation?.valuations && (() => {
                  const v = result.data.target_valuation.valuations;
                  const low = v.ev_ebitda_q1 || v.ev_revenue_q1 || 0;
                  const mid = v.blended_median || v.ev_ebitda_median || v.ev_revenue_median || 0;
                  const high = v.ev_ebitda_q3 || v.ev_revenue_q3 || 0;
                  return (
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '10px', marginBottom: '16px' }}>
                      <MAMetricCard label="LOW IMPLIED EV (Q1)" value={formatCurrency(low)} accentColor={FINCEPT.RED} />
                      <MAMetricCard label="BLENDED IMPLIED EV" value={formatCurrency(mid)} accentColor={FINCEPT.CYAN} />
                      <MAMetricCard label="HIGH IMPLIED EV (Q3)" value={formatCurrency(high)} accentColor={FINCEPT.GREEN} />
                    </div>
                  );
                })()}

                {/* Implied EV by Method */}
                {result.data.target_valuation?.valuations && (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '10px', marginBottom: '16px' }}>
                    {result.data.target_valuation.valuations.ev_revenue_median != null && (
                      <MAMetricCard label="EV/REVENUE IMPLIED" value={formatCurrency(result.data.target_valuation.valuations.ev_revenue_median)} accentColor={FINCEPT.WHITE} />
                    )}
                    {result.data.target_valuation.valuations.ev_ebitda_median != null && (
                      <MAMetricCard label="EV/EBITDA IMPLIED" value={formatCurrency(result.data.target_valuation.valuations.ev_ebitda_median)} accentColor={FINCEPT.WHITE} />
                    )}
                  </div>
                )}

                {/* Transaction Multiples Summary */}
                {result.data.summary_statistics && (
                  <div style={{ marginBottom: '16px' }}>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>TRANSACTION MULTIPLES</div>
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px' }}>
                      {[
                        { label: 'EV/Revenue', median: result.data.summary_statistics.median?.['EV/Revenue'], mean: result.data.summary_statistics.mean?.['EV/Revenue'], isPct: false, isCurrency: false },
                        { label: 'EV/EBITDA', median: result.data.summary_statistics.median?.['EV/EBITDA'], mean: result.data.summary_statistics.mean?.['EV/EBITDA'], isPct: false, isCurrency: false },
                        { label: 'Premium 1D%', median: result.data.summary_statistics.median?.['Premium 1-Day (%)'], mean: result.data.summary_statistics.mean?.['Premium 1-Day (%)'], isPct: true, isCurrency: false },
                        { label: 'Deal Val ($M)', median: result.data.summary_statistics.median?.['Deal Value ($M)'], mean: result.data.summary_statistics.mean?.['Deal Value ($M)'], isPct: false, isCurrency: true },
                      ].map((m) => (
                        <div key={m.label} style={{
                          backgroundColor: FINCEPT.PANEL_BG,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          padding: '10px 12px',
                        }}>
                          <div style={{ fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.MUTED, marginBottom: '6px', letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{m.label}</div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, padding: '2px 0' }}>
                            <span style={{ color: FINCEPT.GRAY }}>Median:</span>
                            <span style={{ color: FINCEPT.CYAN }}>{m.isCurrency ? formatCurrency((m.median || 0) * 1_000_000) : `${(m.median || 0).toFixed(2)}${m.isPct ? '%' : 'x'}`}</span>
                          </div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, padding: '2px 0' }}>
                            <span style={{ color: FINCEPT.GRAY }}>Mean:</span>
                            <span style={{ color: FINCEPT.WHITE }}>{m.isCurrency ? formatCurrency((m.mean || 0) * 1_000_000) : `${(m.mean || 0).toFixed(2)}${m.isPct ? '%' : 'x'}`}</span>
                          </div>
                        </div>
                      ))}
                    </div>
                  </div>
                )}

                {/* Precedent Scatter Plot */}
                {result.data.comparables.length > 0 && (() => {
                  const scatterData = result.data.comparables.map((row: any) => ({
                    x: row['EV/Revenue'] || 0,
                    y: row['EV/EBITDA'] || 0,
                    name: row['Target'] || row['target_name'] || '',
                    dealValue: row['Deal Value ($M)'] || 0,
                  }));

                  return (
                    <div style={{ marginBottom: '16px' }}>
                      <MAChartPanel
                        title="TRANSACTION MULTIPLES SCATTER (EV/REV vs EV/EBITDA)"
                        icon={<Grid3X3 size={12} />}
                        accentColor={MA_COLORS.valuation}
                        height={280}
                      >
                        <ScatterChart>
                          <CartesianGrid strokeDasharray={gridStyle.strokeDasharray} stroke={gridStyle.stroke} />
                          <XAxis
                            type="number"
                            dataKey="x"
                            name="EV/Revenue"
                            tick={axisStyle}
                            axisLine={{ stroke: FINCEPT.BORDER }}
                            label={{ value: 'EV/Revenue', position: 'insideBottom', offset: -5, style: { ...CHART_STYLE.label, fill: FINCEPT.GRAY } }}
                          />
                          <YAxis
                            type="number"
                            dataKey="y"
                            name="EV/EBITDA"
                            tick={axisStyle}
                            axisLine={{ stroke: FINCEPT.BORDER }}
                            label={{ value: 'EV/EBITDA', angle: -90, position: 'insideLeft', offset: 10, style: { ...CHART_STYLE.label, fill: FINCEPT.GRAY } }}
                          />
                          <Tooltip
                            contentStyle={tooltipStyle}
                            formatter={(value: number, name: string) => [
                              `${value.toFixed(2)}x`,
                              name === 'x' ? 'EV/Revenue' : 'EV/EBITDA'
                            ]}
                            labelFormatter={(label: string) => `${label}`}
                          />
                          <Scatter
                            data={scatterData}
                            fill={MA_COLORS.valuation}
                            name="Transactions"
                          >
                            {scatterData.map((_: any, index: number) => (
                              <Cell key={`cell-${index}`} fill={CHART_PALETTE[index % CHART_PALETTE.length]} />
                            ))}
                          </Scatter>
                        </ScatterChart>
                      </MAChartPanel>
                    </div>
                  );
                })()}

                {/* Comparable Transactions Table */}
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>COMPARABLE TRANSACTIONS ({result.data.comp_count})</div>
                <MADataTable
                  columns={[
                    { key: 'Date', label: 'Date', width: '80px', align: 'left', format: (v: string) => <span style={{ color: FINCEPT.GRAY }}>{v || '--'}</span> },
                    { key: 'Acquirer', label: 'Acquirer', width: '120px', align: 'left' },
                    { key: 'Target', label: 'Target', width: '120px', align: 'left', format: (v: string) => <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{v}</span> },
                    { key: 'Deal Value ($M)', label: 'Deal ($M)', align: 'right', format: (v: number) => (v || 0).toLocaleString(undefined, { maximumFractionDigits: 0 }) },
                    { key: 'EV/Revenue', label: 'EV/Rev', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}x` },
                    { key: 'EV/EBITDA', label: 'EV/EBITDA', align: 'right', format: (v: number) => `${(v || 0).toFixed(1)}x` },
                    { key: 'Premium 1-Day (%)', label: 'Prem. 1D%', align: 'right', format: (v: number) => <span style={{ color: (v || 0) > 0 ? FINCEPT.GREEN : FINCEPT.GRAY }}>{(v || 0).toFixed(1)}%</span> },
                    { key: 'Payment', label: 'Payment', width: '80px', align: 'left', format: (v: string) => v || '--' },
                  ]}
                  data={[
                    ...result.data.comparables,
                    ...(result.data.summary_statistics?.median ? [{
                      'Date': 'MEDIAN',
                      'Acquirer': '--',
                      'Target': '--',
                      'Deal Value ($M)': result.data.summary_statistics.median['Deal Value ($M)'] || 0,
                      'EV/Revenue': result.data.summary_statistics.median['EV/Revenue'] || 0,
                      'EV/EBITDA': result.data.summary_statistics.median['EV/EBITDA'] || 0,
                      'Premium 1-Day (%)': result.data.summary_statistics.median['Premium 1-Day (%)'] || 0,
                      'Payment': '--',
                    }] : []),
                  ]}
                  accentColor={MA_COLORS.valuation}
                  compact
                  maxHeight="350px"
                />
              </div>
            )}

            {/* Fallback: Raw JSON for unhandled result formats */}
            {!((method === 'dcf' && result?.equity_value_per_share) ||
               (method === 'lbo') ||
               (method === 'comps' && result?.data?.comparables) ||
               (method === 'precedent' && result?.data?.comparables)) && (
              <div>
                <MASectionHeader title="DETAILED RESULTS" icon={<DollarSign size={14} />} accentColor={FINCEPT.GRAY} />
                <pre style={{
                  fontSize: TYPOGRAPHY.TINY,
                  fontFamily: TYPOGRAPHY.MONO,
                  color: FINCEPT.GRAY,
                  backgroundColor: FINCEPT.PANEL_BG,
                  padding: SPACING.DEFAULT,
                  borderRadius: '2px',
                  overflow: 'auto',
                  maxHeight: '300px',
                  border: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  {JSON.stringify(result, null, 2)}
                </pre>
              </div>
            )}
          </div>
        ) : (
          <MAEmptyState
            icon={<Calculator size={48} />}
            title="SELECT A VALUATION METHOD"
            description="Configure inputs and run DCF, LBO, Trading Comps, or Precedent Transaction analysis"
            accentColor={MA_COLORS.valuation}
          />
        )}
      </div>
    </div>
  );
};
