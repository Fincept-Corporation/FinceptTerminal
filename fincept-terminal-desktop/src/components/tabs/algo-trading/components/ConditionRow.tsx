import React from 'react';
import { Clock, AlertTriangle } from 'lucide-react';
import type { ConditionItem } from '../types';
import { INDICATOR_DEFINITIONS, OPERATORS, getIndicatorDef } from '../constants/indicators';
import { F } from '../constants/theme';

const font = '"IBM Plex Mono", "Consolas", monospace';
const cellLabel: React.CSSProperties = { fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '3px', display: 'block' };
const inputBase: React.CSSProperties = { padding: '5px 6px', backgroundColor: F.DARK_BG, color: F.WHITE, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px', fontSize: '10px', fontFamily: font, outline: 'none' };
const selectBase: React.CSSProperties = { ...inputBase, cursor: 'pointer' };
const paramInput: React.CSSProperties = { ...inputBase, width: '52px', fontSize: '9px', padding: '3px 5px' };

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

function getCompatibilityWarning(indicator1: string, field1: string, indicator2: string, field2: string): string | null {
  const scale1 = getIndicatorScale(indicator1, field1);
  const scale2 = getIndicatorScale(indicator2, field2);
  if (scale1 === scale2) return null;
  const scaleLabels: Record<ScaleType, string> = {
    'price': 'Price', 'percent_0_100': '0-100', 'percent_0_1': '0-1',
    'oscillator': 'Osc', 'direction': 'Dir', 'volume': 'Vol', 'ratio': 'Ratio',
  };
  return `Scale: ${scaleLabels[scale1]} vs ${scaleLabels[scale2]}`;
}

interface ConditionRowProps {
  condition: ConditionItem;
  index: number;
  onChange: (index: number, updated: ConditionItem) => void;
  onRemove: (index: number) => void;
}

const categories = ['stock_attributes', 'moving_averages', 'momentum', 'trend', 'volatility', 'volume'] as const;
const categoryLabels: Record<string, string> = {
  stock_attributes: 'Stock', moving_averages: 'MA', momentum: 'Momentum',
  trend: 'Trend', volatility: 'Volatility', volume: 'Volume',
};

function IndicatorSelect({ value, onChange, style: extraStyle }: {
  value: string; onChange: (name: string) => void; style?: React.CSSProperties;
}) {
  return (
    <select value={value} onChange={e => onChange(e.target.value)} style={{ ...selectBase, ...extraStyle }}>
      {categories.map(cat => {
        const indicators = INDICATOR_DEFINITIONS.filter(d => d.category === cat);
        if (indicators.length === 0) return null;
        return (
          <optgroup key={cat} label={categoryLabels[cat] || cat}>
            {indicators.map(d => <option key={d.name} value={d.name}>{d.label}</option>)}
          </optgroup>
        );
      })}
    </select>
  );
}

const ConditionRow: React.FC<ConditionRowProps> = ({ condition, index, onChange }) => {
  const indicatorDef = getIndicatorDef(condition.indicator);
  const fields = indicatorDef?.fields || ['value'];
  const compareMode = condition.compareMode || 'value';
  const showOffset = (condition.offset !== undefined && condition.offset > 0) ||
                     (condition.compareOffset !== undefined && condition.compareOffset > 0);
  const compareDef = condition.compareIndicator ? getIndicatorDef(condition.compareIndicator) : undefined;
  const compareFields = compareDef?.fields || ['value'];

  const compatibilityWarning = compareMode === 'indicator' && condition.compareIndicator
    ? getCompatibilityWarning(condition.indicator, condition.field, condition.compareIndicator, condition.compareField || 'value')
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

  const handleCompareParamChange = (key: string, val: string) => {
    const num = parseFloat(val);
    if (!isNaN(num)) onChange(index, { ...condition, compareParams: { ...(condition.compareParams || {}), [key]: num } });
  };

  const handleCompareWithChange = (value: string) => {
    if (value === 'NUMBER') {
      onChange(index, { ...condition, compareMode: 'value', compareIndicator: undefined, compareParams: undefined, compareField: undefined });
    } else {
      const def = getIndicatorDef(value);
      if (!def) return;
      const defaultParams: Record<string, number> = {};
      def.params.forEach(p => { defaultParams[p.key] = p.default; });
      onChange(index, { ...condition, compareMode: 'indicator', compareIndicator: value, compareField: def.defaultField, compareParams: defaultParams });
    }
  };

  const needsLookback = ['crossed_above_within', 'crossed_below_within', 'rising', 'falling'].includes(condition.operator);

  /* ─── Horizontal flow layout: [INDICATOR + params] [OPERATOR] [COMPARE + params] ─── */
  return (
    <div style={{ padding: '8px 12px', fontFamily: font }}>
      {/* Warning */}
      {compatibilityWarning && (
        <div style={{ display: 'inline-flex', alignItems: 'center', gap: '4px', padding: '3px 8px', backgroundColor: `${F.YELLOW}15`, border: `1px solid ${F.YELLOW}30`, borderRadius: '2px', marginBottom: '6px', fontSize: '8px', color: F.YELLOW }}>
          <AlertTriangle size={8} />{compatibilityWarning}
        </div>
      )}

      {/* Main row — wrapping flexbox */}
      <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'flex-end', gap: '8px' }}>

        {/* ── SOURCE INDICATOR ── */}
        <div>
          <span style={cellLabel}>INDICATOR</span>
          <IndicatorSelect value={condition.indicator} onChange={handleIndicatorChange} style={{ minWidth: '130px' }} />
        </div>

        {fields.length > 1 && (
          <div>
            <span style={cellLabel}>FIELD</span>
            <select value={condition.field} onChange={e => onChange(index, { ...condition, field: e.target.value })} style={{ ...selectBase, minWidth: '70px', color: F.CYAN }}>
              {fields.map(f => <option key={f} value={f}>{f}</option>)}
            </select>
          </div>
        )}

        {/* Indicator params inline */}
        {indicatorDef && indicatorDef.params.map(p => (
          <div key={p.key}>
            <span style={cellLabel}>{p.label}</span>
            <input type="number" value={condition.params[p.key] ?? p.default}
              onChange={e => handleParamChange(p.key, e.target.value)}
              min={p.min} max={p.max} step={p.key === 'step' || p.key === 'max_step' ? 0.01 : 1}
              style={paramInput}
              onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        ))}

        {/* Source offset */}
        {condition.offset !== undefined && condition.offset > 0 && (
          <div>
            <span style={{ ...cellLabel, color: F.YELLOW }}><Clock size={7} style={{ verticalAlign: 'middle', marginRight: '2px' }} />OFFSET</span>
            <input type="number" value={condition.offset}
              onChange={e => { const v = parseInt(e.target.value); onChange(index, { ...condition, offset: isNaN(v) ? 0 : Math.max(0, v) }); }}
              min={0} max={100} style={{ ...paramInput, color: F.YELLOW, width: '44px' }}
              onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        )}

        {/* ── OPERATOR ── */}
        <div>
          <span style={cellLabel}>OPERATOR</span>
          <select value={condition.operator} onChange={e => onChange(index, { ...condition, operator: e.target.value })} style={{ ...selectBase, minWidth: '110px', color: F.GREEN }}>
            {OPERATORS.map(op => <option key={op.value} value={op.value}>{op.label}</option>)}
          </select>
        </div>

        {needsLookback && (
          <div>
            <span style={cellLabel}>BARS</span>
            <input type="number" value={condition.lookback ?? 5}
              onChange={e => { const v = parseInt(e.target.value); if (!isNaN(v)) onChange(index, { ...condition, lookback: Math.max(1, v) }); }}
              min={1} max={100} style={{ ...paramInput, width: '44px' }}
              onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        )}

        {/* Offset toggle */}
        <div>
          <span style={cellLabel}>&nbsp;</span>
          <button
            onClick={() => {
              if (showOffset) onChange(index, { ...condition, offset: 0, compareOffset: 0 });
              else onChange(index, { ...condition, offset: 1, compareOffset: 0 });
            }}
            style={{
              ...inputBase, padding: '4px 7px', cursor: 'pointer', fontSize: '8px', fontWeight: 700,
              backgroundColor: showOffset ? `${F.YELLOW}15` : 'transparent',
              borderColor: showOffset ? `${F.YELLOW}50` : F.BORDER,
              color: showOffset ? F.YELLOW : F.MUTED,
              display: 'flex', alignItems: 'center', gap: '3px',
            }}
            onMouseEnter={e => { if (!showOffset) { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; } }}
            onMouseLeave={e => { if (!showOffset) { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.MUTED; } }}
          ><Clock size={8} />OFS</button>
        </div>

        {/* ── COMPARE WITH ── */}
        <div>
          <span style={cellLabel}>COMPARE</span>
          <select
            value={compareMode === 'value' ? 'NUMBER' : (condition.compareIndicator || 'EMA')}
            onChange={e => handleCompareWithChange(e.target.value)}
            style={{ ...selectBase, minWidth: '120px', color: compareMode === 'value' ? F.CYAN : F.PURPLE }}
          >
            <option value="NUMBER">Number</option>
            <optgroup label="Indicators">
              {categories.map(cat => {
                const indicators = INDICATOR_DEFINITIONS.filter(d => d.category === cat);
                if (indicators.length === 0) return null;
                return indicators.map(d => <option key={d.name} value={d.name}>{d.label}</option>);
              })}
            </optgroup>
          </select>
        </div>

        {compareMode === 'value' && (
          <div>
            <span style={cellLabel}>VALUE</span>
            <input type="number" value={condition.value}
              onChange={e => { const v = parseFloat(e.target.value); if (!isNaN(v)) onChange(index, { ...condition, value: v }); }}
              step="any" style={{ ...inputBase, width: '72px', color: F.CYAN }}
              onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        )}

        {compareMode === 'value' && condition.operator === 'between' && (
          <>
            <div style={{ display: 'flex', alignItems: 'flex-end' }}>
              <span style={{ fontSize: '9px', color: F.MUTED, fontWeight: 700, padding: '6px 0' }}>AND</span>
            </div>
            <div>
              <span style={cellLabel}>VALUE 2</span>
              <input type="number" value={condition.value2 ?? 0}
                onChange={e => { const v = parseFloat(e.target.value); if (!isNaN(v)) onChange(index, { ...condition, value2: v }); }}
                step="any" style={{ ...inputBase, width: '72px', color: F.CYAN }}
                onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
              />
            </div>
          </>
        )}

        {compareMode === 'indicator' && compareFields.length > 1 && (
          <div>
            <span style={cellLabel}>CMP FIELD</span>
            <select value={condition.compareField || 'value'} onChange={e => onChange(index, { ...condition, compareField: e.target.value })} style={{ ...selectBase, minWidth: '70px', color: F.PURPLE }}>
              {compareFields.map(f => <option key={f} value={f}>{f}</option>)}
            </select>
          </div>
        )}

        {/* Compare indicator params */}
        {compareMode === 'indicator' && compareDef && compareDef.params.map(p => (
          <div key={`cmp-${p.key}`}>
            <span style={cellLabel}>{p.label}</span>
            <input type="number" value={(condition.compareParams || {})[p.key] ?? p.default}
              onChange={e => handleCompareParamChange(p.key, e.target.value)}
              min={p.min} max={p.max} step={p.key === 'step' || p.key === 'max_step' ? 0.01 : 1}
              style={paramInput}
              onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        ))}

        {/* Compare offset */}
        {compareMode === 'indicator' && condition.compareOffset !== undefined && condition.compareOffset > 0 && (
          <div>
            <span style={{ ...cellLabel, color: F.YELLOW }}><Clock size={7} style={{ verticalAlign: 'middle', marginRight: '2px' }} />CMP OFS</span>
            <input type="number" value={condition.compareOffset}
              onChange={e => { const v = parseInt(e.target.value); onChange(index, { ...condition, compareOffset: isNaN(v) ? 0 : Math.max(0, v) }); }}
              min={0} max={100} style={{ ...paramInput, color: F.YELLOW, width: '44px' }}
              onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
          </div>
        )}
      </div>
    </div>
  );
};

export default ConditionRow;
