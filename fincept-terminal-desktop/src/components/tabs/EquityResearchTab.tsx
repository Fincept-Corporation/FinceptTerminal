import React, { useState, useEffect, useRef } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { invoke } from '@tauri-apps/api/core';
import { createChart, CandlestickSeries, HistogramSeries, LineSeries, BarSeries } from 'lightweight-charts';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, ComposedChart, Bar, Area } from 'recharts';

const COLORS = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  RED: '#ef4444',
  GREEN: '#10b981',
  YELLOW: '#fbbf24',
  GRAY: '#787878',
  BLUE: '#3b82f6',
  CYAN: '#06b6d4',
  DARK_BG: '#0a0a0a',
  PANEL_BG: '#1a1a1a',
  BORDER: '#333333',
  MAGENTA: '#FF00FF',
  PURPLE: '#9D4EDD',
};

interface StockInfo {
  symbol: string;
  company_name: string;
  sector: string;
  industry: string;
  market_cap: number | null;
  pe_ratio: number | null;
  forward_pe: number | null;
  dividend_yield: number | null;
  beta: number | null;
  fifty_two_week_high: number | null;
  fifty_two_week_low: number | null;
  average_volume: number | null;
  description: string;
  website: string;
  country: string;
  currency: string;
  exchange: string;
  employees: number | null;
  current_price: number | null;
  target_high_price: number | null;
  target_low_price: number | null;
  target_mean_price: number | null;
  recommendation_mean: number | null;
  recommendation_key: string | null;
  number_of_analyst_opinions: number | null;
  total_cash: number | null;
  total_debt: number | null;
  total_revenue: number | null;
  revenue_per_share: number | null;
  return_on_assets: number | null;
  return_on_equity: number | null;
  gross_profits: number | null;
  free_cashflow: number | null;
  operating_cashflow: number | null;
  earnings_growth: number | null;
  revenue_growth: number | null;
  gross_margins: number | null;
  operating_margins: number | null;
  ebitda_margins: number | null;
  profit_margins: number | null;
  book_value: number | null;
  price_to_book: number | null;
  enterprise_value: number | null;
  enterprise_to_revenue: number | null;
  enterprise_to_ebitda: number | null;
  shares_outstanding: number | null;
  float_shares: number | null;
  held_percent_insiders: number | null;
  held_percent_institutions: number | null;
  short_ratio: number | null;
  short_percent_of_float: number | null;
  peg_ratio: number | null;
}

