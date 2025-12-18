/**
 * SQLite Service - Main Entry Point
 * 
 * This module provides a modular SQLite database service.
 * The service is split into domain-specific modules for maintainability.
 * 
 * Usage:
 *   import { sqliteService } from './services/sqlite';
 *   // or import specific types
 *   import { Credential, ChatSession } from './services/sqlite';
 */

// Re-export all types
export * from './types';

// Re-export base class
export { SQLiteBase } from './SQLiteBase';

// Re-export modules
export * from './modules';

// Import for internal use
import { SQLiteBase } from './SQLiteBase';
import {
    SettingsModule,
    CredentialsModule,
    LLMConfigModule,
    ChatModule,
    DataSourceModule,
    WSProviderModule,
    MCPServerModule,
    CacheModule,
    ContextRecorderModule,
    BacktestingModule,
} from './modules';

// Import types for the facade
import type {
    Credential,
    ApiKeys,
    Setting,
    LLMConfig,
    LLMGlobalSettings,
    ChatSession,
    ChatMessage,
    DataSource,
    WSProviderConfig,
    MCPServer,
    RecordedContext,
    RecordingSession,
    BacktestingProvider,
    BacktestingStrategy,
    BacktestRun,
    OptimizationRun,
    OperationResult,
    OperationResultWithId,
    ToggleResult,
} from './types';

import type { DataSourceConnection } from './modules/CacheModule';

/**
 * SQLiteServiceFacade
 * 
 * Provides a unified API that delegates to domain-specific modules.
 * This maintains backwards compatibility with the original sqliteService.
 */
class SQLiteServiceFacade {
    private base: SQLiteBase;

    // Domain modules
    private _settings: SettingsModule;
    private _credentials: CredentialsModule;
    private _llmConfig: LLMConfigModule;
    private _chat: ChatModule;
    private _dataSources: DataSourceModule;
    private _wsProviders: WSProviderModule;
    private _mcpServers: MCPServerModule;
    private _cache: CacheModule;
    private _contextRecorder: ContextRecorderModule;
    private _backtesting: BacktestingModule;

    constructor() {
        this.base = new SQLiteBase();

        // Initialize modules with base
        this._settings = new SettingsModule(this.base);
        this._credentials = new CredentialsModule(this.base, this._settings);
        this._llmConfig = new LLMConfigModule(this.base);
        this._chat = new ChatModule(this.base);
        this._dataSources = new DataSourceModule(this.base);
        this._wsProviders = new WSProviderModule(this.base);
        this._mcpServers = new MCPServerModule(this.base);
        this._cache = new CacheModule(this.base);
        this._contextRecorder = new ContextRecorderModule(this.base);
        this._backtesting = new BacktestingModule(this.base);
    }

    // =========================
    // Core Methods
    // =========================

    async initialize(): Promise<void> {
        return this.base.initialize();
    }

    async ensureInitialized(): Promise<void> {
        return this.base.ensureInitialized();
    }

    isReady(): boolean {
        return this.base.isReady();
    }

    async execute(sql: string, params?: any[]): Promise<any> {
        return this.base.execute(sql, params);
    }

    async select<T>(sql: string, params?: any[]): Promise<T> {
        return this.base.select<T>(sql, params);
    }

    async healthCheck(): Promise<{ healthy: boolean; message: string }> {
        return this.base.healthCheck();
    }

    // =========================
    // Settings Methods
    // =========================

    async saveSetting(key: string, value: string, category?: string): Promise<void> {
        return this._settings.saveSetting(key, value, category);
    }

    async getSetting(key: string): Promise<string | null> {
        return this._settings.getSetting(key);
    }

    async getSettingsByCategory(category: string): Promise<Setting[]> {
        return this._settings.getSettingsByCategory(category);
    }

    async getAllSettings(): Promise<Setting[]> {
        return this._settings.getAllSettings();
    }

    // =========================
    // Credentials Methods
    // =========================

    async saveCredential(credential: Omit<Credential, 'id' | 'created_at' | 'updated_at'>): Promise<OperationResult> {
        return this._credentials.saveCredential(credential);
    }

    async getCredentials(): Promise<Credential[]> {
        return this._credentials.getCredentials();
    }

    async getCredentialByService(serviceName: string): Promise<Credential | null> {
        return this._credentials.getCredentialByService(serviceName);
    }

