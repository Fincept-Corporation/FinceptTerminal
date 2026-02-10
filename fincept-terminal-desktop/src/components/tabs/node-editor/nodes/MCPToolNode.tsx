// MCP Tool Node - Generic node component for MCP tools in Node Editor
// Dynamically renders based on tool schema

import React, { useState, useEffect } from 'react';
import { Position } from 'reactflow';
import { Zap, AlertCircle, CheckCircle, Loader } from 'lucide-react';
import { mcpToolService, MCPTool } from '@/services/mcp/mcpToolService';
import {
  FINCEPT,
  SPACING,
  FONT_FAMILY,
  BORDER_RADIUS,
  DynamicHandleBaseNode,
  NodeHeader,
  Button,
  InputField,
  TextareaField,
  StatusPanel,
  getLabelStyle,
  getInputStyle,
  getTextareaStyle,
} from './shared';

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
      <div key={key} style={{ marginBottom: SPACING.MD }}>
        <label style={getLabelStyle(isRequired)}>
          {key}{isRequired ? ' *' : ''}
          {isProvidedByConnection && (
            <span style={{ color: FINCEPT.GREEN, marginLeft: SPACING.XS }}>
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
            type="text"
            inputMode="decimal"
            value={value}
            onChange={(e) => {
              const v = e.target.value;
              if (v === '' || /^\d*\.?\d*$/.test(v)) {
                handleParameterChange(key, v === '' ? '' : parseFloat(v));
              }
            }}
            disabled={isProvidedByConnection}
            style={getInputStyle(isProvidedByConnection)}
          />
        ) : (
          <textarea
            value={value}
            onChange={(e) => handleParameterChange(key, e.target.value)}
            disabled={isProvidedByConnection}
            placeholder={propSchema.description || ''}
            style={getTextareaStyle(isProvidedByConnection)}
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

  // Generate dynamic input handles from tool schema
  const inputHandles = tool?.inputSchema?.properties
    ? Object.keys(tool.inputSchema.properties).map((key, index) => ({
        id: key,
        label: key,
        top: `${30 + index * 20}px`,
      }))
    : [];

  return (
    <DynamicHandleBaseNode
      selected={selected}
      minWidth="280px"
      maxWidth="350px"
      inputHandles={inputHandles}
      outputHandles={[{ id: 'output' }]}
      inputColor={FINCEPT.ORANGE}
      outputColor={FINCEPT.GREEN}
    >
      {/* Header */}
      <NodeHeader
        icon={<Zap size={16} />}
        title={data.label}
        subtitle={`${data.serverId} • ${data.toolName}`}
        color={FINCEPT.ORANGE}
      />

      <div style={{ padding: SPACING.LG }}>
        {/* Tool Description */}
        {tool?.description && (
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            marginBottom: SPACING.LG,
            lineHeight: '1.4'
          }}>
            {tool.description}
          </div>
        )}

        {/* Parameters */}
        {tool?.inputSchema?.properties && (
          <div style={{ marginBottom: SPACING.LG }}>
            <div style={{
              color: FINCEPT.YELLOW,
              fontSize: '10px',
              fontWeight: 700,
              marginBottom: SPACING.MD
            }}>
              PARAMETERS
            </div>
            {Object.entries(tool.inputSchema.properties).map(([key, propSchema]: [string, any]) =>
              renderParameterInput(key, propSchema)
            )}
          </div>
        )}

        {/* Execute Button */}
        <Button
          label={isExecuting ? 'EXECUTING...' : 'EXECUTE TOOL'}
          icon={isExecuting ? <Loader size={12} className="animate-spin" /> : <Zap size={12} />}
          onClick={executeTool}
          variant="primary"
          disabled={!isReadyToExecute() || isExecuting}
          fullWidth
        />

        {/* Status */}
        {error && (
          <StatusPanel
            type="error"
            icon={<AlertCircle size={12} />}
            message={error}
          />
        )}

        {lastResult && !error && (
          <StatusPanel
            type="success"
            icon={<CheckCircle size={12} />}
            message="Executed successfully"
          />
        )}
      </div>
    </DynamicHandleBaseNode>
  );
};

export default MCPToolNode;
