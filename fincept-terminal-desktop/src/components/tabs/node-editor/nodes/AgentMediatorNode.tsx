// AgentMediatorNode.tsx - LLM-powered mediator between agents
import React, { useState, useEffect } from 'react';
import { Position } from 'reactflow';
import { MessageSquare, Settings, CheckCircle, AlertCircle, Loader, Brain } from 'lucide-react';
import { sqliteService, LLMConfig, LLMModelConfig } from '@/services/core/sqliteService';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  BaseNode,
  NodeHeader,
  IconButton,
  Button,
  SelectField,
  TextareaField,
  InfoPanel,
  StatusPanel,
  SettingsPanel,
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
  const [availableProviders, setAvailableProviders] = useState<LLMOption[]>([]);
  const [localProvider, setLocalProvider] = useState(data.selectedProvider || 'active');
  const [localPrompt, setLocalPrompt] = useState(data.customPrompt || '');

  // Load available LLM providers (both LLMConfig and LLMModelConfig)
  useEffect(() => {
    const loadProviders = async () => {
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

        setAvailableProviders(options);
      } catch (error) {
        console.error('Failed to load LLM providers:', error);
        // Default to Fincept if loading fails
        setAvailableProviders([{
          id: 'fincept',
          provider: 'fincept',
          model: 'fincept-llm',
          displayName: 'FINCEPT (Pre-configured)',
          type: 'fincept',
          isActive: true
        }]);
      }
    };
    loadProviders();
  }, []);

  const statusColor = getStatusColor(data.status);

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={FINCEPT.BLUE} />;
      case 'completed': return <CheckCircle size={14} color={FINCEPT.GREEN} />;
      case 'error': return <AlertCircle size={14} color={FINCEPT.RED} />;
      default: return <Brain size={14} color={FINCEPT.ORANGE} />;
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
    const provider = availableProviders.find(p => p.id === data.selectedProvider);
    return provider ? provider.displayName : data.selectedProvider;
  };

  const providerOptions = [
    { value: 'active', label: 'Use Active Provider (from Settings)' },
    ...availableProviders.map(p => ({
      value: p.id,
      label: `${p.displayName}${p.isActive ? ' ✓' : ''}`
    }))
  ];

  return (
    <BaseNode
      selected={selected}
      minWidth="280px"
      maxWidth="350px"
      borderColor={statusColor}
      handles={[
        { type: 'target', position: Position.Left, color: FINCEPT.BLUE },
        { type: 'source', position: Position.Right, color: data.status === 'completed' ? FINCEPT.GREEN : FINCEPT.BLUE },
      ]}
    >
      {/* Header */}
      <NodeHeader
        icon={<MessageSquare size={20} />}
        title={data.label}
        subtitle="LLM MEDIATOR"
        color={FINCEPT.BLUE}
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
        {/* Provider Selection */}
        <SelectField
          label="LLM Provider"
          value={localProvider}
          options={providerOptions}
          onChange={setLocalProvider}
        />

        {/* Custom Prompt */}
        <TextareaField
          label="Custom Mediation Prompt (optional)"
          value={localPrompt}
          onChange={setLocalPrompt}
          placeholder="Leave empty for default prompt"
          minHeight="60px"
        />

        <Button
          label="SAVE CONFIG"
          onClick={handleSaveConfig}
          variant="primary"
          fullWidth
        />
      </SettingsPanel>

      {/* Main Content */}
      {!showSettings && (
        <div style={{ padding: SPACING.LG }}>
          {/* LLM Provider Display */}
          <InfoPanel title="LLM PROVIDER">
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '8px',
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.XS
            }}>
              <Brain size={10} color={FINCEPT.BLUE} />
              {getProviderLabel()}
            </div>
          </InfoPanel>

          {/* Input Data Preview */}
          {data.inputData && (
            <div style={{
              backgroundColor: `${FINCEPT.GRAY}15`,
              border: `1px solid ${FINCEPT.GRAY}`,
              borderRadius: BORDER_RADIUS.LG,
              padding: SPACING.SM,
              marginBottom: SPACING.MD
            }}>
              <div style={{
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: SPACING.XS
              }}>
                INPUT DATA
              </div>
              <div style={{
                color: FINCEPT.WHITE,
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
            <StatusPanel
              type="success"
              icon={<CheckCircle size={10} />}
              message={typeof data.result === 'string'
                ? data.result.substring(0, 150) + '...'
                : JSON.stringify(data.result, null, 2).substring(0, 150) + '...'}
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

          {/* Action Buttons */}
          <Button
            label={data.status === 'running' ? 'MEDIATING...' : 'MEDIATE'}
            icon={data.status === 'running' ? <Loader size={12} className="animate-spin" /> : <Brain size={12} />}
            onClick={() => data.onExecute?.(id)}
            variant="primary"
            disabled={data.status === 'running'}
            fullWidth
          />
        </div>
      )}
    </BaseNode>
  );
};

export default AgentMediatorNode;
