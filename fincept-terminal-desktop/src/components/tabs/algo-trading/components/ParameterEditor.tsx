import React, { useState, useEffect } from 'react';
import { Settings, Loader2, AlertCircle, RefreshCw } from 'lucide-react';
import type { StrategyParameter } from '../types';
import { extractStrategyParameters } from '../services/algoTradingService';

const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
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

  // Extract parameters from code if not provided
  useEffect(() => {
    if (providedParameters) {
      setParameters(providedParameters);
      return;
    }

    if (code) {
      extractParametersFromCode(code);
    }
  }, [code, providedParameters]);

  const extractParametersFromCode = async (sourceCode: string) => {
    setLoading(true);
    setError(null);

    try {
      const result = await extractStrategyParameters(sourceCode);
      if (result.success && result.data) {
        setParameters(result.data);

        // Initialize default values if not already set
        const newValues = { ...values };
        result.data.forEach((param) => {
          if (!(param.name in newValues)) {
            newValues[param.name] = param.default_value;
          }
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
    parameters.forEach((param) => {
      defaultValues[param.name] = param.default_value;
    });
    onChange(defaultValues);
  };

  if (loading) {
    return (
      <div
        style={{
          padding: '16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          color: F.GRAY,
          fontSize: '11px',
        }}
      >
        <Loader2 size={14} style={{ animation: 'spin 1s linear infinite' }} />
        Extracting parameters...
        <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
      </div>
    );
  }

  if (error) {
    return (
      <div
        style={{
          padding: '12px',
          backgroundColor: `${F.RED}20`,
          border: `1px solid ${F.RED}`,
          borderRadius: '4px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          color: F.RED,
          fontSize: '11px',
        }}
      >
        <AlertCircle size={14} />
        {error}
      </div>
    );
  }

  if (parameters.length === 0) {
    return (
      <div
        style={{
          padding: '16px',
          textAlign: 'center',
          color: F.GRAY,
          fontSize: '11px',
        }}
      >
        No configurable parameters found
      </div>
    );
  }

  return (
    <div
      style={{
        backgroundColor: F.PANEL_BG,
        borderRadius: '4px',
        border: `1px solid ${F.BORDER}`,
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        style={{
          padding: '10px 12px',
          borderBottom: `1px solid ${F.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          backgroundColor: F.HEADER_BG,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Settings size={12} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>
            STRATEGY PARAMETERS
          </span>
          <span style={{ fontSize: '9px', color: F.GRAY }}>({parameters.length})</span>
        </div>
        <button
          onClick={handleReset}
          disabled={disabled}
          style={{
            padding: '4px 8px',
            backgroundColor: 'transparent',
            border: `1px solid ${F.BORDER}`,
            borderRadius: '2px',
            color: F.GRAY,
            fontSize: '9px',
            cursor: disabled ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            opacity: disabled ? 0.5 : 1,
          }}
        >
          <RefreshCw size={10} />
          RESET
        </button>
      </div>

      {/* Parameters Grid */}
      <div
        style={{
          padding: '12px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
          gap: '12px',
        }}
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

// Parameter Input Component
const ParameterInput: React.FC<{
  parameter: StrategyParameter;
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
}> = ({ parameter, value, onChange, disabled }) => {
  const renderInput = () => {
    switch (parameter.type) {
      case 'bool':
        return (
          <select
            value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '4px',
              color: F.WHITE,
              fontSize: '11px',
              cursor: disabled ? 'not-allowed' : 'pointer',
            }}
          >
            <option value="true">True</option>
            <option value="false">False</option>
          </select>
        );

      case 'int':
        return (
          <input
            type="number"
            value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            min={parameter.min}
            max={parameter.max}
            step={1}
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '4px',
              color: F.WHITE,
              fontSize: '11px',
            }}
          />
        );

      case 'float':
        return (
          <input
            type="number"
            value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            min={parameter.min}
            max={parameter.max}
            step={0.01}
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '4px',
              color: F.WHITE,
              fontSize: '11px',
            }}
          />
        );

      default:
        return (
          <input
            type="text"
            value={value}
            onChange={(e) => onChange(e.target.value)}
            disabled={disabled}
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '4px',
              color: F.WHITE,
              fontSize: '11px',
            }}
          />
        );
    }
  };

  return (
    <div>
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '4px',
        }}
      >
        <label style={{ fontSize: '10px', color: F.GRAY, fontWeight: 600 }}>
          {parameter.name}
        </label>
        <span
          style={{
            fontSize: '8px',
            padding: '2px 4px',
            backgroundColor: F.HOVER,
            color: F.CYAN,
            borderRadius: '2px',
          }}
        >
          {parameter.type}
        </span>
      </div>
      {renderInput()}
      {parameter.description && (
        <div
          style={{
            fontSize: '9px',
            color: F.MUTED,
            marginTop: '4px',
          }}
        >
          {parameter.description}
        </div>
      )}
    </div>
  );
};

export default ParameterEditor;
