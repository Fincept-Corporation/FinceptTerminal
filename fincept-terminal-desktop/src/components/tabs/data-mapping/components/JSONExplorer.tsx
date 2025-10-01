// JSON Explorer - Interactive JSON tree viewer

import React, { useState } from 'react';
import { ChevronRight, ChevronDown, Copy, Wand2, Check } from 'lucide-react';

interface JSONExplorerProps {
  data: any;
  onFieldSelect?: (path: string[], value: any) => void;
  onGenerateExpression?: (path: string[]) => void;
  selectedPath?: string[];
}

export function JSONExplorer({ data, onFieldSelect, onGenerateExpression, selectedPath }: JSONExplorerProps) {
  const [expanded, setExpanded] = useState<Set<string>>(new Set(['/']));
  const [copied, setCopied] = useState<string | null>(null);

  const toggleExpand = (pathKey: string) => {
    const newExpanded = new Set(expanded);
    if (expanded.has(pathKey)) {
      newExpanded.delete(pathKey);
    } else {
      newExpanded.add(pathKey);
    }
    setExpanded(newExpanded);
  };

  const handleCopy = (pathKey: string) => {
    navigator.clipboard.writeText(pathKey);
    setCopied(pathKey);
    setTimeout(() => setCopied(null), 2000);
  };

  const renderValue = (value: any, path: string[], key: string): React.ReactNode => {
    const fullPath = [...path, key];
    const pathKey = fullPath.join('.');
    const isExpanded = expanded.has(pathKey);
    const isSelected = selectedPath && selectedPath.join('.') === pathKey;

    if (value === null || value === undefined) {
      return (
        <div
          className={`flex items-center gap-2 hover:bg-zinc-800 p-1 rounded cursor-pointer ${
            isSelected ? 'bg-orange-900/30' : ''
          }`}
          onClick={() => onFieldSelect?.(fullPath, value)}
        >
          <span className="text-gray-400">{key}:</span>
          <span className="text-gray-500 italic">null</span>
        </div>
      );
    }

    if (typeof value === 'object') {
      const isArray = Array.isArray(value);
      const itemCount = isArray ? value.length : Object.keys(value).length;

      return (
        <div>
          <div
            className={`flex items-center gap-2 hover:bg-zinc-800 p-1 rounded cursor-pointer ${
              isSelected ? 'bg-orange-900/30' : ''
            }`}
          >
            <button
              onClick={() => toggleExpand(pathKey)}
              className="hover:text-orange-500 transition-colors"
            >
              {isExpanded ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
            </button>

            <span className="text-orange-500 font-bold">{key}</span>
            <span className="text-gray-500 text-xs">
              {isArray ? `Array[${itemCount}]` : `Object{${itemCount}}`}
            </span>

            {/* Action buttons */}
            <div className="ml-auto flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  handleCopy(pathKey);
                }}
                className="p-1 hover:bg-zinc-700 rounded text-gray-400 hover:text-white"
                title="Copy path"
              >
                {copied === pathKey ? <Check size={12} className="text-green-500" /> : <Copy size={12} />}
              </button>
              {onGenerateExpression && (
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    onGenerateExpression(fullPath);
                  }}
                  className="p-1 hover:bg-zinc-700 rounded text-purple-400 hover:text-purple-300"
                  title="Generate expression"
                >
                  <Wand2 size={12} />
                </button>
              )}
            </div>
          </div>

          {isExpanded && (
            <div className="ml-6 border-l border-zinc-700 pl-2 mt-1">
              {isArray ? (
                // Render array items (limit to first 10 for performance)
                <>
                  {value.slice(0, 10).map((item, idx) => (
                    <div key={idx} className="group">
                      {renderValue(item, fullPath, `[${idx}]`)}
                    </div>
                  ))}
                  {value.length > 10 && (
                    <div className="text-gray-500 text-xs py-1 px-2">
                      ... and {value.length - 10} more items
                      <button
                        onClick={() => {
                          // Expand all items
                          for (let i = 10; i < value.length; i++) {
                            const itemPath = [...fullPath, `[${i}]`].join('.');
                            setExpanded(new Set([...expanded, itemPath]));
                          }
                        }}
                        className="ml-2 text-orange-500 hover:text-orange-400"
                      >
                        [show all]
                      </button>
                    </div>
                  )}
                </>
              ) : (
                // Render object properties
                Object.entries(value).map(([k, v]) => (
                  <div key={k} className="group">
                    {renderValue(v, fullPath, k)}
                  </div>
                ))
              )}
            </div>
          )}
        </div>
      );
    }

    // Primitive value
    const typeColor =
      typeof value === 'number'
        ? 'text-blue-400'
        : typeof value === 'string'
        ? 'text-green-400'
        : typeof value === 'boolean'
        ? 'text-yellow-400'
        : 'text-gray-400';

    return (
      <div
        className={`flex items-center gap-2 hover:bg-zinc-800 p-1 rounded cursor-pointer group ${
          isSelected ? 'bg-orange-900/30' : ''
        }`}
        onClick={() => onFieldSelect?.(fullPath, value)}
      >
        <span className="text-gray-400 flex-shrink-0">{key}:</span>
        <span className={`${typeColor} truncate`}>
          {typeof value === 'string' ? `"${value}"` : String(value)}
        </span>
        <span className="text-gray-600 text-xs flex-shrink-0">({typeof value})</span>

        {/* Action buttons for primitives */}
        <div className="ml-auto flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity flex-shrink-0">
          <button
            onClick={(e) => {
              e.stopPropagation();
              handleCopy(pathKey);
            }}
            className="p-1 hover:bg-zinc-700 rounded text-gray-400 hover:text-white"
            title="Copy path"
          >
            {copied === pathKey ? <Check size={12} className="text-green-500" /> : <Copy size={12} />}
          </button>
          {onGenerateExpression && (
            <button
              onClick={(e) => {
                e.stopPropagation();
                onGenerateExpression(fullPath);
              }}
              className="p-1 hover:bg-zinc-700 rounded text-purple-400 hover:text-purple-300"
              title="Generate expression"
            >
              <Wand2 size={12} />
            </button>
          )}
        </div>
      </div>
    );
  };

  if (!data) {
    return (
      <div className="bg-zinc-900 rounded p-4 text-center text-gray-500">
        No data to display. Fetch sample data to begin.
      </div>
    );
  }

  return (
    <div className="bg-zinc-900 rounded-lg overflow-hidden border border-zinc-800">
      {/* Header */}
      <div className="bg-zinc-800 px-4 py-2 border-b border-zinc-700">
        <div className="flex items-center justify-between">
          <span className="text-xs font-bold text-orange-500">JSON STRUCTURE</span>
          <div className="flex gap-2">
            <button
              onClick={() => setExpanded(new Set())}
              className="text-xs text-gray-400 hover:text-white"
            >
              Collapse All
            </button>
            <button
              onClick={() => {
                // Expand all first level
                const getAllPaths = (obj: any, prefix = ''): string[] => {
                  const paths: string[] = [];
                  if (typeof obj === 'object' && obj !== null) {
                    for (const key in obj) {
                      const path = prefix ? `${prefix}.${key}` : key;
                      paths.push(path);
                    }
                  }
                  return paths;
                };
                setExpanded(new Set(getAllPaths(data)));
              }}
              className="text-xs text-gray-400 hover:text-white"
            >
              Expand Level 1
            </button>
          </div>
        </div>
        <div className="text-xs text-gray-500 mt-1">
          Click fields to select • Click wand icon to generate expression
        </div>
      </div>

      {/* Tree View */}
      <div className="p-4 font-mono text-xs overflow-auto max-h-[600px] scrollbar-thin scrollbar-thumb-zinc-700 scrollbar-track-zinc-900">
        <div className="group">{renderValue(data, [], 'root')}</div>
      </div>
    </div>
  );
}
