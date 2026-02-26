// SQLite Service - Rust SQLite backend via Tauri commands
// All database operations now use high-performance Rust backend

import { invoke } from '@tauri-apps/api/core';

// Re-export storage service (key-value storage)
export { storageService } from './storageService';
export type { StorageEntry } from './storageService';

// Re-export cache service (TTL-based caching)
export { cacheService, CacheTTL, CACHE_CATEGORIES as CacheCategory } from '../cache/cacheService';
export type { CacheEntry, CacheStats, CategoryStats } from '../cache/cacheService';

// Re-export watchlist services
export { watchlistService } from './watchlistService';
export type { Watchlist, WatchlistStock } from './watchlistService';

// Re-export notes services
export { notesService } from './notesService';
export type { Note, NoteTemplate } from './notesService';

// Re-export excel services
export { excelService } from './excelService';
export type { ExcelFile, ExcelSnapshot } from './excelService';

// ==================== API KEYS ====================

export interface ApiKeys {
  FRED_API_KEY?: string;
  ALPHA_VANTAGE_API_KEY?: string;
  OPENAI_API_KEY?: string;
  ANTHROPIC_API_KEY?: string;
  COINGECKO_API_KEY?: string;
  NASDAQ_API_KEY?: string;
  FINANCIAL_MODELING_PREP_API_KEY?: string;
  OPENROUTER_API_KEY?: string;
  DATABENTO_API_KEY?: string;
  WTO_API_KEY?: string;
  EIA_API_KEY?: string;
  BLS_API_KEY?: string;
  BEA_API_KEY?: string;
  [key: string]: string | undefined;
}

// Data API Keys (non-LLM providers)
export const PREDEFINED_API_KEYS = [
  { key: 'FRED_API_KEY', label: 'FRED API Key', description: 'Federal Reserve Economic Data' },
  { key: 'ALPHA_VANTAGE_API_KEY', label: 'Alpha Vantage API Key', description: 'Stock & crypto data' },
  { key: 'COINGECKO_API_KEY', label: 'CoinGecko API Key', description: 'Cryptocurrency data' },
  { key: 'NASDAQ_API_KEY', label: 'NASDAQ API Key', description: 'NASDAQ market data' },
  { key: 'FINANCIAL_MODELING_PREP_API_KEY', label: 'Financial Modeling Prep', description: 'Financial statements & ratios' },
  { key: 'DATABENTO_API_KEY', label: 'Databento API Key', description: 'Institutional-grade market data (Options, Equities, Futures)' },
  { key: 'OPENROUTER_API_KEY', label: 'OpenRouter API Key', description: 'Access 400+ AI models from all providers (Model Library)' },
  { key: 'WTO_API_KEY', label: 'WTO API Key', description: 'World Trade Organization - Trade statistics, restrictions & notifications' },
  { key: 'EIA_API_KEY', label: 'EIA API Key', description: 'U.S. Energy Information Administration - Petroleum, natural gas & energy data' },
  { key: 'BLS_API_KEY', label: 'BLS API Key', description: 'Bureau of Labor Statistics - Employment, wages, CPI & productivity data' },
  { key: 'BEA_API_KEY', label: 'BEA API Key', description: 'Bureau of Economic Analysis - GDP, income, spending & national accounts (NIPA)' },
] as const;

// LLM Provider configurations
export const LLM_PROVIDERS = [
  {
    id: 'fincept',
    name: 'Fincept LLM',
    description: 'Fincept Research API (Default - 5 credits/response)',
    endpoint: 'https://api.fincept.in/research/llm',
    requiresApiKey: true,
    isDefault: true,
    models: ['fincept-llm']
  },
  {
    id: 'openai',
    name: 'OpenAI',
    description: 'GPT models',
    endpoint: 'https://api.openai.com/v1',
    requiresApiKey: true,
    models: ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'o1', 'o1-mini']
  },
  {
    id: 'anthropic',
    name: 'Anthropic',
    description: 'Claude models',
    endpoint: 'https://api.anthropic.com/v1',
    requiresApiKey: true,
    models: ['claude-sonnet-4-20250514', 'claude-3-5-sonnet-20241022', 'claude-3-5-haiku-20241022', 'claude-3-opus-20240229']
  },
  {
    id: 'google',
    name: 'Google Gemini',
    description: 'Gemini models',
    endpoint: 'https://generativelanguage.googleapis.com/v1beta',
    requiresApiKey: true,
    models: ['gemini-2.5-flash-preview-05-20', 'gemini-2.5-pro-preview-05-06', 'gemini-2.0-flash', 'gemini-2.0-flash-lite', 'gemini-1.5-pro', 'gemini-1.5-flash']
  },
  {
    id: 'ollama',
    name: 'Ollama (Local)',
    description: 'Local LLM models',
    endpoint: 'http://localhost:11434',
    requiresApiKey: false,
    models: [] // Dynamically loaded
  },
  {
    id: 'openrouter',
    name: 'OpenRouter',
    description: 'Access to multiple LLM providers',
    endpoint: 'https://openrouter.ai/api/v1',
    requiresApiKey: true,
    supportsCustomModels: true,
    models: []
  },
  {
    id: 'groq',
    name: 'Groq',
    description: 'Fast inference for open models',
    endpoint: 'https://api.groq.com/openai/v1',
    requiresApiKey: true,
    models: ['llama-3.3-70b-versatile', 'llama-3.1-70b-versatile', 'llama-3.1-8b-instant', 'mixtral-8x7b-32768', 'gemma2-9b-it']
  },
  {
    id: 'deepseek',
    name: 'DeepSeek',
    description: 'Advanced reasoning models',
    endpoint: 'https://api.deepseek.com',
    requiresApiKey: true,
    models: ['deepseek-chat', 'deepseek-coder', 'deepseek-reasoner']
  },
] as const;

// ==================== SETTINGS ====================

export interface Setting {
  setting_key: string;
  setting_value: string;
  category?: string;
  updated_at: string;
}

export const saveSetting = async (key: string, value: string, category?: string): Promise<void> => {
  await invoke('db_save_setting', { key, value, category });
};

export const getSetting = async (key: string): Promise<string | null> => {
  return await invoke<string | null>('db_get_setting', { key });
};

export const getAllSettings = async (): Promise<Setting[]> => {
  return await invoke<Setting[]>('db_get_all_settings');
};

// ==================== CREDENTIALS ====================

export interface Credential {
  id?: number;
  service_name: string;
  username?: string;
  password?: string;
  api_key?: string;
  api_secret?: string;
  additional_data?: string;
  created_at: string;
  updated_at: string;
}

export const saveCredential = async (cred: Omit<Credential, 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string }> => {
  const credWithTimestamps = {
    ...cred,
    created_at: new Date().toISOString(),
    updated_at: new Date().toISOString()
  };
  // Rust command expects 'credential' not 'cred'
  return await invoke('db_save_credential', { credential: credWithTimestamps });
};

