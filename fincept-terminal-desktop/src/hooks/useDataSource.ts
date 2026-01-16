/**
 * useDataSource Hook
 * React hook for managing data source connections and subscriptions
 */

import { useState, useEffect, useCallback } from 'react';
import { getAllDataSources } from '@/services/data-sources/dataSourceRegistry';
import { DataSource } from '@/services/core/sqliteService';

interface UseDataSourceReturn {
  dataSources: DataSource[];
  loading: boolean;
  error: string | null;
  refreshDataSources: () => Promise<void>;
}

export function useDataSource(): UseDataSourceReturn {
  const [dataSources, setDataSources] = useState<DataSource[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const refreshDataSources = useCallback(async () => {
    try {
      setLoading(true);
      setError(null);
      const sources = await getAllDataSources();
      setDataSources(sources);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load data sources');
      console.error('Error loading data sources:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    refreshDataSources();
  }, [refreshDataSources]);

  return {
    dataSources,
    loading,
    error,
    refreshDataSources,
  };
}
