// File: src/hooks/usePolymarketWebSocket.ts
// React hook for Polymarket WebSocket real-time data

import { useEffect, useRef, useState, useCallback } from 'react';
import polymarketServiceEnhanced, { WSMarketUpdate, WSUserUpdate } from '@/services/polymarketServiceEnhanced';

interface UseMarketWebSocketOptions {
  onUpdate?: (update: WSMarketUpdate) => void;
  onError?: (error: Event) => void;
  autoConnect?: boolean;
}

export function useMarketWebSocket(options: UseMarketWebSocketOptions = {}) {
  const { onUpdate, onError, autoConnect = true } = options;
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const subscribedMarkets = useRef<Set<string>>(new Set());

  const connect = useCallback(() => {
    try {
      const ws = polymarketServiceEnhanced.connectMarketWebSocket(
        (update) => {
          if (onUpdate) onUpdate(update);
        },
        (err) => {
          setError('WebSocket connection error');
          setIsConnected(false);
          if (onError) onError(err);
        }
      );

      ws.addEventListener('open', () => {
        setIsConnected(true);
        setError(null);
      });

      ws.addEventListener('close', () => {
        setIsConnected(false);
      });

      wsRef.current = ws;
    } catch (err) {
      setError('Failed to connect to WebSocket');
      console.error('WebSocket connection error:', err);
    }
  }, [onUpdate, onError]);

  const disconnect = useCallback(() => {
    polymarketServiceEnhanced.disconnectWebSocket('market');
    wsRef.current = null;
    setIsConnected(false);
    subscribedMarkets.current.clear();
  }, []);

  const subscribeToMarket = useCallback((marketId: string) => {
    if (!isConnected) {
      console.warn('Cannot subscribe: WebSocket not connected');
      return;
    }
    polymarketServiceEnhanced.subscribeToMarket(marketId);
    subscribedMarkets.current.add(marketId);
  }, [isConnected]);

  const unsubscribeFromMarket = useCallback((marketId: string) => {
    if (!isConnected) return;
    polymarketServiceEnhanced.unsubscribeFromMarket(marketId);
    subscribedMarkets.current.delete(marketId);
  }, [isConnected]);

  useEffect(() => {
    if (autoConnect) {
      connect();
    }

    return () => {
      disconnect();
    };
  }, [autoConnect, connect, disconnect]);

  return {
    isConnected,
    error,
    connect,
    disconnect,
    subscribeToMarket,
    unsubscribeFromMarket,
    subscribedMarkets: Array.from(subscribedMarkets.current)
  };
}

interface UseUserWebSocketOptions {
  onUpdate?: (update: WSUserUpdate) => void;
  onError?: (error: Event) => void;
  autoConnect?: boolean;
}

export function useUserWebSocket(options: UseUserWebSocketOptions = {}) {
  const { onUpdate, onError, autoConnect = false } = options;
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const wsRef = useRef<WebSocket | null>(null);

  const connect = useCallback(() => {
    try {
      const ws = polymarketServiceEnhanced.connectUserWebSocket(
        (update) => {
          if (onUpdate) onUpdate(update);
        },
        (err) => {
          setError('WebSocket connection error');
          setIsConnected(false);
          if (onError) onError(err);
        }
      );

      ws.addEventListener('open', () => {
        setIsConnected(true);
        setError(null);
      });

      ws.addEventListener('close', () => {
        setIsConnected(false);
      });

      wsRef.current = ws;
    } catch (err) {
      setError('Failed to connect to WebSocket. Please ensure you are authenticated.');
      console.error('WebSocket connection error:', err);
    }
  }, [onUpdate, onError]);

  const disconnect = useCallback(() => {
    polymarketServiceEnhanced.disconnectWebSocket('user');
    wsRef.current = null;
    setIsConnected(false);
  }, []);

  useEffect(() => {
    if (autoConnect) {
      connect();
    }

    return () => {
      disconnect();
    };
  }, [autoConnect, connect, disconnect]);

  return {
    isConnected,
    error,
    connect,
    disconnect
  };
}
