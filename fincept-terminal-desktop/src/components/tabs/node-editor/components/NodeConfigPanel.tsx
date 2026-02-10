/**
 * NodeConfigPanel Component
 *
 * Right sidebar panel for configuring selected node properties
 */

import React, { useState, useEffect } from 'react';
import { Node } from 'reactflow';
import {
  Settings2,
  X,
  Trash2,
  Plus,
  Edit3,
  AlertCircle,
  Database,
  TrendingUp,
  Brain,
  Wrench,
  Play,
  BarChart3,
  Zap,
} from 'lucide-react';
import { NodeParameterInput } from '../nodes/NodeParameterInput';
import type { INodeProperties, NodeParameterValue } from '@/services/nodeSystem';
import { useDataSources } from '@/contexts/DataSourceContext';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  FONT_SIZE,
} from '../nodes/shared';

interface NodeConfigPanelProps {
  selectedNode: Node;
  onLabelChange: (nodeId: string, newLabel: string) => void;
  onParameterChange: (paramName: string, value: NodeParameterValue) => void;
  onClose: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
}

// Section header style helper
const sectionHeaderStyle = (color: string): React.CSSProperties => ({
  display: 'flex',
  alignItems: 'center',
  gap: SPACING.MD,
  marginBottom: SPACING.LG,
  paddingBottom: SPACING.MD,
  borderBottom: `1px solid ${FINCEPT.BORDER}`,
});

const sectionHeaderTextStyle = (color: string): React.CSSProperties => ({
  color,
  fontSize: FONT_SIZE.LG,
  fontWeight: 700,
  textTransform: 'uppercase',
  letterSpacing: '0.5px',
});

const labelBlockStyle: React.CSSProperties = {
  display: 'block',
  color: FINCEPT.GRAY,
  fontSize: FONT_SIZE.SM,
  fontWeight: 700,
  marginBottom: SPACING.SM,
  textTransform: 'uppercase',
};

const selectInputStyle: React.CSSProperties = {
  width: '100%',
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.WHITE,
  padding: SPACING.MD,
  fontSize: FONT_SIZE.MD,
  fontFamily: FONT_FAMILY,
};

const textInputStyle: React.CSSProperties = {
  width: '100%',
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.WHITE,
  padding: SPACING.MD,
  fontSize: FONT_SIZE.MD,
  fontFamily: FONT_FAMILY,
  boxSizing: 'border-box',
};

const textareaInputStyle: React.CSSProperties = {
  ...textInputStyle,
  height: '100px',
  resize: 'vertical',
};

const executeButtonStyle = (color: string, disabled: boolean): React.CSSProperties => ({
  width: '100%',
  backgroundColor: disabled ? FINCEPT.BORDER : color,
  color: FINCEPT.DARK_BG,
  border: 'none',
  padding: SPACING.MD,
  fontSize: FONT_SIZE.SM,
  fontWeight: 700,
  cursor: disabled ? 'not-allowed' : 'pointer',
  opacity: disabled ? 0.5 : 1,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  gap: SPACING.SM,
  fontFamily: FONT_FAMILY,
});

const infoPanelStyle: React.CSSProperties = {
  padding: SPACING.LG,
  backgroundColor: FINCEPT.DARK_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  color: FINCEPT.GRAY,
  fontSize: FONT_SIZE.MD,
  textAlign: 'center',
};