export const getCredentials = async (): Promise<Credential[]> => {
  return await invoke<Credential[]>('db_get_credentials');
};

export const getCredentialByService = async (serviceName: string): Promise<Credential | null> => {
  return await invoke<Credential | null>('db_get_credential_by_service', { serviceName });
};

export const deleteCredential = async (id: number): Promise<{ success: boolean; message: string }> => {
  return await invoke('db_delete_credential', { id });
};

// ==================== LLM CONFIG ====================

export interface LLMConfig {
  provider: string;
  api_key: string;
  base_url?: string;
  model: string;
  is_active: boolean;
  created_at: string;
  updated_at: string;
}

export interface LLMModelConfig {
  id: string;
  provider: string;
  model_id: string;
  display_name: string;
  api_key?: string;
  base_url?: string;
  is_enabled: boolean;
  is_default: boolean;
  created_at?: string;
  updated_at?: string;
}

export interface LLMGlobalSettings {
  temperature: number;
  max_tokens: number;
  system_prompt: string;
}

export const getLLMConfigs = async (): Promise<LLMConfig[]> => {
  return await invoke<LLMConfig[]>('db_get_llm_configs');
};

export const saveLLMConfig = async (config: LLMConfig): Promise<void> => {
  await invoke('db_save_llm_config', { config });
};

export const getActiveLLMConfig = async (): Promise<LLMConfig | null> => {
  const configs = await getLLMConfigs();
  return configs.find(c => c.is_active) || null;
};

/**
 * Build a complete api_keys map from LLM configs — the canonical implementation.
 * Matches exactly what useAgentConfig.getApiKeys() does so all panels behave the same.
 *
 * Keys emitted per provider:
 *   OPENAI_API_KEY, openai, openai_base_url, OPENAI_BASE_URL, …
 * Plus fincept fallback from the stored session key.
 */
export const buildApiKeysMap = async (): Promise<Record<string, string>> => {
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
        const providerUpper = config.provider.toUpperCase();
        apiKeys[`${providerUpper}_BASE_URL`] = config.base_url;
        apiKeys[`${config.provider}_base_url`] = config.base_url;
      }
    }

    // Fincept session key — always include so Python agents can use the
    // fincept provider even if the user hasn't added it in LLM Settings.
    if (!apiKeys['fincept']) {
      const finceptKey = await getSetting('fincept_api_key');
      if (finceptKey) {
        apiKeys['fincept'] = finceptKey;
        apiKeys['FINCEPT_API_KEY'] = finceptKey;
      }
    }

    return apiKeys;
  } catch {
    return {};
  }
};

export const getLLMGlobalSettings = async (): Promise<LLMGlobalSettings> => {
  return await invoke<LLMGlobalSettings>('db_get_llm_global_settings');
};

export const saveLLMGlobalSettings = async (settings: LLMGlobalSettings): Promise<void> => {
  await invoke('db_save_llm_global_settings', { settings });
};

// ==================== ALPHA ARENA ====================

export interface AlphaCompetition {
  id: string;
  name: string;
  symbol: string;
  mode: string;
  status: string;
  cycle_count: number;
  cycle_interval_seconds: number;
  initial_capital: number;
  started_at?: string;
  created_at?: string;
  updated_at?: string;
}

export interface AlphaModelState {
  id: string;
  competition_id: string;
  model_name: string;
  provider: string;
  model_id: string;
  capital: number;
  positions_json: string;
  last_decision?: string;
  trades_count?: number;
  total_pnl?: number;
  portfolio_value?: number;
  created_at?: string;
  updated_at?: string;
}

export interface AlphaPerformanceSnapshot {
  id?: number;
  competition_id: string;
  model_name: string;
  timestamp: string;
  cycle_number?: number;
  portfolio_value: number;
  cash: number;
  positions_value: number;
  total_return: number;
  created_at?: string;
}

export interface AlphaDecisionLog {
  id?: number;
  competition_id: string;
  model_name: string;
  cycle_number: number;
  timestamp: string;
  decision: string;
  action?: string;
  quantity?: number;
  price_at_decision?: number;
  reasoning?: string;
  trade_executed?: any;
  created_at?: string;
}

// Alpha Arena methods - These are stubs as the data is managed by Python service
export const createAlphaCompetition = async (competition: Partial<AlphaCompetition>): Promise<void> => {
  console.warn('[SqliteService] createAlphaCompetition - stub method, data managed by Python');
};

export const getAlphaCompetitions = async (limit?: number): Promise<AlphaCompetition[]> => {
  console.warn('[SqliteService] getAlphaCompetitions - stub method, data managed by Python');
  return [];
};

export const updateAlphaCompetition = async (id: string, updates: Partial<AlphaCompetition>): Promise<void> => {
  console.warn('[SqliteService] updateAlphaCompetition - stub method, data managed by Python');
};

export const deleteAlphaCompetition = async (id: string): Promise<void> => {
  console.warn('[SqliteService] deleteAlphaCompetition - stub method, data managed by Python');
};

export const saveAlphaModelState = async (state: Partial<AlphaModelState>): Promise<void> => {
  console.warn('[SqliteService] saveAlphaModelState - stub method, data managed by Python');
};

export const getAlphaModelStates = async (competitionId: string): Promise<AlphaModelState[]> => {
  console.warn('[SqliteService] getAlphaModelStates - stub method, data managed by Python');
  return [];
};

export const saveAlphaPerformanceSnapshot = async (snapshot: Partial<AlphaPerformanceSnapshot>): Promise<void> => {
  console.warn('[SqliteService] saveAlphaPerformanceSnapshot - stub method, data managed by Python');
};

export const getAlphaPerformanceSnapshots = async (competitionId: string, modelName?: string): Promise<AlphaPerformanceSnapshot[]> => {
  console.warn('[SqliteService] getAlphaPerformanceSnapshots - stub method, data managed by Python');
  return [];
};

export const saveAlphaDecisionLog = async (log: Partial<AlphaDecisionLog>): Promise<void> => {
  console.warn('[SqliteService] saveAlphaDecisionLog - stub method, data managed by Python');
};

export const getAlphaDecisionLogs = async (competitionId: string, modelName?: string): Promise<AlphaDecisionLog[]> => {
  console.warn('[SqliteService] getAlphaDecisionLogs - stub method, data managed by Python');
  return [];
};

// ==================== CHAT ====================

export interface ChatSession {
  session_uuid: string;
  title: string;
  message_count: number;
  created_at: string;
  updated_at: string;
}

export interface ChatMessage {
  id: string;
  session_uuid: string;
  role: string;
  content: string;
  timestamp: string;
  provider?: string;
  model?: string;
  tokens_used?: number;
  tool_calls?: ToolCall[];
  metadata?: {
    chart_data?: any;
    ticker?: string;
    company?: string;
  };
}

