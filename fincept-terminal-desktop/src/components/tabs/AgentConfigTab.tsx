// File: src/components/tabs/AgentConfigTab.tsx
// CoreAgent Configuration - Uses finagent_core system with SQLite persistence

import React, { useState, useEffect, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';
import ReactMarkdown from 'react-markdown';
import {
  Bot, Brain, RefreshCw, Save, Play, Trash2, Plus, Settings,
  ChevronDown, ChevronRight, AlertCircle, CheckCircle, Zap,
  Database, Code, MessageSquare, Wrench, Copy, Check, X,
  Cpu, BookOpen, Sparkles,
} from 'lucide-react';
import {
  saveAgentConfig, getAgentConfigs, deleteAgentConfig,
  setActiveAgentConfig, getActiveAgentConfig,
  getLLMConfigs,
  type AgentConfig, type AgentConfigData, type LLMConfig,
} from '@/services/sqliteService';
import { TabFooter } from '@/components/common/TabFooter';

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
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

// 40+ Model providers from finagent_core
const MODEL_PROVIDERS = [
  { id: 'openai', name: 'OpenAI', models: ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'gpt-4', 'o1', 'o1-mini', 'o1-preview'] },
  { id: 'anthropic', name: 'Anthropic', models: ['claude-sonnet-4-5-20250514', 'claude-3-5-sonnet-20241022', 'claude-3-opus-20240229', 'claude-3-haiku-20240307'] },
  { id: 'google', name: 'Google', models: ['gemini-2.0-flash-exp', 'gemini-1.5-pro', 'gemini-1.5-flash'] },
  { id: 'groq', name: 'Groq', models: ['llama-3.3-70b-versatile', 'llama-3.1-70b-versatile', 'mixtral-8x7b-32768'] },
  { id: 'ollama', name: 'Ollama (Local)', models: ['llama3.3', 'llama3.2', 'mistral', 'mixtral', 'qwen2.5', 'phi3', 'gemma2'] },
  { id: 'deepseek', name: 'DeepSeek', models: ['deepseek-chat', 'deepseek-coder', 'deepseek-reasoner'] },
  { id: 'mistral', name: 'Mistral', models: ['mistral-large-latest', 'mistral-medium-latest', 'codestral-latest'] },
  { id: 'cohere', name: 'Cohere', models: ['command-r-plus', 'command-r', 'command-light'] },
  { id: 'xai', name: 'xAI (Grok)', models: ['grok-2', 'grok-2-mini', 'grok-beta'] },
  { id: 'perplexity', name: 'Perplexity', models: ['llama-3.1-sonar-large-128k-online', 'llama-3.1-sonar-small-128k-online'] },
  { id: 'together', name: 'Together AI', models: ['meta-llama/Llama-3.3-70B-Instruct-Turbo', 'mistralai/Mixtral-8x22B-Instruct-v0.1'] },
  { id: 'fireworks', name: 'Fireworks', models: ['accounts/fireworks/models/llama-v3p1-405b-instruct'] },
  { id: 'aws', name: 'AWS Bedrock', models: ['anthropic.claude-3-5-sonnet-20241022-v2:0', 'amazon.titan-text-express-v1'] },
  { id: 'azure', name: 'Azure OpenAI', models: ['gpt-4o', 'gpt-4-turbo', 'gpt-35-turbo'] },
  { id: 'vertexai', name: 'Vertex AI', models: ['gemini-1.5-pro', 'gemini-1.5-flash'] },
];

