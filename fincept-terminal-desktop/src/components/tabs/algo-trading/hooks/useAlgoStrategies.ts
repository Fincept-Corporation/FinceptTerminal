import { useState, useCallback, useEffect } from 'react';
import type { AlgoStrategy } from '../types';
import { listAlgoStrategies, getAlgoStrategy, saveAlgoStrategy, deleteAlgoStrategy } from '../services/algoTradingService';

export function useAlgoStrategies() {
  const [strategies, setStrategies] = useState<AlgoStrategy[]>([]);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    setLoading(true);
    const result = await listAlgoStrategies();
    if (result.success && result.data) {
      setStrategies(result.data);
    }
    setLoading(false);
  }, []);

  useEffect(() => {
    refresh();
  }, [refresh]);

  const get = useCallback(async (id: string) => {
    const result = await getAlgoStrategy(id);
    return result.success ? result.data ?? null : null;
  }, []);

  const save = useCallback(async (strategy: Parameters<typeof saveAlgoStrategy>[0]) => {
    const result = await saveAlgoStrategy(strategy);
    if (result.success) {
      await refresh();
    }
    return result;
  }, [refresh]);

  const remove = useCallback(async (id: string) => {
    const result = await deleteAlgoStrategy(id);
    if (result.success) {
      setStrategies(prev => prev.filter(s => s.id !== id));
    }
    return result;
  }, []);

  return { strategies, loading, refresh, get, save, remove };
}
