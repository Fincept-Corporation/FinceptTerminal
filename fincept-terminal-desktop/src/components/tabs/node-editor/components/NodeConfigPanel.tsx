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
  Info,
} from 'lucide-react';
import { NodeParameterInput } from '../nodes/NodeParameterInput';
import type { INodeProperties, NodeParameterValue } from '@/services/nodeSystem';
import { useDataSources } from '@/contexts/DataSourceContext';

interface NodeConfigPanelProps {
  selectedNode: Node;
  onLabelChange: (nodeId: string, newLabel: string) => void;
  onParameterChange: (paramName: string, value: NodeParameterValue) => void;
  onClose: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
}

// Design system colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
  BLUE: '#0088FF',
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
  const nodeColor = selectedNode.data.color || '#FF8800';

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

  const hasCustomConfig = isDataSourceNode || isTechnicalIndicatorNode || isAgentMediatorNode ||
                          isPythonAgentNode || isMCPToolNode || isBacktestNode || isOptimizationNode;

  return (
    <div
      style={{
        width: '300px',
        backgroundColor: '#000000',
        borderLeft: '2px solid #FF8800',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
    >
      {/* Panel Header */}
      <div
        style={{
          backgroundColor: '#1A1A1A',
          borderBottom: '1px solid #2A2A2A',
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Settings2
            size={14}
            style={{ color: '#FF8800', filter: 'drop-shadow(0 0 4px #FF8800)' }}
          />
          <span
            style={{
              color: '#FF8800',
              fontSize: '11px',
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
            color: '#787878',
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
          backgroundColor: '#0F0F0F',
          padding: '10px 12px',
          borderBottom: '1px solid #2A2A2A',
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
            fontSize: '12px',
            fontWeight: 700,
            color: nodeColor,
          }}
        >
          {(selectedNode.data.label || selectedNode.type || 'N')[0].toUpperCase()}
        </div>
        <div>
          <div style={{ color: '#FFFFFF', fontSize: '11px', fontWeight: 600 }}>
            {selectedNode.data.label || selectedNode.type}
          </div>
          <div style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
            {selectedNode.type}
          </div>
        </div>
      </div>

      {/* Scrollable Content */}
      <div style={{ flex: 1, overflow: 'auto', backgroundColor: '#000000' }}>
        {/* Label Section */}
        <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
          <label
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              color: '#FF8800',
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: '6px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            <span>LABEL</span>
            <span style={{ color: '#4A4A4A', fontSize: '8px', fontWeight: 400 }}>REQUIRED</span>
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
                backgroundColor: '#1A1A1A',
                borderTop: '1px solid #2A2A2A',
                borderLeft: '1px solid #2A2A2A',
                borderBottom: '1px solid #2A2A2A',
                padding: '0 8px',
                display: 'flex',
                alignItems: 'center',
                color: '#FF8800',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
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
                backgroundColor: '#0A0A0A',
                border: '1px solid #2A2A2A',
                borderLeft: 'none',
                padding: '10px 12px',
                color: '#FFFFFF',
                fontSize: '11px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                outline: 'none',
                transition: 'all 0.15s ease',
              }}
            />
          </div>
          <div
            style={{
              marginTop: '4px',
              fontSize: '8px',
              color: '#4A4A4A',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            }}
          >
            Display name for this node in the workflow
          </div>
        </div>

        {/* ==================== DATA SOURCE NODE CONFIG ==================== */}
        {isDataSourceNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Database size={14} color={FINCEPT.ORANGE} />
              <span style={{
                color: FINCEPT.ORANGE,
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                DATA SOURCE CONFIG
              </span>
            </div>

            {/* Connection Selector */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                CONNECTION
              </label>
              <select
                value={localConnection}
                onChange={(e) => {
                  setLocalConnection(e.target.value);
                  selectedNode.data.onConnectionChange?.(e.target.value);
                }}
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="">-- Select Connection --</option>
                {connections.filter(c => c.status === 'connected' || c.status === 'disconnected').map((conn) => (
                  <option key={conn.id} value={conn.id}>
                    {conn.name} ({conn.type})
                  </option>
                ))}
              </select>
              {connections.length === 0 && (
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '4px' }}>
                  No connections. Add them in Data Sources tab.
                </div>
              )}
            </div>

            {/* Query Editor */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                QUERY
              </label>
              <textarea
                value={localQuery}
                onChange={(e) => {
                  setLocalQuery(e.target.value);
                  selectedNode.data.onQueryChange?.(e.target.value);
                }}
                placeholder="SELECT * FROM table..."
                style={{
                  width: '100%',
                  height: '100px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  resize: 'vertical',
                  boxSizing: 'border-box',
                }}
              />
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              disabled={!localConnection || !localQuery}
              style={{
                width: '100%',
                backgroundColor: (!localConnection || !localQuery) ? FINCEPT.BORDER : FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                padding: '8px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: (!localConnection || !localQuery) ? 'not-allowed' : 'pointer',
                opacity: (!localConnection || !localQuery) ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
              }}
            >
              <Play size={10} />
              EXECUTE QUERY
            </button>
          </div>
        )}

        {/* ==================== TECHNICAL INDICATOR NODE CONFIG ==================== */}
        {isTechnicalIndicatorNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <TrendingUp size={14} color={FINCEPT.CYAN} />
              <span style={{
                color: FINCEPT.CYAN,
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                TECHNICAL INDICATOR CONFIG
              </span>
            </div>

            {/* Data Source */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                DATA SOURCE
              </label>
              <select
                value={tiDataSource}
                onChange={(e) => {
                  setTiDataSource(e.target.value);
                  selectedNode.data.onParameterChange?.({ dataSource: e.target.value });
                }}
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="yfinance">Yahoo Finance</option>
                <option value="csv">CSV File</option>
                <option value="json">JSON Data</option>
              </select>
            </div>

            {/* Symbol */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                SYMBOL
              </label>
              <input
                type="text"
                value={tiSymbol}
                onChange={(e) => {
                  setTiSymbol(e.target.value);
                  selectedNode.data.onParameterChange?.({ symbol: e.target.value });
                }}
                placeholder="AAPL, MSFT, BTC-USD..."
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  boxSizing: 'border-box',
                }}
              />
            </div>

            {/* Period */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                PERIOD
              </label>
              <select
                value={tiPeriod}
                onChange={(e) => {
                  setTiPeriod(e.target.value);
                  selectedNode.data.onParameterChange?.({ period: e.target.value });
                }}
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
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
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                INDICATOR CATEGORIES
              </label>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
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
                      padding: '4px 8px',
                      fontSize: '9px',
                      fontWeight: 600,
                      backgroundColor: tiCategories.includes(cat) ? FINCEPT.CYAN + '30' : FINCEPT.DARK_BG,
                      border: `1px solid ${tiCategories.includes(cat) ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                      color: tiCategories.includes(cat) ? FINCEPT.CYAN : FINCEPT.GRAY,
                      cursor: 'pointer',
                      textTransform: 'uppercase',
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
              style={{
                width: '100%',
                backgroundColor: FINCEPT.CYAN,
                color: FINCEPT.DARK_BG,
                border: 'none',
                padding: '8px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
              }}
            >
              <Play size={10} />
              CALCULATE INDICATORS
            </button>
          </div>
        )}

        {/* ==================== AGENT MEDIATOR NODE CONFIG ==================== */}
        {isAgentMediatorNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Brain size={14} color="#a855f7" />
              <span style={{
                color: '#a855f7',
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                AI MEDIATOR CONFIG
              </span>
            </div>

            {/* LLM Provider */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                LLM PROVIDER
              </label>
              <select
                value={amProvider}
                onChange={(e) => {
                  setAmProvider(e.target.value);
                  selectedNode.data.onConfigChange?.({ selectedProvider: e.target.value, customPrompt: amPrompt });
                }}
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="">-- Select Provider --</option>
                <option value="ollama">Ollama (Local)</option>
                <option value="openai">OpenAI</option>
                <option value="anthropic">Anthropic</option>
                <option value="groq">Groq</option>
              </select>
            </div>

            {/* Custom Prompt */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                CUSTOM PROMPT
              </label>
              <textarea
                value={amPrompt}
                onChange={(e) => {
                  setAmPrompt(e.target.value);
                  selectedNode.data.onConfigChange?.({ selectedProvider: amProvider, customPrompt: e.target.value });
                }}
                placeholder="Enter instructions for the AI agent..."
                style={{
                  width: '100%',
                  height: '100px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  resize: 'vertical',
                  boxSizing: 'border-box',
                }}
              />
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              disabled={!amProvider}
              style={{
                width: '100%',
                backgroundColor: !amProvider ? FINCEPT.BORDER : '#a855f7',
                color: FINCEPT.DARK_BG,
                border: 'none',
                padding: '8px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: !amProvider ? 'not-allowed' : 'pointer',
                opacity: !amProvider ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
              }}
            >
              <Play size={10} />
              RUN AI MEDIATOR
            </button>
          </div>
        )}

        {/* ==================== PYTHON AGENT NODE CONFIG ==================== */}
        {isPythonAgentNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <span style={{ fontSize: '14px' }}>🐍</span>
              <span style={{
                color: '#fbbf24',
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                PYTHON AGENT CONFIG
              </span>
            </div>

            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '12px' }}>
              <strong style={{ color: FINCEPT.WHITE }}>Agent:</strong> {selectedNode.data.agentType || 'Unknown'}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '12px' }}>
              <strong style={{ color: FINCEPT.WHITE }}>Category:</strong> {selectedNode.data.agentCategory || 'Unknown'}
            </div>

            {/* LLM Selection */}
            <div style={{ marginBottom: '12px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}>
                LLM PROVIDER
              </label>
              <select
                value={selectedNode.data.selectedLLM || 'active'}
                onChange={(e) => selectedNode.data.onLLMChange?.(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
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
              style={{
                width: '100%',
                backgroundColor: selectedNode.data.status === 'running' ? FINCEPT.BORDER : '#fbbf24',
                color: FINCEPT.DARK_BG,
                border: 'none',
                padding: '8px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: selectedNode.data.status === 'running' ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
              }}
            >
              <Play size={10} />
              {selectedNode.data.status === 'running' ? 'RUNNING...' : 'RUN AGENT'}
            </button>
          </div>
        )}

        {/* ==================== MCP TOOL NODE CONFIG ==================== */}
        {isMCPToolNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Wrench size={14} color={FINCEPT.ORANGE} />
              <span style={{
                color: FINCEPT.ORANGE,
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                MCP TOOL CONFIG
              </span>
            </div>

            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '8px' }}>
              <strong style={{ color: FINCEPT.WHITE }}>Server:</strong> {selectedNode.data.serverId || 'Unknown'}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '12px' }}>
              <strong style={{ color: FINCEPT.WHITE }}>Tool:</strong> {selectedNode.data.toolName || 'Unknown'}
            </div>

            {/* Dynamic parameters would go here based on tool schema */}
            <div style={{
              padding: '8px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              marginBottom: '12px',
            }}>
              Configure tool parameters in the node directly
            </div>

            {/* Execute Button */}
            <button
              onClick={() => selectedNode.data.onExecute?.()}
              style={{
                width: '100%',
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                padding: '8px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
              }}
            >
              <Play size={10} />
              EXECUTE TOOL
            </button>
          </div>
        )}

        {/* ==================== BACKTEST NODE CONFIG ==================== */}
        {isBacktestNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <BarChart3 size={14} color={FINCEPT.GREEN} />
              <span style={{
                color: FINCEPT.GREEN,
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                BACKTEST CONFIG
              </span>
            </div>

            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '10px',
              textAlign: 'center',
            }}>
              Backtest configuration coming soon.<br/>
              Connect data source and strategy nodes.
            </div>
          </div>
        )}

        {/* ==================== OPTIMIZATION NODE CONFIG ==================== */}
        {isOptimizationNode && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Zap size={14} color="#f97316" />
              <span style={{
                color: '#f97316',
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                OPTIMIZATION CONFIG
              </span>
            </div>

            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '10px',
              textAlign: 'center',
            }}>
              Optimization configuration coming soon.<br/>
              Connect backtest node for parameter optimization.
            </div>
          </div>
        )}

        {/* ==================== CUSTOM/REGISTRY NODE (no registryData) ==================== */}
        {selectedNode.type === 'custom' && !registryData && (
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Settings2 size={14} color={FINCEPT.ORANGE} />
              <span style={{
                color: FINCEPT.ORANGE,
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                NODE CONFIG
              </span>
            </div>

            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '10px',
              textAlign: 'center',
            }}>
              {selectedNode.data.nodeTypeName ? (
                <>
                  <div style={{ marginBottom: '8px', color: FINCEPT.WHITE }}>
                    <strong>Type:</strong> {selectedNode.data.nodeTypeName}
                  </div>
                  <div style={{ fontSize: '9px' }}>
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
          <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <span style={{ fontSize: '14px' }}>📊</span>
              <span style={{
                color: FINCEPT.GREEN,
                fontSize: '11px',
                fontWeight: 700,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}>
                RESULTS DISPLAY
              </span>
            </div>

            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '10px',
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
              borderBottom: '1px solid #3A3A3A',
              backgroundColor: '#111111',
            }}
          >
            {/* Parameters Header */}
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: '16px',
                paddingBottom: '10px',
                borderBottom: '1px solid #2A2A2A',
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
                    backgroundColor: '#FF8800',
                  }}
                />
                <span
                  style={{
                    color: '#FF8800',
                    fontSize: '13px',
                    fontWeight: 700,
                    textTransform: 'uppercase',
                    letterSpacing: '1.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  }}
                >
                  PARAMETERS
                </span>
              </div>
              <div
                style={{
                  backgroundColor: '#FF880030',
                  border: '1px solid #FF8800',
                  padding: '4px 10px',
                  fontSize: '12px',
                  fontWeight: 700,
                  color: '#FF8800',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
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
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #333333',
                    position: 'relative',
                  }}
                >
                  {/* Parameter Index Badge */}
                  <div
                    style={{
                      position: 'absolute',
                      top: '-1px',
                      left: '-1px',
                      backgroundColor: '#2A2A2A',
                      color: '#AAAAAA',
                      fontSize: '11px',
                      fontWeight: 700,
                      padding: '4px 8px',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      borderRight: '1px solid #444444',
                      borderBottom: '1px solid #444444',
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
                        borderColor: 'transparent #FF3B3B transparent transparent',
                      }}
                    />
                  )}

                  <div style={{ paddingTop: '12px' }}>
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
                borderTop: '1px solid #2A2A2A',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}
            >
              <div
                style={{
                  width: '8px',
                  height: '8px',
                  backgroundColor: '#00D66F',
                  borderRadius: '50%',
                  boxShadow: '0 0 6px #00D66F80',
                }}
              />
              <span
                style={{
                  color: '#AAAAAA',
                  fontSize: '11px',
                  fontWeight: 600,
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
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
        <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
          <div
            style={{
              color: '#FF8800',
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: '10px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            NODE INFO
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {/* Node ID */}
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                ID
              </span>
              <code style={{ color: '#FFFFFF', fontSize: '9px' }}>{selectedNode.id}</code>
            </div>

            {/* Position */}
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                POS
              </span>
              <span style={{ color: '#FFFFFF', fontSize: '9px' }}>
                {Math.round(selectedNode.position.x)}, {Math.round(selectedNode.position.y)}
              </span>
            </div>

            {/* Registry Type */}
            {selectedNode.data.nodeTypeName && (
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                  TYPE
                </span>
                <span style={{ color: '#FFFFFF', fontSize: '9px' }}>
                  {selectedNode.data.nodeTypeName}
                </span>
              </div>
            )}

            {/* Status */}
            {selectedNode.data.status && (
              <div
                style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}
              >
                <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                  STATUS
                </span>
                <span
                  style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    padding: '2px 6px',
                    backgroundColor:
                      selectedNode.data.status === 'completed'
                        ? '#00D66F20'
                        : selectedNode.data.status === 'error'
                          ? '#FF3B3B20'
                          : selectedNode.data.status === 'running'
                            ? '#0088FF20'
                            : '#78787820',
                    color:
                      selectedNode.data.status === 'completed'
                        ? '#00D66F'
                        : selectedNode.data.status === 'error'
                          ? '#FF3B3B'
                          : selectedNode.data.status === 'running'
                            ? '#0088FF'
                            : '#787878',
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
          <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
            <div
              style={{
                color: '#00E5FF',
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
            >
              DESCRIPTION
            </div>
            <div style={{ color: '#FFFFFF', fontSize: '10px', lineHeight: '1.5' }}>
              {registryData.description}
            </div>
          </div>
        )}

        {/* Error Display */}
        {selectedNode.data.error && (
          <div
            style={{
              margin: '12px',
              backgroundColor: '#FF3B3B15',
              border: '1px solid #FF3B3B',
              padding: '10px',
            }}
          >
            <div
              style={{
                color: '#FF3B3B',
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '4px',
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <AlertCircle size={10} />
              ERROR
            </div>
            <div style={{ color: '#FFFFFF', fontSize: '10px', lineHeight: '1.4' }}>
              {selectedNode.data.error}
            </div>
          </div>
        )}

        {/* Result Preview */}
        {selectedNode.data.result && selectedNode.data.status === 'completed' && (
          <div
            style={{
              margin: '12px',
              backgroundColor: '#00D66F15',
              border: '1px solid #00D66F',
              padding: '10px',
            }}
          >
            <div
              style={{
                color: '#00D66F',
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}
            >
              OUTPUT PREVIEW
            </div>
            <pre
              style={{
                color: '#FFFFFF',
                fontSize: '9px',
                lineHeight: '1.4',
                overflow: 'auto',
                maxHeight: '80px',
                margin: 0,
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
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
          padding: '10px 12px',
          backgroundColor: '#0F0F0F',
          borderTop: '1px solid #2A2A2A',
          display: 'flex',
          gap: '8px',
        }}
      >
        <button
          onClick={onDelete}
          style={{
            flex: 1,
            backgroundColor: 'transparent',
            color: '#FF3B3B',
            border: '1px solid #FF3B3B',
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            textTransform: 'uppercase',
            letterSpacing: '0.3px',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#FF3B3B';
            e.currentTarget.style.color = '#000000';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = '#FF3B3B';
          }}
        >
          <Trash2 size={10} />
          DELETE
        </button>
        <button
          onClick={onDuplicate}
          style={{
            flex: 1,
            backgroundColor: '#FF8800',
            color: '#000000',
            border: '1px solid #FF8800',
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            textTransform: 'uppercase',
            letterSpacing: '0.3px',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#FFA033';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = '#FF8800';
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
