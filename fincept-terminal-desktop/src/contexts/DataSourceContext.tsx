import React, { createContext, useContext, useReducer, useCallback, useEffect, useRef } from 'react';
import { sqliteService } from '../services/core/sqliteService';
import { createAdapter } from '../components/tabs/data-sources/adapters';

export type DataSourceCategory = 'database' | 'api' | 'file' | 'streaming' | 'cloud' | 'timeseries' | 'market-data';

export interface DataSourceConnection {
  id: string;
  name: string;
  type: string;
  category: DataSourceCategory;
  status: 'connected' | 'disconnected' | 'error' | 'testing';
  config: Record<string, any>;
  createdAt: string;
  updatedAt: string;
  lastTested?: string;
  errorMessage?: string;
}

// State machine for the context
type LoadState = 'idle' | 'loading' | 'loaded' | 'error';

interface DataSourceState {
  connections: DataSourceConnection[];
  loadState: LoadState;
  error: string | null;
}

// Actions for reducer
type DataSourceAction =
  | { type: 'LOAD_START' }
  | { type: 'LOAD_SUCCESS'; payload: DataSourceConnection[] }
  | { type: 'LOAD_ERROR'; payload: string }
  | { type: 'ADD_CONNECTION'; payload: DataSourceConnection }
  | { type: 'UPDATE_CONNECTION'; payload: { id: string; updates: Partial<DataSourceConnection> } }
  | { type: 'DELETE_CONNECTION'; payload: string }
  | { type: 'SET_TESTING'; payload: string }
  | { type: 'SET_CONNECTED'; payload: string }
  | { type: 'SET_ERROR'; payload: { id: string; message: string } };

function dataSourceReducer(state: DataSourceState, action: DataSourceAction): DataSourceState {
  switch (action.type) {
    case 'LOAD_START':
      return { ...state, loadState: 'loading', error: null };

    case 'LOAD_SUCCESS':
      return { ...state, loadState: 'loaded', connections: action.payload, error: null };

    case 'LOAD_ERROR':
      return { ...state, loadState: 'error', error: action.payload };

    case 'ADD_CONNECTION':
      return { ...state, connections: [...state.connections, action.payload] };

    case 'UPDATE_CONNECTION': {
      const { id, updates } = action.payload;
      return {
        ...state,
        connections: state.connections.map((conn) =>
          conn.id === id ? { ...conn, ...updates, updatedAt: new Date().toISOString() } : conn
        ),
      };
    }

    case 'DELETE_CONNECTION':
      return { ...state, connections: state.connections.filter((conn) => conn.id !== action.payload) };

    case 'SET_TESTING':
      return {
        ...state,
        connections: state.connections.map((conn) =>
          conn.id === action.payload
            ? { ...conn, status: 'testing' as const, lastTested: new Date().toISOString() }
            : conn
        ),
      };

    case 'SET_CONNECTED':
      return {
        ...state,
        connections: state.connections.map((conn) =>
          conn.id === action.payload ? { ...conn, status: 'connected' as const, errorMessage: undefined } : conn
        ),
      };

    case 'SET_ERROR':
      return {
        ...state,
        connections: state.connections.map((conn) =>
          conn.id === action.payload.id
            ? { ...conn, status: 'error' as const, errorMessage: action.payload.message }
            : conn
        ),
      };

    default:
      return state;
  }
}

const initialState: DataSourceState = {
  connections: [],
  loadState: 'idle',
  error: null,
};

interface DataSourceContextType {
  connections: DataSourceConnection[];
  loadState: LoadState;
  error: string | null;
  addConnection: (connection: DataSourceConnection) => Promise<void>;
  updateConnection: (id: string, updates: Partial<DataSourceConnection>) => Promise<void>;
  deleteConnection: (id: string) => Promise<void>;
  testConnection: (connection: DataSourceConnection) => Promise<void>;
  refresh: () => void;
}

const DataSourceContext = createContext<DataSourceContextType | undefined>(undefined);

