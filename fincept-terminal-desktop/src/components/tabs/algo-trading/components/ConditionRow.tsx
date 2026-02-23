import React from 'react';
import { Clock, ArrowRight, AlertTriangle } from 'lucide-react';
import type { ConditionItem } from '../types';
import { INDICATOR_DEFINITIONS, OPERATORS, getIndicatorDef } from '../constants/indicators';
import { F } from '../constants/theme';

// Operator labels for readable display
const OPERATOR_LABELS: Record<string, string> = {
  '>': 'Greater than',
  '<': 'Less than',
  '>=': 'Greater or equal',
  '<=': 'Less or equal',
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
  'OPEN': 'price', 'HIGH': 'price', 'LOW': 'price', 'CLOSE': 'price',
  'SMA': 'price', 'EMA': 'price', 'WMA': 'price', 'VWAP': 'price',
  'BOLLINGER': 'price', 'SUPERTREND': 'price', 'KELTNER': 'price',
  'DONCHIAN': 'price', 'PIVOT': 'price', 'ATR': 'price', 'PSAR': 'price',
  'RSI': 'percent_0_100', 'STOCH': 'percent_0_100', 'STOCH_RSI': 'percent_0_100',
  'CCI': 'percent_0_100', 'WILLIAMS_R': 'percent_0_100', 'MFI': 'percent_0_100',
  'CMO': 'percent_0_100', 'ADX': 'percent_0_100', 'AROON': 'percent_0_100',
  'MACD': 'oscillator', 'OBV': 'volume', 'VOLUME': 'volume',
  'ROC': 'ratio', 'MOMENTUM': 'oscillator',
};

const FIELD_SCALE_OVERRIDES: Record<string, Record<string, ScaleType>> = {
  'BOLLINGER': { 'pct_b': 'percent_0_1', 'upper': 'price', 'middle': 'price', 'lower': 'price', 'width': 'ratio' },
  'SUPERTREND': { 'value': 'price', 'direction': 'direction' },
  'STOCH': { 'k': 'percent_0_100', 'd': 'percent_0_100' },
  'STOCH_RSI': { 'k': 'percent_0_100', 'd': 'percent_0_100' },
  'MACD': { 'line': 'oscillator', 'signal_line': 'oscillator', 'histogram': 'oscillator' },
  'AROON': { 'up': 'percent_0_100', 'down': 'percent_0_100', 'oscillator': 'percent_0_100' },
};

function getIndicatorScale(indicator: string, field: string): ScaleType {
  if (FIELD_SCALE_OVERRIDES[indicator]?.[field]) return FIELD_SCALE_OVERRIDES[indicator][field];
  return INDICATOR_SCALES[indicator] || 'price';
}

function areScalesCompatible(scale1: ScaleType, scale2: ScaleType): boolean {
  if (scale1 === scale2) return true;
  const priceScales: ScaleType[] = ['price'];
  if (priceScales.includes(scale1) && priceScales.includes(scale2)) return true;
  const percent100Scales: ScaleType[] = ['percent_0_100'];
  if (percent100Scales.includes(scale1) && percent100Scales.includes(scale2)) return true;
  const oscillatorScales: ScaleType[] = ['oscillator'];
  if (oscillatorScales.includes(scale1) && oscillatorScales.includes(scale2)) return true;
  return false;
}

function getCompatibilityWarning(
  indicator1: string, field1: string, indicator2: string, field2: string
): string | null {
  const scale1 = getIndicatorScale(indicator1, field1);
  const scale2 = getIndicatorScale(indicator2, field2);
  if (areScalesCompatible(scale1, scale2)) return null;
  const scaleLabels: Record<ScaleType, string> = {
    'price': 'price level', 'percent_0_100': '0-100 range', 'percent_0_1': '0-1 range',
    'oscillator': 'oscillator', 'direction': 'direction (-1/1)', 'volume': 'volume', 'ratio': 'ratio',
  };
  return `Incompatible scales: ${indicator1} is ${scaleLabels[scale1]}, ${indicator2} is ${scaleLabels[scale2]}`;
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
    <select value={value} onChange={e => onChange(e.target.value)} style={{ ...selectStyle, ...style }}>
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
        <div key={p.key} style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{
            fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
            whiteSpace: 'nowrap',
          }}>
            {p.label.toUpperCase()}
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

