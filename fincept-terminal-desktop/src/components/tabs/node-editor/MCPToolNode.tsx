// MCP Tool Node - Generic node component for MCP tools in Node Editor
// Dynamically renders based on tool schema

import React, { useState, useEffect } from 'react';
import { Handle, Position } from 'reactflow';
import { Zap, AlertCircle, CheckCircle, Loader } from 'lucide-react';
import { mcpToolService, MCPTool } from '../../../services/mcpToolService';

interface MCPToolNodeProps {
  data: {
    serverId: string;
    toolName: string;
    label: string;
    parameters?: Record<string, any>;
    onParameterChange?: (params: Record<string, any>) => void;
    onExecute?: (result: any) => void;
    inputs?: Record<string, any>; // Inputs from connected nodes
  };
  selected: boolean;
}

const MCPToolNode: React.FC<MCPToolNodeProps> = ({ data, selected }) => {
  const [parameters, setParameters] = useState<Record<string, any>>(data.parameters || {});
  const [tool, setTool] = useState<MCPTool | null>(null);
  const [isExecuting, setIsExecuting] = useState(false);
  const [lastResult, setLastResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const RED = '#ef4444';
  const YELLOW = '#FFFF00';

  // Load tool schema on mount
  useEffect(() => {
    const loadTool = async () => {
      try {
        const toolData = await mcpToolService.getTool(data.serverId, data.toolName);
        setTool(toolData);
      } catch (err) {
        console.error('[MCPToolNode] Failed to load tool:', err);
        setError('Failed to load tool schema');
      }
    };
    loadTool();
  }, [data.serverId, data.toolName]);

  // Merge inputs from connected nodes with manual parameters
  const getMergedParameters = (): Record<string, any> => {
    return {
      ...parameters,
      ...(data.inputs || {})
    };
  };

  // Execute the tool
  const executeTool = async () => {
    if (!tool) return;

    setIsExecuting(true);
    setError(null);

    try {
      const mergedParams = getMergedParameters();
      const result = await mcpToolService.executeToolDirect(
        data.serverId,
        data.toolName,
        mergedParams
      );

      if (result.success) {
        setLastResult(result.data);
        data.onExecute?.(result.data);
      } else {
        setError(result.error || 'Unknown error');
      }
    } catch (err: any) {
      setError(err.message || 'Execution failed');
    } finally {
      setIsExecuting(false);
    }
  };

  // Handle parameter change
  const handleParameterChange = (key: string, value: any) => {
    const newParams = { ...parameters, [key]: value };
    setParameters(newParams);
    data.onParameterChange?.(newParams);
  };

  // Get required parameters from schema
  const getRequiredParams = (): string[] => {
    if (!tool?.inputSchema?.required) return [];
    return tool.inputSchema.required;
  };

  // Render parameter input based on type
  const renderParameterInput = (key: string, propSchema: any) => {
    const isRequired = getRequiredParams().includes(key);
    const value = parameters[key] || '';
    const type = propSchema.type || 'string';

    // Check if parameter is provided by connected node
    const isProvidedByConnection = data.inputs && key in data.inputs;

    return (
      <div key={key} style={{ marginBottom: '8px' }}>
        <label style={{
          color: isRequired ? YELLOW : GRAY,
          fontSize: '9px',
          display: 'block',
          marginBottom: '2px',
          fontWeight: isRequired ? 'bold' : 'normal'
        }}>
          {key}{isRequired ? ' *' : ''}
          {isProvidedByConnection && (
            <span style={{ color: GREEN, marginLeft: '4px' }}>
              [Connected]
            </span>
          )}
        </label>
        {type === 'boolean' ? (
          <input
            type="checkbox"
            checked={value || false}
            onChange={(e) => handleParameterChange(key, e.target.checked)}
            disabled={isProvidedByConnection}
            style={{
              cursor: isProvidedByConnection ? 'not-allowed' : 'pointer',
              opacity: isProvidedByConnection ? 0.5 : 1
            }}
          />
        ) : type === 'number' ? (
          <input
            type="number"
            value={value}
            onChange={(e) => handleParameterChange(key, parseFloat(e.target.value))}
            disabled={isProvidedByConnection}
            style={{
              width: '100%',
              backgroundColor: DARK_BG,
              border: `1px solid ${BORDER}`,
              color: WHITE,
              padding: '4px',
              fontSize: '9px',
              fontFamily: 'monospace',
              opacity: isProvidedByConnection ? 0.5 : 1,
              cursor: isProvidedByConnection ? 'not-allowed' : 'text'
            }}
          />
        ) : (
          <textarea
            value={value}
            onChange={(e) => handleParameterChange(key, e.target.value)}
            disabled={isProvidedByConnection}
            placeholder={propSchema.description || ''}
            style={{
              width: '100%',
              backgroundColor: DARK_BG,
              border: `1px solid ${BORDER}`,
              color: WHITE,
              padding: '4px',
              fontSize: '9px',
              fontFamily: 'monospace',
              resize: 'vertical',
              minHeight: '40px',
              opacity: isProvidedByConnection ? 0.5 : 1,
              cursor: isProvidedByConnection ? 'not-allowed' : 'text'
            }}
          />
        )}
      </div>
    );
  };

  // Check if tool is ready to execute
  const isReadyToExecute = (): boolean => {
    if (!tool) return false;
    const mergedParams = getMergedParameters();
    const validation = mcpToolService.validateToolParams(tool, mergedParams);
    return validation.valid;
  };

  return (
    <div style={{
      backgroundColor: PANEL_BG,
      border: `2px solid ${selected ? ORANGE : BORDER}`,
      borderRadius: '6px',
      padding: '12px',
      minWidth: '280px',
      maxWidth: '350px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 12px ${ORANGE}80` : 'none'
    }}>
      {/* Input handles - dynamically generated from schema */}
      {tool?.inputSchema?.properties && Object.keys(tool.inputSchema.properties).map((key, index) => (
        <Handle
          key={`input-${key}`}
          type="target"
          position={Position.Left}
          id={key}
          style={{
            top: `${30 + (index * 20)}px`,
            background: ORANGE,
            width: '10px',
            height: '10px',
            border: `2px solid ${DARK_BG}`
          }}
        />
      ))}

      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        marginBottom: '12px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${BORDER}`
      }}>
        <Zap size={16} color={ORANGE} />
        <div style={{ flex: 1 }}>
          <div style={{
            color: WHITE,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '2px'
          }}>
            {data.label}
          </div>
          <div style={{
            color: GRAY,
            fontSize: '8px'
          }}>
            {data.serverId} • {data.toolName}
          </div>
        </div>
      </div>

      {/* Tool Description */}
      {tool?.description && (
        <div style={{
          color: GRAY,
          fontSize: '9px',
          marginBottom: '12px',
          lineHeight: '1.4'
        }}>
          {tool.description}
        </div>
      )}

      {/* Parameters */}
      {tool?.inputSchema?.properties && (
        <div style={{ marginBottom: '12px' }}>
          <div style={{
            color: YELLOW,
            fontSize: '10px',
            fontWeight: 'bold',
            marginBottom: '8px'
          }}>
            PARAMETERS
          </div>
          {Object.entries(tool.inputSchema.properties).map(([key, propSchema]: [string, any]) =>
            renderParameterInput(key, propSchema)
          )}
        </div>
      )}

      {/* Execute Button */}
      <button
        onClick={executeTool}
        disabled={!isReadyToExecute() || isExecuting}
        style={{
          width: '100%',
          backgroundColor: isReadyToExecute() && !isExecuting ? GREEN : GRAY,
          color: 'black',
          border: 'none',
          padding: '8px',
          fontSize: '10px',
          fontWeight: 'bold',
          cursor: isReadyToExecute() && !isExecuting ? 'pointer' : 'not-allowed',
          borderRadius: '4px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '6px',
          marginBottom: '8px'
        }}
      >
        {isExecuting ? (
          <>
            <Loader size={12} className="animate-spin" />
            EXECUTING...
          </>
        ) : (
          <>
            <Zap size={12} />
            EXECUTE TOOL
          </>
        )}
      </button>

      {/* Status */}
      {error && (
        <div style={{
          backgroundColor: `${RED}20`,
          border: `1px solid ${RED}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px'
        }}>
          <AlertCircle size={12} color={RED} />
          <div style={{ color: RED, fontSize: '9px', flex: 1 }}>
            {error}
          </div>
        </div>
      )}

      {lastResult && !error && (
        <div style={{
          backgroundColor: `${GREEN}20`,
          border: `1px solid ${GREEN}`,
          borderRadius: '4px',
          padding: '6px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px'
        }}>
          <CheckCircle size={12} color={GREEN} />
          <div style={{ color: GREEN, fontSize: '9px', flex: 1 }}>
            Executed successfully
          </div>
        </div>
      )}

      {/* Output handle */}
      <Handle
        type="source"
        position={Position.Right}
        id="output"
        style={{
          background: GREEN,
          width: '10px',
          height: '10px',
          border: `2px solid ${DARK_BG}`
        }}
      />
    </div>
  );
};

export default MCPToolNode;