// 100+ Tools from finagent_core organized by category
const TOOL_CATEGORIES: Record<string, { tools: string[]; color: string }> = {
  'Finance': { tools: ['yfinance', 'polygon', 'alpha_vantage', 'finnhub', 'financial_datasets', 'sec_tools'], color: BLOOMBERG.GREEN },
  'Search': { tools: ['duckduckgo', 'googlesearch', 'tavily', 'exa', 'searxng', 'serpapi'], color: BLOOMBERG.CYAN },
  'Web': { tools: ['requests', 'crawl4ai', 'firecrawl', 'newspaper', 'wikipedia', 'arxiv'], color: BLOOMBERG.BLUE },
  'Data': { tools: ['pandas', 'csv', 'json_tools', 'sql', 'file', 'excel'], color: BLOOMBERG.PURPLE },
  'Code': { tools: ['python', 'shell', 'calculator'], color: BLOOMBERG.YELLOW },
  'AI/ML': { tools: ['openai', 'anthropic', 'ollama', 'huggingface'], color: BLOOMBERG.ORANGE },
  'Communication': { tools: ['email', 'slack', 'discord', 'telegram'], color: BLOOMBERG.RED },
  'Storage': { tools: ['s3', 'gcs', 'github', 'gitlab'], color: BLOOMBERG.GRAY },
};

// Reasoning strategies
const REASONING_STRATEGIES = [
  { id: 'chain_of_thought', name: 'Chain of Thought', desc: 'Step-by-step reasoning' },
  { id: 'tree_of_thought', name: 'Tree of Thought', desc: 'Explore multiple paths' },
  { id: 'self_reflection', name: 'Self Reflection', desc: 'Iterative improvement' },
  { id: 'react', name: 'ReAct', desc: 'Reasoning + Acting' },
];

const CONFIG_CATEGORIES = [
  { id: 'general', name: 'General', icon: '🤖' },
  { id: 'finance', name: 'Finance', icon: '💰' },
  { id: 'research', name: 'Research', icon: '🔬' },
  { id: 'coding', name: 'Coding', icon: '💻' },
  { id: 'trading', name: 'Trading', icon: '📈' },
  { id: 'custom', name: 'Custom', icon: '⚙️' },
];

