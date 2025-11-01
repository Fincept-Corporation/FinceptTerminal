// AgentMediatorNode.tsx - LLM-powered mediator between agents
import React, { useState, useEffect } from 'react';
import { Handle, Position } from 'reactflow';
import { MessageSquare, Settings, CheckCircle, AlertCircle, Loader, Brain } from 'lucide-react';
import { sqliteService, LLMConfig } from '@/services/sqliteService';

interface AgentMediatorNodeProps {
  id: string;
  data: {
    label: string;
    selectedProvider?: string; // Which LLM provider to use for mediation
    customPrompt?: string; // Custom mediation prompt
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: string; // Mediated context
    error?: string;
    inputData?: any; // Data from previous agent
    onExecute?: (nodeId: string) => void;
    onConfigChange?: (config: { selectedProvider?: string; customPrompt?: string }) => void;
  };
  selected: boolean;
}

const AgentMediatorNode: React.FC<AgentMediatorNodeProps> = ({ id, data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [availableProviders, setAvailableProviders] = useState<LLMConfig[]>([]);
  const [localProvider, setLocalProvider] = useState(data.selectedProvider || 'active');
  const [localPrompt, setLocalPrompt] = useState(data.customPrompt || '');

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const RED = '#ef4444';
  const BLUE = '#3b82f6';

  // Load available LLM providers
  useEffect(() => {
    const loadProviders = async () => {
      try {
        const configs = await sqliteService.getLLMConfigs();
        setAvailableProviders(configs);
      } catch (error) {
        console.error('Failed to load LLM providers:', error);
      }
    };
    loadProviders();
  }, []);

  const getStatusColor = () => {
    switch (data.status) {
      case 'running': return BLUE;
      case 'completed': return GREEN;
      case 'error': return RED;
      default: return ORANGE;
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={BLUE} />;
      case 'completed': return <CheckCircle size={14} color={GREEN} />;
      case 'error': return <AlertCircle size={14} color={RED} />;
      default: return <Brain size={14} color={ORANGE} />;
    }
  };

  const handleSaveConfig = () => {
    data.onConfigChange?.({
      selectedProvider: localProvider === 'active' ? undefined : localProvider,
      customPrompt: localPrompt || undefined
    });
    setShowSettings(false);
  };

  const getProviderLabel = () => {
    if (!data.selectedProvider || data.selectedProvider === 'active') {
      return 'Active Provider';
    }
    const provider = availableProviders.find(p => p.provider === data.selectedProvider);
    return provider ? `${provider.provider.toUpperCase()} - ${provider.model}` : data.selectedProvider;
  };

  return (
    <div style={{
      backgroundColor: PANEL_BG,
      border: `2px solid ${selected ? ORANGE : getStatusColor()}`,
      borderRadius: '8px',
      padding: '12px',
      minWidth: '280px',
      maxWidth: '350px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px ${ORANGE}60` : `0 2px 8px rgba(0,0,0,0.3)`
    }}>
      {/* Input Handle */}
      <Handle
        type="target"
        position={Position.Left}
        style={{
          background: BLUE,
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
        <MessageSquare size={20} color={BLUE} />
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
            LLM MEDIATOR
          </div>
        </div>
        {getStatusIcon()}
      </div>

      {/* LLM Provider Display */}
      <div style={{
        backgroundColor: DARK_BG,
        border: `1px solid ${BORDER}`,
        borderRadius: '4px',
        padding: '6px',
        marginBottom: '8px'
      }}>
        <div style={{
          color: BLUE,
          fontSize: '9px',
          fontWeight: 'bold',
          marginBottom: '4px'
        }}>
          LLM PROVIDER
        </div>
        <div style={{
          color: WHITE,
          fontSize: '8px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}>
          <Brain size={10} color={BLUE} />
          {getProviderLabel()}
        </div>
      </div>

      {/* Input Data Preview */}
      {data.inputData && (
        <div style={{
          backgroundColor: `${GRAY}15`,
          border: `1px solid ${GRAY}`,
          borderRadius: '4px',
          padding: '6px',
          marginBottom: '8px'
        }}>
          <div style={{
            color: GRAY,
            fontSize: '9px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            INPUT DATA
          </div>
          <div style={{
            color: WHITE,
            fontSize: '8px',
            fontFamily: 'monospace',
            maxHeight: '50px',
            overflow: 'auto'
          }}>
            {JSON.stringify(data.inputData, null, 2).substring(0, 80)}...
          </div>
        </div>
      )}

      {/* Mediated Result */}
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
            MEDIATED CONTEXT
          </div>
          <div style={{
            color: WHITE,
            fontSize: '8px',
            maxHeight: '80px',
            overflow: 'auto',
            lineHeight: '1.4'
          }}>
            {typeof data.result === 'string'
              ? data.result.substring(0, 150)
              : JSON.stringify(data.result, null, 2).substring(0, 150)
            }...
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

      {/* Settings Panel */}
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
            MEDIATOR SETTINGS
          </div>

          {/* Provider Selection */}
          <div style={{ marginBottom: '8px' }}>
            <label style={{
              color: GRAY,
              fontSize: '8px',
              display: 'block',
              marginBottom: '4px'
            }}>
              LLM Provider
            </label>
            <select
              value={localProvider}
              onChange={(e) => setLocalProvider(e.target.value)}
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
              <option value="active">Use Active Provider</option>
              {availableProviders.map(provider => (
                <option key={provider.provider} value={provider.provider}>
                  {provider.provider.toUpperCase()} - {provider.model}
                </option>
              ))}
            </select>
          </div>

          {/* Custom Prompt */}
          <div style={{ marginBottom: '8px' }}>
            <label style={{
              color: GRAY,
              fontSize: '8px',
              display: 'block',
              marginBottom: '4px'
            }}>
              Custom Mediation Prompt (optional)
            </label>
            <textarea
              value={localPrompt}
              onChange={(e) => setLocalPrompt(e.target.value)}
              placeholder="Leave empty for default prompt"
              rows={3}
              style={{
                width: '100%',
                backgroundColor: PANEL_BG,
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '4px',
                fontSize: '8px',
                borderRadius: '3px',
                resize: 'vertical',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          <button
            onClick={handleSaveConfig}
            style={{
              width: '100%',
              backgroundColor: BLUE,
              color: 'white',
              border: 'none',
              padding: '6px',
              fontSize: '9px',
              fontWeight: 'bold',
              cursor: 'pointer',
              borderRadius: '3px'
            }}
          >
            SAVE CONFIG
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
            backgroundColor: data.status === 'running' ? GRAY : BLUE,
            color: 'white',
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
              MEDIATING...
            </>
          ) : (
            <>
              <Brain size={12} />
              MEDIATE
            </>
          )}
        </button>
        <button
          onClick={() => setShowSettings(!showSettings)}
          style={{
            backgroundColor: showSettings ? BLUE : DARK_BG,
            border: `1px solid ${BORDER}`,
            color: showSettings ? WHITE : GRAY,
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
          background: data.status === 'completed' ? GREEN : BLUE,
          width: '12px',
          height: '12px',
          border: `2px solid ${DARK_BG}`,
          right: '-6px'
        }}
      />
    </div>
  );
};

export default AgentMediatorNode;
