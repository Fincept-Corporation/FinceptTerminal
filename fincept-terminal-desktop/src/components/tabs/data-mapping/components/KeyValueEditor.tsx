// Key-Value Editor - Terminal UI/UX

import React, { useState } from 'react';
import { Plus, X } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface KeyValuePair {
  key: string;
  value: string;
}

interface KeyValueEditorProps {
  items: Record<string, string>;
  onChange: (items: Record<string, string>) => void;
  placeholder?: { key?: string; value?: string };
  allowPlaceholders?: boolean;
}

export function KeyValueEditor({
  items,
  onChange,
  placeholder = { key: 'Key', value: 'Value' },
  allowPlaceholders = false,
}: KeyValueEditorProps) {
  const { colors } = useTerminalTheme();
  const [pairs, setPairs] = useState<KeyValuePair[]>(() =>
    Object.entries(items).map(([key, value]) => ({ key, value }))
  );
  const [focusedCell, setFocusedCell] = useState<string | null>(null);

  const borderColor = 'var(--ft-border-color, #2A2A2A)';

  const handleAdd = () => {
    setPairs([...pairs, { key: '', value: '' }]);
  };

  const handleRemove = (index: number) => {
    const next = pairs.filter((_, i) => i !== index);
    setPairs(next);
    emit(next);
  };

  const handleKeyChange = (index: number, key: string) => {
    const next = [...pairs]; next[index].key = key; setPairs(next); emit(next);
  };

  const handleValueChange = (index: number, value: string) => {
    const next = [...pairs]; next[index].value = value; setPairs(next); emit(next);
  };

  const emit = (newPairs: KeyValuePair[]) => {
    const result: Record<string, string> = {};
    newPairs.forEach(p => { if (p.key.trim()) result[p.key] = p.value; });
    onChange(result);
  };

  return (
    <div className="space-y-1.5">
      {/* Column headers */}
      {pairs.length > 0 && (
        <div className="grid grid-cols-[1fr_1fr_24px] gap-1 px-1">
          <div className="text-[10px] font-bold tracking-wider" style={{ color: colors.textMuted }}>KEY</div>
          <div className="text-[10px] font-bold tracking-wider" style={{ color: colors.textMuted }}>VALUE</div>
          <div />
        </div>
      )}

      {/* Pairs */}
      {pairs.map((pair, index) => (
        <div key={index} className="grid grid-cols-[1fr_1fr_24px] gap-1 items-center">
          <input
            type="text"
            value={pair.key}
            onChange={(e) => handleKeyChange(index, e.target.value)}
            placeholder={placeholder.key}
            className="px-2 py-1.5 text-xs font-mono w-full transition-all outline-none"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${focusedCell === `k${index}` ? colors.primary : borderColor}`,
              color: colors.text,
            }}
            onFocus={() => setFocusedCell(`k${index}`)}
            onBlur={() => setFocusedCell(null)}
          />
          <input
            type="text"
            value={pair.value}
            onChange={(e) => handleValueChange(index, e.target.value)}
            placeholder={placeholder.value}
            className="px-2 py-1.5 text-xs font-mono w-full transition-all outline-none"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${focusedCell === `v${index}` ? colors.primary : borderColor}`,
              color: colors.text,
            }}
            onFocus={() => setFocusedCell(`v${index}`)}
            onBlur={() => setFocusedCell(null)}
          />
          <button
            onClick={() => handleRemove(index)}
            className="flex items-center justify-center w-6 h-6 transition-all"
            style={{ color: colors.textMuted }}
            onMouseEnter={(e) => { e.currentTarget.style.color = colors.alert; e.currentTarget.style.backgroundColor = `${colors.alert}10`; }}
            onMouseLeave={(e) => { e.currentTarget.style.color = colors.textMuted; e.currentTarget.style.backgroundColor = 'transparent'; }}
            title="Remove"
          >
            <X size={12} />
          </button>
        </div>
      ))}

      {/* Add Button */}
      <button
        onClick={handleAdd}
        className="w-full flex items-center justify-center gap-1.5 py-1.5 text-[11px] font-bold tracking-wide transition-all"
        style={{
          backgroundColor: 'transparent',
          border: `1px dashed ${borderColor}`,
          color: colors.textMuted,
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.borderColor = colors.primary;
          e.currentTarget.style.color = colors.primary;
          e.currentTarget.style.backgroundColor = `${colors.primary}05`;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.borderColor = borderColor;
          e.currentTarget.style.color = colors.textMuted;
          e.currentTarget.style.backgroundColor = 'transparent';
        }}
      >
        <Plus size={12} />
        ADD {(placeholder.key || 'ITEM').toUpperCase()}
      </button>

      {/* Placeholder hint */}
      {allowPlaceholders && pairs.length > 0 && (
        <div className="text-[10px] font-mono px-1" style={{ color: colors.textMuted }}>
          Tip: Use{' '}
          <span style={{ color: colors.primary }} className="font-bold">{'{'+'placeholder'+'}'}</span>
          {' '}for dynamic values (e.g., {'{'+'symbol'+'}'}, {'{'+'from'+'}'})
        </div>
      )}
    </div>
  );
}
