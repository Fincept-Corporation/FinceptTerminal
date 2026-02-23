/**
 * useAgentConfig - All state and logic for the Agent Studio tab
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { withTimeout } from '@/services/core/apiUtils';
import { validateSymbol, sanitizeSymbol, validateString } from '@/services/core/validators';
import { useTabSession } from '@/hooks/useTabSession';
import { cacheService, TTL } from '@/services/cache/cacheService';
import agentService, {
  type SystemInfo,
  type ToolsInfo,
} from '@/services/agentService';
import {
  getLLMConfigs,
  getActiveLLMConfig,
  getAgentConfigs,
  getSetting,
  LLM_PROVIDERS as DB_LLM_PROVIDERS,
  type AgentConfigData,
} from '@/services/core/sqliteService';
import { portfolioService, type Portfolio } from '@/services/portfolio/portfolioService';
import type {
  ViewMode,
  ChatMessage,
  EditedConfig,
  AgentTabSessionState,
  AgentCard,
  ExecutionPlan,
  RoutingResult,
  DbLLMConfig,
} from '../types';
import {
  AGENT_CACHE_KEYS,
  DEFAULT_EDITED_CONFIG,
  DEFAULT_TAB_SESSION,
  extractAgentResponseText,
} from '../types';

const AGENT_CACHE_TTL = TTL['10m'];
const SYSTEM_CACHE_TTL = TTL['30m'];

export function useAgentConfig() {
  // ─── Tab Session ──────────────────────────────────────────────────────────
  const { state: tabSession, updateState: updateTabSession, isRestored: isSessionRestored } =
    useTabSession<AgentTabSessionState>({
      tabId: 'agent-config',
      tabName: 'Agent Studio',
      defaultState: DEFAULT_TAB_SESSION,
      debounceMs: 800,
    });

  // ─── View ──────────────────────────────────────────────────────────────────
  const [viewMode, setViewModeInternal] = useState<ViewMode>(tabSession.viewMode);
  const setViewMode = useCallback((mode: ViewMode) => {
    setViewModeInternal(mode);
    updateTabSession({ viewMode: mode });
  }, [updateTabSession]);

  // ─── Agents ───────────────────────────────────────────────────────────────
  const [discoveredAgents, setDiscoveredAgents] = useState<AgentCard[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<AgentCard | null>(null);
  const [agentsByCategory, setAgentsByCategory] = useState<Record<string, AgentCard[]>>({});
  const [selectedCategory, setSelectedCategoryInternal] = useState<string>(tabSession.selectedCategory);
  const setSelectedCategory = useCallback((cat: string) => {
    setSelectedCategoryInternal(cat);
    updateTabSession({ selectedCategory: cat });
  }, [updateTabSession]);

  // ─── Status ───────────────────────────────────────────────────────────────
  const [loading, setLoading] = useState(false);
  const [executing, setExecuting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  // ─── Config Editor ────────────────────────────────────────────────────────
  const [editMode, setEditMode] = useState(false);
  const [editedConfig, setEditedConfig] = useState<EditedConfig>(DEFAULT_EDITED_CONFIG);

  // ─── System ───────────────────────────────────────────────────────────────
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [toolsInfo, setToolsInfo] = useState<ToolsInfo | null>(null);
  const [configuredLLMs, setConfiguredLLMs] = useState<DbLLMConfig[]>([]);
  const [llmProvidersList] = useState(DB_LLM_PROVIDERS);

  // ─── Query & Execution ────────────────────────────────────────────────────
  const [testQuery, setTestQueryInternal] = useState(tabSession.testQuery);
  const setTestQuery = useCallback((q: string) => {
    setTestQueryInternal(q);
    updateTabSession({ testQuery: q });
  }, [updateTabSession]);
  const [testResult, setTestResult] = useState<any>(null);
  const [selectedOutputModel, setSelectedOutputModelInternal] = useState(tabSession.selectedOutputModel);
  const setSelectedOutputModel = useCallback((m: string) => {
    setSelectedOutputModelInternal(m);
    updateTabSession({ selectedOutputModel: m });
  }, [updateTabSession]);
  const [showJsonEditor, setShowJsonEditor] = useState(false);
  const [jsonText, setJsonText] = useState('');

  // ─── Routing ──────────────────────────────────────────────────────────────
  const [useAutoRouting, setUseAutoRoutingInternal] = useState(tabSession.useAutoRouting);
  const setUseAutoRouting = useCallback((v: boolean) => {
    setUseAutoRoutingInternal(v);
    updateTabSession({ useAutoRouting: v });
  }, [updateTabSession]);
  const [lastRoutingResult, setLastRoutingResult] = useState<RoutingResult | null>(null);

  // ─── Team ─────────────────────────────────────────────────────────────────
  const [teamMembers, setTeamMembers] = useState<AgentCard[]>([]);
  const [teamMode, setTeamModeInternal] = useState<'coordinate' | 'route' | 'collaborate'>(tabSession.teamMode);
  const setTeamMode = useCallback((m: 'coordinate' | 'route' | 'collaborate') => {
    setTeamModeInternal(m);
    updateTabSession({ teamMode: m });
  }, [updateTabSession]);
  const [showMembersResponses, setShowMembersResponses] = useState(false);
  const [teamLeaderIndex, setTeamLeaderIndex] = useState<number>(-1);
  // Team-level LLM override — set by the Agent Configuration panel in TeamsView
  const [teamProvider, setTeamProvider] = useState<string>('');
  const [teamModelId, setTeamModelId] = useState<string>('');

  // ─── Chat ─────────────────────────────────────────────────────────────────
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>([]);
  const [chatInput, setChatInput] = useState('');
  const chatEndRef = useRef<HTMLDivElement>(null);

  // ─── Tools ────────────────────────────────────────────────────────────────
  const [toolSearch, setToolSearch] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set(['finance']));
  const [copiedTool, setCopiedTool] = useState<string | null>(null);

  // ─── Portfolio Context ────────────────────────────────────────────────────
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioCtx, setSelectedPortfolioCtx] = useState<Portfolio | null>(null);
  const [showPortfolioDropdown, setShowPortfolioDropdown] = useState(false);

  // ─── Team Execution Log ───────────────────────────────────────────────────
  const [teamLog, setTeamLog] = useState<{ label: string; text: string }[]>([]);

  // ─── Workflow / Planner ───────────────────────────────────────────────────
  const [workflowSymbol, setWorkflowSymbolInternal] = useState(tabSession.workflowSymbol);
  const setWorkflowSymbol = useCallback((s: string) => {
    setWorkflowSymbolInternal(s);
    updateTabSession({ workflowSymbol: s });
  }, [updateTabSession]);
  const [currentPlan, setCurrentPlan] = useState<ExecutionPlan | null>(null);
  const [planExecuting, setPlanExecuting] = useState(false);
  const [planGeneratedBy, setPlanGeneratedBy] = useState<'llm' | 'fallback' | 'template' | null>(null);

  // ─── Cleanup Refs ─────────────────────────────────────────────────────────
  const mountedRef = useRef(true);
  const abortControllerRef = useRef<AbortController | null>(null);
  const fetchIdRef = useRef(0);

  // ─── Session Restore ──────────────────────────────────────────────────────
  useEffect(() => {
    if (isSessionRestored) {
      setViewModeInternal(tabSession.viewMode);
      setSelectedCategoryInternal(tabSession.selectedCategory);
      setTestQueryInternal(tabSession.testQuery);
      setUseAutoRoutingInternal(tabSession.useAutoRouting);
      setTeamModeInternal(tabSession.teamMode);
      setWorkflowSymbolInternal(tabSession.workflowSymbol);
      setSelectedOutputModelInternal(tabSession.selectedOutputModel);
    }
  }, [isSessionRestored]);

  // ─── Cleanup on Unmount ───────────────────────────────────────────────────
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      abortControllerRef.current?.abort();
    };
  }, []);

  // ─── Load Portfolios ──────────────────────────────────────────────────────
  useEffect(() => {
    portfolioService.getPortfolios().then(list => {
      if (mountedRef.current) setPortfolios(list);
    }).catch(() => {});
  }, []);

  // ─── Scroll Chat to Bottom ────────────────────────────────────────────────
  useEffect(() => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [chatMessages]);

  // ─── Initial Load ─────────────────────────────────────────────────────────
  useEffect(() => {
    const loadInitialData = async () => {
      const currentFetchId = ++fetchIdRef.current;
      setLoading(true);
      setError(null);

      try {
        const criticalPromise = Promise.all([loadDiscoveredAgents(), loadLLMConfigs()]);
        const timeoutPromise = new Promise((_, reject) =>
          setTimeout(() => reject(new Error('Loading timeout - please check your backend service')), 30000)
        );
        await Promise.race([criticalPromise, timeoutPromise]);
      } catch (err: any) {
        if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
        console.error('Failed to load initial data:', err);
        setError(err.message || 'Failed to load agent data');
      } finally {
        if (mountedRef.current && currentFetchId === fetchIdRef.current) {
          setLoading(false);
        }
      }

      loadSystemData().catch(err => {
        console.warn('System data loading failed (non-critical):', err);
      });

      // Merge custom agents from SQLite (non-critical, always runs after main load)
      loadCustomAgentsFromDb().catch(() => {});
    };

    loadInitialData();
  }, []);

  // ─── Data Loaders ─────────────────────────────────────────────────────────

  const loadLLMConfigs = async () => {
    try {
      const configs = await getLLMConfigs();
      setConfiguredLLMs(configs);

      const finceptLLM = configs.find(c => c.provider === 'fincept');
      if (finceptLLM) {
        setEditedConfig(prev => ({ ...prev, provider: finceptLLM.provider, model_id: finceptLLM.model }));
      } else {
        const activeConfig = await getActiveLLMConfig();
        if (activeConfig) {
          setEditedConfig(prev => ({ ...prev, provider: activeConfig.provider, model_id: activeConfig.model }));
        } else if (configs.length > 0) {
          setEditedConfig(prev => ({ ...prev, provider: configs[0].provider, model_id: configs[0].model }));
        }
      }
    } catch (err) {
      console.error('Failed to load LLM configs:', err);
    }
  };

  const loadSystemData = async () => {
    try {
      const [cachedSysInfo, cachedTools] = await Promise.all([
        cacheService.get<SystemInfo>(AGENT_CACHE_KEYS.SYSTEM_INFO),
        cacheService.get<ToolsInfo>(AGENT_CACHE_KEYS.TOOLS_INFO),
      ]);
      if (cachedSysInfo?.data && cachedTools?.data) {
        setSystemInfo(cachedSysInfo.data);
        setToolsInfo(cachedTools.data);
        return;
      }
    } catch (cacheErr) {
      console.warn('[useAgentConfig] System data cache read failed:', cacheErr);
    }

    try {
      const timeoutPromise = new Promise<never>((_, reject) =>
        setTimeout(() => reject(new Error('System data loading timeout')), 30000)
      );
      const dataPromise = Promise.all([agentService.getSystemInfo(), agentService.getTools()]);
      const [sysInfo, tools] = await Promise.race([dataPromise, timeoutPromise]);

      if (sysInfo) {
        setSystemInfo(sysInfo);
        await cacheService.set(AGENT_CACHE_KEYS.SYSTEM_INFO, sysInfo, 'api-response', SYSTEM_CACHE_TTL).catch(() => {});
      }
      if (tools) {
        setToolsInfo(tools);
        await cacheService.set(AGENT_CACHE_KEYS.TOOLS_INFO, tools, 'api-response', SYSTEM_CACHE_TTL).catch(() => {});
      }
    } catch (err) {
      console.error('Failed to load system data:', err);
      setSystemInfo(null);
      setToolsInfo(null);
    }
  };

  const loadDiscoveredAgents = async () => {
    try {
      const cached = await cacheService.get<AgentCard[]>(AGENT_CACHE_KEYS.DISCOVERED_AGENTS);
      if (cached && Array.isArray(cached.data) && cached.data.length > 0) {
        setAgentsFromList(cached.data);
        return;
      }
    } catch (cacheErr) {
      console.warn('[useAgentConfig] Cache read failed:', cacheErr);
    }

    try {
      const agents = await invoke<AgentCard[]>('list_available_agents');
      if (Array.isArray(agents) && agents.length > 0) {
        setAgentsFromList(agents);
        await cacheService.set(AGENT_CACHE_KEYS.DISCOVERED_AGENTS, agents, 'api-response', AGENT_CACHE_TTL).catch(() => {});
        return;
      }
    } catch (rustErr) {
      console.warn('[useAgentConfig] Rust discovery failed, trying Python fallback:', rustErr);
    }

    try {
      const timeoutPromise = new Promise<AgentCard[]>((_, reject) =>
        setTimeout(() => reject(new Error('Agent discovery timeout')), 20000)
      );
      const agents = await Promise.race([agentService.discoverAgents(), timeoutPromise]);
      if (Array.isArray(agents) && agents.length > 0) {
        setAgentsFromList(agents);
        await cacheService.set(AGENT_CACHE_KEYS.DISCOVERED_AGENTS, agents, 'api-response', AGENT_CACHE_TTL).catch(() => {});
        return;
      }
    } catch (pythonErr) {
      console.warn('[useAgentConfig] Python discovery failed:', pythonErr);
    }

    setDiscoveredAgents([]);
    setAgentsByCategory({ all: [] });
  };

  /** Load custom agents from SQLite and merge into discoveredAgents */
  const loadCustomAgentsFromDb = useCallback(async () => {
    try {
      const dbAgents = await getAgentConfigs();
      const customCards: AgentCard[] = dbAgents
        .filter(a => {
          try { return (JSON.parse(a.config_json) as any).user_created === true; } catch { return false; }
        })
        .map(a => {
          let cfg: AgentConfigData = {};
          try { cfg = JSON.parse(a.config_json); } catch {}
          return {
            id: a.id,
            name: a.name,
            description: a.description || '',
            category: a.category,
            version: '1.0',
            provider: cfg.model?.provider || 'custom',
            capabilities: cfg.tools || [],
            config: cfg,
          } as AgentCard;
        });

      if (customCards.length === 0) return;

      setDiscoveredAgents(prev => {
        const existingIds = new Set(prev.map(a => a.id));
        const newOnes = customCards.filter(c => !existingIds.has(c.id));
        if (newOnes.length === 0) return prev;
        return [...prev, ...newOnes];
      });
      setAgentsByCategory(prev => {
        const updated = { ...prev };
        for (const card of customCards) {
          const cat = card.category || 'custom';
          if (!updated[cat]) updated[cat] = [];
          if (!updated[cat].find(a => a.id === card.id)) updated[cat].push(card);
          if (!updated.all) updated.all = [];
          if (!updated.all.find(a => a.id === card.id)) updated.all.push(card);
        }
        return updated;
      });
    } catch (e) {
      // Non-fatal — SQLite custom agents are optional
    }
  }, []);

  const setAgentsFromList = (agents: AgentCard[]) => {
    setDiscoveredAgents(agents);
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
    const knowledgeCfg = typeof config.knowledge === 'object' ? config.knowledge : {};
    const guardrailsCfg = typeof config.guardrails === 'object' ? config.guardrails : {};
    const storageCfg = typeof config.storage === 'object' ? config.storage : {};
    const memoryCfg = typeof config.memory === 'object' ? config.memory : {};
    const agenticMemCfg = typeof config.agentic_memory === 'object' ? config.agentic_memory : {};
    const fallback = getActiveFallback();
    setEditedConfig({
      provider: model.provider || fallback.provider,
      model_id: model.model_id || fallback.model_id,
      temperature: model.temperature ?? 0.7,
      max_tokens: model.max_tokens ?? 4096,
      tools: config.tools || [],
      instructions: config.instructions || '',
      enable_memory: !!(config.memory?.enabled ?? config.memory),
      enable_reasoning: !!(config.reasoning?.enabled ?? config.reasoning),
      enable_knowledge: !!(config.knowledge?.enabled ?? config.knowledge),
      enable_guardrails: !!(config.guardrails?.enabled ?? config.guardrails),
      enable_agentic_memory: !!(config.agentic_memory?.enabled ?? config.agentic_memory),
      enable_storage: !!(config.storage?.enabled ?? config.storage),
      enable_tracing: !!config.tracing,
      enable_compression: !!config.compression,
      enable_hooks: !!config.hooks,
      enable_evaluation: !!config.evaluation,
      knowledge_type: knowledgeCfg?.type || 'url',
      knowledge_urls: (knowledgeCfg?.urls || []).join('\n'),
      knowledge_vector_db: knowledgeCfg?.vector_db || 'lancedb',
      knowledge_embedder: knowledgeCfg?.embedder || fallback.provider,
      guardrails_pii: guardrailsCfg?.pii_detection ?? true,
      guardrails_injection: guardrailsCfg?.injection_check ?? true,
      guardrails_compliance: guardrailsCfg?.financial_compliance ?? false,
      storage_type: storageCfg?.type || 'sqlite',
      storage_db_path: storageCfg?.db_path || '',
      storage_table: storageCfg?.table_name || 'agent_sessions',
      agentic_memory_user_id: agenticMemCfg?.user_id || '',
      memory_db_path: memoryCfg?.db_path || '',
      memory_table: memoryCfg?.table_name || 'agent_memory',
      memory_create_user_memories: memoryCfg?.create_user_memories ?? true,
      memory_create_session_summary: memoryCfg?.create_session_summary ?? true,
    });
    setJsonText(JSON.stringify(agent, null, 2));
  };

  // ─── Active LLM fallback (derived from already-loaded configuredLLMs state) ─
  // All panels in this hook use this — no repeated DB calls needed.
  const getActiveFallback = (): { provider: string; model_id: string } => {
    const active = configuredLLMs.find(c => c.is_active) ?? configuredLLMs[0] ?? null;
    return {
      provider: active?.provider ?? 'fincept',
      model_id: active?.model ?? 'fincept-llm',
    };
  };

  // ─── API Keys ─────────────────────────────────────────────────────────────

  const getApiKeys = async (): Promise<Record<string, string>> => {
    try {
      const llmConfigs = await getLLMConfigs();
      const apiKeys: Record<string, string> = {};
      for (const config of llmConfigs) {
        if (config.api_key) {
          const providerUpper = config.provider.toUpperCase();
          apiKeys[`${providerUpper}_API_KEY`] = config.api_key;
          apiKeys[config.provider] = config.api_key;
          apiKeys[config.provider.toLowerCase()] = config.api_key;
        }
        if (config.base_url) {
          apiKeys[`${config.provider.toUpperCase()}_BASE_URL`] = config.base_url;
          apiKeys[`${config.provider}_base_url`] = config.base_url;
        }
      }
      // Always include the Fincept session API key (saved on login) so
      // the finagent_core Python agent can use the fincept provider even
      // if the user hasn't manually added it in LLM Settings.
      if (!apiKeys['fincept']) {
        const finceptKey = await getSetting('fincept_api_key');
        if (finceptKey) {
          apiKeys['fincept'] = finceptKey;
          apiKeys['FINCEPT_API_KEY'] = finceptKey;
        }
      }
      return apiKeys;
    } catch (err) {
      console.error('[useAgentConfig] Failed to get API keys:', err);
      return {};
    }
  };

  // ─── Build Config Options ─────────────────────────────────────────────────

  const buildDetailedConfigOptions = () => ({
    tools: editedConfig.tools,
    temperature: editedConfig.temperature,
    maxTokens: editedConfig.max_tokens,
    memory: editedConfig.enable_memory
      ? {
          enabled: true,
          db_path: editedConfig.memory_db_path || undefined,
          table_name: editedConfig.memory_table || undefined,
          create_user_memories: editedConfig.memory_create_user_memories,
          create_session_summary: editedConfig.memory_create_session_summary,
        }
      : false,
    reasoning: editedConfig.enable_reasoning,
    knowledge: editedConfig.enable_knowledge
      ? {
          enabled: true,
          type: editedConfig.knowledge_type,
          urls: editedConfig.knowledge_urls ? editedConfig.knowledge_urls.split('\n').filter(u => u.trim()) : undefined,
          vector_db: editedConfig.knowledge_vector_db || undefined,
          embedder: editedConfig.knowledge_embedder || undefined,
        }
      : undefined,
    guardrails: editedConfig.enable_guardrails
      ? {
          enabled: true,
          pii_detection: editedConfig.guardrails_pii,
          injection_check: editedConfig.guardrails_injection,
          financial_compliance: editedConfig.guardrails_compliance,
        }
      : false,
    agentic_memory: editedConfig.enable_agentic_memory
      ? { enabled: true, user_id: editedConfig.agentic_memory_user_id || undefined }
      : false,
    storage: editedConfig.enable_storage
      ? {
          enabled: true,
          type: editedConfig.storage_type,
          db_path: editedConfig.storage_db_path || undefined,
          table_name: editedConfig.storage_table || undefined,
        }
      : undefined,
    tracing: editedConfig.enable_tracing,
    compression: editedConfig.enable_compression,
    hooks: editedConfig.enable_hooks,
    evaluation: editedConfig.enable_evaluation,
  });

  // ─── Agent Execution ──────────────────────────────────────────────────────

  const runAgent = async () => {
    const queryValidation = validateString(testQuery, { minLength: 1, maxLength: 10000 });
    if (!queryValidation.valid) { setError(queryValidation.error || 'Invalid query'); return; }

    const currentFetchId = ++fetchIdRef.current;
    setExecuting(true);
    setError(null);
    setTestResult(null);

    try {
      const apiKeys = await getApiKeys();
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      const providerKey = editedConfig.provider.toLowerCase();
      const providerKeyUpper = `${editedConfig.provider.toUpperCase()}_API_KEY`;
      const hasProviderKey = apiKeys[providerKey] || apiKeys[providerKeyUpper];

      if (!hasProviderKey && editedConfig.provider !== 'ollama') {
        setError(
          `${editedConfig.provider.toUpperCase()} API key not configured. ` +
          `Please go to Settings → LLM Configuration and add your API key for ${editedConfig.provider}.`
        );
        setExecuting(false);
        return;
      }

      if (useAutoRouting) {
        const routingResult = await withTimeout(
          agentService.routeQuery(testQuery, apiKeys),
          310000,
          'Query routing timed out'
        );
        setLastRoutingResult(routingResult);
        setChatMessages(prev => [...prev, {
          role: 'system',
          content: `🎯 Routed to: ${routingResult.agent_id} (${routingResult.intent}) - Confidence: ${(routingResult.confidence * 100).toFixed(0)}%`,
          timestamp: new Date(),
          routingInfo: routingResult,
        }]);

        const userConfig = agentService.buildAgentConfig(
          editedConfig.provider, editedConfig.model_id, editedConfig.instructions, buildDetailedConfigOptions()
        );
        const result = await withTimeout(
          agentService.executeRoutedQuery(testQuery, apiKeys, undefined, userConfig),
          310000,
          'Agent execution timed out'
        );
        setTestResult(result);
        setChatMessages(prev => [...prev,
          { role: 'user', content: testQuery, timestamp: new Date() },
          {
            role: 'assistant',
            content: extractAgentResponseText(result.response || result.error || JSON.stringify(result, null, 2)),
            timestamp: new Date(),
            agentName: routingResult.agent_id,
          },
        ]);
      } else {
        const config = agentService.buildAgentConfig(
          editedConfig.provider, editedConfig.model_id, editedConfig.instructions, buildDetailedConfigOptions()
        );
        let result;
        if (selectedOutputModel) {
          result = await withTimeout(
            agentService.runAgentStructured(testQuery, config, selectedOutputModel, apiKeys),
            310000, 'Agent execution timed out'
          );
        } else {
          result = await withTimeout(
            agentService.runAgent(testQuery, config, apiKeys),
            310000, 'Agent execution timed out'
          );
        }
        setTestResult(result);
        setChatMessages(prev => [...prev,
          { role: 'user', content: testQuery, timestamp: new Date() },
          {
            role: 'assistant',
            content: extractAgentResponseText(result.response || result.error || JSON.stringify(result, null, 2)),
            timestamp: new Date(),
            agentName: selectedAgent?.name || 'Agent',
          },
        ]);
      }

      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      setSuccess('Query executed successfully');
      setTimeout(() => setSuccess(null), 3000);
    } catch (err: any) {
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      setError(err.toString());
    } finally {
      if (mountedRef.current && currentFetchId === fetchIdRef.current) {
        setExecuting(false);
      }
    }
  };

  const runTeam = async () => {
    if (teamMembers.length === 0) { setError('Please add at least one agent to the team'); return; }
    const queryValidation = validateString(testQuery, { minLength: 1, maxLength: 10000 });
    if (!queryValidation.valid) { setError(queryValidation.error || 'Invalid query'); return; }

    const currentFetchId = ++fetchIdRef.current;
    setExecuting(true);
    setError(null);
    setTeamLog([]);

    try {
      const apiKeys = await getApiKeys();
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      const fallback = getActiveFallback();
      const resolvedProvider = teamProvider || fallback.provider;
      const resolvedModelId = teamModelId || fallback.model_id;

      const teamConfig = agentService.buildTeamConfig(
        'Custom Team',
        teamMode,
        teamMembers.map(agent => ({
          name: agent.name,
          role: agent.description,
          provider: agent.config?.model?.provider || resolvedProvider,
          modelId: agent.config?.model?.model_id || resolvedModelId,
          tools: agent.config?.tools || [],
          instructions: agent.config?.instructions || '',
        })),
        {
          show_members_responses: showMembersResponses,
          leader_index: teamLeaderIndex >= 0 ? teamLeaderIndex : undefined,
        }
      );

      // Log each member being called
      const logs: { label: string; text: string }[] = [];
      teamMembers.forEach((m, i) => {
        logs.push({ label: `AGENT ${i + 1} — ${m.name}`, text: `Role: ${m.description || 'General'}` });
      });
      logs.push({ label: 'STATUS', text: `Running ${teamMode} team with ${teamMembers.length} agents...` });
      setTeamLog(logs);

      const result = await withTimeout(
        agentService.runTeam(testQuery, teamConfig, apiKeys),
        600000,
        'Team execution timed out'
      );
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      const finalContent = extractAgentResponseText(result.response || result.error || JSON.stringify(result, null, 2));

      setTeamLog(prev => [...prev, { label: 'DONE', text: 'Team execution completed' }]);
      setTestResult({ success: true, response: finalContent });
      setChatMessages(prev => [...prev,
        { role: 'user', content: testQuery, timestamp: new Date() },
        {
          role: 'assistant',
          content: finalContent,
          timestamp: new Date(),
          agentName: `Team (${teamMembers.length} agents)`,
        },
      ]);
      setSuccess('Team executed successfully');
      setTimeout(() => setSuccess(null), 3000);
    } catch (err: any) {
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      setTeamLog(prev => [...prev, { label: 'ERROR', text: err.toString() }]);
      setError(err.toString());
    } finally {
      if (mountedRef.current && currentFetchId === fetchIdRef.current) setExecuting(false);
    }
  };


  const runWorkflow = async (workflowType: string, extraParams?: Record<string, any>) => {
    if (workflowType === 'stock_analysis') {
      const symbolValidation = validateSymbol(workflowSymbol);
      if (!symbolValidation.valid) { setError(symbolValidation.error || 'Invalid symbol'); return; }
    }

    const currentFetchId = ++fetchIdRef.current;
    setExecuting(true);
    setError(null);

    try {
      const apiKeys = await getApiKeys();
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      let result;
      switch (workflowType) {
        case 'stock_analysis':
          result = await agentService.runStockAnalysis(sanitizeSymbol(workflowSymbol), apiKeys);
          break;
        case 'portfolio_rebal':
          result = await agentService.runPortfolioRebalancing({ holdings: [] }, apiKeys);
          break;
        case 'risk_assessment':
          result = await agentService.runRiskAssessment({ holdings: [] }, apiKeys);
          break;
        case 'portfolio_plan':
          result = await agentService.createPortfolioPlan(extraParams?.portfolio_id || 'default');
          break;
        case 'paper_trade': {
          const symbol = extraParams?.symbol || workflowSymbol || 'AAPL';
          result = await agentService.paperExecuteTrade(
            extraParams?.portfolio_id || 'paper-default',
            symbol, extraParams?.action || 'buy',
            extraParams?.quantity || 10, extraParams?.price || 150
          );
          break;
        }
        case 'paper_portfolio':
          result = await agentService.paperGetPortfolio(extraParams?.portfolio_id || 'paper-default');
          break;
        case 'paper_positions':
          result = await agentService.paperGetPositions(extraParams?.portfolio_id || 'paper-default');
          break;
        case 'save_session':
          result = await agentService.saveSession({
            agent_id: selectedAgent?.id || 'default',
            messages: chatMessages.map(m => ({ role: m.role, content: m.content })),
          });
          break;
        case 'save_memory':
          result = await agentService.saveMemoryRepo(
            extraParams?.content || testQuery,
            { type: extraParams?.category || 'general' }
          );
          break;
        case 'search_memories':
          result = await agentService.searchMemoriesRepo(
            extraParams?.query || testQuery, undefined, extraParams?.limit || 10
          );
          break;
        case 'multi_query':
          result = await agentService.executeMultiQuery(extraParams?.query || testQuery, apiKeys);
          break;
        default:
          throw new Error(`Unknown workflow: ${workflowType}`);
      }

      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      setTestResult(result);
      if (result.success) {
        setSuccess(`${workflowType} completed`);
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      setError(err.toString());
    } finally {
      if (mountedRef.current && currentFetchId === fetchIdRef.current) {
        setExecuting(false);
      }
    }
  };

  // ─── Planner ──────────────────────────────────────────────────────────────

  const createPlan = async (planQuery?: string) => {
    if (!workflowSymbol.trim()) return;
    setExecuting(true);
    setError(null);
    setPlanGeneratedBy(null);
    try {
      const apiKeys = await getApiKeys();
      const query = planQuery
        ? `${planQuery} for ${workflowSymbol}`
        : `Perform a comprehensive financial analysis of ${workflowSymbol} including price data, fundamentals, news sentiment, technical analysis, and an overall investment recommendation.`;

      const result = await agentService.generateDynamicPlan(query, apiKeys);
      if (result?.plan) {
        setCurrentPlan(result.plan);
        setPlanGeneratedBy(result.generated_by === 'fallback' ? 'fallback' : 'llm');
        setSuccess(result.generated_by === 'llm' ? 'AI-generated plan ready' : 'Plan created (template fallback)');
        setTimeout(() => setSuccess(null), 3000);
      } else {
        setError('Failed to generate plan');
      }
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setExecuting(false);
    }
  };

  const executePlan = async (planOverride?: ExecutionPlan) => {
    const planToExecute = planOverride ?? currentPlan;
    if (!planToExecute) return;
    setPlanExecuting(true);
    setError(null);
    try {
      const apiKeys = await getApiKeys();
      const result = await agentService.executePlan(planToExecute, apiKeys);
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

  // ─── Team Management ──────────────────────────────────────────────────────

  const addToTeam = (agent: AgentCard) => {
    if (!teamMembers.find(m => m.id === agent.id)) {
      setTeamMembers(prev => [...prev, agent]);
    }
  };

  const removeFromTeam = (agentId: string) => {
    setTeamMembers(prev => prev.filter(m => m.id !== agentId));
  };

  // ─── Tools Management ─────────────────────────────────────────────────────

  const toggleToolCategory = (category: string) => {
    setExpandedCategories(prev => {
      const newSet = new Set(prev);
      if (newSet.has(category)) newSet.delete(category); else newSet.add(category);
      return newSet;
    });
  };

  const copyTool = (tool: string) => {
    navigator.clipboard.writeText(tool);
    setCopiedTool(tool);
    setTimeout(() => setCopiedTool(null), 2000);
  };

  const toggleToolSelection = (tool: string) => {
    setEditedConfig(prev => ({
      ...prev,
      tools: prev.tools.includes(tool) ? prev.tools.filter(t => t !== tool) : [...prev.tools, tool],
    }));
  };

  // ─── Chat ─────────────────────────────────────────────────────────────────

  const handleChatSubmit = async () => {
    if (!chatInput.trim() || loading) return;
    const query = chatInput;
    setChatInput('');
    setTestQuery(query);
    setChatMessages(prev => [...prev, { role: 'user', content: query, timestamp: new Date() }]);
    setExecuting(true);
    try {
      const apiKeys = await getApiKeys();
      const userConfig = agentService.buildAgentConfig(
        editedConfig.provider, editedConfig.model_id, editedConfig.instructions, buildDetailedConfigOptions()
      );
      if (useAutoRouting) {
        const routingResult = await agentService.routeQuery(query, apiKeys);
        setLastRoutingResult(routingResult);
        setChatMessages(prev => [...prev, {
          role: 'system',
          content: `🎯 Routed to: ${routingResult.agent_id} (${routingResult.intent})`,
          timestamp: new Date(),
          routingInfo: routingResult,
        }]);
        const result = await agentService.executeRoutedQuery(query, apiKeys, undefined, userConfig);
        setChatMessages(prev => [...prev, {
          role: 'assistant',
          content: extractAgentResponseText(result.response || result.error || 'No response'),
          timestamp: new Date(),
          agentName: routingResult.agent_id,
        }]);
      } else {
        const result = await agentService.runAgent(query, userConfig, apiKeys);
        setChatMessages(prev => [...prev, {
          role: 'assistant',
          content: extractAgentResponseText(result.response || result.error || 'No response'),
          timestamp: new Date(),
          agentName: selectedAgent?.name || 'Agent',
        }]);
      }
    } catch (err: any) {
      if (mountedRef.current) {
        setChatMessages(prev => [...prev, { role: 'system', content: `Error: ${err.message}`, timestamp: new Date() }]);
      }
    } finally {
      if (mountedRef.current) setExecuting(false);
    }
  };

  // ─── Return All State & Handlers ──────────────────────────────────────────

  return {
    // View
    viewMode, setViewMode,
    // Agents
    discoveredAgents, selectedAgent, setSelectedAgent,
    agentsByCategory, selectedCategory, setSelectedCategory,
    // Status
    loading, executing, error, setError, success,
    // Config
    editMode, setEditMode, editedConfig, setEditedConfig,
    // LLM / System
    systemInfo, toolsInfo, configuredLLMs, llmProvidersList, loadLLMConfigs, loadSystemData,
    // Query
    testQuery, setTestQuery, testResult, setTestResult,
    selectedOutputModel, setSelectedOutputModel,
    showJsonEditor, setShowJsonEditor, jsonText, setJsonText,
    // Routing
    useAutoRouting, setUseAutoRouting, lastRoutingResult,
    // Team
    teamMembers, teamMode, setTeamMode,
    showMembersResponses, setShowMembersResponses,
    teamLeaderIndex, setTeamLeaderIndex,
    teamProvider, setTeamProvider,
    teamModelId, setTeamModelId,
    // Chat
    chatMessages, chatInput, setChatInput, chatEndRef,
    // Tools — selectedTools is derived from editedConfig.tools (single source of truth)
    toolSearch, setToolSearch, expandedCategories,
    selectedTools: editedConfig.tools,
    setSelectedTools: (tools: string[]) => setEditedConfig(prev => ({ ...prev, tools })),
    copiedTool,
    // Portfolio
    portfolios, selectedPortfolioCtx, setSelectedPortfolioCtx,
    showPortfolioDropdown, setShowPortfolioDropdown,
    // Team log
    teamLog,
    // Workflow / Planner
    workflowSymbol, setWorkflowSymbol, currentPlan, planExecuting, planGeneratedBy,
    // Actions
    loadDiscoveredAgents,
    loadCustomAgentsFromDb,
    updateEditedConfigFromAgent,
    runAgent, runTeam, runWorkflow,
    createPlan, executePlan,
    addToTeam, removeFromTeam,
    toggleToolCategory, copyTool, toggleToolSelection,
    handleChatSubmit,
    buildDetailedConfigOptions,
    clearAgentCache: () => agentService.clearCache(),
    getCacheStats: () => agentService.getCacheStats(),
  };
}

export type AgentConfigState = ReturnType<typeof useAgentConfig>;
