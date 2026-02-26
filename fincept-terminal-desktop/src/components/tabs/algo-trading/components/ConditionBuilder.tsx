import React, { useState } from 'react';
import { Sparkles, Zap } from 'lucide-react';
import type { ConditionItem } from '../types';
import ConditionRow from './ConditionRow';
import { F } from '../constants/theme';

interface ConditionBuilderProps {
  label: string;
  conditions: ConditionItem[];
  logic: 'AND' | 'OR';
  onConditionsChange: (conditions: ConditionItem[]) => void;
  onLogicChange: (logic: 'AND' | 'OR') => void;
  selectedIndex?: number;
}

const font = '"IBM Plex Mono", "Consolas", monospace';

const DEFAULT_CONDITION: ConditionItem = {
  indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'crosses_below', value: 30,
};

interface QuickCondition { label: string; category: string; condition: ConditionItem; }

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
  conditions, onConditionsChange, selectedIndex,
}) => {
  const [showTemplates, setShowTemplates] = useState(false);
  const idx = selectedIndex ?? 0;
  const cond = conditions[idx];

  const handleChange = (index: number, updated: ConditionItem) => {
    const next = [...conditions];
    next[index] = updated;
    onConditionsChange(next);
  };

  const handleApplyTemplate = (template: ConditionItem) => {
    const clone = JSON.parse(JSON.stringify(template));
    const next = [...conditions];
    next[idx] = clone;
    onConditionsChange(next);
    setShowTemplates(false);
  };

  const groupedTemplates = QUICK_CONDITIONS.reduce((acc, qc) => {
    if (!acc[qc.category]) acc[qc.category] = [];
    acc[qc.category].push(qc);
    return acc;
  }, {} as Record<string, QuickCondition[]>);

  if (!cond) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '10px', textAlign: 'center' }}>
        <Zap size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
        <span>Select a condition from the left panel</span>
      </div>
    );
  }

  return (
    <div style={{ fontFamily: font }}>
      {/* Template bar */}
      <div style={{ padding: '6px 12px', borderBottom: `1px solid ${F.BORDER}`, display: 'flex', alignItems: 'center', gap: '8px', backgroundColor: F.HEADER_BG }}>
        <button
          onClick={() => setShowTemplates(!showTemplates)}
          style={{
            padding: '4px 8px', backgroundColor: showTemplates ? `${F.CYAN}15` : 'transparent',
            borderWidth: '1px', borderStyle: 'solid', borderColor: showTemplates ? F.CYAN + '50' : F.BORDER,
            color: showTemplates ? F.CYAN : F.GRAY,
            fontSize: '9px', fontWeight: 700, borderRadius: '2px', cursor: 'pointer',
            display: 'flex', alignItems: 'center', gap: '4px', fontFamily: font, transition: 'all 0.2s',
          }}
          onMouseEnter={e => { if (!showTemplates) { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; } }}
          onMouseLeave={e => { if (!showTemplates) { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; } }}
        >
          <Sparkles size={10} /> TEMPLATES
        </button>
        <span style={{ fontSize: '9px', color: F.MUTED }}>Replace current condition with a preset</span>
      </div>

      {/* Template dropdown */}
      {showTemplates && (
        <div style={{ borderBottom: `1px solid ${F.BORDER}`, padding: '8px', backgroundColor: F.DARK_BG }}>
          {Object.entries(groupedTemplates).map(([category, items]) => (
            <div key={category} style={{ marginBottom: '6px' }}>
              <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '4px', padding: '0 4px' }}>{category.toUpperCase()}</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
                {items.map((qc, qi) => (
                  <button
                    key={qi}
                    onClick={() => handleApplyTemplate(qc.condition)}
                    style={{
                      padding: '6px 8px', backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                      color: F.GRAY, fontSize: '9px', fontFamily: font, textAlign: 'left',
                      borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s',
                    }}
                    onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
                    onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
                  >
                    {qc.label}
                  </button>
                ))}
              </div>
            </div>
          ))}
        </div>
      )}

      {/* Condition editor */}
      <ConditionRow
        condition={cond}
        index={idx}
        onChange={handleChange}
        onRemove={() => {}}
      />
    </div>
  );
};

export default ConditionBuilder;