    async getApiKey(serviceName: string): Promise<string | null> {
        return this._credentials.getApiKey(serviceName);
    }

    async setApiKey(keyName: string, value: string): Promise<void> {
        return this._credentials.setApiKey(keyName, value);
    }

    async getAllApiKeys(): Promise<ApiKeys> {
        return this._credentials.getAllApiKeys();
    }

    async deleteCredential(id: number): Promise<OperationResult> {
        return this._credentials.deleteCredential(id);
    }

    // =========================
    // LLM Methods
    // =========================

    async getLLMConfigs(): Promise<LLMConfig[]> {
        return this._llmConfig.getLLMConfigs();
    }

    async getLLMConfig(provider: string): Promise<LLMConfig | null> {
        return this._llmConfig.getLLMConfig(provider);
    }

    async getActiveLLMConfig(): Promise<LLMConfig | null> {
        return this._llmConfig.getActiveLLMConfig();
    }

    async saveLLMConfig(config: Omit<LLMConfig, 'created_at' | 'updated_at'>): Promise<void> {
        return this._llmConfig.saveLLMConfig(config);
    }

    async setActiveLLMProvider(provider: string): Promise<void> {
        return this._llmConfig.setActiveLLMProvider(provider);
    }

    async ensureDefaultLLMConfigs(): Promise<void> {
        return this._llmConfig.ensureDefaultLLMConfigs();
    }

    async getLLMGlobalSettings(): Promise<LLMGlobalSettings> {
        return this._llmConfig.getLLMGlobalSettings();
    }

    async saveLLMGlobalSettings(settings: LLMGlobalSettings): Promise<void> {
        return this._llmConfig.saveLLMGlobalSettings(settings);
    }

    // =========================
    // Chat Methods
    // =========================

    async createChatSession(title: string): Promise<ChatSession> {
        return this._chat.createChatSession(title);
    }

    async getChatSessions(limit?: number): Promise<ChatSession[]> {
        return this._chat.getChatSessions(limit);
    }

    async getChatSession(sessionUuid: string): Promise<ChatSession | null> {
        return this._chat.getChatSession(sessionUuid);
    }

    async updateChatSessionTitle(sessionUuid: string, title: string): Promise<void> {
        return this._chat.updateChatSessionTitle(sessionUuid, title);
    }

    async deleteChatSession(sessionUuid: string): Promise<void> {
        return this._chat.deleteChatSession(sessionUuid);
    }

    async addChatMessage(message: Omit<ChatMessage, 'id' | 'timestamp'>): Promise<ChatMessage> {
        return this._chat.addChatMessage(message);
    }

    async getChatMessages(sessionUuid: string): Promise<ChatMessage[]> {
        return this._chat.getChatMessages(sessionUuid);
    }

    async deleteChatMessage(id: string): Promise<void> {
        return this._chat.deleteChatMessage(id);
    }

    async clearChatSessionMessages(sessionUuid: string): Promise<void> {
        return this._chat.clearChatSessionMessages(sessionUuid);
    }

    // =========================
    // Data Sources Methods
    // =========================

    async saveDataSource(source: Omit<DataSource, 'created_at' | 'updated_at'>): Promise<OperationResultWithId> {
        return this._dataSources.saveDataSource(source);
    }

    async getAllDataSources(): Promise<DataSource[]> {
        return this._dataSources.getAllDataSources();
    }

    async getDataSourceById(id: string): Promise<DataSource | null> {
        return this._dataSources.getDataSourceById(id);
    }

    async getDataSourceByAlias(alias: string): Promise<DataSource | null> {
        return this._dataSources.getDataSourceByAlias(alias);
    }

    async getDataSourcesByType(type: 'websocket' | 'rest_api'): Promise<DataSource[]> {
        return this._dataSources.getDataSourcesByType(type);
    }

    async getDataSourcesByProvider(provider: string): Promise<DataSource[]> {
        return this._dataSources.getDataSourcesByProvider(provider);
    }

    async getEnabledDataSources(): Promise<DataSource[]> {
        return this._dataSources.getEnabledDataSources();
    }

    async toggleDataSourceEnabled(id: string): Promise<ToggleResult> {
        return this._dataSources.toggleDataSourceEnabled(id);
    }

    async deleteDataSource(id: string): Promise<OperationResult> {
        return this._dataSources.deleteDataSource(id);
    }