export interface ToolCall {
  name: string;
  args: any;
  result?: any;
  timestamp: string;
  status: 'calling' | 'success' | 'error';
}

export const createChatSession = async (title: string): Promise<ChatSession> => {
  return await invoke<ChatSession>('db_create_chat_session', { title });
};

export const getChatSessions = async (limit?: number): Promise<ChatSession[]> => {
  return await invoke<ChatSession[]>('db_get_chat_sessions', { limit });
};

export const addChatMessage = async (msg: Omit<ChatMessage, 'id' | 'timestamp'>): Promise<ChatMessage> => {
  const messageWithDefaults = {
    id: crypto.randomUUID(),
    timestamp: new Date().toISOString(),
    ...msg
  };
  return await invoke<ChatMessage>('db_add_chat_message', { message: messageWithDefaults });
};

export const getChatMessages = async (sessionUuid: string): Promise<ChatMessage[]> => {
  return await invoke<ChatMessage[]>('db_get_chat_messages', { sessionUuid });
};

export const deleteChatSession = async (sessionUuid: string): Promise<void> => {
  await invoke('db_delete_chat_session', { sessionUuid });
};

export interface ChatStatistics {
  totalSessions: number;
  totalMessages: number;
  totalTokens: number;
}

export const getChatStatistics = async (): Promise<ChatStatistics> => {
  try {
    return await invoke<ChatStatistics>('db_get_chat_statistics');
  } catch (error) {
    // Fallback if Rust command not implemented yet
    console.warn('[SqliteService] db_get_chat_statistics not implemented, using fallback');
    const sessions = await getChatSessions(10000);
    let totalMessages = 0;
    const totalTokens = 0;

    for (const session of sessions) {
      totalMessages += session.message_count || 0;
      // Note: Can't get token count without loading all messages
    }

    return {
      totalSessions: sessions.length,
      totalMessages,
      totalTokens
    };
  }
};

// ==================== DATA SOURCES ====================

export interface DataSource {
  id: string;
  alias: string;
  display_name: string;
  description?: string;
  type: 'websocket' | 'rest_api';
  provider: string;
  category: string;
  config: string;
  enabled: boolean;
  tags?: string;
  created_at: string;
  updated_at: string;
}

export const saveDataSource = async (source: Omit<DataSource, 'created_at' | 'updated_at'>): Promise<{ success: boolean; message: string; id?: string }> => {
  const sourceWithTimestamps = {
    ...source,
    ds_type: source.type, // Map 'type' to 'ds_type' for Rust backend
    created_at: new Date().toISOString(),
    updated_at: new Date().toISOString()
  };
  return await invoke('db_save_data_source', { source: sourceWithTimestamps });
};

export const getAllDataSources = async (): Promise<DataSource[]> => {
  const sources = await invoke<any[]>('db_get_all_data_sources');
  // Map ds_type back to type for frontend
  return sources.map(s => ({ ...s, type: s.ds_type || s.type }));
};

export const deleteDataSource = async (id: string): Promise<{ success: boolean; message: string }> => {
  return await invoke('db_delete_data_source', { id });
};

// Data Source Connection methods - stub implementations
// These methods are for the data-sources tab's connection management
export const saveDataSourceConnection = async (connection: any): Promise<void> => {
  console.warn('[SqliteService] saveDataSourceConnection - stub method, using getAllDataSources as fallback');
  // Could store as a data source with special category
};

export const updateDataSourceConnection = async (id: string, updates: any): Promise<void> => {
  console.warn('[SqliteService] updateDataSourceConnection - stub method');
};

export const deleteDataSourceConnection = async (id: string): Promise<void> => {
  console.warn('[SqliteService] deleteDataSourceConnection - stub method');
};

export const getDataSourceConnection = async (id: string): Promise<any | null> => {
  console.warn('[SqliteService] getDataSourceConnection - stub method');
  return null;
};

export const getAllDataSourceConnections = async (): Promise<any[]> => {
  console.warn('[SqliteService] getAllDataSourceConnections - stub method, returning empty array');
  return [];
};

export const getDataSourceConnectionsByCategory = async (category: string): Promise<any[]> => {
  console.warn('[SqliteService] getDataSourceConnectionsByCategory - stub method');
  return [];
};

export const getDataSourceConnectionsByType = async (type: string): Promise<any[]> => {
  console.warn('[SqliteService] getDataSourceConnectionsByType - stub method');
  return [];
};

// ==================== MCP SERVERS ====================

export interface MCPServer {
  id: string;
  name: string;
  description?: string;
  command: string;
  args?: string;
  env?: string;
  category: string;
  icon?: string;
  enabled: boolean;
  auto_start: boolean;
  status: string;
  created_at: string;
  updated_at: string;
}

export const addMCPServer = async (server: Omit<MCPServer, 'created_at' | 'updated_at'> | MCPServer): Promise<void> => {
  const serverWithTimestamps = {
    ...server,
    created_at: 'created_at' in server ? server.created_at : new Date().toISOString(),
    updated_at: new Date().toISOString()
  };
  await invoke('db_add_mcp_server', { server: serverWithTimestamps });
};

export const getMCPServers = async (): Promise<MCPServer[]> => {
  return await invoke<MCPServer[]>('db_get_mcp_servers');
};

export interface InternalMCPToolSetting {
  tool_name: string;
  category: string;
  is_enabled: boolean;
  updated_at: string;
}

export const deleteMCPServer = async (id: string): Promise<void> => {
  await invoke('db_delete_mcp_server', { id });
};

export const getInternalToolSettings = async (): Promise<InternalMCPToolSetting[]> => {
  return await invoke<InternalMCPToolSetting[]>('db_get_internal_tool_settings');
};

export const setInternalToolEnabled = async (toolName: string, category: string, isEnabled: boolean): Promise<void> => {
  await invoke('db_set_internal_tool_enabled', { toolName, category, isEnabled });
};

export const isInternalToolEnabled = async (toolName: string): Promise<boolean> => {
  return await invoke<boolean>('db_is_internal_tool_enabled', { toolName });
};

// ==================== BACKTESTING ====================

export interface BacktestingProvider {
  id: string;
  name: string;
  adapter_type: string;
  config: string;
  enabled: boolean;
  is_active: boolean;
  created_at: string;
  updated_at: string;
}

export interface BacktestingStrategy {
  id: string;
  name: string;
  description?: string;
  version: string;
  author?: string;
  provider_type: string;
  strategy_type: string;
  strategy_definition: string;
  tags?: string;
  created_at: string;
  updated_at: string;
}

export interface BacktestRun {
  id: string;
  strategy_id: string;
  provider_name: string;
  config: string;
  results?: string;
  status: string;
  performance_metrics?: string;
  error_message?: string;
  created_at: string;
  completed_at?: string;
  duration_seconds?: number;
}

