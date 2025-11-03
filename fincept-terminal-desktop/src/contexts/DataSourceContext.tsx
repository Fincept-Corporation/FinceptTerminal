import React, { createContext, useContext, useState, useCallback, useEffect } from 'react';
import { sqliteService } from '../services/sqliteService';

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

interface DataSourceContextType {
  connections: DataSourceConnection[];
  addConnection: (connection: DataSourceConnection) => void;
  updateConnection: (id: string, updates: Partial<DataSourceConnection>) => void;
  deleteConnection: (id: string) => void;
  testConnection: (connection: DataSourceConnection) => Promise<void>;
}

const DataSourceContext = createContext<DataSourceContextType | undefined>(undefined);

export const DataSourceProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [connections, setConnections] = useState<DataSourceConnection[]>([]);

  // Load connections from SQLite on mount
  useEffect(() => {
    const loadConnections = async () => {
      try {
        const sources = await sqliteService.getAllDataSources();
        const mapped = sources.map(s => ({
          id: s.id,
          name: s.display_name,
          type: s.provider,
          category: (s.category || 'database') as DataSourceCategory,
          status: (s.enabled ? 'disconnected' : 'error') as 'connected' | 'disconnected' | 'error' | 'testing',
          config: JSON.parse(s.config || '{}'),
          createdAt: s.created_at || new Date().toISOString(),
          updatedAt: s.updated_at || new Date().toISOString(),
        }));
        setConnections(mapped);
      } catch (error) {
        console.error('Failed to load data source connections:', error);
      }
    };
    loadConnections();
  }, []);

  const addConnection = useCallback(async (connection: DataSourceConnection) => {
    try {
      await sqliteService.saveDataSource({
        id: connection.id,
        alias: connection.id,
        display_name: connection.name,
        type: 'rest_api',
        provider: connection.type,
        category: connection.category,
        config: JSON.stringify(connection.config),
        enabled: true,
      });
      setConnections((prev) => [...prev, connection]);
    } catch (error) {
      console.error('Failed to save connection:', error);
    }
  }, []);

  const updateConnection = useCallback(async (id: string, updates: Partial<DataSourceConnection>) => {
    try {
      const existing = connections.find(c => c.id === id);
      if (existing) {
        const updated = { ...existing, ...updates, updatedAt: new Date().toISOString() };
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
        setConnections((prev) =>
          prev.map((conn) => (conn.id === id ? updated : conn))
        );
      }
    } catch (error) {
      console.error('Failed to update connection:', error);
    }
  }, [connections]);

  const deleteConnection = useCallback(async (id: string) => {
    try {
      await sqliteService.deleteDataSource(id);
      setConnections((prev) => prev.filter((conn) => conn.id !== id));
    } catch (error) {
      console.error('Failed to delete connection:', error);
    }
  }, []);

  const testConnection = useCallback(async (connection: DataSourceConnection) => {
    console.log('Testing connection:', connection.id);

    // Simulate connection test
    try {
      setConnections((prev) =>
        prev.map((conn) =>
          conn.id === connection.id
            ? { ...conn, status: 'testing', lastTested: new Date().toISOString() }
            : conn
        )
      );

      // Simulate async test
      await new Promise((resolve) => setTimeout(resolve, 1500));

      // Update to connected status
      setConnections((prev) =>
        prev.map((conn) =>
          conn.id === connection.id
            ? { ...conn, status: 'connected', errorMessage: undefined }
            : conn
        )
      );
    } catch (error) {
      setConnections((prev) =>
        prev.map((conn) =>
          conn.id === connection.id
            ? { ...conn, status: 'error', errorMessage: String(error) }
            : conn
        )
      );
    }
  }, []);

  return (
    <DataSourceContext.Provider
      value={{
        connections,
        addConnection,
        updateConnection,
        deleteConnection,
        testConnection,
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