interface HistoricalData {
  symbol: string;
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

interface QuoteData {
  symbol: string;
  price: number;
  change: number;
  change_percent: number;
  volume: number | null;
  high: number | null;
  low: number | null;
  open: number | null;
  previous_close: number | null;
}

interface FinancialsData {
  symbol: string;
  income_statement: Record<string, Record<string, number>>;
  balance_sheet: Record<string, Record<string, number>>;
  cash_flow: Record<string, Record<string, number>>;
  timestamp: number;
}

// Lightweight Indicator Box Component with Parameter Controls
const IndicatorBox: React.FC<{
  name: string;
  latestValue: number;
  min: number;
  max: number;
  avg: number;
  values: number[];
  priceData: any[];
  color: string;
  category: string;
  onRecompute?: (params: any) => void;
}> = ({ name, latestValue, min, max, avg, values, priceData, color, category, onRecompute }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [customThresholds, setCustomThresholds] = useState<{ upper: number; lower: number } | null>(null);
  const [customParams, setCustomParams] = useState<any>(null);

  const formatValue = (val: number) => {
    if (Math.abs(val) < 0.01) return val.toFixed(6);
    if (Math.abs(val) < 1) return val.toFixed(4);
    if (Math.abs(val) < 100) return val.toFixed(2);
    return val.toFixed(0);
  };

  // Only create indicator chart data (no price data for performance)
  const chartData = values.map((value, i) => ({
    index: i,
    value: value || null,
  }));

  // Get default parameters for this indicator
  const getDefaultParams = (): any => {
    const nameUpper = name.toUpperCase();

    // Moving Averages
    if (nameUpper.includes('SMA_5')) return { period: 5 };
    if (nameUpper.includes('SMA_10')) return { period: 10 };
    if (nameUpper.includes('SMA_20')) return { period: 20 };
    if (nameUpper.includes('SMA_50')) return { period: 50 };
    if (nameUpper.includes('SMA_200')) return { period: 200 };
    if (nameUpper.includes('EMA_9')) return { period: 9 };
    if (nameUpper.includes('EMA_12')) return { period: 12 };
    if (nameUpper.includes('EMA_21')) return { period: 21 };
    if (nameUpper.includes('EMA_26')) return { period: 26 };
    if (nameUpper.includes('EMA_50')) return { period: 50 };
    if (nameUpper.includes('WMA')) return { period: 9 };

    // RSI
    if (nameUpper.includes('RSI')) return { period: 14 };

    // MACD
    if (nameUpper.includes('MACD')) return { fast: 12, slow: 26, signal: 9 };

    // Stochastic
    if (nameUpper.includes('STOCH')) return { k_period: 14, d_period: 3 };

    // Bollinger Bands
    if (nameUpper.includes('BB_')) return { period: 20, std: 2 };

    // ATR
    if (nameUpper.includes('ATR')) return { period: 14 };

    // ADX
    if (nameUpper.includes('ADX')) return { period: 14 };

    // CCI
    if (nameUpper.includes('CCI')) return { period: 20 };

    // Williams %R
    if (nameUpper.includes('WILLIAMS')) return { period: 14 };

    // ROC
    if (nameUpper.includes('ROC')) return { period: 12 };

    // MFI
    if (nameUpper === 'MFI') return { period: 14 };

    // Aroon
    if (nameUpper.includes('AROON')) return { period: 25 };

    // Awesome Oscillator
    if (nameUpper.includes('AWESOME') || nameUpper === 'AO') return { fast: 5, slow: 34 };

    // TSI
    if (nameUpper === 'TSI') return { long: 25, short: 13 };

    // Ultimate Oscillator
    if (nameUpper === 'UO' || nameUpper.includes('ULTIMATE')) return { period1: 7, period2: 14, period3: 28 };

    return null;
  };

  // Get default thresholds for this indicator
  const getDefaultThresholds = (): { upper: number; lower: number; label: string } => {
    const nameUpper = name.toUpperCase();

    if (nameUpper.includes('RSI')) return { upper: 70, lower: 30, label: 'Overbought/Oversold' };
    if (nameUpper.includes('STOCH')) return { upper: 80, lower: 20, label: 'Overbought/Oversold' };
    if (nameUpper.includes('WILLIAMS')) return { upper: -20, lower: -80, label: 'Overbought/Oversold' };
    if (nameUpper.includes('CCI')) return { upper: 100, lower: -100, label: 'Overbought/Oversold' };
    if (nameUpper === 'TSI') return { upper: 25, lower: -25, label: 'Overbought/Oversold' };
    if (nameUpper === 'UO' || nameUpper.includes('ULTIMATE')) return { upper: 70, lower: 30, label: 'Overbought/Oversold' };
    if (nameUpper === 'MFI') return { upper: 80, lower: 20, label: 'Overbought/Oversold' };
    if (nameUpper.includes('ROC')) return { upper: 5, lower: -5, label: 'Strength %' };
    if (nameUpper.includes('ADX') && !nameUpper.includes('POS') && !nameUpper.includes('NEG')) return { upper: 25, lower: 20, label: 'Trend Strength' };
    if (nameUpper.includes('AROON')) return { upper: 70, lower: 30, label: 'Trend Strength' };
    if (nameUpper.includes('BB_PBAND') || nameUpper.includes('KC_PBAND') || nameUpper.includes('DC_PBAND')) return { upper: 0.8, lower: 0.2, label: 'Band Position' };
    if (nameUpper.includes('CMF')) return { upper: 0.1, lower: -0.1, label: 'Money Flow' };

    return { upper: 0, lower: 0, label: 'Custom' };
  };

  const defaultThresholds = getDefaultThresholds();
  const activeThresholds = customThresholds || defaultThresholds;
  const defaultParams = getDefaultParams();
  const activeParams = customParams || defaultParams;

  // Comprehensive signal detection for ALL 80+ indicators
  const getSignal = (): { signal: 'BUY' | 'SELL' | 'NEUTRAL'; reason: string } => {
    const nameUpper = name.toUpperCase();
    const prevValue = values[values.length - 2] || 0;

    // === MOMENTUM INDICATORS ===
    if (nameUpper.includes('RSI')) {
      if (latestValue <= activeThresholds.lower) return { signal: 'BUY', reason: `Oversold <${activeThresholds.lower}` };
      if (latestValue >= activeThresholds.upper) return { signal: 'SELL', reason: `Overbought >${activeThresholds.upper}` };
      return { signal: 'NEUTRAL', reason: `${activeThresholds.lower}-${activeThresholds.upper} range` };
    }

    if (nameUpper.includes('STOCH') && !nameUpper.includes('RSI')) {
      if (latestValue <= activeThresholds.lower) return { signal: 'BUY', reason: `Oversold <${activeThresholds.lower}` };
      if (latestValue >= activeThresholds.upper) return { signal: 'SELL', reason: `Overbought >${activeThresholds.upper}` };
      return { signal: 'NEUTRAL', reason: `${activeThresholds.lower}-${activeThresholds.upper} range` };
    }

    if (nameUpper.includes('WILLIAMS')) {
      if (latestValue <= -80) return { signal: 'BUY', reason: 'Oversold' };
      if (latestValue >= -20) return { signal: 'SELL', reason: 'Overbought' };
      return { signal: 'NEUTRAL', reason: '-20 to -80' };
    }

    if (nameUpper === 'AO' || nameUpper.includes('AWESOME')) {
      if (prevValue < 0 && latestValue > 0) return { signal: 'BUY', reason: 'Zero cross up' };
      if (prevValue > 0 && latestValue < 0) return { signal: 'SELL', reason: 'Zero cross down' };
      return { signal: 'NEUTRAL', reason: 'No crossover' };
    }

    if (nameUpper.includes('ROC')) {
      if (latestValue > 5) return { signal: 'BUY', reason: 'Strong >5%' };
      if (latestValue < -5) return { signal: 'SELL', reason: 'Weak <-5%' };
      return { signal: 'NEUTRAL', reason: '-5% to 5%' };
    }

    if (nameUpper === 'TSI') {
      if (latestValue < -25) return { signal: 'BUY', reason: 'Oversold' };
      if (latestValue > 25) return { signal: 'SELL', reason: 'Overbought' };
      return { signal: 'NEUTRAL', reason: '-25 to 25' };
    }

    if (nameUpper === 'UO' || nameUpper.includes('ULTIMATE')) {
      if (latestValue < 30) return { signal: 'BUY', reason: 'Oversold <30' };
      if (latestValue > 70) return { signal: 'SELL', reason: 'Overbought >70' };
      return { signal: 'NEUTRAL', reason: '30-70 range' };
    }

    if (nameUpper.includes('PPO') && !nameUpper.includes('SIGNAL') && !nameUpper.includes('HIST')) {
      if (prevValue < 0 && latestValue > 0) return { signal: 'BUY', reason: 'Zero cross up' };
      if (prevValue > 0 && latestValue < 0) return { signal: 'SELL', reason: 'Zero cross down' };
      return { signal: 'NEUTRAL', reason: 'No crossover' };
    }

    // === TREND INDICATORS ===
    if (nameUpper.includes('MACD') && !nameUpper.includes('SIGNAL') && !nameUpper.includes('DIFF')) {
      if (prevValue < 0 && latestValue > 0) return { signal: 'BUY', reason: 'Bullish crossover' };
      if (prevValue > 0 && latestValue < 0) return { signal: 'SELL', reason: 'Bearish crossover' };
      return { signal: 'NEUTRAL', reason: 'No crossover' };
    }

    if (nameUpper.includes('CCI')) {
      if (latestValue <= -100) return { signal: 'BUY', reason: 'Oversold <-100' };
      if (latestValue >= 100) return { signal: 'SELL', reason: 'Overbought >100' };
      return { signal: 'NEUTRAL', reason: '-100 to 100' };
    }

    if (nameUpper.includes('ADX') && !nameUpper.includes('POS') && !nameUpper.includes('NEG')) {
      if (latestValue > 25) return { signal: 'BUY', reason: 'Strong trend >25' };
      if (latestValue < 20) return { signal: 'NEUTRAL', reason: 'Weak trend <20' };
      return { signal: 'NEUTRAL', reason: 'Moderate 20-25' };
    }

    if (nameUpper.includes('AROON_UP')) {
      if (latestValue > 70) return { signal: 'BUY', reason: 'Strong uptrend' };
      return { signal: 'NEUTRAL', reason: 'Weak uptrend' };
    }

    if (nameUpper.includes('AROON_DOWN')) {
      if (latestValue > 70) return { signal: 'SELL', reason: 'Strong downtrend' };
      return { signal: 'NEUTRAL', reason: 'Weak downtrend' };
    }

    if (nameUpper.includes('AROON_INDICATOR')) {
      if (latestValue > 50) return { signal: 'BUY', reason: 'Bullish' };
      if (latestValue < -50) return { signal: 'SELL', reason: 'Bearish' };
      return { signal: 'NEUTRAL', reason: '-50 to 50' };
    }

    if (nameUpper.includes('PSAR_UP_INDICATOR')) {
      if (latestValue === 1) return { signal: 'BUY', reason: 'Uptrend active' };
      return { signal: 'NEUTRAL', reason: 'No uptrend' };
    }

    if (nameUpper.includes('PSAR_DOWN_INDICATOR')) {
      if (latestValue === 1) return { signal: 'SELL', reason: 'Downtrend active' };
      return { signal: 'NEUTRAL', reason: 'No downtrend' };
    }

    if (nameUpper.includes('TRIX')) {
      if (prevValue < 0 && latestValue > 0) return { signal: 'BUY', reason: 'Zero cross up' };
      if (prevValue > 0 && latestValue < 0) return { signal: 'SELL', reason: 'Zero cross down' };
      return { signal: 'NEUTRAL', reason: 'No crossover' };
    }

    if (nameUpper.includes('KST') && !nameUpper.includes('SIGNAL')) {
      if (prevValue < 0 && latestValue > 0) return { signal: 'BUY', reason: 'Zero cross up' };
      if (prevValue > 0 && latestValue < 0) return { signal: 'SELL', reason: 'Zero cross down' };
      return { signal: 'NEUTRAL', reason: 'No crossover' };
    }

    if (nameUpper.includes('VORTEX_POS')) {
      if (latestValue > 1) return { signal: 'BUY', reason: 'Positive >1' };
      return { signal: 'NEUTRAL', reason: 'Weak' };
    }

    if (nameUpper.includes('VORTEX_NEG')) {
      if (latestValue > 1) return { signal: 'SELL', reason: 'Negative >1' };
      return { signal: 'NEUTRAL', reason: 'Weak' };
    }

    // === VOLATILITY INDICATORS ===
    if (nameUpper.includes('BB_PBAND') || nameUpper.includes('KC_PBAND') || nameUpper.includes('DC_PBAND')) {
      if (latestValue < 0.2) return { signal: 'BUY', reason: 'Near lower band' };
      if (latestValue > 0.8) return { signal: 'SELL', reason: 'Near upper band' };
      return { signal: 'NEUTRAL', reason: 'Middle band' };
    }

    if (nameUpper.includes('BB_HBAND_INDICATOR')) {
      if (latestValue === 1) return { signal: 'SELL', reason: 'Above upper BB' };
      return { signal: 'NEUTRAL', reason: 'Below upper BB' };
    }

    if (nameUpper.includes('BB_LBAND_INDICATOR')) {
      if (latestValue === 1) return { signal: 'BUY', reason: 'Below lower BB' };
      return { signal: 'NEUTRAL', reason: 'Above lower BB' };
    }

    if (nameUpper.includes('ATR')) {
      if (latestValue > avg * 1.5) return { signal: 'NEUTRAL', reason: 'High volatility' };
      if (latestValue < avg * 0.5) return { signal: 'NEUTRAL', reason: 'Low volatility' };
      return { signal: 'NEUTRAL', reason: 'Normal volatility' };
    }

    if (nameUpper.includes('UI') || nameUpper.includes('ULCER')) {
      if (latestValue > avg * 1.5) return { signal: 'SELL', reason: 'High risk' };
      return { signal: 'NEUTRAL', reason: 'Normal risk' };
    }

    // === VOLUME INDICATORS ===
    if (nameUpper === 'MFI') {
      if (latestValue <= 20) return { signal: 'BUY', reason: 'Oversold <20' };
      if (latestValue >= 80) return { signal: 'SELL', reason: 'Overbought >80' };
      return { signal: 'NEUTRAL', reason: '20-80 range' };
    }

    if (nameUpper.includes('CMF')) {
      if (latestValue > 0.1) return { signal: 'BUY', reason: 'Positive flow' };
      if (latestValue < -0.1) return { signal: 'SELL', reason: 'Negative flow' };
      return { signal: 'NEUTRAL', reason: 'Neutral flow' };
    }

    if (nameUpper === 'OBV' || nameUpper.includes('ADI') || nameUpper.includes('VPT')) {
      if (latestValue > prevValue) return { signal: 'BUY', reason: 'Volume rising' };
      if (latestValue < prevValue) return { signal: 'SELL', reason: 'Volume falling' };
      return { signal: 'NEUTRAL', reason: 'Volume stable' };
    }

    if (nameUpper.includes('EOM')) {
      if (latestValue > 0) return { signal: 'BUY', reason: 'Ease of movement +' };
      if (latestValue < 0) return { signal: 'SELL', reason: 'Ease of movement -' };
      return { signal: 'NEUTRAL', reason: 'No movement' };
    }

    if (nameUpper.includes('FI') && !nameUpper.includes('MFI')) {
      if (latestValue > 0) return { signal: 'BUY', reason: 'Force positive' };
      if (latestValue < 0) return { signal: 'SELL', reason: 'Force negative' };
      return { signal: 'NEUTRAL', reason: 'No force' };
    }

    // === OTHERS (RETURNS) ===
    if (nameUpper.includes('RETURN')) {
      if (latestValue > 2) return { signal: 'BUY', reason: 'Positive return' };
      if (latestValue < -2) return { signal: 'SELL', reason: 'Negative return' };
      return { signal: 'NEUTRAL', reason: 'Flat return' };
    }

    // Default for moving averages and others
    if (nameUpper.includes('SMA') || nameUpper.includes('EMA') || nameUpper.includes('WMA')) {
      return { signal: 'NEUTRAL', reason: 'Compare to price' };
    }

    return { signal: 'NEUTRAL', reason: 'Monitor indicator' };
  };

  const { signal, reason } = getSignal();
  const signalColor = signal === 'BUY' ? COLORS.GREEN : signal === 'SELL' ? COLORS.RED : COLORS.GRAY;

  return (
    <div style={{
      backgroundColor: COLORS.PANEL_BG,
      border: `1px solid ${COLORS.BORDER}`,
      borderLeft: `3px solid ${color}`,
      borderRadius: '6px',
      padding: '12px',
      display: 'flex',
      flexDirection: 'column',
      gap: '8px',
      minHeight: showSettings ? '480px' : '320px',
      height: 'auto',
      boxShadow: '0 1px 3px rgba(0, 0, 0, 0.3)',
      transition: 'all 0.2s ease',
    }}
    onMouseEnter={(e) => {
      e.currentTarget.style.borderColor = color;
      e.currentTarget.style.boxShadow = `0 4px 12px rgba(0, 0, 0, 0.5)`;
    }}
    onMouseLeave={(e) => {
      e.currentTarget.style.borderColor = COLORS.BORDER;
      e.currentTarget.style.boxShadow = '0 1px 3px rgba(0, 0, 0, 0.3)';
    }}
    >
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <div style={{ color: color, fontSize: '10px', fontWeight: 'bold', marginBottom: '2px' }}>
            {category}
          </div>
          <div style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 'bold' }}>
            {name.toUpperCase().replace(/_/g, ' ')}
          </div>
        </div>
        <div style={{ textAlign: 'right' }}>
          <div style={{ color: COLORS.WHITE, fontSize: '16px', fontWeight: 'bold' }}>
            {formatValue(latestValue)}
          </div>
          <div style={{ color: COLORS.GRAY, fontSize: '9px' }}>
            Current
          </div>
        </div>
      </div>

      {/* Lightweight Chart - ONLY indicator line */}
      <div style={{ width: '100%', height: '80px', flex: '0 0 auto' }}>
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={chartData} margin={{ top: 5, right: 5, left: 5, bottom: 5 }}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
            <XAxis dataKey="index" hide />
            <YAxis domain={['auto', 'auto']} hide />
            <Tooltip
              contentStyle={{
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                borderRadius: '4px',
                fontSize: '10px',
              }}
              formatter={(value: any) => formatValue(value)}
            />
            <Line
              type="monotone"
              dataKey="value"
              stroke={color}
              strokeWidth={2}
              dot={false}
              connectNulls
              isAnimationActive={false}
            />
          </LineChart>
        </ResponsiveContainer>
      </div>

