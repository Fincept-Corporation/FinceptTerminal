/**
 * Node Parameter Input Component
 *
 * Universal parameter input component that renders the appropriate UI control
 * based on the parameter type from the node system.
 *
 * Supports all n8n parameter types:
 * - String, Number, Boolean
 * - Options, MultiOptions
 * - Collection, FixedCollection
 * - Color, DateTime
 * - ResourceLocator
 * - Code editor
 * - Expression support
 */

import React, { useState, useCallback } from 'react';
import type {
  INodeProperties,
  NodeParameterValue,
  NodePropertyType,
  INodePropertyOptions,
  INodePropertyCollectionValue,
} from '@/services/nodeSystem';
import { ExpressionEngine, isExpression } from '@/services/nodeSystem';
import { Check, Code2, Calendar, Hash, Type, List, Settings2, Palette } from 'lucide-react';

interface NodeParameterInputProps {
  parameter: INodeProperties;
  value: NodeParameterValue;
  onChange: (value: NodeParameterValue) => void;
  onExpressionChange?: (isExpression: boolean) => void;
  disabled?: boolean;
  itemIndex?: number;
  expressionContext?: any;
}

export const NodeParameterInput: React.FC<NodeParameterInputProps> = ({
  parameter,
  value,
  onChange,
  onExpressionChange,
  disabled = false,
  itemIndex = 0,
  expressionContext,
}) => {
  const [isExpressionMode, setIsExpressionMode] = useState(
    typeof value === 'string' && isExpression(value)
  );

  const handleToggleExpression = useCallback(() => {
    const newMode = !isExpressionMode;
    setIsExpressionMode(newMode);
    onExpressionChange?.(newMode);

    if (newMode) {
      // Convert to expression
      onChange(`={{${value}}}`);
    } else {
      // Convert back to regular value
      const strValue = String(value);
      if (strValue.startsWith('={{') && strValue.endsWith('}}')) {
        onChange(strValue.slice(3, -2));
      }
    }
  }, [isExpressionMode, value, onChange, onExpressionChange]);

  const renderInput = () => {
    // Expression mode - show code editor
    if (isExpressionMode) {
      return (
        <textarea
          value={String(value)}
          onChange={(e) => onChange(e.target.value)}
          disabled={disabled}
          placeholder="={{$json.field}}"
          className="w-full px-3 py-2 bg-gray-800 border border-yellow-500/30 rounded text-yellow-400 font-mono text-sm resize-y min-h-[60px]"
        />
      );
    }

    // Regular input based on type
    switch (parameter.type) {
      case 'string' as NodePropertyType:
        return renderStringInput();

      case 'number' as NodePropertyType:
        return renderNumberInput();

      case 'boolean' as NodePropertyType:
        return renderBooleanInput();

      case 'options' as NodePropertyType:
        return renderOptionsInput();

      case 'multiOptions' as NodePropertyType:
        return renderMultiOptionsInput();

      case 'color' as NodePropertyType:
        return renderColorInput();

      case 'dateTime' as NodePropertyType:
        return renderDateTimeInput();

      case 'json' as NodePropertyType:
      case 'code' as NodePropertyType:
        return renderCodeInput();

      case 'collection' as NodePropertyType:
        return renderCollectionInput();

      case 'fixedCollection' as NodePropertyType:
        return renderFixedCollectionInput();

      case 'resourceLocator' as NodePropertyType:
        return renderResourceLocatorInput();

      default:
        return renderStringInput();
    }
  };

  const renderStringInput = () => (
    <input
      type="text"
      value={String(value || '')}
      onChange={(e) => onChange(e.target.value)}
      disabled={disabled}
      placeholder={parameter.placeholder}
      className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm focus:border-blue-500 focus:outline-none"
    />
  );

  const renderNumberInput = () => (
    <input
      type="number"
      value={Number(value || 0)}
      onChange={(e) => onChange(Number(e.target.value))}
      disabled={disabled}
      placeholder={parameter.placeholder}
      className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm focus:border-blue-500 focus:outline-none"
    />
  );

  const renderBooleanInput = () => (
    <label className="flex items-center gap-2 cursor-pointer">
      <input
        type="checkbox"
        checked={Boolean(value)}
        onChange={(e) => onChange(e.target.checked)}
        disabled={disabled}
        className="w-4 h-4 rounded border-gray-700 bg-gray-800 text-blue-500 focus:ring-blue-500"
      />
      <span className="text-sm text-gray-300">
        {parameter.description || 'Enable'}
      </span>
    </label>
  );

  const renderOptionsInput = () => {
    const options = parameter.options as INodePropertyOptions[] | undefined;

    return (
      <select
        value={String(value || parameter.default || '')}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm focus:border-blue-500 focus:outline-none"
      >
        {options?.map((option) => (
          <option key={String(option.value)} value={String(option.value)}>
            {option.name}
          </option>
        ))}
      </select>
    );
  };

  const renderMultiOptionsInput = () => {
    const options = parameter.options as INodePropertyOptions[] | undefined;
    const selectedValues = Array.isArray(value) ? value : [];

    return (
      <div className="space-y-2">
        {options?.map((option) => (
          <label key={String(option.value)} className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={selectedValues.includes(option.value)}
              onChange={(e) => {
                const newValues = e.target.checked
                  ? [...selectedValues, option.value]
                  : selectedValues.filter((v) => v !== option.value);
                onChange(newValues);
              }}
              disabled={disabled}
              className="w-4 h-4 rounded border-gray-700 bg-gray-800 text-blue-500"
            />
            <span className="text-sm text-gray-300">{option.name}</span>
          </label>
        ))}
      </div>
    );
  };

  const renderColorInput = () => (
    <div className="flex items-center gap-2">
      <input
        type="color"
        value={String(value || '#000000')}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        className="w-12 h-10 rounded border border-gray-700 bg-gray-800 cursor-pointer"
      />
      <input
        type="text"
        value={String(value || '#000000')}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        placeholder="#000000"
        className="flex-1 px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm font-mono focus:border-blue-500 focus:outline-none"
      />
    </div>
  );

  const renderDateTimeInput = () => (
    <input
      type="datetime-local"
      value={value ? new Date(value as string | number).toISOString().slice(0, 16) : ''}
      onChange={(e) => onChange(new Date(e.target.value).toISOString())}
      disabled={disabled}
      className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm focus:border-blue-500 focus:outline-none"
    />
  );

  const renderCodeInput = () => (
    <textarea
      value={String(value || '')}
      onChange={(e) => onChange(e.target.value)}
      disabled={disabled}
      placeholder={parameter.placeholder || '// Enter code here'}
      className="w-full px-3 py-2 bg-gray-900 border border-gray-700 rounded text-green-400 font-mono text-sm resize-y min-h-[120px] focus:border-blue-500 focus:outline-none"
    />
  );

  const renderCollectionInput = () => (
    <div className="p-3 bg-gray-800/50 border border-gray-700 rounded">
      <div className="text-sm text-gray-400 mb-2">Collection (Advanced)</div>
      <textarea
        value={JSON.stringify(value, null, 2)}
        onChange={(e) => {
          try {
            onChange(JSON.parse(e.target.value));
          } catch {
            // Invalid JSON, keep as string
          }
        }}
        disabled={disabled}
        className="w-full px-3 py-2 bg-gray-900 border border-gray-700 rounded text-gray-200 font-mono text-xs resize-y min-h-[100px]"
      />
    </div>
  );

  const renderFixedCollectionInput = () => (
    <div className="p-3 bg-gray-800/50 border border-gray-700 rounded">
      <div className="text-sm text-gray-400 mb-2">Fixed Collection</div>
      <div className="text-xs text-gray-500">
        Complex input - configure in advanced editor
      </div>
    </div>
  );

  const renderResourceLocatorInput = () => {
    // ResourceLocator typically has mode selector and value input
    const resourceValue = (value as any) || { mode: 'id', value: '' };

    return (
      <div className="space-y-2">
        <select
          value={resourceValue.mode || 'id'}
          onChange={(e) => onChange({ ...resourceValue, mode: e.target.value })}
          disabled={disabled}
          className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm"
        >
          <option value="id">By ID</option>
          <option value="url">By URL</option>
          <option value="list">From List</option>
        </select>
        <input
          type="text"
          value={resourceValue.value || ''}
          onChange={(e) => onChange({ ...resourceValue, value: e.target.value })}
          disabled={disabled}
          placeholder={`Enter ${resourceValue.mode || 'ID'}...`}
          className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-gray-200 text-sm focus:border-blue-500 focus:outline-none"
        />
      </div>
    );
  };

  const getParameterIcon = () => {
    switch (parameter.type) {
      case 'string' as NodePropertyType:
        return <Type size={14} />;
      case 'number' as NodePropertyType:
        return <Hash size={14} />;
      case 'boolean' as NodePropertyType:
        return <Check size={14} />;
      case 'options' as NodePropertyType:
      case 'multiOptions' as NodePropertyType:
        return <List size={14} />;
      case 'color' as NodePropertyType:
        return <Palette size={14} />;
      case 'dateTime' as NodePropertyType:
        return <Calendar size={14} />;
      case 'json' as NodePropertyType:
      case 'code' as NodePropertyType:
        return <Code2 size={14} />;
      case 'collection' as NodePropertyType:
      case 'fixedCollection' as NodePropertyType:
        return <Settings2 size={14} />;
      default:
        return <Type size={14} />;
    }
  };

  return (
    <div className="space-y-2">
      {/* Parameter Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span className="text-gray-400">{getParameterIcon()}</span>
          <label className="text-sm font-medium text-gray-200">
            {parameter.displayName}
            {parameter.required && <span className="text-red-400 ml-1">*</span>}
          </label>
        </div>

        {/* Expression Toggle */}
        {parameter.noDataExpression !== true && (
          <button
            onClick={handleToggleExpression}
            disabled={disabled}
            className={`px-2 py-1 text-xs rounded flex items-center gap-1 transition-colors ${
              isExpressionMode
                ? 'bg-yellow-500/20 text-yellow-400 border border-yellow-500/30'
                : 'bg-gray-800 text-gray-400 border border-gray-700 hover:border-yellow-500/50'
            }`}
            title="Toggle expression mode"
          >
            <Code2 size={12} />
            {isExpressionMode ? 'Expression' : 'Fixed'}
          </button>
        )}
      </div>

      {/* Parameter Description */}
      {parameter.description && (
        <div className="text-xs text-gray-500">{parameter.description}</div>
      )}

      {/* Parameter Input */}
      {renderInput()}

      {/* Expression Hint */}
      {isExpressionMode && (
        <div className="text-xs text-yellow-500/70">
          Use expressions like: <code className="bg-gray-900 px-1 rounded">{'{{$json.field}}'}</code> or{' '}
          <code className="bg-gray-900 px-1 rounded">={'{{1 + 2}}'}</code>
        </div>
      )}

      {/* Validation Error */}
      {parameter.validateType && value && (
        <ValidationMessage parameter={parameter} value={value} />
      )}
    </div>
  );
};

/**
 * Validation Message Component
 */
const ValidationMessage: React.FC<{ parameter: INodeProperties; value: NodeParameterValue }> = ({
  parameter,
  value,
}) => {
  const { validateParameterValue } = require('@/services/nodeSystem');

  if (!parameter.validateType || parameter.ignoreValidationDuringExecution) {
    return null;
  }

  const validation = validateParameterValue(value, parameter.validateType);

  if (validation.valid) {
    return null;
  }

  return (
    <div className="text-xs text-red-400 flex items-center gap-1">
      <span>[WARN]️</span>
      <span>{validation.error}</span>
    </div>
  );
};

export default NodeParameterInput;
