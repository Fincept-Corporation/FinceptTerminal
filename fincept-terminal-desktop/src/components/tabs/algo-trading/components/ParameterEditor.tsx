import React, { useState, useEffect } from 'react';
import { Settings, Loader2, AlertCircle, RefreshCw, Code, Hash, ToggleLeft, Type } from 'lucide-react';
import type { StrategyParameter } from '../types';
import { extractStrategyParameters } from '../services/algoTradingService';
import { F } from '../constants/theme';

const inputBaseStyle: React.CSSProperties = {
  width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
  color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
  fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none',
  transition: 'border-color 0.2s',
};

interface ParameterEditorProps {
  code?: string;
  parameters?: StrategyParameter[];
  values: Record<string, string>;
  onChange: (values: Record<string, string>) => void;
  disabled?: boolean;
}

const ParameterEditor: React.FC<ParameterEditorProps> = ({
  code,
  parameters: providedParameters,
  values,
  onChange,
  disabled = false,
}) => {
  const [parameters, setParameters] = useState<StrategyParameter[]>(providedParameters || []);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (providedParameters) {
      setParameters(providedParameters);
      return;
    }
    if (code) extractParametersFromCode(code);
  }, [code, providedParameters]);

  const extractParametersFromCode = async (sourceCode: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await extractStrategyParameters(sourceCode);
      if (result.success && result.data) {
        setParameters(result.data);
        const newValues = { ...values };
        result.data.forEach((param) => {
          if (!(param.name in newValues)) newValues[param.name] = param.default_value;
        });
        onChange(newValues);
      } else {
        setError(result.error || 'Failed to extract parameters');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  const handleValueChange = (name: string, value: string) => {
    onChange({ ...values, [name]: value });
  };

  const handleReset = () => {
    const defaultValues: Record<string, string> = {};
    parameters.forEach((param) => { defaultValues[param.name] = param.default_value; });
    onChange(defaultValues);
  };

  if (loading) {
    return (
      <div style={{
        padding: '24px', display: 'flex', flexDirection: 'column',
        alignItems: 'center', gap: '8px', color: F.GRAY,
      }}>
        <Loader2 size={18} className="animate-spin" style={{ color: F.ORANGE }} />
        <span style={{ fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px' }}>
          EXTRACTING PARAMETERS...
        </span>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        padding: '12px', backgroundColor: `${F.RED}15`,
        border: `1px solid ${F.RED}40`, borderRadius: '2px',
        display: 'flex', alignItems: 'center', gap: '8px',
      }}>
        <AlertCircle size={12} color={F.RED} />
        <span style={{ fontSize: '9px', color: F.RED, fontWeight: 700 }}>{error}</span>
      </div>
    );
  }

  if (parameters.length === 0) {
    return (
      <div style={{
        padding: '24px', textAlign: 'center',
        display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '6px',
      }}>
        <Code size={18} color={F.MUTED} style={{ opacity: 0.5 }} />
        <span style={{ fontSize: '9px', color: F.MUTED, fontWeight: 700 }}>
          NO CONFIGURABLE PARAMETERS
        </span>
      </div>
    );
  }

  return (
    <div style={{
      backgroundColor: F.PANEL_BG, borderRadius: '2px',
      border: `1px solid ${F.BORDER}`, overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: '10px 12px', borderBottom: `1px solid ${F.BORDER}`,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        backgroundColor: F.HEADER_BG,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Settings size={12} color={F.ORANGE} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
            STRATEGY PARAMETERS
          </span>
          <span style={{
            padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            backgroundColor: `${F.ORANGE}20`, color: F.ORANGE,
          }}>
            {parameters.length}
          </span>
        </div>
        <button
          onClick={handleReset}
          disabled={disabled}
          style={{
            padding: '4px 10px', backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`, borderRadius: '2px',
            color: F.GRAY, fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
            cursor: disabled ? 'not-allowed' : 'pointer',
            display: 'flex', alignItems: 'center', gap: '4px',
            opacity: disabled ? 0.5 : 1, transition: 'all 0.2s',
          }}
          onMouseEnter={e => {
            if (!disabled) {
              e.currentTarget.style.borderColor = F.ORANGE;
              e.currentTarget.style.color = F.WHITE;
            }
          }}
          onMouseLeave={e => {
            if (!disabled) {
              e.currentTarget.style.borderColor = F.BORDER;
              e.currentTarget.style.color = F.GRAY;
            }
          }}
        >
          <RefreshCw size={9} />
          RESET DEFAULTS
        </button>
      </div>

      {/* Parameters Grid */}
      <div style={{
        padding: '12px',
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, minmax(220px, 1fr))',
        gap: '10px',
      }}>
        {parameters.map((param) => (
          <ParameterInput
            key={param.name}
            parameter={param}
            value={values[param.name] ?? param.default_value}
            onChange={(value) => handleValueChange(param.name, value)}
            disabled={disabled}
          />
        ))}
      </div>
    </div>
  );
};

// Type icon mapping
const TYPE_ICONS: Record<string, React.ElementType> = {
  'int': Hash,
  'float': Hash,
  'bool': ToggleLeft,
  'string': Type,
};

const TYPE_COLORS: Record<string, string> = {
  'int': F.CYAN,
  'float': F.BLUE,
  'bool': F.GREEN,
  'string': F.YELLOW,
};

// Parameter Input Component
const ParameterInput: React.FC<{
  parameter: StrategyParameter;
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
}> = ({ parameter, value, onChange, disabled }) => {
  const TypeIcon = TYPE_ICONS[parameter.type] || Code;
  const typeColor = TYPE_COLORS[parameter.type] || F.GRAY;

  const renderInput = () => {
    const baseStyle = { ...inputBaseStyle, cursor: disabled ? 'not-allowed' : undefined };

    switch (parameter.type) {
      case 'bool':
        return (
          <select
            value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            style={{ ...baseStyle, color: value === 'true' ? F.GREEN : F.RED }}
          >
            <option value="true">True</option>
            <option value="false">False</option>
          </select>
        );
      case 'int':
        return (
          <input
            type="number" value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            min={parameter.min} max={parameter.max} step={1}
            style={{ ...baseStyle, color: F.CYAN }}
            onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
          />
        );
      case 'float':
        return (
          <input
            type="number" value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            min={parameter.min} max={parameter.max} step={0.01}
            style={{ ...baseStyle, color: F.BLUE }}
            onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
          />
        );
      default:
        return (
          <input
            type="text" value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            style={baseStyle}
            onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
          />
        );
    }
  };

  return (
    <div style={{
      backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
      borderRadius: '2px', padding: '10px', transition: 'border-color 0.2s',
    }}>
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        marginBottom: '6px',
      }}>
        <label style={{
          fontSize: '9px', fontWeight: 700, color: F.GRAY,
          letterSpacing: '0.5px', display: 'flex', alignItems: 'center', gap: '4px',
        }}>
          {parameter.name.toUpperCase()}
        </label>
        <span style={{
          fontSize: '8px', padding: '2px 6px',
          backgroundColor: `${typeColor}15`, color: typeColor,
          borderRadius: '2px', fontWeight: 700, letterSpacing: '0.5px',
          display: 'flex', alignItems: 'center', gap: '3px',
        }}>
          <TypeIcon size={8} />
          {parameter.type.toUpperCase()}
        </span>
      </div>
      {renderInput()}
      {parameter.description && (
        <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '4px', lineHeight: '1.4' }}>
          {parameter.description}
        </div>
      )}
      {(parameter.min !== undefined || parameter.max !== undefined) && (
        <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
          {parameter.min !== undefined && `Min: ${parameter.min}`}
          {parameter.min !== undefined && parameter.max !== undefined && ' | '}
          {parameter.max !== undefined && `Max: ${parameter.max}`}
        </div>
      )}
    </div>
  );
};

export default ParameterEditor;
