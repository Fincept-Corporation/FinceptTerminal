/**
 * Position Management Hook
 * Fetches and manages positions across all exchanges
 * Uses UnifiedMarketDataService for live price updates
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import { getMarketDataService } from '../../../../services/trading/UnifiedMarketDataService';
import type { UnifiedPosition } from '../types';

export function usePositions(symbol?: string, autoRefresh: boolean = true, refreshInterval: number = 2000) {
  const { activeAdapter, activeBroker } = useBrokerContext();
  const [positions, setPositions] = useState<UnifiedPosition[]>([]);
  // Start with loading false - only set true when actually fetching
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  // Track price subscriptions for cleanup
  const priceUnsubscribersRef = useRef<Map<string, () => void>>(new Map());

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

      // Get market data service for live prices
      const marketDataService = getMarketDataService();

      // Normalize to unified format with live prices from market data service
      const normalized: UnifiedPosition[] = (rawPositions || []).map((pos: any) => {
        // Try to get live price from market data service
        const livePrice = marketDataService.getCurrentPrice(pos.symbol, activeBroker || undefined);
        const currentPrice = livePrice?.last || pos.markPrice || pos.currentPrice || pos.current_price || 0;
        const entryPrice = pos.entryPrice || pos.averagePrice || pos.entry_price || 0;
        const quantity = Math.abs(pos.contracts || pos.amount || pos.quantity || 0);
        const side = pos.side;

        // Calculate P&L with live price
        let unrealizedPnl = pos.unrealizedPnl || pos.unrealized_pnl || 0;
        let pnlPercent = pos.percentage || 0;

        // Recalculate P&L if we have live price and entry price
        if (currentPrice > 0 && entryPrice > 0 && quantity > 0) {
          if (side === 'long') {
            unrealizedPnl = (currentPrice - entryPrice) * quantity;
          } else {
            unrealizedPnl = (entryPrice - currentPrice) * quantity;
          }
          pnlPercent = ((currentPrice - entryPrice) / entryPrice) * 100;
          if (side === 'short') pnlPercent = -pnlPercent;
        }

        return {
          symbol: pos.symbol,
          side: side, // Use actual side from position, don't infer
          quantity: quantity,
          entryPrice: entryPrice,
          currentPrice: currentPrice,
          unrealizedPnl: unrealizedPnl,
          realizedPnl: pos.realizedPnl || pos.realized_pnl,
          pnlPercent: pnlPercent,
          leverage: pos.leverage,
          marginMode: pos.marginMode || pos.margin_mode,
          liquidationPrice: pos.liquidationPrice || pos.liquidation_price,
          notionalValue: pos.notional || pos.notionalValue || (currentPrice * quantity),
          marginUsed: pos.collateral || pos.initialMargin,
          openTime: pos.timestamp || pos.opened_at,
          positionId: pos.id || pos.symbol,
        };
      });

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
  }, [activeAdapter, symbol, activeBroker]);

  // Subscribe to price updates for position symbols
  useEffect(() => {
    if (positions.length === 0) return;

    const marketDataService = getMarketDataService();
    const currentUnsubscribers = priceUnsubscribersRef.current;

    // Subscribe to each position's symbol for live updates
    for (const position of positions) {
      if (!currentUnsubscribers.has(position.symbol)) {
        const unsubscribe = marketDataService.subscribeToPrice(position.symbol, () => {
          // Price changed - trigger a refresh to recalculate P&L
          // Use a small debounce to avoid too many refreshes
        });
        currentUnsubscribers.set(position.symbol, unsubscribe);
      }
    }

    // Cleanup subscriptions for symbols no longer in positions
    const positionSymbols = new Set(positions.map(p => p.symbol));
    for (const [symbol, unsubscribe] of currentUnsubscribers) {
      if (!positionSymbols.has(symbol)) {
        unsubscribe();
        currentUnsubscribers.delete(symbol);
      }
    }

    return () => {
      // Don't cleanup here - we want to maintain subscriptions across refreshes
    };
  }, [positions]);

  // Auto-refresh - clear positions when broker changes
  useEffect(() => {
    // Clear positions when switching brokers
    setPositions([]);

    // Cleanup all price subscriptions when broker changes
    const currentUnsubscribers = priceUnsubscribersRef.current;
    for (const [, unsubscribe] of currentUnsubscribers) {
      unsubscribe();
    }
    currentUnsubscribers.clear();

    fetchPositions();

    if (!autoRefresh) return;

    const interval = setInterval(fetchPositions, refreshInterval);
    return () => clearInterval(interval);
  }, [fetchPositions, autoRefresh, refreshInterval, activeBroker]);

  // Cleanup subscriptions on unmount
  useEffect(() => {
    return () => {
      const currentUnsubscribers = priceUnsubscribersRef.current;
      for (const [, unsubscribe] of currentUnsubscribers) {
        unsubscribe();
      }
      currentUnsubscribers.clear();
    };
  }, []);

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
