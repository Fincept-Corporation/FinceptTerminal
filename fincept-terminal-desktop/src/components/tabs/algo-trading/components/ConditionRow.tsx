import React from 'react';
import { X } from 'lucide-react';
import type { ConditionItem } from '../types';
import { INDICATOR_DEFINITIONS, OPERATORS, getIndicatorDef } from '../constants/indicators';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GRAY: '#787878', DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F', BORDER: '#2A2A2A', HOVER: '#1F1F1F', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', RED: '#FF3B3B',
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

const ConditionRow: React.FC<ConditionRowProps> = ({ condition, index, onChange, onRemove }) => {
  const indicatorDef = getIndicatorDef(condition.indicator);
  const fields = indicatorDef?.fields || ['value'];

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

  const categories = ['momentum', 'trend', 'volatility', 'volume', 'price'] as const;

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
      {/* Indicator selector */}
      <select
        value={condition.indicator}
        onChange={e => handleIndicatorChange(e.target.value)}
        style={{ ...selectStyle, width: '120px' }}
      >
        {categories.map(cat => (
          <optgroup key={cat} label={cat.charAt(0).toUpperCase() + cat.slice(1)}>
            {INDICATOR_DEFINITIONS.filter(d => d.category === cat).map(d => (
              <option key={d.name} value={d.name}>{d.label}</option>
            ))}
          </optgroup>
        ))}
      </select>

      {/* Params */}
      {indicatorDef?.params.map(p => (
        <div key={p.key} style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
            {p.label.toUpperCase()}:
          </span>
          <input
            type="number"
            value={condition.params[p.key] ?? p.default}
            onChange={e => handleParamChange(p.key, e.target.value)}
            min={p.min}
            max={p.max}
            style={inputStyle}
          />
        </div>
      ))}

      {/* Field selector */}
      {fields.length > 1 && (
        <select
          value={condition.field}
          onChange={e => onChange(index, { ...condition, field: e.target.value })}
          style={{ ...selectStyle, width: '90px' }}
        >
          {fields.map(f => (
            <option key={f} value={f}>{f}</option>
          ))}
        </select>
      )}

      {/* Operator */}
      <select
        value={condition.operator}
        onChange={e => onChange(index, { ...condition, operator: e.target.value })}
        style={{ ...selectStyle, width: '110px' }}
      >
        {OPERATORS.map(op => (
          <option key={op.value} value={op.value}>{op.label}</option>
        ))}
      </select>

      {/* Value */}
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
