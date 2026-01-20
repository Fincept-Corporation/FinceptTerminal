/**
 * AgentConfigTab - Professional Agent Studio Interface (v2.1)
 *
 * Features:
 * - Dynamic agent discovery from backend
 * - Full agent configuration editor (LLM, tools, memory, reasoning)
 * - SuperAgent query routing with auto-route toggle
 * - Execution plan builder and visualizer
 * - Multi-agent team builder
 * - Tools browser with selection
 * - Chat interface with routing
 * - System capabilities view
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Bot, RefreshCw, Play, Code, AlertCircle, CheckCircle,
  Users, TrendingUp, Globe, Building2, Wrench, Brain,
  GitBranch, Settings, Zap, MessageSquare, Trash2, Plus,
  ChevronRight, ChevronDown, Search, Copy, Check, Cpu,
  Sparkles, Route, ListTree, Save, Edit3, ToggleLeft, ToggleRight,
  Loader2, Target, FileJson, Sliders, Database, Eye,
} from 'lucide-react';
import {
  getLLMConfigs,
  getActiveLLMConfig,
  type LLMConfig as DbLLMConfig,
  LLM_PROVIDERS as DB_LLM_PROVIDERS,
  sqliteService,
} from '@/services/core/sqliteService';
import { TabFooter } from '@/components/common/TabFooter';
import agentService, {
  type AgentConfig as ServiceAgentConfig,
  type SystemInfo,
  type ToolsInfo,
  type AgentCard,
  type ExecutionPlan,
  type PlanStep,
  type RoutingResult,
} from '@/services/agentService';

// =============================================================================
// Constants & Types
// =============================================================================

const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  PURPLE: '#8B5CF6',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

interface AgentDefinition {
  id: string;
  name: string;
  role: string;
  goal: string;
  description: string;
  llm_config: {
    provider: string;
    model_id: string;
    temperature: number;
    max_tokens: number;
  };
  tools: string[];
  enable_memory: boolean;
  enable_agentic_memory: boolean;
  instructions: string;
}

interface ChatMessage {
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: Date;
  agentName?: string;
  routingInfo?: RoutingResult;
}

// LLMConfig is imported from sqliteService as DbLLMConfig

type ViewMode = 'agents' | 'teams' | 'workflows' | 'planner' | 'tools' | 'system' | 'chat';

const VIEW_MODES = [
  { id: 'agents' as ViewMode, name: 'AGENTS', icon: Bot },
  { id: 'teams' as ViewMode, name: 'TEAMS', icon: Users },
  { id: 'workflows' as ViewMode, name: 'WORKFLOWS', icon: GitBranch },
  { id: 'planner' as ViewMode, name: 'PLANNER', icon: ListTree },
  { id: 'tools' as ViewMode, name: 'TOOLS', icon: Wrench },
  { id: 'chat' as ViewMode, name: 'CHAT', icon: MessageSquare },
  { id: 'system' as ViewMode, name: 'SYSTEM', icon: Cpu },
];

const OUTPUT_MODELS = [
  { id: 'trade_signal', name: 'Trade Signal', desc: 'Buy/Sell signals with confidence' },
  { id: 'stock_analysis', name: 'Stock Analysis', desc: 'Comprehensive stock report' },
  { id: 'portfolio_analysis', name: 'Portfolio Analysis', desc: 'Portfolio metrics & allocation' },
  { id: 'risk_assessment', name: 'Risk Assessment', desc: 'Risk metrics & warnings' },
  { id: 'market_analysis', name: 'Market Analysis', desc: 'Market conditions & trends' },
];

// Use LLM_PROVIDERS from sqliteService - includes fincept-llm and all configured providers

// =============================================================================
// Component
// =============================================================================

const AgentConfigTab: React.FC = () => {
  // View state
  const [viewMode, setViewMode] = useState<ViewMode>('agents');

  // Agent state - now using dynamic discovery
  const [discoveredAgents, setDiscoveredAgents] = useState<AgentCard[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<AgentCard | null>(null);
  const [agentsByCategory, setAgentsByCategory] = useState<Record<string, AgentCard[]>>({});
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  // Agent config editor state
  const [editMode, setEditMode] = useState(false);
  const [editedConfig, setEditedConfig] = useState<{
    provider: string;
    model_id: string;
    temperature: number;
    max_tokens: number;
    tools: string[];
    instructions: string;
    enable_memory: boolean;
    enable_reasoning: boolean;
  }>({
    provider: 'openai',
    model_id: 'gpt-4-turbo',
    temperature: 0.7,
    max_tokens: 4096,
    tools: [],
    instructions: '',
    enable_memory: false,
    enable_reasoning: false,
  });

  // System state (cached via service)
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [toolsInfo, setToolsInfo] = useState<ToolsInfo | null>(null);
  const [configuredLLMs, setConfiguredLLMs] = useState<DbLLMConfig[]>([]);
  const [llmProvidersList, setLlmProvidersList] = useState(DB_LLM_PROVIDERS);

  // Query & execution state
  const [testQuery, setTestQuery] = useState('Analyze AAPL stock and provide your investment thesis.');
  const [testResult, setTestResult] = useState<any>(null);
  const [selectedOutputModel, setSelectedOutputModel] = useState('');
  const [showJsonEditor, setShowJsonEditor] = useState(false);
  const [jsonText, setJsonText] = useState('');

  // SuperAgent routing state
  const [useAutoRouting, setUseAutoRouting] = useState(true);
  const [lastRoutingResult, setLastRoutingResult] = useState<RoutingResult | null>(null);

  // Team state
  const [teamMembers, setTeamMembers] = useState<AgentCard[]>([]);
  const [teamMode, setTeamMode] = useState<'coordinate' | 'route' | 'collaborate'>('coordinate');

  // Chat state
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>([]);
  const [chatInput, setChatInput] = useState('');
  const chatEndRef = useRef<HTMLDivElement>(null);

  // Tools state
  const [toolSearch, setToolSearch] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set(['finance']));
  const [selectedTools, setSelectedTools] = useState<string[]>([]);
  const [copiedTool, setCopiedTool] = useState<string | null>(null);

  // Workflow/Planner state
  const [workflowSymbol, setWorkflowSymbol] = useState('AAPL');
  const [currentPlan, setCurrentPlan] = useState<ExecutionPlan | null>(null);
  const [planExecuting, setPlanExecuting] = useState(false);

  // =============================================================================
  // Effects
  // =============================================================================

  useEffect(() => {
    loadDiscoveredAgents();
    loadSystemData();
    loadLLMConfigs();
  }, []);

  useEffect(() => {
    // Scroll chat to bottom
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [chatMessages]);

  const loadLLMConfigs = async () => {
    try {
      // Load configured LLMs from settings database
      const configs = await getLLMConfigs();
      setConfiguredLLMs(configs);

      // Set active config as default if available
      const activeConfig = await getActiveLLMConfig();
      if (activeConfig) {
        setEditedConfig(prev => ({
          ...prev,
          provider: activeConfig.provider,
          model_id: activeConfig.model,
        }));
      } else if (configs.length > 0) {
        // Use first configured LLM if no active one
        setEditedConfig(prev => ({
          ...prev,
          provider: configs[0].provider,
          model_id: configs[0].model,
        }));
      }

      console.log('[AgentConfigTab] Loaded LLM configs from settings:', configs.length);
    } catch (err) {
      console.error('Failed to load LLM configs:', err);
    }
  };

  const loadSystemData = async () => {
    const [sysInfo, tools] = await Promise.all([
      agentService.getSystemInfo(),
      agentService.getTools(),
    ]);
    if (sysInfo) setSystemInfo(sysInfo);
    if (tools) setToolsInfo(tools);
  };

  const loadDiscoveredAgents = async () => {
    setLoading(true);
    setError(null);

    try {
      // Use the new discover_agents endpoint
      const agents = await agentService.discoverAgents();
      setDiscoveredAgents(agents);

      // Group by category
      const byCategory: Record<string, AgentCard[]> = { all: agents };
      agents.forEach(agent => {
        const cat = agent.category || 'other';
        if (!byCategory[cat]) byCategory[cat] = [];
        byCategory[cat].push(agent);
      });
      setAgentsByCategory(byCategory);

      if (agents.length > 0) {
        setSelectedAgent(agents[0]);
        updateEditedConfigFromAgent(agents[0]);
      }
    } catch (err: any) {
      setError(err.toString());
      // Fallback to legacy loading
      await loadLegacyAgents();
    } finally {
      setLoading(false);
    }
  };

  const loadLegacyAgents = async () => {
    // Fallback to reading config files directly
    try {
      const result = await invoke<string>('list_available_agents');
      const agents = JSON.parse(result) as AgentCard[];
      setDiscoveredAgents(agents);

      const byCategory: Record<string, AgentCard[]> = { all: agents };
      agents.forEach(agent => {
        const cat = agent.category || 'other';
        if (!byCategory[cat]) byCategory[cat] = [];
        byCategory[cat].push(agent);
      });
      setAgentsByCategory(byCategory);
    } catch (err) {
      console.error('Legacy agent loading failed:', err);
    }
  };

  const updateEditedConfigFromAgent = (agent: AgentCard) => {
    const config = agent.config || {};
    const model = config.model || {};
    setEditedConfig({
      provider: model.provider || 'openai',
      model_id: model.model_id || 'gpt-4-turbo',
      temperature: model.temperature ?? 0.7,
      max_tokens: model.max_tokens ?? 4096,
      tools: config.tools || [],
      instructions: config.instructions || '',
      enable_memory: config.memory?.enabled || false,
      enable_reasoning: config.reasoning || false,
    });
    setJsonText(JSON.stringify(agent, null, 2));
  };

  // =============================================================================
  // API Key Helper
  // =============================================================================

  const getApiKeys = async (): Promise<Record<string, string>> => {
    try {
      const llmConfigs = await getLLMConfigs();
      const apiKeys: Record<string, string> = {};

      for (const config of llmConfigs) {
        if (config.api_key) {
          const providerUpper = config.provider.toUpperCase();
          apiKeys[`${providerUpper}_API_KEY`] = config.api_key;
          apiKeys[config.provider] = config.api_key;
        }
      }
      return apiKeys;
    } catch {
      return {};
    }
  };

  // =============================================================================
  // Agent Execution
  // =============================================================================

  const runAgent = async () => {
    if (!testQuery.trim()) return;

    setLoading(true);
    setError(null);
    setTestResult(null);

    try {
      const apiKeys = await getApiKeys();

      // Check if we should use auto-routing
      if (useAutoRouting) {
        // First, route the query
        const routingResult = await agentService.routeQuery(testQuery, apiKeys);
        setLastRoutingResult(routingResult);

        // Add system message about routing
        setChatMessages(prev => [
          ...prev,
          {
            role: 'system',
            content: `🎯 Routed to: ${routingResult.agent_id} (${routingResult.intent}) - Confidence: ${(routingResult.confidence * 100).toFixed(0)}%`,
            timestamp: new Date(),
            routingInfo: routingResult,
          },
        ]);

        // Execute with routing
        const result = await agentService.executeRoutedQuery(testQuery, apiKeys);
        setTestResult(result);

        setChatMessages(prev => [
          ...prev,
          { role: 'user', content: testQuery, timestamp: new Date() },
          {
            role: 'assistant',
            content: result.response || result.error || JSON.stringify(result, null, 2),
            timestamp: new Date(),
            agentName: routingResult.agent_id,
          },
        ]);
      } else {
        // Use selected agent directly
        const config = agentService.buildAgentConfig(
          editedConfig.provider,
          editedConfig.model_id,
          editedConfig.instructions,
          {
            tools: selectedTools.length > 0 ? selectedTools : editedConfig.tools,
            temperature: editedConfig.temperature,
            maxTokens: editedConfig.max_tokens,
            memory: editedConfig.enable_memory,
            reasoning: editedConfig.enable_reasoning,
          }
        );

        let result;
        if (selectedOutputModel) {
          result = await agentService.runAgentStructured(testQuery, config, selectedOutputModel, apiKeys);
        } else {
          result = await agentService.runAgent(testQuery, config, apiKeys);
        }

        setTestResult(result);

        setChatMessages(prev => [
          ...prev,
          { role: 'user', content: testQuery, timestamp: new Date() },
          {
            role: 'assistant',
            content: result.response || result.error || JSON.stringify(result, null, 2),
            timestamp: new Date(),
            agentName: selectedAgent?.name || 'Agent',
          },
        ]);
      }

      setSuccess('Query executed successfully');
      setTimeout(() => setSuccess(null), 3000);
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const runTeam = async () => {
    if (teamMembers.length === 0 || !testQuery.trim()) return;

    setLoading(true);
    setError(null);

    try {
      const apiKeys = await getApiKeys();

      const teamConfig = agentService.buildTeamConfig(
        'Custom Team',
        teamMode,
        teamMembers.map(agent => ({
          name: agent.name,
          role: agent.description,
          provider: agent.config?.model?.provider || 'openai',
          modelId: agent.config?.model?.model_id || 'gpt-4-turbo',
          tools: agent.config?.tools || [],
          instructions: agent.config?.instructions || '',
        }))
      );

      const result = await agentService.runTeam(testQuery, teamConfig, apiKeys);
      setTestResult(result);

      setChatMessages(prev => [
        ...prev,
        { role: 'user', content: testQuery, timestamp: new Date() },
        {
          role: 'assistant',
          content: result.response || result.error || JSON.stringify(result, null, 2),
          timestamp: new Date(),
          agentName: `Team (${teamMembers.length} agents)`,
        },
      ]);

      if (result.success) {
        setSuccess('Team executed successfully');
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const runWorkflow = async (workflowType: string) => {
    setLoading(true);
    setError(null);

    try {
      const apiKeys = await getApiKeys();
      let result;

      switch (workflowType) {
        case 'stock_analysis':
          result = await agentService.runStockAnalysis(workflowSymbol, apiKeys);
          break;
        case 'portfolio_rebal':
          result = await agentService.runPortfolioRebalancing({ holdings: [] }, apiKeys);
          break;
        case 'risk_assessment':
          result = await agentService.runRiskAssessment({ holdings: [] }, apiKeys);
          break;
        default:
          throw new Error(`Unknown workflow: ${workflowType}`);
      }

      setTestResult(result);
      if (result.success) {
        setSuccess(`${workflowType} completed`);
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  // =============================================================================
  // Execution Planner
  // =============================================================================

  const createPlan = async () => {
    if (!workflowSymbol.trim()) return;

    setLoading(true);
    setError(null);

    try {
      const plan = await agentService.createStockAnalysisPlan(workflowSymbol);
      if (plan) {
        setCurrentPlan(plan);
        setSuccess('Execution plan created');
        setTimeout(() => setSuccess(null), 3000);
      } else {
        setError('Failed to create plan');
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const executePlan = async () => {
    if (!currentPlan) return;

    setPlanExecuting(true);
    setError(null);

    try {
      const apiKeys = await getApiKeys();
      const result = await agentService.executePlan(currentPlan, apiKeys);

      if (result) {
        setCurrentPlan(result);
        if (result.is_complete) {
          setSuccess('Plan executed successfully');
        } else if (result.has_failed) {
          setError('Plan execution failed');
        }
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setPlanExecuting(false);
    }
  };

  // =============================================================================
  // Team Management
  // =============================================================================

  const addToTeam = (agent: AgentCard) => {
    if (!teamMembers.find(m => m.id === agent.id)) {
      setTeamMembers(prev => [...prev, agent]);
    }
  };

  const removeFromTeam = (agentId: string) => {
    setTeamMembers(prev => prev.filter(m => m.id !== agentId));
  };

  // =============================================================================
  // Tools Management
  // =============================================================================

  const toggleToolCategory = (category: string) => {
    setExpandedCategories(prev => {
      const newSet = new Set(prev);
      if (newSet.has(category)) newSet.delete(category);
      else newSet.add(category);
      return newSet;
    });
  };

  const copyTool = (tool: string) => {
    navigator.clipboard.writeText(tool);
    setCopiedTool(tool);
    setTimeout(() => setCopiedTool(null), 2000);
  };

  const toggleToolSelection = (tool: string) => {
    setSelectedTools(prev =>
      prev.includes(tool) ? prev.filter(t => t !== tool) : [...prev, tool]
    );
    // Also update edited config
    setEditedConfig(prev => ({
      ...prev,
      tools: prev.tools.includes(tool)
        ? prev.tools.filter(t => t !== tool)
        : [...prev.tools, tool],
    }));
  };

  // =============================================================================
  // Render: Agent Config Editor
  // =============================================================================

  const renderAgentConfigEditor = () => (
    <div className="space-y-4 p-4 rounded-lg" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
      <div className="flex items-center justify-between">
        <h3 className="text-sm font-semibold flex items-center gap-2" style={{ color: COLORS.WHITE }}>
          <Sliders size={16} style={{ color: COLORS.ORANGE }} />
          Agent Configuration
        </h3>
        <button
          onClick={() => setEditMode(!editMode)}
          className="px-2 py-1 rounded text-xs flex items-center gap-1"
          style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
        >
          <Edit3 size={12} />
          {editMode ? 'Done' : 'Edit'}
        </button>
      </div>

      {/* LLM Selection - Only from Settings */}
      {configuredLLMs.length > 0 ? (
        <div className="p-3 rounded-lg" style={{ backgroundColor: COLORS.DARK_BG, border: `1px solid ${COLORS.GREEN}40` }}>
          <div className="text-xs font-medium mb-2 flex items-center gap-2" style={{ color: COLORS.GREEN }}>
            <CheckCircle size={12} />
            SELECT LLM (from Settings)
          </div>
          <div className="flex flex-wrap gap-2">
            {configuredLLMs.map((llm, idx) => {
              const hasKey = !!llm.api_key && llm.api_key.length > 0;
              const isSelected = editedConfig.provider === llm.provider && editedConfig.model_id === llm.model;
              return (
                <button
                  key={idx}
                  onClick={() => editMode && setEditedConfig(prev => ({
                    ...prev,
                    provider: llm.provider,
                    model_id: llm.model,
                  }))}
                  className="px-3 py-2 rounded text-xs flex items-center gap-2 transition-colors"
                  style={{
                    backgroundColor: isSelected ? COLORS.ORANGE : COLORS.BORDER,
                    color: COLORS.WHITE,
                    cursor: editMode ? 'pointer' : 'default',
                    opacity: hasKey ? 1 : 0.5,
                  }}
                >
                  {llm.is_active && <span style={{ color: COLORS.GREEN }}>●</span>}
                  <span className="font-medium">{llm.provider}</span>
                  <span style={{ color: isSelected ? COLORS.WHITE : COLORS.GRAY }}>/ {llm.model}</span>
                  {!hasKey && <AlertCircle size={10} style={{ color: COLORS.RED }} />}
                </button>
              );
            })}
          </div>
          {configuredLLMs.some(l => !l.api_key || l.api_key.length === 0) && (
            <div className="text-xs mt-2" style={{ color: COLORS.RED }}>
              ⚠ Some providers missing API keys - configure in Settings → LLM Configuration
            </div>
          )}
        </div>
      ) : (
        <div className="p-3 rounded-lg" style={{ backgroundColor: COLORS.DARK_BG, border: `1px solid ${COLORS.RED}40` }}>
          <div className="text-xs font-medium mb-2 flex items-center gap-2" style={{ color: COLORS.RED }}>
            <AlertCircle size={12} />
            NO LLM CONFIGURED
          </div>
          <p className="text-xs" style={{ color: COLORS.GRAY }}>
            Go to Settings → LLM Configuration to add API keys for your preferred providers.
          </p>
        </div>
      )}

      <div className="grid grid-cols-2 gap-4">
        {/* Current Selection Display */}
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Selected Provider</label>
          <div
            className="w-full px-3 py-2 rounded text-sm"
            style={{
              backgroundColor: COLORS.DARK_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
          >
            {editedConfig.provider || 'None selected'}
          </div>
        </div>

        {/* Current Model Display */}
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Selected Model</label>
          <div
            className="w-full px-3 py-2 rounded text-sm"
            style={{
              backgroundColor: COLORS.DARK_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
          >
            {editedConfig.model_id || 'None selected'}
          </div>
        </div>

        {/* Temperature */}
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>
            Temperature: {editedConfig.temperature.toFixed(2)}
          </label>
          <input
            type="range"
            min="0"
            max="2"
            step="0.1"
            value={editedConfig.temperature}
            onChange={e => setEditedConfig(prev => ({ ...prev, temperature: parseFloat(e.target.value) }))}
            disabled={!editMode}
            className="w-full"
            style={{ accentColor: COLORS.ORANGE }}
          />
        </div>

        {/* Max Tokens */}
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Max Tokens</label>
          <input
            type="number"
            value={editedConfig.max_tokens}
            onChange={e => setEditedConfig(prev => ({ ...prev, max_tokens: parseInt(e.target.value) || 4096 }))}
            disabled={!editMode}
            className="w-full px-3 py-2 rounded text-sm"
            style={{
              backgroundColor: COLORS.DARK_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
              opacity: editMode ? 1 : 0.7,
            }}
          />
        </div>
      </div>

      {/* Toggle Options */}
      <div className="flex gap-4">
        <button
          onClick={() => editMode && setEditedConfig(prev => ({ ...prev, enable_memory: !prev.enable_memory }))}
          className="flex items-center gap-2 px-3 py-2 rounded text-sm"
          style={{
            backgroundColor: editedConfig.enable_memory ? COLORS.ORANGE + '20' : COLORS.BORDER,
            color: editedConfig.enable_memory ? COLORS.ORANGE : COLORS.GRAY,
            cursor: editMode ? 'pointer' : 'default',
          }}
        >
          {editedConfig.enable_memory ? <ToggleRight size={16} /> : <ToggleLeft size={16} />}
          Memory
        </button>

        <button
          onClick={() => editMode && setEditedConfig(prev => ({ ...prev, enable_reasoning: !prev.enable_reasoning }))}
          className="flex items-center gap-2 px-3 py-2 rounded text-sm"
          style={{
            backgroundColor: editedConfig.enable_reasoning ? COLORS.PURPLE + '20' : COLORS.BORDER,
            color: editedConfig.enable_reasoning ? COLORS.PURPLE : COLORS.GRAY,
            cursor: editMode ? 'pointer' : 'default',
          }}
        >
          {editedConfig.enable_reasoning ? <ToggleRight size={16} /> : <ToggleLeft size={16} />}
          Reasoning
        </button>
      </div>

      {/* Instructions */}
      {editMode && (
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>Instructions</label>
          <textarea
            value={editedConfig.instructions}
            onChange={e => setEditedConfig(prev => ({ ...prev, instructions: e.target.value }))}
            rows={4}
            className="w-full px-3 py-2 rounded text-sm font-mono"
            style={{
              backgroundColor: COLORS.DARK_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
            placeholder="Enter agent instructions..."
          />
        </div>
      )}

      {/* Selected Tools */}
      {editedConfig.tools.length > 0 && (
        <div>
          <label className="text-xs mb-1 block" style={{ color: COLORS.GRAY }}>
            Tools ({editedConfig.tools.length})
          </label>
          <div className="flex flex-wrap gap-1">
            {editedConfig.tools.map(tool => (
              <span
                key={tool}
                onClick={() => editMode && toggleToolSelection(tool)}
                className="px-2 py-0.5 rounded text-xs cursor-pointer"
                style={{ backgroundColor: COLORS.ORANGE + '30', color: COLORS.ORANGE }}
              >
                {tool} {editMode && '×'}
              </span>
            ))}
          </div>
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Agents View
  // =============================================================================

  const renderAgentsView = () => (
    <div className="flex h-full">
      {/* Sidebar */}
      <div className="w-72 border-r flex flex-col" style={{ borderColor: COLORS.BORDER }}>
        {/* Header with Refresh */}
        <div className="p-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
          <div className="text-xs font-medium" style={{ color: COLORS.GRAY }}>
            DISCOVERED AGENTS ({discoveredAgents.length})
          </div>
          <button
            onClick={loadDiscoveredAgents}
            disabled={loading}
            className="p-1.5 rounded hover:bg-[#1F1F1F]"
          >
            <RefreshCw size={14} className={loading ? 'animate-spin' : ''} style={{ color: COLORS.ORANGE }} />
          </button>
        </div>

        {/* Category Filter */}
        <div className="p-2 border-b" style={{ borderColor: COLORS.BORDER }}>
          <select
            value={selectedCategory}
            onChange={e => setSelectedCategory(e.target.value)}
            className="w-full px-2 py-1.5 rounded text-sm"
            style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
          >
            <option value="all">All Categories ({discoveredAgents.length})</option>
            {Object.entries(agentsByCategory)
              .filter(([k]) => k !== 'all')
              .map(([cat, agents]) => (
                <option key={cat} value={cat}>{cat} ({agents.length})</option>
              ))}
          </select>
        </div>

        {/* Agent List */}
        <div className="flex-1 overflow-y-auto p-2">
          {loading ? (
            <div className="flex items-center justify-center py-8">
              <Loader2 size={20} className="animate-spin" style={{ color: COLORS.ORANGE }} />
            </div>
          ) : (
            <div className="space-y-1">
              {(agentsByCategory[selectedCategory] || []).map(agent => (
                <button
                  key={agent.id}
                  onClick={() => {
                    setSelectedAgent(agent);
                    updateEditedConfigFromAgent(agent);
                  }}
                  className="w-full text-left px-3 py-2 rounded text-sm transition-colors"
                  style={{
                    backgroundColor: selectedAgent?.id === agent.id ? COLORS.HOVER : 'transparent',
                    color: selectedAgent?.id === agent.id ? COLORS.ORANGE : COLORS.WHITE,
                  }}
                >
                  <div className="font-medium truncate">{agent.name}</div>
                  <div className="text-xs truncate" style={{ color: COLORS.GRAY }}>
                    {agent.category} • v{agent.version}
                  </div>
                </button>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col overflow-hidden">
        {selectedAgent ? (
          <>
            {/* Agent Header */}
            <div className="p-4 border-b" style={{ borderColor: COLORS.BORDER }}>
              <div className="flex items-center justify-between">
                <div>
                  <h2 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>{selectedAgent.name}</h2>
                  <p className="text-sm" style={{ color: COLORS.GRAY }}>{selectedAgent.description}</p>
                </div>
                <div className="flex gap-2">
                  <button
                    onClick={() => addToTeam(selectedAgent)}
                    className="px-3 py-1.5 rounded text-sm flex items-center gap-1"
                    style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
                  >
                    <Plus size={14} /> Add to Team
                  </button>
                  <button
                    onClick={() => setShowJsonEditor(!showJsonEditor)}
                    className="px-3 py-1.5 rounded text-sm flex items-center gap-1"
                    style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
                  >
                    <FileJson size={14} /> {showJsonEditor ? 'Hide' : 'Show'} JSON
                  </button>
                </div>
              </div>

              {/* Capabilities */}
              {selectedAgent.capabilities && selectedAgent.capabilities.length > 0 && (
                <div className="mt-2 flex flex-wrap gap-1">
                  {selectedAgent.capabilities.map(cap => (
                    <span key={cap} className="px-2 py-0.5 rounded text-xs" style={{ backgroundColor: COLORS.BLUE + '20', color: COLORS.BLUE }}>
                      {cap}
                    </span>
                  ))}
                </div>
              )}
            </div>

            {/* Content */}
            <div className="flex-1 overflow-y-auto p-4 space-y-4">
              {showJsonEditor ? (
                <textarea
                  value={jsonText}
                  onChange={e => setJsonText(e.target.value)}
                  className="w-full h-64 p-3 rounded font-mono text-sm"
                  style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
                />
              ) : (
                <>
                  {/* Config Editor */}
                  {renderAgentConfigEditor()}

                  {/* SuperAgent Routing Toggle */}
                  <div className="p-3 rounded-lg flex items-center justify-between" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
                    <div className="flex items-center gap-2">
                      <Route size={16} style={{ color: COLORS.PURPLE }} />
                      <div>
                        <div className="text-sm font-medium" style={{ color: COLORS.WHITE }}>SuperAgent Auto-Routing</div>
                        <div className="text-xs" style={{ color: COLORS.GRAY }}>Automatically route queries to the best agent</div>
                      </div>
                    </div>
                    <button
                      onClick={() => setUseAutoRouting(!useAutoRouting)}
                      className="p-2 rounded"
                      style={{ color: useAutoRouting ? COLORS.GREEN : COLORS.GRAY }}
                    >
                      {useAutoRouting ? <ToggleRight size={24} /> : <ToggleLeft size={24} />}
                    </button>
                  </div>

                  {/* Last Routing Result */}
                  {lastRoutingResult && (
                    <div className="p-3 rounded-lg" style={{ backgroundColor: COLORS.PURPLE + '10', border: `1px solid ${COLORS.PURPLE}40` }}>
                      <div className="text-xs font-medium mb-1" style={{ color: COLORS.PURPLE }}>LAST ROUTING</div>
                      <div className="text-sm" style={{ color: COLORS.WHITE }}>
                        Intent: <span style={{ color: COLORS.ORANGE }}>{lastRoutingResult.intent}</span>
                        {' • '}Agent: <span style={{ color: COLORS.ORANGE }}>{lastRoutingResult.agent_id}</span>
                        {' • '}Confidence: <span style={{ color: COLORS.GREEN }}>{(lastRoutingResult.confidence * 100).toFixed(0)}%</span>
                      </div>
                    </div>
                  )}
                </>
              )}

              {/* Query Input */}
              <div className="space-y-3">
                <div>
                  <div className="text-xs font-medium mb-1" style={{ color: COLORS.GRAY }}>OUTPUT MODEL (Optional)</div>
                  <select
                    value={selectedOutputModel}
                    onChange={e => setSelectedOutputModel(e.target.value)}
                    className="w-full px-3 py-2 rounded text-sm"
                    style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
                  >
                    <option value="">None (free text)</option>
                    {OUTPUT_MODELS.map(m => (
                      <option key={m.id} value={m.id}>{m.name}</option>
                    ))}
                  </select>
                </div>

                <div>
                  <div className="text-xs font-medium mb-1" style={{ color: COLORS.GRAY }}>QUERY</div>
                  <textarea
                    value={testQuery}
                    onChange={e => setTestQuery(e.target.value)}
                    placeholder="Enter your query..."
                    rows={3}
                    className="w-full px-3 py-2 rounded text-sm"
                    style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
                  />
                </div>

                <button
                  onClick={runAgent}
                  disabled={loading}
                  className="w-full py-2 rounded font-medium flex items-center justify-center gap-2"
                  style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE, opacity: loading ? 0.5 : 1 }}
                >
                  {loading ? <Loader2 size={16} className="animate-spin" /> : <Play size={16} />}
                  {useAutoRouting ? 'Run with Auto-Routing' : 'Run Agent'}
                </button>
              </div>

              {/* Result */}
              {testResult && (
                <div className="p-3 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
                  <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>RESULT</div>
                  <pre className="text-sm whitespace-pre-wrap" style={{ color: COLORS.WHITE }}>
                    {typeof testResult === 'string' ? testResult : JSON.stringify(testResult, null, 2)}
                  </pre>
                </div>
              )}
            </div>
          </>
        ) : (
          <div className="flex-1 flex items-center justify-center" style={{ color: COLORS.GRAY }}>
            <div className="text-center">
              <Bot size={48} className="mx-auto mb-4 opacity-50" />
              <p>Select an agent to view details</p>
              <p className="text-sm mt-2">or click Refresh to discover agents</p>
            </div>
          </div>
        )}
      </div>
    </div>
  );

  // =============================================================================
  // Render: Teams View
  // =============================================================================

  const renderTeamsView = () => (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>Team Builder</h2>
        <select
          value={teamMode}
          onChange={e => setTeamMode(e.target.value as any)}
          className="px-3 py-1.5 rounded text-sm"
          style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
        >
          <option value="coordinate">Coordinate (Sequential)</option>
          <option value="route">Route (Best Match)</option>
          <option value="collaborate">Collaborate (Parallel)</option>
        </select>
      </div>

      {/* Team Members */}
      <div className="p-3 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>TEAM MEMBERS ({teamMembers.length})</div>
        {teamMembers.length === 0 ? (
          <p className="text-sm" style={{ color: COLORS.GRAY }}>Add agents from the AGENTS tab</p>
        ) : (
          <div className="space-y-2">
            {teamMembers.map((agent, idx) => (
              <div key={agent.id} className="flex items-center justify-between p-2 rounded" style={{ backgroundColor: COLORS.BORDER }}>
                <div className="flex items-center gap-2">
                  <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE }}>
                    {idx + 1}
                  </span>
                  <div>
                    <div className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{agent.name}</div>
                    <div className="text-xs" style={{ color: COLORS.GRAY }}>{agent.category}</div>
                  </div>
                </div>
                <button onClick={() => removeFromTeam(agent.id)} style={{ color: COLORS.RED }}>
                  <Trash2 size={14} />
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Available Agents to Add */}
      <div className="p-3 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>AVAILABLE AGENTS</div>
        <div className="max-h-48 overflow-y-auto space-y-1">
          {discoveredAgents
            .filter(a => !teamMembers.find(m => m.id === a.id))
            .map(agent => (
              <button
                key={agent.id}
                onClick={() => addToTeam(agent)}
                className="w-full flex items-center justify-between p-2 rounded text-sm hover:bg-[#1F1F1F]"
              >
                <span style={{ color: COLORS.WHITE }}>{agent.name}</span>
                <Plus size={14} style={{ color: COLORS.ORANGE }} />
              </button>
            ))}
        </div>
      </div>

      {/* Query */}
      <div>
        <div className="text-xs font-medium mb-1" style={{ color: COLORS.GRAY }}>TEAM QUERY</div>
        <textarea
          value={testQuery}
          onChange={e => setTestQuery(e.target.value)}
          rows={3}
          className="w-full px-3 py-2 rounded text-sm"
          style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
        />
      </div>

      <button
        onClick={runTeam}
        disabled={loading || teamMembers.length === 0}
        className="w-full py-2 rounded font-medium flex items-center justify-center gap-2"
        style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE, opacity: (loading || teamMembers.length === 0) ? 0.5 : 1 }}
      >
        {loading ? <Loader2 size={16} className="animate-spin" /> : <Play size={16} />}
        Run Team
      </button>

      {testResult && (
        <div className="p-3 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <pre className="text-sm whitespace-pre-wrap" style={{ color: COLORS.WHITE }}>
            {JSON.stringify(testResult, null, 2)}
          </pre>
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Workflows View
  // =============================================================================

  const renderWorkflowsView = () => (
    <div className="p-4 space-y-4">
      <h2 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>Financial Workflows</h2>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        {/* Stock Analysis */}
        <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <div className="flex items-center gap-2 mb-3">
            <TrendingUp size={20} style={{ color: COLORS.ORANGE }} />
            <h3 className="font-medium" style={{ color: COLORS.WHITE }}>Stock Analysis</h3>
          </div>
          <input
            type="text"
            value={workflowSymbol}
            onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())}
            placeholder="Symbol (e.g., AAPL)"
            className="w-full px-3 py-2 rounded text-sm mb-3"
            style={{ backgroundColor: COLORS.DARK_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
          />
          <button
            onClick={() => runWorkflow('stock_analysis')}
            disabled={loading}
            className="w-full py-2 rounded text-sm font-medium"
            style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE }}
          >
            Analyze
          </button>
        </div>

        {/* Portfolio Rebalancing */}
        <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <div className="flex items-center gap-2 mb-3">
            <Brain size={20} style={{ color: COLORS.ORANGE }} />
            <h3 className="font-medium" style={{ color: COLORS.WHITE }}>Portfolio Rebalancing</h3>
          </div>
          <p className="text-xs mb-3" style={{ color: COLORS.GRAY }}>
            Optimize your portfolio allocation based on risk/return targets.
          </p>
          <button
            onClick={() => runWorkflow('portfolio_rebal')}
            disabled={loading}
            className="w-full py-2 rounded text-sm font-medium"
            style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE }}
          >
            Rebalance
          </button>
        </div>

        {/* Risk Assessment */}
        <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <div className="flex items-center gap-2 mb-3">
            <AlertCircle size={20} style={{ color: COLORS.ORANGE }} />
            <h3 className="font-medium" style={{ color: COLORS.WHITE }}>Risk Assessment</h3>
          </div>
          <p className="text-xs mb-3" style={{ color: COLORS.GRAY }}>
            Analyze portfolio risk metrics and potential vulnerabilities.
          </p>
          <button
            onClick={() => runWorkflow('risk_assessment')}
            disabled={loading}
            className="w-full py-2 rounded text-sm font-medium"
            style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE }}
          >
            Assess Risk
          </button>
        </div>
      </div>

      {testResult && (
        <div className="p-3 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>WORKFLOW RESULT</div>
          <pre className="text-sm whitespace-pre-wrap" style={{ color: COLORS.WHITE }}>
            {JSON.stringify(testResult, null, 2)}
          </pre>
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Planner View (NEW)
  // =============================================================================

  const renderPlannerView = () => (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold flex items-center gap-2" style={{ color: COLORS.WHITE }}>
          <ListTree size={20} style={{ color: COLORS.ORANGE }} />
          Execution Planner
        </h2>
        {currentPlan && (
          <span
            className="px-2 py-1 rounded text-xs"
            style={{
              backgroundColor: currentPlan.is_complete ? COLORS.GREEN + '20' : currentPlan.has_failed ? COLORS.RED + '20' : COLORS.ORANGE + '20',
              color: currentPlan.is_complete ? COLORS.GREEN : currentPlan.has_failed ? COLORS.RED : COLORS.ORANGE,
            }}
          >
            {currentPlan.status}
          </span>
        )}
      </div>

      {/* Create Plan */}
      <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>CREATE EXECUTION PLAN</div>
        <div className="flex gap-2">
          <input
            type="text"
            value={workflowSymbol}
            onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())}
            placeholder="Symbol (e.g., AAPL)"
            className="flex-1 px-3 py-2 rounded text-sm"
            style={{ backgroundColor: COLORS.DARK_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
          />
          <button
            onClick={createPlan}
            disabled={loading}
            className="px-4 py-2 rounded text-sm font-medium flex items-center gap-2"
            style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE }}
          >
            {loading ? <Loader2 size={14} className="animate-spin" /> : <Target size={14} />}
            Create Plan
          </button>
        </div>
      </div>

      {/* Plan Visualization */}
      {currentPlan && (
        <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <div className="flex items-center justify-between mb-4">
            <div>
              <div className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{currentPlan.name}</div>
              <div className="text-xs" style={{ color: COLORS.GRAY }}>{currentPlan.description}</div>
            </div>
            <button
              onClick={executePlan}
              disabled={planExecuting || currentPlan.is_complete}
              className="px-4 py-2 rounded text-sm font-medium flex items-center gap-2"
              style={{
                backgroundColor: currentPlan.is_complete ? COLORS.GREEN : COLORS.ORANGE,
                color: COLORS.WHITE,
                opacity: planExecuting ? 0.5 : 1,
              }}
            >
              {planExecuting ? <Loader2 size={14} className="animate-spin" /> : <Play size={14} />}
              {currentPlan.is_complete ? 'Completed' : 'Execute Plan'}
            </button>
          </div>

          {/* Steps */}
          <div className="space-y-2">
            {currentPlan.steps.map((step, idx) => (
              <div
                key={step.id}
                className="flex items-center gap-3 p-3 rounded"
                style={{
                  backgroundColor: COLORS.BORDER,
                  borderLeft: `3px solid ${
                    step.status === 'completed' ? COLORS.GREEN :
                    step.status === 'failed' ? COLORS.RED :
                    step.status === 'running' ? COLORS.ORANGE :
                    COLORS.GRAY
                  }`,
                }}
              >
                <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.DARK_BG, color: COLORS.GRAY }}>
                  {idx + 1}
                </span>
                <div className="flex-1">
                  <div className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{step.name}</div>
                  <div className="text-xs" style={{ color: COLORS.GRAY }}>
                    Type: {step.step_type}
                    {step.dependencies.length > 0 && ` • Depends on: ${step.dependencies.join(', ')}`}
                  </div>
                </div>
                <span
                  className="text-xs px-2 py-0.5 rounded"
                  style={{
                    backgroundColor: step.status === 'completed' ? COLORS.GREEN + '20' :
                      step.status === 'failed' ? COLORS.RED + '20' :
                      step.status === 'running' ? COLORS.ORANGE + '20' :
                      COLORS.BORDER,
                    color: step.status === 'completed' ? COLORS.GREEN :
                      step.status === 'failed' ? COLORS.RED :
                      step.status === 'running' ? COLORS.ORANGE :
                      COLORS.GRAY,
                  }}
                >
                  {step.status}
                </span>
              </div>
            ))}
          </div>

          {/* Results */}
          {currentPlan.is_complete && currentPlan.context && (
            <div className="mt-4 p-3 rounded" style={{ backgroundColor: COLORS.DARK_BG }}>
              <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>PLAN RESULTS</div>
              <pre className="text-xs whitespace-pre-wrap" style={{ color: COLORS.WHITE }}>
                {JSON.stringify(currentPlan.context, null, 2)}
              </pre>
            </div>
          )}
        </div>
      )}

      {/* Plan Templates */}
      <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="text-xs font-medium mb-3" style={{ color: COLORS.GRAY }}>PLAN TEMPLATES</div>
        <div className="grid grid-cols-2 gap-2">
          {[
            { id: 'stock_analysis', name: 'Stock Analysis', desc: 'Full fundamental + technical analysis' },
            { id: 'portfolio_rebalance', name: 'Portfolio Rebalance', desc: 'Analyze and rebalance portfolio' },
            { id: 'sector_rotation', name: 'Sector Rotation', desc: 'Identify sector opportunities' },
            { id: 'risk_audit', name: 'Risk Audit', desc: 'Comprehensive risk assessment' },
          ].map(template => (
            <button
              key={template.id}
              onClick={() => {
                setWorkflowSymbol('AAPL');
                createPlan();
              }}
              className="p-3 rounded text-left hover:bg-[#1F1F1F] transition-colors"
              style={{ backgroundColor: COLORS.BORDER }}
            >
              <div className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{template.name}</div>
              <div className="text-xs" style={{ color: COLORS.GRAY }}>{template.desc}</div>
            </button>
          ))}
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Tools View
  // =============================================================================

  const renderToolsView = () => (
    <div className="p-4 space-y-4">
      <div className="flex items-center gap-3">
        <h2 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>Available Tools</h2>
        {toolsInfo && (
          <span className="px-2 py-0.5 rounded text-xs" style={{ backgroundColor: COLORS.ORANGE + '20', color: COLORS.ORANGE }}>
            {toolsInfo.total_count} tools
          </span>
        )}
      </div>

      {/* Search */}
      <div className="relative">
        <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2" style={{ color: COLORS.GRAY }} />
        <input
          type="text"
          value={toolSearch}
          onChange={e => setToolSearch(e.target.value)}
          placeholder="Search tools..."
          className="w-full pl-10 pr-3 py-2 rounded text-sm"
          style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
        />
      </div>

      {/* Selected Tools */}
      {selectedTools.length > 0 && (
        <div className="p-3 rounded" style={{ backgroundColor: COLORS.ORANGE + '10', border: `1px solid ${COLORS.ORANGE}40` }}>
          <div className="text-xs font-medium mb-2" style={{ color: COLORS.ORANGE }}>SELECTED ({selectedTools.length})</div>
          <div className="flex flex-wrap gap-1">
            {selectedTools.map(tool => (
              <span
                key={tool}
                onClick={() => toggleToolSelection(tool)}
                className="px-2 py-0.5 rounded text-xs cursor-pointer"
                style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE }}
              >
                {tool} ×
              </span>
            ))}
          </div>
        </div>
      )}

      {/* Tool Categories */}
      <div className="space-y-2">
        {toolsInfo?.categories?.map(category => {
          const tools = toolsInfo.tools[category] || [];
          const filteredTools = toolSearch
            ? tools.filter(t => t.toLowerCase().includes(toolSearch.toLowerCase()))
            : tools;

          if (filteredTools.length === 0) return null;

          return (
            <div key={category} className="rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
              <button
                onClick={() => toggleToolCategory(category)}
                className="w-full flex items-center justify-between p-3"
              >
                <span className="font-medium text-sm" style={{ color: COLORS.WHITE }}>{category}</span>
                <div className="flex items-center gap-2">
                  <span className="text-xs" style={{ color: COLORS.GRAY }}>{filteredTools.length}</span>
                  {expandedCategories.has(category) ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
                </div>
              </button>
              {expandedCategories.has(category) && (
                <div className="px-3 pb-3 flex flex-wrap gap-1">
                  {filteredTools.map(tool => (
                    <span
                      key={tool}
                      onClick={() => toggleToolSelection(tool)}
                      className="px-2 py-1 rounded text-xs cursor-pointer flex items-center gap-1"
                      style={{
                        backgroundColor: selectedTools.includes(tool) ? COLORS.ORANGE : COLORS.BORDER,
                        color: COLORS.WHITE,
                      }}
                    >
                      {tool}
                      <button onClick={e => { e.stopPropagation(); copyTool(tool); }}>
                        {copiedTool === tool ? <Check size={10} /> : <Copy size={10} />}
                      </button>
                    </span>
                  ))}
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );

  // =============================================================================
  // Render: Chat View
  // =============================================================================

  const handleChatSubmit = async () => {
    if (!chatInput.trim() || loading) return;

    const query = chatInput;
    setChatInput('');
    setTestQuery(query);

    // Add user message
    setChatMessages(prev => [...prev, { role: 'user', content: query, timestamp: new Date() }]);

    setLoading(true);
    try {
      const apiKeys = await getApiKeys();

      if (useAutoRouting) {
        // Route and execute
        const routingResult = await agentService.routeQuery(query, apiKeys);
        setLastRoutingResult(routingResult);

        setChatMessages(prev => [
          ...prev,
          {
            role: 'system',
            content: `🎯 Routed to: ${routingResult.agent_id} (${routingResult.intent})`,
            timestamp: new Date(),
            routingInfo: routingResult,
          },
        ]);

        const result = await agentService.executeRoutedQuery(query, apiKeys);
        setChatMessages(prev => [
          ...prev,
          {
            role: 'assistant',
            content: result.response || result.error || 'No response',
            timestamp: new Date(),
            agentName: routingResult.agent_id,
          },
        ]);
      } else {
        // Direct agent execution
        const config = agentService.buildAgentConfig(
          editedConfig.provider,
          editedConfig.model_id,
          editedConfig.instructions,
          {
            tools: selectedTools.length > 0 ? selectedTools : editedConfig.tools,
            temperature: editedConfig.temperature,
            maxTokens: editedConfig.max_tokens,
            memory: editedConfig.enable_memory,
          }
        );

        const result = await agentService.runAgent(query, config, apiKeys);
        setChatMessages(prev => [
          ...prev,
          {
            role: 'assistant',
            content: result.response || result.error || 'No response',
            timestamp: new Date(),
            agentName: selectedAgent?.name || 'Agent',
          },
        ]);
      }
    } catch (err: any) {
      setChatMessages(prev => [
        ...prev,
        { role: 'system', content: `Error: ${err.message}`, timestamp: new Date() },
      ]);
    } finally {
      setLoading(false);
    }
  };

  const renderChatView = () => (
    <div className="flex flex-col h-full">
      {/* Chat Header */}
      <div className="p-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <MessageSquare size={16} style={{ color: COLORS.ORANGE }} />
          <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>Agent Chat</span>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-xs" style={{ color: COLORS.GRAY }}>Auto-Route</span>
          <button
            onClick={() => setUseAutoRouting(!useAutoRouting)}
            style={{ color: useAutoRouting ? COLORS.GREEN : COLORS.GRAY }}
          >
            {useAutoRouting ? <ToggleRight size={20} /> : <ToggleLeft size={20} />}
          </button>
        </div>
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto p-4 space-y-3">
        {chatMessages.length === 0 ? (
          <div className="text-center py-8" style={{ color: COLORS.GRAY }}>
            <Sparkles size={32} className="mx-auto mb-2" />
            <p>Start a conversation</p>
            <p className="text-xs mt-1">
              {useAutoRouting ? 'Queries will be auto-routed to the best agent' : `Using: ${selectedAgent?.name || 'No agent selected'}`}
            </p>
          </div>
        ) : (
          chatMessages.map((msg, i) => (
            <div key={i} className={`flex ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
              <div
                className="max-w-[85%] p-3 rounded-lg"
                style={{
                  backgroundColor: msg.role === 'user' ? COLORS.ORANGE :
                    msg.role === 'system' ? COLORS.PURPLE + '20' : COLORS.PANEL_BG,
                  color: COLORS.WHITE,
                  border: msg.role === 'system' ? `1px solid ${COLORS.PURPLE}40` : 'none',
                }}
              >
                {msg.agentName && (
                  <div className="text-xs mb-1 flex items-center gap-1" style={{ color: COLORS.ORANGE }}>
                    <Bot size={12} />
                    {msg.agentName}
                  </div>
                )}
                <div className="text-sm whitespace-pre-wrap">{msg.content}</div>
                <div className="text-xs mt-1 opacity-60">
                  {msg.timestamp.toLocaleTimeString()}
                </div>
              </div>
            </div>
          ))
        )}
        <div ref={chatEndRef} />
      </div>

      {/* Input */}
      <div className="p-4 border-t" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex gap-2">
          <input
            type="text"
            value={chatInput}
            onChange={e => setChatInput(e.target.value)}
            onKeyDown={e => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                handleChatSubmit();
              }
            }}
            placeholder={useAutoRouting ? 'Ask anything - auto-routed to best agent...' : 'Type a message...'}
            className="flex-1 px-3 py-2 rounded text-sm"
            style={{ backgroundColor: COLORS.PANEL_BG, color: COLORS.WHITE, border: `1px solid ${COLORS.BORDER}` }}
          />
          <button
            onClick={handleChatSubmit}
            disabled={loading || !chatInput.trim()}
            className="px-4 py-2 rounded flex items-center gap-2"
            style={{ backgroundColor: COLORS.ORANGE, color: COLORS.WHITE, opacity: loading ? 0.5 : 1 }}
          >
            {loading ? <Loader2 size={16} className="animate-spin" /> : <Zap size={16} />}
            Send
          </button>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: System View
  // =============================================================================

  const renderSystemView = () => (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>System Capabilities</h2>
        <button
          onClick={() => loadSystemData()}
          className="p-2 rounded"
          style={{ backgroundColor: COLORS.BORDER }}
        >
          <RefreshCw size={14} style={{ color: COLORS.WHITE }} />
        </button>
      </div>

      {systemInfo ? (
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <div className="p-4 rounded text-center" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <div className="text-2xl font-bold" style={{ color: COLORS.ORANGE }}>{systemInfo.capabilities.model_providers}</div>
            <div className="text-xs" style={{ color: COLORS.GRAY }}>Model Providers</div>
          </div>
          <div className="p-4 rounded text-center" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <div className="text-2xl font-bold" style={{ color: COLORS.ORANGE }}>{systemInfo.capabilities.tools}</div>
            <div className="text-xs" style={{ color: COLORS.GRAY }}>Tools</div>
          </div>
          <div className="p-4 rounded text-center" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <div className="text-2xl font-bold" style={{ color: COLORS.ORANGE }}>{discoveredAgents.length}</div>
            <div className="text-xs" style={{ color: COLORS.GRAY }}>Discovered Agents</div>
          </div>
          <div className="p-4 rounded text-center" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
            <div className="text-2xl font-bold" style={{ color: COLORS.ORANGE }}>{configuredLLMs.length}</div>
            <div className="text-xs" style={{ color: COLORS.GRAY }}>Configured LLMs</div>
          </div>
        </div>
      ) : (
        <div className="text-center py-8" style={{ color: COLORS.GRAY }}>Loading system info...</div>
      )}

      {/* Configured LLMs from Settings */}
      <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="flex items-center justify-between mb-3">
          <div className="text-xs font-medium" style={{ color: COLORS.GRAY }}>CONFIGURED LLM PROVIDERS (from Settings)</div>
          <button
            onClick={() => loadLLMConfigs()}
            className="px-2 py-1 rounded text-xs"
            style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
          >
            Refresh
          </button>
        </div>
        {configuredLLMs.length > 0 ? (
          <div className="space-y-2">
            {configuredLLMs.map((llm, idx) => {
              const hasKey = !!llm.api_key && llm.api_key.length > 0;
              return (
                <div
                  key={idx}
                  className="flex items-center justify-between p-2 rounded"
                  style={{ backgroundColor: COLORS.BORDER }}
                >
                  <div className="flex items-center gap-2">
                    {llm.is_active && (
                      <span className="px-1.5 py-0.5 rounded text-xs" style={{ backgroundColor: COLORS.GREEN + '30', color: COLORS.GREEN }}>
                        Active
                      </span>
                    )}
                    <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                      {llm.provider.charAt(0).toUpperCase() + llm.provider.slice(1)}
                    </span>
                    <span className="text-xs" style={{ color: COLORS.GRAY }}>
                      {llm.model}
                    </span>
                  </div>
                  <div className="flex items-center gap-2">
                    {hasKey ? (
                      <span className="flex items-center gap-1 text-xs" style={{ color: COLORS.GREEN }}>
                        <CheckCircle size={12} /> API Key Set
                      </span>
                    ) : (
                      <span className="flex items-center gap-1 text-xs" style={{ color: COLORS.RED }}>
                        <AlertCircle size={12} /> No API Key
                      </span>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        ) : (
          <div className="text-center py-4" style={{ color: COLORS.GRAY }}>
            <AlertCircle size={24} className="mx-auto mb-2" />
            <p className="text-sm">No LLM providers configured</p>
            <p className="text-xs mt-1">Go to Settings → LLM Configuration to add providers</p>
          </div>
        )}
        {configuredLLMs.filter(l => !l.api_key || l.api_key.length === 0).length > 0 && (
          <div className="mt-3 p-2 rounded text-xs" style={{ backgroundColor: COLORS.RED + '10', color: COLORS.RED }}>
            {configuredLLMs.filter(l => !l.api_key || l.api_key.length === 0).length} provider(s) missing API keys. Go to Settings → LLM Configuration to add them.
          </div>
        )}
      </div>

      {/* Cache Stats */}
      <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="flex items-center justify-between mb-3">
          <div className="text-xs font-medium" style={{ color: COLORS.GRAY }}>CACHE STATISTICS</div>
          <button
            onClick={() => agentService.clearCache()}
            className="px-2 py-1 rounded text-xs"
            style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
          >
            Clear Cache
          </button>
        </div>
        <div className="grid grid-cols-3 gap-4 text-sm">
          <div>
            <div style={{ color: COLORS.GRAY }}>Static Cache:</div>
            <div style={{ color: COLORS.WHITE }}>{agentService.getCacheStats().cacheSize} items</div>
          </div>
          <div>
            <div style={{ color: COLORS.GRAY }}>Response Cache:</div>
            <div style={{ color: COLORS.WHITE }}>{agentService.getCacheStats().responseCacheSize} items</div>
          </div>
          <div>
            <div style={{ color: COLORS.GRAY }}>Max Size:</div>
            <div style={{ color: COLORS.WHITE }}>{agentService.getCacheStats().maxSize} items</div>
          </div>
        </div>
      </div>

      {systemInfo?.features && (
        <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
          <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>FEATURES</div>
          <div className="flex flex-wrap gap-2">
            {systemInfo.features.map(feature => (
              <span key={feature} className="px-2 py-1 rounded text-xs" style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}>
                {feature.replace(/_/g, ' ')}
              </span>
            ))}
          </div>
        </div>
      )}

      <div className="p-4 rounded" style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}>
        <div className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>VERSION INFO</div>
        <div className="grid grid-cols-2 gap-2 text-sm">
          <div style={{ color: COLORS.GRAY }}>Version:</div>
          <div style={{ color: COLORS.WHITE }}>{systemInfo?.version || 'N/A'}</div>
          <div style={{ color: COLORS.GRAY }}>Framework:</div>
          <div style={{ color: COLORS.WHITE }}>{systemInfo?.framework || 'N/A'}</div>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Main Render
  // =============================================================================

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: COLORS.DARK_BG }}>
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-3">
          <Bot size={20} style={{ color: COLORS.ORANGE }} />
          <h1 className="text-lg font-semibold" style={{ color: COLORS.WHITE }}>Agent Studio</h1>
          <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.PURPLE + '20', color: COLORS.PURPLE }}>
            v2.1
          </span>
        </div>

        {/* View Mode Tabs */}
        <div className="flex gap-1">
          {VIEW_MODES.map(mode => (
            <button
              key={mode.id}
              onClick={() => setViewMode(mode.id)}
              className="px-3 py-1.5 rounded text-xs font-medium flex items-center gap-1.5 transition-colors"
              style={{
                backgroundColor: viewMode === mode.id ? COLORS.ORANGE : 'transparent',
                color: viewMode === mode.id ? COLORS.WHITE : COLORS.GRAY,
              }}
            >
              <mode.icon size={14} />
              {mode.name}
            </button>
          ))}
        </div>
      </div>

      {/* Notifications */}
      {error && (
        <div className="mx-4 mt-2 p-3 rounded flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '20', color: COLORS.RED }}>
          <AlertCircle size={16} />
          <span className="text-sm flex-1">{error}</span>
          <button onClick={() => setError(null)}>×</button>
        </div>
      )}
      {success && (
        <div className="mx-4 mt-2 p-3 rounded flex items-center gap-2" style={{ backgroundColor: COLORS.GREEN + '20', color: COLORS.GREEN }}>
          <CheckCircle size={16} />
          <span className="text-sm">{success}</span>
        </div>
      )}

      {/* Content */}
      <div className="flex-1 overflow-hidden">
        {viewMode === 'agents' && renderAgentsView()}
        {viewMode === 'teams' && renderTeamsView()}
        {viewMode === 'workflows' && renderWorkflowsView()}
        {viewMode === 'planner' && renderPlannerView()}
        {viewMode === 'tools' && renderToolsView()}
        {viewMode === 'chat' && renderChatView()}
        {viewMode === 'system' && renderSystemView()}
      </div>

      <TabFooter tabName="Agent Studio v2.1" />
    </div>
  );
};

export default AgentConfigTab;
