// File: src/components/tabs/DerivativesTab.tsx
// Professional Derivatives Pricing & Valuation - Powered by FinancePy

import React, { useState } from 'react';
import {
  Calculator, TrendingUp, DollarSign, Activity, BarChart3,
  Percent, Calendar, Settings, Info, ChevronDown, ChevronUp,
  Copy, Download, RefreshCw, Zap, Target, AlertCircle
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';
import { TabFooter } from '@/components/common/TabFooter';

// Fincept-style color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  BLUE: '#0088FF',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BG: '#0F0F0F',
  PANEL: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  TEXT: '#FFFFFF',
  MUTED: '#787878'
};

type InstrumentType = 'bonds' | 'equity-options' | 'fx-options' | 'swaps' | 'credit';

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

  const copyResults = () => {
    if (result) {
      navigator.clipboard.writeText(JSON.stringify(result, null, 2));
    }
  };

  const renderInstrumentSelector = () => {
    const instruments = [
      { id: 'bonds', label: 'Bonds', icon: DollarSign },
      { id: 'equity-options', label: 'Equity Options', icon: TrendingUp },
      { id: 'fx-options', label: 'FX Options', icon: Activity },
      { id: 'swaps', label: 'Interest Rate Swaps', icon: BarChart3 },
      { id: 'credit', label: 'Credit Derivatives', icon: Target }
    ];

    return (
      <div className="flex gap-2 mb-6 overflow-x-auto pb-2">
        {instruments.map(({ id, label, icon: Icon }) => (
          <button
            key={id}
            onClick={() => {
              setActiveInstrument(id as InstrumentType);
              setResult(null);
              setError(null);
            }}
            className={`flex items-center gap-2 px-4 py-2 rounded whitespace-nowrap transition-all ${
              activeInstrument === id
                ? 'bg-orange-500 text-white shadow-lg'
                : 'bg-gray-800 text-gray-300 hover:bg-gray-700'
            }`}
          >
            <Icon className="w-4 h-4" />
            <span className="text-sm font-medium">{label}</span>
          </button>
        ))}
      </div>
    );
  };

  const renderBondsPanel = () => (
    <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
      {/* Bond Pricing Calculator */}
      <div className="bg-gray-900 rounded-lg border border-gray-800 p-6">
        <div className="flex items-center gap-2 mb-4">
          <Calculator className="w-5 h-5 text-orange-500" />
          <h3 className="text-lg font-semibold text-white">Bond Price Calculator</h3>
        </div>

        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Issue Date</label>
              <input
                type="date"
                value={bondParams.issueDate}
                onChange={(e) => setBondParams({ ...bondParams, issueDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Settlement Date</label>
              <input
                type="date"
                value={bondParams.settlementDate}
                onChange={(e) => setBondParams({ ...bondParams, settlementDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div>
            <label className="block text-xs text-gray-400 mb-1">Maturity Date</label>
            <input
              type="date"
              value={bondParams.maturityDate}
              onChange={(e) => setBondParams({ ...bondParams, maturityDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Coupon Rate (%)</label>
              <input
                type="number"
                step="0.1"
                value={bondParams.couponRate}
                onChange={(e) => setBondParams({ ...bondParams, couponRate: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">YTM (%)</label>
              <input
                type="number"
                step="0.1"
                value={bondParams.ytm}
                onChange={(e) => setBondParams({ ...bondParams, ytm: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div>
            <label className="block text-xs text-gray-400 mb-1">Payment Frequency</label>
            <select
              value={bondParams.freq}
              onChange={(e) => setBondParams({ ...bondParams, freq: parseInt(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            >
              <option value={1}>Annual</option>
              <option value={2}>Semi-Annual</option>
              <option value={4}>Quarterly</option>
            </select>
          </div>

          <button
            onClick={calculateBondPrice}
            disabled={loading}
            className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
          >
            {loading ? 'Calculating...' : 'Calculate Bond Price'}
          </button>
        </div>
      </div>

      {/* YTM Calculator */}
      <div className="bg-gray-900 rounded-lg border border-gray-800 p-6">
        <div className="flex items-center gap-2 mb-4">
          <Percent className="w-5 h-5 text-orange-500" />
          <h3 className="text-lg font-semibold text-white">Yield to Maturity Calculator</h3>
        </div>

        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Issue Date</label>
              <input
                type="date"
                value={bondParams.issueDate}
                onChange={(e) => setBondParams({ ...bondParams, issueDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Settlement Date</label>
              <input
                type="date"
                value={bondParams.settlementDate}
                onChange={(e) => setBondParams({ ...bondParams, settlementDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div>
            <label className="block text-xs text-gray-400 mb-1">Maturity Date</label>
            <input
              type="date"
              value={bondParams.maturityDate}
              onChange={(e) => setBondParams({ ...bondParams, maturityDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Coupon Rate (%)</label>
              <input
                type="number"
                step="0.1"
                value={bondParams.couponRate}
                onChange={(e) => setBondParams({ ...bondParams, couponRate: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Clean Price</label>
              <input
                type="number"
                step="0.01"
                value={bondParams.cleanPrice}
                onChange={(e) => setBondParams({ ...bondParams, cleanPrice: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div>
            <label className="block text-xs text-gray-400 mb-1">Payment Frequency</label>
            <select
              value={bondParams.freq}
              onChange={(e) => setBondParams({ ...bondParams, freq: parseInt(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            >
              <option value={1}>Annual</option>
              <option value={2}>Semi-Annual</option>
              <option value={4}>Quarterly</option>
            </select>
          </div>

          <button
            onClick={calculateBondYTM}
            disabled={loading}
            className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
          >
            {loading ? 'Calculating...' : 'Calculate YTM'}
          </button>
        </div>
      </div>
    </div>
  );

  const renderEquityOptionsPanel = () => (
    <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
      {/* Option Pricing */}
      <div className="bg-gray-900 rounded-lg border border-gray-800 p-6">
        <div className="flex items-center gap-2 mb-4">
          <TrendingUp className="w-5 h-5 text-orange-500" />
          <h3 className="text-lg font-semibold text-white">Black-Scholes Option Pricing</h3>
        </div>

        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Valuation Date</label>
              <input
                type="date"
                value={optionParams.valuationDate}
                onChange={(e) => setOptionParams({ ...optionParams, valuationDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Expiry Date</label>
              <input
                type="date"
                value={optionParams.expiryDate}
                onChange={(e) => setOptionParams({ ...optionParams, expiryDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Strike Price</label>
              <input
                type="number"
                step="0.01"
                value={optionParams.strike}
                onChange={(e) => setOptionParams({ ...optionParams, strike: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Spot Price</label>
              <input
                type="number"
                step="0.01"
                value={optionParams.spot}
                onChange={(e) => setOptionParams({ ...optionParams, spot: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Volatility (%)</label>
              <input
                type="number"
                step="0.1"
                value={optionParams.volatility}
                onChange={(e) => setOptionParams({ ...optionParams, volatility: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Risk-Free Rate (%)</label>
              <input
                type="number"
                step="0.1"
                value={optionParams.riskFreeRate}
                onChange={(e) => setOptionParams({ ...optionParams, riskFreeRate: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Dividend Yield (%)</label>
              <input
                type="number"
                step="0.1"
                value={optionParams.dividendYield}
                onChange={(e) => setOptionParams({ ...optionParams, dividendYield: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Option Type</label>
              <select
                value={optionParams.optionType}
                onChange={(e) => setOptionParams({ ...optionParams, optionType: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              >
                <option value="call">Call</option>
                <option value="put">Put</option>
              </select>
            </div>
          </div>

          <button
            onClick={calculateOptionPrice}
            disabled={loading}
            className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
          >
            {loading ? 'Calculating...' : 'Calculate Option Price & Greeks'}
          </button>
        </div>
      </div>

      {/* Implied Volatility */}
      <div className="bg-gray-900 rounded-lg border border-gray-800 p-6">
        <div className="flex items-center gap-2 mb-4">
          <Activity className="w-5 h-5 text-orange-500" />
          <h3 className="text-lg font-semibold text-white">Implied Volatility Calculator</h3>
        </div>

        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Valuation Date</label>
              <input
                type="date"
                value={optionParams.valuationDate}
                onChange={(e) => setOptionParams({ ...optionParams, valuationDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Expiry Date</label>
              <input
                type="date"
                value={optionParams.expiryDate}
                onChange={(e) => setOptionParams({ ...optionParams, expiryDate: e.target.value })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Strike Price</label>
              <input
                type="number"
                step="0.01"
                value={optionParams.strike}
                onChange={(e) => setOptionParams({ ...optionParams, strike: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Spot Price</label>
              <input
                type="number"
                step="0.01"
                value={optionParams.spot}
                onChange={(e) => setOptionParams({ ...optionParams, spot: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div>
            <label className="block text-xs text-gray-400 mb-1">Market Option Price</label>
            <input
              type="number"
              step="0.01"
              value={optionParams.optionPrice}
              onChange={(e) => setOptionParams({ ...optionParams, optionPrice: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-xs text-gray-400 mb-1">Risk-Free Rate (%)</label>
              <input
                type="number"
                step="0.1"
                value={optionParams.riskFreeRate}
                onChange={(e) => setOptionParams({ ...optionParams, riskFreeRate: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
            <div>
              <label className="block text-xs text-gray-400 mb-1">Dividend Yield (%)</label>
              <input
                type="number"
                step="0.1"
                value={optionParams.dividendYield}
                onChange={(e) => setOptionParams({ ...optionParams, dividendYield: parseFloat(e.target.value) })}
                className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
              />
            </div>
          </div>

          <div>
            <label className="block text-xs text-gray-400 mb-1">Option Type</label>
            <select
              value={optionParams.optionType}
              onChange={(e) => setOptionParams({ ...optionParams, optionType: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            >
              <option value="call">Call</option>
              <option value="put">Put</option>
            </select>
          </div>

          <button
            onClick={calculateImpliedVol}
            disabled={loading}
            className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
          >
            {loading ? 'Calculating...' : 'Calculate Implied Volatility'}
          </button>
        </div>
      </div>
    </div>
  );

  const renderFXOptionsPanel = () => (
    <div className="bg-gray-900 rounded-lg border border-gray-800 p-6 max-w-2xl">
      <div className="flex items-center gap-2 mb-4">
        <Activity className="w-5 h-5 text-orange-500" />
        <h3 className="text-lg font-semibold text-white">FX Vanilla Option Pricing</h3>
      </div>

      <div className="space-y-4">
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Valuation Date</label>
            <input
              type="date"
              value={fxParams.valuationDate}
              onChange={(e) => setFxParams({ ...fxParams, valuationDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Expiry Date</label>
            <input
              type="date"
              value={fxParams.expiryDate}
              onChange={(e) => setFxParams({ ...fxParams, expiryDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Strike FX Rate</label>
            <input
              type="number"
              step="0.0001"
              value={fxParams.strike}
              onChange={(e) => setFxParams({ ...fxParams, strike: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Spot FX Rate</label>
            <input
              type="number"
              step="0.0001"
              value={fxParams.spot}
              onChange={(e) => setFxParams({ ...fxParams, spot: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <div className="grid grid-cols-3 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Volatility (%)</label>
            <input
              type="number"
              step="0.1"
              value={fxParams.volatility}
              onChange={(e) => setFxParams({ ...fxParams, volatility: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Domestic Rate (%)</label>
            <input
              type="number"
              step="0.1"
              value={fxParams.domesticRate}
              onChange={(e) => setFxParams({ ...fxParams, domesticRate: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Foreign Rate (%)</label>
            <input
              type="number"
              step="0.1"
              value={fxParams.foreignRate}
              onChange={(e) => setFxParams({ ...fxParams, foreignRate: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Notional</label>
            <input
              type="number"
              step="10000"
              value={fxParams.notional}
              onChange={(e) => setFxParams({ ...fxParams, notional: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Option Type</label>
            <select
              value={fxParams.optionType}
              onChange={(e) => setFxParams({ ...fxParams, optionType: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            >
              <option value="call">Call</option>
              <option value="put">Put</option>
            </select>
          </div>
        </div>

        <button
          onClick={calculateFXOption}
          disabled={loading}
          className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
        >
          {loading ? 'Calculating...' : 'Calculate FX Option Price'}
        </button>
      </div>
    </div>
  );

  const renderSwapsPanel = () => (
    <div className="bg-gray-900 rounded-lg border border-gray-800 p-6 max-w-2xl">
      <div className="flex items-center gap-2 mb-4">
        <BarChart3 className="w-5 h-5 text-orange-500" />
        <h3 className="text-lg font-semibold text-white">Interest Rate Swap Pricing</h3>
      </div>

      <div className="space-y-4">
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Effective Date</label>
            <input
              type="date"
              value={swapParams.effectiveDate}
              onChange={(e) => setSwapParams({ ...swapParams, effectiveDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Maturity Date</label>
            <input
              type="date"
              value={swapParams.maturityDate}
              onChange={(e) => setSwapParams({ ...swapParams, maturityDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <div className="grid grid-cols-3 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Fixed Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={swapParams.fixedRate}
              onChange={(e) => setSwapParams({ ...swapParams, fixedRate: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Frequency</label>
            <select
              value={swapParams.freq}
              onChange={(e) => setSwapParams({ ...swapParams, freq: parseInt(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            >
              <option value={1}>Annual</option>
              <option value={2}>Semi-Annual</option>
              <option value={4}>Quarterly</option>
            </select>
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Discount Rate (%)</label>
            <input
              type="number"
              step="0.01"
              value={swapParams.discountRate}
              onChange={(e) => setSwapParams({ ...swapParams, discountRate: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <div>
          <label className="block text-xs text-gray-400 mb-1">Notional Amount</label>
          <input
            type="number"
            step="100000"
            value={swapParams.notional}
            onChange={(e) => setSwapParams({ ...swapParams, notional: parseFloat(e.target.value) })}
            className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
          />
        </div>

        <button
          onClick={calculateSwap}
          disabled={loading}
          className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
        >
          {loading ? 'Calculating...' : 'Calculate Swap Value'}
        </button>
      </div>
    </div>
  );

  const renderCreditPanel = () => (
    <div className="bg-gray-900 rounded-lg border border-gray-800 p-6 max-w-2xl">
      <div className="flex items-center gap-2 mb-4">
        <Target className="w-5 h-5 text-orange-500" />
        <h3 className="text-lg font-semibold text-white">Credit Default Swap Pricing</h3>
      </div>

      <div className="space-y-4">
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Valuation Date</label>
            <input
              type="date"
              value={cdsParams.valuationDate}
              onChange={(e) => setCdsParams({ ...cdsParams, valuationDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Maturity Date</label>
            <input
              type="date"
              value={cdsParams.maturityDate}
              onChange={(e) => setCdsParams({ ...cdsParams, maturityDate: e.target.value })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <div className="grid grid-cols-3 gap-3">
          <div>
            <label className="block text-xs text-gray-400 mb-1">Recovery Rate (%)</label>
            <input
              type="number"
              step="1"
              value={cdsParams.recoveryRate}
              onChange={(e) => setCdsParams({ ...cdsParams, recoveryRate: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Spread (bps)</label>
            <input
              type="number"
              step="1"
              value={cdsParams.spreadBps}
              onChange={(e) => setCdsParams({ ...cdsParams, spreadBps: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
          <div>
            <label className="block text-xs text-gray-400 mb-1">Notional</label>
            <input
              type="number"
              step="1000000"
              value={cdsParams.notional}
              onChange={(e) => setCdsParams({ ...cdsParams, notional: parseFloat(e.target.value) })}
              className="w-full bg-gray-800 border border-gray-700 rounded px-3 py-2 text-sm text-white"
            />
          </div>
        </div>

        <button
          onClick={calculateCDS}
          disabled={loading}
          className="w-full bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 rounded transition-colors disabled:opacity-50"
        >
          {loading ? 'Calculating...' : 'Calculate CDS Value'}
        </button>
      </div>
    </div>
  );

  const renderResults = () => {
    if (error) {
      return (
        <div className="mt-6 bg-red-900/20 border border-red-500 rounded-lg p-4">
          <div className="flex items-start gap-2">
            <AlertCircle className="w-5 h-5 text-red-500 mt-0.5" />
            <div>
              <h4 className="text-red-500 font-semibold mb-1">Error</h4>
              <p className="text-sm text-red-300">{error}</p>
            </div>
          </div>
        </div>
      );
    }

    if (!result) return null;

    return (
      <div className="mt-6 bg-gray-900 border border-gray-800 rounded-lg p-6">
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-lg font-semibold text-white">Results</h3>
          <button
            onClick={copyResults}
            className="flex items-center gap-2 px-3 py-1.5 bg-gray-800 hover:bg-gray-700 text-gray-300 rounded text-sm transition-colors"
          >
            <Copy className="w-4 h-4" />
            Copy
          </button>
        </div>

        <div className="space-y-3">
          {/* Bond Results */}
          {result.clean_price !== undefined && (
            <>
              <div className="grid grid-cols-2 gap-4">
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Clean Price</div>
                  <div className="text-lg font-semibold text-white">{result.clean_price.toFixed(4)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Dirty Price</div>
                  <div className="text-lg font-semibold text-white">{result.dirty_price?.toFixed(4)}</div>
                </div>
              </div>
              <div className="grid grid-cols-3 gap-4">
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Duration</div>
                  <div className="text-lg font-semibold text-white">{result.duration?.toFixed(2)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Convexity</div>
                  <div className="text-lg font-semibold text-white">{result.convexity?.toFixed(4)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Accrued Interest</div>
                  <div className="text-lg font-semibold text-white">{result.accrued_interest?.toFixed(4)}</div>
                </div>
              </div>
            </>
          )}

          {/* YTM Results */}
          {result.ytm !== undefined && !result.clean_price && (
            <div className="bg-gray-800 rounded p-4">
              <div className="text-xs text-gray-400 mb-1">Yield to Maturity</div>
              <div className="text-2xl font-bold text-orange-500">{result.ytm.toFixed(4)}%</div>
            </div>
          )}

          {/* Option Results */}
          {result.price !== undefined && result.greeks && (
            <>
              <div className="bg-gray-800 rounded p-4">
                <div className="text-xs text-gray-400 mb-1">Option Price</div>
                <div className="text-2xl font-bold text-orange-500">{result.price.toFixed(4)}</div>
              </div>
              <div className="grid grid-cols-5 gap-3">
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Delta</div>
                  <div className="text-sm font-semibold text-white">{result.greeks.delta.toFixed(4)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Gamma</div>
                  <div className="text-sm font-semibold text-white">{result.greeks.gamma.toFixed(4)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Vega</div>
                  <div className="text-sm font-semibold text-white">{result.greeks.vega.toFixed(4)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Theta</div>
                  <div className="text-sm font-semibold text-white">{result.greeks.theta.toFixed(4)}</div>
                </div>
                <div className="bg-gray-800 rounded p-3">
                  <div className="text-xs text-gray-400 mb-1">Rho</div>
                  <div className="text-sm font-semibold text-white">{result.greeks.rho.toFixed(4)}</div>
                </div>
              </div>
            </>
          )}

          {/* Implied Vol Results */}
          {result.implied_volatility !== undefined && (
            <div className="bg-gray-800 rounded p-4">
              <div className="text-xs text-gray-400 mb-1">Implied Volatility</div>
              <div className="text-2xl font-bold text-orange-500">{result.implied_volatility.toFixed(4)}%</div>
            </div>
          )}

          {/* Raw JSON for other results */}
          {!result.clean_price && !result.ytm && !result.price && !result.greeks && !result.implied_volatility && (
            <pre className="bg-gray-800 rounded p-4 text-xs text-gray-300 overflow-x-auto">
              {JSON.stringify(result, null, 2)}
            </pre>
          )}
        </div>
      </div>
    );
  };

  return (
    <div className="h-full flex flex-col bg-black text-white">
      {/* Header */}
      <div className="bg-gray-900 border-b border-gray-800 px-6 py-4">
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold text-white flex items-center gap-2">
              <Calculator className="w-6 h-6 text-orange-500" />
              Derivatives Pricing
            </h1>
            <p className="text-sm text-gray-400 mt-1">
              Professional derivatives valuation powered by FinancePy
            </p>
          </div>
          <div className="flex items-center gap-3">
            <div className="flex items-center gap-2 bg-green-500/10 px-3 py-1.5 rounded">
              <Zap className="w-4 h-4 text-green-500" />
              <span className="text-xs font-medium text-green-500">FinancePy Active</span>
            </div>
          </div>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-y-auto p-6">
        {renderInstrumentSelector()}

        <div className="max-w-7xl mx-auto">
          {activeInstrument === 'bonds' && renderBondsPanel()}
          {activeInstrument === 'equity-options' && renderEquityOptionsPanel()}
          {activeInstrument === 'fx-options' && renderFXOptionsPanel()}
          {activeInstrument === 'swaps' && renderSwapsPanel()}
          {activeInstrument === 'credit' && renderCreditPanel()}

          {renderResults()}
        </div>
      </div>

      <TabFooter tabName="Derivatives" statusInfo="FinancePy v1.0.1" />
    </div>
  );
}