export const saveBacktestingProvider = async (provider: Omit<BacktestingProvider, 'created_at' | 'updated_at'> | BacktestingProvider): Promise<{ success: boolean; message: string }> => {
  const providerWithTimestamps = {
    ...provider,
    created_at: 'created_at' in provider ? provider.created_at : new Date().toISOString(),
    updated_at: new Date().toISOString()
  };
  return await invoke('db_save_backtesting_provider', { provider: providerWithTimestamps });
};

export const getBacktestingProviders = async (): Promise<BacktestingProvider[]> => {
  return await invoke<BacktestingProvider[]>('db_get_backtesting_providers');
};

export const saveBacktestingStrategy = async (strategy: BacktestingStrategy): Promise<{ success: boolean; message: string }> => {
  return await invoke('db_save_backtesting_strategy', { strategy });
};

export const getBacktestingStrategies = async (): Promise<BacktestingStrategy[]> => {
  return await invoke<BacktestingStrategy[]>('db_get_backtesting_strategies');
};

export const saveBacktestRun = async (run: BacktestRun): Promise<{ success: boolean; message: string }> => {
  return await invoke('db_save_backtest_run', { run });
};

export const getBacktestRuns = async (limit?: number): Promise<BacktestRun[]> => {
  return await invoke<BacktestRun[]>('db_get_backtest_runs', { limit });
};

// ==================== CONTEXT RECORDING ====================

export interface RecordingSession {
  id: string;
  tab_name: string;
  is_active: boolean;
  auto_record: boolean;
  filters?: string;
  started_at: string;
  stopped_at?: string;
}

export interface RecordedContext {
  id: string;
  tab_name: string;
  data_type: string;
  label: string;
  raw_data: string;
  metadata?: string;
  data_size: number;
  created_at: string;
  tags?: string;
}

export const saveRecordedContext = async (context: Omit<RecordedContext, 'created_at'> | RecordedContext): Promise<void> => {
  const contextWithTimestamp = {
    ...context,
    created_at: 'created_at' in context ? context.created_at : new Date().toISOString()
  };
  await invoke('db_save_recorded_context', { context: contextWithTimestamp });
};

export const getRecordedContexts = async (filters?: string | { tabName?: string; limit?: number; dataType?: string; tags?: string[] }): Promise<RecordedContext[]> => {
  // Handle both old string API and new object API
  if (typeof filters === 'string') {
    return await invoke<RecordedContext[]>('db_get_recorded_contexts', { tabName: filters, limit: undefined });
  } else if (filters && typeof filters === 'object') {
    return await invoke<RecordedContext[]>('db_get_recorded_contexts', { tabName: filters.tabName, limit: filters.limit });
  } else {
    return await invoke<RecordedContext[]>('db_get_recorded_contexts', {});
  }
};

export const deleteRecordedContext = async (id: string): Promise<void> => {
  await invoke('db_delete_recorded_context', { id });
};

// Recording Session methods - stub implementation as not in Rust backend
export const startRecordingSession = async (session: Partial<RecordingSession>): Promise<void> => {
  console.warn('[SqliteService] startRecordingSession - stub method');
};

export const stopRecordingSession = async (tabName: string): Promise<void> => {
  console.warn('[SqliteService] stopRecordingSession - stub method');
};

export const getActiveRecordingSession = async (tabName: string): Promise<RecordingSession | null> => {
  console.warn('[SqliteService] getActiveRecordingSession - stub method');
  return null;
};

export const getRecordedContext = async (id: string): Promise<RecordedContext | null> => {
  const contexts = await getRecordedContexts();
  return contexts.find(c => c.id === id) || null;
};

export const linkContextToChat = async (contextId: string, chatSessionUuid: string): Promise<void> => {
  console.warn('[SqliteService] linkContextToChat - stub method');
};

export const unlinkContextFromChat = async (contextId: string, chatSessionUuid: string): Promise<void> => {
  console.warn('[SqliteService] unlinkContextFromChat - stub method');
};

export const getChatContexts = async (chatSessionUuid: string): Promise<RecordedContext[]> => {
  console.warn('[SqliteService] getChatContexts - stub method');
  return [];
};

export const toggleChatContextActive = async (contextId: string, chatSessionUuid: string): Promise<void> => {
  console.warn('[SqliteService] toggleChatContextActive - stub method');
};

export const clearOldContexts = async (olderThanDays: number): Promise<void> => {
  console.warn('[SqliteService] clearOldContexts - stub method');
};

export const getContextStorageStats = async (): Promise<any> => {
  console.warn('[SqliteService] getContextStorageStats - stub method');
  return null;
};

// ==================== AGENT CONFIGURATIONS ====================

export interface AgentConfig {
  id: string;
  name: string;
  description?: string;
  config_json: string;
  category: string;
  is_default: boolean;
  is_active: boolean;
  created_at: string;
  updated_at: string;
}

export interface AgentConfigData {
  model?: {
    provider: string;
    model_id?: string;
    temperature?: number;
    max_tokens?: number;
  };
  instructions?: string;
  tools?: string[];
  memory?: {
    enabled: boolean;
    optimization_strategy?: string;
  };
  knowledge?: {
    embedder?: { provider: string; model?: string };
    vectordb?: { type: string };
  };
  reasoning?: {
    strategy: string;
    max_steps?: number;
  };
  output_format?: string;
  debug?: boolean;
  mcp_servers?: Array<{
    id?: string;
    name?: string;
    command?: string;
    args?: string[] | string;
    env?: Record<string, string>;
    transport?: string;
    url?: string;
  }>;
}

export const saveAgentConfig = async (config: Omit<AgentConfig, 'created_at' | 'updated_at'> | AgentConfig): Promise<{ success: boolean; message: string }> => {
  const configWithTimestamps = {
    ...config,
    created_at: 'created_at' in config ? config.created_at : new Date().toISOString(),
    updated_at: new Date().toISOString()
  };
  return await invoke('db_save_agent_config', { config: configWithTimestamps });
};

export const getAgentConfigs = async (): Promise<AgentConfig[]> => {
  return await invoke<AgentConfig[]>('db_get_agent_configs');
};

export const getAgentConfig = async (id: string): Promise<AgentConfig | null> => {
  return await invoke<AgentConfig | null>('db_get_agent_config', { id });
};

export const getAgentConfigsByCategory = async (category: string): Promise<AgentConfig[]> => {
  return await invoke<AgentConfig[]>('db_get_agent_configs_by_category', { category });
};

export const deleteAgentConfig = async (id: string): Promise<{ success: boolean; message: string }> => {
  return await invoke('db_delete_agent_config', { id });
};

export const setActiveAgentConfig = async (id: string): Promise<{ success: boolean; message: string }> => {
  return await invoke('db_set_active_agent_config', { id });
};

export const getActiveAgentConfig = async (): Promise<AgentConfig | null> => {
  return await invoke<AgentConfig | null>('db_get_active_agent_config');
};

