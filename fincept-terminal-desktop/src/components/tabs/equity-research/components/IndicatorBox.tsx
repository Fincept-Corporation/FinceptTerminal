import React, { useState, useMemo } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, ReferenceLine, Area, ComposedChart } from 'recharts';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS } from '../../portfolio-tab/finceptStyles';
import { formatIndicatorValue } from '../utils/formatters';
import { TrendingUp, TrendingDown, Minus, Settings2, Activity, AlertCircle, CheckCircle, XCircle, ArrowUp, ArrowDown } from 'lucide-react';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  MUTED: FINCEPT.MUTED,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  BORDER: FINCEPT.BORDER,
  PURPLE: FINCEPT.PURPLE,
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
 * Get indicator zone info (overbought/oversold thresholds)
 */
const getIndicatorZones = (name: string): {
  hasZones: boolean;
  overbought: number;
  oversold: number;
  neutral: { min: number; max: number };
  rangeMin: number;
  rangeMax: number;
  inverted?: boolean;
} => {
  const nameUpper = name.toUpperCase();

  if (nameUpper.includes('RSI')) {
    return { hasZones: true, overbought: 70, oversold: 30, neutral: { min: 30, max: 70 }, rangeMin: 0, rangeMax: 100 };
  }
  if (nameUpper.includes('STOCH') && !nameUpper.includes('RSI')) {
    return { hasZones: true, overbought: 80, oversold: 20, neutral: { min: 20, max: 80 }, rangeMin: 0, rangeMax: 100 };
  }
  if (nameUpper.includes('WILLIAMS')) {
    return { hasZones: true, overbought: -20, oversold: -80, neutral: { min: -80, max: -20 }, rangeMin: -100, rangeMax: 0, inverted: true };
  }
  if (nameUpper.includes('CCI')) {
    return { hasZones: true, overbought: 100, oversold: -100, neutral: { min: -100, max: 100 }, rangeMin: -200, rangeMax: 200 };
  }
  if (nameUpper === 'MFI') {
    return { hasZones: true, overbought: 80, oversold: 20, neutral: { min: 20, max: 80 }, rangeMin: 0, rangeMax: 100 };
  }
  if (nameUpper === 'TSI') {
    return { hasZones: true, overbought: 25, oversold: -25, neutral: { min: -25, max: 25 }, rangeMin: -50, rangeMax: 50 };
  }
  if (nameUpper === 'UO' || nameUpper.includes('ULTIMATE')) {
    return { hasZones: true, overbought: 70, oversold: 30, neutral: { min: 30, max: 70 }, rangeMin: 0, rangeMax: 100 };
  }
  if (nameUpper.includes('BB_PBAND') || nameUpper.includes('KC_PBAND') || nameUpper.includes('DC_PBAND')) {
    return { hasZones: true, overbought: 0.8, oversold: 0.2, neutral: { min: 0.2, max: 0.8 }, rangeMin: 0, rangeMax: 1 };
  }
  if (nameUpper.includes('ADX') && !nameUpper.includes('POS') && !nameUpper.includes('NEG')) {
    return { hasZones: true, overbought: 50, oversold: 20, neutral: { min: 20, max: 50 }, rangeMin: 0, rangeMax: 100 };
  }

  return { hasZones: false, overbought: 0, oversold: 0, neutral: { min: 0, max: 0 }, rangeMin: 0, rangeMax: 100 };
};

/**
 * Get trading signal based on indicator values
 */
