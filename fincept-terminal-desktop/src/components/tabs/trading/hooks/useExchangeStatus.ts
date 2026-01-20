/**
 * Exchange Status Hook
 * Fetches exchange operational status using fetchStatus
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

export interface ExchangeStatus {
  status: 'ok' | 'maintenance' | 'degraded' | 'offline' | 'unknown';
  updated: number;
  eta?: string; // Estimated time for maintenance to end
  url?: string; // Status page URL
  info?: Record<string, any>;
}

export function useExchangeStatus(autoRefresh: boolean = true, refreshInterval: number = 30000) {
  const { activeAdapter, activeBroker, tradingMode } = useBrokerContext();
  const [status, setStatus] = useState<ExchangeStatus | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchStatus = useCallback(async () => {
    // For paper trading, always show as "ok"
    if (tradingMode === 'paper') {
      setStatus({
        status: 'ok',
        updated: Date.now(),
      });
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setStatus(null);
      return;
    }

    // Check if adapter supports fetchStatus
    if (typeof (activeAdapter as any).fetchStatus !== 'function') {
      setIsSupported(false);
      // Default to 'ok' if not supported but connected
      setStatus({
        status: 'ok',
        updated: Date.now(),
      });
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawStatus = await (activeAdapter as any).fetchStatus();

      // Normalize status response
      let normalizedStatus: 'ok' | 'maintenance' | 'degraded' | 'offline' | 'unknown' = 'unknown';

      if (rawStatus?.status) {
        const statusStr = String(rawStatus.status).toLowerCase();
        if (statusStr === 'ok' || statusStr === 'operational' || statusStr === 'online') {
          normalizedStatus = 'ok';
        } else if (statusStr === 'maintenance' || statusStr === 'scheduled') {
          normalizedStatus = 'maintenance';
        } else if (statusStr === 'degraded' || statusStr === 'partial' || statusStr === 'limited') {
          normalizedStatus = 'degraded';
        } else if (statusStr === 'offline' || statusStr === 'down' || statusStr === 'unavailable') {
          normalizedStatus = 'offline';
        }
      } else {
        // If we got a response but no status field, assume ok
        normalizedStatus = 'ok';
      }

      setStatus({
        status: normalizedStatus,
        updated: rawStatus?.updated || rawStatus?.timestamp || Date.now(),
        eta: rawStatus?.eta,
        url: rawStatus?.url,
        info: rawStatus?.info || rawStatus,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useExchangeStatus] Failed to fetch status:', error);
      setError(error);

      // If error, assume exchange might be having issues
      setStatus({
        status: 'unknown',
        updated: Date.now(),
      });

      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode]);

  // Fetch on mount and when dependencies change
  useEffect(() => {
    fetchStatus();

    if (!autoRefresh) return;

    // Refresh status periodically
    const interval = setInterval(fetchStatus, refreshInterval);
    return () => clearInterval(interval);
  }, [fetchStatus, autoRefresh, refreshInterval, activeBroker]);

  return {
    status,
    isLoading,
    error,
    isSupported,
    refresh: fetchStatus,
  };
}

/**
 * Hook to get exchange server time
 */
export function useExchangeTime() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [serverTime, setServerTime] = useState<number | null>(null);
  const [latency, setLatency] = useState<number | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  const fetchTime = useCallback(async () => {
    if (tradingMode === 'paper') {
      setServerTime(Date.now());
      setLatency(0);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setServerTime(null);
      setLatency(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchTime !== 'function') {
      setServerTime(null);
      setLatency(null);
      return;
    }

    setIsLoading(true);

    try {
      const startTime = Date.now();
      const time = await (activeAdapter as any).fetchTime();
      const endTime = Date.now();

      setServerTime(time);
      setLatency(Math.round((endTime - startTime) / 2)); // Approximate one-way latency
    } catch (err) {
      console.error('[useExchangeTime] Failed to fetch time:', err);
      setServerTime(null);
      setLatency(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode]);

  useEffect(() => {
    fetchTime();
    const interval = setInterval(fetchTime, 10000); // Update every 10s
    return () => clearInterval(interval);
  }, [fetchTime]);

  return {
    serverTime,
    latency,
    isLoading,
    refresh: fetchTime,
  };
}
