/**
 * Options Trading Hooks
 * Greeks, option chains, volatility for options trading
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface GreeksInfo {
  symbol: string;
  delta: number;
  gamma: number;
  theta: number;
  vega: number;
  rho?: number;
  impliedVolatility?: number;
  timestamp: number;
  info?: any;
}

export interface OptionContract {
  symbol: string;
  underlying: string;
  strike: number;
  expiry: number;
  expiryDatetime: string;
  optionType: 'call' | 'put';
  bid?: number;
  ask?: number;
  last?: number;
  volume?: number;
  openInterest?: number;
  greeks?: GreeksInfo;
  info?: any;
}

export interface OptionChain {
  underlying: string;
  expirations: number[];
  strikes: number[];
  calls: OptionContract[];
  puts: OptionContract[];
}

export interface VolatilityData {
  symbol: string;
  impliedVolatility: number;
  historicalVolatility?: number;
  timestamp: number;
  datetime: string;
}

// ============================================================================
// GREEKS HOOK
// ============================================================================

export function useGreeks(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [greeks, setGreeks] = useState<GreeksInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchGreeks = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setGreeks(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setGreeks(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchGreeks !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchGreeks(symbol);

      setGreeks({
        symbol: raw.symbol || symbol,
        delta: raw.delta || 0,
        gamma: raw.gamma || 0,
        theta: raw.theta || 0,
        vega: raw.vega || 0,
        rho: raw.rho,
        impliedVolatility: raw.impliedVolatility || raw.iv,
        timestamp: raw.timestamp || Date.now(),
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useGreeks] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchGreeks();
    const interval = setInterval(fetchGreeks, 10000); // Update every 10s
    return () => clearInterval(interval);
  }, [fetchGreeks]);

  return { greeks, isLoading, error, isSupported, refresh: fetchGreeks };
}

// ============================================================================
// OPTION HOOK (Single Contract)
// ============================================================================

export function useOption(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [option, setOption] = useState<OptionContract | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchOption = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setOption(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setOption(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchOption !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchOption(symbol);

      setOption({
        symbol: raw.symbol || symbol,
        underlying: raw.underlying || raw.base,
        strike: raw.strike || 0,
        expiry: raw.expiry || raw.expiryTimestamp,
        expiryDatetime: raw.expiryDatetime || raw.expiry,
        optionType: raw.optionType || raw.type,
        bid: raw.bid,
        ask: raw.ask,
        last: raw.last || raw.lastPrice,
        volume: raw.volume,
        openInterest: raw.openInterest,
        greeks: raw.greeks,
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useOption] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchOption();
    const interval = setInterval(fetchOption, 5000);
    return () => clearInterval(interval);
  }, [fetchOption]);

  return { option, isLoading, error, isSupported, refresh: fetchOption };
}

// ============================================================================
// OPTION CHAIN HOOK
// ============================================================================

export function useOptionChain(underlyingSymbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [chain, setChain] = useState<OptionChain | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchOptionChain = useCallback(async () => {
    if (tradingMode === 'paper' || !underlyingSymbol) {
      setChain(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setChain(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchOptionChain !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchOptionChain(underlyingSymbol);

      // Normalize option chain data
      const calls: OptionContract[] = [];
      const puts: OptionContract[] = [];
      const expirations = new Set<number>();
      const strikes = new Set<number>();

      // Process raw data - could be array or object
      const options = Array.isArray(raw) ? raw : Object.values(raw || {});

      for (const opt of options) {
        const contract: OptionContract = {
          symbol: opt.symbol,
          underlying: opt.underlying || opt.base || underlyingSymbol,
          strike: opt.strike || 0,
          expiry: opt.expiry || opt.expiryTimestamp || 0,
          expiryDatetime: opt.expiryDatetime || opt.expiry || '',
          optionType: opt.optionType || opt.type,
          bid: opt.bid,
          ask: opt.ask,
          last: opt.last || opt.lastPrice,
          volume: opt.volume,
          openInterest: opt.openInterest,
          greeks: opt.greeks,
          info: opt.info,
        };

        if (contract.expiry) expirations.add(contract.expiry);
        if (contract.strike) strikes.add(contract.strike);

        if (contract.optionType === 'call') {
          calls.push(contract);
        } else if (contract.optionType === 'put') {
          puts.push(contract);
        }
      }

      setChain({
        underlying: underlyingSymbol,
        expirations: Array.from(expirations).sort((a, b) => a - b),
        strikes: Array.from(strikes).sort((a, b) => a - b),
        calls: calls.sort((a, b) => a.strike - b.strike),
        puts: puts.sort((a, b) => a.strike - b.strike),
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useOptionChain] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, underlyingSymbol, tradingMode]);

  useEffect(() => {
    fetchOptionChain();
  }, [fetchOptionChain]);

  return { chain, isLoading, error, isSupported, refresh: fetchOptionChain };
}

// ============================================================================
// VOLATILITY HISTORY HOOK
// ============================================================================

export function useVolatilityHistory(symbol?: string, timeframe: string = '1h', since?: number, limit: number = 100) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<VolatilityData[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchHistory = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setHistory([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setHistory([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchVolatilityHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchVolatilityHistory(symbol, timeframe, since, limit);

      const normalized: VolatilityData[] = (raw || []).map((item: any) => ({
        symbol: item.symbol || symbol,
        impliedVolatility: item.impliedVolatility || item.iv || 0,
        historicalVolatility: item.historicalVolatility || item.hv,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
      }));

      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useVolatilityHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, timeframe, since, limit, tradingMode]);

  useEffect(() => {
    fetchHistory();
  }, [fetchHistory]);

  return { history, isLoading, error, isSupported, refresh: fetchHistory };
}