      {/* Signal Badge */}
      <div style={{
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center',
        gap: '8px',
        padding: '4px',
        backgroundColor: `${signalColor}15`,
        border: `1px solid ${signalColor}`,
        borderRadius: '4px',
      }}>
        <span style={{ color: signalColor, fontSize: '11px', fontWeight: 'bold' }}>
          {signal}
        </span>
        <span style={{ color: COLORS.GRAY, fontSize: '9px' }}>
          {reason}
        </span>
      </div>

      {/* Settings Toggle & Parameters */}
      <div style={{ flex: '0 0 auto' }}>
        <button
          onClick={() => setShowSettings(!showSettings)}
          style={{
            fontSize: '9px',
            fontWeight: '500',
            textAlign: 'center',
            padding: '4px 8px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            width: '100%',
            userSelect: 'none',
            backgroundColor: showSettings ? COLORS.DARK_BG : COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            borderRadius: '3px',
            color: COLORS.GRAY,
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.borderColor = COLORS.ORANGE;
            e.currentTarget.style.color = COLORS.ORANGE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.borderColor = COLORS.BORDER;
            e.currentTarget.style.color = COLORS.GRAY;
          }}
        >
          <span style={{ fontSize: '10px' }}>⚙️</span>
          <span>{showSettings ? 'Hide' : 'Adjust'}</span>
        </button>

        {showSettings && (
          <div style={{
            backgroundColor: COLORS.DARK_BG,
            border: `1px solid ${COLORS.BORDER}`,
            borderRadius: '4px',
            padding: '10px',
            marginTop: '8px',
            fontSize: '10px',
          }}>
            {/* Indicator Parameters Section */}
            {defaultParams && (
              <div style={{ marginBottom: defaultThresholds.upper !== 0 ? '12px' : '0' }}>
                <div style={{ color: COLORS.CYAN, fontWeight: 'bold', marginBottom: '8px', fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <span>📊</span>
                  <span>Indicator Parameters</span>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(100px, 1fr))', gap: '8px', marginBottom: '8px' }}>
                  {Object.keys(defaultParams).map((key) => (
                    <div key={key}>
                      <label style={{ color: COLORS.GRAY, display: 'block', marginBottom: '3px', fontSize: '9px', textTransform: 'capitalize' }}>
                        {key.replace(/_/g, ' ')}
                      </label>
                      <input
                        type="number"
                        step="1"
                        value={customParams?.[key] ?? defaultParams[key]}
                        onChange={(e) => setCustomParams({
                          ...defaultParams,
                          ...customParams,
                          [key]: parseInt(e.target.value) || 1
                        })}
                        style={{
                          width: '100%',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          borderRadius: '3px',
                          color: COLORS.WHITE,
                          padding: '4px 6px',
                          fontSize: '10px',
                        }}
                      />
                    </div>
                  ))}
                </div>

                {customParams && (
                  <button
                    onClick={() => {
                      // Call recompute with new parameters
                      if (onRecompute) {
                        onRecompute({ indicator: name, params: customParams });
                      }
                    }}
                    style={{
                      width: '100%',
                      backgroundColor: COLORS.CYAN,
                      border: 'none',
                      borderRadius: '3px',
                      color: COLORS.DARK_BG,
                      padding: '6px',
                      fontSize: '10px',
                      cursor: 'pointer',
                      fontWeight: 'bold',
                      marginBottom: '6px',
                    }}
                  >
                    🔄 Recompute with New Parameters
                  </button>
                )}
              </div>
            )}

            {/* Signal Thresholds Section */}
            {defaultThresholds.upper !== 0 && (
              <div>
                <div style={{ color: COLORS.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <span>🎯</span>
                  <span>{defaultThresholds.label}</span>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '8px' }}>
                  <div>
                    <label style={{ color: COLORS.GRAY, display: 'block', marginBottom: '3px', fontSize: '9px' }}>
                      Upper Threshold
                    </label>
                    <input
                      type="number"
                      step="0.1"
                      value={customThresholds?.upper ?? defaultThresholds.upper}
                      onChange={(e) => setCustomThresholds({
                        upper: parseFloat(e.target.value),
                        lower: customThresholds?.lower ?? defaultThresholds.lower
                      })}
                      style={{
                        width: '100%',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        borderRadius: '3px',
                        color: COLORS.WHITE,
                        padding: '4px 6px',
                        fontSize: '10px',
                      }}
                    />
                  </div>

                  <div>
                    <label style={{ color: COLORS.GRAY, display: 'block', marginBottom: '3px', fontSize: '9px' }}>
                      Lower Threshold
                    </label>
                    <input
                      type="number"
                      step="0.1"
                      value={customThresholds?.lower ?? defaultThresholds.lower}
                      onChange={(e) => setCustomThresholds({
                        upper: customThresholds?.upper ?? defaultThresholds.upper,
                        lower: parseFloat(e.target.value)
                      })}
                      style={{
                        width: '100%',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        borderRadius: '3px',
                        color: COLORS.WHITE,
                        padding: '4px 6px',
                        fontSize: '10px',
                      }}
                    />
                  </div>
                </div>
              </div>
            )}

            {/* Reset Button */}
            <button
              onClick={() => {
                setCustomThresholds(null);
                setCustomParams(null);
              }}
              style={{
                width: '100%',
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                borderRadius: '3px',
                color: COLORS.GRAY,
                padding: '5px',
                fontSize: '9px',
                cursor: 'pointer',
                fontWeight: '600',
              }}
            >
              Reset All to Defaults
            </button>
          </div>
        )}
      </div>

      {/* Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', fontSize: '9px' }}>
        <div>
          <div style={{ color: COLORS.GRAY }}>MIN</div>
          <div style={{ color: COLORS.RED, fontWeight: 'bold' }}>{formatValue(min)}</div>
        </div>
        <div>
          <div style={{ color: COLORS.GRAY }}>AVG</div>
          <div style={{ color: COLORS.YELLOW, fontWeight: 'bold' }}>{formatValue(avg)}</div>
        </div>
        <div>
          <div style={{ color: COLORS.GRAY }}>MAX</div>
          <div style={{ color: COLORS.GREEN, fontWeight: 'bold' }}>{formatValue(max)}</div>
        </div>
      </div>

      {/* Data Points Count */}
      <div style={{ color: COLORS.GRAY, fontSize: '9px', textAlign: 'center', borderTop: `1px solid ${COLORS.BORDER}`, paddingTop: '4px' }}>
        {values.length} data points
      </div>
    </div>
  );
};

