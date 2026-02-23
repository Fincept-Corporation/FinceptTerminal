import React, { useState } from 'react';
import { Plus, ChevronDown, ChevronRight, Zap, Layers, Sparkles } from 'lucide-react';
import type { ConditionItem } from '../types';
import { getIndicatorDef } from '../constants/indicators';
import ConditionRow from './ConditionRow';
import { F } from '../constants/theme';

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
  category: string;
  condition: ConditionItem;
}

const QUICK_CONDITIONS: QuickCondition[] = [
  { label: 'RSI < 30 (Oversold)', category: 'Momentum', condition: { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 } },
  { label: 'RSI > 70 (Overbought)', category: 'Momentum', condition: { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '>', value: 70 } },
  { label: 'MACD Histogram > 0', category: 'Momentum', condition: { indicator: 'MACD', params: { fast: 12, slow: 26, signal: 9 }, field: 'histogram', operator: '>', value: 0 } },
  { label: 'Close > EMA(20)', category: 'Trend', condition: { indicator: 'CLOSE', params: {}, field: 'value', operator: '>', value: 0, compareMode: 'indicator', compareIndicator: 'EMA', compareParams: { period: 20 }, compareField: 'value' } },
  { label: 'Close X above SMA(200)', category: 'Trend', condition: { indicator: 'CLOSE', params: {}, field: 'value', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'SMA', compareParams: { period: 200 }, compareField: 'value' } },
  { label: 'Supertrend Bullish', category: 'Trend', condition: { indicator: 'SUPERTREND', params: { period: 10, multiplier: 3 }, field: 'direction', operator: '==', value: 1 } },
  { label: 'Bollinger %B < 0 (Oversold)', category: 'Volatility', condition: { indicator: 'BOLLINGER', params: { period: 20, std_dev: 2 }, field: 'pct_b', operator: '<', value: 0 } },
  { label: 'ADX > 25 (Strong Trend)', category: 'Trend', condition: { indicator: 'ADX', params: { period: 14 }, field: 'value', operator: '>', value: 25 } },
  { label: 'MFI > 80 (Overbought)', category: 'Volume', condition: { indicator: 'MFI', params: { period: 14 }, field: 'value', operator: '>', value: 80 } },
  { label: 'Stoch RSI K X above D', category: 'Momentum', condition: { indicator: 'STOCH_RSI', params: { period: 14, smooth1: 3, smooth2: 3 }, field: 'k', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'STOCH_RSI', compareParams: { period: 14, smooth1: 3, smooth2: 3 }, compareField: 'd' } },
  { label: 'Price Rising 3 Bars', category: 'Trend', condition: { indicator: 'CLOSE', params: {}, field: 'value', operator: 'rising', value: 0, lookback: 3 } },
  { label: 'Custom (blank)', category: 'Custom', condition: { ...DEFAULT_CONDITION } },
];

