// File: src/components/tabs/DerivativesTab.tsx
// Professional Derivatives Pricing & Valuation - Powered by FinancePy

import React, { useState , useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  Calculator, TrendingUp, DollarSign, Activity, BarChart3,
  Percent, Calendar, Settings, Info, ChevronDown, ChevronUp,
  Copy, Download, RefreshCw, Zap, Target, AlertCircle
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',      // Primary accent, CTAs, selected states
  WHITE: '#FFFFFF',       // Primary text
  RED: '#FF3B3B',         // Negative, errors, sell
  GREEN: '#00D66F',       // Positive, success, buy
  GRAY: '#787878',        // Secondary text, labels
  DARK_BG: '#000000',     // Main background
  PANEL_BG: '#0F0F0F',    // Card/panel background
  HEADER_BG: '#1A1A1A',   // Header/toolbar background
  BORDER: '#2A2A2A',      // Borders, dividers
  HOVER: '#1F1F1F',       // Hover state background
  MUTED: '#4A4A4A',       // Disabled, inactive
  CYAN: '#00E5FF',        // Info, data values
  YELLOW: '#FFD700',      // Alerts, prices
  BLUE: '#0088FF',        // Secondary accent
  PURPLE: '#9D4EDD',      // Tertiary accent
};

type InstrumentType = 'bonds' | 'equity-options' | 'fx-options' | 'swaps' | 'credit' | 'vollib';

interface BondResult {
  clean_price: number;
  dirty_price: number;
  accrued_interest: number;
  duration: number;
  convexity: number;
  ytm: number;
}

interface OptionResult {
  price: number;
  greeks: {
    delta: number;
    gamma: number;
    vega: number;
    theta: number;
    rho: number;
  };
}