// ==================== CACHE ====================
// Forum cache methods - using unified cache system

const forumCacheKey = (type: string, suffix?: string) =>
  suffix ? `forum:${type}:${suffix}` : `forum:${type}`;

// Forum categories cache
export const getCachedForumCategories = async (_maxAgeMinutes: number): Promise<any[] | null> => {
  const { cacheService } = await import('../cache/cacheService');
  const result = await cacheService.get<any[]>(forumCacheKey('categories'));
  return result?.data ?? null;
};

export const cacheForumCategories = async (categories: any[]): Promise<void> => {
  const { cacheService } = await import('../cache/cacheService');
  await cacheService.set(forumCacheKey('categories'), categories, 'forum', '5m');
};

// Forum posts cache
export const getCachedForumPosts = async (categoryId: number | null, sortBy: string, _maxAgeMinutes: number): Promise<any[] | null> => {
  const { cacheService } = await import('../cache/cacheService');
  const key = forumCacheKey('posts', `${categoryId || 'all'}-${sortBy}`);
  const result = await cacheService.get<any[]>(key);
  return result?.data ?? null;
};

export const cacheForumPosts = async (categoryId: number | null, sortBy: string, posts: any[]): Promise<void> => {
  const { cacheService } = await import('../cache/cacheService');
  const key = forumCacheKey('posts', `${categoryId || 'all'}-${sortBy}`);
  await cacheService.set(key, posts, 'forum', '5m');
};

// Forum stats cache
export const getCachedForumStats = async (_maxAgeMinutes: number): Promise<any | null> => {
  const { cacheService } = await import('../cache/cacheService');
  const result = await cacheService.get<any>(forumCacheKey('stats'));
  return result?.data ?? null;
};

export const cacheForumStats = async (stats: any): Promise<void> => {
  const { cacheService } = await import('../cache/cacheService');
  await cacheService.set(forumCacheKey('stats'), stats, 'forum', '5m');
};

// ==================== PAPER TRADING ====================

// Portfolio Types
export interface PaperTradingPortfolio {
  id: string;
  name: string;
  provider: string;
  initial_balance: number;
  current_balance: number;
  currency: string;
  margin_mode: string;
  leverage: number;
  created_at: string;
  updated_at: string;
}

export interface PaperTradingPosition {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'long' | 'short';
  entry_price: number;
  quantity: number;
  position_value?: number;
  current_price?: number;
  unrealized_pnl?: number;
  realized_pnl: number;
  leverage: number;
  margin_mode: string;
  liquidation_price?: number;
  opened_at: string;
  closed_at?: string;
  status: 'open' | 'closed';
}

export interface PaperTradingOrder {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'buy' | 'sell';
  order_type: 'market' | 'limit' | 'stop_market' | 'stop_limit';
  quantity: number;
  price?: number;
  stop_price?: number;
  filled_quantity: number;
  avg_fill_price?: number;
  status: 'pending' | 'filled' | 'partial' | 'cancelled' | 'rejected' | 'triggered';
  time_in_force: string;
  post_only: boolean;
  reduce_only: boolean;
  created_at: string;
  filled_at?: string;
  updated_at: string;
}

export interface PaperTradingTrade {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  side: string;
  price: number;
  quantity: number;
  fee: number;
  fee_rate: number;
  is_maker: boolean;
  timestamp: string;
}

export interface MarginBlock {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  blocked_amount: number;
  created_at: string;
}

export interface PaperTradingHolding {
  id: string;
  portfolio_id: string;
  symbol: string;
  quantity: number;
  average_price: number;
  invested_value: number;
  current_price: number;
  current_value: number;
  pnl: number;
  pnl_percent: number;
  t1_quantity: number;
  available_quantity: number;
  created_at: string;
  updated_at: string;
}

export interface PortfolioStats {
  total_value: number;
  available_margin: number;
  blocked_margin: number;
  realized_pnl: number;
  unrealized_pnl: number;
  total_pnl: number;
  open_positions: number;
  total_trades: number;
}

// Portfolio Operations
export const createPortfolio = async (id: string, name: string, provider: string, initialBalance: number, currency: string, marginMode: string, leverage: number): Promise<PaperTradingPortfolio> => {
  return await invoke('db_create_portfolio', { id, name, provider, initialBalance, currency, marginMode, leverage });
};

export const getPortfolio = async (id: string): Promise<PaperTradingPortfolio> => {
  return await invoke('db_get_portfolio', { id });
};

export const listPortfolios = async (): Promise<PaperTradingPortfolio[]> => {
  return await invoke<PaperTradingPortfolio[]>('db_list_portfolios');
};

export const updatePortfolioBalance = async (id: string, newBalance: number): Promise<void> => {
  await invoke('db_update_portfolio_balance', { id, newBalance });
};

export const deletePortfolio = async (id: string): Promise<void> => {
  await invoke('db_delete_portfolio', { id });
};

// Position Operations
export const createPosition = async (
  id: string,
  portfolioId: string,
  symbol: string,
  side: string,
  entryPrice: number,
  quantity: number,
  leverage: number,
  marginMode: string
): Promise<void> => {
  await invoke('db_create_position', { id, portfolioId, symbol, side, entryPrice, quantity, leverage, marginMode });
};

export const getPosition = async (id: string): Promise<PaperTradingPosition> => {
  return await invoke('db_get_position', { id });
};

export const getPositionBySymbol = async (portfolioId: string, symbol: string, status: string): Promise<PaperTradingPosition | null> => {
  return await invoke('db_get_position_by_symbol', { portfolioId, symbol, status });
};

export const getPositionBySymbolAndSide = async (portfolioId: string, symbol: string, side: string, status: string): Promise<PaperTradingPosition | null> => {
  return await invoke('db_get_position_by_symbol_and_side', { portfolioId, symbol, side, status });
};

export const getPortfolioPositions = async (portfolioId: string, status?: string): Promise<PaperTradingPosition[]> => {
  return await invoke<PaperTradingPosition[]>('db_get_portfolio_positions', { portfolioId, status });
};

export const updatePosition = async (
  id: string,
  quantity?: number,
  entryPrice?: number,
  currentPrice?: number,
  unrealizedPnl?: number,
  realizedPnl?: number,
  liquidationPrice?: number,
  status?: string,
  closedAt?: string
): Promise<void> => {
  await invoke('db_update_position', { id, quantity, entryPrice, currentPrice, unrealizedPnl, realizedPnl, liquidationPrice, status, closedAt });
};

export const deletePosition = async (id: string): Promise<void> => {
  await invoke('db_delete_position', { id });
};

// Order Operations
export const createOrder = async (
  id: string,
  portfolioId: string,
  symbol: string,
  side: string,
  orderType: string,
  quantity: number,
  price: number | null,
  timeInForce: string
): Promise<void> => {
  await invoke('db_create_order', { id, portfolioId, symbol, side, orderType, quantity, price, timeInForce });
};

