// SQLite Service - Rust SQLite backend via Tauri commands
// All database operations now use high-performance Rust backend

import { invoke } from '@tauri-apps/api/core';

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
  POLYGON_API_KEY?: string;
  ALPHA_VANTAGE_API_KEY?: string;
  OPENAI_API_KEY?: string;
  ANTHROPIC_API_KEY?: string;
  COINGECKO_API_KEY?: string;
  NASDAQ_API_KEY?: string;
  FINANCIAL_MODELING_PREP_API_KEY?: string;
  [key: string]: string | undefined;
}

// Data API Keys (non-LLM providers)
export const PREDEFINED_API_KEYS = [
  { key: 'FRED_API_KEY', label: 'FRED API Key', description: 'Federal Reserve Economic Data' },
  { key: 'POLYGON_API_KEY', label: 'Polygon.io API Key', description: 'Stock market data' },
  { key: 'ALPHA_VANTAGE_API_KEY', label: 'Alpha Vantage API Key', description: 'Stock & crypto data' },
  { key: 'COINGECKO_API_KEY', label: 'CoinGecko API Key', description: 'Cryptocurrency data' },
  { key: 'NASDAQ_API_KEY', label: 'NASDAQ API Key', description: 'NASDAQ market data' },
  { key: 'FINANCIAL_MODELING_PREP_API_KEY', label: 'Financial Modeling Prep', description: 'Financial statements & ratios' },
] as const;

