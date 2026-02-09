import React, { useState } from 'react';
import { Plus, ChevronDown, ChevronRight } from 'lucide-react';
import type { ConditionItem } from '../types';
import { getIndicatorDef } from '../constants/indicators';
import ConditionRow from './ConditionRow';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GRAY: '#787878', DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F', HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A',
  HOVER: '#1F1F1F', CYAN: '#00E5FF', BLUE: '#0088FF', MUTED: '#4A4A4A',
};

interface ConditionBuilderProps {
  label: string;
  conditions: ConditionItem[];
  logic: 'AND' | 'OR';
  onConditionsChange: (conditions: ConditionItem[]) => void;
  onLogicChange: (logic: 'AND' | 'OR') => void;
}

const DEFAULT_CONDITION: ConditionItem = {
  indicator: 'RSI',
  params: { period: 14 },
  field: 'value',
  operator: 'crosses_below',
  value: 30,
};

// Quick-add condition templates for the dropdown
interface QuickCondition {
  label: string;
  condition: ConditionItem;
}

const QUICK_CONDITIONS: QuickCondition[] = [
  { label: 'RSI < 30 (Oversold)', condition: { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 } },
  { label: 'RSI > 70 (Overbought)', condition: { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '>', value: 70 } },
  { label: 'MACD Histogram > 0', condition: { indicator: 'MACD', params: { fast: 12, slow: 26, signal: 9 }, field: 'histogram', operator: '>', value: 0 } },
  { label: 'Close > EMA(20)', condition: { indicator: 'CLOSE', params: {}, field: 'value', operator: '>', value: 0, compareMode: 'indicator', compareIndicator: 'EMA', compareParams: { period: 20 }, compareField: 'value' } },
  { label: 'Close X↑ SMA(200)', condition: { indicator: 'CLOSE', params: {}, field: 'value', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'SMA', compareParams: { period: 200 }, compareField: 'value' } },
  { label: 'Supertrend Bullish', condition: { indicator: 'SUPERTREND', params: { period: 10, multiplier: 3 }, field: 'direction', operator: '==', value: 1 } },
  { label: 'Bollinger %B < 0 (Oversold)', condition: { indicator: 'BOLLINGER', params: { period: 20, std_dev: 2 }, field: 'pct_b', operator: '<', value: 0 } },
  { label: 'ADX > 25 (Strong Trend)', condition: { indicator: 'ADX', params: { period: 14 }, field: 'value', operator: '>', value: 25 } },
  { label: 'MFI > 80 (OB)', condition: { indicator: 'MFI', params: { period: 14 }, field: 'value', operator: '>', value: 80 } },
  { label: 'Stoch RSI K X↑ D', condition: { indicator: 'STOCH_RSI', params: { period: 14, smooth1: 3, smooth2: 3 }, field: 'k', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'STOCH_RSI', compareParams: { period: 14, smooth1: 3, smooth2: 3 }, compareField: 'd' } },
  { label: 'Price Rising 3 Bars', condition: { indicator: 'CLOSE', params: {}, field: 'value', operator: 'rising', value: 0, lookback: 3 } },
  { label: 'Custom (blank)', condition: { ...DEFAULT_CONDITION } },
];

