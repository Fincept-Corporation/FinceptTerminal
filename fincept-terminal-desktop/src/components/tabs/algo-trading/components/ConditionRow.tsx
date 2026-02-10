import React from 'react';
import { Clock } from 'lucide-react';
import type { ConditionItem } from '../types';
import { INDICATOR_DEFINITIONS, OPERATORS, getIndicatorDef } from '../constants/indicators';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GRAY: '#787878', DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F', BORDER: '#2A2A2A', HOVER: '#1F1F1F', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', RED: '#FF3B3B', GREEN: '#00D66F', YELLOW: '#FFD600',
  PURPLE: '#9D4EDD', BLUE: '#0088FF',
};

// Operator labels for readable display
const OPERATOR_LABELS: Record<string, string> = {
  '>': 'Greater than',
  '<': 'Less than',
  '>=': 'Greater than or equal',
  '<=': 'Less than or equal',
  '==': 'Equal to',
  'crosses_above': 'Crossed above',
  'crosses_below': 'Crossed below',
  'between': 'Between',
  'crossed_above_within': 'Crossed above within',
  'crossed_below_within': 'Crossed below within',
  'rising': 'Rising for',
  'falling': 'Falling for',
};

// Indicator scale types for compatibility checking
type ScaleType = 'price' | 'percent_0_100' | 'percent_0_1' | 'oscillator' | 'direction' | 'volume' | 'ratio';

const INDICATOR_SCALES: Record<string, ScaleType> = {
  // Price-based (actual price values)
  'OPEN': 'price',
  'HIGH': 'price',
  'LOW': 'price',
  'CLOSE': 'price',
  'SMA': 'price',
  'EMA': 'price',
  'WMA': 'price',
  'VWAP': 'price',
  'BOLLINGER': 'price', // upper/middle/lower are price, but pct_b is 0-1
  'SUPERTREND': 'price', // value is price, direction is -1/1
  'KELTNER': 'price',
  'DONCHIAN': 'price',
  'PIVOT': 'price',

  // 0-100 oscillators
  'RSI': 'percent_0_100',
  'STOCH': 'percent_0_100',
  'STOCH_RSI': 'percent_0_100',
  'CCI': 'percent_0_100', // can exceed 100 but similar scale
  'WILLIAMS_R': 'percent_0_100',
  'MFI': 'percent_0_100',
  'CMO': 'percent_0_100',

  // 0-1 or small range
  'ADX': 'percent_0_100',
  'ATR': 'price',
  'AROON': 'percent_0_100',

  // Unbounded oscillators (can be negative/positive)
  'MACD': 'oscillator',
  'OBV': 'volume',
  'VOLUME': 'volume',
  'ROC': 'ratio',
  'MOMENTUM': 'oscillator',

  // Direction indicators (-1, 0, 1)
  'PSAR': 'price',
};

// Field-specific scale overrides
const FIELD_SCALE_OVERRIDES: Record<string, Record<string, ScaleType>> = {
  'BOLLINGER': {
    'pct_b': 'percent_0_1',
    'upper': 'price',
    'middle': 'price',
    'lower': 'price',
    'width': 'ratio',
  },
  'SUPERTREND': {
    'value': 'price',
    'direction': 'direction',
  },
  'STOCH': {
    'k': 'percent_0_100',
    'd': 'percent_0_100',
  },
  'STOCH_RSI': {
    'k': 'percent_0_100',
    'd': 'percent_0_100',
  },
  'MACD': {
    'line': 'oscillator',
    'signal_line': 'oscillator',
    'histogram': 'oscillator',
  },
  'AROON': {
    'up': 'percent_0_100',
    'down': 'percent_0_100',
    'oscillator': 'percent_0_100',
  },
};

// Get the scale type for an indicator + field combination
function getIndicatorScale(indicator: string, field: string): ScaleType {
  // Check field-specific override first
  if (FIELD_SCALE_OVERRIDES[indicator]?.[field]) {
    return FIELD_SCALE_OVERRIDES[indicator][field];
  }
  // Fall back to indicator default
  return INDICATOR_SCALES[indicator] || 'price';
}

