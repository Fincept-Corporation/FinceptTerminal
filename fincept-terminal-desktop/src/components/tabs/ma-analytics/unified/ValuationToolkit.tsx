/**
 * Valuation Toolkit - Unified DCF, LBO, Trading Comps, Precedent Transactions
 *
 * Complete valuation toolkit with full LBO modeling, debt schedules, and sensitivity analysis
 */

import React, { useState } from 'react';
import { Calculator, TrendingUp, Building2, BarChart3, PlayCircle, Layers, Grid3X3, DollarSign, Calendar, Plus, Minus } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, LBOInputs } from '@/services/maAnalyticsService';
import { showSuccess, showError, showWarning } from '@/utils/notifications';

type ValuationMethod = 'dcf' | 'lbo' | 'lbo_model' | 'debt_schedule' | 'lbo_sensitivity' | 'comps' | 'precedent';
type LBOSubTab = 'returns' | 'full_model' | 'debt_schedule' | 'sensitivity';

export const ValuationToolkit: React.FC = () => {
  const [method, setMethod] = useState<ValuationMethod>('dcf');
  const [lboSubTab, setLboSubTab] = useState<LBOSubTab>('returns');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);

  // DCF Inputs
  const [dcfInputs, setDcfInputs] = useState({
    ebit: 500,
    taxRate: 0.21,
    riskFreeRate: 0.045,
    beta: 1.2,
    terminalGrowth: 0.025,
    shares: 100,
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
          cost_of_debt: 0.05,
          tax_rate: dcfInputs.taxRate,
          market_value_equity: 5000000000,
          market_value_debt: 1000000000,
        },
        fcf_inputs: {
          ebit: dcfInputs.ebit * 1000000,
          tax_rate: dcfInputs.taxRate,
          depreciation: 100000000,
          capex: 150000000,
          change_in_nwc: 20000000,
        },
        growth_rates: [0.15, 0.12, 0.10, 0.08, 0.06],
        terminal_growth: dcfInputs.terminalGrowth,
        balance_sheet: {
          cash: 500000000,
          debt: 1000000000,
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

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT - Method Selector & Inputs */}
      <div style={{
        width: '400px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {/* Method Selector */}
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
            VALUATION METHOD
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.TINY }}>
            {[
              { id: 'dcf', label: 'DCF MODEL', icon: Calculator },
              { id: 'lbo', label: 'LBO ANALYSIS', icon: Building2 },
              { id: 'comps', label: 'TRADING COMPS', icon: BarChart3 },
              { id: 'precedent', label: 'PRECEDENT TXNS', icon: Layers },
            ].map((m) => {
              const Icon = m.icon;
              return (
                <button
                  key={m.id}
                  onClick={() => { setMethod(m.id as ValuationMethod); setResult(null); }}
                  style={{
                    ...COMMON_STYLES.tabButton(method === m.id || (method === 'lbo' && m.id === 'lbo')),
                    width: '100%',
                    justifyContent: 'flex-start',
                  }}
                >
                  <Icon size={12} style={{ marginRight: SPACING.SMALL }} />
                  {m.label}
                </button>
              );
            })}
          </div>

          {/* LBO Sub-tabs */}
          {method === 'lbo' && (
            <div style={{ marginTop: SPACING.DEFAULT }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
                LBO ANALYSIS TYPE
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.TINY }}>
                {[
                  { id: 'returns', label: 'Returns', icon: TrendingUp },
                  { id: 'full_model', label: 'Full Model', icon: Layers },
                  { id: 'debt_schedule', label: 'Debt Sched', icon: Calendar },
                  { id: 'sensitivity', label: 'Sensitivity', icon: Grid3X3 },
                ].map((t) => {
                  const Icon = t.icon;
                  return (
                    <button
                      key={t.id}
                      onClick={() => { setLboSubTab(t.id as LBOSubTab); setResult(null); }}
                      style={{
                        ...COMMON_STYLES.tabButton(lboSubTab === t.id),
                        padding: '6px 8px',
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
          {method === 'dcf' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>DCF INPUTS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('EBIT ($M)', dcfInputs.ebit, (v) => setDcfInputs({ ...dcfInputs, ebit: parseFloat(v) || 0 }))}
                {renderInputField('Tax Rate', dcfInputs.taxRate, (v) => setDcfInputs({ ...dcfInputs, taxRate: parseFloat(v) || 0 }))}
                {renderInputField('Risk-Free Rate', dcfInputs.riskFreeRate, (v) => setDcfInputs({ ...dcfInputs, riskFreeRate: parseFloat(v) || 0 }))}
                {renderInputField('Beta', dcfInputs.beta, (v) => setDcfInputs({ ...dcfInputs, beta: parseFloat(v) || 0 }))}
                {renderInputField('Terminal Growth', dcfInputs.terminalGrowth, (v) => setDcfInputs({ ...dcfInputs, terminalGrowth: parseFloat(v) || 0 }))}
                {renderInputField('Shares Outstanding (M)', dcfInputs.shares, (v) => setDcfInputs({ ...dcfInputs, shares: parseFloat(v) || 0 }))}
              </div>
            </>
          )}

          {method === 'lbo' && lboSubTab === 'returns' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>LBO RETURNS ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Entry Valuation', lboInputs.entryValuation, (v) => setLboInputs({ ...lboInputs, entryValuation: parseFloat(v) || 0 }))}
                {renderInputField('Exit Valuation', lboInputs.exitValuation, (v) => setLboInputs({ ...lboInputs, exitValuation: parseFloat(v) || 0 }))}
                {renderInputField('Equity Invested', lboInputs.equityInvested, (v) => setLboInputs({ ...lboInputs, equityInvested: parseFloat(v) || 0 }))}
                {renderInputField('Holding Period (Years)', lboInputs.holdingPeriod, (v) => setLboInputs({ ...lboInputs, holdingPeriod: parseInt(v) || 0 }))}
              </div>
            </>
          )}

          {method === 'lbo' && lboSubTab === 'full_model' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>COMPANY DATA ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Revenue', lboModelInputs.company_data.revenue, (v) => setLboModelInputs({
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
                {renderInputField('CapEx', lboModelInputs.company_data.capex, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  company_data: { ...lboModelInputs.company_data, capex: parseFloat(v) || 0 }
                }))}
                {renderInputField('Net Working Capital', lboModelInputs.company_data.nwc, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  company_data: { ...lboModelInputs.company_data, nwc: parseFloat(v) || 0 }
                }))}
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>TRANSACTION ASSUMPTIONS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Purchase Price ($M)', lboModelInputs.transaction_assumptions.purchase_price, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  transaction_assumptions: { ...lboModelInputs.transaction_assumptions, purchase_price: parseFloat(v) || 0 }
                }))}
                {renderInputField('Entry Multiple', lboModelInputs.transaction_assumptions.entry_multiple, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  transaction_assumptions: { ...lboModelInputs.transaction_assumptions, entry_multiple: parseFloat(v) || 0 }
                }))}
                {renderInputField('Exit Multiple', lboModelInputs.transaction_assumptions.exit_multiple, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  transaction_assumptions: { ...lboModelInputs.transaction_assumptions, exit_multiple: parseFloat(v) || 0 }
                }))}
                {renderInputField('Debt %', lboModelInputs.transaction_assumptions.debt_percent, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  transaction_assumptions: { ...lboModelInputs.transaction_assumptions, debt_percent: parseFloat(v) || 0 }
                }))}
                {renderInputField('Equity %', lboModelInputs.transaction_assumptions.equity_percent, (v) => setLboModelInputs({
                  ...lboModelInputs,
                  transaction_assumptions: { ...lboModelInputs.transaction_assumptions, equity_percent: parseFloat(v) || 0 }
                }))}
              </div>

              {renderInputField('Projection Years', lboModelInputs.projection_years, (v) => setLboModelInputs({
                ...lboModelInputs,
                projection_years: parseInt(v) || 5
              }))}
            </>
          )}

          {method === 'lbo' && lboSubTab === 'debt_schedule' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>SENIOR DEBT</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Amount ($M)', debtScheduleInputs.seniorDebt, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, seniorDebt: parseFloat(v) || 0 }))}
                {renderInputField('Interest Rate', debtScheduleInputs.seniorRate, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, seniorRate: parseFloat(v) || 0 }))}
                {renderInputField('Term (Years)', debtScheduleInputs.seniorTerm, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, seniorTerm: parseInt(v) || 0 }))}
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>SUBORDINATED DEBT</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Amount ($M)', debtScheduleInputs.subDebt, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, subDebt: parseFloat(v) || 0 }))}
                {renderInputField('Interest Rate', debtScheduleInputs.subRate, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, subRate: parseFloat(v) || 0 }))}
                {renderInputField('Term (Years)', debtScheduleInputs.subTerm, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, subTerm: parseInt(v) || 0 }))}
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>REVOLVER</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Amount ($M)', debtScheduleInputs.revolver, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, revolver: parseFloat(v) || 0 }))}
                {renderInputField('Interest Rate', debtScheduleInputs.revolverRate, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, revolverRate: parseFloat(v) || 0 }))}
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>CASH FLOWS ($M, comma-separated)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('EBITDA by Year', debtScheduleInputs.ebitdaYears, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, ebitdaYears: v }), '100,110,121,...')}
                {renderInputField('CapEx by Year', debtScheduleInputs.capexYears, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, capexYears: v }), '25,27,30,...')}
              </div>

              {renderInputField('Cash Sweep %', debtScheduleInputs.sweepPct, (v) => setDebtScheduleInputs({ ...debtScheduleInputs, sweepPct: parseFloat(v) || 0 }))}
            </>
          )}

          {method === 'lbo' && lboSubTab === 'sensitivity' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>BASE CASE</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Base Revenue ($M)', sensitivityInputs.baseRevenue, (v) => setSensitivityInputs({ ...sensitivityInputs, baseRevenue: parseFloat(v) || 0 }))}
                {renderInputField('EBITDA Margin', sensitivityInputs.baseEbitdaMargin, (v) => setSensitivityInputs({ ...sensitivityInputs, baseEbitdaMargin: parseFloat(v) || 0 }))}
                {renderInputField('Exit Multiple', sensitivityInputs.baseExitMultiple, (v) => setSensitivityInputs({ ...sensitivityInputs, baseExitMultiple: parseFloat(v) || 0 }))}
                {renderInputField('Debt %', sensitivityInputs.baseDebtPct, (v) => setSensitivityInputs({ ...sensitivityInputs, baseDebtPct: parseFloat(v) || 0 }))}
                {renderInputField('Holding Period', sensitivityInputs.holdingPeriod, (v) => setSensitivityInputs({ ...sensitivityInputs, holdingPeriod: parseInt(v) || 0 }))}
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>SENSITIVITY SCENARIOS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Revenue Growth Rates (comma-sep)', sensitivityInputs.revenueGrowthScenarios, (v) => setSensitivityInputs({ ...sensitivityInputs, revenueGrowthScenarios: v }), '0.02,0.05,0.08,...')}
                {renderInputField('Exit Multiples (comma-sep)', sensitivityInputs.exitMultipleScenarios, (v) => setSensitivityInputs({ ...sensitivityInputs, exitMultipleScenarios: v }), '7.0,8.0,9.0,...')}
              </div>
            </>
          )}

          {method === 'comps' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>TRADING COMPS</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {renderInputField('Target Ticker', compsInputs.targetTicker, (v) => setCompsInputs({ ...compsInputs, targetTicker: v.toUpperCase() }), 'AAPL')}
                {renderInputField('Comparable Tickers (comma-sep)', compsInputs.compTickers, (v) => setCompsInputs({ ...compsInputs, compTickers: v.toUpperCase() }), 'MSFT,GOOGL,META')}
              </div>
            </>
          )}

          {method === 'precedent' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>TARGET COMPANY ($M)</div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                {renderInputField('Target Revenue', precedentInputs.targetRevenue, (v) => setPrecedentInputs({ ...precedentInputs, targetRevenue: parseFloat(v) || 0 }))}
                {renderInputField('Target EBITDA', precedentInputs.targetEbitda, (v) => setPrecedentInputs({ ...precedentInputs, targetEbitda: parseFloat(v) || 0 }))}
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span>PRECEDENT TRANSACTIONS ($M)</span>
                <button
                  onClick={addPrecedentTransaction}
                  style={{ ...COMMON_STYLES.tabButton(false), padding: '4px 8px' }}
                >
                  <Plus size={12} />
                </button>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                {precedentInputs.transactions.map((txn, idx) => (
                  <div key={idx} style={{
                    backgroundColor: FINCEPT.HEADER_BG,
                    padding: SPACING.SMALL,
                    borderRadius: '2px',
                    border: `1px solid ${FINCEPT.BORDER}`,
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: SPACING.TINY }}>
                      <input
                        type="text"
                        value={txn.name}
                        onChange={(e) => {
                          const updated = [...precedentInputs.transactions];
                          updated[idx].name = e.target.value;
                          setPrecedentInputs({ ...precedentInputs, transactions: updated });
                        }}
                        style={{ ...COMMON_STYLES.inputField, width: '120px', marginBottom: 0 }}
                      />
                      <button
                        onClick={() => removePrecedentTransaction(idx)}
                        style={{ ...COMMON_STYLES.tabButton(false), padding: '4px', color: FINCEPT.RED }}
                      >
                        <Minus size={12} />
                      </button>
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.TINY }}>
                      <div>
                        <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>EV</label>
                        <input
                          type="text"
                          value={txn.ev}
                          onChange={(e) => {
                            const updated = [...precedentInputs.transactions];
                            updated[idx].ev = parseFloat(e.target.value) || 0;
                            setPrecedentInputs({ ...precedentInputs, transactions: updated });
                          }}
                          style={{ ...COMMON_STYLES.inputField, padding: '4px' }}
                        />
                      </div>
                      <div>
                        <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Rev</label>
                        <input
                          type="text"
                          value={txn.revenue}
                          onChange={(e) => {
                            const updated = [...precedentInputs.transactions];
                            updated[idx].revenue = parseFloat(e.target.value) || 0;
                            setPrecedentInputs({ ...precedentInputs, transactions: updated });
                          }}
                          style={{ ...COMMON_STYLES.inputField, padding: '4px' }}
                        />
                      </div>
                      <div>
                        <label style={{ fontSize: '9px', color: FINCEPT.GRAY }}>EBITDA</label>
                        <input
                          type="text"
                          value={txn.ebitda}
                          onChange={(e) => {
                            const updated = [...precedentInputs.transactions];
                            updated[idx].ebitda = parseFloat(e.target.value) || 0;
                            setPrecedentInputs({ ...precedentInputs, transactions: updated });
                          }}
                          style={{ ...COMMON_STYLES.inputField, padding: '4px' }}
                        />
                      </div>
                    </div>
                  </div>
                ))}
              </div>
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
            {loading ? 'CALCULATING...' : 'RUN VALUATION'}
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
              {method === 'dcf' && 'DCF VALUATION RESULTS'}
              {method === 'lbo' && lboSubTab === 'returns' && 'LBO RETURNS'}
              {method === 'lbo' && lboSubTab === 'full_model' && 'FULL LBO MODEL'}
              {method === 'lbo' && lboSubTab === 'debt_schedule' && 'DEBT SCHEDULE ANALYSIS'}
              {method === 'lbo' && lboSubTab === 'sensitivity' && 'SENSITIVITY ANALYSIS'}
              {method === 'comps' && 'TRADING COMPS RESULTS'}
              {method === 'precedent' && 'PRECEDENT TRANSACTIONS'}
            </div>

            {/* DCF Results */}
            {method === 'dcf' && result.equity_value_per_share && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{
                  ...COMMON_STYLES.metricCard,
                  backgroundColor: `${FINCEPT.CYAN}15`,
                  borderLeft: `3px solid ${FINCEPT.CYAN}`,
                  marginBottom: SPACING.DEFAULT,
                }}>
                  <div style={COMMON_STYLES.dataLabel}>EQUITY VALUE PER SHARE</div>
                  <div style={{
                    fontSize: '36px',
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.CYAN,
                    marginTop: SPACING.SMALL,
                  }}>
                    ${result.equity_value_per_share.toFixed(2)}
                  </div>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>ENTERPRISE VALUE</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.enterprise_value || 0)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EQUITY VALUE</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.equity_value || 0)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>WACC</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.ORANGE, marginTop: SPACING.TINY }}>
                      {(result.wacc * 100).toFixed(2)}%
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* LBO Returns Results */}
            {method === 'lbo' && lboSubTab === 'returns' && result.irr && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE }}>
                <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.GREEN}15` }}>
                  <div style={COMMON_STYLES.dataLabel}>IRR</div>
                  <div style={{ fontSize: '28px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                    {result.irr.toFixed(1)}%
                  </div>
                </div>
                <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.CYAN}15` }}>
                  <div style={COMMON_STYLES.dataLabel}>MOIC</div>
                  <div style={{ fontSize: '28px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                    {result.moic.toFixed(2)}x
                  </div>
                </div>
                <div style={COMMON_STYLES.metricCard}>
                  <div style={COMMON_STYLES.dataLabel}>ABSOLUTE GAIN</div>
                  <div style={{ fontSize: '20px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                    {formatCurrency(result.absolute_gain || 0)}
                  </div>
                </div>
              </div>
            )}

            {/* Full LBO Model Results */}
            {method === 'lbo' && lboSubTab === 'full_model' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                {result.data.returns && (
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.DEFAULT }}>
                    <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.GREEN}15` }}>
                      <div style={COMMON_STYLES.dataLabel}>IRR</div>
                      <div style={{ fontSize: '24px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                        {(result.data.returns.irr * 100).toFixed(1)}%
                      </div>
                    </div>
                    <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.CYAN}15` }}>
                      <div style={COMMON_STYLES.dataLabel}>MOIC</div>
                      <div style={{ fontSize: '24px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                        {result.data.returns.moic.toFixed(2)}x
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>ENTRY EQUITY</div>
                      <div style={{ fontSize: '18px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                        {formatCurrency(result.data.returns.entry_equity || 0)}
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>EXIT EQUITY</div>
                      <div style={{ fontSize: '18px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                        {formatCurrency(result.data.returns.exit_equity || 0)}
                      </div>
                    </div>
                  </div>
                )}

                {result.data.sources_uses && (
                  <div style={{ marginBottom: SPACING.DEFAULT }}>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>SOURCES & USES</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: SPACING.TINY }}>SOURCES</div>
                        {Object.entries(result.data.sources_uses.sources || {}).map(([key, val]) => (
                          <div key={key} style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY }}>
                            <span style={{ color: FINCEPT.GRAY }}>{key.replace(/_/g, ' ').toUpperCase()}</span>
                            <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(val as number)}</span>
                          </div>
                        ))}
                      </div>
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: SPACING.TINY }}>USES</div>
                        {Object.entries(result.data.sources_uses.uses || {}).map(([key, val]) => (
                          <div key={key} style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY }}>
                            <span style={{ color: FINCEPT.GRAY }}>{key.replace(/_/g, ' ').toUpperCase()}</span>
                            <span style={{ color: FINCEPT.WHITE }}>{formatCurrency(val as number)}</span>
                          </div>
                        ))}
                      </div>
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* Debt Schedule Results */}
            {method === 'lbo' && lboSubTab === 'debt_schedule' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                {result.data.summary && (
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.DEFAULT }}>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>TOTAL DEBT</div>
                      <div style={{ fontSize: '20px', color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                        {formatCurrency(result.data.summary.total_debt || 0)}
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>TOTAL INTEREST</div>
                      <div style={{ fontSize: '20px', color: FINCEPT.RED, marginTop: SPACING.TINY }}>
                        {formatCurrency(result.data.summary.total_interest || 0)}
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>TOTAL PAYDOWN</div>
                      <div style={{ fontSize: '20px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                        {formatCurrency(result.data.summary.total_paydown || 0)}
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>EXIT DEBT</div>
                      <div style={{ fontSize: '20px', color: FINCEPT.ORANGE, marginTop: SPACING.TINY }}>
                        {formatCurrency(result.data.summary.exit_debt || 0)}
                      </div>
                    </div>
                  </div>
                )}

                {result.data.schedule && Array.isArray(result.data.schedule) && (
                  <div>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>YEAR-BY-YEAR SCHEDULE</div>
                    <div style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      borderRadius: '2px',
                      overflow: 'auto',
                    }}>
                      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                        <thead>
                          <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                            <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Year</th>
                            <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Opening</th>
                            <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Interest</th>
                            <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Paydown</th>
                            <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Closing</th>
                          </tr>
                        </thead>
                        <tbody>
                          {result.data.schedule.map((row: any, idx: number) => (
                            <tr key={idx}>
                              <td style={{ padding: '8px', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{row.year}</td>
                              <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{formatCurrency(row.opening || 0)}</td>
                              <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.RED, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{formatCurrency(row.interest || 0)}</td>
                              <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GREEN, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{formatCurrency(row.paydown || 0)}</td>
                              <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.WHITE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{formatCurrency(row.closing || 0)}</td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* Sensitivity Results */}
            {method === 'lbo' && lboSubTab === 'sensitivity' && result.data && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>IRR SENSITIVITY TABLE</div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  borderRadius: '2px',
                  overflow: 'auto',
                  marginBottom: SPACING.DEFAULT,
                }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                    <thead>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                        <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.ORANGE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          IRR
                        </th>
                        {result.exitMultiples?.map((m: number) => (
                          <th key={m} style={{ padding: '8px', textAlign: 'center', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                            {m.toFixed(1)}x Exit
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {result.revenueGrowths?.map((g: number, gIdx: number) => (
                        <tr key={gIdx}>
                          <td style={{ padding: '8px', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                            {(g * 100).toFixed(0)}% Growth
                          </td>
                          {result.exitMultiples?.map((m: number, mIdx: number) => {
                            const irr = result.data?.irr_matrix?.[gIdx]?.[mIdx] || 0;
                            const color = irr >= 0.25 ? FINCEPT.GREEN : irr >= 0.15 ? FINCEPT.YELLOW : FINCEPT.RED;
                            return (
                              <td key={mIdx} style={{ padding: '8px', textAlign: 'center', color, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                                {(irr * 100).toFixed(1)}%
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>

                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>MOIC SENSITIVITY TABLE</div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  borderRadius: '2px',
                  overflow: 'auto',
                }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                    <thead>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                        <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.CYAN, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          MOIC
                        </th>
                        {result.exitMultiples?.map((m: number) => (
                          <th key={m} style={{ padding: '8px', textAlign: 'center', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                            {m.toFixed(1)}x Exit
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {result.revenueGrowths?.map((g: number, gIdx: number) => (
                        <tr key={gIdx}>
                          <td style={{ padding: '8px', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                            {(g * 100).toFixed(0)}% Growth
                          </td>
                          {result.exitMultiples?.map((m: number, mIdx: number) => {
                            const moic = result.data?.moic_matrix?.[gIdx]?.[mIdx] || 0;
                            const color = moic >= 3 ? FINCEPT.GREEN : moic >= 2 ? FINCEPT.YELLOW : FINCEPT.RED;
                            return (
                              <td key={mIdx} style={{ padding: '8px', textAlign: 'center', color, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                                {moic.toFixed(2)}x
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            )}

            {/* Precedent Transaction Results */}
            {method === 'precedent' && result?.data?.comparables && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                {/* Implied Valuation Summary */}
                {result.data.target_valuation?.valuations && (() => {
                  const v = result.data.target_valuation.valuations;
                  const low = v.ev_ebitda_q1 || v.ev_revenue_q1 || 0;
                  const mid = v.blended_median || v.ev_ebitda_median || v.ev_revenue_median || 0;
                  const high = v.ev_ebitda_q3 || v.ev_revenue_q3 || 0;
                  return (
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.DEFAULT }}>
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={COMMON_STYLES.dataLabel}>LOW IMPLIED EV (Q1)</div>
                        <div style={{ fontSize: '20px', color: FINCEPT.RED, marginTop: SPACING.TINY }}>
                          {formatCurrency(low)}
                        </div>
                      </div>
                      <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.CYAN}15`, borderLeft: `3px solid ${FINCEPT.CYAN}` }}>
                        <div style={COMMON_STYLES.dataLabel}>BLENDED IMPLIED EV</div>
                        <div style={{ fontSize: '24px', color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                          {formatCurrency(mid)}
                        </div>
                      </div>
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={COMMON_STYLES.dataLabel}>HIGH IMPLIED EV (Q3)</div>
                        <div style={{ fontSize: '20px', color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                          {formatCurrency(high)}
                        </div>
                      </div>
                    </div>
                  );
                })()}

                {/* Implied EV by Method */}
                {result.data.target_valuation?.valuations && (
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.DEFAULT }}>
                    {result.data.target_valuation.valuations.ev_revenue_median != null && (
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>EV/Revenue Implied</div>
                        <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                          {formatCurrency(result.data.target_valuation.valuations.ev_revenue_median)}
                        </div>
                      </div>
                    )}
                    {result.data.target_valuation.valuations.ev_ebitda_median != null && (
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>EV/EBITDA Implied</div>
                        <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                          {formatCurrency(result.data.target_valuation.valuations.ev_ebitda_median)}
                        </div>
                      </div>
                    )}
                  </div>
                )}

                {/* Transaction Multiples Summary */}
                {result.data.summary_statistics && (
                  <div style={{ marginBottom: SPACING.DEFAULT }}>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>TRANSACTION MULTIPLES</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                      {[
                        { label: 'EV/Revenue', median: result.data.summary_statistics.median?.['EV/Revenue'], mean: result.data.summary_statistics.mean?.['EV/Revenue'] },
                        { label: 'EV/EBITDA', median: result.data.summary_statistics.median?.['EV/EBITDA'], mean: result.data.summary_statistics.mean?.['EV/EBITDA'] },
                        { label: 'Premium 1D%', median: result.data.summary_statistics.median?.['Premium 1-Day (%)'], mean: result.data.summary_statistics.mean?.['Premium 1-Day (%)'] },
                        { label: 'Deal Val ($M)', median: result.data.summary_statistics.median?.['Deal Value ($M)'], mean: result.data.summary_statistics.mean?.['Deal Value ($M)'] },
                      ].map((m) => (
                        <div key={m.label} style={COMMON_STYLES.metricCard}>
                          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: SPACING.TINY }}>{m.label}</div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY }}>
                            <span style={{ color: FINCEPT.GRAY }}>Median:</span>
                            <span style={{ color: FINCEPT.CYAN }}>{m.label.includes('$M') ? formatCurrency((m.median || 0) * 1_000_000) : `${(m.median || 0).toFixed(2)}${m.label.includes('%') ? '%' : 'x'}`}</span>
                          </div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.TINY }}>
                            <span style={{ color: FINCEPT.GRAY }}>Mean:</span>
                            <span style={{ color: FINCEPT.WHITE }}>{m.label.includes('$M') ? formatCurrency((m.mean || 0) * 1_000_000) : `${(m.mean || 0).toFixed(2)}${m.label.includes('%') ? '%' : 'x'}`}</span>
                          </div>
                        </div>
                      ))}
                    </div>
                  </div>
                )}

                {/* Comparable Transactions Table */}
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>COMPARABLE TRANSACTIONS ({result.data.comp_count})</div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  borderRadius: '2px',
                  overflow: 'auto',
                }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                    <thead>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                        {['Date', 'Acquirer', 'Target', 'Deal ($M)', 'EV/Rev', 'EV/EBITDA', 'Prem. 1D%', 'Payment'].map((h) => (
                          <th key={h} style={{ padding: '8px 6px', textAlign: h === 'Date' || h === 'Acquirer' || h === 'Target' || h === 'Payment' ? 'left' : 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}`, whiteSpace: 'nowrap' }}>
                            {h}
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {result.data.comparables.map((row: any, idx: number) => (
                        <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          <td style={{ padding: '8px 6px', color: FINCEPT.GRAY }}>{row['Date']}</td>
                          <td style={{ padding: '8px 6px', color: FINCEPT.WHITE, maxWidth: '120px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{row['Acquirer']}</td>
                          <td style={{ padding: '8px 6px', color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD, maxWidth: '120px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{row['Target']}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['Deal Value ($M)'] || 0).toLocaleString(undefined, { maximumFractionDigits: 0 })}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EV/Revenue'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EV/EBITDA'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: (row['Premium 1-Day (%)'] || 0) > 0 ? FINCEPT.GREEN : FINCEPT.GRAY }}>{(row['Premium 1-Day (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', color: FINCEPT.GRAY }}>{row['Payment'] || '—'}</td>
                        </tr>
                      ))}
                      {/* Median Row */}
                      {result.data.summary_statistics?.median && (
                        <tr style={{ backgroundColor: `${FINCEPT.CYAN}10`, borderTop: `2px solid ${FINCEPT.CYAN}` }}>
                          <td style={{ padding: '8px 6px', color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>MEDIAN</td>
                          <td style={{ padding: '8px 6px' }}>—</td>
                          <td style={{ padding: '8px 6px' }}>—</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['Deal Value ($M)'] || 0).toLocaleString(undefined, { maximumFractionDigits: 0 })}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EV/Revenue'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EV/EBITDA'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['Premium 1-Day (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px' }}>—</td>
                        </tr>
                      )}
                    </tbody>
                  </table>
                </div>
              </div>
            )}

            {/* Trading Comps Results */}
            {method === 'comps' && result?.data?.comparables && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                {/* Target & Summary Header */}
                {result.data.target_ticker && (
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr 1fr',
                    gap: SPACING.DEFAULT,
                    marginBottom: SPACING.DEFAULT,
                  }}>
                    <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.CYAN}15`, borderLeft: `3px solid ${FINCEPT.CYAN}` }}>
                      <div style={COMMON_STYLES.dataLabel}>TARGET</div>
                      <div style={{ fontSize: '24px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                        {result.data.target_ticker}
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>COMPARABLES</div>
                      <div style={{ fontSize: '24px', fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                        {result.data.comp_count}
                      </div>
                    </div>
                    <div style={COMMON_STYLES.metricCard}>
                      <div style={COMMON_STYLES.dataLabel}>AS OF</div>
                      <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                        {result.data.as_of_date}
                      </div>
                    </div>
                  </div>
                )}

                {/* Implied Valuation from Comps */}
                {result.data.target_valuation?.valuations && (
                  <div style={{ marginBottom: SPACING.DEFAULT }}>
                    <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>IMPLIED ENTERPRISE VALUE</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT }}>
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>EV/Revenue (Median)</div>
                        <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                          {formatCurrency(result.data.target_valuation.valuations.ev_revenue_median || 0)}
                        </div>
                      </div>
                      <div style={COMMON_STYLES.metricCard}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>EV/EBITDA (Median)</div>
                        <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                          {formatCurrency(result.data.target_valuation.valuations.ev_ebitda_median || 0)}
                        </div>
                      </div>
                      <div style={{ ...COMMON_STYLES.metricCard, backgroundColor: `${FINCEPT.GREEN}15`, borderLeft: `3px solid ${FINCEPT.GREEN}` }}>
                        <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Blended Median</div>
                        <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.GREEN, marginTop: SPACING.TINY }}>
                          {formatCurrency(result.data.target_valuation.valuations.blended_median || 0)}
                        </div>
                      </div>
                    </div>
                  </div>
                )}

                {/* Comp Table */}
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>COMPARABLE COMPANIES</div>
                <div style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  borderRadius: '2px',
                  overflow: 'auto',
                }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.TINY }}>
                    <thead>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                        {['Ticker', 'Company', 'Mkt Cap ($M)', 'EV ($M)', 'EV/Rev', 'EV/EBITDA', 'EV/EBIT', 'P/E', 'Rev Gr.%', 'EBITDA Mg.%', 'Net Mg.%', 'ROE%'].map((h) => (
                          <th key={h} style={{ padding: '8px 6px', textAlign: h === 'Ticker' || h === 'Company' ? 'left' : 'right', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}`, whiteSpace: 'nowrap' }}>
                            {h}
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {result.data.comparables.map((row: any, idx: number) => (
                        <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          <td style={{ padding: '8px 6px', color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{row['Ticker']}</td>
                          <td style={{ padding: '8px 6px', color: FINCEPT.WHITE, maxWidth: '140px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{row['Company']}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['Market Cap ($M)'] || 0).toLocaleString(undefined, { maximumFractionDigits: 0 })}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EV ($M)'] || 0).toLocaleString(undefined, { maximumFractionDigits: 0 })}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EV/Revenue'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EV/EBITDA'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EV/EBIT'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['P/E'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: (row['Rev Growth (%)'] || 0) >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>{(row['Rev Growth (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['EBITDA Margin (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['Net Margin (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>{(row['ROE (%)'] || 0).toFixed(1)}%</td>
                        </tr>
                      ))}
                      {/* Median Summary Row */}
                      {result.data.summary_statistics?.median && (
                        <tr style={{ backgroundColor: `${FINCEPT.CYAN}10`, borderTop: `2px solid ${FINCEPT.CYAN}` }}>
                          <td style={{ padding: '8px 6px', color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>MEDIAN</td>
                          <td style={{ padding: '8px 6px', color: FINCEPT.GRAY }}>—</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['Market Cap ($M)'] || 0).toLocaleString(undefined, { maximumFractionDigits: 0 })}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EV ($M)'] || 0).toLocaleString(undefined, { maximumFractionDigits: 0 })}</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EV/Revenue'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EV/EBITDA'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EV/EBIT'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['P/E'] || 0).toFixed(1)}x</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['Rev Growth (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['EBITDA Margin (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['Net Margin (%)'] || 0).toFixed(1)}%</td>
                          <td style={{ padding: '8px 6px', textAlign: 'right', color: FINCEPT.CYAN }}>{(result.data.summary_statistics.median['ROE (%)'] || 0).toFixed(1)}%</td>
                        </tr>
                      )}
                    </tbody>
                  </table>
                </div>
              </div>
            )}

            {/* Fallback: Raw JSON for unhandled result formats */}
            {!((method === 'dcf' && result?.equity_value_per_share) ||
               (method === 'lbo') ||
               (method === 'comps' && result?.data?.comparables) ||
               (method === 'precedent' && result?.data?.comparables)) && (
              <div>
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
            )}
          </div>
        ) : (
          <div style={COMMON_STYLES.emptyState}>
            <Calculator size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
            <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>
              Select a valuation method and configure inputs
            </div>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, marginTop: SPACING.SMALL }}>
              DCF • LBO Analysis • Trading Comps • Precedent Transactions
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