export const getOrder = async (id: string): Promise<PaperTradingOrder> => {
  return await invoke('db_get_order', { id });
};

export const getPendingOrders = async (portfolioId?: string): Promise<PaperTradingOrder[]> => {
  return await invoke<PaperTradingOrder[]>('db_get_pending_orders', { portfolioId });
};

export const getPortfolioOrders = async (portfolioId: string, status?: string): Promise<PaperTradingOrder[]> => {
  return await invoke<PaperTradingOrder[]>('db_get_portfolio_orders', { portfolioId, status });
};

export const updateOrder = async (
  id: string,
  filledQuantity?: number,
  avgFillPrice?: number,
  status?: string,
  filledAt?: string
): Promise<void> => {
  await invoke('db_update_order', { id, filledQuantity, avgFillPrice, status, filledAt });
};

export const deleteOrder = async (id: string): Promise<void> => {
  await invoke('db_delete_order', { id });
};

// Trade Operations
export const createTrade = async (
  id: string,
  portfolioId: string,
  orderId: string,
  symbol: string,
  side: string,
  price: number,
  quantity: number,
  fee: number,
  feeRate: number,
  isMaker: boolean
): Promise<void> => {
  await invoke('db_create_trade', { id, portfolioId, orderId, symbol, side, price, quantity, fee, feeRate, isMaker });
};

export const getTrade = async (id: string): Promise<PaperTradingTrade> => {
  return await invoke('db_get_trade', { id });
};

export const getOrderTrades = async (orderId: string): Promise<PaperTradingTrade[]> => {
  return await invoke<PaperTradingTrade[]>('db_get_order_trades', { orderId });
};

export const getPortfolioTrades = async (portfolioId: string, limit?: number): Promise<PaperTradingTrade[]> => {
  return await invoke<PaperTradingTrade[]>('db_get_portfolio_trades', { portfolioId, limit });
};

export const deleteTrade = async (id: string): Promise<void> => {
  await invoke('db_delete_trade', { id });
};

// Margin Block Operations
export const createMarginBlock = async (
  id: string,
  portfolioId: string,
  orderId: string,
  symbol: string,
  blockedAmount: number
): Promise<void> => {
  await invoke('db_create_margin_block', { id, portfolioId, orderId, symbol, blockedAmount });
};

export const getMarginBlocks = async (portfolioId: string): Promise<MarginBlock[]> => {
  return await invoke<MarginBlock[]>('db_get_margin_blocks', { portfolioId });
};

export const getMarginBlockByOrder = async (orderId: string): Promise<MarginBlock | null> => {
  return await invoke('db_get_margin_block_by_order', { orderId });
};

export const deleteMarginBlock = async (orderId: string): Promise<number> => {
  return await invoke<number>('db_delete_margin_block', { orderId });
};

export const getTotalBlockedMargin = async (portfolioId: string): Promise<number> => {
  return await invoke<number>('db_get_total_blocked_margin', { portfolioId });
};

export const getAvailableMargin = async (portfolioId: string): Promise<number> => {
  return await invoke<number>('db_get_available_margin', { portfolioId });
};

// Holdings Operations (T+1 Settlement)
export const createHolding = async (
  id: string,
  portfolioId: string,
  symbol: string,
  quantity: number,
  averagePrice: number
): Promise<void> => {
  await invoke('db_create_holding', { id, portfolioId, symbol, quantity, averagePrice });
};

export const getHoldings = async (portfolioId: string): Promise<PaperTradingHolding[]> => {
  return await invoke<PaperTradingHolding[]>('db_get_holdings', { portfolioId });
};

export const getHoldingBySymbol = async (portfolioId: string, symbol: string): Promise<PaperTradingHolding | null> => {
  return await invoke('db_get_holding_by_symbol', { portfolioId, symbol });
};

export const updateHolding = async (
  id: string,
  quantity?: number,
  averagePrice?: number,
  currentPrice?: number,
  t1Quantity?: number,
  availableQuantity?: number
): Promise<void> => {
  await invoke('db_update_holding', { id, quantity, averagePrice, currentPrice, t1Quantity, availableQuantity });
};

export const deleteHolding = async (id: string): Promise<void> => {
  await invoke('db_delete_holding', { id });
};

export const processT1Settlement = async (portfolioId: string): Promise<number> => {
  return await invoke<number>('db_process_t1_settlement', { portfolioId });
};

// Execution Engine Operations
export const fillOrder = async (
  orderId: string,
  fillPrice: number,
  fillQuantity: number,
  fee: number,
  feeRate: number
): Promise<string> => {
  return await invoke<string>('db_fill_order', { orderId, fillPrice, fillQuantity, fee, feeRate });
};

export const getPortfolioStats = async (portfolioId: string): Promise<PortfolioStats> => {
  return await invoke<PortfolioStats>('db_get_portfolio_stats', { portfolioId });
};

export const resetPortfolio = async (portfolioId: string, initialBalance: number): Promise<void> => {
  await invoke('db_reset_portfolio', { portfolioId, initialBalance });
};

// Legacy service class for compatibility
class SqliteService {
  async initialize() {
    console.log('[SqliteService] Using Rust SQLite backend - no initialization needed');
    return true;
  }

  isReady() {
    return true; // Rust backend is always ready
  }

  async healthCheck() {
    return { healthy: true, message: 'Rust backend operational' };
  }

  // Settings
  saveSetting = saveSetting;
  getSetting = getSetting;
  getAllSettings = getAllSettings;

  // Credentials
  saveCredential = saveCredential;
  getCredentials = getCredentials;
  getCredentialByService = getCredentialByService;
  deleteCredential = deleteCredential;

  // LLM
  getLLMConfigs = getLLMConfigs;
  saveLLMConfig = saveLLMConfig;
  getActiveLLMConfig = getActiveLLMConfig;
  getLLMGlobalSettings = getLLMGlobalSettings;
  saveLLMGlobalSettings = saveLLMGlobalSettings;
  buildApiKeysMap = buildApiKeysMap;

  async ensureDefaultLLMConfigs() {
    // Not needed with Rust backend - schema handles defaults
    return true;
  }

  // LLM Model Configs
  async getLLMModelConfigs(): Promise<LLMModelConfig[]> {
    try {
      return await invoke<LLMModelConfig[]>('db_get_llm_model_configs');
    } catch (error) {
      console.error('[SqliteService] Failed to get LLM model configs:', error);
      return [];
    }
  }

  async saveLLMModelConfig(config: LLMModelConfig): Promise<{ success: boolean; message: string }> {
    try {
      return await invoke<{ success: boolean; message: string }>('db_save_llm_model_config', { config });
    } catch (error) {
      console.error('[SqliteService] Failed to save LLM model config:', error);
      return { success: false, message: String(error) };
    }
  }

