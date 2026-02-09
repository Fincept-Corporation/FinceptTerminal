import React from 'react';
import { X, ToggleLeft, ToggleRight, Clock } from 'lucide-react';
import type { ConditionItem } from '../types';
import { INDICATOR_DEFINITIONS, OPERATORS, getIndicatorDef } from '../constants/indicators';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GRAY: '#787878', DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F', BORDER: '#2A2A2A', HOVER: '#1F1F1F', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', RED: '#FF3B3B', GREEN: '#00D66F', YELLOW: '#FFD600',
};

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

const ConditionRow: React.FC<ConditionRowProps> = ({ condition, index, onChange, onRemove }) => {
  const indicatorDef = getIndicatorDef(condition.indicator);
  const fields = indicatorDef?.fields || ['value'];
  const compareMode = condition.compareMode || 'value';
  const showOffset = (condition.offset !== undefined && condition.offset > 0) ||
                     (condition.compareOffset !== undefined && condition.compareOffset > 0);

  const compareDef = condition.compareIndicator ? getIndicatorDef(condition.compareIndicator) : undefined;
  const compareFields = compareDef?.fields || ['value'];

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

  const toggleCompareMode = () => {
    if (compareMode === 'value') {
      // Switch to indicator mode — set default compare indicator
      const defaultCompare = 'EMA';
      const def = getIndicatorDef(defaultCompare)!;
      const defaultParams: Record<string, number> = {};
      def.params.forEach(p => { defaultParams[p.key] = p.default; });
      onChange(index, {
        ...condition,
        compareMode: 'indicator',
        compareIndicator: defaultCompare,
        compareField: def.defaultField,
        compareParams: defaultParams,
      });
    } else {
      // Switch back to value mode
      onChange(index, {
        ...condition,
        compareMode: 'value',
        compareIndicator: undefined,
        compareParams: undefined,
        compareField: undefined,
      });
    }
  };

  const needsLookback = ['crossed_above_within', 'crossed_below_within', 'rising', 'falling'].includes(condition.operator);

  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      gap: '6px',
      padding: '6px 8px',
      flexWrap: 'wrap',
      borderRadius: '2px',
      transition: 'all 0.2s',
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

      {/* Compare mode toggle */}
      <button
        onClick={toggleCompareMode}
        style={{
          ...tinyBtnStyle,
          color: compareMode === 'indicator' ? F.ORANGE : F.MUTED,
          borderColor: compareMode === 'indicator' ? F.ORANGE : F.BORDER,
        }}
        title={compareMode === 'value' ? 'Compare with number' : 'Compare with indicator'}
      >
        {compareMode === 'indicator' ? <ToggleRight size={10} /> : <ToggleLeft size={10} />}
        {compareMode === 'indicator' ? 'IND' : '#'}
      </button>

      {/* Right side: value OR indicator comparison */}
      {compareMode === 'value' ? (
        <>
          <input
            type="number"
            value={condition.value}
            onChange={e => {
              const v = parseFloat(e.target.value);
              if (!isNaN(v)) onChange(index, { ...condition, value: v });
            }}
            step="any"
            style={{ ...inputStyle, width: '72px', color: F.CYAN }}
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
                style={{ ...inputStyle, width: '72px', color: F.CYAN }}
              />
            </>
          )}
        </>
      ) : (
        <>
          {/* Compare indicator selector */}
          <IndicatorSelect
            value={condition.compareIndicator || 'EMA'}
            onChange={handleCompareIndicatorChange}
            style={{ borderColor: F.ORANGE + '60' }}
          />

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

      {/* Remove */}
      <button
        onClick={() => onRemove(index)}
        style={{
          padding: '4px',
          backgroundColor: 'transparent',
          border: 'none',
          color: F.MUTED,
          cursor: 'pointer',
          borderRadius: '2px',
          transition: 'all 0.2s',
        }}
        onMouseEnter={e => { (e.currentTarget as HTMLElement).style.color = F.RED; }}
        onMouseLeave={e => { (e.currentTarget as HTMLElement).style.color = F.MUTED; }}
      >
        <X size={12} />
      </button>
    </div>
  );
};

export default ConditionRow;