const AgentConfigTab: React.FC = () => {
  const { t } = useTranslation('agentConfig');

  // Saved configs
  const [savedConfigs, setSavedConfigs] = useState<AgentConfig[]>([]);
  const [activeConfigId, setActiveConfigId] = useState<string | null>(null);
  const [selectedConfigId, setSelectedConfigId] = useState<string | null>(null);
  const [filterCategory, setFilterCategory] = useState<string>('all');

  // Current config being edited
  const [configName, setConfigName] = useState('New Agent');
  const [configDescription, setConfigDescription] = useState('');
  const [configCategory, setConfigCategory] = useState('general');

  // Model
  const [modelProvider, setModelProvider] = useState('openai');
  const [modelId, setModelId] = useState('gpt-4o-mini');
  const [temperature, setTemperature] = useState(0.7);
  const [maxTokens, setMaxTokens] = useState(4096);

  // Agent
  const [instructions, setInstructions] = useState('You are a helpful AI assistant specialized in financial analysis.');
  const [selectedTools, setSelectedTools] = useState<string[]>(['yfinance', 'calculator']);

  // Memory
  const [memoryEnabled, setMemoryEnabled] = useState(false);

  // Reasoning
  const [reasoningEnabled, setReasoningEnabled] = useState(false);
  const [reasoningStrategy, setReasoningStrategy] = useState('chain_of_thought');
  const [reasoningMaxSteps, setReasoningMaxSteps] = useState(10);

  // Output
  const [markdownOutput, setMarkdownOutput] = useState(true);
  const [debugMode, setDebugMode] = useState(false);

  // UI
  const [loading, setLoading] = useState(false);
  const [testing, setTesting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const [expandedSections, setExpandedSections] = useState<Set<string>>(new Set(['model', 'instructions', 'tools']));
  const [testQuery, setTestQuery] = useState('');
  const [testResponse, setTestResponse] = useState<string | null>(null);
  const [showJson, setShowJson] = useState(false);

  useEffect(() => {
    loadConfigs();
  }, []);

  const loadConfigs = async () => {
    try {
      const configs = await getAgentConfigs();
      setSavedConfigs(configs);
      const active = await getActiveAgentConfig();
      if (active) setActiveConfigId(active.id);
    } catch (err) {
      console.error('Failed to load configs:', err);
    }
  };

  const toggleSection = (id: string) => {
    const s = new Set(expandedSections);
    s.has(id) ? s.delete(id) : s.add(id);
    setExpandedSections(s);
  };

  const toggleTool = (tool: string) => {
    setSelectedTools(prev =>
      prev.includes(tool) ? prev.filter(t => t !== tool) : [...prev, tool]
    );
  };

  const buildConfig = useCallback((): AgentConfigData => {
    const cfg: AgentConfigData = {
      model: { provider: modelProvider, model_id: modelId, temperature, max_tokens: maxTokens },
      instructions,
    };
    if (selectedTools.length > 0) cfg.tools = selectedTools;
    if (memoryEnabled) cfg.memory = { enabled: true };
    if (reasoningEnabled) cfg.reasoning = { strategy: reasoningStrategy, max_steps: reasoningMaxSteps };
    if (markdownOutput) cfg.output_format = 'markdown';
    if (debugMode) cfg.debug = true;
    return cfg;
  }, [modelProvider, modelId, temperature, maxTokens, instructions, selectedTools, memoryEnabled, reasoningEnabled, reasoningStrategy, reasoningMaxSteps, markdownOutput, debugMode]);

  const handleSave = async () => {
    if (!configName.trim()) {
      setError('Please enter a config name');
      return;
    }
    setLoading(true);
    setError(null);
    try {
      const id = selectedConfigId || crypto.randomUUID();
      const result = await saveAgentConfig({
        id,
        name: configName,
        description: configDescription || undefined,
        config_json: JSON.stringify(buildConfig()),
        category: configCategory,
        is_default: false,
        is_active: activeConfigId === id,
      });
      if (result.success) {
        setSuccess('Saved!');
        setSelectedConfigId(id);
        await loadConfigs();
        setTimeout(() => setSuccess(null), 2000);
      } else {
        setError(result.message);
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const handleLoad = (cfg: AgentConfig) => {
    try {
      const data: AgentConfigData = JSON.parse(cfg.config_json);
      setConfigName(cfg.name);
      setConfigDescription(cfg.description || '');
      setConfigCategory(cfg.category);
      setSelectedConfigId(cfg.id);
      if (data.model) {
        setModelProvider(data.model.provider || 'openai');
        setModelId(data.model.model_id || 'gpt-4o-mini');
        setTemperature(data.model.temperature ?? 0.7);
        setMaxTokens(data.model.max_tokens ?? 4096);
      }
      setInstructions(data.instructions || '');
      setSelectedTools(data.tools || []);
      setMemoryEnabled(data.memory?.enabled ?? false);
      setReasoningEnabled(!!data.reasoning);
      if (data.reasoning) {
        setReasoningStrategy(data.reasoning.strategy || 'chain_of_thought');
        setReasoningMaxSteps(data.reasoning.max_steps ?? 10);
      }
      setMarkdownOutput(data.output_format === 'markdown');
      setDebugMode(data.debug ?? false);
      setTestResponse(null);
    } catch (err) {
      setError('Failed to parse config');
    }
  };

  const handleDelete = async (id: string) => {
    if (!confirm('Delete this configuration?')) return;
    try {
      await deleteAgentConfig(id);
      if (selectedConfigId === id) handleNew();
      await loadConfigs();
      setSuccess('Deleted');
      setTimeout(() => setSuccess(null), 2000);
    } catch (err: any) {
      setError(err.toString());
    }
  };

  const handleSetActive = async (id: string) => {
    try {
      await setActiveAgentConfig(id);
      setActiveConfigId(id);
      setSuccess('Activated');
      setTimeout(() => setSuccess(null), 2000);
    } catch (err: any) {
      setError(err.toString());
    }
  };

  const handleNew = () => {
    setSelectedConfigId(null);
    setConfigName('New Agent');
    setConfigDescription('');
    setConfigCategory('general');
    setModelProvider('openai');
    setModelId('gpt-4o-mini');
    setTemperature(0.7);
    setMaxTokens(4096);
    setInstructions('You are a helpful AI assistant.');
    setSelectedTools([]);
    setMemoryEnabled(false);
    setReasoningEnabled(false);
    setDebugMode(false);
    setTestResponse(null);
  };

  // Helper to fetch API keys from Settings LLM configs
  const fetchApiKeysFromSettings = async (): Promise<Record<string, string>> => {
    try {
      const llmConfigs = await getLLMConfigs();
      const apiKeys: Record<string, string> = {};

      // Map provider configs to API keys
      for (const config of llmConfigs) {
        if (config.api_key) {
          // Store by provider name (lowercase)
          const providerKey = `${config.provider.toUpperCase()}_API_KEY`;
          apiKeys[providerKey] = config.api_key;
          // Also store by common name patterns
          apiKeys[config.provider] = config.api_key;
        }
      }

      return apiKeys;
    } catch (err) {
      console.error('Failed to fetch API keys from settings:', err);
      return {};
    }
  };

  const handleTest = async () => {
    if (!testQuery.trim()) {
      setError('Enter a test query');
      return;
    }
    setTesting(true);
    setError(null);
    setTestResponse(null);
    try {
      // Fetch API keys from Settings tab's LLM configurations
      const apiKeys = await fetchApiKeysFromSettings();

      // Check if we have API key for selected provider
      const providerKey = `${modelProvider.toUpperCase()}_API_KEY`;
      if (!apiKeys[providerKey] && !apiKeys[modelProvider] && modelProvider !== 'ollama') {
        setError(`No API key found for ${modelProvider}. Please configure it in Settings → LLM Config.`);
        setTesting(false);
        return;
      }

      const result = await invoke<string>('execute_core_agent', {
        queryJson: testQuery,
        configJson: JSON.stringify(buildConfig()),
        apiKeysJson: JSON.stringify(apiKeys),
      });
      const parsed = JSON.parse(result);
      if (parsed.success) {
        setTestResponse(parsed.response || parsed.content || 'No response');
      } else {
        setError(parsed.error || 'Test failed');
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setTesting(false);
    }
  };

  const filteredConfigs = filterCategory === 'all'
    ? savedConfigs
    : savedConfigs.filter(c => c.category === filterCategory);

  const availableModels = MODEL_PROVIDERS.find(p => p.id === modelProvider)?.models || [];

  const SectionHeader = ({ id, title, icon, badge }: { id: string; title: string; icon: React.ReactNode; badge?: string }) => (
    <button
      onClick={() => toggleSection(id)}
      className="w-full flex items-center justify-between p-2 rounded transition-colors"
      style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
    >
      <div className="flex items-center gap-2">
        <span style={{ color: BLOOMBERG.ORANGE }}>{icon}</span>
        <span style={{ color: BLOOMBERG.WHITE, fontSize: '11px', fontWeight: 600 }}>{title}</span>
        {badge && <span style={{ color: BLOOMBERG.CYAN, fontSize: '9px' }}>({badge})</span>}
      </div>
      {expandedSections.has(id) ? <ChevronDown size={14} color={BLOOMBERG.GRAY} /> : <ChevronRight size={14} color={BLOOMBERG.GRAY} />}
    </button>
  );

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG, fontFamily: 'Consolas, monospace' }}>
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 border-b" style={{ borderColor: BLOOMBERG.ORANGE, backgroundColor: BLOOMBERG.HEADER_BG }}>
        <div className="flex items-center gap-3">
          <Bot size={20} color={BLOOMBERG.ORANGE} />
          <span style={{ color: BLOOMBERG.ORANGE, fontWeight: 700, fontSize: '13px' }}>CORE AGENT BUILDER</span>
          <span style={{ color: BLOOMBERG.GRAY, fontSize: '9px' }}>finagent_core</span>
        </div>
        <div className="flex items-center gap-2">
          <button onClick={handleNew} className="flex items-center gap-1 px-2 py-1 text-xs" style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}`, color: BLOOMBERG.WHITE }}>
            <Plus size={12} /> New
          </button>
          <button onClick={handleSave} disabled={loading} className="flex items-center gap-1 px-2 py-1 text-xs" style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG, fontWeight: 600 }}>
            <Save size={12} /> {loading ? 'Saving...' : 'Save'}
          </button>
        </div>
      </div>

      {/* Alerts */}
      {error && (
        <div className="flex items-center gap-2 px-3 py-1 text-xs" style={{ backgroundColor: `${BLOOMBERG.RED}20`, color: BLOOMBERG.RED }}>
          <AlertCircle size={12} /> {error}
          <button onClick={() => setError(null)} className="ml-auto"><X size={12} /></button>
        </div>
      )}
      {success && (
        <div className="flex items-center gap-2 px-3 py-1 text-xs" style={{ backgroundColor: `${BLOOMBERG.GREEN}20`, color: BLOOMBERG.GREEN }}>
          <CheckCircle size={12} /> {success}
        </div>
      )}

      <div className="flex flex-1 overflow-hidden">
        {/* Left: Saved Configs */}
        <div className="w-56 border-r flex flex-col" style={{ borderColor: BLOOMBERG.BORDER, backgroundColor: BLOOMBERG.PANEL_BG }}>
          <div className="p-2 border-b" style={{ borderColor: BLOOMBERG.BORDER }}>
            <select
              value={filterCategory}
              onChange={e => setFilterCategory(e.target.value)}
              className="w-full px-2 py-1 text-xs"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <option value="all">All Categories</option>
              {CONFIG_CATEGORIES.map(c => <option key={c.id} value={c.id}>{c.icon} {c.name}</option>)}
            </select>
          </div>
          <div className="flex-1 overflow-auto p-2 space-y-1">
            {filteredConfigs.length === 0 ? (
              <div className="text-center py-8 text-xs" style={{ color: BLOOMBERG.GRAY }}>No saved configs</div>
            ) : (
              filteredConfigs.map(cfg => (
                <div
                  key={cfg.id}
                  onClick={() => handleLoad(cfg)}
                  className="p-2 rounded cursor-pointer group"
                  style={{
                    backgroundColor: selectedConfigId === cfg.id ? BLOOMBERG.HOVER : 'transparent',
                    border: `1px solid ${activeConfigId === cfg.id ? BLOOMBERG.GREEN : selectedConfigId === cfg.id ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                  }}
                >
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-medium truncate" style={{ color: BLOOMBERG.WHITE }}>{cfg.name}</span>
                    <div className="flex gap-1 opacity-0 group-hover:opacity-100">
                      {activeConfigId !== cfg.id && (
                        <button onClick={e => { e.stopPropagation(); handleSetActive(cfg.id); }} title="Set Active">
                          <Check size={12} color={BLOOMBERG.GREEN} />
                        </button>
                      )}
                      <button onClick={e => { e.stopPropagation(); handleDelete(cfg.id); }} title="Delete">
                        <Trash2 size={12} color={BLOOMBERG.RED} />
                      </button>
                    </div>
                  </div>
                  <div className="text-xs" style={{ color: BLOOMBERG.GRAY }}>{cfg.category}</div>
                  {activeConfigId === cfg.id && (
                    <div className="flex items-center gap-1 mt-1 text-xs" style={{ color: BLOOMBERG.GREEN }}>
                      <Zap size={10} /> Active
                    </div>
                  )}
                </div>
              ))
            )}
          </div>
        </div>

        {/* Center: Config Editor */}
        <div className="flex-1 overflow-auto p-3 space-y-2">
          {/* Basic Info */}
          <div className="grid grid-cols-3 gap-2">
            <div className="col-span-2">
              <label className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Name</label>
              <input
                value={configName}
                onChange={e => setConfigName(e.target.value)}
                className="w-full px-2 py-1 text-xs mt-1"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
              />
            </div>
            <div>
              <label className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Category</label>
              <select
                value={configCategory}
                onChange={e => setConfigCategory(e.target.value)}
                className="w-full px-2 py-1 text-xs mt-1"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                {CONFIG_CATEGORIES.map(c => <option key={c.id} value={c.id}>{c.icon} {c.name}</option>)}
              </select>
            </div>
          </div>

          {/* Model */}
          <div className="rounded" style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}>
            <SectionHeader id="model" title="MODEL" icon={<Cpu size={14} />} />
            {expandedSections.has('model') && (
              <div className="p-2 space-y-2" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                <div className="grid grid-cols-2 gap-2">
                  <div>
                    <label className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Provider</label>
                    <select
                      value={modelProvider}
                      onChange={e => { setModelProvider(e.target.value); setModelId(MODEL_PROVIDERS.find(p => p.id === e.target.value)?.models[0] || ''); }}
                      className="w-full px-2 py-1 text-xs mt-1"
                      style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.CYAN, border: `1px solid ${BLOOMBERG.BORDER}` }}
                    >
                      {MODEL_PROVIDERS.map(p => <option key={p.id} value={p.id}>{p.name}</option>)}
                    </select>
                  </div>
                  <div>
                    <label className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Model</label>
                    <select
                      value={modelId}
                      onChange={e => setModelId(e.target.value)}
                      className="w-full px-2 py-1 text-xs mt-1"
                      style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.PURPLE, border: `1px solid ${BLOOMBERG.BORDER}` }}
                    >
                      {availableModels.map(m => <option key={m} value={m}>{m}</option>)}
                    </select>
                  </div>
                </div>
                <div className="grid grid-cols-2 gap-2">
                  <div>
                    <label className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Temperature: {temperature}</label>
                    <input type="range" min="0" max="2" step="0.1" value={temperature} onChange={e => setTemperature(parseFloat(e.target.value))} className="w-full mt-1" />
                  </div>
                  <div>
                    <label className="text-xs" style={{ color: BLOOMBERG.GRAY }}>Max Tokens</label>
                    <input
                      type="number"
                      value={maxTokens}
                      onChange={e => setMaxTokens(parseInt(e.target.value) || 4096)}
                      className="w-full px-2 py-1 text-xs mt-1"
                      style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
                    />
                  </div>
                </div>
              </div>
            )}
          </div>

          {/* Instructions */}
          <div className="rounded" style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}>
            <SectionHeader id="instructions" title="INSTRUCTIONS" icon={<MessageSquare size={14} />} />
            {expandedSections.has('instructions') && (
              <div className="p-2" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                <textarea
                  value={instructions}
                  onChange={e => setInstructions(e.target.value)}
                  rows={5}
                  placeholder="System instructions..."
                  className="w-full px-2 py-1 text-xs font-mono"
                  style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}`, resize: 'vertical' }}
                />
              </div>
            )}
          </div>

          {/* Tools */}
          <div className="rounded" style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}>
            <SectionHeader id="tools" title="TOOLS" icon={<Wrench size={14} />} badge={`${selectedTools.length}`} />
            {expandedSections.has('tools') && (
              <div className="p-2 space-y-2" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                {Object.entries(TOOL_CATEGORIES).map(([cat, { tools, color }]) => (
                  <div key={cat}>
                    <div className="text-xs mb-1" style={{ color }}>{cat}</div>
                    <div className="flex flex-wrap gap-1">
                      {tools.map(tool => (
                        <button
                          key={tool}
                          onClick={() => toggleTool(tool)}
                          className="px-2 py-0.5 text-xs rounded"
                          style={{
                            backgroundColor: selectedTools.includes(tool) ? color : BLOOMBERG.HEADER_BG,
                            color: selectedTools.includes(tool) ? BLOOMBERG.DARK_BG : BLOOMBERG.WHITE,
                            border: `1px solid ${BLOOMBERG.BORDER}`,
                          }}
                        >
                          {tool}
                        </button>
                      ))}
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Memory & Reasoning */}
          <div className="rounded" style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}>
            <SectionHeader id="advanced" title="ADVANCED" icon={<Brain size={14} />} />
            {expandedSections.has('advanced') && (
              <div className="p-2 space-y-2" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                <label className="flex items-center gap-2 cursor-pointer">
                  <input type="checkbox" checked={memoryEnabled} onChange={e => setMemoryEnabled(e.target.checked)} />
                  <span className="text-xs" style={{ color: BLOOMBERG.WHITE }}>Enable Memory</span>
                </label>
                <label className="flex items-center gap-2 cursor-pointer">
                  <input type="checkbox" checked={reasoningEnabled} onChange={e => setReasoningEnabled(e.target.checked)} />
                  <span className="text-xs" style={{ color: BLOOMBERG.WHITE }}>Enable Reasoning</span>
                </label>
                {reasoningEnabled && (
                  <div className="grid grid-cols-2 gap-2 ml-4">
                    <select
                      value={reasoningStrategy}
                      onChange={e => setReasoningStrategy(e.target.value)}
                      className="px-2 py-1 text-xs"
                      style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.CYAN, border: `1px solid ${BLOOMBERG.BORDER}` }}
                    >
                      {REASONING_STRATEGIES.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
                    </select>
                    <input
                      type="number"
                      value={reasoningMaxSteps}
                      onChange={e => setReasoningMaxSteps(parseInt(e.target.value) || 10)}
                      placeholder="Max steps"
                      className="px-2 py-1 text-xs"
                      style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}
                    />
                  </div>
                )}
                <label className="flex items-center gap-2 cursor-pointer">
                  <input type="checkbox" checked={markdownOutput} onChange={e => setMarkdownOutput(e.target.checked)} />
                  <span className="text-xs" style={{ color: BLOOMBERG.WHITE }}>Markdown Output</span>
                </label>
                <label className="flex items-center gap-2 cursor-pointer">
                  <input type="checkbox" checked={debugMode} onChange={e => setDebugMode(e.target.checked)} />
                  <span className="text-xs" style={{ color: BLOOMBERG.WHITE }}>Debug Mode</span>
                </label>
              </div>
            )}
          </div>

          {/* JSON Preview */}
          <div className="rounded" style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}>
            <SectionHeader id="json" title="JSON CONFIG" icon={<Code size={14} />} />
            {expandedSections.has('json') && (
              <div className="p-2" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                <pre className="text-xs p-2 rounded overflow-auto max-h-40" style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.GREEN }}>
                  {JSON.stringify(buildConfig(), null, 2)}
                </pre>
              </div>
            )}
          </div>
        </div>

        {/* Right: Test Panel */}
        <div className="w-72 border-l flex flex-col" style={{ borderColor: BLOOMBERG.BORDER, backgroundColor: BLOOMBERG.PANEL_BG }}>
          <div className="p-2 border-b flex items-center gap-2" style={{ borderColor: BLOOMBERG.BORDER }}>
            <Play size={14} color={BLOOMBERG.GREEN} />
            <span className="text-xs font-medium" style={{ color: BLOOMBERG.WHITE }}>Test Agent</span>
          </div>
          <div className="p-2 space-y-2 flex-1 overflow-auto">
            <textarea
              value={testQuery}
              onChange={e => setTestQuery(e.target.value)}
              rows={3}
              placeholder="Enter test query..."
              className="w-full px-2 py-1 text-xs"
              style={{ backgroundColor: BLOOMBERG.HEADER_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}`, resize: 'vertical' }}
            />
            <button
              onClick={handleTest}
              disabled={testing || !testQuery.trim()}
              className="w-full flex items-center justify-center gap-2 px-2 py-2 text-xs font-medium rounded"
              style={{ backgroundColor: testing ? BLOOMBERG.MUTED : BLOOMBERG.GREEN, color: BLOOMBERG.DARK_BG, opacity: testing || !testQuery.trim() ? 0.6 : 1 }}
            >
              {testing ? <><RefreshCw size={12} className="animate-spin" /> Running...</> : <><Play size={12} /> Run Test</>}
            </button>
            {testResponse && (
              <div className="mt-2">
                <div className="text-xs mb-1" style={{ color: BLOOMBERG.GRAY }}>Response:</div>
                <div className="p-2 rounded text-xs overflow-auto max-h-80" style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE, border: `1px solid ${BLOOMBERG.BORDER}` }}>
                  <ReactMarkdown
                    components={{
                      p: ({ children }) => <p className="mb-2">{children}</p>,
                      strong: ({ children }) => <strong style={{ color: BLOOMBERG.CYAN }}>{children}</strong>,
                      code: ({ children }) => <code style={{ backgroundColor: BLOOMBERG.HEADER_BG, padding: '1px 4px', color: BLOOMBERG.GREEN }}>{children}</code>,
                    }}
                  >
                    {testResponse}
                  </ReactMarkdown>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      <TabFooter tabName="Core Agent Builder" />
    </div>
  );
};

export default AgentConfigTab;
