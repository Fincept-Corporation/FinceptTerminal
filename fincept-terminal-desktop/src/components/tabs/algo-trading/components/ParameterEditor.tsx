import React, { useState, useEffect } from 'react';
import { Settings, Loader2, AlertCircle, RefreshCw, Code, Hash, ToggleLeft, Type } from 'lucide-react';
import type { StrategyParameter } from '../types';
import { extractStrategyParameters } from '../services/algoTradingService';
import { F } from '../constants/theme';
import { S } from '../constants/styles';

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
      <div className="flex flex-col items-center gap-2 py-6" style={{ color: F.GRAY }}>
        <Loader2 size={20} className="animate-spin" style={{ color: F.ORANGE }} />
        <span className="text-[11px] font-bold tracking-wide">EXTRACTING PARAMETERS...</span>
      </div>
    );
  }

  if (error) {
    return (
      <div
        className="flex items-center gap-2 rounded px-4 py-3"
        style={{ backgroundColor: `${F.RED}15`, border: `1px solid ${F.RED}40` }}
      >
        <AlertCircle size={14} color={F.RED} />
        <span className="text-[11px] font-bold" style={{ color: F.RED }}>{error}</span>
      </div>
    );
  }

  if (parameters.length === 0) {
    return (
      <div className="flex flex-col items-center gap-2 py-6">
        <Code size={20} style={{ color: F.MUTED, opacity: 0.5 }} />
        <span className="text-[11px] font-bold" style={{ color: F.MUTED }}>
          NO CONFIGURABLE PARAMETERS
        </span>
      </div>
    );
  }

  return (
    <div className={S.section} style={{ backgroundColor: F.PANEL_BG }}>
      {/* Header */}
      <div
        className={`${S.sectionHeader} justify-between`}
        style={{ backgroundColor: F.HEADER_BG }}
      >
        <div className="flex items-center gap-2">
          <Settings size={14} color={F.ORANGE} />
          <span className="text-[11px] font-bold tracking-wide" style={{ color: F.WHITE }}>
            STRATEGY PARAMETERS
          </span>
          <span
            className={S.badge}
            style={{ backgroundColor: `${F.ORANGE}20`, color: F.ORANGE }}
          >
            {parameters.length}
          </span>
        </div>
        <button
          onClick={handleReset}
          disabled={disabled}
          className={S.btnOutline}
          style={{ opacity: disabled ? 0.5 : 1, cursor: disabled ? 'not-allowed' : 'pointer' }}
          onMouseEnter={e => {
            if (!disabled) { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }
          }}
          onMouseLeave={e => {
            if (!disabled) { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }
          }}
        >
          <RefreshCw size={10} />
          RESET DEFAULTS
        </button>
      </div>

      {/* Parameters Grid */}
      <div
        className="p-4 grid gap-3"
        style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(240px, 1fr))' }}
      >
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
    switch (parameter.type) {
      case 'bool':
        return (
          <select
            value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            className={S.select}
            style={{
              color: value === 'true' ? F.GREEN : F.RED,
              cursor: disabled ? 'not-allowed' : undefined,
            }}
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
            className={S.input}
            style={{ color: F.CYAN, cursor: disabled ? 'not-allowed' : undefined }}
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
            className={S.input}
            style={{ color: F.BLUE, cursor: disabled ? 'not-allowed' : undefined }}
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
            className={S.input}
            style={{ cursor: disabled ? 'not-allowed' : undefined }}
            onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
          />
        );
    }
  };

  return (
    <div
      className="rounded p-3 transition-colors duration-200"
      style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}` }}
    >
      <div className="flex items-center justify-between mb-2">
        <label className="text-[10px] font-bold tracking-wide flex items-center gap-1.5" style={{ color: F.GRAY }}>
          {parameter.name.toUpperCase()}
        </label>
        <span
          className={`${S.badge} flex items-center gap-1`}
          style={{ backgroundColor: `${typeColor}15`, color: typeColor }}
        >
          <TypeIcon size={10} />
          {parameter.type.toUpperCase()}
        </span>
      </div>
      {renderInput()}
      {parameter.description && (
        <div className="text-[10px] mt-1.5 leading-relaxed" style={{ color: F.MUTED }}>
          {parameter.description}
        </div>
      )}
      {(parameter.min !== undefined || parameter.max !== undefined) && (
        <div className="text-[10px] mt-1" style={{ color: F.MUTED }}>
          {parameter.min !== undefined && `Min: ${parameter.min}`}
          {parameter.min !== undefined && parameter.max !== undefined && ' | '}
          {parameter.max !== undefined && `Max: ${parameter.max}`}
        </div>
      )}
    </div>
  );
};

export default ParameterEditor;