    async searchDataSources(query: string): Promise<DataSource[]> {
        return this._dataSources.searchDataSources(query);
    }

    // =========================
    // WS Provider Methods
    // =========================

    async saveWSProviderConfig(config: Omit<WSProviderConfig, 'id' | 'created_at' | 'updated_at'>): Promise<OperationResult> {
        return this._wsProviders.saveWSProviderConfig(config);
    }

    async getWSProviderConfigs(): Promise<WSProviderConfig[]> {
        return this._wsProviders.getWSProviderConfigs();
    }

    async getWSProviderConfig(providerName: string): Promise<WSProviderConfig | null> {
        return this._wsProviders.getWSProviderConfig(providerName);
    }

    async getEnabledWSProviderConfigs(): Promise<WSProviderConfig[]> {
        return this._wsProviders.getEnabledWSProviderConfigs();
    }

    async toggleWSProviderEnabled(providerName: string): Promise<ToggleResult> {
        return this._wsProviders.toggleWSProviderEnabled(providerName);
    }

    async deleteWSProviderConfig(providerName: string): Promise<OperationResult> {
        return this._wsProviders.deleteWSProviderConfig(providerName);
    }

    // =========================
    // MCP Server Methods
    // =========================

    async addMCPServer(server: Omit<MCPServer, 'created_at' | 'updated_at'>): Promise<void> {
        return this._mcpServers.addMCPServer(server);
    }

    async getMCPServers(): Promise<MCPServer[]> {
        return this._mcpServers.getMCPServers();
    }

    async getMCPServer(id: string): Promise<MCPServer | null> {
        return this._mcpServers.getMCPServer(id);
    }

    async updateMCPServerStatus(id: string, status: string): Promise<void> {
        return this._mcpServers.updateMCPServerStatus(id, status);
    }

    async updateMCPServerConfig(id: string, updates: Partial<MCPServer>): Promise<void> {
        return this._mcpServers.updateMCPServerConfig(id, updates);
    }

    async deleteMCPServer(id: string): Promise<void> {
        return this._mcpServers.deleteMCPServer(id);
    }

    async getAutoStartServers(): Promise<MCPServer[]> {
        return this._mcpServers.getAutoStartServers();
    }

    async cacheMCPTools(serverId: string, tools: any[]): Promise<void> {
        return this._mcpServers.cacheMCPTools(serverId, tools);
    }

    async getMCPServerStats(serverId: string): Promise<{ toolCount: number; callsToday: number; lastUsed: string | null }> {
        return this._mcpServers.getMCPServerStats(serverId);
    }

    async logMCPToolUsage(log: {
        serverId: string;
        toolName: string;
        args: string;
        result: string | null;
        success: boolean;
        executionTime: number;
        error?: string;
    }): Promise<void> {
        return this._mcpServers.logMCPToolUsage(log);
    }

    async getMCPTools(): Promise<any[]> {
        return this._mcpServers.getMCPTools();
    }

    async getMCPToolsByServer(serverId: string): Promise<any[]> {
        return this._mcpServers.getMCPToolsByServer(serverId);
    }

    // =========================
    // Cache Methods
    // =========================

    async saveMarketDataCache(symbol: string, category: string, quoteData: any): Promise<void> {
        return this._cache.saveMarketDataCache(symbol, category, quoteData);
    }

    async getCachedMarketData(symbol: string, category: string, maxAgeMinutes?: number): Promise<any | null> {
        return this._cache.getCachedMarketData(symbol, category, maxAgeMinutes);
    }

    async clearMarketDataCache(): Promise<void> {
        return this._cache.clearMarketDataCache();
    }

    async clearExpiredMarketCache(maxAgeMinutes?: number): Promise<void> {
        return this._cache.clearExpiredMarketCache(maxAgeMinutes);
    }

    async getCachedCategoryData(category: string, symbols: string[], maxAgeMinutes?: number): Promise<Map<string, any>> {
        return this._cache.getCachedCategoryData(category, symbols, maxAgeMinutes);
    }

    async saveDataSourceConnection(connection: DataSourceConnection): Promise<void> {
        return this._cache.saveDataSourceConnection(connection);
    }

    async getDataSourceConnection(id: string): Promise<DataSourceConnection | null> {
        return this._cache.getDataSourceConnection(id);
    }