const NodeConfigPanel: React.FC<NodeConfigPanelProps> = ({
  selectedNode,
  onLabelChange,
  onParameterChange,
  onClose,
  onDelete,
  onDuplicate,
}) => {
  const registryData = selectedNode.data.registryData;
  const nodeProperties: INodeProperties[] = registryData?.properties || [];
  const nodeColor = selectedNode.data.color || FINCEPT.ORANGE;

  // Data source context for data-source nodes
  const { connections } = useDataSources();

  // Local state for custom node configs
  const [localConnection, setLocalConnection] = useState(selectedNode.data.selectedConnectionId || '');
  const [localQuery, setLocalQuery] = useState(selectedNode.data.query || '');

  // Technical indicator state
  const [tiDataSource, setTiDataSource] = useState(selectedNode.data.dataSource || 'yfinance');
  const [tiSymbol, setTiSymbol] = useState(selectedNode.data.symbol || 'AAPL');
  const [tiPeriod, setTiPeriod] = useState(selectedNode.data.period || '1y');
  const [tiCategories, setTiCategories] = useState<string[]>(selectedNode.data.categories || ['momentum', 'volume', 'volatility', 'trend', 'others']);

  // Agent mediator state
  const [amProvider, setAmProvider] = useState(selectedNode.data.selectedProvider || '');
  const [amPrompt, setAmPrompt] = useState(selectedNode.data.customPrompt || '');

  // Sync local state when node changes
  useEffect(() => {
    setLocalConnection(selectedNode.data.selectedConnectionId || '');
    setLocalQuery(selectedNode.data.query || '');
    setTiDataSource(selectedNode.data.dataSource || 'yfinance');
    setTiSymbol(selectedNode.data.symbol || 'AAPL');
    setTiPeriod(selectedNode.data.period || '1y');
    setTiCategories(selectedNode.data.categories || ['momentum', 'volume', 'volatility', 'trend', 'others']);
    setAmProvider(selectedNode.data.selectedProvider || '');
    setAmPrompt(selectedNode.data.customPrompt || '');
  }, [selectedNode.id]);

  // Check if this is a custom node type that needs special handling
  const isDataSourceNode = selectedNode.type === 'data-source';
  const isTechnicalIndicatorNode = selectedNode.type === 'technical-indicator';
  const isAgentMediatorNode = selectedNode.type === 'agent-mediator';
  const isPythonAgentNode = selectedNode.type === 'python-agent';
  const isMCPToolNode = selectedNode.type === 'mcp-tool';
  const isBacktestNode = selectedNode.type === 'backtest';
  const isOptimizationNode = selectedNode.type === 'optimization';
  const isResultsDisplayNode = selectedNode.type === 'results-display';

  const getNodeStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return FINCEPT.GREEN;
      case 'error': return FINCEPT.RED;
      case 'running': return FINCEPT.BLUE;
      default: return FINCEPT.GRAY;
    }
  };

  return (
    <div
      style={{
        width: '300px',
        backgroundColor: FINCEPT.DARK_BG,
        borderLeft: `2px solid ${FINCEPT.ORANGE}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        fontFamily: FONT_FAMILY,
      }}
    >
      {/* Panel Header */}
      <div
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          padding: `${SPACING.MD} ${SPACING.LG}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MD }}>
          <Settings2
            size={14}
            style={{ color: FINCEPT.ORANGE, filter: `drop-shadow(0 0 4px ${FINCEPT.ORANGE})` }}
          />
          <span
            style={{
              color: FINCEPT.ORANGE,
              fontSize: FONT_SIZE.LG,
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
            }}
          >
            NODE CONFIG
          </span>
        </div>
        <button
          onClick={onClose}
          style={{
            backgroundColor: 'transparent',
            border: 'none',
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            padding: '2px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          <X size={14} />
        </button>
      </div>

      {/* Node Type Badge */}
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          padding: `10px ${SPACING.LG}`,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
        }}
      >
        <div
          style={{
            width: '28px',
            height: '28px',
            backgroundColor: `${nodeColor}20`,
            border: `1px solid ${nodeColor}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: FONT_SIZE.XL,
            fontWeight: 700,
            color: nodeColor,
          }}
        >
          {(selectedNode.data.label || selectedNode.type || 'N')[0].toUpperCase()}
        </div>
        <div>
          <div style={{ color: FINCEPT.WHITE, fontSize: FONT_SIZE.LG, fontWeight: 600 }}>
            {selectedNode.data.label || selectedNode.type}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.SM, textTransform: 'uppercase' }}>
            {selectedNode.type}
          </div>
        </div>
      </div>

      {/* Scrollable Content */}
      <div style={{ flex: 1, overflow: 'auto', backgroundColor: FINCEPT.DARK_BG }}>
        {/* Label Section */}
        <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <label
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              color: FINCEPT.ORANGE,
              fontSize: FONT_SIZE.SM,
              fontWeight: 700,
              marginBottom: SPACING.SM,
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            <span>LABEL</span>
            <span style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.XS, fontWeight: 400, opacity: 0.6 }}>REQUIRED</span>
          </label>
          <div
            style={{
              position: 'relative',
              display: 'flex',
              alignItems: 'stretch',
            }}
          >
            <div
              style={{
                backgroundColor: FINCEPT.HEADER_BG,
                borderTop: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: `1px solid ${FINCEPT.BORDER}`,
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                padding: `0 ${SPACING.MD}`,
                display: 'flex',
                alignItems: 'center',
                color: FINCEPT.ORANGE,
                fontSize: FONT_SIZE.MD,
                fontFamily: FONT_FAMILY,
              }}
            >
              <Edit3 size={10} />
            </div>
            <input
              type="text"
              value={selectedNode.data.label || ''}
              onChange={(e) => onLabelChange(selectedNode.id, e.target.value)}
              placeholder="Enter node label..."
              style={{
                flex: 1,
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: 'none',
                padding: '10px 12px',
                color: FINCEPT.WHITE,
                fontSize: FONT_SIZE.LG,
                fontFamily: FONT_FAMILY,
                outline: 'none',
                transition: 'all 0.15s ease',
              }}
            />
          </div>
          <div
            style={{
              marginTop: SPACING.XS,
              fontSize: FONT_SIZE.XS,
              color: FINCEPT.GRAY,
              fontFamily: FONT_FAMILY,
              opacity: 0.6,
            }}
          >
            Display name for this node in the workflow
          </div>
        </div>

        {/* ==================== DATA SOURCE NODE CONFIG ==================== */}
        {isDataSourceNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.ORANGE)}>
              <Database size={14} color={FINCEPT.ORANGE} />
              <span style={sectionHeaderTextStyle(FINCEPT.ORANGE)}>
                DATA SOURCE CONFIG
              </span>
            </div>

            {/* Connection Selector */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>CONNECTION</label>
              <select
                value={localConnection}
                onChange={(e) => {
                  setLocalConnection(e.target.value);
                  selectedNode.data.onConnectionChange?.(e.target.value);
                }}
                style={selectInputStyle}
              >
                <option value="">-- Select Connection --</option>
                {connections.filter(c => c.status === 'connected' || c.status === 'disconnected').map((conn) => (
                  <option key={conn.id} value={conn.id}>
                    {conn.name} ({conn.type})
                  </option>
                ))}
              </select>
              {connections.length === 0 && (
                <div style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.SM, marginTop: SPACING.XS }}>
                  No connections. Add them in Data Sources tab.
                </div>
              )}
            </div>

            {/* Query Editor */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>QUERY</label>
              <textarea
                value={localQuery}
                onChange={(e) => {
                  setLocalQuery(e.target.value);
                  selectedNode.data.onQueryChange?.(e.target.value);
                }}
                placeholder="SELECT * FROM table..."
                style={textareaInputStyle}
              />
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              disabled={!localConnection || !localQuery}
              style={executeButtonStyle(FINCEPT.ORANGE, !localConnection || !localQuery)}
            >
              <Play size={10} />
              EXECUTE QUERY
            </button>
          </div>
        )}

        {/* ==================== TECHNICAL INDICATOR NODE CONFIG ==================== */}
        {isTechnicalIndicatorNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.CYAN)}>
              <TrendingUp size={14} color={FINCEPT.CYAN} />
              <span style={sectionHeaderTextStyle(FINCEPT.CYAN)}>
                TECHNICAL INDICATOR CONFIG
              </span>
            </div>

            {/* Data Source */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>DATA SOURCE</label>
              <select
                value={tiDataSource}
                onChange={(e) => {
                  setTiDataSource(e.target.value);
                  selectedNode.data.onParameterChange?.({ dataSource: e.target.value });
                }}
                style={selectInputStyle}
              >
                <option value="yfinance">Yahoo Finance</option>
                <option value="csv">CSV File</option>
                <option value="json">JSON Data</option>
              </select>
            </div>

            {/* Symbol */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>SYMBOL</label>
              <input
                type="text"
                value={tiSymbol}
                onChange={(e) => {
                  setTiSymbol(e.target.value);
                  selectedNode.data.onParameterChange?.({ symbol: e.target.value });
                }}
                placeholder="AAPL, MSFT, BTC-USD..."
                style={textInputStyle}
              />
            </div>

            {/* Period */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>PERIOD</label>
              <select
                value={tiPeriod}
                onChange={(e) => {
                  setTiPeriod(e.target.value);
                  selectedNode.data.onParameterChange?.({ period: e.target.value });
                }}
                style={selectInputStyle}
              >
                <option value="1d">1 Day</option>
                <option value="5d">5 Days</option>
                <option value="1mo">1 Month</option>
                <option value="3mo">3 Months</option>
                <option value="6mo">6 Months</option>
                <option value="1y">1 Year</option>
                <option value="2y">2 Years</option>
                <option value="5y">5 Years</option>
              </select>
            </div>

            {/* Indicator Categories */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>INDICATOR CATEGORIES</label>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.SM }}>
                {['momentum', 'volume', 'volatility', 'trend', 'others'].map((cat) => (
                  <button
                    key={cat}
                    onClick={() => {
                      const newCats = tiCategories.includes(cat)
                        ? tiCategories.filter(c => c !== cat)
                        : [...tiCategories, cat];
                      setTiCategories(newCats);
                      selectedNode.data.onParameterChange?.({ categories: newCats });
                    }}
                    style={{
                      padding: `${SPACING.XS} ${SPACING.MD}`,
                      fontSize: FONT_SIZE.SM,
                      fontWeight: 600,
                      backgroundColor: tiCategories.includes(cat) ? `${FINCEPT.CYAN}30` : FINCEPT.DARK_BG,
                      border: `1px solid ${tiCategories.includes(cat) ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                      color: tiCategories.includes(cat) ? FINCEPT.CYAN : FINCEPT.GRAY,
                      cursor: 'pointer',
                      textTransform: 'uppercase',
                      fontFamily: FONT_FAMILY,
                    }}
                  >
                    {cat}
                  </button>
                ))}
              </div>
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              style={executeButtonStyle(FINCEPT.CYAN, false)}
            >
              <Play size={10} />
              CALCULATE INDICATORS
            </button>
          </div>
        )}

        {/* ==================== AGENT MEDIATOR NODE CONFIG ==================== */}
        {isAgentMediatorNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.CYAN)}>
              <Brain size={14} color={FINCEPT.CYAN} />
              <span style={sectionHeaderTextStyle(FINCEPT.CYAN)}>
                AI MEDIATOR CONFIG
              </span>
            </div>

            {/* LLM Provider */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>LLM PROVIDER</label>
              <select
                value={amProvider}
                onChange={(e) => {
                  setAmProvider(e.target.value);
                  selectedNode.data.onConfigChange?.({ selectedProvider: e.target.value, customPrompt: amPrompt });
                }}
                style={selectInputStyle}
              >
                <option value="">-- Select Provider --</option>
                <option value="ollama">Ollama (Local)</option>
                <option value="openai">OpenAI</option>
                <option value="anthropic">Anthropic</option>
                <option value="groq">Groq</option>
              </select>
            </div>

            {/* Custom Prompt */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>CUSTOM PROMPT</label>
              <textarea
                value={amPrompt}
                onChange={(e) => {
                  setAmPrompt(e.target.value);
                  selectedNode.data.onConfigChange?.({ selectedProvider: amProvider, customPrompt: e.target.value });
                }}
                placeholder="Enter instructions for the AI agent..."
                style={textareaInputStyle}
              />
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              disabled={!amProvider}
              style={executeButtonStyle(FINCEPT.CYAN, !amProvider)}
            >
              <Play size={10} />
              RUN AI MEDIATOR
            </button>
          </div>
        )}

        {/* ==================== PYTHON AGENT NODE CONFIG ==================== */}
        {isPythonAgentNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.ORANGE)}>
              <span style={{ fontSize: '14px' }}>🐍</span>
              <span style={sectionHeaderTextStyle(FINCEPT.ORANGE)}>
                PYTHON AGENT CONFIG
              </span>
            </div>

            <div style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.MD, marginBottom: SPACING.LG }}>
              <strong style={{ color: FINCEPT.WHITE }}>Agent:</strong> {selectedNode.data.agentType || 'Unknown'}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.MD, marginBottom: SPACING.LG }}>
              <strong style={{ color: FINCEPT.WHITE }}>Category:</strong> {selectedNode.data.agentCategory || 'Unknown'}
            </div>

            {/* LLM Selection */}
            <div style={{ marginBottom: SPACING.LG }}>
              <label style={labelBlockStyle}>LLM PROVIDER</label>
              <select
                value={selectedNode.data.selectedLLM || 'active'}
                onChange={(e) => selectedNode.data.onLLMChange?.(e.target.value)}
                style={selectInputStyle}
              >
                <option value="active">Active LLM</option>
                <option value="ollama">Ollama</option>
                <option value="openai">OpenAI</option>
                <option value="anthropic">Anthropic</option>
              </select>
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.(selectedNode.id)}
              disabled={selectedNode.data.status === 'running'}
              style={executeButtonStyle(FINCEPT.ORANGE, selectedNode.data.status === 'running')}
            >
              <Play size={10} />
              {selectedNode.data.status === 'running' ? 'RUNNING...' : 'RUN AGENT'}
            </button>
          </div>
        )}

        {/* ==================== MCP TOOL NODE CONFIG ==================== */}
        {isMCPToolNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.ORANGE)}>
              <Wrench size={14} color={FINCEPT.ORANGE} />
              <span style={sectionHeaderTextStyle(FINCEPT.ORANGE)}>
                MCP TOOL CONFIG
              </span>
            </div>

            <div style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.MD, marginBottom: SPACING.MD }}>
              <strong style={{ color: FINCEPT.WHITE }}>Server:</strong> {selectedNode.data.serverId || 'Unknown'}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.MD, marginBottom: SPACING.LG }}>
              <strong style={{ color: FINCEPT.WHITE }}>Tool:</strong> {selectedNode.data.toolName || 'Unknown'}
            </div>

            <div style={{
              ...infoPanelStyle,
              marginBottom: SPACING.LG,
            }}>
              Configure tool parameters in the node directly
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              style={executeButtonStyle(FINCEPT.ORANGE, false)}
            >
              <Play size={10} />
              EXECUTE TOOL
            </button>
          </div>
        )}

        {/* ==================== BACKTEST NODE CONFIG ==================== */}
        {isBacktestNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.GREEN)}>
              <BarChart3 size={14} color={FINCEPT.GREEN} />
              <span style={sectionHeaderTextStyle(FINCEPT.GREEN)}>
                BACKTEST CONFIG
              </span>
            </div>

            <div style={infoPanelStyle}>
              Backtest configuration coming soon.<br/>
              Connect data source and strategy nodes.
            </div>
          </div>
        )}

        {/* ==================== OPTIMIZATION NODE CONFIG ==================== */}
        {isOptimizationNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.ORANGE)}>
              <Zap size={14} color={FINCEPT.ORANGE} />
              <span style={sectionHeaderTextStyle(FINCEPT.ORANGE)}>
                OPTIMIZATION CONFIG
              </span>
            </div>

            <div style={infoPanelStyle}>
              Optimization configuration coming soon.<br/>
              Connect backtest node for parameter optimization.
            </div>
          </div>
        )}

        {/* ==================== CUSTOM/REGISTRY NODE (no registryData) ==================== */}
        {selectedNode.type === 'custom' && !registryData && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.ORANGE)}>
              <Settings2 size={14} color={FINCEPT.ORANGE} />
              <span style={sectionHeaderTextStyle(FINCEPT.ORANGE)}>
                NODE CONFIG
              </span>
            </div>

            <div style={infoPanelStyle}>
              {selectedNode.data.nodeTypeName ? (
                <>
                  <div style={{ marginBottom: SPACING.MD, color: FINCEPT.WHITE }}>
                    <strong>Type:</strong> {selectedNode.data.nodeTypeName}
                  </div>
                  <div style={{ fontSize: FONT_SIZE.SM }}>
                    This node's configuration will be available when executed in a workflow.
                  </div>
                </>
              ) : (
                <div>Configure this node's connections and parameters.</div>
              )}
            </div>
          </div>
        )}

        {/* ==================== RESULTS DISPLAY NODE ==================== */}
        {isResultsDisplayNode && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={sectionHeaderStyle(FINCEPT.GREEN)}>
              <span style={{ fontSize: '14px' }}>📊</span>
              <span style={sectionHeaderTextStyle(FINCEPT.GREEN)}>
                RESULTS DISPLAY
              </span>
            </div>

            <div style={{
              ...infoPanelStyle,
              textAlign: 'left',
            }}>
              This node displays output from connected nodes.<br/>
              Connect input from any data-producing node.
            </div>
          </div>
        )}

        {/* Registry Node Parameters */}
        {nodeProperties.length > 0 && (
          <div
            style={{
              padding: '14px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              backgroundColor: FINCEPT.PANEL_BG,
            }}
          >
            {/* Parameters Header */}
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: SPACING.XL,
                paddingBottom: '10px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
              }}
            >
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                }}
              >
                <div
                  style={{
                    width: '4px',
                    height: '18px',
                    backgroundColor: FINCEPT.ORANGE,
                  }}
                />
                <span
                  style={{
                    color: FINCEPT.ORANGE,
                    fontSize: '13px',
                    fontWeight: 700,
                    textTransform: 'uppercase',
                    letterSpacing: '1.5px',
                    fontFamily: FONT_FAMILY,
                  }}
                >
                  PARAMETERS
                </span>
              </div>
              <div
                style={{
                  backgroundColor: `${FINCEPT.ORANGE}30`,
                  border: `1px solid ${FINCEPT.ORANGE}`,
                  padding: `${SPACING.XS} 10px`,
                  fontSize: FONT_SIZE.XL,
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  fontFamily: FONT_FAMILY,
                }}
              >
                {nodeProperties.length}
              </div>
            </div>

            {/* Parameters List */}
            <div
              style={{
                display: 'flex',
                flexDirection: 'column',
                gap: '14px',
              }}
            >
              {nodeProperties.map((param, index) => (
                <div
                  key={param.name}
                  style={{
                    padding: '14px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    position: 'relative',
                  }}
                >
                  {/* Parameter Index Badge */}
                  <div
                    style={{
                      position: 'absolute',
                      top: '-1px',
                      left: '-1px',
                      backgroundColor: FINCEPT.BORDER,
                      color: FINCEPT.GRAY,
                      fontSize: FONT_SIZE.LG,
                      fontWeight: 700,
                      padding: `${SPACING.XS} ${SPACING.MD}`,
                      fontFamily: FONT_FAMILY,
                      borderRight: `1px solid ${FINCEPT.GRAY}50`,
                      borderBottom: `1px solid ${FINCEPT.GRAY}50`,
                    }}
                  >
                    {String(index + 1).padStart(2, '0')}
                  </div>

                  {/* Required Indicator */}
                  {param.required && (
                    <div
                      style={{
                        position: 'absolute',
                        top: '0',
                        right: '0',
                        width: '0',
                        height: '0',
                        borderStyle: 'solid',
                        borderWidth: '0 16px 16px 0',
                        borderColor: `transparent ${FINCEPT.RED} transparent transparent`,
                      }}
                    />
                  )}

                  <div style={{ paddingTop: SPACING.LG }}>
                    <NodeParameterInput
                      parameter={param}
                      value={selectedNode.data.parameters?.[param.name] ?? param.default}
                      onChange={(value) => onParameterChange(param.name, value)}
                    />
                  </div>
                </div>
              ))}
            </div>

            {/* Parameters Footer */}
            <div
              style={{
                marginTop: '14px',
                paddingTop: '10px',
                borderTop: `1px solid ${FINCEPT.BORDER}`,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.MD,
              }}
            >
              <div
                style={{
                  width: '8px',
                  height: '8px',
                  backgroundColor: FINCEPT.GREEN,
                  borderRadius: '50%',
                  boxShadow: `0 0 6px ${FINCEPT.GREEN}80`,
                }}
              />
              <span
                style={{
                  color: FINCEPT.GRAY,
                  fontSize: FONT_SIZE.LG,
                  fontWeight: 600,
                  fontFamily: FONT_FAMILY,
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}
              >
                {nodeProperties.filter((p) => p.required).length > 0
                  ? `${nodeProperties.filter((p) => p.required).length} REQUIRED`
                  : 'ALL OPTIONAL'}
              </span>
            </div>
          </div>
        )}

        {/* Node Info Section */}
        <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div
            style={{
              color: FINCEPT.ORANGE,
              fontSize: FONT_SIZE.SM,
              fontWeight: 700,
              marginBottom: '10px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            NODE INFO
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SM }}>
            {/* Node ID */}
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.SM, textTransform: 'uppercase' }}>
                ID
              </span>
              <code style={{ color: FINCEPT.WHITE, fontSize: FONT_SIZE.SM }}>{selectedNode.id}</code>
            </div>

            {/* Position */}
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.SM, textTransform: 'uppercase' }}>
                POS
              </span>
              <span style={{ color: FINCEPT.WHITE, fontSize: FONT_SIZE.SM }}>
                {Math.round(selectedNode.position.x)}, {Math.round(selectedNode.position.y)}
              </span>
            </div>

            {/* Registry Type */}
            {selectedNode.data.nodeTypeName && (
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.SM, textTransform: 'uppercase' }}>
                  TYPE
                </span>
                <span style={{ color: FINCEPT.WHITE, fontSize: FONT_SIZE.SM }}>
                  {selectedNode.data.nodeTypeName}
                </span>
              </div>
            )}

            {/* Status */}
            {selectedNode.data.status && (
              <div
                style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}
              >
                <span style={{ color: FINCEPT.GRAY, fontSize: FONT_SIZE.SM, textTransform: 'uppercase' }}>
                  STATUS
                </span>
                <span
                  style={{
                    fontSize: FONT_SIZE.SM,
                    fontWeight: 700,
                    padding: `2px ${SPACING.SM}`,
                    backgroundColor: `${getNodeStatusColor(selectedNode.data.status)}20`,
                    color: getNodeStatusColor(selectedNode.data.status),
                    textTransform: 'uppercase',
                  }}
                >
                  {selectedNode.data.status}
                </span>
              </div>
            )}
          </div>
        </div>

        {/* Description (for registry nodes) */}
        {registryData?.description && (
          <div style={{ padding: SPACING.LG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div
              style={{
                color: FINCEPT.CYAN,
                fontSize: FONT_SIZE.SM,
                fontWeight: 700,
                marginBottom: SPACING.SM,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
            >
              DESCRIPTION
            </div>
            <div style={{ color: FINCEPT.WHITE, fontSize: FONT_SIZE.MD, lineHeight: '1.5' }}>
              {registryData.description}
            </div>
          </div>
        )}

        {/* Error Display */}
        {selectedNode.data.error && (
          <div
            style={{
              margin: SPACING.LG,
              backgroundColor: `${FINCEPT.RED}15`,
              border: `1px solid ${FINCEPT.RED}`,
              padding: '10px',
            }}
          >
            <div
              style={{
                color: FINCEPT.RED,
                fontSize: FONT_SIZE.SM,
                fontWeight: 700,
                marginBottom: SPACING.XS,
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.XS,
              }}
            >
              <AlertCircle size={10} />
              ERROR
            </div>
            <div style={{ color: FINCEPT.WHITE, fontSize: FONT_SIZE.MD, lineHeight: '1.4' }}>
              {selectedNode.data.error}
            </div>
          </div>
        )}

        {/* Result Preview */}
        {selectedNode.data.result && selectedNode.data.status === 'completed' && (
          <div
            style={{
              margin: SPACING.LG,
              backgroundColor: `${FINCEPT.GREEN}15`,
              border: `1px solid ${FINCEPT.GREEN}`,
              padding: '10px',
            }}
          >
            <div
              style={{
                color: FINCEPT.GREEN,
                fontSize: FONT_SIZE.SM,
                fontWeight: 700,
                marginBottom: SPACING.SM,
                textTransform: 'uppercase',
              }}
            >
              OUTPUT PREVIEW
            </div>
            <pre
              style={{
                color: FINCEPT.WHITE,
                fontSize: FONT_SIZE.SM,
                lineHeight: '1.4',
                overflow: 'auto',
                maxHeight: '80px',
                margin: 0,
                fontFamily: FONT_FAMILY,
              }}
            >
              {typeof selectedNode.data.result === 'string'
                ? selectedNode.data.result.substring(0, 200)
                : JSON.stringify(selectedNode.data.result, null, 2).substring(0, 200)}
              {(typeof selectedNode.data.result === 'string'
                ? selectedNode.data.result.length
                : JSON.stringify(selectedNode.data.result).length) > 200 && '...'}
            </pre>
          </div>
        )}
      </div>

      {/* Panel Footer with Actions */}
      <div
        style={{
          padding: `10px ${SPACING.LG}`,
          backgroundColor: FINCEPT.PANEL_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          gap: SPACING.MD,
        }}
      >
        <button
          onClick={onDelete}
          style={{
            flex: 1,
            backgroundColor: 'transparent',
            color: FINCEPT.RED,
            border: `1px solid ${FINCEPT.RED}`,
            padding: `${SPACING.SM} ${SPACING.MD}`,
            fontSize: FONT_SIZE.SM,
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: SPACING.XS,
            textTransform: 'uppercase',
            letterSpacing: '0.3px',
            fontFamily: FONT_FAMILY,
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.RED;
            e.currentTarget.style.color = FINCEPT.DARK_BG;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = FINCEPT.RED;
          }}
        >
          <Trash2 size={10} />
          DELETE
        </button>
        <button
          onClick={onDuplicate}
          style={{
            flex: 1,
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.ORANGE}`,
            padding: `${SPACING.SM} ${SPACING.MD}`,
            fontSize: FONT_SIZE.SM,
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: SPACING.XS,
            textTransform: 'uppercase',
            letterSpacing: '0.3px',
            fontFamily: FONT_FAMILY,
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = `${FINCEPT.ORANGE}CC`;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.ORANGE;
          }}
        >
          <Plus size={10} />
          DUPLICATE
        </button>
      </div>
    </div>
  );
};

export default NodeConfigPanel;