const getSignal = (
  name: string,
  latestValue: number,
  prevValue: number,
  avg: number
): { signal: 'BUY' | 'SELL' | 'NEUTRAL'; reason: string; strength: number; zone: 'OVERBOUGHT' | 'OVERSOLD' | 'NEUTRAL' | 'STRONG_TREND' | 'WEAK_TREND' } => {
  const nameUpper = name.toUpperCase();
  const zones = getIndicatorZones(name);

  // === MOMENTUM INDICATORS ===
  if (nameUpper.includes('RSI')) {
    if (latestValue <= 30) return { signal: 'BUY', reason: 'Oversold territory', strength: Math.min(100, (30 - latestValue) * 3), zone: 'OVERSOLD' };
    if (latestValue >= 70) return { signal: 'SELL', reason: 'Overbought territory', strength: Math.min(100, (latestValue - 70) * 3), zone: 'OVERBOUGHT' };
    if (latestValue > 50) return { signal: 'NEUTRAL', reason: 'Bullish momentum', strength: 50, zone: 'NEUTRAL' };
    return { signal: 'NEUTRAL', reason: 'Bearish momentum', strength: 50, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('STOCH') && !nameUpper.includes('RSI')) {
    if (latestValue <= 20) return { signal: 'BUY', reason: 'Oversold zone', strength: Math.min(100, (20 - latestValue) * 5), zone: 'OVERSOLD' };
    if (latestValue >= 80) return { signal: 'SELL', reason: 'Overbought zone', strength: Math.min(100, (latestValue - 80) * 5), zone: 'OVERBOUGHT' };
    return { signal: 'NEUTRAL', reason: 'Neutral zone', strength: 40, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('WILLIAMS')) {
    if (latestValue <= -80) return { signal: 'BUY', reason: 'Oversold', strength: Math.min(100, Math.abs(-100 - latestValue) * 5), zone: 'OVERSOLD' };
    if (latestValue >= -20) return { signal: 'SELL', reason: 'Overbought', strength: Math.min(100, Math.abs(latestValue) * 5), zone: 'OVERBOUGHT' };
    return { signal: 'NEUTRAL', reason: 'Mid range', strength: 40, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('MACD') && !nameUpper.includes('SIGNAL') && !nameUpper.includes('DIFF')) {
    if (prevValue < 0 && latestValue > 0) return { signal: 'BUY', reason: 'Bullish crossover', strength: 85, zone: 'NEUTRAL' };
    if (prevValue > 0 && latestValue < 0) return { signal: 'SELL', reason: 'Bearish crossover', strength: 85, zone: 'NEUTRAL' };
    if (latestValue > 0 && latestValue > prevValue) return { signal: 'BUY', reason: 'Bullish momentum', strength: 60, zone: 'NEUTRAL' };
    if (latestValue < 0 && latestValue < prevValue) return { signal: 'SELL', reason: 'Bearish momentum', strength: 60, zone: 'NEUTRAL' };
    return { signal: 'NEUTRAL', reason: 'Sideways', strength: 30, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('CCI')) {
    if (latestValue <= -100) return { signal: 'BUY', reason: 'Oversold', strength: Math.min(100, Math.abs(latestValue + 100) / 2), zone: 'OVERSOLD' };
    if (latestValue >= 100) return { signal: 'SELL', reason: 'Overbought', strength: Math.min(100, (latestValue - 100) / 2), zone: 'OVERBOUGHT' };
    return { signal: 'NEUTRAL', reason: 'Normal range', strength: 40, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('ADX') && !nameUpper.includes('POS') && !nameUpper.includes('NEG')) {
    if (latestValue > 50) return { signal: 'BUY', reason: 'Very strong trend', strength: 90, zone: 'STRONG_TREND' };
    if (latestValue > 25) return { signal: 'BUY', reason: 'Strong trend', strength: 70, zone: 'STRONG_TREND' };
    if (latestValue < 20) return { signal: 'NEUTRAL', reason: 'Weak/no trend', strength: 30, zone: 'WEAK_TREND' };
    return { signal: 'NEUTRAL', reason: 'Moderate trend', strength: 50, zone: 'NEUTRAL' };
  }

  if (nameUpper === 'MFI') {
    if (latestValue <= 20) return { signal: 'BUY', reason: 'Oversold', strength: Math.min(100, (20 - latestValue) * 5), zone: 'OVERSOLD' };
    if (latestValue >= 80) return { signal: 'SELL', reason: 'Overbought', strength: Math.min(100, (latestValue - 80) * 5), zone: 'OVERBOUGHT' };
    return { signal: 'NEUTRAL', reason: 'Normal flow', strength: 40, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('ROC')) {
    if (latestValue > 5) return { signal: 'BUY', reason: 'Strong momentum', strength: Math.min(100, latestValue * 10), zone: 'NEUTRAL' };
    if (latestValue < -5) return { signal: 'SELL', reason: 'Weak momentum', strength: Math.min(100, Math.abs(latestValue) * 10), zone: 'NEUTRAL' };
    return { signal: 'NEUTRAL', reason: 'Flat', strength: 30, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('BB_PBAND') || nameUpper.includes('KC_PBAND') || nameUpper.includes('DC_PBAND')) {
    if (latestValue < 0.2) return { signal: 'BUY', reason: 'Near lower band', strength: Math.min(100, (0.2 - latestValue) * 500), zone: 'OVERSOLD' };
    if (latestValue > 0.8) return { signal: 'SELL', reason: 'Near upper band', strength: Math.min(100, (latestValue - 0.8) * 500), zone: 'OVERBOUGHT' };
    return { signal: 'NEUTRAL', reason: 'Mid band', strength: 40, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('CMF')) {
    if (latestValue > 0.1) return { signal: 'BUY', reason: 'Buying pressure', strength: Math.min(100, latestValue * 500), zone: 'NEUTRAL' };
    if (latestValue < -0.1) return { signal: 'SELL', reason: 'Selling pressure', strength: Math.min(100, Math.abs(latestValue) * 500), zone: 'NEUTRAL' };
    return { signal: 'NEUTRAL', reason: 'Balanced flow', strength: 30, zone: 'NEUTRAL' };
  }

  if (nameUpper.includes('ATR')) {
    if (latestValue > avg * 1.5) return { signal: 'NEUTRAL', reason: 'High volatility', strength: 70, zone: 'NEUTRAL' };
    if (latestValue < avg * 0.5) return { signal: 'NEUTRAL', reason: 'Low volatility', strength: 30, zone: 'NEUTRAL' };
    return { signal: 'NEUTRAL', reason: 'Normal volatility', strength: 50, zone: 'NEUTRAL' };
  }

  // Moving averages - compare trend direction
  if (nameUpper.includes('SMA') || nameUpper.includes('EMA') || nameUpper.includes('WMA')) {
    const change = ((latestValue - prevValue) / prevValue) * 100;
    if (change > 0.5) return { signal: 'BUY', reason: 'Upward slope', strength: Math.min(100, change * 20), zone: 'NEUTRAL' };
    if (change < -0.5) return { signal: 'SELL', reason: 'Downward slope', strength: Math.min(100, Math.abs(change) * 20), zone: 'NEUTRAL' };
    return { signal: 'NEUTRAL', reason: 'Flat', strength: 30, zone: 'NEUTRAL' };
  }

  return { signal: 'NEUTRAL', reason: 'Monitor', strength: 30, zone: 'NEUTRAL' };
};

/**
 * Calculate momentum direction and strength
 */
const calculateMomentum = (values: number[]): { direction: 'UP' | 'DOWN' | 'FLAT'; change: number; shortTermTrend: 'BULLISH' | 'BEARISH' | 'NEUTRAL' } => {
  if (values.length < 5) return { direction: 'FLAT', change: 0, shortTermTrend: 'NEUTRAL' };

  const recent = values.slice(-5);
  const older = values.slice(-10, -5);

  const recentAvg = recent.reduce((a, b) => a + b, 0) / recent.length;
  const olderAvg = older.length > 0 ? older.reduce((a, b) => a + b, 0) / older.length : recentAvg;

  const change = olderAvg !== 0 ? ((recentAvg - olderAvg) / Math.abs(olderAvg)) * 100 : 0;

  let direction: 'UP' | 'DOWN' | 'FLAT' = 'FLAT';
  let shortTermTrend: 'BULLISH' | 'BEARISH' | 'NEUTRAL' = 'NEUTRAL';

  if (change > 2) {
    direction = 'UP';
    shortTermTrend = 'BULLISH';
  } else if (change < -2) {
    direction = 'DOWN';
    shortTermTrend = 'BEARISH';
  }

  return { direction, change, shortTermTrend };
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

  const prevValue = values[values.length - 2] || latestValue;
  const { signal, reason, strength, zone } = getSignal(name, latestValue, prevValue, avg);
  const signalColor = signal === 'BUY' ? COLORS.GREEN : signal === 'SELL' ? COLORS.RED : COLORS.GRAY;
  const SignalIcon = signal === 'BUY' ? TrendingUp : signal === 'SELL' ? TrendingDown : Minus;

  const zones = getIndicatorZones(name);
  const momentum = calculateMomentum(values);

  // Calculate position in range (0-100%)
  const range = max - min;
  const position = range > 0 ? ((latestValue - min) / range) * 100 : 50;

  // Change from previous
  const change = prevValue !== 0 ? ((latestValue - prevValue) / Math.abs(prevValue)) * 100 : 0;

  // Chart data with zones
  const chartData = useMemo(() => {
    return values.map((value, i) => ({
      index: i,
      value: value || null,
      upper: zones.hasZones ? zones.overbought : null,
      lower: zones.hasZones ? zones.oversold : null,
    }));
  }, [values, zones]);

  // Zone color
  const getZoneColor = () => {
    if (zone === 'OVERBOUGHT') return COLORS.RED;
    if (zone === 'OVERSOLD') return COLORS.GREEN;
    if (zone === 'STRONG_TREND') return COLORS.CYAN;
    if (zone === 'WEAK_TREND') return COLORS.YELLOW;
    return COLORS.GRAY;
  };

  const getZoneLabel = () => {
    if (zone === 'OVERBOUGHT') return 'OVERBOUGHT';
    if (zone === 'OVERSOLD') return 'OVERSOLD';
    if (zone === 'STRONG_TREND') return 'STRONG TREND';
    if (zone === 'WEAK_TREND') return 'WEAK TREND';
    return 'NEUTRAL';
  };

  return (
    <div style={{
      backgroundColor: COLORS.PANEL_BG,
      border: BORDERS.STANDARD,
      borderTop: `2px solid ${signalColor}`,
      display: 'flex',
      flexDirection: 'column',
      fontFamily: TYPOGRAPHY.MONO,
      transition: EFFECTS.TRANSITION_STANDARD,
    }}>
      {/* Header with Signal */}
      <div style={{
        backgroundColor: COLORS.HEADER_BG,
        padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        borderBottom: BORDERS.STANDARD,
      }}>
        <div style={{ flex: 1 }}>
          <div style={{
            color: COLORS.GRAY,
            fontSize: TYPOGRAPHY.TINY,
            textTransform: 'uppercase',
            letterSpacing: TYPOGRAPHY.WIDE,
            marginBottom: '2px',
          }}>
            {category}
          </div>
          <div style={{
            color: COLORS.WHITE,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
          }}>
            {name.toUpperCase().replace(/_/g, ' ')}
          </div>
        </div>

        {/* Signal Badge */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.SMALL,
          padding: `${SPACING.TINY} ${SPACING.SMALL}`,
          backgroundColor: `${signalColor}20`,
          borderLeft: `2px solid ${signalColor}`,
        }}>
          <SignalIcon size={12} color={signalColor} />
          <span style={{
            color: signalColor,
            fontSize: TYPOGRAPHY.SMALL,
            fontWeight: TYPOGRAPHY.BOLD,
          }}>
            {signal}
          </span>
        </div>
      </div>

      {/* Main Value Display with Gauge */}
      <div style={{
        padding: SPACING.MEDIUM,
        display: 'flex',
        gap: SPACING.MEDIUM,
        borderBottom: BORDERS.STANDARD,
      }}>
        {/* Current Value */}
        <div style={{ flex: 1 }}>
          <div style={{
            color: COLORS.WHITE,
            fontSize: '24px',
            fontWeight: TYPOGRAPHY.BOLD,
            fontFamily: TYPOGRAPHY.MONO,
            lineHeight: 1,
          }}>
            {formatIndicatorValue(latestValue)}
          </div>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.TINY,
            marginTop: SPACING.TINY,
          }}>
            {change >= 0 ? (
              <ArrowUp size={10} color={COLORS.GREEN} />
            ) : (
              <ArrowDown size={10} color={COLORS.RED} />
            )}
            <span style={{
              color: change >= 0 ? COLORS.GREEN : COLORS.RED,
              fontSize: TYPOGRAPHY.TINY,
              fontWeight: TYPOGRAPHY.SEMIBOLD,
            }}>
              {change >= 0 ? '+' : ''}{change.toFixed(2)}%
            </span>
          </div>
        </div>

        {/* Zone Indicator */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'flex-end',
          justifyContent: 'center',
        }}>
          <div style={{
            color: getZoneColor(),
            fontSize: TYPOGRAPHY.TINY,
            fontWeight: TYPOGRAPHY.BOLD,
            textTransform: 'uppercase',
            padding: `${SPACING.TINY} ${SPACING.SMALL}`,
            backgroundColor: `${getZoneColor()}15`,
            border: `1px solid ${getZoneColor()}40`,
          }}>
            {getZoneLabel()}
          </div>
          <div style={{
            color: COLORS.MUTED,
            fontSize: TYPOGRAPHY.TINY,
            marginTop: SPACING.TINY,
          }}>
            {reason}
          </div>
        </div>
      </div>

      {/* Visual Gauge Bar */}
      {zones.hasZones && (
        <div style={{
          padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
          backgroundColor: COLORS.DARK_BG,
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.TINY,
            marginBottom: SPACING.TINY,
          }}>
            <span style={{ color: COLORS.GREEN, fontSize: TYPOGRAPHY.TINY }}>
              {zones.inverted ? zones.overbought : zones.oversold}
            </span>
            <div style={{
              flex: 1,
              height: '8px',
              backgroundColor: COLORS.PANEL_BG,
              border: BORDERS.STANDARD,
              position: 'relative',
              overflow: 'hidden',
            }}>
              {/* Oversold Zone */}
              <div style={{
                position: 'absolute',
                left: 0,
                top: 0,
                bottom: 0,
                width: `${((zones.oversold - zones.rangeMin) / (zones.rangeMax - zones.rangeMin)) * 100}%`,
                backgroundColor: `${COLORS.GREEN}30`,
              }} />
              {/* Overbought Zone */}
              <div style={{
                position: 'absolute',
                right: 0,
                top: 0,
                bottom: 0,
                width: `${((zones.rangeMax - zones.overbought) / (zones.rangeMax - zones.rangeMin)) * 100}%`,
                backgroundColor: `${COLORS.RED}30`,
              }} />
              {/* Current Position Marker */}
              <div style={{
                position: 'absolute',
                left: `${Math.max(0, Math.min(100, ((latestValue - zones.rangeMin) / (zones.rangeMax - zones.rangeMin)) * 100))}%`,
                top: '-2px',
                bottom: '-2px',
                width: '3px',
                backgroundColor: signalColor,
                transform: 'translateX(-50%)',
                boxShadow: `0 0 6px ${signalColor}`,
              }} />
            </div>
            <span style={{ color: COLORS.RED, fontSize: TYPOGRAPHY.TINY }}>
              {zones.inverted ? zones.oversold : zones.overbought}
            </span>
          </div>
        </div>
      )}

      {/* Chart with Reference Lines */}
      <div style={{ padding: SPACING.SMALL, height: '80px' }}>
        <ResponsiveContainer width="100%" height="100%">
          <ComposedChart data={chartData} margin={{ top: 5, right: 5, left: 5, bottom: 5 }}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.2} />
            <XAxis dataKey="index" hide />
            <YAxis domain={['auto', 'auto']} hide />
            {zones.hasZones && (
              <>
                <ReferenceLine y={zones.overbought} stroke={COLORS.RED} strokeDasharray="3 3" strokeOpacity={0.5} />
                <ReferenceLine y={zones.oversold} stroke={COLORS.GREEN} strokeDasharray="3 3" strokeOpacity={0.5} />
              </>
            )}
            <Tooltip
              contentStyle={{
                backgroundColor: COLORS.DARK_BG,
                border: BORDERS.STANDARD,
                fontSize: TYPOGRAPHY.SMALL,
                fontFamily: TYPOGRAPHY.MONO,
              }}
              formatter={(value: any) => [formatIndicatorValue(value), name]}
            />
            <Line
              type="monotone"
              dataKey="value"
              stroke={color}
              strokeWidth={1.5}
              dot={false}
              connectNulls
              isAnimationActive={false}
            />
          </ComposedChart>
        </ResponsiveContainer>
      </div>

      {/* Signal Strength + Momentum */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr 1fr',
        gap: SPACING.TINY,
        padding: `0 ${SPACING.MEDIUM} ${SPACING.SMALL}`,
      }}>
        {/* Signal Strength */}
        <div style={{
          backgroundColor: COLORS.DARK_BG,
          padding: SPACING.SMALL,
          border: BORDERS.STANDARD,
        }}>
          <div style={{
            color: COLORS.GRAY,
            fontSize: TYPOGRAPHY.TINY,
            textTransform: 'uppercase',
            marginBottom: SPACING.TINY,
          }}>
            SIGNAL STRENGTH
          </div>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <div style={{
              flex: 1,
              height: '4px',
              backgroundColor: COLORS.PANEL_BG,
              overflow: 'hidden',
            }}>
              <div style={{
                width: `${strength}%`,
                height: '100%',
                backgroundColor: signalColor,
              }} />
            </div>
            <span style={{
              color: signalColor,
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
            }}>
              {strength}%
            </span>
          </div>
        </div>

        {/* Momentum */}
        <div style={{
          backgroundColor: COLORS.DARK_BG,
          padding: SPACING.SMALL,
          border: BORDERS.STANDARD,
        }}>
          <div style={{
            color: COLORS.GRAY,
            fontSize: TYPOGRAPHY.TINY,
            textTransform: 'uppercase',
            marginBottom: SPACING.TINY,
          }}>
            MOMENTUM
          </div>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.TINY,
          }}>
            {momentum.direction === 'UP' && <ArrowUp size={12} color={COLORS.GREEN} />}
            {momentum.direction === 'DOWN' && <ArrowDown size={12} color={COLORS.RED} />}
            {momentum.direction === 'FLAT' && <Minus size={12} color={COLORS.GRAY} />}
            <span style={{
              color: momentum.shortTermTrend === 'BULLISH' ? COLORS.GREEN :
                     momentum.shortTermTrend === 'BEARISH' ? COLORS.RED : COLORS.GRAY,
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
            }}>
              {momentum.shortTermTrend}
            </span>
          </div>
        </div>
      </div>

      {/* Stats Row */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr 1fr',
        gap: '1px',
        backgroundColor: COLORS.BORDER,
        borderTop: BORDERS.STANDARD,
      }}>
        <div style={{
          backgroundColor: COLORS.DARK_BG,
          padding: SPACING.SMALL,
          textAlign: 'center',
        }}>
          <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.TINY, textTransform: 'uppercase' }}>MIN</div>
          <div style={{ color: COLORS.RED, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD }}>
            {formatIndicatorValue(min)}
          </div>
        </div>
        <div style={{
          backgroundColor: COLORS.DARK_BG,
          padding: SPACING.SMALL,
          textAlign: 'center',
        }}>
          <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.TINY, textTransform: 'uppercase' }}>AVG</div>
          <div style={{ color: COLORS.YELLOW, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD }}>
            {formatIndicatorValue(avg)}
          </div>
        </div>
        <div style={{
          backgroundColor: COLORS.DARK_BG,
          padding: SPACING.SMALL,
          textAlign: 'center',
        }}>
          <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.TINY, textTransform: 'uppercase' }}>MAX</div>
          <div style={{ color: COLORS.GREEN, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD }}>
            {formatIndicatorValue(max)}
          </div>
        </div>
        <div style={{
          backgroundColor: COLORS.DARK_BG,
          padding: SPACING.SMALL,
          textAlign: 'center',
        }}>
          <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.TINY, textTransform: 'uppercase' }}>PTS</div>
          <div style={{ color: COLORS.CYAN, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD }}>
            {values.length}
          </div>
        </div>
      </div>

      {/* Settings Toggle */}
      <button
        onClick={() => setShowSettings(!showSettings)}
        style={{
          fontSize: TYPOGRAPHY.TINY,
          fontWeight: TYPOGRAPHY.SEMIBOLD,
          textAlign: 'center',
          padding: SPACING.SMALL,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: SPACING.TINY,
          width: '100%',
          backgroundColor: showSettings ? COLORS.DARK_BG : 'transparent',
          border: 'none',
          borderTop: BORDERS.STANDARD,
          color: COLORS.MUTED,
          fontFamily: TYPOGRAPHY.MONO,
          textTransform: 'uppercase',
          letterSpacing: TYPOGRAPHY.WIDE,
          transition: EFFECTS.TRANSITION_FAST,
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.color = COLORS.ORANGE;
          e.currentTarget.style.backgroundColor = COLORS.HEADER_BG;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.color = COLORS.MUTED;
          e.currentTarget.style.backgroundColor = showSettings ? COLORS.DARK_BG : 'transparent';
        }}
      >
        <Settings2 size={10} />
        <span>{showSettings ? 'HIDE' : 'SETTINGS'}</span>
      </button>

      {showSettings && (
        <div style={{
          padding: SPACING.SMALL,
          backgroundColor: COLORS.DARK_BG,
          borderTop: BORDERS.STANDARD,
          fontSize: TYPOGRAPHY.TINY,
          color: COLORS.GRAY,
          textAlign: 'center',
        }}>
          Parameter customization coming soon
        </div>
      )}
    </div>
  );
};

export default IndicatorBox;
