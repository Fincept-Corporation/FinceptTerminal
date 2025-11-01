// Universal Data Source Hook
// Use this hook to access any data source (WebSocket or REST API) by alias

import { useState, useEffect, useRef } from 'react';
import { getDataSourceByAlias, getWebSocketTopic, parseDataSourceConfig, WebSocketSourceConfig, RestAPISourceConfig } from '../services/dataSourceRegistry';
import { DataSource } from '../services/sqliteService';
import { useWebSocket } from './useWebSocket';
import { mappingDatabase } from '../components/tabs/data-mapping/services/MappingDatabase';
import { mappingEngine } from '../components/tabs/data-mapping/engine/MappingEngine';

export interface DataSourceHookResult {
  data: any;
  error: string | null;
  loading: boolean;
  connected: boolean;
  source: DataSource | null;
  refresh: () => Promise<void>;
}

/**
 * Universal hook to access any data source by alias
 * Works with both WebSocket and REST API sources
 *
 * @example
 * ```tsx
 * // WebSocket source
 * const { data, connected } = useDataSource('btc_live');
 *
 * // REST API source
 * const { data, loading, refresh } = useDataSource('sp500_data');
 * ```
 */
export function useDataSource(alias: string, parameters?: Record<string, any>): DataSourceHookResult {
  const [source, setSource] = useState<DataSource | null>(null);
  const [data, setData] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState<boolean>(true);
  const [connected, setConnected] = useState<boolean>(false);

  const wsTopicRef = useRef<string | null>(null);
  const refreshIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // WebSocket hook (conditionally used)
  const wsResult = useWebSocket(
    source?.type === 'websocket' && source?.enabled ? (wsTopicRef.current || '') : null,
    null,
    {
      autoSubscribe: source?.type === 'websocket' && source?.enabled === true
    }
  );

  // Load data source on mount or alias change
  useEffect(() => {
    loadDataSource();

    return () => {
      // Cleanup
      if (refreshIntervalRef.current) {
        clearInterval(refreshIntervalRef.current);
      }
    };
  }, [alias]);

  // Handle WebSocket data updates
  useEffect(() => {
    if (source?.type === 'websocket' && wsResult.message) {
      setData(wsResult.message.data);
      setConnected(wsResult.status === 'connected');
      setError(null);
      setLoading(false);
    }
  }, [wsResult.message, wsResult.status, source?.type]);

  async function loadDataSource() {
    try {
      setLoading(true);
      setError(null);

      // Fetch data source from database
      const dataSource = await getDataSourceByAlias(alias);

      if (!dataSource) {
        setError(`Data source '${alias}' not found`);
        setLoading(false);
        return;
      }

      if (!dataSource.enabled) {
        setError(`Data source '${alias}' is disabled`);
        setLoading(false);
        return;
      }

      setSource(dataSource);

      // Handle based on type
      if (dataSource.type === 'websocket') {
        await handleWebSocketSource(dataSource);
      } else if (dataSource.type === 'rest_api') {
        await handleRestAPISource(dataSource);
      }
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : String(err);
      setError(errorMsg);
      setLoading(false);
    }
  }

  async function handleWebSocketSource(dataSource: DataSource) {
    try {
      const topic = getWebSocketTopic(dataSource);

      if (!topic) {
        throw new Error('Invalid WebSocket configuration');
      }

      // Ensure provider config is set in WebSocket Manager
      const config = parseDataSourceConfig(dataSource) as WebSocketSourceConfig;
      if (config) {
        const { getWebSocketManager } = await import('../services/websocket');
        const wsManager = getWebSocketManager();

        const existingConfig = wsManager.getProviderConfig(config.provider);
        if (!existingConfig) {
          wsManager.setProviderConfig(config.provider, {
            provider_name: config.provider,
            enabled: true,
            api_key: config.params?.api_key,
            api_secret: config.params?.api_secret,
            endpoint: config.params?.endpoint
          });
          console.log(`[useDataSource] Provider config set for: ${config.provider}`);
        }
      }

      wsTopicRef.current = topic;
      setLoading(false);
      // Data will come through wsResult
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : String(err);
      setError(errorMsg);
      setLoading(false);
    }
  }

  async function handleRestAPISource(dataSource: DataSource) {
    try {
      const config = parseDataSourceConfig(dataSource) as RestAPISourceConfig;

      if (!config || !config.mappingId) {
        throw new Error('Invalid REST API configuration');
      }

      // Load mapping from database
      const mapping = await mappingDatabase.getMapping(config.mappingId);

      if (!mapping) {
        throw new Error(`Mapping '${config.mappingId}' not found`);
      }

      // Execute mapping
      const result = await mappingEngine.execute({
        mapping,
        parameters: parameters || config.parameters
      });

      if (result.success) {
        setData(result.data);
        setConnected(true);
        setError(null);
      } else {
        throw new Error(result.errors?.join(', ') || 'Mapping execution failed');
      }

      setLoading(false);
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : String(err);
      setError(errorMsg);
      setLoading(false);
    }
  }

  async function refresh() {
    if (!source) return;

    if (source.type === 'rest_api') {
      await handleRestAPISource(source);
    }
    // WebSocket sources refresh automatically
  }

  return {
    data,
    error,
    loading,
    connected,
    source,
    refresh
  };
}

/**
 * Hook to get all available data sources
 */
export function useDataSources() {
  const [sources, setSources] = useState<DataSource[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadSources();
  }, []);

  async function loadSources() {
    try {
      setLoading(true);
      const { sqliteService } = await import('../services/sqliteService');
      const allSources = await sqliteService.getAllDataSources();
      setSources(allSources);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setLoading(false);
    }
  }

  return {
    sources,
    loading,
    error,
    reload: loadSources
  };
}

/**
 * Hook to get data sources by type
 */
export function useDataSourcesByType(type: 'websocket' | 'rest_api') {
  const [sources, setSources] = useState<DataSource[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadSourcesByType();
  }, [type]);

  async function loadSourcesByType() {
    try {
      setLoading(true);
      const { sqliteService } = await import('../services/sqliteService');
      const filtered = await sqliteService.getDataSourcesByType(type);
      setSources(filtered);
    } catch (err) {
      console.error('Failed to load data sources:', err);
    } finally {
      setLoading(false);
    }
  }

  return {
    sources,
    loading,
    reload: loadSourcesByType
  };
}