    async getAllDataSourceConnections(): Promise<DataSourceConnection[]> {
        return this._cache.getAllDataSourceConnections();
    }

    async getDataSourceConnectionsByCategory(category: string): Promise<DataSourceConnection[]> {
        return this._cache.getDataSourceConnectionsByCategory(category);
    }

    async getDataSourceConnectionsByType(type: string): Promise<DataSourceConnection[]> {
        return this._cache.getDataSourceConnectionsByType(type);
    }

    async updateDataSourceConnection(id: string, updates: Partial<DataSourceConnection>): Promise<void> {
        return this._cache.updateDataSourceConnection(id, updates);
    }

    async deleteDataSourceConnection(id: string): Promise<void> {
        return this._cache.deleteDataSourceConnection(id);
    }

    async cacheForumCategories(categories: any[]): Promise<void> {
        return this._cache.cacheForumCategories(categories);
    }

    async getCachedForumCategories(maxAgeMinutes?: number): Promise<any[] | null> {
        return this._cache.getCachedForumCategories(maxAgeMinutes);
    }

    async cacheForumPosts(categoryId: number | null, sortBy: string, posts: any[]): Promise<void> {
        return this._cache.cacheForumPosts(categoryId, sortBy, posts);
    }

    async getCachedForumPosts(categoryId: number | null, sortBy: string, maxAgeMinutes?: number): Promise<any[] | null> {
        return this._cache.getCachedForumPosts(categoryId, sortBy, maxAgeMinutes);
    }

    async cacheForumStats(stats: any): Promise<void> {
        return this._cache.cacheForumStats(stats);
    }

    async getCachedForumStats(maxAgeMinutes?: number): Promise<any | null> {
        return this._cache.getCachedForumStats(maxAgeMinutes);
    }

    async cacheForumPostDetails(postUuid: string, post: any, comments: any[]): Promise<void> {
        return this._cache.cacheForumPostDetails(postUuid, post, comments);
    }

    async getCachedForumPostDetails(postUuid: string, maxAgeMinutes?: number): Promise<{ post: any; comments: any[] } | null> {
        return this._cache.getCachedForumPostDetails(postUuid, maxAgeMinutes);
    }

    async clearForumCache(): Promise<void> {
        return this._cache.clearForumCache();
    }

    async clearExpiredForumCache(maxAgeMinutes?: number): Promise<void> {
        return this._cache.clearExpiredForumCache(maxAgeMinutes);
    }

    // =========================
    // Context Recorder Methods
    // =========================

    async saveRecordedContext(context: Omit<RecordedContext, 'created_at'>): Promise<void> {
        return this._contextRecorder.saveRecordedContext(context);
    }

    async getRecordedContexts(filters?: {
        tabName?: string;
        dataType?: string;
        tags?: string[];
        limit?: number;
    }): Promise<RecordedContext[]> {
        return this._contextRecorder.getRecordedContexts(filters);
    }

    async getRecordedContext(id: string): Promise<RecordedContext | null> {
        return this._contextRecorder.getRecordedContext(id);
    }

    async deleteRecordedContext(id: string): Promise<void> {
        return this._contextRecorder.deleteRecordedContext(id);
    }

    async clearOldContexts(maxAgeDays: number): Promise<void> {
        return this._contextRecorder.clearOldContexts(maxAgeDays);
    }

    async getContextStorageStats(): Promise<{ count: number; totalSize: number }> {
        return this._contextRecorder.getContextStorageStats();
    }

    async startRecordingSession(session: Omit<RecordingSession, 'started_at'>): Promise<void> {
        return this._contextRecorder.startRecordingSession(session);
    }

    async getActiveRecordingSession(tabName: string): Promise<RecordingSession | null> {
        return this._contextRecorder.getActiveRecordingSession(tabName);
    }

    async stopRecordingSession(tabName: string): Promise<void> {
        return this._contextRecorder.stopRecordingSession(tabName);
    }

    async linkContextToChat(chatSessionUuid: string, contextId: string): Promise<void> {
        return this._contextRecorder.linkContextToChat(chatSessionUuid, contextId);
    }

    async unlinkContextFromChat(chatSessionUuid: string, contextId: string): Promise<void> {
        return this._contextRecorder.unlinkContextFromChat(chatSessionUuid, contextId);
    }