// Check if two scales are compatible for comparison
function areScalesCompatible(scale1: ScaleType, scale2: ScaleType): boolean {
  if (scale1 === scale2) return true;

  // Price scales are compatible with each other
  const priceScales: ScaleType[] = ['price'];
  if (priceScales.includes(scale1) && priceScales.includes(scale2)) return true;

  // Percent 0-100 scales are compatible
  const percent100Scales: ScaleType[] = ['percent_0_100'];
  if (percent100Scales.includes(scale1) && percent100Scales.includes(scale2)) return true;

  // Oscillators can be compared with each other
  const oscillatorScales: ScaleType[] = ['oscillator'];
  if (oscillatorScales.includes(scale1) && oscillatorScales.includes(scale2)) return true;

  return false;
}

// Get warning message for incompatible comparison
function getCompatibilityWarning(
  indicator1: string,
  field1: string,
  indicator2: string,
  field2: string
): string | null {
  const scale1 = getIndicatorScale(indicator1, field1);
  const scale2 = getIndicatorScale(indicator2, field2);

  if (areScalesCompatible(scale1, scale2)) return null;

  const scaleLabels: Record<ScaleType, string> = {
    'price': 'price level',
    'percent_0_100': '0-100 range',
    'percent_0_1': '0-1 range',
    'oscillator': 'oscillator',
    'direction': 'direction (-1/1)',
    'volume': 'volume',
    'ratio': 'ratio',
  };

  return `⚠ Incompatible scales: ${indicator1} is ${scaleLabels[scale1]}, ${indicator2} is ${scaleLabels[scale2]}`;
}

interface ConditionRowProps {
  condition: ConditionItem;
  index: number;
  onChange: (index: number, updated: ConditionItem) => void;
  onRemove: (index: number) => void;
}

const selectStyle: React.CSSProperties = {
  backgroundColor: F.DARK_BG,
  color: F.WHITE,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
  padding: '6px 8px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", monospace',
  outline: 'none',
};

const inputStyle: React.CSSProperties = {
  ...selectStyle,
  width: '60px',
};

const tinyBtnStyle: React.CSSProperties = {
  padding: '3px 6px',
  backgroundColor: 'transparent',
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
  color: F.MUTED,
  cursor: 'pointer',
  fontSize: '8px',
  fontWeight: 700,
  fontFamily: '"IBM Plex Mono", monospace',
  letterSpacing: '0.5px',
  display: 'flex',
  alignItems: 'center',
  gap: '3px',
  transition: 'all 0.15s',
};

const categories = ['stock_attributes', 'moving_averages', 'momentum', 'trend', 'volatility', 'volume'] as const;

const categoryLabels: Record<string, string> = {
  stock_attributes: 'Stock',
  moving_averages: 'Moving Avg',
  momentum: 'Momentum',
  trend: 'Trend',
  volatility: 'Volatility',
  volume: 'Volume',
};

// Render indicator selector dropdown
function IndicatorSelect({ value, onChange, style }: {
  value: string;
  onChange: (name: string) => void;
  style?: React.CSSProperties;
}) {
  return (
    <select value={value} onChange={e => onChange(e.target.value)} style={{ ...selectStyle, width: '130px', ...style }}>
      {categories.map(cat => {
        const indicators = INDICATOR_DEFINITIONS.filter(d => d.category === cat);
        if (indicators.length === 0) return null;
        return (
          <optgroup key={cat} label={categoryLabels[cat] || cat}>
            {indicators.map(d => (
              <option key={d.name} value={d.name}>{d.label}</option>
            ))}
          </optgroup>
        );
      })}
    </select>
  );
}

// Render params inputs for an indicator
function ParamInputs({ indicatorDef, params, onChange }: {
  indicatorDef: ReturnType<typeof getIndicatorDef>;
  params: Record<string, number>;
  onChange: (key: string, val: string) => void;
}) {
  if (!indicatorDef) return null;
  return (
    <>
      {indicatorDef.params.map(p => (
        <div key={p.key} style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
          <span style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
            {p.label.toUpperCase()}:
          </span>
          <input
            type="number"
            value={params[p.key] ?? p.default}
            onChange={e => onChange(p.key, e.target.value)}
            min={p.min}
            max={p.max}
            step={p.key === 'step' || p.key === 'max_step' ? 0.01 : 1}
            style={inputStyle}
          />
        </div>
      ))}
    </>
  );
}

