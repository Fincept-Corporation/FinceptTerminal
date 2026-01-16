/**
 * Agent Configuration UI
 *
 * Complete control panel for creating and configuring AI trading agents.
 * Includes all Alpha Arena features: auto-trading, TP/SL, confidence, safety limits.
 */

import React, { useState } from 'react';
import {
  Bot, Brain, Shield, Target, Zap, Settings, AlertTriangle,
  Clock, TrendingUp, Activity, BarChart3, Sliders, Play, Pause,
  Power, Info, ChevronDown, ChevronUp, Save
} from 'lucide-react';
import agnoTradingService, { type AgentConfig } from '../../../../services/trading/agnoTradingService';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

interface AgentConfigUIProps {
  onAgentCreated: (agent: any) => void;
}

export function AgentConfigurationUI({ onAgentCreated }: AgentConfigUIProps) {
  // Basic Config
  const [agentName, setAgentName] = useState('');
  const [agentType, setAgentType] = useState('trading_strategy');
  const [description, setDescription] = useState('');

  // LLM Config
  const [provider, setProvider] = useState('openai');
  const [model, setModel] = useState('gpt-4o-mini');
  const [temperature, setTemperature] = useState(0.7);

  // Trading Config
  const [symbols, setSymbols] = useState(['BTC/USD']);
  const [autoTrading, setAutoTrading] = useState(false);
  const [executionInterval, setExecutionInterval] = useState(180); // 3 minutes like Alpha Arena

  // Safety Limits
  const [maxPositionSize, setMaxPositionSize] = useState(0.1); // 10%
  const [maxLeverage, setMaxLeverage] = useState(1.0);
  const [maxDrawdown, setMaxDrawdown] = useState(0.15); // 15%
  const [maxDailyLoss, setMaxDailyLoss] = useState(0.05); // 5%
  const [minConfidence, setMinConfidence] = useState(0.6);
  const [maxOpenPositions, setMaxOpenPositions] = useState(5);

  // TP/SL Config
  const [useDynamicTPSL, setUseDynamicTPSL] = useState(true);
  const [riskRewardRatio, setRiskRewardRatio] = useState(2.0);
  const [stopLossPercent, setStopLossPercent] = useState(2.0);
  const [takeProfitPercent, setTakeProfitPercent] = useState(5.0);
  const [useTrailingStop, setUseTrailingStop] = useState(false);
  const [trailingPercent, setTrailingPercent] = useState(1.0);

  // Advanced Config
  const [enableMemory, setEnableMemory] = useState(true);
  const [enableKnowledge, setEnableKnowledge] = useState(false);
  const [customInstructions, setCustomInstructions] = useState('');

  // UI State
  const [expandedSections, setExpandedSections] = useState<Set<string>>(
    new Set(['basic', 'llm', 'trading'])
  );
  const [isCreating, setIsCreating] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const toggleSection = (section: string) => {
    const newExpanded = new Set(expandedSections);
    if (newExpanded.has(section)) {
      newExpanded.delete(section);
    } else {
      newExpanded.add(section);
    }
    setExpandedSections(newExpanded);
  };

  const handleCreateAgent = async () => {
    setIsCreating(true);
    setError(null);

    try {
      const config: AgentConfig = {
        name: agentName || 'Trading Agent',
        role: agentTypeToRole(agentType),
        description: description || getDefaultDescription(agentType),
        agent_model: `${provider}:${model}`,
        temperature,
        tools: getToolsForAgentType(agentType),
        symbols,
        enable_memory: enableMemory,
        risk_tolerance: getRiskTolerance(),
        instructions: customInstructions.split('\n').filter(i => i.trim())
      };

      const response = await agnoTradingService.createAgent(config);

      if (response.success && response.data) {
        onAgentCreated({
          ...response.data,
          autoTrading: {
            enabled: autoTrading,
            interval: executionInterval,
            safety: {
              maxPositionSize,
              maxLeverage,
              maxDrawdown,
              maxDailyLoss,
              minConfidence,
              maxOpenPositions
            },
            tpsl: {
              dynamic: useDynamicTPSL,
              riskReward: riskRewardRatio,
              slPercent: stopLossPercent,
              tpPercent: takeProfitPercent,
              trailing: useTrailingStop,
              trailingPercent
            }
          }
        });
      } else {
        setError(response.error || 'Failed to create agent');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setIsCreating(false);
    }
  };

  const agentTypeToRole = (type: string): string => {
    const roles: Record<string, string> = {
      'market_analyst': 'Market Analysis Specialist',
      'trading_strategy': 'Trading Strategy Expert',
      'risk_manager': 'Risk Management Specialist',
      'portfolio_manager': 'Portfolio Manager',
      'sentiment_analyst': 'Sentiment Analysis Expert'
    };
    return roles[type] || 'Trading Agent';
  };

  const getDefaultDescription = (type: string): string => {
    const descriptions: Record<string, string> = {
      'market_analyst': 'Analyzes market data, technical indicators, and trends to provide insights',
      'trading_strategy': 'Generates trade signals with entry/exit points and risk management',
      'risk_manager': 'Monitors portfolio risk and ensures compliance with safety limits',
      'portfolio_manager': 'Manages asset allocation and portfolio optimization',
      'sentiment_analyst': 'Analyzes market sentiment from news and social media'
    };
    return descriptions[type] || 'AI-powered trading agent';
  };

  const getToolsForAgentType = (type: string): string[] => {
    const toolMap: Record<string, string[]> = {
      'market_analyst': ['market_data', 'technical_analysis'],
      'trading_strategy': ['market_data', 'technical_analysis', 'order_execution'],
      'risk_manager': ['portfolio', 'market_data'],
      'portfolio_manager': ['portfolio', 'market_data'],
      'sentiment_analyst': ['news_sentiment', 'market_data']
    };
    return toolMap[type] || ['market_data'];
  };

  const getRiskTolerance = (): string => {
    if (maxDrawdown <= 0.10) return 'conservative';
    if (maxDrawdown >= 0.20) return 'aggressive';
    return 'moderate';
  };

  return (
    <div style={{ padding: '8px', height: '100%', overflow: 'auto' }}>
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>

      {/* Basic Configuration */}
      <ConfigSection
        title="BASIC CONFIGURATION"
        icon={<Bot size={14} />}
        expanded={expandedSections.has('basic')}
        onToggle={() => toggleSection('basic')}
      >
        <div style={{ display: 'grid', gap: '12px' }}>
          <InputField label="Agent Name" value={agentName} onChange={setAgentName} placeholder="My Trading Agent" />

          <SelectField
            label="Agent Type"
            value={agentType}
            onChange={setAgentType}
            options={[
              { value: 'market_analyst', label: 'Market Analyst' },
              { value: 'trading_strategy', label: 'Trading Strategy' },
              { value: 'risk_manager', label: 'Risk Manager' },
              { value: 'portfolio_manager', label: 'Portfolio Manager' },
              { value: 'sentiment_analyst', label: 'Sentiment Analyst' }
            ]}
          />

          <TextArea label="Description (Optional)" value={description} onChange={setDescription} rows={2} />
        </div>
      </ConfigSection>

      {/* LLM Configuration */}
      <ConfigSection
        title="LLM CONFIGURATION"
        icon={<Brain size={14} />}
        expanded={expandedSections.has('llm')}
        onToggle={() => toggleSection('llm')}
      >
        <div style={{ display: 'grid', gap: '12px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <SelectField
              label="Provider"
              value={provider}
              onChange={setProvider}
              options={[
                { value: 'openai', label: 'OpenAI' },
                { value: 'anthropic', label: 'Anthropic' },
                { value: 'google', label: 'Google' },
                { value: 'groq', label: 'Groq (Fast & Free)' },
                { value: 'deepseek', label: 'DeepSeek' },
                { value: 'xai', label: 'xAI (Grok)' },
                { value: 'ollama', label: 'Ollama (Local)' }
              ]}
            />

            <InputField label="Model" value={model} onChange={setModel} placeholder="gpt-4o-mini" />
          </div>

          <SliderField
            label="Temperature"
            value={temperature}
            onChange={setTemperature}
            min={0}
            max={1}
            step={0.1}
            showValue
          />
        </div>
      </ConfigSection>

      {/* Trading Configuration */}
      <ConfigSection
        title="TRADING CONFIGURATION"
        icon={<TrendingUp size={14} />}
        expanded={expandedSections.has('trading')}
        onToggle={() => toggleSection('trading')}
      >
        <div style={{ display: 'grid', gap: '12px' }}>
          <InputField
            label="Trading Symbols (comma-separated)"
            value={symbols.join(', ')}
            onChange={(val: string) => setSymbols(val.split(',').map((s: string) => s.trim()))}
            placeholder="BTC/USD, ETH/USD"
          />

          <CheckboxField
            label="Enable Auto-Trading (Alpha Arena Mode)"
            checked={autoTrading}
            onChange={setAutoTrading}
            description="Agent will execute trades automatically every few minutes"
          />

          {autoTrading && (
            <SliderField
              label="Execution Interval (seconds)"
              value={executionInterval}
              onChange={setExecutionInterval}
              min={60}
              max={600}
              step={30}
              showValue
              description="How often the agent makes trading decisions (Alpha Arena uses 180s)"
            />
          )}
        </div>
      </ConfigSection>

      {/* Safety Limits */}
      <ConfigSection
        title="SAFETY LIMITS"
        icon={<Shield size={14} />}
        expanded={expandedSections.has('safety')}
        onToggle={() => toggleSection('safety')}
      >
        <div style={{ display: 'grid', gap: '12px' }}>
          <SliderField
            label="Max Position Size (% of portfolio)"
            value={maxPositionSize * 100}
            onChange={(val: number) => setMaxPositionSize(val / 100)}
            min={1}
            max={50}
            step={1}
            showValue
            suffix="%"
          />

          <SliderField
            label="Max Leverage"
            value={maxLeverage}
            onChange={setMaxLeverage}
            min={1}
            max={10}
            step={0.5}
            showValue
            suffix="x"
          />

          <SliderField
            label="Max Drawdown (circuit breaker)"
            value={maxDrawdown * 100}
            onChange={(val: number) => setMaxDrawdown(val / 100)}
            min={5}
            max={50}
            step={5}
            showValue
            suffix="%"
            description="Auto-trading stops if drawdown exceeds this"
          />

          <SliderField
            label="Max Daily Loss"
            value={maxDailyLoss * 100}
            onChange={(val: number) => setMaxDailyLoss(val / 100)}
            min={1}
            max={20}
            step={1}
            showValue
            suffix="%"
          />

          <SliderField
            label="Min Confidence to Execute"
            value={minConfidence * 100}
            onChange={(val: number) => setMinConfidence(val / 100)}
            min={30}
            max={90}
            step={5}
            showValue
            suffix="%"
            description="Only execute trades above this confidence"
          />

          <SliderField
            label="Max Open Positions"
            value={maxOpenPositions}
            onChange={setMaxOpenPositions}
            min={1}
            max={20}
            step={1}
            showValue
          />
        </div>
      </ConfigSection>

      {/* TP/SL Configuration */}
      <ConfigSection
        title="TAKE PROFIT / STOP LOSS"
        icon={<Target size={14} />}
        expanded={expandedSections.has('tpsl')}
        onToggle={() => toggleSection('tpsl')}
      >
        <div style={{ display: 'grid', gap: '12px' }}>
          <CheckboxField
            label="Dynamic TP/SL (based on volatility)"
            checked={useDynamicTPSL}
            onChange={setUseDynamicTPSL}
            description="Alpha Arena style - adjusts TP/SL based on current market volatility"
          />

          {useDynamicTPSL ? (
            <SliderField
              label="Risk/Reward Ratio"
              value={riskRewardRatio}
              onChange={setRiskRewardRatio}
              min={1}
              max={5}
              step={0.5}
              showValue
              suffix=":1"
            />
          ) : (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <SliderField
                label="Stop Loss %"
                value={stopLossPercent}
                onChange={setStopLossPercent}
                min={0.5}
                max={10}
                step={0.5}
                showValue
                suffix="%"
              />

              <SliderField
                label="Take Profit %"
                value={takeProfitPercent}
                onChange={setTakeProfitPercent}
                min={1}
                max={20}
                step={0.5}
                showValue
                suffix="%"
              />
            </div>
          )}

          <CheckboxField
            label="Trailing Stop Loss"
            checked={useTrailingStop}
            onChange={setUseTrailingStop}
            description="Lock in profits as price moves favorably"
          />

          {useTrailingStop && (
            <SliderField
              label="Trailing Distance %"
              value={trailingPercent}
              onChange={setTrailingPercent}
              min={0.5}
              max={5}
              step={0.5}
              showValue
              suffix="%"
            />
          )}
        </div>
      </ConfigSection>

      {/* Advanced Configuration */}
      <ConfigSection
        title="ADVANCED OPTIONS"
        icon={<Settings size={14} />}
        expanded={expandedSections.has('advanced')}
        onToggle={() => toggleSection('advanced')}
      >
        <div style={{ display: 'grid', gap: '12px' }}>
          <CheckboxField
            label="Enable Memory"
            checked={enableMemory}
            onChange={setEnableMemory}
            description="Agent remembers past decisions and learns from them"
          />

          <CheckboxField
            label="Enable Knowledge Base"
            checked={enableKnowledge}
            onChange={setEnableKnowledge}
            description="Access to historical market data and patterns"
          />

          <TextArea
            label="Custom Instructions (one per line)"
            value={customInstructions}
            onChange={setCustomInstructions}
            rows={4}
            placeholder="Always use risk management&#10;Never trade during low volume&#10;Focus on high confidence setups"
          />
        </div>
      </ConfigSection>

      {/* Error Display */}
      {error && (
        <div style={{
          background: `${BLOOMBERG.RED}15`,
          border: `1px solid ${BLOOMBERG.RED}`,
          borderRadius: '4px',
          padding: '12px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <AlertTriangle size={16} color={BLOOMBERG.RED} />
          <span style={{ color: BLOOMBERG.RED, fontSize: '11px' }}>{error}</span>
        </div>
      )}

      {/* Create Button */}
      <button
        onClick={handleCreateAgent}
        disabled={isCreating || !agentName}
        style={{
          background: `linear-gradient(135deg, ${BLOOMBERG.GREEN} 0%, #00B359 100%)`,
          border: 'none',
          color: BLOOMBERG.DARK_BG,
          padding: '14px',
          borderRadius: '4px',
          fontSize: '12px',
          fontWeight: '700',
          cursor: isCreating || !agentName ? 'not-allowed' : 'pointer',
          opacity: isCreating || !agentName ? 0.5 : 1,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '8px',
          marginTop: '8px'
        }}
      >
        <Save size={16} />
        {isCreating ? 'CREATING AGENT...' : 'CREATE AGENT'}
      </button>
      </div>
    </div>
  );
}

// Helper Components

function ConfigSection({ title, icon, expanded, onToggle, children }: any) {
  return (
    <div style={{
      background: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '4px',
      overflow: 'hidden'
    }}>
      <div
        onClick={onToggle}
        style={{
          background: BLOOMBERG.HEADER_BG,
          padding: '10px 12px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          cursor: 'pointer',
          borderBottom: expanded ? `1px solid ${BLOOMBERG.BORDER}` : 'none'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: BLOOMBERG.ORANGE }}>{icon}</span>
          <span style={{ color: BLOOMBERG.WHITE, fontSize: '11px', fontWeight: '700' }}>{title}</span>
        </div>
        {expanded ? <ChevronUp size={14} color={BLOOMBERG.GRAY} /> : <ChevronDown size={14} color={BLOOMBERG.GRAY} />}
      </div>
      {expanded && (
        <div style={{ padding: '12px' }}>
          {children}
        </div>
      )}
    </div>
  );
}

function InputField({ label, value, onChange, placeholder = '', type = 'text' }: any) {
  return (
    <div>
      <label style={{ color: BLOOMBERG.GRAY, fontSize: '10px', fontWeight: '600', display: 'block', marginBottom: '4px' }}>
        {label}
      </label>
      <input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          width: '100%',
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          color: BLOOMBERG.WHITE,
          padding: '8px',
          borderRadius: '3px',
          fontSize: '11px'
        }}
      />
    </div>
  );
}

function SelectField({ label, value, onChange, options }: any) {
  return (
    <div>
      <label style={{ color: BLOOMBERG.GRAY, fontSize: '10px', fontWeight: '600', display: 'block', marginBottom: '4px' }}>
        {label}
      </label>
      <select
        value={value}
        onChange={(e) => onChange(e.target.value)}
        style={{
          width: '100%',
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          color: BLOOMBERG.WHITE,
          padding: '8px',
          borderRadius: '3px',
          fontSize: '11px',
          cursor: 'pointer'
        }}
      >
        {options.map((opt: any) => (
          <option key={opt.value} value={opt.value}>{opt.label}</option>
        ))}
      </select>
    </div>
  );
}

function TextArea({ label, value, onChange, rows = 3, placeholder = '' }: any) {
  return (
    <div>
      <label style={{ color: BLOOMBERG.GRAY, fontSize: '10px', fontWeight: '600', display: 'block', marginBottom: '4px' }}>
        {label}
      </label>
      <textarea
        value={value}
        onChange={(e) => onChange(e.target.value)}
        rows={rows}
        placeholder={placeholder}
        style={{
          width: '100%',
          background: BLOOMBERG.DARK_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          color: BLOOMBERG.WHITE,
          padding: '8px',
          borderRadius: '3px',
          fontSize: '11px',
          resize: 'vertical',
          fontFamily: 'inherit'
        }}
      />
    </div>
  );
}

function CheckboxField({ label, checked, onChange, description = '' }: any) {
  return (
    <div>
      <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => onChange(e.target.checked)}
          style={{ cursor: 'pointer' }}
        />
        <span style={{ color: BLOOMBERG.WHITE, fontSize: '11px', fontWeight: '600' }}>{label}</span>
      </label>
      {description && (
        <p style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginTop: '4px', marginLeft: '24px' }}>
          {description}
        </p>
      )}
    </div>
  );
}

function SliderField({ label, value, onChange, min, max, step, showValue, suffix = '', description = '' }: any) {
  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
        <label style={{ color: BLOOMBERG.GRAY, fontSize: '10px', fontWeight: '600' }}>
          {label}
        </label>
        {showValue && (
          <span style={{ color: BLOOMBERG.CYAN, fontSize: '10px', fontWeight: '700' }}>
            {value}{suffix}
          </span>
        )}
      </div>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => onChange(parseFloat(e.target.value))}
        style={{
          width: '100%',
          cursor: 'pointer',
          accentColor: BLOOMBERG.ORANGE
        }}
      />
      {description && (
        <p style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginTop: '4px' }}>
          {description}
        </p>
      )}
    </div>
  );
}
