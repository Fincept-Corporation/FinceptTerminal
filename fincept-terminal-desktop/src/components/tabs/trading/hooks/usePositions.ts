/**
 * Position Management Hook
 * Fetches and manages positions across all exchanges
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { UnifiedPosition } from '../types';

export function usePositions(symbol?: string, autoRefresh: boolean = true, refreshInterval: number = 2000) {
  const { activeAdapter, tradingMode, activeBroker } = useBrokerContext();
  const [positions, setPositions] = useState<UnifiedPosition[]>([]);
  // Start with loading false - only set true when actually fetching
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchPositions = useCallback(async () => {
    // Quick early exit if no adapter - don't set loading state
    if (!activeAdapter) {
      setPositions([]);
      setIsLoading(false);
      return;
    }

    // Set loading only if we don't have positions yet
    if (positions.length === 0) {
      setIsLoading(true);
    }
    setError(null);

    try {
      // Fetch positions from adapter (removed timeout - let it complete naturally)
      const rawPositions = await activeAdapter.fetchPositions(symbol ? [symbol] : undefined);

      // Normalize to unified format
      const normalized: UnifiedPosition[] = (rawPositions || []).map((pos: any) => ({
        symbol: pos.symbol,
        side: pos.side, // Use actual side from position, don't infer
        quantity: Math.abs(pos.contracts || pos.amount || 0),
        entryPrice: pos.entryPrice || pos.averagePrice || 0,
        currentPrice: pos.markPrice || pos.currentPrice || 0,
        unrealizedPnl: pos.unrealizedPnl || 0,
        realizedPnl: pos.realizedPnl,
        pnlPercent: pos.percentage || 0,
        leverage: pos.leverage,
        marginMode: pos.marginMode,
        liquidationPrice: pos.liquidationPrice,
        notionalValue: pos.notional || pos.notionalValue || 0,
        marginUsed: pos.collateral || pos.initialMargin,
        openTime: pos.timestamp,
        positionId: pos.id || pos.symbol,
      }));

      setPositions(normalized);
      setError(null); // Clear error on success
    } catch (err) {
      const error = err as Error;
      console.error('[usePositions] Failed to fetch positions:', error);
      setError(error);
      // DON'T clear positions on error - keep existing data
      // This prevents the "0 positions" flash when API fails temporarily
      console.warn('[usePositions] Keeping existing positions due to fetch error');
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol]);

  // Auto-refresh - clear positions when broker changes
  useEffect(() => {
    // Clear positions when switching brokers
    setPositions([]);

    fetchPositions();

    if (!autoRefresh) return;

    const interval = setInterval(fetchPositions, refreshInterval);
    return () => clearInterval(interval);
  }, [fetchPositions, autoRefresh, refreshInterval, activeBroker]);

  return {
    positions,
    isLoading,
    error,
    refresh: fetchPositions,
  };
}

/**
 * Hook to close a position
 */
export function useClosePosition() {
  const { activeAdapter } = useBrokerContext();
  const [isClosing, setIsClosing] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const closePosition = useCallback(
    async (position: UnifiedPosition) => {
      if (!activeAdapter || !activeAdapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      setIsClosing(true);
      setError(null);

      try {
        console.log(`[useClosePosition] Closing ${position.side} position: ${position.quantity} ${position.symbol}`);

        // Create opposite order to close position
        const closeSide = position.side === 'long' ? 'sell' : 'buy';

        await activeAdapter.createOrder(
          position.symbol,
          'market',
          closeSide,
          position.quantity,
          undefined,
          { reduceOnly: true }
        );

        return true;
      } catch (err) {
        const error = err as Error;
        setError(error);
        throw error;
      } finally {
        setIsClosing(false);
      }
    },
    [activeAdapter]
  );

  return { closePosition, isClosing, error };
}
