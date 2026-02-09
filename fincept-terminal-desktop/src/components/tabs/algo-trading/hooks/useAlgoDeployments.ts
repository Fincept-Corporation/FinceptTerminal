import { useState, useCallback, useEffect, useRef } from 'react';
import type { AlgoDeployment } from '../types';
import { listAlgoDeployments, stopAlgoDeployment, stopAllAlgoDeployments } from '../services/algoTradingService';

const POLL_INTERVAL = 5000;

export function useAlgoDeployments() {
  const [deployments, setDeployments] = useState<AlgoDeployment[]>([]);
  const [loading, setLoading] = useState(true);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  const refresh = useCallback(async () => {
    const result = await listAlgoDeployments();
    if (result.success && result.data) {
      setDeployments(result.data);
    }
    setLoading(false);
  }, []);

  useEffect(() => {
    refresh();
    intervalRef.current = setInterval(refresh, POLL_INTERVAL);
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [refresh]);

  const stop = useCallback(async (deployId: string) => {
    const result = await stopAlgoDeployment(deployId);
    if (result.success) {
      await refresh();
    }
    return result;
  }, [refresh]);

  const stopAll = useCallback(async () => {
    const result = await stopAllAlgoDeployments();
    if (result.success) {
      await refresh();
    }
    return result;
  }, [refresh]);

  const running = deployments.filter(d => d.status === 'running');
  const stopped = deployments.filter(d => d.status !== 'running');

  return { deployments, running, stopped, loading, refresh, stop, stopAll };
}