export function DerivativesTab() {
  const { t } = useTranslation();
  const [activeInstrument, setActiveInstrument] = useState<InstrumentType>('bonds');

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeInstrument,
  }), [activeInstrument]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeInstrument === "string") setActiveInstrument(state.activeInstrument as any);
  }, []);

  useWorkspaceTabState("derivatives", getWorkspaceState, setWorkspaceState);
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  // Bond pricing state
  const [bondParams, setBondParams] = useState({
    issueDate: '2023-01-01',
    settlementDate: new Date().toISOString().split('T')[0],
    maturityDate: '2029-01-01',
    couponRate: 5.0,
    ytm: 4.5,
    freq: 2,
    cleanPrice: 102.0
  });

  // Equity option state
  const [optionParams, setOptionParams] = useState({
    valuationDate: new Date().toISOString().split('T')[0],
    expiryDate: new Date(Date.now() + 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    strike: 100,
    spot: 105,
    volatility: 25,
    riskFreeRate: 5,
    dividendYield: 2,
    optionType: 'call',
    optionPrice: 15.0
  });

  // FX option state
  const [fxParams, setFxParams] = useState({
    valuationDate: new Date().toISOString().split('T')[0],
    expiryDate: new Date(Date.now() + 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    strike: 1.10,
    spot: 1.08,
    volatility: 12,
    domesticRate: 2,
    foreignRate: 1.5,
    optionType: 'call',
    notional: 1000000
  });

  // Swap state
  const [swapParams, setSwapParams] = useState({
    effectiveDate: new Date().toISOString().split('T')[0],
    maturityDate: new Date(Date.now() + 5 * 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    fixedRate: 3.0,
    freq: 2,
    notional: 1000000,
    discountRate: 3.5
  });

  // py_vollib state
  const [vollibModel, setVollibModel] = useState<'black' | 'bs' | 'bsm'>('bs');
  const [vollibParams, setVollibParams] = useState({
    S: 100,
    K: 100,
    t: 0.25,
    r: 0.05,
    sigma: 0.20,
    q: 0.02,
    flag: 'c',
    optionPrice: 5.0,
  });

  // CDS state
  const [cdsParams, setCdsParams] = useState({
    valuationDate: new Date().toISOString().split('T')[0],
    maturityDate: new Date(Date.now() + 5 * 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    recoveryRate: 40,
    notional: 10000000,
    spreadBps: 150
  });

  const calculateBondPrice = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_bond_price', {
        issueDate: bondParams.issueDate,
        settlementDate: bondParams.settlementDate,
        maturityDate: bondParams.maturityDate,
        couponRate: bondParams.couponRate,
        ytm: bondParams.ytm,
        freq: bondParams.freq
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const calculateBondYTM = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_bond_ytm', {
        issueDate: bondParams.issueDate,
        settlementDate: bondParams.settlementDate,
        maturityDate: bondParams.maturityDate,
        couponRate: bondParams.couponRate,
        cleanPrice: bondParams.cleanPrice,
        freq: bondParams.freq
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const calculateOptionPrice = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_equity_option_price', {
        valuationDate: optionParams.valuationDate,
        expiryDate: optionParams.expiryDate,
        strike: optionParams.strike,
        spot: optionParams.spot,
        volatility: optionParams.volatility,
        riskFreeRate: optionParams.riskFreeRate,
        dividendYield: optionParams.dividendYield,
        optionType: optionParams.optionType
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const calculateImpliedVol = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_equity_option_implied_vol', {
        valuationDate: optionParams.valuationDate,
        expiryDate: optionParams.expiryDate,
        strike: optionParams.strike,
        spot: optionParams.spot,
        optionPrice: optionParams.optionPrice,
        riskFreeRate: optionParams.riskFreeRate,
        dividendYield: optionParams.dividendYield,
        optionType: optionParams.optionType
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const calculateFXOption = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_fx_option_price', {
        valuationDate: fxParams.valuationDate,
        expiryDate: fxParams.expiryDate,
        strike: fxParams.strike,
        spot: fxParams.spot,
        volatility: fxParams.volatility,
        domesticRate: fxParams.domesticRate,
        foreignRate: fxParams.foreignRate,
        optionType: fxParams.optionType,
        notional: fxParams.notional
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const calculateSwap = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_ibor_swap_price', {
        effectiveDate: swapParams.effectiveDate,
        maturityDate: swapParams.maturityDate,
        fixedRate: swapParams.fixedRate,
        freq: swapParams.freq,
        notional: swapParams.notional,
        discountRate: swapParams.discountRate
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  const calculateCDS = async () => {
    setLoading(true);
    setError(null);
    try {
      const resultStr = await invoke<string>('financepy_cds_spread', {
        valuationDate: cdsParams.valuationDate,
        maturityDate: cdsParams.maturityDate,
        recoveryRate: cdsParams.recoveryRate,
        notional: cdsParams.notional,
        spreadBps: cdsParams.spreadBps
      });
      setResult(JSON.parse(resultStr));
    } catch (e: any) {
      setError(e.toString());
    } finally {
      setLoading(false);
    }
  };

  // py_vollib handlers
  const calculateVollibPrice = async () => {
    setLoading(true); setError(null);
    try {
      const p = vollibParams;
      let resultStr: string;
      if (vollibModel === 'black') {
        resultStr = await invoke<string>('vollib_black_price', { s: p.S, k: p.K, t: p.t, r: p.r, sigma: p.sigma, flag: p.flag });
      } else if (vollibModel === 'bs') {
        resultStr = await invoke<string>('vollib_bs_price', { s: p.S, k: p.K, t: p.t, r: p.r, sigma: p.sigma, flag: p.flag });
      } else {
        resultStr = await invoke<string>('vollib_bsm_price', { s: p.S, k: p.K, t: p.t, r: p.r, sigma: p.sigma, q: p.q, flag: p.flag });
      }
      setResult(JSON.parse(resultStr));
    } catch (e: any) { setError(e.toString()); } finally { setLoading(false); }
  };

  const calculateVollibGreeks = async () => {
    setLoading(true); setError(null);
    try {
      const p = vollibParams;
      let resultStr: string;
      if (vollibModel === 'black') {
        resultStr = await invoke<string>('vollib_black_greeks', { s: p.S, k: p.K, t: p.t, r: p.r, sigma: p.sigma, flag: p.flag });
      } else if (vollibModel === 'bs') {
        resultStr = await invoke<string>('vollib_bs_greeks', { s: p.S, k: p.K, t: p.t, r: p.r, sigma: p.sigma, flag: p.flag });
      } else {
        resultStr = await invoke<string>('vollib_bsm_greeks', { s: p.S, k: p.K, t: p.t, r: p.r, sigma: p.sigma, q: p.q, flag: p.flag });
      }
      const greeks = JSON.parse(resultStr);
      setResult({ price: null, greeks });
    } catch (e: any) { setError(e.toString()); } finally { setLoading(false); }
  };

  const calculateVollibIV = async () => {
    setLoading(true); setError(null);
    try {
      const p = vollibParams;
      let resultStr: string;
      if (vollibModel === 'black') {
        resultStr = await invoke<string>('vollib_black_iv', { price: p.optionPrice, s: p.S, k: p.K, t: p.t, r: p.r, flag: p.flag });
      } else if (vollibModel === 'bs') {
        resultStr = await invoke<string>('vollib_bs_iv', { price: p.optionPrice, s: p.S, k: p.K, t: p.t, r: p.r, flag: p.flag });
      } else {
        resultStr = await invoke<string>('vollib_bsm_iv', { price: p.optionPrice, s: p.S, k: p.K, t: p.t, r: p.r, q: p.q, flag: p.flag });
      }
      setResult(JSON.parse(resultStr));
    } catch (e: any) { setError(e.toString()); } finally { setLoading(false); }
  };

  const copyResults = () => {
    if (result) {
      navigator.clipboard.writeText(JSON.stringify(result, null, 2));
    }
  };

  const renderInstrumentSelector = () => {
    const instruments = [
      { id: 'bonds', label: 'BONDS', icon: DollarSign },
      { id: 'equity-options', label: 'EQUITY OPTIONS', icon: TrendingUp },
      { id: 'fx-options', label: 'FX OPTIONS', icon: Activity },
      { id: 'swaps', label: 'IR SWAPS', icon: BarChart3 },
      { id: 'credit', label: 'CREDIT', icon: Target },
      { id: 'vollib', label: 'PY_VOLLIB', icon: Zap }
    ];

    return (
      <div style={{ display: 'flex', gap: '4px', marginBottom: '16px', overflowX: 'auto', paddingBottom: '8px' }}>
        {instruments.map(({ id, label, icon: Icon }) => (
          <button
            key={id}
            onClick={() => {
              setActiveInstrument(id as InstrumentType);
              setResult(null);
              setError(null);
            }}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '6px 12px',
              backgroundColor: activeInstrument === id ? FINCEPT.ORANGE : 'transparent',
              color: activeInstrument === id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              whiteSpace: 'nowrap',
              cursor: 'pointer',
              transition: 'all 0.2s',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            }}
          >
            <Icon size={12} />
            <span>{label}</span>
          </button>
        ))}
      </div>
    );
  };

  // Shared styles
  const panelStyle: React.CSSProperties = {
    backgroundColor: FINCEPT.PANEL_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
  };

  const sectionHeaderStyle: React.CSSProperties = {
    padding: '12px',
    backgroundColor: FINCEPT.HEADER_BG,
    borderBottom: `1px solid ${FINCEPT.BORDER}`,
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
  };

  const labelStyle: React.CSSProperties = {
    fontSize: '9px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    letterSpacing: '0.5px',
    marginBottom: '4px',
    display: 'block',
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '10px',
    fontFamily: '"IBM Plex Mono", monospace',
  };

  const primaryButtonStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 16px',
    backgroundColor: FINCEPT.ORANGE,
    color: FINCEPT.DARK_BG,
    border: 'none',
    borderRadius: '2px',
    fontSize: '9px',
    fontWeight: 700,
    cursor: 'pointer',
    transition: 'all 0.2s',
    fontFamily: '"IBM Plex Mono", monospace',
  };

  const renderBondsPanel = () => (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px' }}>
      {/* Bond Pricing Calculator */}
      <div style={panelStyle}>
        <div style={sectionHeaderStyle}>
          <Calculator size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>BOND PRICE CALCULATOR</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>ISSUE DATE</label>
              <input
                type="date"
                value={bondParams.issueDate}
                onChange={(e) => setBondParams({ ...bondParams, issueDate: e.target.value })}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>SETTLEMENT DATE</label>
              <input
                type="date"
                value={bondParams.settlementDate}
                onChange={(e) => setBondParams({ ...bondParams, settlementDate: e.target.value })}
                style={inputStyle}
              />
            </div>
          </div>

          <div>
            <label style={labelStyle}>MATURITY DATE</label>
            <input
              type="date"
              value={bondParams.maturityDate}
              onChange={(e) => setBondParams({ ...bondParams, maturityDate: e.target.value })}
              style={inputStyle}
            />
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>COUPON RATE (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={bondParams.couponRate}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setBondParams({ ...bondParams, couponRate: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>YTM (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={bondParams.ytm}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setBondParams({ ...bondParams, ytm: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
          </div>

          <div>
            <label style={labelStyle}>PAYMENT FREQUENCY</label>
            <select
              value={bondParams.freq}
              onChange={(e) => setBondParams({ ...bondParams, freq: parseInt(e.target.value) })}
              style={inputStyle}
            >
              <option value={1}>Annual</option>
              <option value={2}>Semi-Annual</option>
              <option value={4}>Quarterly</option>
            </select>
          </div>

          <button
            onClick={calculateBondPrice}
            disabled={loading}
            style={{
              ...primaryButtonStyle,
              opacity: loading ? 0.5 : 1,
            }}
          >
            {loading ? 'CALCULATING...' : 'CALCULATE BOND PRICE'}
          </button>
        </div>
      </div>

      {/* YTM Calculator */}
      <div style={panelStyle}>
        <div style={sectionHeaderStyle}>
          <Percent size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>YIELD TO MATURITY</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>ISSUE DATE</label>
              <input
                type="date"
                value={bondParams.issueDate}
                onChange={(e) => setBondParams({ ...bondParams, issueDate: e.target.value })}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>SETTLEMENT DATE</label>
              <input
                type="date"
                value={bondParams.settlementDate}
                onChange={(e) => setBondParams({ ...bondParams, settlementDate: e.target.value })}
                style={inputStyle}
              />
            </div>
          </div>

          <div>
            <label style={labelStyle}>MATURITY DATE</label>
            <input
              type="date"
              value={bondParams.maturityDate}
              onChange={(e) => setBondParams({ ...bondParams, maturityDate: e.target.value })}
              style={inputStyle}
            />
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>COUPON RATE (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={bondParams.couponRate}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setBondParams({ ...bondParams, couponRate: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>CLEAN PRICE</label>
              <input
                type="text"
                inputMode="decimal"
                value={bondParams.cleanPrice}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setBondParams({ ...bondParams, cleanPrice: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
          </div>

          <div>
            <label style={labelStyle}>PAYMENT FREQUENCY</label>
            <select
              value={bondParams.freq}
              onChange={(e) => setBondParams({ ...bondParams, freq: parseInt(e.target.value) })}
              style={inputStyle}
            >
              <option value={1}>Annual</option>
              <option value={2}>Semi-Annual</option>
              <option value={4}>Quarterly</option>
            </select>
          </div>

          <button
            onClick={calculateBondYTM}
            disabled={loading}
            style={{
              ...primaryButtonStyle,
              opacity: loading ? 0.5 : 1,
            }}
          >
            {loading ? 'CALCULATING...' : 'CALCULATE YTM'}
          </button>
        </div>
      </div>
    </div>
  );

  const renderEquityOptionsPanel = () => (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px' }}>
      {/* Option Pricing */}
      <div style={panelStyle}>
        <div style={sectionHeaderStyle}>
          <TrendingUp size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>BLACK-SCHOLES PRICING</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>VALUATION DATE</label>
              <input
                type="date"
                value={optionParams.valuationDate}
                onChange={(e) => setOptionParams({ ...optionParams, valuationDate: e.target.value })}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>EXPIRY DATE</label>
              <input
                type="date"
                value={optionParams.expiryDate}
                onChange={(e) => setOptionParams({ ...optionParams, expiryDate: e.target.value })}
                style={inputStyle}
              />
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>STRIKE PRICE</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.strike}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, strike: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>SPOT PRICE</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.spot}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, spot: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>VOLATILITY (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.volatility}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, volatility: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>RISK-FREE RATE (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.riskFreeRate}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, riskFreeRate: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>DIVIDEND YIELD (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.dividendYield}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, dividendYield: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>OPTION TYPE</label>
              <select
                value={optionParams.optionType}
                onChange={(e) => setOptionParams({ ...optionParams, optionType: e.target.value })}
                style={inputStyle}
              >
                <option value="call">Call</option>
                <option value="put">Put</option>
              </select>
            </div>
          </div>

          <button
            onClick={calculateOptionPrice}
            disabled={loading}
            style={{
              ...primaryButtonStyle,
              opacity: loading ? 0.5 : 1,
            }}
          >
            {loading ? 'CALCULATING...' : 'CALCULATE PRICE & GREEKS'}
          </button>
        </div>
      </div>

      {/* Implied Volatility */}
      <div style={panelStyle}>
        <div style={sectionHeaderStyle}>
          <Activity size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>IMPLIED VOLATILITY</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>VALUATION DATE</label>
              <input
                type="date"
                value={optionParams.valuationDate}
                onChange={(e) => setOptionParams({ ...optionParams, valuationDate: e.target.value })}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>EXPIRY DATE</label>
              <input
                type="date"
                value={optionParams.expiryDate}
                onChange={(e) => setOptionParams({ ...optionParams, expiryDate: e.target.value })}
                style={inputStyle}
              />
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>STRIKE PRICE</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.strike}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, strike: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>SPOT PRICE</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.spot}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, spot: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
          </div>

          <div>
            <label style={labelStyle}>MARKET OPTION PRICE</label>
            <input
              type="text"
              inputMode="decimal"
              value={optionParams.optionPrice}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, optionPrice: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>RISK-FREE RATE (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.riskFreeRate}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, riskFreeRate: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
            <div>
              <label style={labelStyle}>DIVIDEND YIELD (%)</label>
              <input
                type="text"
                inputMode="decimal"
                value={optionParams.dividendYield}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptionParams({ ...optionParams, dividendYield: parseFloat(v) || 0 }); }}
                style={inputStyle}
              />
            </div>
          </div>

          <div>
            <label style={labelStyle}>OPTION TYPE</label>
            <select
              value={optionParams.optionType}
              onChange={(e) => setOptionParams({ ...optionParams, optionType: e.target.value })}
              style={inputStyle}
            >
              <option value="call">Call</option>
              <option value="put">Put</option>
            </select>
          </div>

          <button
            onClick={calculateImpliedVol}
            disabled={loading}
            style={{
              ...primaryButtonStyle,
              opacity: loading ? 0.5 : 1,
            }}
          >
            {loading ? 'CALCULATING...' : 'CALCULATE IMPLIED VOL'}
          </button>
        </div>
      </div>
    </div>
  );

  const renderFXOptionsPanel = () => (
    <div style={{ ...panelStyle, maxWidth: '700px' }}>
      <div style={sectionHeaderStyle}>
        <Activity size={14} style={{ color: FINCEPT.ORANGE }} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>FX VANILLA OPTION PRICING</span>
      </div>

      <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>VALUATION DATE</label>
            <input
              type="date"
              value={fxParams.valuationDate}
              onChange={(e) => setFxParams({ ...fxParams, valuationDate: e.target.value })}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>EXPIRY DATE</label>
            <input
              type="date"
              value={fxParams.expiryDate}
              onChange={(e) => setFxParams({ ...fxParams, expiryDate: e.target.value })}
              style={inputStyle}
            />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>STRIKE FX RATE</label>
            <input
              type="text"
              inputMode="decimal"
              value={fxParams.strike}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFxParams({ ...fxParams, strike: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>SPOT FX RATE</label>
            <input
              type="text"
              inputMode="decimal"
              value={fxParams.spot}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFxParams({ ...fxParams, spot: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>VOLATILITY (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={fxParams.volatility}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFxParams({ ...fxParams, volatility: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>DOMESTIC RATE (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={fxParams.domesticRate}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFxParams({ ...fxParams, domesticRate: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>FOREIGN RATE (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={fxParams.foreignRate}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFxParams({ ...fxParams, foreignRate: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>NOTIONAL</label>
            <input
              type="text"
              inputMode="decimal"
              value={fxParams.notional}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setFxParams({ ...fxParams, notional: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>OPTION TYPE</label>
            <select
              value={fxParams.optionType}
              onChange={(e) => setFxParams({ ...fxParams, optionType: e.target.value })}
              style={inputStyle}
            >
              <option value="call">Call</option>
              <option value="put">Put</option>
            </select>
          </div>
        </div>

        <button
          onClick={calculateFXOption}
          disabled={loading}
          style={{
            ...primaryButtonStyle,
            opacity: loading ? 0.5 : 1,
          }}
        >
          {loading ? 'CALCULATING...' : 'CALCULATE FX OPTION PRICE'}
        </button>
      </div>
    </div>
  );

  const renderSwapsPanel = () => (
    <div style={{ ...panelStyle, maxWidth: '700px' }}>
      <div style={sectionHeaderStyle}>
        <BarChart3 size={14} style={{ color: FINCEPT.ORANGE }} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>INTEREST RATE SWAP PRICING</span>
      </div>

      <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>EFFECTIVE DATE</label>
            <input
              type="date"
              value={swapParams.effectiveDate}
              onChange={(e) => setSwapParams({ ...swapParams, effectiveDate: e.target.value })}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>MATURITY DATE</label>
            <input
              type="date"
              value={swapParams.maturityDate}
              onChange={(e) => setSwapParams({ ...swapParams, maturityDate: e.target.value })}
              style={inputStyle}
            />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>FIXED RATE (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={swapParams.fixedRate}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setSwapParams({ ...swapParams, fixedRate: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>FREQUENCY</label>
            <select
              value={swapParams.freq}
              onChange={(e) => setSwapParams({ ...swapParams, freq: parseInt(e.target.value) })}
              style={inputStyle}
            >
              <option value={1}>Annual</option>
              <option value={2}>Semi-Annual</option>
              <option value={4}>Quarterly</option>
            </select>
          </div>
          <div>
            <label style={labelStyle}>DISCOUNT RATE (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={swapParams.discountRate}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setSwapParams({ ...swapParams, discountRate: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
        </div>

        <div>
          <label style={labelStyle}>NOTIONAL AMOUNT</label>
          <input
            type="text"
            inputMode="decimal"
            value={swapParams.notional}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setSwapParams({ ...swapParams, notional: parseFloat(v) || 0 }); }}
            style={inputStyle}
          />
        </div>

        <button
          onClick={calculateSwap}
          disabled={loading}
          style={{
            ...primaryButtonStyle,
            opacity: loading ? 0.5 : 1,
          }}
        >
          {loading ? 'CALCULATING...' : 'CALCULATE SWAP VALUE'}
        </button>
      </div>
    </div>
  );

  const renderCreditPanel = () => (
    <div style={{ ...panelStyle, maxWidth: '700px' }}>
      <div style={sectionHeaderStyle}>
        <Target size={14} style={{ color: FINCEPT.ORANGE }} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>CREDIT DEFAULT SWAP PRICING</span>
      </div>

      <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>VALUATION DATE</label>
            <input
              type="date"
              value={cdsParams.valuationDate}
              onChange={(e) => setCdsParams({ ...cdsParams, valuationDate: e.target.value })}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>MATURITY DATE</label>
            <input
              type="date"
              value={cdsParams.maturityDate}
              onChange={(e) => setCdsParams({ ...cdsParams, maturityDate: e.target.value })}
              style={inputStyle}
            />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          <div>
            <label style={labelStyle}>RECOVERY RATE (%)</label>
            <input
              type="text"
              inputMode="decimal"
              value={cdsParams.recoveryRate}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setCdsParams({ ...cdsParams, recoveryRate: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>SPREAD (BPS)</label>
            <input
              type="text"
              inputMode="decimal"
              value={cdsParams.spreadBps}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setCdsParams({ ...cdsParams, spreadBps: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
          <div>
            <label style={labelStyle}>NOTIONAL</label>
            <input
              type="text"
              inputMode="decimal"
              value={cdsParams.notional}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setCdsParams({ ...cdsParams, notional: parseFloat(v) || 0 }); }}
              style={inputStyle}
            />
          </div>
        </div>

        <button
          onClick={calculateCDS}
          disabled={loading}
          style={{
            ...primaryButtonStyle,
            opacity: loading ? 0.5 : 1,
          }}
        >
          {loading ? 'CALCULATING...' : 'CALCULATE CDS VALUE'}
        </button>
      </div>
    </div>
  );

  const renderVolLibPanel = () => (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px' }}>
      {/* Pricing & Greeks */}
      <div style={panelStyle}>
        <div style={sectionHeaderStyle}>
          <Zap size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>PY_VOLLIB PRICING & GREEKS</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Model selector */}
          <div>
            <label style={labelStyle}>PRICING MODEL</label>
            <select
              value={vollibModel}
              onChange={(e) => setVollibModel(e.target.value as any)}
              style={inputStyle}
            >
              <option value="black">Black (Futures/Forwards)</option>
              <option value="bs">Black-Scholes (Equity, no dividends)</option>
              <option value="bsm">Black-Scholes-Merton (Equity + dividends)</option>
            </select>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>{vollibModel === 'black' ? 'FORWARD PRICE' : 'SPOT PRICE'} (S)</label>
              <input type="text" inputMode="decimal" value={vollibParams.S}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, S: parseFloat(v) || 0 }); }}
                style={inputStyle} />
            </div>
            <div>
              <label style={labelStyle}>STRIKE (K)</label>
              <input type="text" inputMode="decimal" value={vollibParams.K}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, K: parseFloat(v) || 0 }); }}
                style={inputStyle} />
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>TIME TO EXPIRY (years)</label>
              <input type="text" inputMode="decimal" value={vollibParams.t}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, t: parseFloat(v) || 0 }); }}
                style={inputStyle} />
            </div>
            <div>
              <label style={labelStyle}>RISK-FREE RATE (decimal)</label>
              <input type="text" inputMode="decimal" value={vollibParams.r}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, r: parseFloat(v) || 0 }); }}
                style={inputStyle} />
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: vollibModel === 'bsm' ? '1fr 1fr 1fr' : '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>VOLATILITY (decimal)</label>
              <input type="text" inputMode="decimal" value={vollibParams.sigma}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, sigma: parseFloat(v) || 0 }); }}
                style={inputStyle} />
            </div>
            <div>
              <label style={labelStyle}>OPTION TYPE</label>
              <select value={vollibParams.flag}
                onChange={(e) => setVollibParams({ ...vollibParams, flag: e.target.value })}
                style={inputStyle}>
                <option value="c">Call</option>
                <option value="p">Put</option>
              </select>
            </div>
            {vollibModel === 'bsm' && (
              <div>
                <label style={labelStyle}>DIVIDEND YIELD (q)</label>
                <input type="text" inputMode="decimal" value={vollibParams.q}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, q: parseFloat(v) || 0 }); }}
                  style={inputStyle} />
              </div>
            )}
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <button onClick={calculateVollibPrice} disabled={loading}
              style={{ ...primaryButtonStyle, opacity: loading ? 0.5 : 1 }}>
              {loading ? 'CALCULATING...' : 'CALCULATE PRICE'}
            </button>
            <button onClick={calculateVollibGreeks} disabled={loading}
              style={{ ...primaryButtonStyle, opacity: loading ? 0.5 : 1, backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}>
              {loading ? 'CALCULATING...' : 'CALCULATE GREEKS'}
            </button>
          </div>
        </div>
      </div>

      {/* Implied Volatility */}
      <div style={panelStyle}>
        <div style={sectionHeaderStyle}>
          <Activity size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>IMPLIED VOLATILITY (PY_VOLLIB)</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{ padding: '8px', backgroundColor: `${FINCEPT.CYAN}10`, border: `1px solid ${FINCEPT.CYAN}30`, borderRadius: '2px' }}>
            <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
              Model: {vollibModel === 'black' ? 'Black' : vollibModel === 'bs' ? 'Black-Scholes' : 'Black-Scholes-Merton'}
              {' '}| Uses same S, K, t, r{vollibModel === 'bsm' ? ', q' : ''} from left panel
            </span>
          </div>

          <div>
            <label style={labelStyle}>MARKET OPTION PRICE</label>
            <input type="text" inputMode="decimal" value={vollibParams.optionPrice}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^-?\d*\.?\d*$/.test(v)) setVollibParams({ ...vollibParams, optionPrice: parseFloat(v) || 0 }); }}
              style={inputStyle} />
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>{vollibModel === 'black' ? 'FORWARD' : 'SPOT'} (S)</label>
              <div style={{ ...inputStyle, backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.CYAN }}>{vollibParams.S}</div>
            </div>
            <div>
              <label style={labelStyle}>STRIKE (K)</label>
              <div style={{ ...inputStyle, backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.CYAN }}>{vollibParams.K}</div>
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div>
              <label style={labelStyle}>TIME (t)</label>
              <div style={{ ...inputStyle, backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.CYAN }}>{vollibParams.t}</div>
            </div>
            <div>
              <label style={labelStyle}>RATE (r)</label>
              <div style={{ ...inputStyle, backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.CYAN }}>{vollibParams.r}</div>
            </div>
          </div>

          <button onClick={calculateVollibIV} disabled={loading}
            style={{ ...primaryButtonStyle, opacity: loading ? 0.5 : 1 }}>
            {loading ? 'CALCULATING...' : 'CALCULATE IMPLIED VOL'}
          </button>
        </div>
      </div>
    </div>
  );

  const renderResults = () => {
    if (error) {
      return (
        <div style={{
          marginTop: '16px',
          backgroundColor: `${FINCEPT.RED}20`,
          border: `1px solid ${FINCEPT.RED}`,
          borderRadius: '2px',
          padding: '12px',
        }}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <AlertCircle size={14} style={{ color: FINCEPT.RED, marginTop: '2px' }} />
            <div>
              <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '4px' }}>ERROR</div>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
            </div>
          </div>
        </div>
      );
    }

    if (!result) return null;

    const resultCardStyle: React.CSSProperties = {
      backgroundColor: FINCEPT.HEADER_BG,
      borderRadius: '2px',
      padding: '10px',
    };

    const resultLabelStyle: React.CSSProperties = {
      fontSize: '9px',
      fontWeight: 700,
      color: FINCEPT.GRAY,
      letterSpacing: '0.5px',
      marginBottom: '4px',
    };

    const resultValueStyle: React.CSSProperties = {
      fontSize: '12px',
      fontWeight: 700,
      color: FINCEPT.CYAN,
      fontFamily: '"IBM Plex Mono", monospace',
    };

    const bigResultValueStyle: React.CSSProperties = {
      fontSize: '16px',
      fontWeight: 700,
      color: FINCEPT.ORANGE,
      fontFamily: '"IBM Plex Mono", monospace',
    };

    return (
      <div style={{
        marginTop: '16px',
        ...panelStyle,
      }}>
        <div style={{
          ...sectionHeaderStyle,
          justifyContent: 'space-between',
        }}>
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>RESULTS</span>
          <button
            onClick={copyResults}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
          >
            <Copy size={10} />
            COPY
          </button>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
          {/* Bond Results */}
          {result.clean_price !== undefined && (
            <>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>CLEAN PRICE</div>
                  <div style={resultValueStyle}>{result.clean_price.toFixed(4)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>DIRTY PRICE</div>
                  <div style={resultValueStyle}>{result.dirty_price?.toFixed(4)}</div>
                </div>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>DURATION</div>
                  <div style={resultValueStyle}>{result.duration?.toFixed(2)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>CONVEXITY</div>
                  <div style={resultValueStyle}>{result.convexity?.toFixed(4)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>ACCRUED INT</div>
                  <div style={resultValueStyle}>{result.accrued_interest?.toFixed(4)}</div>
                </div>
              </div>
            </>
          )}

          {/* YTM Results */}
          {result.ytm !== undefined && !result.clean_price && (
            <div style={resultCardStyle}>
              <div style={resultLabelStyle}>YIELD TO MATURITY</div>
              <div style={bigResultValueStyle}>{result.ytm.toFixed(4)}%</div>
            </div>
          )}

          {/* Option Results */}
          {result.price !== undefined && result.greeks && (
            <>
              <div style={resultCardStyle}>
                <div style={resultLabelStyle}>OPTION PRICE</div>
                <div style={bigResultValueStyle}>{result.price.toFixed(4)}</div>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '8px' }}>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>DELTA</div>
                  <div style={resultValueStyle}>{result.greeks.delta.toFixed(4)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>GAMMA</div>
                  <div style={resultValueStyle}>{result.greeks.gamma.toFixed(4)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>VEGA</div>
                  <div style={resultValueStyle}>{result.greeks.vega.toFixed(4)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>THETA</div>
                  <div style={resultValueStyle}>{result.greeks.theta.toFixed(4)}</div>
                </div>
                <div style={resultCardStyle}>
                  <div style={resultLabelStyle}>RHO</div>
                  <div style={resultValueStyle}>{result.greeks.rho.toFixed(4)}</div>
                </div>
              </div>
            </>
          )}

          {/* Implied Vol Results */}
          {result.implied_volatility !== undefined && (
            <div style={resultCardStyle}>
              <div style={resultLabelStyle}>IMPLIED VOLATILITY</div>
              <div style={bigResultValueStyle}>{result.implied_volatility.toFixed(4)}%</div>
            </div>
          )}

          {/* Raw JSON for other results */}
          {!result.clean_price && !result.ytm && !result.price && !result.greeks && !result.implied_volatility && (
            <pre style={{
              backgroundColor: FINCEPT.HEADER_BG,
              borderRadius: '2px',
              padding: '12px',
              fontSize: '9px',
              color: FINCEPT.CYAN,
              overflow: 'auto',
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {JSON.stringify(result, null, 2)}
            </pre>
          )}
        </div>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Calculator size={20} style={{ color: FINCEPT.ORANGE }} />
          <div>
            <h1 style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE, margin: 0 }}>
              DERIVATIVES PRICING
            </h1>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              PROFESSIONAL VALUATION POWERED BY FINANCEPY & PY_VOLLIB
            </span>
          </div>
        </div>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          borderRadius: '2px',
        }}>
          <Zap size={10} style={{ color: FINCEPT.GREEN }} />
          <span style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GREEN }}>FINANCEPY ACTIVE</span>
        </div>
      </div>

      {/* Content Area */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {renderInstrumentSelector()}

        <div style={{ maxWidth: '1400px' }}>
          {activeInstrument === 'bonds' && renderBondsPanel()}
          {activeInstrument === 'equity-options' && renderEquityOptionsPanel()}
          {activeInstrument === 'fx-options' && renderFXOptionsPanel()}
          {activeInstrument === 'swaps' && renderSwapsPanel()}
          {activeInstrument === 'credit' && renderCreditPanel()}
          {activeInstrument === 'vollib' && renderVolLibPanel()}

          {renderResults()}
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontSize: '9px',
        color: FINCEPT.GRAY,
      }}>
        <span>DERIVATIVES TAB</span>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>INSTRUMENT: {activeInstrument.toUpperCase().replace('-', ' ')}</span>
          <span style={{ color: FINCEPT.CYAN }}>{activeInstrument === 'vollib' ? 'PY_VOLLIB' : 'FINANCEPY v1.0.1'}</span>
        </div>
      </div>
    </div>
  );
}
