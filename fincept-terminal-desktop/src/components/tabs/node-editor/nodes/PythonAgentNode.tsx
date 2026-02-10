// PythonAgentNode.tsx - Visual node for Python agents in Node Editor
import React, { useState, useEffect } from 'react';
import { Position } from 'reactflow';
import { Play, Settings, CheckCircle, AlertCircle, Loader, Brain } from 'lucide-react';
import { sqliteService, LLMConfig, LLMModelConfig } from '@/services/core/sqliteService';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  BaseNode,
  NodeHeader,
  IconButton,
  Button,
  SelectField,
  InfoPanel,
  StatusPanel,
  SettingsPanel,
  KeyValue,
  getStatusColor,
} from './shared';

// Combined LLM option for display
interface LLMOption {
  id: string;
  provider: string;
  model: string;
  displayName: string;
  type: 'provider' | 'model' | 'fincept';
  isActive?: boolean;
}

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
  const [availableLLMs, setAvailableLLMs] = useState<LLMOption[]>([]);
  const [selectedLLM, setSelectedLLM] = useState(data.selectedLLM || 'active');

  // Load available LLM providers from Settings (both LLMConfig and LLMModelConfig)
  useEffect(() => {
    const loadLLMs = async () => {
      try {
        const options: LLMOption[] = [];

        // Load provider-level configs (LLMConfig)
        const providerConfigs = await sqliteService.getLLMConfigs();
        for (const config of providerConfigs) {
          // Fincept is special - always show it prominently
          if (config.provider === 'fincept') {
            options.unshift({
              id: 'fincept',
              provider: 'fincept',
              model: config.model || 'fincept-llm',
              displayName: 'FINCEPT (Pre-configured)',
              type: 'fincept',
              isActive: config.is_active
            });
          } else {
            options.push({
              id: config.provider,
              provider: config.provider,
              model: config.model,
              displayName: `${config.provider.toUpperCase()} - ${config.model}`,
              type: 'provider',
              isActive: config.is_active
            });
          }
        }

        // Load custom model configs (LLMModelConfig) from Settings
        const modelConfigs = await sqliteService.getLLMModelConfigs();
        for (const model of modelConfigs) {
          if (model.is_enabled) {
            options.push({
              id: model.id,
              provider: model.provider,
              model: model.model_id,
              displayName: `${model.display_name} (${model.provider.toUpperCase()})`,
              type: 'model',
              isActive: model.is_default
            });
          }
        }

        // If Fincept wasn't in provider configs, add it as default
        if (!options.find(o => o.provider === 'fincept')) {
          options.unshift({
            id: 'fincept',
            provider: 'fincept',
            model: 'fincept-llm',
            displayName: 'FINCEPT (Pre-configured)',
            type: 'fincept',
            isActive: true
          });
        }

        setAvailableLLMs(options);
      } catch (error) {
        console.error('Failed to load LLM configs:', error);
        // Default to Fincept if loading fails
        setAvailableLLMs([{
          id: 'fincept',
          provider: 'fincept',
          model: 'fincept-llm',
          displayName: 'FINCEPT (Pre-configured)',
          type: 'fincept',
          isActive: true
        }]);
      }
    };
    loadLLMs();
  }, []);

  const statusColor = getStatusColor(data.status);

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={FINCEPT.ORANGE} />;
      case 'completed': return <CheckCircle size={14} color={FINCEPT.GREEN} />;
      case 'error': return <AlertCircle size={14} color={FINCEPT.RED} />;
      default: return null;
    }
  };

  const llmOptions = [
    { value: 'active', label: 'Use Active Provider (from Settings)' },
    ...availableLLMs.map(llm => ({
      value: llm.id,
      label: `${llm.displayName}${llm.isActive ? ' ✓' : ''}`
    }))
  ];

  return (
    <BaseNode
      selected={selected}
      minWidth="250px"
      maxWidth="320px"
      borderColor={statusColor}
      handles={[
        { type: 'target', position: Position.Left, color: data.color },
        { type: 'source', position: Position.Right, color: data.status === 'completed' ? FINCEPT.GREEN : data.color },
      ]}
    >
      {/* Header */}
      <NodeHeader
        icon={<span style={{ fontSize: '20px' }}>{data.icon}</span>}
        title={data.label}
        subtitle={data.agentCategory}
        color={FINCEPT.WHITE}
        rightActions={
          <>
            {getStatusIcon()}
            <IconButton
              icon={<Settings size={14} />}
              onClick={() => setShowSettings(!showSettings)}
              active={showSettings}
            />
          </>
        }
      />

      {/* Settings Panel */}
      <SettingsPanel isOpen={showSettings}>
        <SelectField
          label="LLM Provider for Analysis"
          value={selectedLLM}
          options={llmOptions}
          onChange={(value) => {
            setSelectedLLM(value);
            data.onLLMChange?.(value);
          }}
        />

        <Button
          label="CLOSE"
          onClick={() => setShowSettings(false)}
          variant="primary"
          fullWidth
        />
      </SettingsPanel>

      {/* Main Content */}
      {!showSettings && (
        <div style={{ padding: SPACING.LG }}>
          {/* Parameters Summary */}
          {Object.keys(data.parameters).length > 0 && (
            <InfoPanel title="PARAMETERS">
              {Object.entries(data.parameters).slice(0, 2).map(([key, value]) => (
                <KeyValue
                  key={key}
                  label={key}
                  value={Array.isArray(value) ? value.join(', ') : String(value).substring(0, 20)}
                />
              ))}
              {Object.keys(data.parameters).length > 2 && (
                <div style={{ color: FINCEPT.GRAY, fontSize: '8px', fontStyle: 'italic' }}>
                  +{Object.keys(data.parameters).length - 2} more...
                </div>
              )}
            </InfoPanel>
          )}

          {/* LLM Display - Clickable to configure */}
          <div
            onClick={() => setShowSettings(!showSettings)}
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${showSettings ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              borderRadius: BORDER_RADIUS.LG,
              padding: SPACING.SM,
              marginBottom: SPACING.MD,
              cursor: 'pointer',
              transition: 'border-color 0.2s'
            }}
          >
            <div style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: SPACING.XS,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              gap: SPACING.XS
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.XS }}>
                <Brain size={10} />
                LLM
              </div>
              <Settings size={10} color={showSettings ? FINCEPT.ORANGE : FINCEPT.GRAY} />
            </div>
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '8px'
            }}>
              {selectedLLM === 'active'
                ? 'Active Provider'
                : (availableLLMs.find(l => l.id === selectedLLM)?.displayName || selectedLLM.toUpperCase())
              }
            </div>
          </div>

          {/* Result Preview */}
          {data.status === 'completed' && data.result && (
            <StatusPanel
              type="success"
              icon={<CheckCircle size={10} />}
              message={JSON.stringify(data.result, null, 2).substring(0, 100) + '...'}
            />
          )}

          {/* Error Display */}
          {data.status === 'error' && data.error && (
            <StatusPanel
              type="error"
              icon={<AlertCircle size={10} />}
              message={data.error}
            />
          )}

          {/* Action Button */}
          <Button
            label={data.status === 'running' ? 'RUNNING...' : 'EXECUTE'}
            icon={data.status === 'running' ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
            onClick={() => data.onExecute?.(id)}
            disabled={data.status === 'running'}
            fullWidth
            variant="primary"
          />
        </div>
      )}
    </BaseNode>
  );
};

export default PythonAgentNode;