    async getChatContexts(chatSessionUuid: string): Promise<RecordedContext[]> {
        return this._contextRecorder.getChatContexts(chatSessionUuid);
    }

    async toggleChatContextActive(chatSessionUuid: string, contextId: string): Promise<void> {
        return this._contextRecorder.toggleChatContextActive(chatSessionUuid, contextId);
    }

    // =========================
    // Backtesting Methods
    // =========================

    async saveBacktestingProvider(provider: Omit<BacktestingProvider, 'created_at' | 'updated_at'>): Promise<OperationResult> {
        return this._backtesting.saveBacktestingProvider(provider);
    }

    async getBacktestingProviders(): Promise<BacktestingProvider[]> {
        return this._backtesting.getBacktestingProviders();
    }

    async getBacktestingProvider(name: string): Promise<BacktestingProvider | null> {
        return this._backtesting.getBacktestingProvider(name);
    }

    async getActiveBacktestingProvider(): Promise<BacktestingProvider | null> {
        return this._backtesting.getActiveBacktestingProvider();
    }

    async setActiveBacktestingProvider(name: string): Promise<OperationResult> {
        return this._backtesting.setActiveBacktestingProvider(name);
    }

    async deleteBacktestingProvider(name: string): Promise<OperationResult> {
        return this._backtesting.deleteBacktestingProvider(name);
    }

    async saveBacktestingStrategy(strategy: Omit<BacktestingStrategy, 'created_at' | 'updated_at'>): Promise<OperationResult> {
        return this._backtesting.saveBacktestingStrategy(strategy);
    }

    async getBacktestingStrategies(): Promise<BacktestingStrategy[]> {
        return this._backtesting.getBacktestingStrategies();
    }

    async getBacktestingStrategy(id: string): Promise<BacktestingStrategy | null> {
        return this._backtesting.getBacktestingStrategy(id);
    }

    async getBacktestingStrategiesByProvider(providerType: string): Promise<BacktestingStrategy[]> {
        return this._backtesting.getBacktestingStrategiesByProvider(providerType);
    }

    async deleteBacktestingStrategy(id: string): Promise<OperationResult> {
        return this._backtesting.deleteBacktestingStrategy(id);
    }

    async saveBacktestRun(run: Omit<BacktestRun, 'created_at' | 'completed_at' | 'duration_seconds'>): Promise<OperationResult> {
        return this._backtesting.saveBacktestRun(run);
    }

    async updateBacktestRun(id: string, updates: Partial<BacktestRun>): Promise<OperationResult> {
        return this._backtesting.updateBacktestRun(id, updates);
    }

    async getBacktestRuns(limit?: number): Promise<BacktestRun[]> {
        return this._backtesting.getBacktestRuns(limit);
    }

    async getBacktestRun(id: string): Promise<BacktestRun | null> {
        return this._backtesting.getBacktestRun(id);
    }

    async getBacktestRunsByStrategy(strategyId: string): Promise<BacktestRun[]> {
        return this._backtesting.getBacktestRunsByStrategy(strategyId);
    }

    async getBacktestRunsByProvider(providerName: string): Promise<BacktestRun[]> {
        return this._backtesting.getBacktestRunsByProvider(providerName);
    }

    async deleteBacktestRun(id: string): Promise<OperationResult> {
        return this._backtesting.deleteBacktestRun(id);
    }

    async saveOptimizationRun(run: Omit<OptimizationRun, 'created_at' | 'completed_at' | 'duration_seconds'>): Promise<OperationResult> {
        return this._backtesting.saveOptimizationRun(run);
    }

    async updateOptimizationRun(id: string, updates: Partial<OptimizationRun>): Promise<OperationResult> {
        return this._backtesting.updateOptimizationRun(id, updates);
    }

    async getOptimizationRuns(limit?: number): Promise<OptimizationRun[]> {
        return this._backtesting.getOptimizationRuns(limit);
    }

    async getOptimizationRun(id: string): Promise<OptimizationRun | null> {
        return this._backtesting.getOptimizationRun(id);
    }

    async deleteOptimizationRun(id: string): Promise<OperationResult> {
        return this._backtesting.deleteOptimizationRun(id);
    }
}

// Export singleton instance (backwards compatible)
export const sqliteService = new SQLiteServiceFacade();
export default sqliteService;

