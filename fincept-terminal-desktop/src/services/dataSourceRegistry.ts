// Data Source Registry Service
// Central service for managing unified data sources (WebSocket + REST API)

import { sqliteService, DataSource } from './sqliteService';
import { getWebSocketManager } from './websocket';
import { mappingEngine } from '../components/tabs/data-mapping/engine/MappingEngine';
import { DataMappingConfig } from '../components/tabs/data-mapping/types';

export interface WebSocketSourceConfig {
  provider: string;
  channel: string;
  symbol?: string;
  params?: Record<string, any>;
}

export interface RestAPISourceConfig {
  mappingId: string;
  parameters?: Record<string, any>;
}

/**
 * Create a WebSocket data source
 */
export async function createWebSocketDataSource(input: {
  alias: string;
  displayName: string;
  description?: string;
  provider: string;
  channel: string;
  symbol?: string;
  params?: Record<string, any>;
  category?: string;
  tags?: string[];
  enabled?: boolean;
}): Promise<{ success: boolean; message: string; id?: string }> {
  const id = crypto.randomUUID();

  const config: WebSocketSourceConfig = {
    provider: input.provider,
    channel: input.channel,
    symbol: input.symbol,
    params: input.params
  };

  const dataSource: Omit<DataSource, 'created_at' | 'updated_at'> = {
    id,
    alias: input.alias,
    display_name: input.displayName,
    description: input.description,
    type: 'websocket',
    provider: input.provider,
    category: input.category || 'websocket',
    config: JSON.stringify(config),
    enabled: input.enabled !== false,
    tags: input.tags ? JSON.stringify(input.tags) : undefined
  };

  const result = await sqliteService.saveDataSource(dataSource);

  if (result.success && dataSource.enabled) {
    // Auto-initialize in WebSocket Manager
    const wsManager = getWebSocketManager();
    const topic = `${config.provider}.${config.channel}${config.symbol ? '.' + config.symbol : ''}`;

    try {
      // Set provider config if not already set
      const existingConfig = wsManager.getProviderConfig(config.provider);
      if (!existingConfig) {
        wsManager.setProviderConfig(config.provider, {
          provider_name: config.provider,
          enabled: true,
          api_key: input.params?.api_key,
          api_secret: input.params?.api_secret,
          endpoint: input.params?.endpoint
        });
        console.log(`[DataSourceRegistry] Provider config set for: ${config.provider}`);
      }

      // Note: Actual subscription happens when component uses useDataSource
      console.log(`[DataSourceRegistry] WebSocket source created: ${input.alias} -> ${topic}`);
    } catch (error) {
      console.error('[DataSourceRegistry] Failed to initialize WS source:', error);
    }
  }

  return result;
}

/**
 * Create a REST API data source from a data mapping
 */
export async function createRestAPIDataSource(input: {
  alias: string;
  displayName: string;
  description?: string;
  mappingId: string;
  provider?: string;
  category?: string;
  tags?: string[];
  enabled?: boolean;
}): Promise<{ success: boolean; message: string; id?: string }> {
  const id = crypto.randomUUID();

  const config: RestAPISourceConfig = {
    mappingId: input.mappingId
  };

  const dataSource: Omit<DataSource, 'created_at' | 'updated_at'> = {
    id,
    alias: input.alias,
    display_name: input.displayName,
    description: input.description,
    type: 'rest_api',
    provider: input.provider || 'custom',
    category: input.category || 'rest_api',
    config: JSON.stringify(config),
    enabled: input.enabled !== false,
    tags: input.tags ? JSON.stringify(input.tags) : undefined
  };

  return await sqliteService.saveDataSource(dataSource);
}

/**
 * Get all data sources
 */
export async function getAllDataSources(): Promise<DataSource[]> {
  return await sqliteService.getAllDataSources();
}

/**
 * Get active/enabled data sources only
 */
export async function getActiveDataSources(): Promise<DataSource[]> {
  return await sqliteService.getEnabledDataSources();
}

/**
 * Get data source by alias
 */
export async function getDataSourceByAlias(alias: string): Promise<DataSource | null> {
  return await sqliteService.getDataSourceByAlias(alias);
}

/**
 * Get data sources by type
 */
export async function getDataSourcesByType(type: 'websocket' | 'rest_api'): Promise<DataSource[]> {
  return await sqliteService.getDataSourcesByType(type);
}

/**
 * Get data sources by provider
 */
export async function getDataSourcesByProvider(provider: string): Promise<DataSource[]> {
  return await sqliteService.getDataSourcesByProvider(provider);
}

/**
 * Toggle data source
 */
export async function toggleDataSource(id: string): Promise<{ success: boolean; message: string; enabled: boolean }> {
  return await sqliteService.toggleDataSourceEnabled(id);
}

/**
 * Delete data source
 */
export async function deleteDataSource(id: string): Promise<{ success: boolean; message: string }> {
  return await sqliteService.deleteDataSource(id);
}

/**
 * Update data source
 */
export async function updateDataSource(source: DataSource): Promise<{ success: boolean; message: string }> {
  const result = await sqliteService.saveDataSource(source);
  return { success: result.success, message: result.message };
}

/**
 * Search data sources
 */
export async function searchDataSources(query: string): Promise<DataSource[]> {
  return await sqliteService.searchDataSources(query);
}

/**
 * Get WebSocket topic from data source
 */
export function getWebSocketTopic(source: DataSource): string | null {
  if (source.type !== 'websocket') return null;

  try {
    const config: WebSocketSourceConfig = JSON.parse(source.config);
    return `${config.provider}.${config.channel}${config.symbol ? '.' + config.symbol : ''}`;
  } catch (error) {
    console.error('[DataSourceRegistry] Failed to parse WS config:', error);
    return null;
  }
}

/**
 * Get data source configuration (parsed)
 */
export function parseDataSourceConfig(source: DataSource): WebSocketSourceConfig | RestAPISourceConfig | null {
  try {
    return JSON.parse(source.config);
  } catch (error) {
    console.error('[DataSourceRegistry] Failed to parse config:', error);
    return null;
  }
}

/**
 * Get available WebSocket providers from adapters
 */
export function getAvailableWebSocketProviders(): string[] {
  // This would come from the adapter registry
  return ['kraken', 'fyers', 'binance']; // Will be dynamic later
}

/**
 * Get data source statistics
 */
export async function getDataSourceStats(): Promise<{
  total: number;
  websocket: number;
  restApi: number;
  enabled: number;
  disabled: number;
}> {
  const all = await getAllDataSources();

  return {
    total: all.length,
    websocket: all.filter(s => s.type === 'websocket').length,
    restApi: all.filter(s => s.type === 'rest_api').length,
    enabled: all.filter(s => s.enabled).length,
    disabled: all.filter(s => !s.enabled).length
  };
}
