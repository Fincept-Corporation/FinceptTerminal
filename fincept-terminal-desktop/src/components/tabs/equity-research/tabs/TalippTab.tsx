import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, AreaChart, Area, BarChart, Bar } from 'recharts';
import { Activity, TrendingUp, Waves, Volume2, BarChart3, Zap, Play, Loader2 } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS } from '../../portfolio-tab/finceptStyles';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  CYAN: FINCEPT.CYAN,
  BLUE: FINCEPT.BLUE,
  PURPLE: FINCEPT.PURPLE,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  HOVER: FINCEPT.HOVER,
  BORDER: FINCEPT.BORDER,
  MUTED: FINCEPT.MUTED,
};

type IndicatorCategory = 'trend' | 'trend_advanced' | 'momentum' | 'volatility' | 'volume' | 'specialized';

interface IndicatorDef {
  id: string;
  label: string;
  dataType: 'prices' | 'ohlcv';
  category: IndicatorCategory;
}

const INDICATORS: Record<IndicatorCategory, { label: string; icon: React.ElementType; color: string; items: IndicatorDef[] }> = {
  trend: {
    label: 'TREND',
    icon: TrendingUp,
    color: COLORS.CYAN,
    items: [
      { id: 'sma', label: 'SMA', dataType: 'prices', category: 'trend' },
      { id: 'ema', label: 'EMA', dataType: 'prices', category: 'trend' },
      { id: 'wma', label: 'WMA', dataType: 'prices', category: 'trend' },
      { id: 'dema', label: 'DEMA', dataType: 'prices', category: 'trend' },
      { id: 'tema', label: 'TEMA', dataType: 'prices', category: 'trend' },
      { id: 'hma', label: 'HMA', dataType: 'prices', category: 'trend' },
      { id: 'kama', label: 'KAMA', dataType: 'prices', category: 'trend' },
      { id: 'alma', label: 'ALMA', dataType: 'prices', category: 'trend' },
      { id: 't3', label: 'T3', dataType: 'prices', category: 'trend' },
      { id: 'zlema', label: 'ZLEMA', dataType: 'prices', category: 'trend' },
    ],
  },
  trend_advanced: {
    label: 'TREND+',
    icon: Activity,
    color: COLORS.BLUE,
    items: [
      { id: 'adx', label: 'ADX', dataType: 'ohlcv', category: 'trend_advanced' },
      { id: 'aroon', label: 'Aroon', dataType: 'ohlcv', category: 'trend_advanced' },
      { id: 'ichimoku', label: 'Ichimoku', dataType: 'ohlcv', category: 'trend_advanced' },
      { id: 'parabolic_sar', label: 'Parabolic SAR', dataType: 'ohlcv', category: 'trend_advanced' },
      { id: 'supertrend', label: 'SuperTrend', dataType: 'ohlcv', category: 'trend_advanced' },
    ],
  },
  momentum: {
    label: 'MOMENTUM',
    icon: Zap,
    color: COLORS.ORANGE,
    items: [
      { id: 'rsi', label: 'RSI', dataType: 'prices', category: 'momentum' },
      { id: 'macd', label: 'MACD', dataType: 'prices', category: 'momentum' },
      { id: 'stoch', label: 'Stochastic', dataType: 'ohlcv', category: 'momentum' },
      { id: 'stoch_rsi', label: 'StochRSI', dataType: 'prices', category: 'momentum' },
      { id: 'cci', label: 'CCI', dataType: 'ohlcv', category: 'momentum' },
      { id: 'roc', label: 'ROC', dataType: 'prices', category: 'momentum' },
      { id: 'tsi', label: 'TSI', dataType: 'prices', category: 'momentum' },
      { id: 'williams', label: 'Williams %R', dataType: 'ohlcv', category: 'momentum' },
    ],
  },
  volatility: {
    label: 'VOLATILITY',
    icon: Waves,
    color: COLORS.YELLOW,
    items: [
      { id: 'atr', label: 'ATR', dataType: 'ohlcv', category: 'volatility' },
      { id: 'bb', label: 'Bollinger Bands', dataType: 'prices', category: 'volatility' },
      { id: 'keltner', label: 'Keltner Channels', dataType: 'ohlcv', category: 'volatility' },
      { id: 'donchian', label: 'Donchian Channels', dataType: 'ohlcv', category: 'volatility' },
      { id: 'chandelier_stop', label: 'Chandelier Stop', dataType: 'ohlcv', category: 'volatility' },
      { id: 'natr', label: 'NATR', dataType: 'ohlcv', category: 'volatility' },
    ],
  },
  volume: {
    label: 'VOLUME',
    icon: Volume2,
    color: COLORS.GREEN,
    items: [
      { id: 'obv', label: 'OBV', dataType: 'ohlcv', category: 'volume' },
      { id: 'vwap', label: 'VWAP', dataType: 'ohlcv', category: 'volume' },
      { id: 'vwma', label: 'VWMA', dataType: 'ohlcv', category: 'volume' },
      { id: 'mfi', label: 'MFI', dataType: 'ohlcv', category: 'volume' },
      { id: 'chaikin_osc', label: 'Chaikin Osc', dataType: 'ohlcv', category: 'volume' },
      { id: 'force_index', label: 'Force Index', dataType: 'ohlcv', category: 'volume' },
    ],
  },
  specialized: {
    label: 'SPECIAL',
    icon: BarChart3,
    color: COLORS.PURPLE,
    items: [
      { id: 'ao', label: 'Awesome Osc', dataType: 'ohlcv', category: 'specialized' },
      { id: 'accu_dist', label: 'Accum/Dist', dataType: 'ohlcv', category: 'specialized' },
      { id: 'bop', label: 'Balance of Power', dataType: 'ohlcv', category: 'specialized' },
      { id: 'chop', label: 'CHOP', dataType: 'ohlcv', category: 'specialized' },
      { id: 'coppock_curve', label: 'Coppock Curve', dataType: 'prices', category: 'specialized' },
      { id: 'dpo', label: 'DPO', dataType: 'prices', category: 'specialized' },
      { id: 'emv', label: 'EMV', dataType: 'ohlcv', category: 'specialized' },
      { id: 'fibonacci', label: 'Fibonacci', dataType: 'ohlcv', category: 'specialized' },
      { id: 'ibs', label: 'IBS', dataType: 'ohlcv', category: 'specialized' },
      { id: 'kst', label: 'KST', dataType: 'prices', category: 'specialized' },
      { id: 'kvo', label: 'KVO', dataType: 'ohlcv', category: 'specialized' },
      { id: 'mass_index', label: 'Mass Index', dataType: 'ohlcv', category: 'specialized' },
      { id: 'mcginley', label: 'McGinley', dataType: 'prices', category: 'specialized' },
      { id: 'mean_dev', label: 'Mean Dev', dataType: 'prices', category: 'specialized' },
      { id: 'pivots_hl', label: 'Pivots H/L', dataType: 'ohlcv', category: 'specialized' },
      { id: 'rogers_satchell', label: 'Rogers-Satchell', dataType: 'ohlcv', category: 'specialized' },
      { id: 'sfx', label: 'SFX', dataType: 'ohlcv', category: 'specialized' },
      { id: 'smma', label: 'SMMA', dataType: 'prices', category: 'specialized' },
      { id: 'sobv', label: 'Smoothed OBV', dataType: 'ohlcv', category: 'specialized' },
      { id: 'stc', label: 'STC', dataType: 'prices', category: 'specialized' },
      { id: 'std_dev', label: 'Std Dev', dataType: 'prices', category: 'specialized' },
      { id: 'trix', label: 'TRIX', dataType: 'prices', category: 'specialized' },
      { id: 'ttm', label: 'TTM Squeeze', dataType: 'ohlcv', category: 'specialized' },
      { id: 'uo', label: 'Ultimate Osc', dataType: 'ohlcv', category: 'specialized' },
      { id: 'vtx', label: 'Vortex', dataType: 'ohlcv', category: 'specialized' },
      { id: 'zigzag', label: 'ZigZag', dataType: 'ohlcv', category: 'specialized' },
    ],
  },
};