const EquityResearchTab: React.FC = () => {
  const { colors, fontSize: themeFontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [searchSymbol, setSearchSymbol] = useState('');
  const [currentSymbol, setCurrentSymbol] = useState('AAPL');
  const [stockInfo, setStockInfo] = useState<StockInfo | null>(null);
  const [quoteData, setQuoteData] = useState<QuoteData | null>(null);
  const [historicalData, setHistoricalData] = useState<HistoricalData[]>([]);
  const [financials, setFinancials] = useState<FinancialsData | null>(null);
  const [loading, setLoading] = useState(false);
  const [chartPeriod, setChartPeriod] = useState<'1M' | '3M' | '6M' | '1Y' | '5Y'>('6M');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeTab, setActiveTab] = useState<'overview' | 'financials' | 'analysis' | 'technicals' | 'news'>('overview');
  const [fontSize, setFontSize] = useState(12);
  const [selectedMetrics, setSelectedMetrics] = useState<string[]>(['Total Revenue', 'Gross Profit', 'Operating Income', 'Net Income', 'EBITDA', 'Basic EPS', 'Diluted EPS']);
  const [showYearsCount, setShowYearsCount] = useState(4);

  // 3 Chart Metrics Selection
  const [chart1Metrics, setChart1Metrics] = useState<string[]>(['Total Revenue', 'Gross Profit']);
  const [chart2Metrics, setChart2Metrics] = useState<string[]>(['Net Income', 'Operating Income']);
  const [chart3Metrics, setChart3Metrics] = useState<string[]>(['EBITDA']);

  // Technicals state
  const [technicalsData, setTechnicalsData] = useState<any>(null);
  const [technicalsLoading, setTechnicalsLoading] = useState(false);

  // News state
  const [newsData, setNewsData] = useState<any>(null);
  const [newsLoading, setNewsLoading] = useState(false);

  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<any>(null);
  const candlestickSeriesRef = useRef<any>(null);
  const volumeSeriesRef = useRef<any>(null);

  const financialChart1Ref = useRef<HTMLDivElement>(null);
  const financialChart2Ref = useRef<HTMLDivElement>(null);
  const financialChart3Ref = useRef<HTMLDivElement>(null);
  const financialChart1InstanceRef = useRef<any>(null);
  const financialChart2InstanceRef = useRef<any>(null);
  const financialChart3InstanceRef = useRef<any>(null);

  // Data Cache: { symbol: { info, quote, historical: { period: data }, financials, timestamp } }
  const dataCache = useRef<Record<string, {
    stockInfo: StockInfo;
    quoteData: QuoteData;
    historicalData: Record<string, HistoricalData[]>;
    financials: FinancialsData;
    timestamp: number;
  }>>({});

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const fetchStockData = async (symbol: string, forceRefresh = false) => {
    setLoading(true);
    console.log('Fetching data for:', symbol, '| Force refresh:', forceRefresh);

    try {
      // Check cache first
      const cached = dataCache.current[symbol];
      const now = Date.now();
      const CACHE_DURATION = 5 * 60 * 1000; // 5 minutes

      if (cached && !forceRefresh && (now - cached.timestamp < CACHE_DURATION)) {
        console.log('Using cached data for', symbol);
        setStockInfo(cached.stockInfo);
        setQuoteData(cached.quoteData);
        setFinancials(cached.financials);

        // Check if we have historical data for this period
        if (cached.historicalData[chartPeriod]) {
          setHistoricalData(cached.historicalData[chartPeriod]);
          setLoading(false);
          return;
        }
      }

      // Fetch fresh data
      console.log('Fetching fresh data for', symbol);

      // Fetch quote
      const quoteResponse: any = await invoke('get_market_quote', { symbol });
      let newQuote: QuoteData | null = null;
      if (quoteResponse.success && quoteResponse.data) {
        setQuoteData(quoteResponse.data);
        newQuote = quoteResponse.data;
      }

      // Fetch stock info
      const infoResponse: any = await invoke('get_stock_info', { symbol });
      let newInfo: StockInfo | null = null;
      if (infoResponse.success && infoResponse.data) {
        setStockInfo(infoResponse.data);
        newInfo = infoResponse.data;
      }

      // Fetch historical data
      const endDate = new Date();
      const startDate = new Date();
      switch (chartPeriod) {
        case '1M': startDate.setMonth(endDate.getMonth() - 1); break;
        case '3M': startDate.setMonth(endDate.getMonth() - 3); break;
        case '6M': startDate.setMonth(endDate.getMonth() - 6); break;
        case '1Y': startDate.setFullYear(endDate.getFullYear() - 1); break;
        case '5Y': startDate.setFullYear(endDate.getFullYear() - 5); break;
      }

      const historicalResponse: any = await invoke('get_historical_data', {
        symbol,
        startDate: startDate.toISOString().split('T')[0],
        endDate: endDate.toISOString().split('T')[0],
      });
      let newHistorical: HistoricalData[] = [];
      if (historicalResponse.success && historicalResponse.data) {
        setHistoricalData(historicalResponse.data);
        newHistorical = historicalResponse.data;
      }

      // Fetch financials
      const financialsResponse: any = await invoke('get_financials', { symbol });
      let newFinancials: FinancialsData | null = null;
      if (financialsResponse.success && financialsResponse.data) {
        setFinancials(financialsResponse.data);
        newFinancials = financialsResponse.data;
      }

      // Update cache
      if (newInfo && newQuote && newFinancials) {
        dataCache.current[symbol] = {
          stockInfo: newInfo,
          quoteData: newQuote,
          historicalData: {
            ...((cached?.historicalData) || {}),
            [chartPeriod]: newHistorical,
          },
          financials: newFinancials,
          timestamp: now,
        };
      }
    } catch (error) {
      console.error('Error fetching stock data:', error);
    } finally {
      setLoading(false);
    }
  };

  // Fetch company news
  const fetchNews = async () => {
    if (!currentSymbol) {
      console.log('No symbol selected for news fetch');
      return;
    }

    setNewsLoading(true);
    console.log('Fetching news for:', currentSymbol);

    try {
      // Use company name if available, otherwise use symbol
      const query = stockInfo?.company_name || currentSymbol;

      const response: any = await invoke('fetch_company_news', {
        query: query,
        maxResults: 20,
        period: '7d',
        language: 'en',
        country: 'US'
      });

      console.log('News response:', response);

      if (typeof response === 'string') {
        const parsed = JSON.parse(response);
        setNewsData(parsed);
      } else {
        setNewsData(response);
      }
    } catch (error) {
      console.error('Error fetching news:', error);
      setNewsData({
        success: false,
        error: String(error),
        data: []
      });
    } finally {
      setNewsLoading(false);
    }
  };

  // Initialize price chart with volume
  useEffect(() => {
    if (chartContainerRef.current && !chartRef.current) {
      const containerHeight = chartContainerRef.current.clientHeight;
      const containerWidth = chartContainerRef.current.clientWidth;

      const chart = createChart(chartContainerRef.current, {
        layout: {
          background: { color: COLORS.DARK_BG },
          textColor: COLORS.WHITE,
        },
        grid: {
          vertLines: { color: COLORS.BORDER },
          horzLines: { color: COLORS.BORDER },
        },
        width: containerWidth,
        height: containerHeight > 0 ? containerHeight : 300,
        timeScale: {
          borderColor: COLORS.GRAY,
          timeVisible: true,
        },
        rightPriceScale: {
          borderColor: COLORS.GRAY,
        },
      });

      const candlestickSeries = chart.addSeries(CandlestickSeries, {
        upColor: COLORS.GREEN,
        downColor: COLORS.RED,
        borderVisible: false,
        wickUpColor: COLORS.GREEN,
        wickDownColor: COLORS.RED,
      });

      const volumeSeries = chart.addSeries(HistogramSeries, {
        color: COLORS.BLUE,
        priceFormat: {
          type: 'volume',
        },
        priceScaleId: '',
      });

      volumeSeries.priceScale().applyOptions({
        scaleMargins: {
          top: 0.8,
          bottom: 0,
        },
      });

      chartRef.current = chart;
      candlestickSeriesRef.current = candlestickSeries;
      volumeSeriesRef.current = volumeSeries;

      const handleResize = () => {
        if (chartContainerRef.current && chartRef.current) {
          const newHeight = chartContainerRef.current.clientHeight;
          const newWidth = chartContainerRef.current.clientWidth;
          chartRef.current.applyOptions({
            width: newWidth,
            height: newHeight > 0 ? newHeight : 300,
          });
        }
      };

      // Use ResizeObserver for better resize detection
      const resizeObserver = new ResizeObserver(handleResize);
      if (chartContainerRef.current) {
        resizeObserver.observe(chartContainerRef.current);
      }

      window.addEventListener('resize', handleResize);
      return () => {
        window.removeEventListener('resize', handleResize);
        resizeObserver.disconnect();
        chart.remove();
        chartRef.current = null;
        candlestickSeriesRef.current = null;
        volumeSeriesRef.current = null;
      };
    }
  }, []);

  // Update price chart data
  useEffect(() => {
    if (candlestickSeriesRef.current && volumeSeriesRef.current && historicalData.length > 0) {
      const chartData = historicalData.map((d) => ({
        time: Math.floor(d.timestamp) as any,
        open: d.open,
        high: d.high,
        low: d.low,
        close: d.close,
      })).sort((a, b) => a.time - b.time);

      const volumeData = historicalData.map((d) => ({
        time: Math.floor(d.timestamp) as any,
        value: d.volume,
        color: d.close >= d.open ? COLORS.GREEN + '80' : COLORS.RED + '80',
      })).sort((a, b) => a.time - b.time);

      candlestickSeriesRef.current.setData(chartData);
      volumeSeriesRef.current.setData(volumeData);

      if (chartRef.current) {
        chartRef.current.timeScale().fitContent();
      }
    }
  }, [historicalData]);

  // Helper function to create a financial chart with dynamic metrics
  const createFinancialChart = (
    container: HTMLDivElement | null,
    metrics: string[],
    colors: string[]
  ) => {
    if (!container || !financials) return null;

    const chart = createChart(container, {
      layout: {
        background: { color: COLORS.DARK_BG },
        textColor: COLORS.WHITE,
      },
      grid: {
        vertLines: { color: COLORS.BORDER },
        horzLines: { color: COLORS.BORDER },
      },
      width: container.clientWidth,
      height: 250,
      timeScale: {
        borderColor: COLORS.GRAY,
        timeVisible: true,
      },
      rightPriceScale: {
        borderColor: COLORS.GRAY,
      },
    });

    const periods = Object.keys(financials.income_statement);
    const seriesArray: any[] = [];

    metrics.forEach((metric, idx) => {
      const color = colors[idx % colors.length];
      const series = chart.addSeries(LineSeries, {
        color,
        lineWidth: 2,
        title: metric,
      });

      const data: any[] = [];
      periods.forEach((period) => {
        const timestamp = new Date(period).getTime() / 1000;
        const statements = financials.income_statement[period];

        if (statements[metric] !== undefined && statements[metric] !== null) {
          const value = metric.includes('EPS')
            ? statements[metric]
            : statements[metric] / 1e9; // Convert to billions
          data.push({ time: timestamp, value });
        }
      });

      if (data.length > 0) {
        series.setData(data.sort((a, b) => a.time - b.time));
      }
      seriesArray.push(series);
    });

    chart.timeScale().fitContent();
    return chart;
  };

  // Update financial charts when data or metrics change
  useEffect(() => {
    if (activeTab === 'financials' && financials) {
      // Cleanup existing charts
      if (financialChart1InstanceRef.current) {
        financialChart1InstanceRef.current.remove();
        financialChart1InstanceRef.current = null;
      }
      if (financialChart2InstanceRef.current) {
        financialChart2InstanceRef.current.remove();
        financialChart2InstanceRef.current = null;
      }
      if (financialChart3InstanceRef.current) {
        financialChart3InstanceRef.current.remove();
        financialChart3InstanceRef.current = null;
      }

      // Create new charts
      const chartColors = [COLORS.GREEN, COLORS.CYAN, COLORS.ORANGE, COLORS.YELLOW, COLORS.MAGENTA, COLORS.BLUE];

      if (financialChart1Ref.current && chart1Metrics.length > 0) {
        financialChart1InstanceRef.current = createFinancialChart(
          financialChart1Ref.current,
          chart1Metrics,
          chartColors
        );
      }

      if (financialChart2Ref.current && chart2Metrics.length > 0) {
        financialChart2InstanceRef.current = createFinancialChart(
          financialChart2Ref.current,
          chart2Metrics,
          chartColors
        );
      }

      if (financialChart3Ref.current && chart3Metrics.length > 0) {
        financialChart3InstanceRef.current = createFinancialChart(
          financialChart3Ref.current,
          chart3Metrics,
          chartColors
        );
      }

      // Handle window resize
      const handleResize = () => {
        if (financialChart1Ref.current && financialChart1InstanceRef.current) {
          financialChart1InstanceRef.current.applyOptions({ width: financialChart1Ref.current.clientWidth });
        }
        if (financialChart2Ref.current && financialChart2InstanceRef.current) {
          financialChart2InstanceRef.current.applyOptions({ width: financialChart2Ref.current.clientWidth });
        }
        if (financialChart3Ref.current && financialChart3InstanceRef.current) {
          financialChart3InstanceRef.current.applyOptions({ width: financialChart3Ref.current.clientWidth });
        }
      };

      window.addEventListener('resize', handleResize);
      return () => {
        window.removeEventListener('resize', handleResize);
        if (financialChart1InstanceRef.current) financialChart1InstanceRef.current.remove();
        if (financialChart2InstanceRef.current) financialChart2InstanceRef.current.remove();
        if (financialChart3InstanceRef.current) financialChart3InstanceRef.current.remove();
      };
    }
  }, [financials, activeTab, chart1Metrics, chart2Metrics, chart3Metrics]);

  // Compute technicals from historical data
  const computeTechnicals = async (historical: HistoricalData[]) => {
    setTechnicalsLoading(true);
    try {
      // Validate historical data
      if (!historical || !Array.isArray(historical) || historical.length === 0) {
        console.error('Invalid historical data for technicals computation');
        setTechnicalsData(null);
        return;
      }

      // Convert historical data to format expected by Python script
      const historicalJson = JSON.stringify(historical.map(d => ({
        timestamp: d.timestamp,
        open: d.open,
        high: d.high,
        low: d.low,
        close: d.close,
        volume: d.volume
      })));

      console.log('Sending historical data for technicals computation:', historical.length, 'data points');

      const response: any = await invoke('compute_all_technicals', {
        historicalData: historicalJson
      });

      console.log('Technicals response:', response);
      console.log('Response type:', typeof response);
      console.log('Response keys:', response ? Object.keys(response) : 'null');

      if (typeof response === 'string') {
        try {
          const parsed = JSON.parse(response);
          if (parsed.success && parsed.data && Array.isArray(parsed.data) && parsed.data.length > 0) {
            setTechnicalsData(parsed);
          } else {
            console.error('Error computing technicals:', parsed.error || 'Invalid data structure');
            setTechnicalsData(null);
          }
        } catch (parseError) {
          console.error('Error parsing technicals response:', parseError);
          setTechnicalsData(null);
        }
      } else if (response && response.success && response.data && Array.isArray(response.data) && response.data.length > 0) {
        setTechnicalsData(response);
      } else {
        console.error('Invalid technicals response structure');
        setTechnicalsData(null);
      }
    } catch (error) {
      console.error('Error computing technicals:', error);
      setTechnicalsData(null);
    } finally {
      setTechnicalsLoading(false);
    }
  };

  // Load initial data
  useEffect(() => {
    fetchStockData(currentSymbol);
  }, [currentSymbol, chartPeriod]);

  // Compute technicals when historical data changes
  useEffect(() => {
    if (historicalData.length > 0) {
      computeTechnicals(historicalData);
    }
  }, [historicalData]);

  // Fetch news when news tab is activated or symbol changes
  useEffect(() => {
    if (activeTab === 'news' && currentSymbol) {
      fetchNews();
    }
  }, [activeTab, currentSymbol]);

  const handleSearch = () => {
    if (searchSymbol.trim()) {
      setCurrentSymbol(searchSymbol.trim().toUpperCase());
      setSearchSymbol('');
    }
  };

  const formatNumber = (num: number | null | undefined, decimals = 2) => {
    if (num === null || num === undefined) return 'N/A';
    return num.toLocaleString(undefined, { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
  };

  const formatLargeNumber = (num: number | null | undefined) => {
    if (num === null || num === undefined) return 'N/A';
    if (num >= 1e12) return `$${(num / 1e12).toFixed(2)}T`;
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(2)}M`;
    return `$${num.toFixed(2)}`;
  };

  const formatPercent = (num: number | null | undefined) => {
    if (num === null || num === undefined) return 'N/A';
    return `${(num * 100).toFixed(2)}%`;
  };

  const currentPrice = quoteData?.price || stockInfo?.current_price || 0;
  const priceChange = quoteData?.change || 0;
  const priceChangePercent = quoteData?.change_percent || 0;

  const getRecommendationColor = (key: string | null) => {
    if (!key) return COLORS.GRAY;
    if (key === 'buy' || key === 'strong_buy') return COLORS.GREEN;
    if (key === 'sell' || key === 'strong_sell') return COLORS.RED;
    return COLORS.YELLOW;
  };

  const getRecommendationText = (key: string | null) => {
    if (!key) return 'N/A';
    return key.toUpperCase().replace('_', ' ');
  };

  // Helper function to get year from period string
  const getYearFromPeriod = (period: string) => {
    return new Date(period).getFullYear();
  };

  // Get all available metrics from income statement
  const getAllMetrics = () => {
    if (!financials || Object.keys(financials.income_statement).length === 0) return [];
    const firstPeriod = Object.keys(financials.income_statement)[0];
    return Object.keys(financials.income_statement[firstPeriod]);
  };

  const toggleMetric = (metric: string) => {
    setSelectedMetrics(prev =>
      prev.includes(metric)
        ? prev.filter(m => m !== metric)
        : [...prev, metric]
    );
  };

  return (
    <div style={{
      height: '100%',
      width: '100%',
      backgroundColor: COLORS.DARK_BG,
      color: COLORS.WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.BORDER}`,
        padding: '6px 12px',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '13px' }}>EQUITY RESEARCH TERMINAL</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <input
                type="text"
                value={searchSymbol}
                onChange={(e) => setSearchSymbol(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
                placeholder="Enter symbol..."
                style={{
                  backgroundColor: COLORS.DARK_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  padding: '3px 8px',
                  fontSize: '11px',
                  width: '120px',
                  fontFamily: 'Consolas, monospace',
                }}
              />
              <button
                onClick={handleSearch}
                style={{
                  backgroundColor: COLORS.ORANGE,
                  border: 'none',
                  color: COLORS.DARK_BG,
                  padding: '3px 10px',
                  fontSize: '10px',
                  cursor: 'pointer',
                  fontWeight: 'bold',
                }}
              >
                SEARCH
              </button>
            </div>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '10px' }}>
            <span style={{ color: COLORS.GRAY }}>{currentTime.toLocaleTimeString()}</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{currentSymbol}</span>
            {loading && <span style={{ color: COLORS.YELLOW }}>● LOADING...</span>}
            {!loading && <span style={{ color: COLORS.GREEN }}>● LIVE</span>}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        scrollbarColor: 'rgba(120, 120, 120, 0.3) transparent',
        scrollbarWidth: 'thin',
        minHeight: 0,
        backgroundColor: COLORS.DARK_BG,
      }} className="custom-scrollbar">

        {/* Stock Header with Price */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          padding: '12px',
          margin: '8px',
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '4px' }}>
                <span style={{ color: COLORS.ORANGE, fontSize: '20px', fontWeight: 'bold' }}>{currentSymbol}</span>
                <span style={{ color: COLORS.WHITE, fontSize: '14px' }}>{stockInfo?.company_name || 'Loading...'}</span>
                {stockInfo?.sector && (
                  <span style={{
                    color: COLORS.BLUE,
                    fontSize: '10px',
                    padding: '2px 6px',
                    backgroundColor: 'rgba(100,150,250,0.2)',
                    border: `1px solid ${COLORS.BLUE}`,
                  }}>
                    {stockInfo.sector}
                  </span>
                )}
                {stockInfo?.recommendation_key && (
                  <span style={{
                    color: getRecommendationColor(stockInfo.recommendation_key),
                    fontSize: '10px',
                    padding: '2px 6px',
                    backgroundColor: `${getRecommendationColor(stockInfo.recommendation_key)}20`,
                    border: `1px solid ${getRecommendationColor(stockInfo.recommendation_key)}`,
                    fontWeight: 'bold',
                  }}>
                    {getRecommendationText(stockInfo.recommendation_key)}
                  </span>
                )}
              </div>
              <div style={{ fontSize: '10px', color: COLORS.GRAY }}>
                {stockInfo?.exchange || 'N/A'} | {stockInfo?.industry || 'N/A'} | {stockInfo?.country || 'N/A'}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: '24px', fontWeight: 'bold', color: COLORS.WHITE, marginBottom: '2px' }}>
                ${formatNumber(currentPrice)}
              </div>
              <div style={{ fontSize: '12px', color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED }}>
                {priceChange >= 0 ? '▲' : '▼'} ${Math.abs(priceChange).toFixed(2)} ({priceChangePercent.toFixed(2)}%)
              </div>
            </div>
          </div>
        </div>

        {/* Tab Navigation */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`,
          margin: '0 8px 8px 8px',
          display: 'flex',
          gap: '1px',
        }}>
          {(['overview', 'financials', 'analysis', 'technicals', 'news'] as const).map((tab) => (
            <button
              key={tab}
              onClick={() => setActiveTab(tab)}
              style={{
                flex: 1,
                backgroundColor: activeTab === tab ? COLORS.ORANGE : COLORS.DARK_BG,
                border: 'none',
                color: activeTab === tab ? COLORS.DARK_BG : COLORS.WHITE,
                padding: '8px 16px',
                fontSize: '11px',
                cursor: 'pointer',
                fontWeight: activeTab === tab ? 'bold' : 'normal',
                textTransform: 'uppercase',
              }}
            >
              {tab}
            </button>
          ))}
        </div>

        {/* Overview Tab */}
        {activeTab === 'overview' && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '6px',
            padding: '8px',
            height: '100%',
            boxSizing: 'border-box',
          }}>
            {/* Top Data Grid - 4 Columns */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(4, 1fr)',
              gap: '6px',
              flex: '0 0 auto',
              minHeight: '400px',
            }}>
              {/* Column 1 - Trading & Valuation */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {/* Today's Trading */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    TODAY'S TRADING
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>OPEN:</span>
                      <span style={{ color: COLORS.CYAN }}>${formatNumber(quoteData?.open)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                      <span style={{ color: COLORS.GREEN }}>${formatNumber(quoteData?.high)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>LOW:</span>
                      <span style={{ color: COLORS.RED }}>${formatNumber(quoteData?.low)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>PREV CLOSE:</span>
                      <span style={{ color: COLORS.WHITE }}>${formatNumber(quoteData?.previous_close)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>VOLUME:</span>
                      <span style={{ color: COLORS.YELLOW }}>{formatNumber(quoteData?.volume || 0, 0)}</span>
                    </div>
                  </div>
                </div>

                {/* Valuation Metrics */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.CYAN, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    VALUATION
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>MARKET CAP:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.market_cap)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>P/E RATIO:</span>
                      <span style={{ color: COLORS.YELLOW }}>{formatNumber(stockInfo?.pe_ratio)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>FWD P/E:</span>
                      <span style={{ color: COLORS.YELLOW }}>{formatNumber(stockInfo?.forward_pe)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>PEG RATIO:</span>
                      <span style={{ color: COLORS.YELLOW }}>{formatNumber(stockInfo?.peg_ratio)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>P/B RATIO:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo?.price_to_book)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>DIV YIELD:</span>
                      <span style={{ color: COLORS.GREEN }}>
                        {formatPercent(stockInfo?.dividend_yield)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>BETA:</span>
                      <span style={{ color: COLORS.WHITE }}>{formatNumber(stockInfo?.beta)}</span>
                    </div>
                  </div>
                </div>

                {/* Share Statistics */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.CYAN, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    SHARE STATS
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>SHARES OUT:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.shares_outstanding)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>FLOAT:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.float_shares)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>INSIDERS:</span>
                      <span style={{ color: COLORS.YELLOW }}>{formatPercent(stockInfo?.held_percent_insiders)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>INSTITUTIONS:</span>
                      <span style={{ color: COLORS.YELLOW }}>{formatPercent(stockInfo?.held_percent_institutions)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>SHORT %:</span>
                      <span style={{ color: COLORS.RED }}>{formatPercent(stockInfo?.short_percent_of_float)}</span>
                    </div>
                  </div>
                </div>
              </div>

              {/* Column 2 - Price Chart */}
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                gap: '6px',
                gridColumn: 'span 2',
                overflow: 'hidden',
                minHeight: 0,
              }}>
                {/* Chart Period Selector */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '4px 6px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  flexShrink: 0,
                }}>
                  <span style={{ color: COLORS.GRAY, fontSize: '9px' }}>PERIOD:</span>
                  {(['1M', '3M', '6M', '1Y', '5Y'] as const).map((period) => (
                    <button
                      key={period}
                      onClick={() => setChartPeriod(period)}
                      style={{
                        backgroundColor: chartPeriod === period ? COLORS.ORANGE : COLORS.DARK_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: chartPeriod === period ? COLORS.DARK_BG : COLORS.WHITE,
                        padding: '3px 8px',
                        fontSize: '8px',
                        cursor: 'pointer',
                        fontWeight: chartPeriod === period ? 'bold' : 'normal',
                      }}
                    >
                      {period}
                    </button>
                  ))}
                </div>

                {/* Chart with Volume */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                  overflow: 'hidden',
                  flex: 1,
                  display: 'flex',
                  flexDirection: 'column',
                  minHeight: 0,
                }}>
                  <div style={{ color: COLORS.ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', flexShrink: 0 }}>
                    PRICE CHART & VOLUME - {chartPeriod}
                  </div>
                  <div ref={chartContainerRef} style={{
                    backgroundColor: COLORS.DARK_BG,
                    flex: 1,
                    width: '100%',
                    overflow: 'hidden',
                    minHeight: '280px',
                  }} />
                </div>
              </div>

              {/* Column 3 - Analyst & Performance */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {/* Analyst Targets */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.MAGENTA, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    ANALYST TARGETS
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                      <span style={{ color: COLORS.GREEN }}>${formatNumber(stockInfo?.target_high_price)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>MEAN:</span>
                      <span style={{ color: COLORS.YELLOW }}>${formatNumber(stockInfo?.target_mean_price)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>LOW:</span>
                      <span style={{ color: COLORS.RED }}>${formatNumber(stockInfo?.target_low_price)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>ANALYSTS:</span>
                      <span style={{ color: COLORS.CYAN }}>{stockInfo?.number_of_analyst_opinions || 'N/A'}</span>
                    </div>
                  </div>
                </div>

                {/* 52 Week Range */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    52 WEEK RANGE
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>HIGH:</span>
                      <span style={{ color: COLORS.GREEN }}>${formatNumber(stockInfo?.fifty_two_week_high)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>LOW:</span>
                      <span style={{ color: COLORS.RED }}>${formatNumber(stockInfo?.fifty_two_week_low)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>AVG VOL:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo?.average_volume, 0)}</span>
                    </div>
                  </div>
                </div>

                {/* Profitability */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.GREEN, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    PROFITABILITY
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>GROSS MARGIN:</span>
                      <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.gross_margins)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>OPER. MARGIN:</span>
                      <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.operating_margins)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>PROFIT MARGIN:</span>
                      <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.profit_margins)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>ROA:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatPercent(stockInfo?.return_on_assets)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>ROE:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatPercent(stockInfo?.return_on_equity)}</span>
                    </div>
                  </div>
                </div>

                {/* Growth */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.BLUE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    GROWTH RATES
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>REVENUE:</span>
                      <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.revenue_growth)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>EARNINGS:</span>
                      <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.earnings_growth)}</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* Bottom Section - Company Overview & Key Info */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '2fr 1fr',
              gap: '6px',
              flex: '0 0 auto',
              minHeight: '120px',
            }}>
              {/* Company Description */}
              {stockInfo?.description && stockInfo.description !== 'N/A' && (
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                  display: 'flex',
                  flexDirection: 'column',
                  minHeight: 0,
                }}>
                  <div style={{ color: COLORS.CYAN, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px', flexShrink: 0 }}>
                    COMPANY OVERVIEW
                  </div>
                  <div style={{
                    color: COLORS.WHITE,
                    fontSize: '9px',
                    lineHeight: '1.5',
                    overflow: 'auto',
                    flex: 1,
                    minHeight: 0,
                  }} className="custom-scrollbar">
                    {stockInfo.description}
                  </div>
                </div>
              )}

              {/* Company Info & Financial Health */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {/* Company Info */}
                {stockInfo && (
                  <div style={{
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    padding: '6px',
                  }}>
                    <div style={{ color: COLORS.WHITE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                      COMPANY INFO
                    </div>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '3px', fontSize: '9px' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                        <span style={{ color: COLORS.GRAY }}>EMPLOYEES:</span>
                        <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo.employees, 0)}</span>
                      </div>
                      {stockInfo.website && stockInfo.website !== 'N/A' && (
                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                          <span style={{ color: COLORS.GRAY }}>WEBSITE:</span>
                          <span style={{ color: COLORS.BLUE, fontSize: '8px' }}>{stockInfo.website.substring(0, 25)}</span>
                        </div>
                      )}
                      <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                        <span style={{ color: COLORS.GRAY }}>CURRENCY:</span>
                        <span style={{ color: COLORS.CYAN }}>{stockInfo.currency || 'N/A'}</span>
                      </div>
                    </div>
                  </div>
                )}

                {/* Key Financial Health */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '6px',
                }}>
                  <div style={{ color: COLORS.ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                    FINANCIAL HEALTH
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '3px', fontSize: '9px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>CASH:</span>
                      <span style={{ color: COLORS.GREEN }}>{formatLargeNumber(stockInfo?.total_cash)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>DEBT:</span>
                      <span style={{ color: COLORS.RED }}>{formatLargeNumber(stockInfo?.total_debt)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: COLORS.GRAY }}>FREE CF:</span>
                      <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.free_cashflow)}</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Financials Tab */}
        {activeTab === 'financials' && (
          <div style={{ padding: '8px' }}>
            {!financials ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '400px',
                gap: '16px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
                  {loading ? '⏳ LOADING FINANCIAL DATA...' : '⚠️ NO FINANCIAL DATA AVAILABLE'}
                </div>
                <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                  {loading ? 'Please wait while we fetch the data' : 'Financial statements are not available for this symbol'}
                </div>
              </div>
            ) : (
          <div style={{ padding: '0' }}>
            {/* Controls Panel */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '10px 12px',
              marginBottom: '8px',
              display: 'flex',
              gap: '20px',
              alignItems: 'center',
              flexWrap: 'wrap',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>FONT SIZE:</span>
                <button
                  onClick={() => setFontSize(Math.max(10, fontSize - 1))}
                  style={{
                    backgroundColor: COLORS.DARK_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    padding: '4px 8px',
                    fontSize: `${fontSize}px`,
                    cursor: 'pointer',
                  }}
                >
                  -
                </button>
                <span style={{ color: COLORS.ORANGE, fontSize: `${fontSize}px`, fontWeight: 'bold', minWidth: '30px', textAlign: 'center' }}>{fontSize}px</span>
                <button
                  onClick={() => setFontSize(Math.min(20, fontSize + 1))}
                  style={{
                    backgroundColor: COLORS.DARK_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    padding: '4px 8px',
                    fontSize: `${fontSize}px`,
                    cursor: 'pointer',
                  }}
                >
                  +
                </button>
              </div>

              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>SHOW YEARS:</span>
                {[2, 3, 4, 5].map((count) => (
                  <button
                    key={count}
                    onClick={() => setShowYearsCount(count)}
                    style={{
                      backgroundColor: showYearsCount === count ? COLORS.ORANGE : COLORS.DARK_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      color: showYearsCount === count ? COLORS.DARK_BG : COLORS.WHITE,
                      padding: '4px 10px',
                      fontSize: `${fontSize - 1}px`,
                      cursor: 'pointer',
                      fontWeight: showYearsCount === count ? 'bold' : 'normal',
                    }}
                  >
                    {count}
                  </button>
                ))}
              </div>

              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ color: COLORS.GRAY, fontSize: `${fontSize}px` }}>SELECT METRICS:</span>
                <button
                  onClick={() => {
                    const allMetrics = getAllMetrics();
                    setSelectedMetrics(selectedMetrics.length === allMetrics.length ? [] : allMetrics);
                  }}
                  style={{
                    backgroundColor: COLORS.BLUE,
                    border: 'none',
                    color: COLORS.WHITE,
                    padding: '4px 10px',
                    fontSize: `${fontSize - 1}px`,
                    cursor: 'pointer',
                    fontWeight: 'bold',
                  }}
                >
                  {selectedMetrics.length === getAllMetrics().length ? 'DESELECT ALL' : 'SELECT ALL'}
                </button>
              </div>
            </div>

            {/* Metric Selector */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              marginBottom: '8px',
            }}>
              <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '10px' }}>
                SELECT METRICS TO COMPARE
              </div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                {getAllMetrics().map((metric) => (
                  <button
                    key={metric}
                    onClick={() => toggleMetric(metric)}
                    style={{
                      backgroundColor: selectedMetrics.includes(metric) ? COLORS.GREEN : COLORS.DARK_BG,
                      border: `1px solid ${selectedMetrics.includes(metric) ? COLORS.GREEN : COLORS.BORDER}`,
                      color: selectedMetrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                      padding: '6px 12px',
                      fontSize: `${fontSize - 1}px`,
                      cursor: 'pointer',
                      fontWeight: selectedMetrics.includes(metric) ? 'bold' : 'normal',
                      borderRadius: '3px',
                    }}
                  >
                    {metric}
                  </button>
                ))}
              </div>
            </div>

            {/* Financial Comparison Charts - 3 Charts in a Row */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              marginBottom: '8px',
            }}>
              <div style={{ color: COLORS.ORANGE, fontSize: `${fontSize + 3}px`, fontWeight: 'bold', marginBottom: '12px' }}>
                FINANCIAL TRENDS COMPARISON (in Billions $)
              </div>

              {/* Chart Controls */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', marginBottom: '12px' }}>
                {/* Chart 1 Selector */}
                <div>
                  <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                    CHART 1 METRICS:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {getAllMetrics().slice(0, 8).map((metric) => (
                      <button
                        key={metric}
                        onClick={() => {
                          setChart1Metrics(prev =>
                            prev.includes(metric)
                              ? prev.filter(m => m !== metric)
                              : [...prev, metric]
                          );
                        }}
                        style={{
                          backgroundColor: chart1Metrics.includes(metric) ? COLORS.GREEN : COLORS.DARK_BG,
                          border: `1px solid ${chart1Metrics.includes(metric) ? COLORS.GREEN : COLORS.BORDER}`,
                          color: chart1Metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                          padding: '3px 6px',
                          fontSize: `${fontSize - 3}px`,
                          cursor: 'pointer',
                          fontWeight: chart1Metrics.includes(metric) ? 'bold' : 'normal',
                        }}
                      >
                        {metric.substring(0, 15)}
                      </button>
                    ))}
                  </div>
                </div>

                {/* Chart 2 Selector */}
                <div>
                  <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                    CHART 2 METRICS:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {getAllMetrics().slice(0, 8).map((metric) => (
                      <button
                        key={metric}
                        onClick={() => {
                          setChart2Metrics(prev =>
                            prev.includes(metric)
                              ? prev.filter(m => m !== metric)
                              : [...prev, metric]
                          );
                        }}
                        style={{
                          backgroundColor: chart2Metrics.includes(metric) ? COLORS.ORANGE : COLORS.DARK_BG,
                          border: `1px solid ${chart2Metrics.includes(metric) ? COLORS.ORANGE : COLORS.BORDER}`,
                          color: chart2Metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                          padding: '3px 6px',
                          fontSize: `${fontSize - 3}px`,
                          cursor: 'pointer',
                          fontWeight: chart2Metrics.includes(metric) ? 'bold' : 'normal',
                        }}
                      >
                        {metric.substring(0, 15)}
                      </button>
                    ))}
                  </div>
                </div>

                {/* Chart 3 Selector */}
                <div>
                  <div style={{ color: COLORS.CYAN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px' }}>
                    CHART 3 METRICS:
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {getAllMetrics().slice(0, 8).map((metric) => (
                      <button
                        key={metric}
                        onClick={() => {
                          setChart3Metrics(prev =>
                            prev.includes(metric)
                              ? prev.filter(m => m !== metric)
                              : [...prev, metric]
                          );
                        }}
                        style={{
                          backgroundColor: chart3Metrics.includes(metric) ? COLORS.MAGENTA : COLORS.DARK_BG,
                          border: `1px solid ${chart3Metrics.includes(metric) ? COLORS.MAGENTA : COLORS.BORDER}`,
                          color: chart3Metrics.includes(metric) ? COLORS.DARK_BG : COLORS.WHITE,
                          padding: '3px 6px',
                          fontSize: `${fontSize - 3}px`,
                          cursor: 'pointer',
                          fontWeight: chart3Metrics.includes(metric) ? 'bold' : 'normal',
                        }}
                      >
                        {metric.substring(0, 15)}
                      </button>
                    ))}
                  </div>
                </div>
              </div>

              {/* 3 Charts Side by Side */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{ color: COLORS.GREEN, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
                    {chart1Metrics.join(', ') || 'Select metrics'}
                  </div>
                  <div ref={financialChart1Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
                </div>
                <div>
                  <div style={{ color: COLORS.ORANGE, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
                    {chart2Metrics.join(', ') || 'Select metrics'}
                  </div>
                  <div ref={financialChart2Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
                </div>
                <div>
                  <div style={{ color: COLORS.MAGENTA, fontSize: `${fontSize}px`, fontWeight: 'bold', marginBottom: '6px', textAlign: 'center' }}>
                    {chart3Metrics.join(', ') || 'Select metrics'}
                  </div>
                  <div ref={financialChart3Ref} style={{ backgroundColor: COLORS.DARK_BG }} />
                </div>
              </div>
            </div>

            {/* Financial Comparison Table */}
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              marginBottom: '8px',
            }}>
              <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 3}px`, fontWeight: 'bold', marginBottom: '12px' }}>
                YEAR-OVER-YEAR COMPARISON ({selectedMetrics.length} Metrics)
              </div>

              {selectedMetrics.length === 0 ? (
                <div style={{ color: COLORS.YELLOW, fontSize: `${fontSize + 1}px`, textAlign: 'center', padding: '20px' }}>
                  Please select at least one metric to compare
                </div>
              ) : (
                <div style={{ overflowX: 'auto' }} className="custom-scrollbar">
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: `${fontSize}px` }}>
                    <thead>
                      <tr style={{ borderBottom: `2px solid ${COLORS.BORDER}` }}>
                        <th style={{ textAlign: 'left', padding: '10px', color: COLORS.GRAY, fontSize: `${fontSize + 1}px` }}>METRIC</th>
                        {Object.keys(financials.income_statement).slice(0, showYearsCount).map((period) => (
                          <th key={period} style={{ textAlign: 'right', padding: '10px', color: COLORS.ORANGE, fontSize: `${fontSize + 1}px` }}>
                            {getYearFromPeriod(period)}
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {selectedMetrics.map((metric, idx) => {
                        const periods = Object.keys(financials.income_statement).slice(0, showYearsCount);
                        return (
                          <tr key={metric} style={{ borderBottom: `1px solid ${COLORS.BORDER}20` }}>
                            <td style={{ padding: '10px', color: COLORS.WHITE, fontWeight: 'bold', fontSize: `${fontSize}px` }}>{metric}</td>
                            {periods.map((period) => {
                              const value = financials.income_statement[period][metric];
                              return (
                                <td key={period} style={{ textAlign: 'right', padding: '10px', color: COLORS.CYAN, fontSize: `${fontSize}px` }}>
                                  {value ? (metric.includes('EPS') ? `$${formatNumber(value)}` : formatLargeNumber(value)) : 'N/A'}
                                </td>
                              );
                            })}
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                </div>
              )}
            </div>

            {/* Detailed Financial Statements */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
              {/* Income Statement */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.GREEN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  INCOME STATEMENT (Latest)
                </div>
                <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
                  {Object.keys(financials.income_statement).length > 0 ? (
                    <>
                      {Object.entries(Object.values(financials.income_statement)[0] || {}).map(([metric, _], idx) => {
                        const latestPeriod = Object.keys(financials.income_statement)[0];
                        const value = financials.income_statement[latestPeriod][metric];
                        return (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                            <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                            <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                          </div>
                        );
                      })}
                    </>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
                  )}
                </div>
              </div>

              {/* Balance Sheet */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  BALANCE SHEET (Latest)
                </div>
                <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
                  {Object.keys(financials.balance_sheet).length > 0 ? (
                    <>
                      {Object.entries(Object.values(financials.balance_sheet)[0] || {}).map(([metric, _], idx) => {
                        const latestPeriod = Object.keys(financials.balance_sheet)[0];
                        const value = financials.balance_sheet[latestPeriod][metric];
                        return (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                            <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                            <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                          </div>
                        );
                      })}
                    </>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
                  )}
                </div>
              </div>

              {/* Cash Flow */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '8px',
              }}>
                <div style={{ color: COLORS.CYAN, fontSize: `${fontSize + 2}px`, fontWeight: 'bold', marginBottom: '8px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '4px' }}>
                  CASH FLOW STATEMENT (Latest)
                </div>
                <div style={{ fontSize: `${fontSize}px`, maxHeight: '500px', overflow: 'auto' }} className="custom-scrollbar">
                  {Object.keys(financials.cash_flow).length > 0 ? (
                    <>
                      {Object.entries(Object.values(financials.cash_flow)[0] || {}).map(([metric, _], idx) => {
                        const latestPeriod = Object.keys(financials.cash_flow)[0];
                        const value = financials.cash_flow[latestPeriod][metric];
                        return (
                          <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', paddingBottom: '6px', borderBottom: `1px solid rgba(255,255,255,0.05)` }}>
                            <span style={{ color: COLORS.GRAY, fontSize: `${fontSize - 1}px`, maxWidth: '60%' }}>{metric}:</span>
                            <span style={{ color: COLORS.CYAN, fontSize: `${fontSize}px` }}>{formatLargeNumber(value)}</span>
                          </div>
                        );
                      })}
                    </>
                  ) : (
                    <div style={{ color: COLORS.GRAY, textAlign: 'center', padding: '20px', fontSize: `${fontSize}px` }}>No data available</div>
                  )}
                </div>
              </div>
            </div>
          </div>
            )}
          </div>
        )}

        {/* Analysis Tab */}
        {activeTab === 'analysis' && (
          <div style={{ padding: '0 8px 8px 8px' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              {/* Financial Health */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.ORANGE, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  FINANCIAL HEALTH
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '11px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>TOTAL CASH</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.total_cash)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>TOTAL DEBT</div>
                    <div style={{ color: COLORS.RED, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.total_debt)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>FREE CASHFLOW</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.free_cashflow)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>OPERATING CASHFLOW</div>
                    <div style={{ color: COLORS.BLUE, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.operating_cashflow)}</div>
                  </div>
                </div>
              </div>

              {/* Enterprise Value */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.CYAN, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  ENTERPRISE VALUE METRICS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '11px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>ENTERPRISE VALUE</div>
                    <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.enterprise_value)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>EV/REVENUE</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.enterprise_to_revenue)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>EV/EBITDA</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.enterprise_to_ebitda)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>BOOK VALUE</div>
                    <div style={{ color: COLORS.WHITE, fontSize: '16px', fontWeight: 'bold' }}>${formatNumber(stockInfo?.book_value)}</div>
                  </div>
                </div>
              </div>

              {/* Revenue & Profits */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.GREEN, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  REVENUE & PROFITS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', fontSize: '11px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>TOTAL REVENUE</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.total_revenue)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>REVENUE PER SHARE</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '16px', fontWeight: 'bold' }}>${formatNumber(stockInfo?.revenue_per_share)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>GROSS PROFITS</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '16px', fontWeight: 'bold' }}>{formatLargeNumber(stockInfo?.gross_profits)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px' }}>EBITDA MARGINS</div>
                    <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 'bold' }}>{formatPercent(stockInfo?.ebitda_margins)}</div>
                  </div>
                </div>
              </div>

              {/* Key Ratios Summary */}
              <div style={{
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                padding: '12px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '13px', fontWeight: 'bold', marginBottom: '12px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '6px' }}>
                  KEY RATIOS SUMMARY
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', fontSize: '10px' }}>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>P/E RATIO</div>
                    <div style={{ color: COLORS.WHITE, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.pe_ratio)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>PEG RATIO</div>
                    <div style={{ color: COLORS.WHITE, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.peg_ratio)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>ROE</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold' }}>{formatPercent(stockInfo?.return_on_equity)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>ROA</div>
                    <div style={{ color: COLORS.GREEN, fontSize: '14px', fontWeight: 'bold' }}>{formatPercent(stockInfo?.return_on_assets)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>BETA</div>
                    <div style={{ color: COLORS.CYAN, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.beta)}</div>
                  </div>
                  <div>
                    <div style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '3px' }}>SHORT RATIO</div>
                    <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>{formatNumber(stockInfo?.short_ratio)}</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* News Tab */}
        {activeTab === 'news' && (
          <div style={{ padding: '8px', height: 'calc(100vh - 280px)', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
            {newsLoading ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '400px',
                gap: '16px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
                  ⏳ FETCHING LATEST NEWS...
                </div>
                <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                  Loading news for {currentSymbol}
                </div>
              </div>
            ) : !newsData || !newsData.success ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '400px',
                gap: '16px',
              }}>
                <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>
                  ⚠️ NO NEWS AVAILABLE
                </div>
                <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                  {newsData?.error || 'Unable to fetch news at this time'}
                </div>
                <button
                  onClick={fetchNews}
                  style={{
                    backgroundColor: COLORS.ORANGE,
                    border: 'none',
                    color: COLORS.DARK_BG,
                    padding: '8px 16px',
                    fontSize: '10px',
                    cursor: 'pointer',
                    fontWeight: 'bold',
                    marginTop: '8px',
                  }}
                >
                  RETRY
                </button>
              </div>
            ) : newsData.data && newsData.data.length === 0 ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '400px',
                gap: '16px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
                  📰 NO NEWS FOUND
                </div>
                <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                  No recent news articles found for {stockInfo?.company_name || currentSymbol}
                </div>
              </div>
            ) : (
              <>
                {/* News Header */}
                <div style={{
                  backgroundColor: COLORS.PANEL_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  padding: '8px 12px',
                  marginBottom: '8px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  flexShrink: 0,
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                    <span style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
                      📰 LATEST NEWS
                    </span>
                    <span style={{ color: COLORS.GRAY, fontSize: '9px' }}>
                      {newsData.count || 0} articles found
                    </span>
                    <span style={{ color: COLORS.CYAN, fontSize: '9px' }}>
                      {stockInfo?.company_name || currentSymbol}
                    </span>
                  </div>
                  <button
                    onClick={fetchNews}
                    style={{
                      backgroundColor: COLORS.DARK_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      color: COLORS.WHITE,
                      padding: '4px 12px',
                      fontSize: '9px',
                      cursor: 'pointer',
                      fontWeight: 'bold',
                    }}
                  >
                    🔄 REFRESH
                  </button>
                </div>

                {/* News Articles List */}
                <div style={{
                  flex: 1,
                  overflow: 'auto',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '8px',
                }} className="custom-scrollbar">
                  {newsData.data.map((article: any, index: number) => (
                    <div
                      key={index}
                      style={{
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        padding: '12px',
                        cursor: 'pointer',
                        transition: 'all 0.2s',
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.backgroundColor = '#252525';
                        e.currentTarget.style.borderColor = COLORS.ORANGE;
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = COLORS.PANEL_BG;
                        e.currentTarget.style.borderColor = COLORS.BORDER;
                      }}
                      onClick={() => {
                        if (article.url && article.url !== '#') {
                          window.open(article.url, '_blank');
                        }
                      }}
                    >
                      {/* Article Header */}
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
                        <div style={{ flex: 1, marginRight: '12px' }}>
                          <h3 style={{
                            color: COLORS.WHITE,
                            fontSize: '12px',
                            fontWeight: 'bold',
                            margin: 0,
                            marginBottom: '6px',
                            lineHeight: '1.4',
                          }}>
                            {article.title}
                          </h3>
                          <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
                            <span style={{
                              color: COLORS.CYAN,
                              fontSize: '9px',
                              fontWeight: 'bold',
                            }}>
                              {article.publisher || 'Unknown Source'}
                            </span>
                            <span style={{
                              color: COLORS.GRAY,
                              fontSize: '9px',
                            }}>
                              {article.published_date || 'Unknown Date'}
                            </span>
                          </div>
                        </div>
                      </div>

                      {/* Article Description */}
                      {article.description && article.description !== 'N/A' && (
                        <div style={{
                          color: COLORS.GRAY,
                          fontSize: '10px',
                          lineHeight: '1.5',
                          marginBottom: '8px',
                        }}>
                          {article.description}
                        </div>
                      )}

                      {/* Read More Link */}
                      {article.url && article.url !== '#' && (
                        <div style={{
                          color: COLORS.ORANGE,
                          fontSize: '9px',
                          fontWeight: 'bold',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                        }}>
                          READ FULL ARTICLE →
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              </>
            )}
          </div>
        )}

        {/* Technicals Tab */}
        {activeTab === 'technicals' && (
          <div style={{ padding: '8px' }}>
            {technicalsLoading ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '400px',
                gap: '16px',
              }}>
                <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
                  ⏳ COMPUTING TECHNICAL INDICATORS...
                </div>
                <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                  Analyzing {historicalData.length} data points
                </div>
              </div>
            ) : !technicalsData ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '400px',
                gap: '16px',
              }}>
                <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>
                  ⚠️ NO TECHNICAL DATA AVAILABLE
                </div>
                <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                  Please wait for historical data to load
                </div>
              </div>
            ) : (
              <div style={{ padding: '8px', overflow: 'auto', height: 'calc(100vh - 280px)' }} className="custom-scrollbar">
                {/* Check if we have valid indicator data */}
                {(!technicalsData.indicator_columns || !technicalsData.data || !Array.isArray(technicalsData.data) || technicalsData.data.length === 0) ? (
                  <div style={{
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    padding: '16px',
                    textAlign: 'center',
                    color: COLORS.YELLOW,
                  }}>
                    <div style={{ fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                      ⚠️ NO INDICATOR DATA
                    </div>
                    <div style={{ fontSize: '11px', color: COLORS.GRAY }}>
                      Technical indicators could not be computed. Please try again or select a different symbol.
                    </div>
                  </div>
                ) : (
                  <>
                    {/* Render Individual Indicator Boxes */}
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(420px, 1fr))', gap: '16px' }}>
                      {/* Trend Indicators */}
                      {technicalsData.indicator_columns?.trend && Array.isArray(technicalsData.indicator_columns.trend) &&
                        technicalsData.indicator_columns.trend.map((indicator: string) => {
                          const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
                          if (values.length === 0) return null;
                          const latestValue = values[values.length - 1];
                          const min = Math.min(...values);
                          const max = Math.max(...values);
                          const avg = values.reduce((a: number, b: number) => a + b, 0) / values.length;

                          return (
                            <IndicatorBox
                              key={indicator}
                              name={indicator}
                              latestValue={latestValue}
                              min={min}
                              max={max}
                              avg={avg}
                              values={values}
                              priceData={historicalData}
                              color={COLORS.CYAN}
                              category="TREND"
                            />
                          );
                        })}

                      {/* Momentum Indicators */}
                      {technicalsData.indicator_columns?.momentum && Array.isArray(technicalsData.indicator_columns.momentum) &&
                        technicalsData.indicator_columns.momentum.map((indicator: string) => {
                          const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
                          if (values.length === 0) return null;
                          const latestValue = values[values.length - 1];
                          const min = Math.min(...values);
                          const max = Math.max(...values);
                          const avg = values.reduce((a: number, b: number) => a + b, 0) / values.length;

                          return (
                            <IndicatorBox
                              key={indicator}
                              name={indicator}
                              latestValue={latestValue}
                              min={min}
                              max={max}
                              avg={avg}
                              values={values}
                              priceData={historicalData}
                              color={COLORS.GREEN}
                              category="MOMENTUM"
                            />
                          );
                        })}

                      {/* Volatility Indicators */}
                      {technicalsData.indicator_columns?.volatility && Array.isArray(technicalsData.indicator_columns.volatility) &&
                        technicalsData.indicator_columns.volatility.map((indicator: string) => {
                          const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
                          if (values.length === 0) return null;
                          const latestValue = values[values.length - 1];
                          const min = Math.min(...values);
                          const max = Math.max(...values);
                          const avg = values.reduce((a: number, b: number) => a + b, 0) / values.length;

                          return (
                            <IndicatorBox
                              key={indicator}
                              name={indicator}
                              latestValue={latestValue}
                              min={min}
                              max={max}
                              avg={avg}
                              values={values}
                              priceData={historicalData}
                              color={COLORS.YELLOW}
                              category="VOLATILITY"
                            />
                          );
                        })}

                      {/* Volume Indicators */}
                      {technicalsData.indicator_columns?.volume && Array.isArray(technicalsData.indicator_columns.volume) &&
                        technicalsData.indicator_columns.volume.map((indicator: string) => {
                          const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
                          if (values.length === 0) return null;
                          const latestValue = values[values.length - 1];
                          const min = Math.min(...values);
                          const max = Math.max(...values);
                          const avg = values.reduce((a: number, b: number) => a + b, 0) / values.length;

                          return (
                            <IndicatorBox
                              key={indicator}
                              name={indicator}
                              latestValue={latestValue}
                              min={min}
                              max={max}
                              avg={avg}
                              values={values}
                              priceData={historicalData}
                              color={COLORS.BLUE}
                              category="VOLUME"
                            />
                          );
                        })}

                      {/* Others Indicators */}
                      {technicalsData.indicator_columns?.others && Array.isArray(technicalsData.indicator_columns.others) &&
                        technicalsData.indicator_columns.others.map((indicator: string) => {
                          const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
                          if (values.length === 0) return null;
                          const latestValue = values[values.length - 1];
                          const min = Math.min(...values);
                          const max = Math.max(...values);
                          const avg = values.reduce((a: number, b: number) => a + b, 0) / values.length;

                          return (
                            <IndicatorBox
                              key={indicator}
                              name={indicator}
                              latestValue={latestValue}
                              min={min}
                              max={max}
                              avg={avg}
                              values={values}
                              priceData={historicalData}
                              color={COLORS.MAGENTA}
                              category="RETURN"
                            />
                          );
                        })}
                    </div>
                  </>
                )}
              </div>
            )}
          </div>
        )}
      </div>

      {/* Footer / Status Bar */}
      <div style={{
        borderTop: `1px solid ${COLORS.BORDER}`,
        backgroundColor: COLORS.PANEL_BG,
        padding: '4px 12px',
        fontSize: '10px',
        color: COLORS.GRAY,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Equity Research Terminal v1.0 | Real-time data analysis and company fundamentals</span>
            <span>Data Source: YFinance</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{currentSymbol}</span>
            {stockInfo?.exchange && <span>{stockInfo.exchange}</span>}
            {currentPrice > 0 && (
              <>
                <span>Price: ${currentPrice.toFixed(2)}</span>
                {priceChange !== 0 && (
                  <span style={{ color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED }}>
                    {priceChange >= 0 ? '▲' : '▼'} {Math.abs(priceChangePercent).toFixed(2)}%
                  </span>
                )}
              </>
            )}
            <span style={{ color: loading ? COLORS.YELLOW : COLORS.GREEN }}>
              {loading ? 'LOADING...' : 'READY'}
            </span>
          </div>
        </div>
      </div>

      {/* Custom Scrollbar Styles */}
      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 4px;
          height: 4px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: transparent;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: rgba(120, 120, 120, 0.3);
          border-radius: 2px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: rgba(120, 120, 120, 0.6);
        }
        .custom-scrollbar::-webkit-scrollbar-corner {
          background: transparent;
        }
      `}</style>
    </div>
  );
};

export default EquityResearchTab;