  async deleteLLMModelConfig(id: string): Promise<{ success: boolean; message: string }> {
    try {
      return await invoke<{ success: boolean; message: string }>('db_delete_llm_model_config', { id });
    } catch (error) {
      console.error('[SqliteService] Failed to delete LLM model config:', error);
      return { success: false, message: String(error) };
    }
  }

  async toggleLLMModelConfigEnabled(id: string): Promise<{ success: boolean; message: string }> {
    try {
      return await invoke<{ success: boolean; message: string }>('db_toggle_llm_model_config_enabled', { id });
    } catch (error) {
      console.error('[SqliteService] Failed to toggle LLM model config:', error);
      return { success: false, message: String(error) };
    }
  }

  async updateLLMModelId(id: string, newModelId: string): Promise<{ success: boolean; message: string }> {
    try {
      return await invoke<{ success: boolean; message: string }>('db_update_llm_model_id', { id, newModelId });
    } catch (error) {
      console.error('[SqliteService] Failed to update LLM model ID:', error);
      return { success: false, message: String(error) };
    }
  }

  async fixGoogleModelIds(): Promise<{ success: boolean; message: string }> {
    try {
      return await invoke<{ success: boolean; message: string }>('db_fix_google_model_ids');
    } catch (error) {
      console.error('[SqliteService] Failed to fix Google model IDs:', error);
      return { success: false, message: String(error) };
    }
  }

  async setActiveLLMProvider(provider: string): Promise<void> {
    try {
      await invoke('db_set_active_llm_provider', { provider });
    } catch (error) {
      console.error('[SqliteService] Failed to set active LLM provider:', error);
    }
  }

  // Alpha Arena
  createAlphaCompetition = createAlphaCompetition;
  getAlphaCompetitions = getAlphaCompetitions;
  updateAlphaCompetition = updateAlphaCompetition;
  deleteAlphaCompetition = deleteAlphaCompetition;
  saveAlphaModelState = saveAlphaModelState;
  getAlphaModelStates = getAlphaModelStates;
  saveAlphaPerformanceSnapshot = saveAlphaPerformanceSnapshot;
  getAlphaPerformanceSnapshots = getAlphaPerformanceSnapshots;
  saveAlphaDecisionLog = saveAlphaDecisionLog;
  getAlphaDecisionLogs = getAlphaDecisionLogs;

  // Chat
  createChatSession = createChatSession;
  getChatSessions = getChatSessions;
  addChatMessage = addChatMessage;
  getChatMessages = getChatMessages;
  deleteChatSession = deleteChatSession;
  getChatStatistics = getChatStatistics;

  async updateChatSessionTitle(sessionUuid: string, title: string) {
    // Not implemented in Rust yet - would need to add this command
    console.warn('[SqliteService] updateChatSessionTitle not implemented in Rust backend');
  }

  async clearChatSessionMessages(sessionUuid: string) {
    // Not implemented in Rust yet - would need to add this command
    console.warn('[SqliteService] clearChatSessionMessages not implemented in Rust backend');
  }

  // Data Sources
  saveDataSource = saveDataSource;
  getAllDataSources = getAllDataSources;
  deleteDataSource = deleteDataSource;
  saveDataSourceConnection = saveDataSourceConnection;
  updateDataSourceConnection = updateDataSourceConnection;
  deleteDataSourceConnection = deleteDataSourceConnection;
  getDataSourceConnection = getDataSourceConnection;
  getAllDataSourceConnections = getAllDataSourceConnections;
  getDataSourceConnectionsByCategory = getDataSourceConnectionsByCategory;
  getDataSourceConnectionsByType = getDataSourceConnectionsByType;

  async getEnabledDataSources(): Promise<DataSource[]> {
    const all = await getAllDataSources();
    return all.filter(ds => ds.enabled);
  }

  async getDataSourceByAlias(alias: string): Promise<DataSource | null> {
    const all = await getAllDataSources();
    return all.find(ds => ds.alias === alias) || null;
  }

  async getDataSourcesByType(type: 'websocket' | 'rest_api'): Promise<DataSource[]> {
    const all = await getAllDataSources();
    return all.filter(ds => ds.type === type);
  }

  async getDataSourcesByProvider(provider: string): Promise<DataSource[]> {
    const all = await getAllDataSources();
    return all.filter(ds => ds.provider === provider);
  }

  async toggleDataSourceEnabled(id: string): Promise<{ success: boolean; message: string; enabled: boolean }> {
    const all = await getAllDataSources();
    const source = all.find(ds => ds.id === id);
    if (source) {
      const newEnabled = !source.enabled;
      await saveDataSource({ ...source, enabled: newEnabled });
      return { success: true, message: 'Data source toggled', enabled: newEnabled };
    }
    return { success: false, message: 'Data source not found', enabled: false };
  }

  async searchDataSources(query: string): Promise<DataSource[]> {
    const all = await getAllDataSources();
    const lowerQuery = query.toLowerCase();
    return all.filter(ds =>
      ds.display_name.toLowerCase().includes(lowerQuery) ||
      ds.alias.toLowerCase().includes(lowerQuery) ||
      ds.provider.toLowerCase().includes(lowerQuery)
    );
  }

  async getCachedCategoryData(category: string, _maxAgeMinutes: number): Promise<string | null> {
    // Use unified cache service
    const cacheKey = `market-quotes:${category}:*`;
    const { cacheService } = await import('../cache/cacheService');
    const cached = await cacheService.get<string>(cacheKey);
    return cached?.data || null;
  }

  // Forum cache - using unified cache
  getCachedForumCategories = getCachedForumCategories;
  cacheForumCategories = cacheForumCategories;
  getCachedForumPosts = getCachedForumPosts;
  cacheForumPosts = cacheForumPosts;
  getCachedForumStats = getCachedForumStats;
  cacheForumStats = cacheForumStats;

  // MCP
  addMCPServer = addMCPServer;
  getMCPServers = getMCPServers;
  deleteMCPServer = deleteMCPServer;

  async getMCPServer(id: string): Promise<MCPServer | null> {
    const all = await getMCPServers();
    return all.find(s => s.id === id) || null;
  }

  async updateMCPServerConfig(id: string, updates: Partial<MCPServer>): Promise<void> {
    const server = await this.getMCPServer(id);
    if (server) {
      await addMCPServer({ ...server, ...updates });
    }
  }

  async updateMCPServerStatus(id: string, status: string): Promise<void> {
    await this.updateMCPServerConfig(id, { status });
  }

  async cacheMCPTools(serverId: string, tools: any[]): Promise<void> {
    // Not implemented - would need separate table for MCP tools cache
    console.warn('[SqliteService] cacheMCPTools not implemented');
  }

  async getMCPServerStats(serverId: string): Promise<any> {
    // Not implemented - would need MCP stats table
    return null;
  }

