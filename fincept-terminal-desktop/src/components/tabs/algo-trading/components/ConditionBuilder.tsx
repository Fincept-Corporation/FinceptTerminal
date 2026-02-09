import React from 'react';
import { Plus } from 'lucide-react';
import type { ConditionItem } from '../types';
import ConditionRow from './ConditionRow';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GRAY: '#787878', DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F', HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A',
  HOVER: '#1F1F1F', CYAN: '#00E5FF', BLUE: '#0088FF',
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

const ConditionBuilder: React.FC<ConditionBuilderProps> = ({
  label,
  conditions,
  logic,
  onConditionsChange,
  onLogicChange,
}) => {
  const handleAdd = () => {
    onConditionsChange([...conditions, { ...DEFAULT_CONDITION }]);
  };

  const handleChange = (index: number, updated: ConditionItem) => {
    const next = [...conditions];
    next[index] = updated;
    onConditionsChange(next);
  };

  const handleRemove = (index: number) => {
    onConditionsChange(conditions.filter((_, i) => i !== index));
  };

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
        <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
          {label.toUpperCase()}
        </span>
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

      {/* Add button */}
      <div style={{ padding: '4px 8px 8px' }}>
        <button
          onClick={handleAdd}
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
      </div>
    </div>
  );
};

export default ConditionBuilder;
