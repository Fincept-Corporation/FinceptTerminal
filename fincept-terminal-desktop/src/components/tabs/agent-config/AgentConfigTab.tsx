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
  Loader2, Target, FileJson, Sliders, Database, Eye, Workflow,
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
// Constants & Types - Fincept Professional Terminal Palette
// =============================================================================

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

// Terminal Styles
const TERMINAL_STYLES = `
  .agent-terminal * {
    font-family: "IBM Plex Mono", "Consolas", monospace;
  }
  .agent-terminal::-webkit-scrollbar { width: 6px; height: 6px; }
  .agent-terminal::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .agent-terminal::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  .agent-terminal::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  .agent-hover:hover { background-color: ${FINCEPT.HOVER} !important; }
  .agent-selected {
    background-color: ${FINCEPT.ORANGE}15 !important;
    border-left: 2px solid ${FINCEPT.ORANGE} !important;
  }
  @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
  .agent-pulse { animation: pulse 2s infinite; }
`;

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
    // Load all data in parallel with timeout
    const loadInitialData = async () => {
      setLoading(true);
      setError(null);

      try {
        // Set a timeout for the entire loading process
        const timeoutPromise = new Promise((_, reject) =>
          setTimeout(() => reject(new Error('Loading timeout - please check your backend service')), 10000)
        );

        const loadPromise = Promise.all([
          loadDiscoveredAgents(),
          loadSystemData(),
          loadLLMConfigs(),
        ]);

        await Promise.race([loadPromise, timeoutPromise]);
      } catch (err: any) {
        console.error('Failed to load initial data:', err);
        setError(err.message || 'Failed to load agent data');
      } finally {
        setLoading(false);
      }
    };

    loadInitialData();
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
    try {
      // Add timeout for system data loading
      const timeoutPromise = new Promise<never>((_, reject) =>
        setTimeout(() => reject(new Error('System data loading timeout')), 5000)
      );

      const dataPromise = Promise.all([
        agentService.getSystemInfo(),
        agentService.getTools(),
      ]);

      const [sysInfo, tools] = await Promise.race([dataPromise, timeoutPromise]);

      if (sysInfo) setSystemInfo(sysInfo);
      if (tools) setToolsInfo(tools);
    } catch (err) {
      console.error('Failed to load system data:', err);
      // Set default/empty system info
      setSystemInfo(null);
      setToolsInfo(null);
    }
  };

  const loadDiscoveredAgents = async () => {
    // Strategy: Use fast Rust-based discovery first (list_available_agents)
    // Only fall back to Python-based discovery if Rust fails

    try {
      // First try: Fast Rust-based config file reading (no Python needed)
      console.log('[AgentConfigTab] Trying fast Rust-based agent discovery...');
      const agents = await invoke<AgentCard[]>('list_available_agents');

      if (Array.isArray(agents) && agents.length > 0) {
        console.log(`[AgentConfigTab] Loaded ${agents.length} agents via Rust (fast path)`);
        setAgentsFromList(agents);
        return;
      }
    } catch (rustErr) {
      console.warn('[AgentConfigTab] Rust discovery failed, trying Python fallback:', rustErr);
    }

    // Second try: Python-based discovery with timeout (slower but more comprehensive)
    try {
      const timeoutPromise = new Promise<AgentCard[]>((_, reject) =>
        setTimeout(() => reject(new Error('Agent discovery timeout')), 5000)
      );

      const discoveryPromise = agentService.discoverAgents();
      const agents = await Promise.race([discoveryPromise, timeoutPromise]);

      if (Array.isArray(agents) && agents.length > 0) {
        console.log(`[AgentConfigTab] Loaded ${agents.length} agents via Python discovery`);
        setAgentsFromList(agents);
        return;
      }
    } catch (pythonErr) {
      console.warn('[AgentConfigTab] Python discovery failed:', pythonErr);
    }

    // All methods failed - set empty state
    console.log('[AgentConfigTab] All discovery methods failed, using empty state');
    setDiscoveredAgents([]);
    setAgentsByCategory({ all: [] });
  };

  const setAgentsFromList = (agents: AgentCard[]) => {
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
  // Render: Agent Config Editor - Terminal Style
  // =============================================================================

  const renderAgentConfigEditor = () => (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
    }}>
      {/* Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Sliders size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
            AGENT CONFIGURATION
          </span>
        </div>
        <button
          onClick={() => setEditMode(!editMode)}
          style={{
            padding: '4px 10px',
            backgroundColor: editMode ? `${FINCEPT.GREEN}20` : 'transparent',
            border: `1px solid ${editMode ? FINCEPT.GREEN : FINCEPT.BORDER}`,
            color: editMode ? FINCEPT.GREEN : FINCEPT.GRAY,
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            borderRadius: '2px',
          }}
        >
          <Edit3 size={10} />
          {editMode ? 'EDITING' : 'EDIT'}
        </button>
      </div>

      <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* LLM Selection */}
        {configuredLLMs.length > 0 ? (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.GREEN}40`,
            borderRadius: '2px',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              marginBottom: '10px',
            }}>
              <CheckCircle size={10} style={{ color: FINCEPT.GREEN }} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GREEN }}>
                SELECT LLM PROVIDER
              </span>
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
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
                    style={{
                      padding: '6px 10px',
                      backgroundColor: isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER,
                      color: isSelected ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                      border: 'none',
                      borderRadius: '2px',
                      fontSize: '9px',
                      fontWeight: 600,
                      cursor: editMode ? 'pointer' : 'default',
                      opacity: hasKey ? 1 : 0.5,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                    }}
                  >
                    {llm.is_active && <span style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.GREEN }}>●</span>}
                    <span>{llm.provider.toUpperCase()}</span>
                    <span style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.GRAY }}>/ {llm.model}</span>
                    {!hasKey && <AlertCircle size={10} style={{ color: FINCEPT.RED }} />}
                  </button>
                );
              })}
            </div>
            {configuredLLMs.some(l => !l.api_key || l.api_key.length === 0) && (
              <div style={{ fontSize: '9px', marginTop: '8px', color: FINCEPT.RED }}>
                ⚠ Some providers missing API keys - configure in Settings
              </div>
            )}
          </div>
        ) : (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.RED}40`,
            borderRadius: '2px',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              marginBottom: '6px',
            }}>
              <AlertCircle size={10} style={{ color: FINCEPT.RED }} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED }}>
                NO LLM CONFIGURED
              </span>
            </div>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              Go to Settings → LLM Configuration to add API keys.
            </span>
          </div>
        )}

        {/* Parameters Grid */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {/* Provider */}
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              PROVIDER
            </label>
            <div style={{
              padding: '8px 10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              color: FINCEPT.CYAN,
            }}>
              {editedConfig.provider || 'None'}
            </div>
          </div>

          {/* Model */}
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              MODEL
            </label>
            <div style={{
              padding: '8px 10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              color: FINCEPT.CYAN,
            }}>
              {editedConfig.model_id || 'None'}
            </div>
          </div>

          {/* Temperature */}
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              TEMPERATURE: <span style={{ color: FINCEPT.YELLOW }}>{editedConfig.temperature.toFixed(2)}</span>
            </label>
            <input
              type="range"
              min="0"
              max="2"
              step="0.1"
              value={editedConfig.temperature}
              onChange={e => setEditedConfig(prev => ({ ...prev, temperature: parseFloat(e.target.value) }))}
              disabled={!editMode}
              style={{
                width: '100%',
                accentColor: FINCEPT.ORANGE,
                opacity: editMode ? 1 : 0.7,
              }}
            />
          </div>

          {/* Max Tokens */}
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              MAX TOKENS
            </label>
            <input
              type="number"
              value={editedConfig.max_tokens}
              onChange={e => setEditedConfig(prev => ({ ...prev, max_tokens: parseInt(e.target.value) || 4096 }))}
              disabled={!editMode}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.YELLOW,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                opacity: editMode ? 1 : 0.7,
              }}
            />
          </div>
        </div>

        {/* Toggle Options */}
        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={() => editMode && setEditedConfig(prev => ({ ...prev, enable_memory: !prev.enable_memory }))}
            style={{
              padding: '8px 12px',
              backgroundColor: editedConfig.enable_memory ? `${FINCEPT.ORANGE}20` : FINCEPT.DARK_BG,
              border: `1px solid ${editedConfig.enable_memory ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              color: editedConfig.enable_memory ? FINCEPT.ORANGE : FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: editMode ? 'pointer' : 'default',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              borderRadius: '2px',
            }}
          >
            {editedConfig.enable_memory ? <ToggleRight size={14} /> : <ToggleLeft size={14} />}
            MEMORY
          </button>

          <button
            onClick={() => editMode && setEditedConfig(prev => ({ ...prev, enable_reasoning: !prev.enable_reasoning }))}
            style={{
              padding: '8px 12px',
              backgroundColor: editedConfig.enable_reasoning ? `${FINCEPT.PURPLE}20` : FINCEPT.DARK_BG,
              border: `1px solid ${editedConfig.enable_reasoning ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
              color: editedConfig.enable_reasoning ? FINCEPT.PURPLE : FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: editMode ? 'pointer' : 'default',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              borderRadius: '2px',
            }}
          >
            {editedConfig.enable_reasoning ? <ToggleRight size={14} /> : <ToggleLeft size={14} />}
            REASONING
          </button>
        </div>

        {/* Instructions */}
        {editMode && (
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              INSTRUCTIONS
            </label>
            <textarea
              value={editedConfig.instructions}
              onChange={e => setEditedConfig(prev => ({ ...prev, instructions: e.target.value }))}
              rows={3}
              placeholder="Enter agent instructions..."
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                resize: 'none',
              }}
            />
          </div>
        )}

        {/* Selected Tools */}
        {editedConfig.tools.length > 0 && (
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              TOOLS ({editedConfig.tools.length})
            </label>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
              {editedConfig.tools.map(tool => (
                <span
                  key={tool}
                  onClick={() => editMode && toggleToolSelection(tool)}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: `${FINCEPT.ORANGE}20`,
                    color: FINCEPT.ORANGE,
                    fontSize: '9px',
                    borderRadius: '2px',
                    cursor: editMode ? 'pointer' : 'default',
                  }}
                >
                  {tool} {editMode && '×'}
                </span>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );

  // =============================================================================
  // Render: Agents View - Three Panel Terminal Layout
  // =============================================================================

  const renderAgentsView = () => (
    <div style={{
      display: 'flex',
      height: '100%',
      overflow: 'hidden',
    }}>
      {/* LEFT SIDEBAR - Agent List (280px fixed) */}
      <div style={{
        width: '280px',
        minWidth: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        {/* Sidebar Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '8px',
          }}>
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
            }}>
              AGENTS ({discoveredAgents.length})
            </span>
            <button
              onClick={loadDiscoveredAgents}
              disabled={loading}
              style={{
                padding: '4px',
                backgroundColor: 'transparent',
                border: 'none',
                cursor: loading ? 'not-allowed' : 'pointer',
                opacity: loading ? 0.5 : 1,
              }}
            >
              <RefreshCw size={12} className={loading ? 'animate-spin' : ''} style={{ color: FINCEPT.ORANGE }} />
            </button>
          </div>
          {/* Category Filter */}
          <select
            value={selectedCategory}
            onChange={e => setSelectedCategory(e.target.value)}
            style={{
              width: '100%',
              padding: '6px 8px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              cursor: 'pointer',
            }}
          >
            <option value="all">All Categories ({discoveredAgents.length})</option>
            {Object.entries(agentsByCategory)
              .filter(([k]) => k !== 'all')
              .map(([cat, agents]) => (
                <option key={cat} value={cat}>{cat.toUpperCase()} ({agents.length})</option>
              ))}
          </select>
        </div>

        {/* Agent List */}
        <div style={{ flex: 1, overflow: 'auto', padding: '4px' }}>
          {loading ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '32px 16px',
            }}>
              <Loader2 size={20} className="animate-spin" style={{ color: FINCEPT.ORANGE, marginBottom: '12px' }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Loading agents...</span>
            </div>
          ) : discoveredAgents.length === 0 ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '32px 16px',
              textAlign: 'center',
            }}>
              <Bot size={32} style={{ color: FINCEPT.MUTED, marginBottom: '12px', opacity: 0.5 }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>No agents discovered</span>
              <button
                onClick={loadDiscoveredAgents}
                style={{
                  padding: '6px 12px',
                  backgroundColor: FINCEPT.ORANGE,
                  color: FINCEPT.WHITE,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                }}
              >
                <RefreshCw size={10} /> RETRY
              </button>
            </div>
          ) : (
            (agentsByCategory[selectedCategory] || []).map(agent => (
              <div
                key={agent.id}
                onClick={() => {
                  setSelectedAgent(agent);
                  updateEditedConfigFromAgent(agent);
                }}
                style={{
                  padding: '10px 12px',
                  marginBottom: '2px',
                  backgroundColor: selectedAgent?.id === agent.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: selectedAgent?.id === agent.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  if (selectedAgent?.id !== agent.id) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedAgent?.id !== agent.id) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: '4px',
                }}>
                  <span style={{
                    fontSize: '11px',
                    fontWeight: 600,
                    color: selectedAgent?.id === agent.id ? FINCEPT.ORANGE : FINCEPT.WHITE,
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                  }}>
                    {agent.name}
                  </span>
                  <span style={{
                    fontSize: '8px',
                    padding: '2px 4px',
                    backgroundColor: `${FINCEPT.CYAN}20`,
                    color: FINCEPT.CYAN,
                    borderRadius: '2px',
                  }}>
                    v{agent.version}
                  </span>
                </div>
                <div style={{
                  fontSize: '9px',
                  color: FINCEPT.GRAY,
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  whiteSpace: 'nowrap',
                }}>
                  {agent.category}
                </div>
              </div>
            ))
          )}
        </div>

        {/* Quick Stats at Bottom */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'grid',
          gridTemplateColumns: '1fr 1fr',
          gap: '8px',
        }}>
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.DARK_BG,
            borderRadius: '2px',
            textAlign: 'center',
          }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.CYAN }}>{discoveredAgents.length}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>TOTAL</div>
          </div>
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.DARK_BG,
            borderRadius: '2px',
            textAlign: 'center',
          }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN }}>{teamMembers.length}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>IN TEAM</div>
          </div>
        </div>
      </div>

      {/* CENTER - Main Content Area */}
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        minWidth: 0,
      }}>
        {selectedAgent ? (
          <>
            {/* Agent Info Bar */}
            <div style={{
              padding: '12px 16px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  marginBottom: '4px',
                }}>
                  <span style={{
                    fontSize: '14px',
                    fontWeight: 700,
                    color: FINCEPT.WHITE,
                  }}>
                    {selectedAgent.name}
                  </span>
                  {selectedAgent.capabilities && selectedAgent.capabilities.slice(0, 3).map(cap => (
                    <span key={cap} style={{
                      padding: '2px 6px',
                      backgroundColor: `${FINCEPT.BLUE}20`,
                      color: FINCEPT.BLUE,
                      fontSize: '8px',
                      borderRadius: '2px',
                    }}>
                      {cap}
                    </span>
                  ))}
                </div>
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.GRAY,
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  whiteSpace: 'nowrap',
                }}>
                  {selectedAgent.description}
                </div>
              </div>
              <div style={{ display: 'flex', gap: '8px', flexShrink: 0 }}>
                <button
                  onClick={() => addToTeam(selectedAgent)}
                  style={{
                    padding: '6px 10px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    borderRadius: '2px',
                  }}
                >
                  <Plus size={10} /> ADD TO TEAM
                </button>
                <button
                  onClick={() => setShowJsonEditor(!showJsonEditor)}
                  style={{
                    padding: '6px 10px',
                    backgroundColor: showJsonEditor ? `${FINCEPT.PURPLE}20` : 'transparent',
                    border: `1px solid ${showJsonEditor ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                    color: showJsonEditor ? FINCEPT.PURPLE : FINCEPT.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    borderRadius: '2px',
                  }}
                >
                  <FileJson size={10} /> JSON
                </button>
              </div>
            </div>

            {/* Main Content - Split View */}
            <div style={{
              flex: 1,
              display: 'flex',
              overflow: 'hidden',
            }}>
              {/* Config Area */}
              <div style={{
                flex: 1,
                overflow: 'auto',
                padding: '16px',
              }}>
                {showJsonEditor ? (
                  <textarea
                    value={jsonText}
                    onChange={e => setJsonText(e.target.value)}
                    style={{
                      width: '100%',
                      height: '100%',
                      padding: '12px',
                      backgroundColor: FINCEPT.DARK_BG,
                      color: FINCEPT.WHITE,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '11px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      resize: 'none',
                    }}
                  />
                ) : (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                    {/* Config Editor */}
                    {renderAgentConfigEditor()}

                    {/* Routing Toggle */}
                    <div style={{
                      padding: '12px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                    }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                        <Route size={16} style={{ color: FINCEPT.PURPLE }} />
                        <div>
                          <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>
                            SuperAgent Auto-Routing
                          </div>
                          <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                            Automatically route queries to the best agent
                          </div>
                        </div>
                      </div>
                      <button
                        onClick={() => setUseAutoRouting(!useAutoRouting)}
                        style={{
                          background: 'none',
                          border: 'none',
                          cursor: 'pointer',
                          color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.GRAY,
                        }}
                      >
                        {useAutoRouting ? <ToggleRight size={24} /> : <ToggleLeft size={24} />}
                      </button>
                    </div>

                    {/* Last Routing Result */}
                    {lastRoutingResult && (
                      <div style={{
                        padding: '12px',
                        backgroundColor: `${FINCEPT.PURPLE}10`,
                        border: `1px solid ${FINCEPT.PURPLE}40`,
                        borderRadius: '2px',
                      }}>
                        <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.PURPLE, marginBottom: '6px' }}>
                          LAST ROUTING RESULT
                        </div>
                        <div style={{ fontSize: '11px', color: FINCEPT.WHITE }}>
                          <span style={{ color: FINCEPT.GRAY }}>Intent:</span>{' '}
                          <span style={{ color: FINCEPT.ORANGE }}>{lastRoutingResult.intent}</span>
                          {' • '}
                          <span style={{ color: FINCEPT.GRAY }}>Agent:</span>{' '}
                          <span style={{ color: FINCEPT.ORANGE }}>{lastRoutingResult.agent_id}</span>
                          {' • '}
                          <span style={{ color: FINCEPT.GRAY }}>Confidence:</span>{' '}
                          <span style={{ color: FINCEPT.GREEN }}>{(lastRoutingResult.confidence * 100).toFixed(0)}%</span>
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            </div>
          </>
        ) : (
          <div style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{ textAlign: 'center', padding: '32px' }}>
              <Bot size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
              <div style={{ fontSize: '12px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                Select an agent to view details
              </div>
              {discoveredAgents.length === 0 && (
                <button
                  onClick={() => setViewMode('chat')}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.WHITE,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  GO TO CHAT
                </button>
              )}
            </div>
          </div>
        )}
      </div>

      {/* RIGHT PANEL - Query & Results (300px fixed) */}
      <div style={{
        width: '300px',
        minWidth: '300px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        {/* Panel Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
          }}>
            QUERY EXECUTION
          </span>
        </div>

        {/* Query Form */}
        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Output Model */}
          <div>
            <label style={{
              display: 'block',
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '6px',
            }}>
              OUTPUT MODEL
            </label>
            <select
              value={selectedOutputModel}
              onChange={e => setSelectedOutputModel(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
              }}
            >
              <option value="">None (free text)</option>
              {OUTPUT_MODELS.map(m => (
                <option key={m.id} value={m.id}>{m.name}</option>
              ))}
            </select>
          </div>

          {/* Query Input */}
          <div>
            <label style={{
              display: 'block',
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '6px',
            }}>
              QUERY
            </label>
            <textarea
              value={testQuery}
              onChange={e => setTestQuery(e.target.value)}
              placeholder="Enter your query..."
              rows={4}
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '11px',
                fontFamily: '"IBM Plex Mono", monospace',
                resize: 'none',
              }}
            />
          </div>

          {/* Run Button */}
          <button
            onClick={runAgent}
            disabled={loading}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: loading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              opacity: loading ? 0.7 : 1,
            }}
          >
            {loading ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
            {useAutoRouting ? 'RUN WITH AUTO-ROUTING' : 'RUN AGENT'}
          </button>
        </div>

        {/* Results Area */}
        <div style={{
          flex: 1,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
        }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
            }}>
              RESULT
            </span>
            {testResult && (
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${FINCEPT.GREEN}20`,
                color: FINCEPT.GREEN,
                fontSize: '8px',
                borderRadius: '2px',
              }}>
                SUCCESS
              </span>
            )}
          </div>
          <div style={{
            flex: 1,
            overflow: 'auto',
            padding: '12px',
          }}>
            {testResult ? (
              <pre style={{
                fontSize: '10px',
                color: FINCEPT.WHITE,
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-word',
                margin: 0,
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                {typeof testResult === 'string' ? testResult : JSON.stringify(testResult, null, 2)}
              </pre>
            ) : (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                color: FINCEPT.MUTED,
                fontSize: '10px',
                textAlign: 'center',
              }}>
                <Target size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>Run a query to see results</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Teams View
  // =============================================================================

  const renderTeamsView = () => (
    <div style={{
      display: 'flex',
      height: '100%',
      overflow: 'hidden',
    }}>
      {/* Left - Team Members */}
      <div style={{
        width: '280px',
        minWidth: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        {/* Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            TEAM ({teamMembers.length})
          </span>
          <select
            value={teamMode}
            onChange={e => setTeamMode(e.target.value as any)}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
            }}
          >
            <option value="coordinate">SEQUENTIAL</option>
            <option value="route">ROUTE</option>
            <option value="collaborate">PARALLEL</option>
          </select>
        </div>

        {/* Team Members List */}
        <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
          {teamMembers.length === 0 ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '32px 16px',
              textAlign: 'center',
            }}>
              <Users size={32} style={{ color: FINCEPT.MUTED, marginBottom: '12px', opacity: 0.5 }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>No agents in team</span>
              <span style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>Add from available list</span>
            </div>
          ) : (
            teamMembers.map((agent, idx) => (
              <div key={agent.id} style={{
                padding: '10px',
                marginBottom: '4px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{
                    width: '20px',
                    height: '20px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    fontSize: '10px',
                    fontWeight: 700,
                    borderRadius: '2px',
                  }}>
                    {idx + 1}
                  </span>
                  <div>
                    <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>{agent.name}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{agent.category}</div>
                  </div>
                </div>
                <button
                  onClick={() => removeFromTeam(agent.id)}
                  style={{
                    background: 'none',
                    border: 'none',
                    cursor: 'pointer',
                    color: FINCEPT.RED,
                    padding: '4px',
                  }}
                >
                  <Trash2 size={12} />
                </button>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Center - Available Agents */}
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        minWidth: 0,
      }}>
        {/* Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            AVAILABLE AGENTS ({discoveredAgents.filter(a => !teamMembers.find(m => m.id === a.id)).length})
          </span>
        </div>

        {/* Agent Grid */}
        <div style={{
          flex: 1,
          overflow: 'auto',
          padding: '16px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
          gap: '8px',
          alignContent: 'start',
        }}>
          {discoveredAgents
            .filter(a => !teamMembers.find(m => m.id === a.id))
            .map(agent => (
              <div
                key={agent.id}
                onClick={() => addToTeam(agent)}
                style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                }}
              >
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: '4px',
                }}>
                  <span style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>{agent.name}</span>
                  <Plus size={12} style={{ color: FINCEPT.ORANGE }} />
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{agent.category}</div>
              </div>
            ))}
        </div>
      </div>

      {/* Right - Query Panel */}
      <div style={{
        width: '300px',
        minWidth: '300px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        {/* Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            TEAM QUERY
          </span>
        </div>

        {/* Query Form */}
        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <textarea
            value={testQuery}
            onChange={e => setTestQuery(e.target.value)}
            rows={6}
            placeholder="Enter your query for the team..."
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '11px',
              fontFamily: '"IBM Plex Mono", monospace',
              resize: 'none',
            }}
          />

          <button
            onClick={runTeam}
            disabled={loading || teamMembers.length === 0}
            style={{
              width: '100%',
              padding: '10px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: (loading || teamMembers.length === 0) ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              opacity: (loading || teamMembers.length === 0) ? 0.7 : 1,
            }}
          >
            {loading ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
            RUN TEAM
          </button>
        </div>

        {/* Results */}
        <div style={{ flex: 1, borderTop: `1px solid ${FINCEPT.BORDER}`, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              RESULT
            </span>
          </div>
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {testResult ? (
              <pre style={{
                fontSize: '10px',
                color: FINCEPT.WHITE,
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-word',
                margin: 0,
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                {JSON.stringify(testResult, null, 2)}
              </pre>
            ) : (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                color: FINCEPT.MUTED,
                fontSize: '10px',
                textAlign: 'center',
              }}>
                <Users size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>Run team query to see results</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Workflows View - Terminal Style
  // =============================================================================

  const renderWorkflowsView = () => (
    <div style={{
      display: 'flex',
      height: '100%',
      overflow: 'hidden',
    }}>
      {/* Left - Workflow Cards */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
      }}>
        {/* Header */}
        <div style={{
          marginBottom: '16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
        }}>
          <Workflow size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>
            FINANCIAL WORKFLOWS
          </span>
        </div>

        {/* Workflow Grid */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
          gap: '12px',
        }}>
          {/* Stock Analysis */}
          <div style={{
            padding: '16px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
            }}>
              <TrendingUp size={16} style={{ color: FINCEPT.ORANGE }} />
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                STOCK ANALYSIS
              </span>
            </div>
            <input
              type="text"
              value={workflowSymbol}
              onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())}
              placeholder="Symbol (e.g., AAPL)"
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                marginBottom: '12px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            />
            <button
              onClick={() => runWorkflow('stock_analysis')}
              disabled={loading}
              style={{
                width: '100%',
                padding: '8px',
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: loading ? 'not-allowed' : 'pointer',
                opacity: loading ? 0.7 : 1,
              }}
            >
              ANALYZE
            </button>
          </div>

          {/* Portfolio Rebalancing */}
          <div style={{
            padding: '16px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
            }}>
              <Brain size={16} style={{ color: FINCEPT.PURPLE }} />
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                PORTFOLIO REBALANCING
              </span>
            </div>
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px', lineHeight: 1.4 }}>
              Optimize your portfolio allocation based on risk/return targets.
            </p>
            <button
              onClick={() => runWorkflow('portfolio_rebal')}
              disabled={loading}
              style={{
                width: '100%',
                padding: '8px',
                backgroundColor: FINCEPT.PURPLE,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: loading ? 'not-allowed' : 'pointer',
                opacity: loading ? 0.7 : 1,
              }}
            >
              REBALANCE
            </button>
          </div>

          {/* Risk Assessment */}
          <div style={{
            padding: '16px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
            }}>
              <AlertCircle size={16} style={{ color: FINCEPT.RED }} />
              <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                RISK ASSESSMENT
              </span>
            </div>
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px', lineHeight: 1.4 }}>
              Analyze portfolio risk metrics and potential vulnerabilities.
            </p>
            <button
              onClick={() => runWorkflow('risk_assessment')}
              disabled={loading}
              style={{
                width: '100%',
                padding: '8px',
                backgroundColor: FINCEPT.RED,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: loading ? 'not-allowed' : 'pointer',
                opacity: loading ? 0.7 : 1,
              }}
            >
              ASSESS RISK
            </button>
          </div>
        </div>
      </div>

      {/* Right - Results Panel */}
      <div style={{
        width: '350px',
        minWidth: '350px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            WORKFLOW RESULT
          </span>
        </div>
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          {testResult ? (
            <pre style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
              margin: 0,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {JSON.stringify(testResult, null, 2)}
            </pre>
          ) : (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: FINCEPT.MUTED,
              fontSize: '10px',
              textAlign: 'center',
            }}>
              <Workflow size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <span>Run a workflow to see results</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Planner View - Terminal Style
  // =============================================================================

  const renderPlannerView = () => (
    <div style={{
      display: 'flex',
      height: '100%',
      overflow: 'hidden',
    }}>
      {/* Left - Plan Creation & Templates */}
      <div style={{
        width: '320px',
        minWidth: '320px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        {/* Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
        }}>
          <ListTree size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
            EXECUTION PLANNER
          </span>
        </div>

        {/* Create Plan Form */}
        <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px' }}>
            CREATE PLAN
          </label>
          <input
            type="text"
            value={workflowSymbol}
            onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())}
            placeholder="Symbol (e.g., AAPL)"
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              marginBottom: '8px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}
          />
          <button
            onClick={createPlan}
            disabled={loading}
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: loading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              opacity: loading ? 0.7 : 1,
            }}
          >
            {loading ? <Loader2 size={10} className="animate-spin" /> : <Target size={10} />}
            CREATE PLAN
          </button>
        </div>

        {/* Plan Templates */}
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px' }}>
            TEMPLATES
          </label>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {[
              { id: 'stock_analysis', name: 'Stock Analysis', desc: 'Fundamental + technical' },
              { id: 'portfolio_rebalance', name: 'Portfolio Rebalance', desc: 'Optimize allocation' },
              { id: 'sector_rotation', name: 'Sector Rotation', desc: 'Identify opportunities' },
              { id: 'risk_audit', name: 'Risk Audit', desc: 'Risk assessment' },
            ].map(template => (
              <button
                key={template.id}
                onClick={() => { setWorkflowSymbol('AAPL'); createPlan(); }}
                style={{
                  padding: '10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  textAlign: 'left',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              >
                <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '2px' }}>
                  {template.name}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{template.desc}</div>
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Center - Plan Visualization */}
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        minWidth: 0,
      }}>
        {currentPlan ? (
          <>
            {/* Plan Header */}
            <div style={{
              padding: '12px 16px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <div>
                <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{currentPlan.name}</div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{currentPlan.description}</div>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{
                  padding: '4px 8px',
                  backgroundColor: currentPlan.is_complete ? `${FINCEPT.GREEN}20` : currentPlan.has_failed ? `${FINCEPT.RED}20` : `${FINCEPT.ORANGE}20`,
                  color: currentPlan.is_complete ? FINCEPT.GREEN : currentPlan.has_failed ? FINCEPT.RED : FINCEPT.ORANGE,
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                }}>
                  {currentPlan.status}
                </span>
                <button
                  onClick={executePlan}
                  disabled={planExecuting || currentPlan.is_complete}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: currentPlan.is_complete ? FINCEPT.GREEN : FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: (planExecuting || currentPlan.is_complete) ? 'not-allowed' : 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    opacity: planExecuting ? 0.7 : 1,
                  }}
                >
                  {planExecuting ? <Loader2 size={10} className="animate-spin" /> : <Play size={10} />}
                  {currentPlan.is_complete ? 'DONE' : 'EXECUTE'}
                </button>
              </div>
            </div>

            {/* Steps */}
            <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {currentPlan.steps.map((step, idx) => (
                  <div
                    key={step.id}
                    style={{
                      padding: '12px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderLeft: `3px solid ${
                        step.status === 'completed' ? FINCEPT.GREEN :
                        step.status === 'failed' ? FINCEPT.RED :
                        step.status === 'running' ? FINCEPT.ORANGE : FINCEPT.GRAY
                      }`,
                      borderRadius: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '12px',
                    }}
                  >
                    <span style={{
                      width: '24px',
                      height: '24px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      backgroundColor: FINCEPT.DARK_BG,
                      color: FINCEPT.GRAY,
                      fontSize: '10px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}>
                      {idx + 1}
                    </span>
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>{step.name}</div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {step.step_type}{step.dependencies.length > 0 && ` • Deps: ${step.dependencies.join(', ')}`}
                      </div>
                    </div>
                    <span style={{
                      padding: '4px 8px',
                      backgroundColor: step.status === 'completed' ? `${FINCEPT.GREEN}20` :
                        step.status === 'failed' ? `${FINCEPT.RED}20` :
                        step.status === 'running' ? `${FINCEPT.ORANGE}20` : FINCEPT.BORDER,
                      color: step.status === 'completed' ? FINCEPT.GREEN :
                        step.status === 'failed' ? FINCEPT.RED :
                        step.status === 'running' ? FINCEPT.ORANGE : FINCEPT.GRAY,
                      fontSize: '8px',
                      fontWeight: 700,
                      borderRadius: '2px',
                      textTransform: 'uppercase',
                    }}>
                      {step.status}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          </>
        ) : (
          <div style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{ textAlign: 'center', color: FINCEPT.MUTED }}>
              <ListTree size={40} style={{ marginBottom: '12px', opacity: 0.5 }} />
              <div style={{ fontSize: '11px' }}>Create a plan to visualize steps</div>
            </div>
          </div>
        )}
      </div>

      {/* Right - Results Panel */}
      {currentPlan?.is_complete && currentPlan.context && (
        <div style={{
          width: '300px',
          minWidth: '300px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          flexShrink: 0,
        }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              PLAN RESULTS
            </span>
          </div>
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            <pre style={{
              fontSize: '9px',
              color: FINCEPT.WHITE,
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
              margin: 0,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {JSON.stringify(currentPlan.context, null, 2)}
            </pre>
          </div>
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Tools View - Terminal Style
  // =============================================================================

  const renderToolsView = () => (
    <div style={{
      display: 'flex',
      height: '100%',
      overflow: 'hidden',
    }}>
      {/* Left - Selected Tools */}
      <div style={{
        width: '280px',
        minWidth: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            SELECTED ({selectedTools.length})
          </span>
          {selectedTools.length > 0 && (
            <button
              onClick={() => setSelectedTools([])}
              style={{
                padding: '2px 6px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
                fontSize: '8px',
                cursor: 'pointer',
                borderRadius: '2px',
              }}
            >
              CLEAR
            </button>
          )}
        </div>
        <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
          {selectedTools.length === 0 ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: FINCEPT.MUTED,
              fontSize: '10px',
              textAlign: 'center',
            }}>
              <Wrench size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <span>No tools selected</span>
            </div>
          ) : (
            selectedTools.map(tool => (
              <div
                key={tool}
                style={{
                  padding: '8px 10px',
                  marginBottom: '4px',
                  backgroundColor: `${FINCEPT.ORANGE}15`,
                  border: `1px solid ${FINCEPT.ORANGE}40`,
                  borderRadius: '2px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                }}
              >
                <span style={{ fontSize: '10px', color: FINCEPT.ORANGE }}>{tool}</span>
                <button
                  onClick={() => toggleToolSelection(tool)}
                  style={{
                    background: 'none',
                    border: 'none',
                    color: FINCEPT.RED,
                    cursor: 'pointer',
                    fontSize: '12px',
                  }}
                >
                  ×
                </button>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Center - Tool Browser */}
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        minWidth: 0,
      }}>
        {/* Header with Search */}
        <div style={{
          padding: '12px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Wrench size={14} style={{ color: FINCEPT.ORANGE }} />
            <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
              AVAILABLE TOOLS
            </span>
            {toolsInfo && (
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${FINCEPT.CYAN}20`,
                color: FINCEPT.CYAN,
                fontSize: '8px',
                borderRadius: '2px',
              }}>
                {toolsInfo.total_count}
              </span>
            )}
          </div>
          <div style={{ flex: 1, position: 'relative' }}>
            <Search size={12} style={{
              position: 'absolute',
              left: '10px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: FINCEPT.GRAY,
            }} />
            <input
              type="text"
              value={toolSearch}
              onChange={e => setToolSearch(e.target.value)}
              placeholder="Search tools..."
              style={{
                width: '100%',
                padding: '6px 10px 6px 30px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
              }}
            />
          </div>
        </div>

        {/* Categories */}
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          {toolsInfo?.categories?.map(category => {
            const tools = toolsInfo.tools[category] || [];
            const filteredTools = toolSearch
              ? tools.filter(t => t.toLowerCase().includes(toolSearch.toLowerCase()))
              : tools;

            if (filteredTools.length === 0) return null;

            return (
              <div key={category} style={{
                marginBottom: '8px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
              }}>
                <button
                  onClick={() => toggleToolCategory(category)}
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                  }}
                >
                  <span style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE }}>{category}</span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{filteredTools.length}</span>
                    {expandedCategories.has(category) ? <ChevronDown size={12} color={FINCEPT.GRAY} /> : <ChevronRight size={12} color={FINCEPT.GRAY} />}
                  </div>
                </button>
                {expandedCategories.has(category) && (
                  <div style={{
                    padding: '8px 12px 12px',
                    borderTop: `1px solid ${FINCEPT.BORDER}`,
                    display: 'flex',
                    flexWrap: 'wrap',
                    gap: '6px',
                  }}>
                    {filteredTools.map(tool => (
                      <span
                        key={tool}
                        onClick={() => toggleToolSelection(tool)}
                        style={{
                          padding: '4px 8px',
                          backgroundColor: selectedTools.includes(tool) ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                          color: selectedTools.includes(tool) ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                          border: `1px solid ${selectedTools.includes(tool) ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '9px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '6px',
                        }}
                      >
                        {tool}
                        <button
                          onClick={e => { e.stopPropagation(); copyTool(tool); }}
                          style={{
                            background: 'none',
                            border: 'none',
                            cursor: 'pointer',
                            color: selectedTools.includes(tool) ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            padding: 0,
                          }}
                        >
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
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: FINCEPT.PANEL_BG,
    }}>
      {/* Chat Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <MessageSquare size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
            AGENT CHAT
          </span>
          {selectedAgent && (
            <span style={{
              padding: '2px 6px',
              backgroundColor: `${FINCEPT.CYAN}20`,
              color: FINCEPT.CYAN,
              fontSize: '8px',
              borderRadius: '2px',
            }}>
              {selectedAgent.name}
            </span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>AUTO-ROUTE</span>
            <button
              onClick={() => setUseAutoRouting(!useAutoRouting)}
              style={{
                background: 'none',
                border: 'none',
                cursor: 'pointer',
                color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.GRAY,
              }}
            >
              {useAutoRouting ? <ToggleRight size={18} /> : <ToggleLeft size={18} />}
            </button>
          </div>
        </div>
      </div>

      {/* Messages Area */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
        display: 'flex',
        flexDirection: 'column',
        gap: '12px',
      }}>
        {chatMessages.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.GRAY,
            textAlign: 'center',
          }}>
            <Sparkles size={40} style={{ marginBottom: '16px', opacity: 0.5 }} />
            <span style={{ fontSize: '12px', marginBottom: '8px' }}>Start a conversation</span>
            <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
              {useAutoRouting ? 'Queries will be auto-routed to the best agent' : `Using: ${selectedAgent?.name || 'No agent selected'}`}
            </span>
          </div>
        ) : (
          chatMessages.map((msg, i) => (
            <div key={i} style={{
              display: 'flex',
              justifyContent: msg.role === 'user' ? 'flex-end' : 'flex-start',
            }}>
              <div style={{
                maxWidth: '80%',
                padding: '12px',
                borderRadius: '4px',
                backgroundColor: msg.role === 'user' ? FINCEPT.ORANGE :
                  msg.role === 'system' ? `${FINCEPT.PURPLE}20` : FINCEPT.DARK_BG,
                color: msg.role === 'user' ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                border: msg.role === 'system' ? `1px solid ${FINCEPT.PURPLE}40` :
                  msg.role === 'user' ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              }}>
                {msg.agentName && (
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    marginBottom: '6px',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.CYAN,
                  }}>
                    <Bot size={10} />
                    {msg.agentName.toUpperCase()}
                  </div>
                )}
                <div style={{
                  fontSize: '11px',
                  whiteSpace: 'pre-wrap',
                  lineHeight: 1.5,
                }}>
                  {msg.content}
                </div>
                <div style={{
                  fontSize: '9px',
                  marginTop: '8px',
                  opacity: 0.6,
                  color: msg.role === 'user' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                }}>
                  {msg.timestamp.toLocaleTimeString()}
                </div>
              </div>
            </div>
          ))
        )}
        <div ref={chatEndRef} />
      </div>

      {/* Input Area */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ display: 'flex', gap: '8px' }}>
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
            style={{
              flex: 1,
              padding: '10px 12px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '11px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}
          />
          <button
            onClick={handleChatSubmit}
            disabled={loading || !chatInput.trim()}
            style={{
              padding: '10px 16px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: (loading || !chatInput.trim()) ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              opacity: (loading || !chatInput.trim()) ? 0.7 : 1,
            }}
          >
            {loading ? <Loader2 size={12} className="animate-spin" /> : <Zap size={12} />}
            SEND
          </button>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Render: System View
  // =============================================================================

  const renderSystemView = () => (
    <div style={{
      display: 'flex',
      height: '100%',
      overflow: 'hidden',
    }}>
      {/* Main Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
      }}>
        {/* Header */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '16px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Cpu size={16} style={{ color: FINCEPT.ORANGE }} />
            <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>
              SYSTEM CAPABILITIES
            </span>
          </div>
          <button
            onClick={() => loadSystemData()}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              borderRadius: '2px',
            }}
          >
            <RefreshCw size={10} /> REFRESH
          </button>
        </div>

        {/* Stats Grid */}
        {systemInfo ? (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(4, 1fr)',
            gap: '12px',
            marginBottom: '16px',
          }}>
            {[
              { value: systemInfo.capabilities.model_providers, label: 'PROVIDERS', color: FINCEPT.ORANGE },
              { value: systemInfo.capabilities.tools, label: 'TOOLS', color: FINCEPT.CYAN },
              { value: discoveredAgents.length, label: 'AGENTS', color: FINCEPT.GREEN },
              { value: configuredLLMs.length, label: 'LLMs', color: FINCEPT.PURPLE },
            ].map((stat, idx) => (
              <div key={idx} style={{
                padding: '16px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                textAlign: 'center',
              }}>
                <div style={{ fontSize: '24px', fontWeight: 700, color: stat.color }}>{stat.value}</div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px' }}>{stat.label}</div>
              </div>
            ))}
          </div>
        ) : (
          <div style={{
            padding: '32px',
            textAlign: 'center',
            color: FINCEPT.GRAY,
            fontSize: '11px',
          }}>
            Loading system info...
          </div>
        )}

        {/* LLM Providers Section */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: '12px',
        }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY }}>
              CONFIGURED LLM PROVIDERS
            </span>
            <button
              onClick={() => loadLLMConfigs()}
              style={{
                padding: '2px 6px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                fontSize: '8px',
                cursor: 'pointer',
                borderRadius: '2px',
              }}
            >
              REFRESH
            </button>
          </div>
          <div style={{ padding: '12px' }}>
            {configuredLLMs.length > 0 ? (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {configuredLLMs.map((llm, idx) => {
                  const hasKey = !!llm.api_key && llm.api_key.length > 0;
                  return (
                    <div key={idx} style={{
                      padding: '10px 12px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                    }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        {llm.is_active && (
                          <span style={{
                            padding: '2px 6px',
                            backgroundColor: `${FINCEPT.GREEN}20`,
                            color: FINCEPT.GREEN,
                            fontSize: '8px',
                            fontWeight: 700,
                            borderRadius: '2px',
                          }}>
                            ACTIVE
                          </span>
                        )}
                        <span style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE }}>
                          {llm.provider.toUpperCase()}
                        </span>
                        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{llm.model}</span>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                        {hasKey ? (
                          <>
                            <CheckCircle size={10} style={{ color: FINCEPT.GREEN }} />
                            <span style={{ fontSize: '9px', color: FINCEPT.GREEN }}>KEY SET</span>
                          </>
                        ) : (
                          <>
                            <AlertCircle size={10} style={{ color: FINCEPT.RED }} />
                            <span style={{ fontSize: '9px', color: FINCEPT.RED }}>NO KEY</span>
                          </>
                        )}
                      </div>
                    </div>
                  );
                })}
              </div>
            ) : (
              <div style={{
                padding: '24px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
              }}>
                <AlertCircle size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <div style={{ fontSize: '10px' }}>No LLM providers configured</div>
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                  Go to Settings → LLM Configuration
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Cache Stats */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: '12px',
        }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY }}>CACHE STATISTICS</span>
            <button
              onClick={() => agentService.clearCache()}
              style={{
                padding: '2px 6px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
                fontSize: '8px',
                cursor: 'pointer',
                borderRadius: '2px',
              }}
            >
              CLEAR
            </button>
          </div>
          <div style={{
            padding: '12px',
            display: 'grid',
            gridTemplateColumns: 'repeat(3, 1fr)',
            gap: '12px',
          }}>
            {[
              { label: 'STATIC', value: agentService.getCacheStats().cacheSize },
              { label: 'RESPONSE', value: agentService.getCacheStats().responseCacheSize },
              { label: 'MAX SIZE', value: agentService.getCacheStats().maxSize },
            ].map((stat, idx) => (
              <div key={idx} style={{
                padding: '12px',
                backgroundColor: FINCEPT.DARK_BG,
                borderRadius: '2px',
                textAlign: 'center',
              }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN }}>{stat.value}</div>
                <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '4px' }}>{stat.label}</div>
              </div>
            ))}
          </div>
        </div>

        {/* Features */}
        {systemInfo?.features && (
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            marginBottom: '12px',
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY }}>FEATURES</span>
            </div>
            <div style={{ padding: '12px', display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {systemInfo.features.map(feature => (
                <span key={feature} style={{
                  padding: '4px 8px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  fontSize: '9px',
                  borderRadius: '2px',
                }}>
                  {feature.replace(/_/g, ' ').toUpperCase()}
                </span>
              ))}
            </div>
          </div>
        )}

        {/* Version Info */}
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY }}>VERSION INFO</span>
          </div>
          <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>VERSION</div>
            <div style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{systemInfo?.version || 'N/A'}</div>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>FRAMEWORK</div>
            <div style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{systemInfo?.framework || 'N/A'}</div>
          </div>
        </div>
      </div>
    </div>
  );

  // =============================================================================
  // Main Render
  // =============================================================================

  // Show initial loading screen
  if (loading && discoveredAgents.length === 0 && !systemInfo && !toolsInfo) {
    return (
      <div
        className="agent-terminal flex flex-col h-full items-center justify-center"
        style={{
          backgroundColor: FINCEPT.DARK_BG,
          fontFamily: '"IBM Plex Mono", "Consolas", monospace',
        }}
      >
        <style>{TERMINAL_STYLES}</style>
        <div style={{
          padding: '48px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '4px',
          textAlign: 'center',
        }}>
          <Bot size={48} className="mx-auto mb-4" style={{ color: FINCEPT.ORANGE }} />
          <Loader2 size={32} className="animate-spin mx-auto mb-4" style={{ color: FINCEPT.ORANGE }} />
          <h2 style={{
            fontSize: '18px',
            fontWeight: 700,
            marginBottom: '8px',
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
          }}>
            INITIALIZING AGENT STUDIO
          </h2>
          <p style={{ fontSize: '11px', marginBottom: '16px', color: FINCEPT.GRAY }}>
            Loading agents and system capabilities...
          </p>
          <button
            onClick={() => {
              setLoading(false);
              setError('Agent loading was skipped. Some features may not be available.');
            }}
            style={{
              padding: '8px 16px',
              backgroundColor: FINCEPT.BORDER,
              color: FINCEPT.WHITE,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              letterSpacing: '0.5px',
            }}
          >
            SKIP LOADING
          </button>
        </div>
      </div>
    );
  }

  return (
    <div
      className="agent-terminal flex flex-col h-full overflow-hidden"
      style={{
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
    >
      <style>{TERMINAL_STYLES}</style>

      {/* Top Navigation Bar - Terminal Style with Orange Accent */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        {/* Left: Logo and Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Bot size={20} style={{ color: FINCEPT.ORANGE }} />
          <span style={{
            fontSize: '14px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
          }}>
            AGENT STUDIO
          </span>
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${FINCEPT.PURPLE}20`,
            color: FINCEPT.PURPLE,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
          }}>
            v2.1
          </span>
          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER, margin: '0 4px' }} />

          {/* Status Indicators */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                backgroundColor: discoveredAgents.length > 0 ? FINCEPT.GREEN : FINCEPT.RED,
              }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                {discoveredAgents.length} AGENTS
              </span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                backgroundColor: configuredLLMs.length > 0 ? FINCEPT.GREEN : FINCEPT.YELLOW,
                animation: configuredLLMs.length === 0 ? 'pulse 2s infinite' : 'none',
              }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                {configuredLLMs.length} LLMs
              </span>
            </div>
          </div>
        </div>

        {/* Center: View Mode Tabs */}
        <div style={{ display: 'flex', gap: '2px' }}>
          {VIEW_MODES.map(mode => (
            <button
              key={mode.id}
              onClick={() => setViewMode(mode.id)}
              style={{
                padding: '6px 12px',
                backgroundColor: viewMode === mode.id ? FINCEPT.ORANGE : 'transparent',
                color: viewMode === mode.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => {
                if (viewMode !== mode.id) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }
              }}
              onMouseLeave={(e) => {
                if (viewMode !== mode.id) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              <mode.icon size={12} />
              {mode.name}
            </button>
          ))}
        </div>

        {/* Right: Actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={loadDiscoveredAgents}
            disabled={loading}
            style={{
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: loading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '9px',
              borderRadius: '2px',
              opacity: loading ? 0.5 : 1,
            }}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
            REFRESH
          </button>
          <button
            onClick={() => setUseAutoRouting(!useAutoRouting)}
            style={{
              padding: '4px 8px',
              backgroundColor: useAutoRouting ? `${FINCEPT.GREEN}20` : 'transparent',
              border: `1px solid ${useAutoRouting ? FINCEPT.GREEN : FINCEPT.BORDER}`,
              color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '9px',
              borderRadius: '2px',
            }}
          >
            <Route size={12} />
            AUTO-ROUTE
          </button>
        </div>
      </div>

      {/* Notifications */}
      {error && (
        <div style={{
          margin: '8px 16px',
          padding: '12px',
          backgroundColor: `${FINCEPT.RED}15`,
          border: `1px solid ${FINCEPT.RED}40`,
          borderRadius: '2px',
        }}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <AlertCircle size={14} style={{ color: FINCEPT.RED, marginTop: '1px', flexShrink: 0 }} />
            <div style={{ flex: 1 }}>
              <div style={{ fontSize: '10px', fontWeight: 700, marginBottom: '4px', color: FINCEPT.RED }}>ERROR</div>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
              {error.includes('timeout') && (
                <div style={{ fontSize: '9px', marginTop: '4px', color: FINCEPT.MUTED }}>
                  Backend may be slow. You can continue with manual configuration.
                </div>
              )}
            </div>
            <button
              onClick={() => setError(null)}
              style={{
                background: 'none',
                border: 'none',
                color: FINCEPT.RED,
                cursor: 'pointer',
                fontSize: '14px',
                lineHeight: 1,
              }}
            >
              ×
            </button>
          </div>
        </div>
      )}
      {success && (
        <div style={{
          margin: '8px 16px',
          padding: '12px',
          backgroundColor: `${FINCEPT.GREEN}15`,
          border: `1px solid ${FINCEPT.GREEN}40`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
        }}>
          <CheckCircle size={14} style={{ color: FINCEPT.GREEN }} />
          <span style={{ fontSize: '10px', color: FINCEPT.GREEN }}>{success}</span>
        </div>
      )}

      {/* Main Content Area */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {viewMode === 'agents' && renderAgentsView()}
        {viewMode === 'teams' && renderTeamsView()}
        {viewMode === 'workflows' && renderWorkflowsView()}
        {viewMode === 'planner' && renderPlannerView()}
        {viewMode === 'tools' && renderToolsView()}
        {viewMode === 'chat' && renderChatView()}
        {viewMode === 'system' && renderSystemView()}
      </div>

      {/* Status Bar at Bottom */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>
            MODE: <span style={{ color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.ORANGE }}>
              {useAutoRouting ? 'AUTO-ROUTING' : 'MANUAL'}
            </span>
          </span>
          <span>
            VIEW: <span style={{ color: FINCEPT.CYAN }}>{viewMode.toUpperCase()}</span>
          </span>
          {selectedAgent && (
            <span>
              AGENT: <span style={{ color: FINCEPT.ORANGE }}>{selectedAgent.name}</span>
            </span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>
            LLM: <span style={{ color: FINCEPT.PURPLE }}>
              {editedConfig.provider}/{editedConfig.model_id}
            </span>
          </span>
          <span style={{ color: FINCEPT.MUTED }}>AGENT STUDIO v2.1</span>
        </div>
      </div>
    </div>
  );
};

export default AgentConfigTab;