const ConditionBuilder: React.FC<ConditionBuilderProps> = ({
  label,
  conditions,
  logic,
  onConditionsChange,
  onLogicChange,
}) => {
  const [showQuickAdd, setShowQuickAdd] = useState(false);
  const [dragIndex, setDragIndex] = useState<number | null>(null);

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

  const handleDuplicate = (index: number) => {
    const clone = JSON.parse(JSON.stringify(conditions[index]));
    const next = [...conditions];
    next.splice(index + 1, 0, clone);
    onConditionsChange(next);
  };

  const handleMoveUp = (index: number) => {
    if (index === 0) return;
    const next = [...conditions];
    [next[index - 1], next[index]] = [next[index], next[index - 1]];
    onConditionsChange(next);
  };

  const handleMoveDown = (index: number) => {
    if (index >= conditions.length - 1) return;
    const next = [...conditions];
    [next[index], next[index + 1]] = [next[index + 1], next[index]];
    onConditionsChange(next);
  };

  // Group quick conditions by category
  const groupedQuickConditions = QUICK_CONDITIONS.reduce((acc, qc) => {
    if (!acc[qc.category]) acc[qc.category] = [];
    acc[qc.category].push(qc);
    return acc;
  }, {} as Record<string, QuickCondition[]>);

  return (
    <div style={{ position: 'relative' }}>
      {/* Logic toggle + label bar */}
      {label && (
        <div style={{
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Layers size={10} color={F.MUTED} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
              {label.toUpperCase()}
            </span>
          </div>
        </div>
      )}

      {/* Logic selector - shown when >1 conditions */}
      {conditions.length > 1 && (
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '4px 12px 8px',
        }}>
          <span style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>
            MATCH:
          </span>
          {(['AND', 'OR'] as const).map(l => (
            <button
              key={l}
              onClick={() => onLogicChange(l)}
              style={{
                padding: '4px 12px',
                backgroundColor: logic === l ? (l === 'AND' ? F.ORANGE : F.BLUE) : 'transparent',
                color: logic === l ? (l === 'AND' ? F.DARK_BG : F.WHITE) : F.GRAY,
                border: logic === l ? 'none' : `1px solid ${F.BORDER}`,
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                borderRadius: '2px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={e => {
                if (logic !== l) {
                  e.currentTarget.style.borderColor = F.ORANGE;
                  e.currentTarget.style.color = F.WHITE;
                }
              }}
              onMouseLeave={e => {
                if (logic !== l) {
                  e.currentTarget.style.borderColor = F.BORDER;
                  e.currentTarget.style.color = F.GRAY;
                }
              }}
            >
              {l === 'AND' ? 'ALL CONDITIONS (AND)' : 'ANY CONDITION (OR)'}
            </button>
          ))}
        </div>
      )}

      {/* Condition Cards */}
      <div style={{ padding: '0 8px' }}>
        {conditions.length === 0 && (
          <div style={{
            padding: '32px 16px',
            textAlign: 'center',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: '8px',
          }}>
            <Zap size={20} color={F.MUTED} style={{ opacity: 0.5 }} />
            <div style={{ fontSize: '10px', color: F.MUTED, fontWeight: 700 }}>
              NO CONDITIONS DEFINED
            </div>
            <div style={{ fontSize: '9px', color: F.MUTED, maxWidth: '300px', lineHeight: '1.5' }}>
              Add technical indicator conditions to define when trades should be triggered.
              Use the buttons below to add or select from templates.
            </div>
          </div>
        )}

        {conditions.map((cond, i) => (
          <div key={i}>
            {/* Logic connector between cards */}
            {i > 0 && (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '4px 0',
                position: 'relative',
              }}>
                <div style={{
                  position: 'absolute', left: '50%', top: 0, bottom: 0,
                  width: '1px', backgroundColor: F.BORDER,
                }} />
                <div style={{
                  padding: '2px 10px',
                  backgroundColor: logic === 'AND' ? F.ORANGE : F.BLUE,
                  color: logic === 'AND' ? F.DARK_BG : F.WHITE,
                  fontSize: '8px',
                  fontWeight: 700,
                  letterSpacing: '1px',
                  borderRadius: '2px',
                  position: 'relative',
                  zIndex: 1,
                }}>
                  {logic}
                </div>
              </div>
            )}

            {/* Condition Card */}
            <div style={{
              backgroundColor: F.DARK_BG,
              border: `1px solid ${dragIndex === i ? F.ORANGE : F.BORDER}`,
              borderRadius: '2px',
              overflow: 'hidden',
              transition: 'border-color 0.2s',
            }}>
              {/* Card header with number and indicator name */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                padding: '6px 10px',
                backgroundColor: F.HEADER_BG,
                borderBottom: `1px solid ${F.BORDER}`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{
                    width: '18px', height: '18px', borderRadius: '2px',
                    backgroundColor: `${F.ORANGE}20`, color: F.ORANGE,
                    fontSize: '9px', fontWeight: 700,
                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                  }}>
                    {i + 1}
                  </span>
                  <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                    {getIndicatorDef(cond.indicator)?.label || cond.indicator}
                  </span>
                  <span style={{ fontSize: '8px', color: F.MUTED }}>
                    {Object.values(cond.params).length > 0 && `(${Object.values(cond.params).join(', ')})`}
                  </span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '2px' }}>
                  {/* Move up/down */}
                  {conditions.length > 1 && (
                    <>
                      <button
                        onClick={() => handleMoveUp(i)}
                        disabled={i === 0}
                        style={{
                          padding: '2px 4px', backgroundColor: 'transparent', border: 'none',
                          color: i === 0 ? F.MUTED + '40' : F.MUTED, cursor: i === 0 ? 'default' : 'pointer',
                          fontSize: '10px', lineHeight: 1,
                        }}
                        onMouseEnter={e => { if (i > 0) e.currentTarget.style.color = F.WHITE; }}
                        onMouseLeave={e => { if (i > 0) e.currentTarget.style.color = F.MUTED; }}
                        title="Move up"
                      >
                        &#9650;
                      </button>
                      <button
                        onClick={() => handleMoveDown(i)}
                        disabled={i >= conditions.length - 1}
                        style={{
                          padding: '2px 4px', backgroundColor: 'transparent', border: 'none',
                          color: i >= conditions.length - 1 ? F.MUTED + '40' : F.MUTED,
                          cursor: i >= conditions.length - 1 ? 'default' : 'pointer',
                          fontSize: '10px', lineHeight: 1,
                        }}
                        onMouseEnter={e => { if (i < conditions.length - 1) e.currentTarget.style.color = F.WHITE; }}
                        onMouseLeave={e => { if (i < conditions.length - 1) e.currentTarget.style.color = F.MUTED; }}
                        title="Move down"
                      >
                        &#9660;
                      </button>
                    </>
                  )}
                  {/* Duplicate */}
                  <button
                    onClick={() => handleDuplicate(i)}
                    style={{
                      padding: '2px 6px', backgroundColor: 'transparent', border: 'none',
                      color: F.MUTED, cursor: 'pointer', fontSize: '9px', fontWeight: 700,
                    }}
                    onMouseEnter={e => { e.currentTarget.style.color = F.CYAN; }}
                    onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
                    title="Duplicate condition"
                  >
                    DUP
                  </button>
                  {/* Remove */}
                  <button
                    onClick={() => handleRemove(i)}
                    style={{
                      padding: '2px 6px', backgroundColor: 'transparent', border: 'none',
                      color: F.MUTED, cursor: 'pointer', fontSize: '10px', fontWeight: 700,
                    }}
                    onMouseEnter={e => { e.currentTarget.style.color = F.RED; }}
                    onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
                    title="Remove condition"
                  >
                    &#10005;
                  </button>
                </div>
              </div>

              {/* Card body with controls */}
              <ConditionRow
                condition={cond}
                index={i}
                onChange={handleChange}
                onRemove={handleRemove}
              />
            </div>
          </div>
        ))}
      </div>

      {/* ─── ADD BUTTONS ─── */}
      <div style={{
        padding: '10px 12px',
        display: 'flex',
        gap: '6px',
        alignItems: 'center',
      }}>
        <button
          onClick={() => handleAdd()}
          style={{
            padding: '8px 14px',
            backgroundColor: 'transparent',
            border: `1px dashed ${F.BORDER}`,
            color: F.GRAY,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            transition: 'all 0.2s',
            letterSpacing: '0.5px',
          }}
          onMouseEnter={e => {
            e.currentTarget.style.borderColor = F.ORANGE;
            e.currentTarget.style.color = F.ORANGE;
            e.currentTarget.style.backgroundColor = `${F.ORANGE}10`;
          }}
          onMouseLeave={e => {
            e.currentTarget.style.borderColor = F.BORDER;
            e.currentTarget.style.color = F.GRAY;
            e.currentTarget.style.backgroundColor = 'transparent';
          }}
        >
          <Plus size={11} />
          ADD CONDITION
        </button>

        <button
          onClick={() => setShowQuickAdd(!showQuickAdd)}
          style={{
            padding: '8px 14px',
            backgroundColor: showQuickAdd ? `${F.CYAN}15` : 'transparent',
            border: `1px solid ${showQuickAdd ? F.CYAN + '60' : F.BORDER}`,
            color: showQuickAdd ? F.CYAN : F.GRAY,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            transition: 'all 0.2s',
            letterSpacing: '0.5px',
          }}
          onMouseEnter={e => {
            if (!showQuickAdd) {
              e.currentTarget.style.borderColor = F.CYAN;
              e.currentTarget.style.color = F.CYAN;
            }
          }}
          onMouseLeave={e => {
            if (!showQuickAdd) {
              e.currentTarget.style.borderColor = F.BORDER;
              e.currentTarget.style.color = F.GRAY;
            }
          }}
        >
          <Sparkles size={10} />
          {showQuickAdd ? <ChevronDown size={10} /> : <ChevronRight size={10} />}
          TEMPLATES
        </button>
      </div>

      {/* ─── TEMPLATE DROPDOWN (Categorized) ─── */}
      {showQuickAdd && (
        <div style={{
          margin: '0 12px 12px',
          backgroundColor: F.DARK_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
          overflow: 'hidden',
        }}>
          {Object.entries(groupedQuickConditions).map(([category, items]) => (
            <div key={category}>
              {/* Category header */}
              <div style={{
                padding: '6px 12px',
                backgroundColor: F.HEADER_BG,
                borderBottom: `1px solid ${F.BORDER}`,
              }}>
                <span style={{
                  fontSize: '8px', fontWeight: 700, color: F.MUTED,
                  letterSpacing: '0.5px',
                }}>
                  {category.toUpperCase()}
                </span>
              </div>
              {/* Category items */}
              {items.map((qc, i) => (
                <button
                  key={i}
                  onClick={() => handleAdd(qc.condition)}
                  style={{
                    width: '100%',
                    padding: '8px 12px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    borderBottom: `1px solid ${F.BORDER}30`,
                    color: F.GRAY,
                    fontSize: '9px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    cursor: 'pointer',
                    textAlign: 'left',
                    transition: 'all 0.15s',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                  }}
                  onMouseEnter={e => {
                    e.currentTarget.style.backgroundColor = `${F.ORANGE}10`;
                    e.currentTarget.style.color = F.WHITE;
                  }}
                  onMouseLeave={e => {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = F.GRAY;
                  }}
                >
                  <Plus size={9} style={{ color: F.ORANGE, flexShrink: 0 }} />
                  {qc.label}
                </button>
              ))}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default ConditionBuilder;
