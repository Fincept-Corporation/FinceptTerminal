/**
 * Valuation Toolkit - Unified DCF, LBO, Trading Comps Calculator
 *
 * All valuation tools in one clean interface
 */

import React, { useState } from 'react';
import { Calculator, TrendingUp, Building2, BarChart3, PlayCircle } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';

type ValuationMethod = 'dcf' | 'lbo' | 'comps';

export const ValuationToolkit: React.FC = () => {
  const [method, setMethod] = useState<ValuationMethod>('dcf');
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

  // LBO Inputs
  const [lboInputs, setLboInputs] = useState({
    entryValuation: 1000,
    exitValuation: 1500,
    equityInvested: 300,
    holdingPeriod: 5,
  });

  // Comps Inputs
  const [compsInputs, setCompsInputs] = useState({
    targetTicker: '',
    compTickers: 'AAPL,MSFT,GOOGL',
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
      alert(`✓ DCF Complete!\nEquity Value/Share: $${res.equity_value_per_share?.toFixed(2)}`);
    } catch (error) {
      alert(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setLoading(false);
    }
  };

  const handleRunLBO = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.LBO.calculateLBOReturns(
        lboInputs.entryValuation * 1000000,
        lboInputs.exitValuation * 1000000,
        lboInputs.equityInvested * 1000000,
        lboInputs.holdingPeriod
      );
      setResult(res);
      alert(`✓ LBO Complete!\nIRR: ${res.irr?.toFixed(1)}%\nMOIC: ${res.moic?.toFixed(2)}x`);
    } catch (error) {
      alert(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setLoading(false);
    }
  };

  const handleRunComps = async () => {
    if (!compsInputs.targetTicker.trim()) {
      alert('Please enter a target ticker');
      return;
    }
    setLoading(true);
    try {
      const res = await MAAnalyticsService.Valuation.calculateTradingComps(
        compsInputs.targetTicker,
        compsInputs.compTickers.split(',').map(t => t.trim())
      );
      setResult(res);
      alert('✓ Trading Comps Complete!');
    } catch (error) {
      alert(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    switch (method) {
      case 'dcf': return handleRunDCF();
      case 'lbo': return handleRunLBO();
      case 'comps': return handleRunComps();
    }
  };

  const formatCurrency = (value: number) => {
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT - Method Selector & Inputs */}
      <div style={{
        width: '380px',
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
              { id: 'lbo', label: 'LBO RETURNS', icon: Building2 },
              { id: 'comps', label: 'TRADING COMPS', icon: BarChart3 },
            ].map((m) => {
              const Icon = m.icon;
              return (
                <button
                  key={m.id}
                  onClick={() => { setMethod(m.id as ValuationMethod); setResult(null); }}
                  style={{
                    ...COMMON_STYLES.tabButton(method === m.id),
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
        </div>

        {/* Input Forms */}
        <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
          {method === 'dcf' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                DCF INPUTS
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    EBIT ($M)
                  </label>
                  <input
                    type="number"
                    value={dcfInputs.ebit}
                    onChange={(e) => setDcfInputs({ ...dcfInputs, ebit: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Tax Rate
                  </label>
                  <input
                    type="number"
                    step="0.01"
                    value={dcfInputs.taxRate}
                    onChange={(e) => setDcfInputs({ ...dcfInputs, taxRate: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Risk-Free Rate
                  </label>
                  <input
                    type="number"
                    step="0.001"
                    value={dcfInputs.riskFreeRate}
                    onChange={(e) => setDcfInputs({ ...dcfInputs, riskFreeRate: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Beta
                  </label>
                  <input
                    type="number"
                    step="0.1"
                    value={dcfInputs.beta}
                    onChange={(e) => setDcfInputs({ ...dcfInputs, beta: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Terminal Growth
                  </label>
                  <input
                    type="number"
                    step="0.001"
                    value={dcfInputs.terminalGrowth}
                    onChange={(e) => setDcfInputs({ ...dcfInputs, terminalGrowth: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Shares Outstanding (M)
                  </label>
                  <input
                    type="number"
                    value={dcfInputs.shares}
                    onChange={(e) => setDcfInputs({ ...dcfInputs, shares: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
              </div>
            </>
          )}

          {method === 'lbo' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                LBO INPUTS ($M)
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Entry Valuation
                  </label>
                  <input
                    type="number"
                    value={lboInputs.entryValuation}
                    onChange={(e) => setLboInputs({ ...lboInputs, entryValuation: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Exit Valuation
                  </label>
                  <input
                    type="number"
                    value={lboInputs.exitValuation}
                    onChange={(e) => setLboInputs({ ...lboInputs, exitValuation: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Equity Invested
                  </label>
                  <input
                    type="number"
                    value={lboInputs.equityInvested}
                    onChange={(e) => setLboInputs({ ...lboInputs, equityInvested: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Holding Period (Years)
                  </label>
                  <input
                    type="number"
                    value={lboInputs.holdingPeriod}
                    onChange={(e) => setLboInputs({ ...lboInputs, holdingPeriod: parseInt(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
              </div>
            </>
          )}

          {method === 'comps' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                TRADING COMPS
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Target Ticker
                  </label>
                  <input
                    type="text"
                    placeholder="AAPL"
                    value={compsInputs.targetTicker}
                    onChange={(e) => setCompsInputs({ ...compsInputs, targetTicker: e.target.value.toUpperCase() })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Comparable Tickers (comma-separated)
                  </label>
                  <input
                    type="text"
                    placeholder="MSFT,GOOGL,META"
                    value={compsInputs.compTickers}
                    onChange={(e) => setCompsInputs({ ...compsInputs, compTickers: e.target.value.toUpperCase() })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
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
              {method === 'lbo' && 'LBO RETURNS'}
              {method === 'comps' && 'TRADING COMPS RESULTS'}
            </div>

            {/* Key Metrics Cards */}
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

            {method === 'lbo' && result.irr && (
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

            {/* Full JSON Results */}
            <div style={COMMON_STYLES.dataLabel}>DETAILED RESULTS</div>
            <pre style={{
              fontSize: TYPOGRAPHY.TINY,
              color: FINCEPT.GRAY,
              backgroundColor: FINCEPT.PANEL_BG,
              padding: SPACING.DEFAULT,
              borderRadius: '4px',
              overflow: 'auto',
              maxHeight: '400px',
            }}>
              {JSON.stringify(result, null, 2)}
            </pre>
          </div>
        ) : (
          <div style={COMMON_STYLES.emptyState}>
            <Calculator size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
            <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>
              Select a valuation method and configure inputs
            </div>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, marginTop: SPACING.SMALL }}>
              DCF, LBO Returns, Trading Comps
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