// LLM Provider configurations
export const LLM_PROVIDERS = [
  {
    id: 'fincept',
    name: 'Fincept LLM',
    description: 'Fincept Research API (Default - 5 credits/response)',
    endpoint: 'https://finceptbackend.share.zrok.io/research/llm',
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
    models: ['gpt-4-turbo', 'gpt-4', 'gpt-3.5-turbo', 'gpt-4o']
  },
  {
    id: 'anthropic',
    name: 'Anthropic',
    description: 'Claude models',
    endpoint: 'https://api.anthropic.com/v1',
    requiresApiKey: true,
    models: ['claude-3-opus-20240229', 'claude-3-sonnet-20240229', 'claude-3-haiku-20240307', 'claude-3-5-sonnet-20241022']
  },
  {
    id: 'google',
    name: 'Google Gemini',
    description: 'Gemini models',
    endpoint: 'https://generativelanguage.googleapis.com/v1beta',
    requiresApiKey: true,
    models: ['gemini-pro', 'gemini-1.5-pro-latest', 'gemini-1.5-flash']
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
  return await invoke('db_save_credential', { cred: credWithTimestamps });
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
  return await invoke<ChatMessage>('db_add_chat_message', { msg: messageWithDefaults });
};

export const getChatMessages = async (sessionUuid: string): Promise<ChatMessage[]> => {
  return await invoke<ChatMessage[]>('db_get_chat_messages', { sessionUuid });
};

export const deleteChatSession = async (sessionUuid: string): Promise<void> => {
  await invoke('db_delete_chat_session', { sessionUuid });
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

export const deleteMCPServer = async (id: string): Promise<void> => {
  await invoke('db_delete_mcp_server', { id });
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

// ==================== CACHE ====================

export const saveMarketDataCache = async (symbol: string, category: string, quoteData: string): Promise<void> => {
  await invoke('db_save_market_data_cache', { symbol, category, quoteData });
};

export const getCachedMarketData = async (symbol: string, category: string, maxAgeMinutes: number): Promise<string | null> => {
  return await invoke<string | null>('db_get_cached_market_data', { symbol, category, maxAgeMinutes });
};

export const clearMarketDataCache = async (): Promise<void> => {
  await invoke('db_clear_market_data_cache');
};

// Forum caching methods - stub implementation
export const getCachedForumCategories = async (maxAgeMinutes: number): Promise<any[] | null> => {
  console.warn('[SqliteService] getCachedForumCategories - stub method');
  return null;
};

export const cacheForumCategories = async (categories: any[]): Promise<void> => {
  console.warn('[SqliteService] cacheForumCategories - stub method');
};

export const getCachedForumPosts = async (categoryId: string | number | null, sortBy: string, maxAgeMinutes: number): Promise<any[] | null> => {
  console.warn('[SqliteService] getCachedForumPosts - stub method');
  return null;
};

export const cacheForumPosts = async (categoryId: string | number | null, sortBy: string, posts: any[]): Promise<void> => {
  console.warn('[SqliteService] cacheForumPosts - stub method');
};

export const getCachedForumStats = async (maxAgeMinutes: number): Promise<any | null> => {
  console.warn('[SqliteService] getCachedForumStats - stub method');
  return null;
};

export const cacheForumStats = async (stats: any): Promise<void> => {
  console.warn('[SqliteService] cacheForumStats - stub method');
};

// ==================== PAPER TRADING ====================

export const createPortfolio = async (id: string, name: string, provider: string, initialBalance: number, currency: string, marginMode: string, leverage: number): Promise<any> => {
  return await invoke('db_create_portfolio', { id, name, provider, initialBalance, currency, marginMode, leverage });
};

export const getPortfolio = async (id: string): Promise<any> => {
  return await invoke('db_get_portfolio', { id });
};

export const listPortfolios = async (): Promise<any[]> => {
  return await invoke<any[]>('db_list_portfolios');
};

export const updatePortfolioBalance = async (id: string, currentBalance: number, totalPnl: number): Promise<void> => {
  await invoke('db_update_portfolio_balance', { id, currentBalance, totalPnl });
};

export const createPosition = async (position: any): Promise<any> => {
  return await invoke('db_create_position', { position });
};

export const getPortfolioPositions = async (portfolioId: string): Promise<any[]> => {
  return await invoke<any[]>('db_get_portfolio_positions', { portfolioId });
};

export const createOrder = async (order: any): Promise<any> => {
  return await invoke('db_create_order', { order });
};

export const getPortfolioOrders = async (portfolioId: string, limit?: number): Promise<any[]> => {
  return await invoke<any[]>('db_get_portfolio_orders', { portfolioId, limit });
};

export const getPortfolioTrades = async (portfolioId: string, limit?: number): Promise<any[]> => {
  return await invoke<any[]>('db_get_portfolio_trades', { portfolioId, limit });
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

  async ensureDefaultLLMConfigs() {
    // Not needed with Rust backend - schema handles defaults
    return true;
  }

  // LLM Model Configs
  async getLLMModelConfigs(): Promise<LLMModelConfig[]> {
    // Stub - not implemented in Rust backend yet
    console.warn('[SqliteService] getLLMModelConfigs not implemented');
    return [];
  }

  async saveLLMModelConfig(config: LLMModelConfig): Promise<{ success: boolean; message: string }> {
    console.warn('[SqliteService] saveLLMModelConfig not implemented');
    return { success: false, message: 'Not implemented' };
  }

  async deleteLLMModelConfig(id: string): Promise<{ success: boolean; message: string }> {
    console.warn('[SqliteService] deleteLLMModelConfig not implemented');
    return { success: false, message: 'Not implemented' };
  }

  async toggleLLMModelConfigEnabled(id: string): Promise<{ success: boolean; message: string }> {
    console.warn('[SqliteService] toggleLLMModelConfigEnabled not implemented');
    return { success: false, message: 'Not implemented' };
  }

  async setActiveLLMProvider(provider: string): Promise<void> {
    console.warn('[SqliteService] setActiveLLMProvider not implemented');
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

  async getCachedCategoryData(category: string, maxAgeMinutes: number): Promise<string | null> {
    return await getCachedMarketData('*', category, maxAgeMinutes);
  }

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

  // Cache
  saveMarketDataCache = saveMarketDataCache;
  getCachedMarketData = getCachedMarketData;
  clearMarketDataCache = clearMarketDataCache;
  getCachedForumCategories = getCachedForumCategories;
  cacheForumCategories = cacheForumCategories;
  getCachedForumPosts = getCachedForumPosts;
  cacheForumPosts = cacheForumPosts;
  getCachedForumStats = getCachedForumStats;
  cacheForumStats = cacheForumStats;

  // Paper Trading
  createPortfolio = createPortfolio;
  getPortfolio = getPortfolio;
  listPortfolios = listPortfolios;
  updatePortfolioBalance = updatePortfolioBalance;
  createPosition = createPosition;
  getPortfolioPositions = getPortfolioPositions;
  createOrder = createOrder;
  getPortfolioOrders = getPortfolioOrders;
  getPortfolioTrades = getPortfolioTrades;

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
