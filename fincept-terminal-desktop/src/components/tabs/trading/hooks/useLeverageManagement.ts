/**
 * Leverage Management Hooks
 * Set/fetch leverage, margin mode, leverage tiers
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface LeverageSettings {
  symbol: string;
  leverage: number;
  longLeverage?: number;
  shortLeverage?: number;
  maxLeverage?: number;
  marginMode?: 'cross' | 'isolated';
  info?: any;
}

export interface LeverageTier {
  tier: number;
  minNotional: number;
  maxNotional: number;
  maintenanceMarginRate: number;
  maxLeverage: number;
  info?: any;
}

export interface MarginModeInfo {
  symbol: string;
  marginMode: 'cross' | 'isolated';
  info?: any;
}

// ============================================================================
// FETCH LEVERAGE HOOK
// ============================================================================

export function useFetchLeverage(symbol?: string) {
  const { activeAdapter, tradingMode, activeBroker } = useBrokerContext();
  const [leverage, setLeverage] = useState<LeverageSettings | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchLeverage = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setLeverage(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setLeverage(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchLeverage !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchLeverage(symbol);

      setLeverage({
        symbol: raw.symbol || symbol,
        leverage: raw.leverage || raw.longLeverage || 1,
        longLeverage: raw.longLeverage,
        shortLeverage: raw.shortLeverage,
        maxLeverage: raw.maxLeverage,
        marginMode: raw.marginMode,
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useFetchLeverage] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchLeverage();
  }, [fetchLeverage, activeBroker]);

  return { leverage, isLoading, error, isSupported, refresh: fetchLeverage };
}

// ============================================================================
// SET LEVERAGE HOOK
// ============================================================================

export function useSetLeverage() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastResult, setLastResult] = useState<LeverageSettings | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const setLeverage = useCallback(
    async (leverage: number, symbol: string, params?: { side?: 'long' | 'short' | 'both' }) => {
      if (tradingMode === 'paper') {
        setError(new Error('Cannot set leverage in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).setLeverage !== 'function') {
        setIsSupported(false);
        setError(new Error('Setting leverage not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).setLeverage(leverage, symbol, params);

        const result: LeverageSettings = {
          symbol,
          leverage: raw?.leverage || leverage,
          longLeverage: raw?.longLeverage,
          shortLeverage: raw?.shortLeverage,
          marginMode: raw?.marginMode,
          info: raw?.info,
        };

        setLastResult(result);
        setIsSupported(true);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useSetLeverage] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { setLeverage, isLoading, error, lastResult, isSupported };
}

// ============================================================================
// FETCH LEVERAGE TIERS HOOK
// ============================================================================

export function useLeverageTiers(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [tiers, setTiers] = useState<LeverageTier[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchTiers = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setTiers([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setTiers([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchLeverageTiers !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchLeverageTiers([symbol]);
      const symbolTiers = raw?.[symbol] || [];

      const normalized: LeverageTier[] = symbolTiers.map((tier: any, index: number) => ({
        tier: tier.tier || index + 1,
        minNotional: tier.minNotional || tier.notionalFloor || 0,
        maxNotional: tier.maxNotional || tier.notionalCap || Infinity,
        maintenanceMarginRate: tier.maintenanceMarginRate || tier.maintenanceMargin || 0,
        maxLeverage: tier.maxLeverage || 1,
        info: tier.info,
      }));

      setTiers(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useLeverageTiers] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchTiers();
  }, [fetchTiers]);

  return { tiers, isLoading, error, isSupported, refresh: fetchTiers };
}

// ============================================================================
// SET MARGIN MODE HOOK
// ============================================================================

export function useSetMarginMode() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastResult, setLastResult] = useState<MarginModeInfo | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const setMarginMode = useCallback(
    async (marginMode: 'cross' | 'isolated', symbol: string) => {
      if (tradingMode === 'paper') {
        setError(new Error('Cannot set margin mode in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).setMarginMode !== 'function') {
        setIsSupported(false);
        setError(new Error('Setting margin mode not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).setMarginMode(marginMode, symbol);

        const result: MarginModeInfo = {
          symbol,
          marginMode: raw?.marginMode || marginMode,
          info: raw?.info,
        };

        setLastResult(result);
        setIsSupported(true);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useSetMarginMode] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { setMarginMode, isLoading, error, lastResult, isSupported };
}

// ============================================================================
// FETCH POSITION MODE HOOK
// ============================================================================

export function usePositionMode(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [positionMode, setPositionMode] = useState<'one-way' | 'hedge' | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchPositionMode = useCallback(async () => {
    if (tradingMode === 'paper') {
      setPositionMode('one-way');
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setPositionMode(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchPositionMode !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchPositionMode(symbol);

      // Normalize position mode
      const mode = raw?.hedged || raw?.dualSidePosition ? 'hedge' : 'one-way';
      setPositionMode(mode);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[usePositionMode] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchPositionMode();
  }, [fetchPositionMode]);

  return { positionMode, isLoading, error, isSupported, refresh: fetchPositionMode };
}

// ============================================================================
// SET POSITION MODE HOOK
// ============================================================================

export function useSetPositionMode() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const setPositionModeAction = useCallback(
    async (hedged: boolean, symbol?: string) => {
      if (tradingMode === 'paper') {
        setError(new Error('Cannot set position mode in paper trading mode'));
        return false;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return false;
      }

      if (typeof (activeAdapter as any).setPositionMode !== 'function') {
        setIsSupported(false);
        setError(new Error('Setting position mode not supported by this exchange'));
        return false;
      }

      setIsLoading(true);
      setError(null);

      try {
        await (activeAdapter as any).setPositionMode(hedged, symbol);
        setIsSupported(true);
        return true;
      } catch (err) {
        const error = err as Error;
        console.error('[useSetPositionMode] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return false;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { setPositionMode: setPositionModeAction, isLoading, error, isSupported };
}

// ============================================================================
// CLOSE ALL POSITIONS HOOK
// ============================================================================

export function useCloseAllPositions() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastResult, setLastResult] = useState<any[] | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const closeAllPositions = useCallback(
    async (params?: { symbol?: string }) => {
      if (tradingMode === 'paper') {
        setError(new Error('Close all positions not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).closeAllPositions !== 'function') {
        setIsSupported(false);
        setError(new Error('Close all positions not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const result = await (activeAdapter as any).closeAllPositions(params);
        setLastResult(result);
        setIsSupported(true);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useCloseAllPositions] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { closeAllPositions, isLoading, error, lastResult, isSupported };
}
