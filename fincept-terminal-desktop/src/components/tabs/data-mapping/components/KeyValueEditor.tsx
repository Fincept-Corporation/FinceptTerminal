// Key-Value Editor Component - For headers and query parameters

import React, { useState } from 'react';
import { Plus, Trash2 } from 'lucide-react';

interface KeyValuePair {
  key: string;
  value: string;
}

interface KeyValueEditorProps {
  items: Record<string, string>;
  onChange: (items: Record<string, string>) => void;
  placeholder?: {
    key?: string;
    value?: string;
  };
  allowPlaceholders?: boolean;
}

export function KeyValueEditor({
  items,
  onChange,
  placeholder = { key: 'Key', value: 'Value' },
  allowPlaceholders = false,
}: KeyValueEditorProps) {
  const [pairs, setPairs] = useState<KeyValuePair[]>(() => {
    return Object.entries(items).map(([key, value]) => ({ key, value }));
  });

  const handleAdd = () => {
    setPairs([...pairs, { key: '', value: '' }]);
  };

  const handleRemove = (index: number) => {
    const newPairs = pairs.filter((_, i) => i !== index);
    setPairs(newPairs);
    emitChange(newPairs);
  };

  const handleKeyChange = (index: number, key: string) => {
    const newPairs = [...pairs];
    newPairs[index].key = key;
    setPairs(newPairs);
    emitChange(newPairs);
  };

  const handleValueChange = (index: number, value: string) => {
    const newPairs = [...pairs];
    newPairs[index].value = value;
    setPairs(newPairs);
    emitChange(newPairs);
  };

  const emitChange = (newPairs: KeyValuePair[]) => {
    const newItems: Record<string, string> = {};
    newPairs.forEach(pair => {
      if (pair.key.trim()) {
        newItems[pair.key] = pair.value;
      }
    });
    onChange(newItems);
  };

  return (
    <div className="space-y-2">
      {pairs.map((pair, index) => (
        <div key={index} className="flex gap-2 items-center">
          <input
            type="text"
            value={pair.key}
            onChange={(e) => handleKeyChange(index, e.target.value)}
            placeholder={placeholder.key}
            className="flex-1 bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500 font-mono"
          />
          <input
            type="text"
            value={pair.value}
            onChange={(e) => handleValueChange(index, e.target.value)}
            placeholder={placeholder.value}
            className="flex-1 bg-zinc-900 border border-zinc-700 text-gray-300 px-3 py-2 text-xs rounded focus:outline-none focus:border-orange-500 font-mono"
          />
          <button
            onClick={() => handleRemove(index)}
            className="p-2 text-red-500 hover:text-red-400 hover:bg-zinc-800 rounded transition-colors"
            title="Remove"
          >
            <Trash2 size={14} />
          </button>
        </div>
      ))}

      <button
        onClick={handleAdd}
        className="w-full bg-zinc-800 hover:bg-zinc-700 text-gray-400 hover:text-white py-2 rounded text-xs font-bold flex items-center justify-center gap-2 transition-colors"
      >
        <Plus size={14} />
        Add {placeholder.key || 'Item'}
      </button>

      {allowPlaceholders && (
        <div className="text-xs text-gray-500 mt-2">
          💡 Use {'{'}placeholder{'}'} syntax for dynamic values (e.g., {'{'}symbol{'}'}, {'{'}from{'}'}, {'{'}to{'}'})
        </div>
      )}
    </div>
  );
}