const ConditionBuilder: React.FC<ConditionBuilderProps> = ({
  label,
  conditions,
  logic,
  onConditionsChange,
  onLogicChange,
}) => {
  const [showQuickAdd, setShowQuickAdd] = useState(false);

  const handleAdd = (condition?: ConditionItem) => {
    // Deep clone to avoid mutating template constants (params/compareParams are nested objects)
    const newCondition = JSON.parse(JSON.stringify(condition || DEFAULT_CONDITION));
    onConditionsChange([...conditions, newCondition]);
    setShowQuickAdd(false);
  };

  const handleChange = (index: number, updated: ConditionItem) => {
    const next = [...conditions];
    next[index] = updated;
    onConditionsChange(next);
  };

  const handleRemove = (index: number) => {
    onConditionsChange(conditions.filter((_, i) => i !== index));
  };

  // Build inline summary for header
  const condCount = conditions.length;
  const indicatorNames = conditions.map(c => {
    const def = getIndicatorDef(c.indicator);
    return def?.label || c.indicator;
  });

  return (
    <div style={{
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.BORDER}`,
      borderRadius: '2px',
    }}>
      {/* Section Header */}
      <div style={{
        padding: '8px 12px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
            {label.toUpperCase()}
          </span>
          {condCount > 0 && (
            <span style={{ fontSize: '8px', color: F.MUTED }}>
              ({condCount} condition{condCount !== 1 ? 's' : ''}: {indicatorNames.join(` ${logic} `)})
            </span>
          )}
        </div>
        <div style={{ display: 'flex', gap: '2px' }}>
          {(['AND', 'OR'] as const).map(l => (
            <button
              key={l}
              onClick={() => onLogicChange(l)}
              style={{
                padding: '3px 8px',
                backgroundColor: logic === l ? F.ORANGE : 'transparent',
                color: logic === l ? F.DARK_BG : F.GRAY,
                border: logic === l ? 'none' : `1px solid ${F.BORDER}`,
                fontSize: '8px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                borderRadius: '2px',
                transition: 'all 0.2s',
              }}
            >
              {l}
            </button>
          ))}
        </div>
      </div>

      {/* Condition rows */}
      <div style={{ padding: '4px 0' }}>
        {conditions.length === 0 && (
          <div style={{
            padding: '12px 16px', textAlign: 'center', color: F.MUTED, fontSize: '9px',
          }}>
            No conditions added. Click "ADD CONDITION" below.
          </div>
        )}
        {conditions.map((cond, i) => (
          <div key={i}>
            {i > 0 && (
              <div style={{
                fontSize: '8px',
                fontWeight: 700,
                color: F.ORANGE,
                paddingLeft: '12px',
                paddingTop: '2px',
                paddingBottom: '2px',
                letterSpacing: '0.5px',
              }}>
                {logic}
              </div>
            )}
            <ConditionRow
              condition={cond}
              index={i}
              onChange={handleChange}
              onRemove={handleRemove}
            />
          </div>
        ))}
      </div>

      {/* Add button + Quick add dropdown */}
      <div style={{ padding: '4px 8px 8px', position: 'relative' }}>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => handleAdd()}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              color: F.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={e => {
              (e.currentTarget as HTMLElement).style.borderColor = F.ORANGE;
              (e.currentTarget as HTMLElement).style.color = F.WHITE;
            }}
            onMouseLeave={e => {
              (e.currentTarget as HTMLElement).style.borderColor = F.BORDER;
              (e.currentTarget as HTMLElement).style.color = F.GRAY;
            }}
          >
            <Plus size={10} />
            ADD CONDITION
          </button>
          <button
            onClick={() => setShowQuickAdd(!showQuickAdd)}
            style={{
              padding: '6px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              color: showQuickAdd ? F.CYAN : F.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              transition: 'all 0.2s',
              borderColor: showQuickAdd ? F.CYAN : F.BORDER,
            }}
          >
            {showQuickAdd ? <ChevronDown size={10} /> : <ChevronRight size={10} />}
            TEMPLATES
          </button>
        </div>

        {/* Quick-add dropdown */}
        {showQuickAdd && (
          <div style={{
            marginTop: '4px',
            backgroundColor: F.DARK_BG,
            border: `1px solid ${F.BORDER}`,
            borderRadius: '2px',
            maxHeight: '200px',
            overflow: 'auto',
          }}>
            {QUICK_CONDITIONS.map((qc, i) => (
              <button
                key={i}
                onClick={() => handleAdd(qc.condition)}
                style={{
                  width: '100%',
                  padding: '6px 10px',
                  backgroundColor: 'transparent',
                  border: 'none',
                  borderBottom: i < QUICK_CONDITIONS.length - 1 ? `1px solid ${F.BORDER}` : 'none',
                  color: F.GRAY,
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  cursor: 'pointer',
                  textAlign: 'left',
                  transition: 'all 0.1s',
                }}
                onMouseEnter={e => {
                  (e.currentTarget as HTMLElement).style.backgroundColor = F.HEADER_BG;
                  (e.currentTarget as HTMLElement).style.color = F.WHITE;
                }}
                onMouseLeave={e => {
                  (e.currentTarget as HTMLElement).style.backgroundColor = 'transparent';
                  (e.currentTarget as HTMLElement).style.color = F.GRAY;
                }}
              >
                {qc.label}
              </button>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default ConditionBuilder;
