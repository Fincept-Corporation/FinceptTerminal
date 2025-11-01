// PythonAgentNode.tsx - Visual node for Python agents in Node Editor
import React, { useState, useEffect } from 'react';
import { Handle, Position } from 'reactflow';
import { Play, Settings, CheckCircle, AlertCircle, Loader, TrendingUp, Brain } from 'lucide-react';
import { sqliteService, LLMConfig } from '@/services/sqliteService';

interface PythonAgentNodeProps {
  id: string;
  data: {
    agentType: string;
    agentCategory: string;
    label: string;
    icon: string;
    color: string;
    parameters: Record<string, any>;
    selectedLLM?: string; // Which LLM provider to use for this agent
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: any;
    error?: string;
    onExecute?: (nodeId: string) => void;
    onParameterChange?: (params: Record<string, any>) => void;
    onLLMChange?: (provider: string) => void;
  };
  selected: boolean;
}

const PythonAgentNode: React.FC<PythonAgentNodeProps> = ({ id, data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [availableLLMs, setAvailableLLMs] = useState<LLMConfig[]>([]);
  const [selectedLLM, setSelectedLLM] = useState(data.selectedLLM || 'active');

  // Load available LLM providers from Settings
  useEffect(() => {
    const loadLLMs = async () => {
      try {
        const configs = await sqliteService.getLLMConfigs();
        setAvailableLLMs(configs);
      } catch (error) {
        console.error('Failed to load LLM configs:', error);
      }
    };
    loadLLMs();
  }, []);

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const RED = '#ef4444';

  const getStatusColor = () => {
    switch (data.status) {
      case 'running': return ORANGE;
      case 'completed': return GREEN;
      case 'error': return RED;
      default: return GRAY;
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={ORANGE} />;
      case 'completed': return <CheckCircle size={14} color={GREEN} />;
      case 'error': return <AlertCircle size={14} color={RED} />;
      default: return <TrendingUp size={14} color={GRAY} />;
    }
  };

  return (
    <div style={{
      backgroundColor: PANEL_BG,
      border: `2px solid ${selected ? ORANGE : getStatusColor()}`,
      borderRadius: '8px',
      padding: '12px',
      minWidth: '250px',
      maxWidth: '320px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px ${ORANGE}60` : `0 2px 8px rgba(0,0,0,0.3)`
    }}>
      {/* Input Handle */}
      <Handle
        type="target"
        position={Position.Left}
        style={{
          background: data.color,
          width: '12px',
          height: '12px',
          border: `2px solid ${DARK_BG}`,
          left: '-6px'
        }}
      />

      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        marginBottom: '10px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${BORDER}`
      }}>
        <span style={{ fontSize: '20px' }}>{data.icon}</span>
        <div style={{ flex: 1 }}>
          <div style={{
            color: WHITE,
            fontSize: '12px',
            fontWeight: 'bold',
            marginBottom: '2px'
          }}>
            {data.label}
          </div>
          <div style={{
            color: GRAY,
            fontSize: '9px',
            textTransform: 'uppercase'
          }}>
            {data.agentCategory}
          </div>
        </div>
        {getStatusIcon()}
      </div>

      {/* Parameters Summary */}
      {Object.keys(data.parameters).length > 0 && (
        <div style={{
          backgroundColor: DARK_BG,
          border: `1px solid ${BORDER}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: ORANGE,
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            PARAMETERS
          </div>
          {Object.entries(data.parameters).slice(0, 2).map(([key, value]) => (
            <div key={key} style={{
              color: GRAY,
              fontSize: '8px',
              marginBottom: '2px',
              display: 'flex',
              justifyContent: 'space-between'
            }}>
              <span>{key}:</span>
              <span style={{ color: WHITE }}>
                {Array.isArray(value) ? value.join(', ') : String(value).substring(0, 20)}
              </span>
            </div>
          ))}
          {Object.keys(data.parameters).length > 2 && (
            <div style={{ color: GRAY, fontSize: '8px', fontStyle: 'italic' }}>
              +{Object.keys(data.parameters).length - 2} more...
            </div>
          )}
        </div>
      )}

      {/* Result Preview */}
      {data.status === 'completed' && data.result && (
        <div style={{
          backgroundColor: `${GREEN}15`,
          border: `1px solid ${GREEN}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: GREEN,
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <CheckCircle size={10} />
            RESULT
          </div>
          <div style={{
            color: WHITE,
            fontSize: '8px',
            fontFamily: 'monospace',
            maxHeight: '60px',
            overflow: 'auto'
          }}>
            {JSON.stringify(data.result, null, 2).substring(0, 100)}...
          </div>
        </div>
      )}

      {/* Error Display */}
      {data.status === 'error' && data.error && (
        <div style={{
          backgroundColor: `${RED}15`,
          border: `1px solid ${RED}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: RED,
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <AlertCircle size={10} />
            ERROR
          </div>
          <div style={{
            color: RED,
            fontSize: '8px',
            wordBreak: 'break-word'
          }}>
            {data.error}
          </div>
        </div>
      )}

      {/* LLM Display - Always visible, clickable to configure */}
      <div
        onClick={() => setShowSettings(!showSettings)}
        style={{
          backgroundColor: DARK_BG,
          border: `1px solid ${showSettings ? ORANGE : BORDER}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px',
          cursor: 'pointer',
          transition: 'border-color 0.2s'
        }}
      >
        <div style={{
          color: GRAY,
          fontSize: '9px',
          fontWeight: 'bold',
          marginBottom: '4px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: '4px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <Brain size={10} />
            LLM
          </div>
          <Settings size={10} color={showSettings ? ORANGE : GRAY} />
        </div>
        <div style={{
          color: WHITE,
          fontSize: '8px'
        }}>
          {selectedLLM === 'active' ? 'Active Provider' : selectedLLM.toUpperCase()}
        </div>
      </div>

      {/* LLM Selection Panel */}
      {showSettings && (
        <div style={{
          backgroundColor: DARK_BG,
          border: `1px solid ${ORANGE}`,
          borderRadius: '4px',
          padding: '8px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: ORANGE,
            fontSize: '10px',
            fontWeight: 'bold',
            marginBottom: '8px'
          }}>
            LLM CONFIGURATION
          </div>

          <div style={{ marginBottom: '8px' }}>
            <label style={{
              color: GRAY,
              fontSize: '8px',
              display: 'block',
              marginBottom: '4px'
            }}>
              LLM Provider for Analysis
            </label>
            <select
              value={selectedLLM}
              onChange={(e) => {
                setSelectedLLM(e.target.value);
                data.onLLMChange?.(e.target.value);
              }}
              style={{
                width: '100%',
                backgroundColor: PANEL_BG,
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '4px',
                fontSize: '8px',
                borderRadius: '3px'
              }}
            >
              <option value="active">Use Active Provider (from Settings)</option>
              {availableLLMs.map(llm => (
                <option key={llm.provider} value={llm.provider}>
                  {llm.provider.toUpperCase()} - {llm.model}
                </option>
              ))}
            </select>
          </div>

          <button
            onClick={(e) => {
              e.stopPropagation();
              setShowSettings(false);
            }}
            style={{
              width: '100%',
              backgroundColor: ORANGE,
              color: 'black',
              border: 'none',
              padding: '6px',
              fontSize: '9px',
              fontWeight: 'bold',
              cursor: 'pointer',
              borderRadius: '3px'
            }}
          >
            CLOSE
          </button>
        </div>
      )}

      {/* Action Buttons */}
      <div style={{ display: 'flex', gap: '6px' }}>
        <button
          onClick={() => data.onExecute?.(id)}
          disabled={data.status === 'running'}
          style={{
            flex: 1,
            backgroundColor: data.status === 'running' ? GRAY : data.color,
            color: 'black',
            border: 'none',
            padding: '8px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: data.status === 'running' ? 'not-allowed' : 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            opacity: data.status === 'running' ? 0.6 : 1
          }}
        >
          {data.status === 'running' ? (
            <>
              <Loader size={12} className="animate-spin" />
              RUNNING...
            </>
          ) : (
            <>
              <Play size={12} />
              EXECUTE
            </>
          )}
        </button>
        <button
          onClick={() => setShowSettings(!showSettings)}
          style={{
            backgroundColor: showSettings ? ORANGE : DARK_BG,
            border: `1px solid ${BORDER}`,
            color: showSettings ? 'black' : GRAY,
            padding: '8px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center'
          }}
        >
          <Settings size={14} />
        </button>
      </div>

      {/* Output Handle */}
      <Handle
        type="source"
        position={Position.Right}
        style={{
          background: data.status === 'completed' ? GREEN : data.color,
          width: '12px',
          height: '12px',
          border: `2px solid ${DARK_BG}`,
          right: '-6px'
        }}
      />
    </div>
  );
};

export default PythonAgentNode;