// Map category to Tauri command
const COMMAND_MAP: Record<IndicatorCategory, string> = {
  trend: 'talipp_trend',
  trend_advanced: 'talipp_trend_advanced',
  momentum: 'talipp_momentum',
  volatility: 'talipp_volatility',
  volume: 'talipp_volume',
  specialized: 'talipp_specialized',
};

interface TalippTabProps {
  historicalData: Array<{ open: number; high: number; low: number; close: number; volume?: number }>;
}

export const TalippTab: React.FC<TalippTabProps> = ({ historicalData }) => {
  const [activeCategory, setActiveCategory] = useState<IndicatorCategory>('trend');
  const [selectedIndicator, setSelectedIndicator] = useState<string>('sma');
  const [period, setPeriod] = useState(20);
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  const calculateIndicator = async () => {
    if (historicalData.length === 0) {
      setError('No historical data available. Please load a symbol first.');
      return;
    }

    setLoading(true);
    setError(null);
    setResult(null);

    try {
      const def = Object.values(INDICATORS)
        .flatMap(cat => cat.items)
        .find(i => i.id === selectedIndicator);

      if (!def) {
        setError('Unknown indicator');
        return;
      }

      const command = COMMAND_MAP[def.category];
      const prices = historicalData.map(d => d.close);
      const ohlcv = historicalData.map(d => ({
        open: d.open,
        high: d.high,
        low: d.low,
        close: d.close,
        volume: d.volume || 0,
      }));

      const args: Record<string, any> = { indicator: def.id, period };

      if (def.dataType === 'prices') {
        args.prices = prices;
      }
      if (def.dataType === 'ohlcv') {
        args.ohlcv = ohlcv;
      }
      // For momentum indicators that can take either prices or ohlcv
      if (def.category === 'momentum') {
        if (def.dataType === 'prices') {
          args.prices = prices;
        } else {
          args.ohlcv = ohlcv;
        }
      }

      const raw = await invoke<string>(command, args);
      const parsed = JSON.parse(raw);

      if (parsed.error) {
        setError(parsed.error);
      } else {
        setResult(parsed);
      }
    } catch (e: any) {
      setError(e?.toString() || 'Calculation failed');
    } finally {
      setLoading(false);
    }
  };

  const renderChart = () => {
    if (!result) return null;

    // Build chart data from result
    const chartData: any[] = [];

    if (result.values) {
      // Simple values array
      result.values.forEach((v: number | null, i: number) => {
        chartData.push({ idx: i, value: v, price: historicalData[i]?.close });
      });
    } else if (result.upper && result.middle && result.lower) {
      // Band indicators (BB, Keltner, Donchian)
      result.upper.forEach((_: any, i: number) => {
        chartData.push({
          idx: i,
          upper: result.upper[i],
          middle: result.middle[i],
          lower: result.lower[i],
          price: historicalData[i]?.close,
        });
      });
    } else if (result.macd) {
      // MACD
      result.macd.forEach((_: any, i: number) => {
        chartData.push({
          idx: i,
          macd: result.macd[i],
          signal: result.signal[i],
          histogram: result.histogram[i],
        });
      });
    } else if (result.k && result.d) {
      // Stochastic / StochRSI
      result.k.forEach((_: any, i: number) => {
        chartData.push({ idx: i, k: result.k[i], d: result.d[i] });
      });
    } else if (result.tsi) {
      result.tsi.forEach((_: any, i: number) => {
        chartData.push({ idx: i, value: result.tsi[i], signal: result.signal[i] });
      });
    } else if (result.trix) {
      result.trix.forEach((_: any, i: number) => {
        chartData.push({ idx: i, value: result.trix[i], signal: result.signal[i] });
      });
    } else if (result.kst) {
      result.kst.forEach((_: any, i: number) => {
        chartData.push({ idx: i, value: result.kst[i], signal: result.signal[i] });
      });
    } else if (result.kvo) {
      result.kvo.forEach((_: any, i: number) => {
        chartData.push({ idx: i, value: result.kvo[i], signal: result.signal[i] });
      });
    } else if (result.up && result.down) {
      // Aroon
      result.up.forEach((_: any, i: number) => {
        chartData.push({ idx: i, up: result.up[i], down: result.down[i] });
      });
    } else if (result.plus && result.minus) {
      // VTX
      result.plus.forEach((_: any, i: number) => {
        chartData.push({ idx: i, plus: result.plus[i], minus: result.minus[i] });
      });
    } else if (result.long_stop && result.short_stop) {
      result.long_stop.forEach((_: any, i: number) => {
        chartData.push({ idx: i, long_stop: result.long_stop[i], short_stop: result.short_stop[i], price: historicalData[i]?.close });
      });
    } else if (result.tenkan_sen) {
      // Ichimoku
      result.tenkan_sen.forEach((_: any, i: number) => {
        chartData.push({
          idx: i,
          tenkan: result.tenkan_sen[i],
          kijun: result.kijun_sen[i],
          spanA: result.senkou_span_a[i],
          spanB: result.senkou_span_b[i],
          price: historicalData[i]?.close,
        });
      });
    }

    if (chartData.length === 0) return null;

    const catColor = INDICATORS[activeCategory]?.color || COLORS.ORANGE;

    // MACD gets bar chart for histogram
    if (result.macd) {
      return (
        <ResponsiveContainer width="100%" height={280}>
          <BarChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="idx" tick={{ fontSize: 9, fill: COLORS.GRAY }} />
            <YAxis tick={{ fontSize: 9, fill: COLORS.GRAY }} />
            <Tooltip
              contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: 10 }}
              labelStyle={{ color: COLORS.GRAY }}
            />
            <Bar dataKey="histogram" fill={COLORS.CYAN} opacity={0.6} />
            <Line type="monotone" dataKey="macd" stroke={COLORS.ORANGE} dot={false} strokeWidth={1.5} />
            <Line type="monotone" dataKey="signal" stroke={COLORS.RED} dot={false} strokeWidth={1.5} />
          </BarChart>
        </ResponsiveContainer>
      );
    }

    // Band indicators
    if (result.upper && result.middle && result.lower) {
      return (
        <ResponsiveContainer width="100%" height={280}>
          <AreaChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="idx" tick={{ fontSize: 9, fill: COLORS.GRAY }} />
            <YAxis tick={{ fontSize: 9, fill: COLORS.GRAY }} domain={['auto', 'auto']} />
            <Tooltip
              contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: 10 }}
              labelStyle={{ color: COLORS.GRAY }}
            />
            <Area type="monotone" dataKey="upper" stroke={COLORS.RED} fill={COLORS.RED} fillOpacity={0.05} dot={false} />
            <Line type="monotone" dataKey="middle" stroke={catColor} dot={false} strokeWidth={1.5} />
            <Area type="monotone" dataKey="lower" stroke={COLORS.GREEN} fill={COLORS.GREEN} fillOpacity={0.05} dot={false} />
            <Line type="monotone" dataKey="price" stroke={COLORS.GRAY} dot={false} strokeWidth={1} strokeDasharray="3 3" />
          </AreaChart>
        </ResponsiveContainer>
      );
    }

    // Dual line (k/d, up/down, plus/minus, value/signal)
    const keys = Object.keys(chartData[0] || {}).filter(k => k !== 'idx' && k !== 'price');
    if (keys.length === 2 || (keys.length === 3 && keys.includes('price'))) {
      const dataKeys = keys.filter(k => k !== 'price');
      return (
        <ResponsiveContainer width="100%" height={280}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="idx" tick={{ fontSize: 9, fill: COLORS.GRAY }} />
            <YAxis tick={{ fontSize: 9, fill: COLORS.GRAY }} />
            <Tooltip
              contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: 10 }}
              labelStyle={{ color: COLORS.GRAY }}
            />
            <Line type="monotone" dataKey={dataKeys[0]} stroke={catColor} dot={false} strokeWidth={1.5} />
            <Line type="monotone" dataKey={dataKeys[1]} stroke={COLORS.RED} dot={false} strokeWidth={1.5} />
            {keys.includes('price') && (
              <Line type="monotone" dataKey="price" stroke={COLORS.GRAY} dot={false} strokeWidth={1} strokeDasharray="3 3" />
            )}
          </LineChart>
        </ResponsiveContainer>
      );
    }

    // Ichimoku (multi-line)
    if (result.tenkan_sen) {
      return (
        <ResponsiveContainer width="100%" height={280}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="idx" tick={{ fontSize: 9, fill: COLORS.GRAY }} />
            <YAxis tick={{ fontSize: 9, fill: COLORS.GRAY }} domain={['auto', 'auto']} />
            <Tooltip
              contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: 10 }}
              labelStyle={{ color: COLORS.GRAY }}
            />
            <Line type="monotone" dataKey="tenkan" stroke={COLORS.CYAN} dot={false} strokeWidth={1} name="Tenkan" />
            <Line type="monotone" dataKey="kijun" stroke={COLORS.RED} dot={false} strokeWidth={1} name="Kijun" />
            <Line type="monotone" dataKey="spanA" stroke={COLORS.GREEN} dot={false} strokeWidth={1} name="Span A" />
            <Line type="monotone" dataKey="spanB" stroke={COLORS.ORANGE} dot={false} strokeWidth={1} name="Span B" />
            <Line type="monotone" dataKey="price" stroke={COLORS.GRAY} dot={false} strokeWidth={1} strokeDasharray="3 3" name="Price" />
          </LineChart>
        </ResponsiveContainer>
      );
    }

    // Default single-line chart
    return (
      <ResponsiveContainer width="100%" height={280}>
        <LineChart data={chartData}>
          <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
          <XAxis dataKey="idx" tick={{ fontSize: 9, fill: COLORS.GRAY }} />
          <YAxis tick={{ fontSize: 9, fill: COLORS.GRAY }} />
          <Tooltip
            contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: 10 }}
            labelStyle={{ color: COLORS.GRAY }}
          />
          <Line type="monotone" dataKey="value" stroke={catColor} dot={false} strokeWidth={1.5} />
          {chartData[0]?.price !== undefined && (
            <Line type="monotone" dataKey="price" stroke={COLORS.GRAY} dot={false} strokeWidth={1} strokeDasharray="3 3" />
          )}
        </LineChart>
      </ResponsiveContainer>
    );
  };

  const renderLastValues = () => {
    if (!result) return null;

    const entries: { label: string; value: string; color: string }[] = [];

    if (result.last !== undefined && result.last !== null) {
      entries.push({ label: 'LAST', value: typeof result.last === 'object' ? JSON.stringify(result.last) : Number(result.last).toFixed(4), color: COLORS.ORANGE });
    }
    if (result.last_macd !== undefined) {
      entries.push({ label: 'MACD', value: Number(result.last_macd).toFixed(4), color: COLORS.ORANGE });
      entries.push({ label: 'SIGNAL', value: Number(result.last_signal).toFixed(4), color: COLORS.RED });
      entries.push({ label: 'HIST', value: Number(result.last_histogram).toFixed(4), color: COLORS.CYAN });
    }
    if (result.last_k !== undefined) {
      entries.push({ label: '%K', value: Number(result.last_k).toFixed(2), color: COLORS.CYAN });
      entries.push({ label: '%D', value: Number(result.last_d).toFixed(2), color: COLORS.RED });
    }
    if (result.last_upper !== undefined) {
      entries.push({ label: 'UPPER', value: Number(result.last_upper).toFixed(4), color: COLORS.RED });
      entries.push({ label: 'MIDDLE', value: Number(result.last_middle).toFixed(4), color: COLORS.ORANGE });
      entries.push({ label: 'LOWER', value: Number(result.last_lower).toFixed(4), color: COLORS.GREEN });
    }
    if (result.last_up !== undefined) {
      entries.push({ label: 'UP', value: Number(result.last_up).toFixed(2), color: COLORS.GREEN });
      entries.push({ label: 'DOWN', value: Number(result.last_down).toFixed(2), color: COLORS.RED });
    }
    if (result.last_plus !== undefined) {
      entries.push({ label: '+VI', value: Number(result.last_plus).toFixed(4), color: COLORS.GREEN });
      entries.push({ label: '-VI', value: Number(result.last_minus).toFixed(4), color: COLORS.RED });
    }
    if (result.last_tsi !== undefined) {
      entries.push({ label: 'TSI', value: Number(result.last_tsi).toFixed(4), color: COLORS.ORANGE });
      entries.push({ label: 'SIGNAL', value: Number(result.last_signal).toFixed(4), color: COLORS.RED });
    }
    if (result.last_trix !== undefined) {
      entries.push({ label: 'TRIX', value: Number(result.last_trix).toFixed(4), color: COLORS.ORANGE });
      entries.push({ label: 'SIGNAL', value: Number(result.last_signal).toFixed(4), color: COLORS.RED });
    }
    if (result.last_kst !== undefined) {
      entries.push({ label: 'KST', value: Number(result.last_kst).toFixed(4), color: COLORS.ORANGE });
      entries.push({ label: 'SIGNAL', value: Number(result.last_signal).toFixed(4), color: COLORS.RED });
    }
    if (result.last_kvo !== undefined) {
      entries.push({ label: 'KVO', value: Number(result.last_kvo).toFixed(4), color: COLORS.ORANGE });
      entries.push({ label: 'SIGNAL', value: Number(result.last_signal).toFixed(4), color: COLORS.RED });
    }
    if (result.last_long_stop !== undefined) {
      entries.push({ label: 'LONG STOP', value: Number(result.last_long_stop).toFixed(4), color: COLORS.GREEN });
      entries.push({ label: 'SHORT STOP', value: Number(result.last_short_stop).toFixed(4), color: COLORS.RED });
    }
    if (result.last_tenkan !== undefined) {
      entries.push({ label: 'TENKAN', value: Number(result.last_tenkan).toFixed(4), color: COLORS.CYAN });
      entries.push({ label: 'KIJUN', value: Number(result.last_kijun).toFixed(4), color: COLORS.RED });
    }
    if (result.last_value !== undefined) {
      entries.push({ label: 'VALUE', value: Number(result.last_value).toFixed(4), color: COLORS.ORANGE });
      if (result.last_trend !== undefined) {
        entries.push({ label: 'TREND', value: result.last_trend > 0 ? 'BULLISH' : 'BEARISH', color: result.last_trend > 0 ? COLORS.GREEN : COLORS.RED });
      }
    }
    if (result.period !== undefined) {
      entries.push({ label: 'PERIOD', value: String(result.period), color: COLORS.GRAY });
    }

    if (entries.length === 0) return null;

    return (
      <div style={{ display: 'flex', gap: SPACING.DEFAULT, flexWrap: 'wrap' }}>
        {entries.map((e, i) => (
          <div key={i} style={{
            padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
          }}>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY, letterSpacing: '0.5px' }}>{e.label}</div>
            <div style={{ fontSize: TYPOGRAPHY.SUBHEADING, color: e.color, fontWeight: TYPOGRAPHY.BOLD }}>{e.value}</div>
          </div>
        ))}
      </div>
    );
  };

  const currentCat = INDICATORS[activeCategory];
  const currentItems = currentCat.items;

  return (
    <div style={{ padding: SPACING.DEFAULT, display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      {/* Category Tabs */}
      <div style={{ display: 'flex', gap: '2px', flexWrap: 'wrap' }}>
        {(Object.entries(INDICATORS) as [IndicatorCategory, typeof INDICATORS[IndicatorCategory]][]).map(([catId, cat]) => {
          const Icon = cat.icon;
          return (
            <button
              key={catId}
              onClick={() => {
                setActiveCategory(catId);
                setSelectedIndicator(cat.items[0].id);
                setResult(null);
                setError(null);
              }}
              style={{
                padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                backgroundColor: activeCategory === catId ? cat.color : 'transparent',
                border: `1px solid ${activeCategory === catId ? cat.color : COLORS.BORDER}`,
                color: activeCategory === catId ? COLORS.DARK_BG : COLORS.GRAY,
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                transition: EFFECTS.TRANSITION_FAST,
              }}
            >
              <Icon size={12} />
              {cat.label}
            </button>
          );
        })}
      </div>

      {/* Indicator Selector + Period + Run */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.DEFAULT,
        padding: SPACING.DEFAULT,
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
      }}>
        <select
          value={selectedIndicator}
          onChange={e => {
            setSelectedIndicator(e.target.value);
            setResult(null);
            setError(null);
          }}
          style={{
            backgroundColor: COLORS.DARK_BG,
            color: COLORS.WHITE,
            border: `1px solid ${COLORS.BORDER}`,
            padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
            fontSize: TYPOGRAPHY.BODY,
            fontFamily: TYPOGRAPHY.MONO,
            flex: 1,
            maxWidth: 240,
          }}
        >
          {currentItems.map(item => (
            <option key={item.id} value={item.id}>{item.label}</option>
          ))}
        </select>

        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{ fontSize: TYPOGRAPHY.SMALL, color: COLORS.GRAY }}>PERIOD:</span>
          <input
            type="number"
            value={period}
            onChange={e => setPeriod(Math.max(1, parseInt(e.target.value) || 1))}
            style={{
              width: 60,
              backgroundColor: COLORS.DARK_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
              padding: `${SPACING.SMALL} ${SPACING.SMALL}`,
              fontSize: TYPOGRAPHY.BODY,
              fontFamily: TYPOGRAPHY.MONO,
              textAlign: 'center',
            }}
          />
        </div>

        <button
          onClick={calculateIndicator}
          disabled={loading}
          style={{
            padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
            backgroundColor: loading ? COLORS.MUTED : currentCat.color,
            color: COLORS.DARK_BG,
            border: 'none',
            fontSize: TYPOGRAPHY.SMALL,
            fontWeight: TYPOGRAPHY.BOLD,
            cursor: loading ? 'default' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
          }}
        >
          {loading ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
          {loading ? 'COMPUTING...' : 'CALCULATE'}
        </button>

        <div style={{ flex: 1 }} />

        <span style={{ fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY }}>
          {historicalData.length} data points | TALIpp Engine
        </span>
      </div>

      {/* Error */}
      {error && (
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: 'rgba(255, 59, 59, 0.1)',
          border: `1px solid ${COLORS.RED}`,
          color: COLORS.RED,
          fontSize: TYPOGRAPHY.BODY,
        }}>
          {error}
        </div>
      )}

      {/* Results */}
      {result && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
          {/* Last Values */}
          {renderLastValues()}

          {/* Chart */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: SPACING.DEFAULT,
          }}>
            <div style={{
              fontSize: TYPOGRAPHY.SMALL,
              color: currentCat.color,
              fontWeight: TYPOGRAPHY.BOLD,
              marginBottom: SPACING.SMALL,
              letterSpacing: '0.5px',
            }}>
              {currentItems.find(i => i.id === selectedIndicator)?.label?.toUpperCase()} — {currentCat.label}
            </div>
            {renderChart()}
          </div>
        </div>
      )}

      {/* Empty state */}
      {!result && !error && !loading && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          color: COLORS.GRAY,
          fontSize: TYPOGRAPHY.BODY,
          border: `1px solid ${COLORS.BORDER}`,
          backgroundColor: COLORS.PANEL_BG,
        }}>
          <Activity size={32} style={{ marginBottom: '8px', opacity: 0.3 }} />
          <div>Select an indicator and click CALCULATE</div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, marginTop: '4px' }}>
            50+ indicators across 6 categories — powered by TALIpp incremental engine
          </div>
        </div>
      )}
    </div>
  );
};
