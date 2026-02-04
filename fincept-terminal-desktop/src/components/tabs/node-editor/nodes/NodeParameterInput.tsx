/**
 * Node Parameter Input Component - Fincept Terminal Style
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
} from '@/services/nodeSystem';
import { isExpression } from '@/services/nodeSystem';
import { Check, Code2, Calendar, Hash, Type, List, Settings2, Palette, ChevronDown } from 'lucide-react';

// Fincept Terminal Style Constants
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
  GRAY: '#BBBBBB',
  MUTED: '#888888',
  DARK_BG: '#000000',
  PANEL_BG: '#141414',
  INPUT_BG: '#1A1A1A',
  BORDER: '#3A3A3A',
  FONT: '"IBM Plex Mono", "Consolas", monospace',
};

const inputBaseStyle: React.CSSProperties = {
  width: '100%',
  backgroundColor: FINCEPT.INPUT_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  padding: '10px 12px',
  color: FINCEPT.WHITE,
  fontSize: '13px',
  fontFamily: FINCEPT.FONT,
  outline: 'none',
  transition: 'all 0.15s ease',
};

const labelStyle: React.CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  gap: '8px',
  color: FINCEPT.GRAY,
  fontSize: '12px',
  fontWeight: 600,
  marginBottom: '8px',
  textTransform: 'uppercase',
  letterSpacing: '0.5px',
};

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
}) => {
  const [isExpressionMode, setIsExpressionMode] = useState(
    typeof value === 'string' && isExpression(value)
  );
  const [isFocused, setIsFocused] = useState(false);

  const handleToggleExpression = useCallback(() => {
    const newMode = !isExpressionMode;
    setIsExpressionMode(newMode);
    onExpressionChange?.(newMode);

    if (newMode) {
      onChange(`={{${value}}}`);
    } else {
      const strValue = String(value);
      if (strValue.startsWith('={{') && strValue.endsWith('}}')) {
        onChange(strValue.slice(3, -2));
      }
    }
  }, [isExpressionMode, value, onChange, onExpressionChange]);

  const getFocusStyle = (): React.CSSProperties => ({
    borderColor: isFocused ? FINCEPT.ORANGE : FINCEPT.BORDER,
    boxShadow: isFocused ? `0 0 8px ${FINCEPT.ORANGE}30` : 'none',
    backgroundColor: isFocused ? FINCEPT.PANEL_BG : FINCEPT.INPUT_BG,
  });

  const renderInput = () => {
    if (isExpressionMode) {
      return (
        <textarea
          value={String(value)}
          onChange={(e) => onChange(e.target.value)}
          disabled={disabled}
          placeholder="={{$json.field}}"
          onFocus={() => setIsFocused(true)}
          onBlur={() => setIsFocused(false)}
          style={{
            ...inputBaseStyle,
            ...getFocusStyle(),
            color: FINCEPT.YELLOW,
            minHeight: '60px',
            resize: 'vertical',
          }}
        />
      );
    }

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
      placeholder={parameter.placeholder || 'Enter value...'}
      onFocus={() => setIsFocused(true)}
      onBlur={() => setIsFocused(false)}
      style={{
        ...inputBaseStyle,
        ...getFocusStyle(),
      }}
    />
  );

  const renderNumberInput = () => (
    <div style={{ display: 'flex', alignItems: 'stretch' }}>
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${isFocused ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
        borderRight: 'none',
        padding: '0 10px',
        display: 'flex',
        alignItems: 'center',
        color: FINCEPT.ORANGE,
        fontSize: '13px',
        transition: 'all 0.15s ease',
      }}>
        <Hash size={14} />
      </div>
      <input
        type="text"
        inputMode="decimal"
        value={String(value || '')}
        onChange={(e) => {
          const v = e.target.value;
          if (v === '' || /^\d*\.?\d*$/.test(v)) {
            onChange(v);
          }
        }}
        disabled={disabled}
        placeholder={parameter.placeholder}
        onFocus={() => setIsFocused(true)}
        onBlur={() => setIsFocused(false)}
        style={{
          ...inputBaseStyle,
          ...getFocusStyle(),
          borderLeft: 'none',
          flex: 1,
        }}
      />
    </div>
  );

  const renderBooleanInput = () => (
    <div
      onClick={() => !disabled && onChange(!Boolean(value))}
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '10px',
        padding: '8px 10px',
        backgroundColor: FINCEPT.INPUT_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        cursor: disabled ? 'not-allowed' : 'pointer',
        transition: 'all 0.15s ease',
      }}
      onMouseEnter={(e) => {
        if (!disabled) e.currentTarget.style.borderColor = FINCEPT.ORANGE;
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.borderColor = FINCEPT.BORDER;
      }}
    >
      <div style={{
        width: '32px',
        height: '16px',
        backgroundColor: Boolean(value) ? FINCEPT.GREEN : FINCEPT.MUTED,
        borderRadius: '8px',
        position: 'relative',
        transition: 'all 0.15s ease',
      }}>
        <div style={{
          position: 'absolute',
          top: '2px',
          left: Boolean(value) ? '16px' : '2px',
          width: '12px',
          height: '12px',
          backgroundColor: FINCEPT.WHITE,
          borderRadius: '50%',
          transition: 'all 0.15s ease',
        }} />
      </div>
      <span style={{
        color: Boolean(value) ? FINCEPT.GREEN : FINCEPT.GRAY,
        fontSize: '13px',
        fontFamily: FINCEPT.FONT,
        fontWeight: 600,
        textTransform: 'uppercase',
      }}>
        {Boolean(value) ? 'ENABLED' : 'DISABLED'}
      </span>
    </div>
  );

  const renderOptionsInput = () => {
    const options = parameter.options as INodePropertyOptions[] | undefined;

    return (
      <div style={{ position: 'relative' }}>
        <select
          value={String(value || parameter.default || '')}
          onChange={(e) => onChange(e.target.value)}
          disabled={disabled}
          onFocus={() => setIsFocused(true)}
          onBlur={() => setIsFocused(false)}
          style={{
            ...inputBaseStyle,
            ...getFocusStyle(),
            appearance: 'none',
            paddingRight: '28px',
            cursor: 'pointer',
          }}
        >
          {options?.map((option) => (
            <option
              key={String(option.value)}
              value={String(option.value)}
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
              }}
            >
              {option.name}
            </option>
          ))}
        </select>
        <ChevronDown
          size={16}
          style={{
            position: 'absolute',
            right: '12px',
            top: '50%',
            transform: 'translateY(-50%)',
            color: FINCEPT.ORANGE,
            pointerEvents: 'none',
          }}
        />
      </div>
    );
  };

  const renderMultiOptionsInput = () => {
    const options = parameter.options as INodePropertyOptions[] | undefined;
    const selectedValues = Array.isArray(value) ? value : [];

    return (
      <div style={{
        backgroundColor: FINCEPT.INPUT_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        padding: '8px',
      }}>
        {options?.map((option) => (
          <div
            key={String(option.value)}
            onClick={() => {
              if (disabled) return;
              const newValues = selectedValues.includes(option.value)
                ? selectedValues.filter((v) => v !== option.value)
                : [...selectedValues, option.value];
              onChange(newValues);
            }}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '6px 8px',
              cursor: disabled ? 'not-allowed' : 'pointer',
              backgroundColor: selectedValues.includes(option.value) ? `${FINCEPT.ORANGE}15` : 'transparent',
              marginBottom: '4px',
              transition: 'all 0.15s ease',
            }}
          >
            <div style={{
              width: '18px',
              height: '18px',
              border: `2px solid ${selectedValues.includes(option.value) ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              backgroundColor: selectedValues.includes(option.value) ? FINCEPT.ORANGE : 'transparent',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
            }}>
              {selectedValues.includes(option.value) && (
                <Check size={14} style={{ color: FINCEPT.DARK_BG }} />
              )}
            </div>
            <span style={{
              color: selectedValues.includes(option.value) ? FINCEPT.WHITE : FINCEPT.GRAY,
              fontSize: '13px',
              fontFamily: FINCEPT.FONT,
            }}>
              {option.name}
            </span>
          </div>
        ))}
      </div>
    );
  };

  const renderColorInput = () => (
    <div style={{ display: 'flex', alignItems: 'stretch', gap: '8px' }}>
      <input
        type="color"
        value={String(value || '#FF8800')}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        style={{
          width: '40px',
          height: '32px',
          border: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.INPUT_BG,
          cursor: 'pointer',
          padding: '2px',
        }}
      />
      <input
        type="text"
        value={String(value || '#FF8800')}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        placeholder="#FF8800"
        onFocus={() => setIsFocused(true)}
        onBlur={() => setIsFocused(false)}
        style={{
          ...inputBaseStyle,
          ...getFocusStyle(),
          flex: 1,
          textTransform: 'uppercase',
        }}
      />
    </div>
  );

  const renderDateTimeInput = () => (
    <div style={{ display: 'flex', alignItems: 'stretch' }}>
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${isFocused ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
        borderRight: 'none',
        padding: '0 10px',
        display: 'flex',
        alignItems: 'center',
        color: FINCEPT.ORANGE,
        transition: 'all 0.15s ease',
      }}>
        <Calendar size={14} />
      </div>
      <input
        type="datetime-local"
        value={value ? new Date(value as string | number).toISOString().slice(0, 16) : ''}
        onChange={(e) => onChange(new Date(e.target.value).toISOString())}
        disabled={disabled}
        onFocus={() => setIsFocused(true)}
        onBlur={() => setIsFocused(false)}
        style={{
          ...inputBaseStyle,
          ...getFocusStyle(),
          borderLeft: 'none',
          flex: 1,
        }}
      />
    </div>
  );

  const renderCodeInput = () => (
    <textarea
      value={String(value || '')}
      onChange={(e) => onChange(e.target.value)}
      disabled={disabled}
      placeholder={parameter.placeholder || '// Enter code here'}
      onFocus={() => setIsFocused(true)}
      onBlur={() => setIsFocused(false)}
      style={{
        ...inputBaseStyle,
        ...getFocusStyle(),
        color: FINCEPT.GREEN,
        minHeight: '100px',
        resize: 'vertical',
        lineHeight: '1.5',
      }}
    />
  );

  const renderCollectionInput = () => (
    <div style={{
      backgroundColor: FINCEPT.INPUT_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      padding: '12px',
    }}>
      <div style={{
        color: FINCEPT.CYAN,
        fontSize: '12px',
        fontWeight: 700,
        marginBottom: '10px',
        textTransform: 'uppercase',
      }}>
        COLLECTION (JSON)
      </div>
      <textarea
        value={JSON.stringify(value, null, 2)}
        onChange={(e) => {
          try {
            onChange(JSON.parse(e.target.value));
          } catch {
            // Invalid JSON
          }
        }}
        disabled={disabled}
        onFocus={() => setIsFocused(true)}
        onBlur={() => setIsFocused(false)}
        style={{
          ...inputBaseStyle,
          ...getFocusStyle(),
          minHeight: '80px',
          resize: 'vertical',
          fontSize: '12px',
        }}
      />
    </div>
  );

  const renderFixedCollectionInput = () => (
    <div style={{
      backgroundColor: FINCEPT.INPUT_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      padding: '14px',
      textAlign: 'center',
    }}>
      <Settings2 size={20} style={{ color: FINCEPT.MUTED, margin: '0 auto 10px' }} />
      <div style={{
        color: FINCEPT.GRAY,
        fontSize: '12px',
        textTransform: 'uppercase',
      }}>
        ADVANCED CONFIGURATION
      </div>
      <div style={{ color: FINCEPT.MUTED, fontSize: '11px', marginTop: '6px' }}>
        Configure in advanced editor
      </div>
    </div>
  );

  const renderResourceLocatorInput = () => {
    const resourceValue = (value as any) || { mode: 'id', value: '' };

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <div style={{ position: 'relative' }}>
          <select
            value={resourceValue.mode || 'id'}
            onChange={(e) => onChange({ ...resourceValue, mode: e.target.value })}
            disabled={disabled}
            style={{
              ...inputBaseStyle,
              appearance: 'none',
              paddingRight: '28px',
              cursor: 'pointer',
            }}
          >
            <option value="id" style={{ backgroundColor: FINCEPT.DARK_BG }}>BY ID</option>
            <option value="url" style={{ backgroundColor: FINCEPT.DARK_BG }}>BY URL</option>
            <option value="list" style={{ backgroundColor: FINCEPT.DARK_BG }}>FROM LIST</option>
          </select>
          <ChevronDown
            size={16}
            style={{
              position: 'absolute',
              right: '12px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: FINCEPT.ORANGE,
              pointerEvents: 'none',
            }}
          />
        </div>
        <input
          type="text"
          value={resourceValue.value || ''}
          onChange={(e) => onChange({ ...resourceValue, value: e.target.value })}
          disabled={disabled}
          placeholder={`Enter ${resourceValue.mode || 'ID'}...`}
          onFocus={() => setIsFocused(true)}
          onBlur={() => setIsFocused(false)}
          style={{
            ...inputBaseStyle,
            ...getFocusStyle(),
          }}
        />
      </div>
    );
  };

  const getParameterIcon = () => {
    const iconStyle = { color: FINCEPT.ORANGE };
    switch (parameter.type) {
      case 'string' as NodePropertyType:
        return <Type size={14} style={iconStyle} />;
      case 'number' as NodePropertyType:
        return <Hash size={14} style={iconStyle} />;
      case 'boolean' as NodePropertyType:
        return <Check size={14} style={iconStyle} />;
      case 'options' as NodePropertyType:
      case 'multiOptions' as NodePropertyType:
        return <List size={14} style={iconStyle} />;
      case 'color' as NodePropertyType:
        return <Palette size={14} style={iconStyle} />;
      case 'dateTime' as NodePropertyType:
        return <Calendar size={14} style={iconStyle} />;
      case 'json' as NodePropertyType:
      case 'code' as NodePropertyType:
        return <Code2 size={14} style={iconStyle} />;
      case 'collection' as NodePropertyType:
      case 'fixedCollection' as NodePropertyType:
        return <Settings2 size={14} style={iconStyle} />;
      default:
        return <Type size={14} style={iconStyle} />;
    }
  };

  return (
    <div style={{ marginBottom: '4px' }}>
      {/* Parameter Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '6px',
      }}>
        <div style={labelStyle}>
          {getParameterIcon()}
          <span>{parameter.displayName}</span>
          {parameter.required && (
            <span style={{ color: FINCEPT.RED, marginLeft: '2px' }}>*</span>
          )}
        </div>

        {/* Expression Toggle */}
        {parameter.noDataExpression !== true && (
          <button
            onClick={handleToggleExpression}
            disabled={disabled}
            style={{
              padding: '4px 8px',
              fontSize: '10px',
              fontFamily: FINCEPT.FONT,
              fontWeight: 600,
              border: `1px solid ${isExpressionMode ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
              backgroundColor: isExpressionMode ? `${FINCEPT.YELLOW}20` : 'transparent',
              color: isExpressionMode ? FINCEPT.YELLOW : FINCEPT.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              textTransform: 'uppercase',
              transition: 'all 0.15s ease',
            }}
          >
            <Code2 size={12} />
            {isExpressionMode ? 'EXPR' : 'FIXED'}
          </button>
        )}
      </div>

      {/* Parameter Description */}
      {parameter.description && (
        <div style={{
          fontSize: '11px',
          color: FINCEPT.MUTED,
          marginBottom: '8px',
          lineHeight: '1.5',
        }}>
          {parameter.description}
        </div>
      )}

      {/* Parameter Input */}
      {renderInput()}

      {/* Expression Hint */}
      {isExpressionMode && (
        <div style={{
          fontSize: '8px',
          color: `${FINCEPT.YELLOW}90`,
          marginTop: '4px',
          fontFamily: FINCEPT.FONT,
        }}>
          SYNTAX: <code style={{ backgroundColor: FINCEPT.PANEL_BG, padding: '1px 4px' }}>{'{{$json.field}}'}</code>
        </div>
      )}
    </div>
  );
};

export default NodeParameterInput;
