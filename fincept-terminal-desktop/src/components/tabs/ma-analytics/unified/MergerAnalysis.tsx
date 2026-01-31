/**
 * Merger Analysis - Unified Accretion/Dilution, Synergies, Deal Structure
 *
 * All merger analysis tools in one interface
 */

import React, { useState } from 'react';
import { GitMerge, Zap, DollarSign, PlayCircle, TrendingUp, TrendingDown } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService } from '@/services/maAnalyticsService';

type AnalysisType = 'accretion' | 'synergies' | 'structure';

export const MergerAnalysis: React.FC = () => {
  const [analysisType, setAnalysisType] = useState<AnalysisType>('accretion');
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

  // Deal Structure Inputs
  const [structureInputs, setStructureInputs] = useState({
    purchasePrice: 1000,
    cashPct: 60,
    acquirerCash: 500,
    debtCapacity: 400,
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
      alert(`✓ Merger Analysis Complete!\n${res.eps_accretion_pct > 0 ? 'ACCRETIVE' : 'DILUTIVE'}: ${res.eps_accretion_pct?.toFixed(1)}%`);
    } catch (error) {
      alert(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
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
      alert(`✓ ${synergiesInputs.synergyType === 'revenue' ? 'Revenue' : 'Cost'} Synergies Calculated!`);
    } catch (error) {
      alert(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setLoading(false);
    }
  };

  const handleRunStructure = async () => {
    setLoading(true);
    try {
      const res = await MAAnalyticsService.DealStructure.analyzePaymentStructure(
        structureInputs.purchasePrice * 1000000,
        structureInputs.cashPct,
        structureInputs.acquirerCash * 1000000,
        structureInputs.debtCapacity * 1000000
      );
      setResult(res);
      alert('✓ Payment Structure Analyzed!');
    } catch (error) {
      alert(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    switch (analysisType) {
      case 'accretion': return handleRunAccretion();
      case 'synergies': return handleRunSynergies();
      case 'structure': return handleRunStructure();
    }
  };

  const formatCurrency = (value: number) => {
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT - Analysis Type & Inputs */}
      <div style={{
        width: '400px',
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
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.TINY }}>
            {[
              { id: 'accretion', label: 'ACCRETION / DILUTION', icon: GitMerge },
              { id: 'synergies', label: 'SYNERGIES', icon: Zap },
              { id: 'structure', label: 'DEAL STRUCTURE', icon: DollarSign },
            ].map((type) => {
              const Icon = type.icon;
              return (
                <button
                  key={type.id}
                  onClick={() => { setAnalysisType(type.id as AnalysisType); setResult(null); }}
                  style={{
                    ...COMMON_STYLES.tabButton(analysisType === type.id),
                    width: '100%',
                    justifyContent: 'flex-start',
                  }}
                >
                  <Icon size={12} style={{ marginRight: SPACING.SMALL }} />
                  {type.label}
                </button>
              );
            })}
          </div>
        </div>

        {/* Input Forms */}
        <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
          {analysisType === 'accretion' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                ACQUIRER ($M)
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Revenue</label>
                  <input
                    type="number"
                    value={accretionInputs.acquirerRevenue}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, acquirerRevenue: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Net Income</label>
                  <input
                    type="number"
                    value={accretionInputs.acquirerNetIncome}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, acquirerNetIncome: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Shares (M)</label>
                  <input
                    type="number"
                    value={accretionInputs.acquirerShares}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, acquirerShares: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>EPS</label>
                  <input
                    type="number"
                    value={accretionInputs.acquirerEPS}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, acquirerEPS: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                TARGET ($M)
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Revenue</label>
                  <input
                    type="number"
                    value={accretionInputs.targetRevenue}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, targetRevenue: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Net Income</label>
                  <input
                    type="number"
                    value={accretionInputs.targetNetIncome}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, targetNetIncome: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Purchase Price</label>
                  <input
                    type="number"
                    value={accretionInputs.purchasePrice}
                    onChange={(e) => setAccretionInputs({ ...accretionInputs, purchasePrice: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>Cash %</label>
                  <input
                    type="number"
                    value={accretionInputs.cashPct}
                    onChange={(e) => {
                      const cash = parseFloat(e.target.value);
                      setAccretionInputs({ ...accretionInputs, cashPct: cash, stockPct: 100 - cash });
                    }}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
              </div>
            </>
          )}

          {analysisType === 'synergies' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                SYNERGY TYPE
              </div>
              <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.DEFAULT }}>
                <button
                  onClick={() => setSynergiesInputs({ ...synergiesInputs, synergyType: 'revenue' })}
                  style={{
                    ...COMMON_STYLES.tabButton(synergiesInputs.synergyType === 'revenue'),
                    flex: 1,
                  }}
                >
                  REVENUE
                </button>
                <button
                  onClick={() => setSynergiesInputs({ ...synergiesInputs, synergyType: 'cost' })}
                  style={{
                    ...COMMON_STYLES.tabButton(synergiesInputs.synergyType === 'cost'),
                    flex: 1,
                  }}
                >
                  COST
                </button>
              </div>

              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                SYNERGY INPUTS
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Base Revenue ($M)
                  </label>
                  <input
                    type="number"
                    value={synergiesInputs.baseRevenue}
                    onChange={(e) => setSynergiesInputs({ ...synergiesInputs, baseRevenue: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Cross-Sell Rate
                  </label>
                  <input
                    type="number"
                    step="0.01"
                    value={synergiesInputs.crossSellRate}
                    onChange={(e) => setSynergiesInputs({ ...synergiesInputs, crossSellRate: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Projection Years
                  </label>
                  <input
                    type="number"
                    value={synergiesInputs.years}
                    onChange={(e) => setSynergiesInputs({ ...synergiesInputs, years: parseInt(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
              </div>
            </>
          )}

          {analysisType === 'structure' && (
            <>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                PAYMENT STRUCTURE ($M)
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Purchase Price
                  </label>
                  <input
                    type="number"
                    value={structureInputs.purchasePrice}
                    onChange={(e) => setStructureInputs({ ...structureInputs, purchasePrice: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Cash %
                  </label>
                  <input
                    type="number"
                    value={structureInputs.cashPct}
                    onChange={(e) => setStructureInputs({ ...structureInputs, cashPct: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Acquirer Cash Available
                  </label>
                  <input
                    type="number"
                    value={structureInputs.acquirerCash}
                    onChange={(e) => setStructureInputs({ ...structureInputs, acquirerCash: parseFloat(e.target.value) })}
                    style={COMMON_STYLES.inputField}
                  />
                </div>
                <div>
                  <label style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
                    Debt Capacity
                  </label>
                  <input
                    type="number"
                    value={structureInputs.debtCapacity}
                    onChange={(e) => setStructureInputs({ ...structureInputs, debtCapacity: parseFloat(e.target.value) })}
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
              {analysisType === 'structure' && 'PAYMENT STRUCTURE ANALYSIS'}
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
                      <div style={{
                        fontSize: '32px',
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: FINCEPT.WHITE,
                        marginTop: SPACING.TINY,
                      }}>
                        ${result.pro_forma_eps.toFixed(2)}
                      </div>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div style={{
                        fontSize: TYPOGRAPHY.SMALL,
                        color: result.eps_accretion_pct > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                        display: 'flex',
                        alignItems: 'center',
                        gap: SPACING.TINY,
                      }}>
                        {result.eps_accretion_pct > 0 ? <TrendingUp size={16} /> : <TrendingDown size={16} />}
                        {result.eps_accretion_pct > 0 ? 'ACCRETIVE' : 'DILUTIVE'}
                      </div>
                      <div style={{
                        fontSize: '24px',
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: result.eps_accretion_pct > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                      }}>
                        {result.eps_accretion_pct > 0 ? '+' : ''}{result.eps_accretion_pct.toFixed(1)}%
                      </div>
                    </div>
                  </div>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PRO FORMA REVENUE</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                      {formatCurrency(result.pro_forma_revenue || 0)}
                    </div>
                  </div>
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>PRO FORMA SHARES</div>
                    <div style={{ fontSize: TYPOGRAPHY.HEADING, color: FINCEPT.CYAN, marginTop: SPACING.TINY }}>
                      {((result.pro_forma_shares || 0) / 1000000).toFixed(1)}M
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
              borderRadius: '4px',
              overflow: 'auto',
              maxHeight: '500px',
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
              Accretion/Dilution, Synergies, Deal Structure
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