// Render field selector
function FieldSelect({ fields, value, onChange }: {
  fields: string[];
  value: string;
  onChange: (field: string) => void;
}) {
  if (fields.length <= 1) return null;
  return (
    <select value={value} onChange={e => onChange(e.target.value)} style={{ ...selectStyle, width: '100px' }}>
      {fields.map(f => <option key={f} value={f}>{f}</option>)}
    </select>
  );
}

// Build a human-readable summary of the condition (Chartink-style)
function buildReadableSummary(condition: ConditionItem): React.ReactNode {
  const indicatorDef = getIndicatorDef(condition.indicator);
  const indicatorLabel = indicatorDef?.label || condition.indicator;
  const compareMode = condition.compareMode || 'value';
  const operatorLabel = OPERATOR_LABELS[condition.operator] || condition.operator;

  // Build indicator part with params
  const paramValues = Object.values(condition.params);
  const paramStr = paramValues.length > 0 ? `(${paramValues.join(',')})` : '';
  const fieldStr = condition.field !== 'value' ? `.${condition.field}` : '';

  // Build target part
  let targetPart: React.ReactNode;
  if (compareMode === 'indicator' && condition.compareIndicator) {
    const compareDef = getIndicatorDef(condition.compareIndicator);
    const compareLabel = compareDef?.label || condition.compareIndicator;
    const compareParamValues = Object.values(condition.compareParams || {});
    const compareParamStr = compareParamValues.length > 0 ? `(${compareParamValues.join(',')})` : '';
    const compareFieldStr = condition.compareField && condition.compareField !== 'value' ? `.${condition.compareField}` : '';
    targetPart = (
      <span style={{ color: F.PURPLE, fontWeight: 700 }}>
        {compareLabel}{compareParamStr}{compareFieldStr}
      </span>
    );
  } else if (condition.operator === 'rising' || condition.operator === 'falling') {
    targetPart = (
      <span style={{ color: F.CYAN, fontWeight: 700 }}>
        {condition.lookback || 5} bars
      </span>
    );
  } else if (condition.operator === 'between') {
    targetPart = (
      <>
        <span style={{ color: F.CYAN, fontWeight: 700 }}>{condition.value}</span>
        <span style={{ color: F.GRAY }}> and </span>
        <span style={{ color: F.CYAN, fontWeight: 700 }}>{condition.value2 ?? 0}</span>
      </>
    );
  } else {
    targetPart = (
      <span style={{ color: F.CYAN, fontWeight: 700 }}>
        Number {condition.value}
      </span>
    );
  }

  return (
    <span style={{ fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <span style={{ color: F.WHITE, fontWeight: 700 }}>{indicatorLabel}{paramStr}{fieldStr}</span>
      <span style={{ color: F.GREEN, margin: '0 6px' }}>{operatorLabel}</span>
      {targetPart}
    </span>
  );
}

const ConditionRow: React.FC<ConditionRowProps> = ({ condition, index, onChange, onRemove }) => {
  const indicatorDef = getIndicatorDef(condition.indicator);
  const fields = indicatorDef?.fields || ['value'];
  const compareMode = condition.compareMode || 'value';
  const showOffset = (condition.offset !== undefined && condition.offset > 0) ||
                     (condition.compareOffset !== undefined && condition.compareOffset > 0);

  const compareDef = condition.compareIndicator ? getIndicatorDef(condition.compareIndicator) : undefined;
  const compareFields = compareDef?.fields || ['value'];

  // Check for compatibility warning when comparing two indicators
  const compatibilityWarning = compareMode === 'indicator' && condition.compareIndicator
    ? getCompatibilityWarning(
        condition.indicator,
        condition.field,
        condition.compareIndicator,
        condition.compareField || 'value'
      )
    : null;

  const handleIndicatorChange = (name: string) => {
    const def = getIndicatorDef(name);
    if (!def) return;
    const defaultParams: Record<string, number> = {};
    def.params.forEach(p => { defaultParams[p.key] = p.default; });
    onChange(index, {
      ...condition,
      indicator: name,
      field: def.defaultField,
      params: defaultParams,
    });
  };

  const handleParamChange = (key: string, val: string) => {
    const num = parseFloat(val);
    if (!isNaN(num)) {
      onChange(index, { ...condition, params: { ...condition.params, [key]: num } });
    }
  };

  const handleCompareIndicatorChange = (name: string) => {
    const def = getIndicatorDef(name);
    if (!def) return;
    const defaultParams: Record<string, number> = {};
    def.params.forEach(p => { defaultParams[p.key] = p.default; });
    onChange(index, {
      ...condition,
      compareIndicator: name,
      compareField: def.defaultField,
      compareParams: defaultParams,
    });
  };

  const handleCompareParamChange = (key: string, val: string) => {
    const num = parseFloat(val);
    if (!isNaN(num)) {
      onChange(index, {
        ...condition,
        compareParams: { ...(condition.compareParams || {}), [key]: num },
      });
    }
  };

  // Handle "Compare With" dropdown change
  const handleCompareWithChange = (value: string) => {
    if (value === 'NUMBER') {
      // Switch to value mode
      onChange(index, {
        ...condition,
        compareMode: 'value',
        compareIndicator: undefined,
        compareParams: undefined,
        compareField: undefined,
      });
    } else {
      // Switch to indicator mode with selected indicator
      const def = getIndicatorDef(value);
      if (!def) return;
      const defaultParams: Record<string, number> = {};
      def.params.forEach(p => { defaultParams[p.key] = p.default; });
      onChange(index, {
        ...condition,
        compareMode: 'indicator',
        compareIndicator: value,
        compareField: def.defaultField,
        compareParams: defaultParams,
      });
    }
  };

  const needsLookback = ['crossed_above_within', 'crossed_below_within', 'rising', 'falling'].includes(condition.operator);

  return (
    <div style={{
      padding: '6px 8px',
      borderRadius: '2px',
      transition: 'all 0.2s',
    }}>
      {/* Readable summary line (Chartink-style) */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: compatibilityWarning ? '4px' : '6px',
        padding: '6px 10px',
        backgroundColor: F.DARK_BG,
        borderRadius: '2px',
        border: `1px solid ${compatibilityWarning ? F.YELLOW + '60' : F.BORDER}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: F.MUTED, fontSize: '9px', fontWeight: 700 }}>▸</span>
          {buildReadableSummary(condition)}
        </div>
        <button
          onClick={() => onRemove(index)}
          style={{
            padding: '2px 6px',
            backgroundColor: 'transparent',
            border: 'none',
            color: F.MUTED,
            cursor: 'pointer',
            borderRadius: '2px',
            fontSize: '10px',
          }}
          onMouseEnter={e => { (e.currentTarget as HTMLElement).style.color = F.RED; }}
          onMouseLeave={e => { (e.currentTarget as HTMLElement).style.color = F.MUTED; }}
          title="Remove condition"
        >
          ✕
        </button>
      </div>

      {/* Compatibility Warning */}
      {compatibilityWarning && (
        <div style={{
          marginBottom: '6px',
          marginLeft: '12px',
          padding: '4px 8px',
          backgroundColor: F.YELLOW + '15',
          border: `1px solid ${F.YELLOW}40`,
          borderRadius: '2px',
          fontSize: '9px',
          color: F.YELLOW,
          fontFamily: '"IBM Plex Mono", monospace',
        }}>
          {compatibilityWarning}
        </div>
      )}

      {/* Controls row */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        flexWrap: 'wrap',
        paddingLeft: '12px',
      }}>
        {/* Left side: indicator + params + field */}
        <IndicatorSelect value={condition.indicator} onChange={handleIndicatorChange} />

      <ParamInputs
        indicatorDef={indicatorDef}
        params={condition.params}
        onChange={handleParamChange}
      />

      <FieldSelect
        fields={fields}
        value={condition.field}
        onChange={f => onChange(index, { ...condition, field: f })}
      />

      {/* Offset for left side */}
      {condition.offset !== undefined && condition.offset > 0 && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '2px' }}>
          <Clock size={8} color={F.YELLOW} />
          <input
            type="number"
            value={condition.offset}
            onChange={e => {
              const v = parseInt(e.target.value);
              onChange(index, { ...condition, offset: isNaN(v) ? 0 : Math.max(0, v) });
            }}
            min={0}
            max={100}
            style={{ ...inputStyle, width: '40px', color: F.YELLOW, fontSize: '9px' }}
            title="Bars ago"
          />
        </div>
      )}

      {/* Operator */}
      <select
        value={condition.operator}
        onChange={e => onChange(index, { ...condition, operator: e.target.value })}
        style={{ ...selectStyle, width: '120px' }}
      >
        {OPERATORS.map(op => (
          <option key={op.value} value={op.value}>{op.label}</option>
        ))}
      </select>

      {/* Lookback for advanced operators */}
      {needsLookback && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
          <span style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY }}>N:</span>
          <input
            type="number"
            value={condition.lookback ?? 5}
            onChange={e => {
              const v = parseInt(e.target.value);
              if (!isNaN(v)) onChange(index, { ...condition, lookback: Math.max(1, v) });
            }}
            min={1}
            max={100}
            style={{ ...inputStyle, width: '45px' }}
          />
        </div>
      )}

      {/* Compare With dropdown - unified selector for Number or Indicator */}
      <select
        value={compareMode === 'value' ? 'NUMBER' : (condition.compareIndicator || 'EMA')}
        onChange={e => handleCompareWithChange(e.target.value)}
        style={{
          ...selectStyle,
          width: '130px',
          color: compareMode === 'value' ? F.CYAN : F.PURPLE,
          borderColor: compareMode === 'value' ? F.CYAN + '40' : F.PURPLE + '40',
        }}
      >
        <option value="NUMBER" style={{ color: F.CYAN }}>Number</option>
        <optgroup label="─── Indicators ───">
          {categories.map(cat => {
            const indicators = INDICATOR_DEFINITIONS.filter(d => d.category === cat);
            if (indicators.length === 0) return null;
            return indicators.map(d => (
              <option key={d.name} value={d.name}>{d.label}</option>
            ));
          })}
        </optgroup>
      </select>

      {/* Value input (when comparing to Number) */}
      {compareMode === 'value' && (
        <>
          <input
            type="number"
            value={condition.value}
            onChange={e => {
              const v = parseFloat(e.target.value);
              if (!isNaN(v)) onChange(index, { ...condition, value: v });
            }}
            step="any"
            style={{ ...inputStyle, width: '72px', color: F.CYAN, borderColor: F.CYAN + '40' }}
            placeholder="Value"
          />

          {/* Value2 for between */}
          {condition.operator === 'between' && (
            <>
              <span style={{ fontSize: '9px', color: F.GRAY }}>AND</span>
              <input
                type="number"
                value={condition.value2 ?? 0}
                onChange={e => {
                  const v = parseFloat(e.target.value);
                  if (!isNaN(v)) onChange(index, { ...condition, value2: v });
                }}
                step="any"
                style={{ ...inputStyle, width: '72px', color: F.CYAN, borderColor: F.CYAN + '40' }}
              />
            </>
          )}
        </>
      )}

      {/* Indicator params (when comparing to another Indicator) */}
      {compareMode === 'indicator' && (
        <>
          <ParamInputs
            indicatorDef={compareDef}
            params={condition.compareParams || {}}
            onChange={handleCompareParamChange}
          />

          <FieldSelect
            fields={compareFields}
            value={condition.compareField || 'value'}
            onChange={f => onChange(index, { ...condition, compareField: f })}
          />

          {/* Offset for compare side */}
          {condition.compareOffset !== undefined && condition.compareOffset > 0 && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '2px' }}>
              <Clock size={8} color={F.YELLOW} />
              <input
                type="number"
                value={condition.compareOffset}
                onChange={e => {
                  const v = parseInt(e.target.value);
                  onChange(index, { ...condition, compareOffset: isNaN(v) ? 0 : Math.max(0, v) });
                }}
                min={0}
                max={100}
                style={{ ...inputStyle, width: '40px', color: F.YELLOW, fontSize: '9px' }}
                title="Bars ago"
              />
            </div>
          )}
        </>
      )}

      {/* Offset toggle */}
      <button
        onClick={() => {
          if (showOffset) {
            onChange(index, { ...condition, offset: 0, compareOffset: 0 });
          } else {
            onChange(index, { ...condition, offset: 1, compareOffset: 0 });
          }
        }}
        style={{
          ...tinyBtnStyle,
          color: showOffset ? F.YELLOW : F.MUTED,
          borderColor: showOffset ? F.YELLOW + '60' : F.BORDER,
        }}
        title="Toggle bar offset (n candles ago)"
      >
        <Clock size={9} />
      </button>
      </div>
    </div>
  );
};

export default ConditionRow;