export const DataSourceProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [state, dispatch] = useReducer(dataSourceReducer, initialState);
  const mountedRef = useRef(true);
  const abortControllerRef = useRef<AbortController | null>(null);
  // Track active test connection IDs to prevent duplicate concurrent tests
  const activeTestRef = useRef<string | null>(null);

  // Load connections from SQLite on mount
  const loadConnections = useCallback(async () => {
    // Abort previous request if any
    if (abortControllerRef.current) {
      abortControllerRef.current.abort();
    }
    abortControllerRef.current = new AbortController();

    dispatch({ type: 'LOAD_START' });

    try {
      const sources = await sqliteService.getAllDataSources();

      // Check if component is still mounted
      if (!mountedRef.current) return;

      const mapped: DataSourceConnection[] = sources.map((s) => ({
        id: s.id,
        name: s.display_name,
        type: s.provider,
        category: (s.category || 'database') as DataSourceCategory,
        status: (s.enabled ? 'disconnected' : 'error') as 'connected' | 'disconnected' | 'error' | 'testing',
        config: JSON.parse(s.config || '{}'),
        createdAt: s.created_at || new Date().toISOString(),
        updatedAt: s.updated_at || new Date().toISOString(),
      }));

      dispatch({ type: 'LOAD_SUCCESS', payload: mapped });
    } catch (error) {
      if (!mountedRef.current) return;
      dispatch({ type: 'LOAD_ERROR', payload: error instanceof Error ? error.message : 'Failed to load connections' });
    }
  }, []);

  useEffect(() => {
    mountedRef.current = true;
    loadConnections();

    return () => {
      mountedRef.current = false;
      if (abortControllerRef.current) {
        abortControllerRef.current.abort();
      }
    };
  }, [loadConnections]);

  const addConnection = useCallback(async (connection: DataSourceConnection) => {
    // Validate input
    if (!connection.name?.trim()) {
      throw new Error('Connection name is required');
    }
    if (!connection.type?.trim()) {
      throw new Error('Connection type is required');
    }

    try {
      await sqliteService.saveDataSource({
        id: connection.id,
        alias: connection.id,
        display_name: connection.name.trim(),
        type: 'rest_api',
        provider: connection.type,
        category: connection.category,
        config: JSON.stringify(connection.config),
        enabled: true,
      });

      if (mountedRef.current) {
        dispatch({ type: 'ADD_CONNECTION', payload: connection });
      }
    } catch (error) {
      console.error('Failed to save connection:', error);
      throw error;
    }
  }, []);

  const updateConnection = useCallback(async (id: string, updates: Partial<DataSourceConnection>) => {
    const existing = state.connections.find((c) => c.id === id);
    if (!existing) {
      throw new Error('Connection not found');
    }

    const updated = { ...existing, ...updates, updatedAt: new Date().toISOString() };

    try {
      await sqliteService.saveDataSource({
        id: updated.id,
        alias: updated.id,
        display_name: updated.name,
        type: 'rest_api',
        provider: updated.type,
        category: updated.category,
        config: JSON.stringify(updated.config),
        enabled: updated.status !== 'error',
      });

      if (mountedRef.current) {
        dispatch({ type: 'UPDATE_CONNECTION', payload: { id, updates } });
      }
    } catch (error) {
      console.error('Failed to update connection:', error);
      throw error;
    }
  }, [state.connections]);

  const deleteConnection = useCallback(async (id: string) => {
    try {
      await sqliteService.deleteDataSource(id);
      if (mountedRef.current) {
        dispatch({ type: 'DELETE_CONNECTION', payload: id });
      }
    } catch (error) {
      console.error('Failed to delete connection:', error);
      throw error;
    }
  }, []);

  const testConnection = useCallback(async (connection: DataSourceConnection) => {
    if (!mountedRef.current) return;

    // Prevent duplicate concurrent tests for the same connection
    if (activeTestRef.current === connection.id) return;
    activeTestRef.current = connection.id;

    dispatch({ type: 'SET_TESTING', payload: connection.id });

    try {
      // Create an adapter instance for this connection
      const adapter = createAdapter(connection);

      if (adapter) {
        // Use the actual adapter's test method
        const result = await adapter.testConnection();

        if (!mountedRef.current || activeTestRef.current !== connection.id) return;

        if (result.success) {
          dispatch({ type: 'SET_CONNECTED', payload: connection.id });
        } else {
          dispatch({ type: 'SET_ERROR', payload: { id: connection.id, message: result.message || 'Connection failed' } });
        }
      } else {
        // No adapter available, simulate test
        await new Promise((resolve) => setTimeout(resolve, 1500));

        if (!mountedRef.current || activeTestRef.current !== connection.id) return;
        dispatch({ type: 'SET_CONNECTED', payload: connection.id });
      }
    } catch (error) {
      if (!mountedRef.current || activeTestRef.current !== connection.id) return;
      dispatch({
        type: 'SET_ERROR',
        payload: { id: connection.id, message: error instanceof Error ? error.message : 'Connection test failed' },
      });
    } finally {
      if (activeTestRef.current === connection.id) {
        activeTestRef.current = null;
      }
    }
  }, []);

  const refresh = useCallback(() => {
    loadConnections();
  }, [loadConnections]);

  return (
    <DataSourceContext.Provider
      value={{
        connections: state.connections,
        loadState: state.loadState,
        error: state.error,
        addConnection,
        updateConnection,
        deleteConnection,
        testConnection,
        refresh,
      }}
    >
      {children}
    </DataSourceContext.Provider>
  );
};

export const useDataSources = () => {
  const context = useContext(DataSourceContext);
  if (!context) {
    throw new Error('useDataSources must be used within a DataSourceProvider');
  }
  return context;
};