const ConditionRow: React.FC<ConditionRowProps> = ({ condition, index, onChange, onRemove }) => {
  const indicatorDef = getIndicatorDef(condition.indicator);
  const fields = indicatorDef?.fields || ['value'];
  const compareMode = condition.compareMode || 'value';
  const showOffset = (condition.offset !== undefined && condition.offset > 0) ||
                     (condition.compareOffset !== undefined && condition.compareOffset > 0);

  const compareDef = condition.compareIndicator ? getIndicatorDef(condition.compareIndicator) : undefined;
  const compareFields = compareDef?.fields || ['value'];

  const compatibilityWarning = compareMode === 'indicator' && condition.compareIndicator
    ? getCompatibilityWarning(
        condition.indicator, condition.field,
        condition.compareIndicator, condition.compareField || 'value'
      )
    : null;

  const handleIndicatorChange = (name: string) => {
    const def = getIndicatorDef(name);
    if (!def) return;
    const defaultParams: Record<string, number> = {};
    def.params.forEach(p => { defaultParams[p.key] = p.default; });
    onChange(index, { ...condition, indicator: name, field: def.defaultField, params: defaultParams });
  };

  const handleParamChange = (key: string, val: string) => {
    const num = parseFloat(val);
    if (!isNaN(num)) onChange(index, { ...condition, params: { ...condition.params, [key]: num } });
  };

  const handleCompareIndicatorChange = (name: string) => {
    const def = getIndicatorDef(name);
    if (!def) return;
    const defaultParams: Record<string, number> = {};
    def.params.forEach(p => { defaultParams[p.key] = p.default; });
    onChange(index, {
      ...condition, compareIndicator: name, compareField: def.defaultField, compareParams: defaultParams,
    });
  };

  const handleCompareParamChange = (key: string, val: string) => {
    const num = parseFloat(val);
    if (!isNaN(num)) {
      onChange(index, { ...condition, compareParams: { ...(condition.compareParams || {}), [key]: num } });
    }
  };

  const handleCompareWithChange = (value: string) => {
    if (value === 'NUMBER') {
      onChange(index, {
        ...condition, compareMode: 'value',
        compareIndicator: undefined, compareParams: undefined, compareField: undefined,
      });
    } else {
      const def = getIndicatorDef(value);
      if (!def) return;
      const defaultParams: Record<string, number> = {};
      def.params.forEach(p => { defaultParams[p.key] = p.default; });
      onChange(index, {
        ...condition, compareMode: 'indicator',
        compareIndicator: value, compareField: def.defaultField, compareParams: defaultParams,
      });
    }
  };

  const needsLookback = ['crossed_above_within', 'crossed_below_within', 'rising', 'falling'].includes(condition.operator);

  // Build readable summary
  const indicatorLabel = indicatorDef?.label || condition.indicator;
  const paramValues = Object.values(condition.params);
  const paramStr = paramValues.length > 0 ? `(${paramValues.join(',')})` : '';
  const fieldStr = condition.field !== 'value' ? `.${condition.field}` : '';
  const operatorLabel = OPERATOR_LABELS[condition.operator] || condition.operator;

  let compareStr = '';
  if (compareMode === 'indicator' && condition.compareIndicator) {
    const cDef = getIndicatorDef(condition.compareIndicator);
    const cLabel = cDef?.label || condition.compareIndicator;
    const cpv = Object.values(condition.compareParams || {});
    const cpStr = cpv.length > 0 ? `(${cpv.join(',')})` : '';
    const cfStr = condition.compareField && condition.compareField !== 'value' ? `.${condition.compareField}` : '';
    compareStr = `${cLabel}${cpStr}${cfStr}`;
  } else if (condition.operator === 'rising' || condition.operator === 'falling') {
    compareStr = `${condition.lookback || 5} bars`;
  } else if (condition.operator === 'between') {
    compareStr = `${condition.value} and ${condition.value2 ?? 0}`;
  } else {
    compareStr = `${condition.value}`;
  }

  return (
    <div style={{ padding: '10px 12px' }}>
      {/* ─── Readable Summary ─── */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        padding: '8px 12px',
        backgroundColor: `${F.PANEL_BG}`,
        borderRadius: '2px',
        border: `1px solid ${compatibilityWarning ? F.YELLOW + '40' : F.BORDER}`,
        marginBottom: '10px',
      }}>
        <span style={{ fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace' }}>
          <span style={{ color: F.WHITE, fontWeight: 700 }}>{indicatorLabel}{paramStr}{fieldStr}</span>
          <span style={{ color: F.GREEN, margin: '0 8px', fontWeight: 600 }}>{operatorLabel}</span>
          <span style={{ color: compareMode === 'indicator' ? F.PURPLE : F.CYAN, fontWeight: 700 }}>
            {compareStr}
          </span>
        </span>
      </div>

      {/* Compatibility Warning */}
      {compatibilityWarning && (
        <div style={{
          marginBottom: '10px',
          padding: '6px 10px',
          backgroundColor: `${F.YELLOW}12`,
          border: `1px solid ${F.YELLOW}30`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
        }}>
          <AlertTriangle size={10} color={F.YELLOW} />
          <span style={{ fontSize: '9px', color: F.YELLOW, fontFamily: '"IBM Plex Mono", monospace' }}>
            {compatibilityWarning}
          </span>
        </div>
      )}

      {/* ─── Controls Grid: Structured 3-column flow ─── */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr auto 1fr',
        gap: '8px',
        alignItems: 'start',
      }}>
        {/* LEFT: Source Indicator */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          gap: '6px',
          padding: '8px',
          backgroundColor: F.PANEL_BG,
          borderRadius: '2px',
          border: `1px solid ${F.BORDER}`,
        }}>
          <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>
            INDICATOR
          </span>
          <IndicatorSelect value={condition.indicator} onChange={handleIndicatorChange} style={{ width: '100%' }} />
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
            <ParamInputs
              indicatorDef={indicatorDef}
              params={condition.params}
              onChange={handleParamChange}
            />
          </div>
          {fields.length > 1 && (
            <div>
              <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px', display: 'block' }}>
                FIELD
              </span>
              <FieldSelect
                fields={fields}
                value={condition.field}
                onChange={f => onChange(index, { ...condition, field: f })}
              />
            </div>
          )}
          {condition.offset !== undefined && condition.offset > 0 && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Clock size={9} color={F.YELLOW} />
              <span style={{ fontSize: '8px', fontWeight: 700, color: F.YELLOW }}>OFFSET:</span>
              <input
                type="number"
                value={condition.offset}
                onChange={e => {
                  const v = parseInt(e.target.value);
                  onChange(index, { ...condition, offset: isNaN(v) ? 0 : Math.max(0, v) });
                }}
                min={0} max={100}
                style={{ ...inputStyle, width: '45px', color: F.YELLOW, fontSize: '9px' }}
                title="Bars ago"
              />
            </div>
          )}
        </div>

        {/* CENTER: Operator */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: '6px',
          paddingTop: '20px',
        }}>
          <ArrowRight size={12} color={F.ORANGE} />
          <select
            value={condition.operator}
            onChange={e => onChange(index, { ...condition, operator: e.target.value })}
            style={{ ...selectStyle, width: '140px', textAlign: 'center', color: F.GREEN, fontWeight: 700 }}
          >
            {OPERATORS.map(op => (
              <option key={op.value} value={op.value}>{op.label}</option>
            ))}
          </select>
          {needsLookback && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY }}>BARS:</span>
              <input
                type="number"
                value={condition.lookback ?? 5}
                onChange={e => {
                  const v = parseInt(e.target.value);
                  if (!isNaN(v)) onChange(index, { ...condition, lookback: Math.max(1, v) });
                }}
                min={1} max={100}
                style={{ ...inputStyle, width: '50px' }}
              />
            </div>
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
              padding: '3px 8px',
              backgroundColor: showOffset ? `${F.YELLOW}20` : 'transparent',
              border: `1px solid ${showOffset ? F.YELLOW + '60' : F.BORDER}`,
              borderRadius: '2px',
              color: showOffset ? F.YELLOW : F.MUTED,
              cursor: 'pointer',
              fontSize: '8px',
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
              letterSpacing: '0.5px',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              transition: 'all 0.2s',
            }}
            title="Toggle bar offset (n candles ago)"
          >
            <Clock size={8} />
            OFFSET
          </button>
        </div>

        {/* RIGHT: Compare Target */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          gap: '6px',
          padding: '8px',
          backgroundColor: F.PANEL_BG,
          borderRadius: '2px',
          border: `1px solid ${F.BORDER}`,
        }}>
          <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>
            COMPARE WITH
          </span>
          <select
            value={compareMode === 'value' ? 'NUMBER' : (condition.compareIndicator || 'EMA')}
            onChange={e => handleCompareWithChange(e.target.value)}
            style={{
              ...selectStyle,
              width: '100%',
              color: compareMode === 'value' ? F.CYAN : F.PURPLE,
              borderColor: compareMode === 'value' ? `${F.CYAN}40` : `${F.PURPLE}40`,
            }}
          >
            <option value="NUMBER" style={{ color: F.CYAN }}>Number (fixed value)</option>
            <optgroup label="--- Indicators ---">
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
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              <div>
                <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px', display: 'block' }}>
                  VALUE
                </span>
                <input
                  type="number"
                  value={condition.value}
                  onChange={e => {
                    const v = parseFloat(e.target.value);
                    if (!isNaN(v)) onChange(index, { ...condition, value: v });
                  }}
                  step="any"
                  style={{ ...inputStyle, width: '100%', color: F.CYAN, borderColor: `${F.CYAN}40` }}
                  placeholder="Value"
                />
              </div>
              {condition.operator === 'between' && (
                <div>
                  <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px', display: 'block' }}>
                    AND VALUE
                  </span>
                  <input
                    type="number"
                    value={condition.value2 ?? 0}
                    onChange={e => {
                      const v = parseFloat(e.target.value);
                      if (!isNaN(v)) onChange(index, { ...condition, value2: v });
                    }}
                    step="any"
                    style={{ ...inputStyle, width: '100%', color: F.CYAN, borderColor: `${F.CYAN}40` }}
                  />
                </div>
              )}
            </div>
          )}

          {/* Indicator params (when comparing to another Indicator) */}
          {compareMode === 'indicator' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                <ParamInputs
                  indicatorDef={compareDef}
                  params={condition.compareParams || {}}
                  onChange={handleCompareParamChange}
                />
              </div>
              {compareFields.length > 1 && (
                <div>
                  <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '2px', display: 'block' }}>
                    FIELD
                  </span>
                  <FieldSelect
                    fields={compareFields}
                    value={condition.compareField || 'value'}
                    onChange={f => onChange(index, { ...condition, compareField: f })}
                  />
                </div>
              )}
              {condition.compareOffset !== undefined && condition.compareOffset > 0 && (
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <Clock size={9} color={F.YELLOW} />
                  <span style={{ fontSize: '8px', fontWeight: 700, color: F.YELLOW }}>OFFSET:</span>
                  <input
                    type="number"
                    value={condition.compareOffset}
                    onChange={e => {
                      const v = parseInt(e.target.value);
                      onChange(index, { ...condition, compareOffset: isNaN(v) ? 0 : Math.max(0, v) });
                    }}
                    min={0} max={100}
                    style={{ ...inputStyle, width: '45px', color: F.YELLOW, fontSize: '9px' }}
                    title="Bars ago"
                  />
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default ConditionRow;
