import React, { useState } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';
import { formatIndicatorValue } from '../utils/formatters';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  BORDER: FINCEPT.BORDER,
};

interface IndicatorBoxProps {
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
}

/**
 * Get default parameters for a given indicator
 */
const getDefaultParams = (name: string): any => {
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

/**
 * Get default thresholds for a given indicator
 */
const getDefaultThresholds = (name: string): { upper: number; lower: number; label: string } => {
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

/**
 * Get trading signal based on indicator values
 */
const getSignal = (
  name: string,
  latestValue: number,
  prevValue: number,
  avg: number,
  activeThresholds: { upper: number; lower: number }
): { signal: 'BUY' | 'SELL' | 'NEUTRAL'; reason: string } => {
  const nameUpper = name.toUpperCase();

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

export const IndicatorBox: React.FC<IndicatorBoxProps> = ({
  name,
  latestValue,
  min,
  max,
  avg,
  values,
  priceData,
  color,
  category,
  onRecompute,
}) => {
  const [showSettings, setShowSettings] = useState(false);
  const [customThresholds, setCustomThresholds] = useState<{ upper: number; lower: number } | null>(null);
  const [customParams, setCustomParams] = useState<any>(null);

  // Only create indicator chart data (no price data for performance)
  const chartData = values.map((value, i) => ({
    index: i,
    value: value || null,
  }));

  const defaultThresholds = getDefaultThresholds(name);
  const activeThresholds = customThresholds || defaultThresholds;
  const defaultParams = getDefaultParams(name);
  const activeParams = customParams || defaultParams;

  const prevValue = values[values.length - 2] || 0;
  const { signal, reason } = getSignal(name, latestValue, prevValue, avg, activeThresholds);
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
            {formatIndicatorValue(latestValue)}
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
              formatter={(value: any) => formatIndicatorValue(value)}
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
          <span style={{ fontSize: '10px' }}></span>
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
                    Recompute with New Parameters
                  </button>
                )}
              </div>
            )}

            {/* Signal Thresholds Section */}
            {defaultThresholds.upper !== 0 && (
              <div>
                <div style={{ color: COLORS.ORANGE, fontWeight: 'bold', marginBottom: '8px', fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px' }}>
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
          <div style={{ color: COLORS.RED, fontWeight: 'bold' }}>{formatIndicatorValue(min)}</div>
        </div>
        <div>
          <div style={{ color: COLORS.GRAY }}>AVG</div>
          <div style={{ color: COLORS.YELLOW, fontWeight: 'bold' }}>{formatIndicatorValue(avg)}</div>
        </div>
        <div>
          <div style={{ color: COLORS.GRAY }}>MAX</div>
          <div style={{ color: COLORS.GREEN, fontWeight: 'bold' }}>{formatIndicatorValue(max)}</div>
        </div>
      </div>

      {/* Data Points Count */}
      <div style={{ color: COLORS.GRAY, fontSize: '9px', textAlign: 'center', borderTop: `1px solid ${COLORS.BORDER}`, paddingTop: '4px' }}>
        {values.length} data points
      </div>
    </div>
  );
};

export default IndicatorBox;