  async logMCPToolUsage(params: { serverId: string; toolName: string; args: string; result: string | null; success: boolean; executionTime: number; errorMessage?: string }): Promise<void> {
    // Not implemented - would need MCP usage logs table
    console.warn('[SqliteService] logMCPToolUsage not implemented');
  }

  async getAutoStartServers(): Promise<MCPServer[]> {
    const all = await getMCPServers();
    return all.filter(s => s.auto_start && s.enabled);
  }

  async getMCPTools(): Promise<any[]> {
    // Stub - not implemented
    console.warn('[SqliteService] getMCPTools not implemented');
    return [];
  }

  // Backtesting
  saveBacktestingProvider = saveBacktestingProvider;
  getBacktestingProviders = getBacktestingProviders;
  saveBacktestingStrategy = saveBacktestingStrategy;
  getBacktestingStrategies = getBacktestingStrategies;
  saveBacktestRun = saveBacktestRun;
  getBacktestRuns = getBacktestRuns;

  async setActiveBacktestingProvider(id: string): Promise<void> {
    const providers = await getBacktestingProviders();
    for (const provider of providers) {
      await saveBacktestingProvider({
        ...provider,
        is_active: provider.id === id
      });
    }
  }

  async deleteBacktestingProvider(id: string): Promise<void> {
    // Not implemented in Rust - would need to add delete command
    console.warn('[SqliteService] deleteBacktestingProvider not implemented');
  }

  // Context Recording
  saveRecordedContext = saveRecordedContext;
  getRecordedContexts = getRecordedContexts;
  deleteRecordedContext = deleteRecordedContext;
  startRecordingSession = startRecordingSession;
  stopRecordingSession = stopRecordingSession;
  getActiveRecordingSession = getActiveRecordingSession;
  getRecordedContext = getRecordedContext;
  linkContextToChat = linkContextToChat;
  unlinkContextFromChat = unlinkContextFromChat;
  getChatContexts = getChatContexts;
  toggleChatContextActive = toggleChatContextActive;
  clearOldContexts = clearOldContexts;
  getContextStorageStats = getContextStorageStats;

  // Cache - REMOVED: Use unified cache system (src/services/cache/cacheService.ts)

  // Paper Trading - Portfolio
  createPortfolio = createPortfolio;
  getPortfolio = getPortfolio;
  listPortfolios = listPortfolios;
  updatePortfolioBalance = updatePortfolioBalance;
  deletePortfolio = deletePortfolio;

  // Paper Trading - Positions
  createPosition = createPosition;
  getPosition = getPosition;
  getPositionBySymbol = getPositionBySymbol;
  getPositionBySymbolAndSide = getPositionBySymbolAndSide;
  getPortfolioPositions = getPortfolioPositions;
  updatePosition = updatePosition;
  deletePosition = deletePosition;

  // Paper Trading - Orders
  createOrder = createOrder;
  getOrder = getOrder;
  getPendingOrders = getPendingOrders;
  getPortfolioOrders = getPortfolioOrders;
  updateOrder = updateOrder;
  deleteOrder = deleteOrder;

  // Paper Trading - Trades
  createTrade = createTrade;
  getTrade = getTrade;
  getOrderTrades = getOrderTrades;
  getPortfolioTrades = getPortfolioTrades;
  deleteTrade = deleteTrade;

  // Paper Trading - Margin Blocks
  createMarginBlock = createMarginBlock;
  getMarginBlocks = getMarginBlocks;
  getMarginBlockByOrder = getMarginBlockByOrder;
  deleteMarginBlock = deleteMarginBlock;
  getTotalBlockedMargin = getTotalBlockedMargin;
  getAvailableMargin = getAvailableMargin;

  // Paper Trading - Holdings (T+1 Settlement)
  createHolding = createHolding;
  getHoldings = getHoldings;
  getHoldingBySymbol = getHoldingBySymbol;
  updateHolding = updateHolding;
  deleteHolding = deleteHolding;
  processT1Settlement = processT1Settlement;

  // Paper Trading - Execution Engine
  fillOrder = fillOrder;
  getPortfolioStats = getPortfolioStats;
  resetPortfolio = resetPortfolio;

  // Agent Configurations
  saveAgentConfig = saveAgentConfig;
  getAgentConfigs = getAgentConfigs;
  getAgentConfig = getAgentConfig;
  getAgentConfigsByCategory = getAgentConfigsByCategory;
  deleteAgentConfig = deleteAgentConfig;
  setActiveAgentConfig = setActiveAgentConfig;
  getActiveAgentConfig = getActiveAgentConfig;

  // WebSocket Provider Configs (not in Rust backend yet)
  async getWSProviderConfigs(): Promise<any[]> {
    // Not implemented - return empty array
    console.warn('[SqliteService] getWSProviderConfigs not implemented in Rust backend');
    return [];
  }

  async saveWSProviderConfig(config: any): Promise<{ success: boolean; message: string }> {
    console.warn('[SqliteService] saveWSProviderConfig not implemented in Rust backend');
    return { success: false, message: 'Not implemented' };
  }

  async deleteWSProviderConfig(id: number | string): Promise<{ success: boolean; message: string }> {
    console.warn('[SqliteService] deleteWSProviderConfig not implemented in Rust backend');
    return { success: false, message: 'Not implemented' };
  }

  async toggleWSProviderEnabled(id: number | string): Promise<{ success: boolean; message: string; enabled: boolean }> {
    console.warn('[SqliteService] toggleWSProviderEnabled not implemented in Rust backend');
    return { success: false, message: 'Not implemented', enabled: false };
  }

  async getWSProviderConfig(id: string): Promise<any | null> {
    console.warn('[SqliteService] getWSProviderConfig not implemented in Rust backend');
    return null;
  }

  // API Keys helper methods
  async getAllApiKeys(): Promise<ApiKeys> {
    const settings = await getAllSettings();
    const apiKeys: ApiKeys = {};
    for (const predefinedKey of PREDEFINED_API_KEYS) {
      const setting = settings.find(s => s.setting_key === predefinedKey.key);
      if (setting) {
        apiKeys[predefinedKey.key] = setting.setting_value;
      }
    }
    return apiKeys;
  }

  async getApiKey(key: string): Promise<string | null> {
    return await getSetting(key);
  }

  async setApiKey(key: string, value: string): Promise<void> {
    await saveSetting(key, value, 'api_keys');
  }

  // Raw SQL methods for compatibility - these are stubs
  async execute(sql: string, params?: any[]): Promise<void> {
    console.warn('[SqliteService] execute() - raw SQL not supported, use Rust invoke commands');
    throw new Error('Raw SQL execute() not supported - use Rust invoke() commands instead');
  }

  async select<T = any>(sql: string, params?: any[]): Promise<T[]> {
    console.warn('[SqliteService] select() - raw SQL not supported, use Rust invoke commands');
    return [];
  }
}

export const sqliteService = new SqliteService();
export default sqliteService;
