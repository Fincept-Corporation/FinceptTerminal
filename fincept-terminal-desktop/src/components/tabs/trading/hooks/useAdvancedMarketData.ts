/**
 * Advanced Market Data Hooks
 * Mark OHLCV, Index OHLCV, Premium Index OHLCV, Long/Short Ratio
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface OHLCVCandle {
  timestamp: number;
  datetime: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface LongShortRatio {
  symbol: string;
  longShortRatio: number;
  longAccount?: number;
  shortAccount?: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

// ============================================================================
// MARK OHLCV HOOK
// ============================================================================

export function useMarkOHLCV(
  symbol?: string,
  timeframe: string = '1h',
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [candles, setCandles] = useState<OHLCVCandle[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchMarkOHLCV = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setCandles([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setCandles([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchMarkOHLCV !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchMarkOHLCV(symbol, timeframe, since, limit);

      const normalized: OHLCVCandle[] = (raw || []).map((candle: any[]) => ({
        timestamp: candle[0],
        datetime: new Date(candle[0]).toISOString(),
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5] || 0,
      }));

      setCandles(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useMarkOHLCV] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, timeframe, since, limit, tradingMode]);

  useEffect(() => {
    fetchMarkOHLCV();
  }, [fetchMarkOHLCV]);

  return { candles, isLoading, error, isSupported, refresh: fetchMarkOHLCV };
}

// ============================================================================
// INDEX OHLCV HOOK
// ============================================================================

export function useIndexOHLCV(
  symbol?: string,
  timeframe: string = '1h',
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [candles, setCandles] = useState<OHLCVCandle[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchIndexOHLCV = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setCandles([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setCandles([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchIndexOHLCV !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchIndexOHLCV(symbol, timeframe, since, limit);

      const normalized: OHLCVCandle[] = (raw || []).map((candle: any[]) => ({
        timestamp: candle[0],
        datetime: new Date(candle[0]).toISOString(),
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5] || 0,
      }));

      setCandles(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useIndexOHLCV] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, timeframe, since, limit, tradingMode]);

  useEffect(() => {
    fetchIndexOHLCV();
  }, [fetchIndexOHLCV]);

  return { candles, isLoading, error, isSupported, refresh: fetchIndexOHLCV };
}

// ============================================================================
// PREMIUM INDEX OHLCV HOOK
// ============================================================================

export function usePremiumIndexOHLCV(
  symbol?: string,
  timeframe: string = '1h',
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [candles, setCandles] = useState<OHLCVCandle[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchPremiumIndexOHLCV = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setCandles([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setCandles([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchPremiumIndexOHLCV !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchPremiumIndexOHLCV(symbol, timeframe, since, limit);

      const normalized: OHLCVCandle[] = (raw || []).map((candle: any[]) => ({
        timestamp: candle[0],
        datetime: new Date(candle[0]).toISOString(),
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5] || 0,
      }));

      setCandles(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[usePremiumIndexOHLCV] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, timeframe, since, limit, tradingMode]);

  useEffect(() => {
    fetchPremiumIndexOHLCV();
  }, [fetchPremiumIndexOHLCV]);

  return { candles, isLoading, error, isSupported, refresh: fetchPremiumIndexOHLCV };
}

// ============================================================================
// LONG SHORT RATIO HISTORY HOOK
// ============================================================================

export function useLongShortRatioHistory(
  symbol?: string,
  timeframe: string = '1h',
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<LongShortRatio[]>([]);
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

    if (typeof (activeAdapter as any).fetchLongShortRatioHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchLongShortRatioHistory(symbol, timeframe, since, limit);

      const normalized: LongShortRatio[] = (raw || []).map((item: any) => ({
        symbol: item.symbol || symbol,
        longShortRatio: item.longShortRatio || item.ratio || 0,
        longAccount: item.longAccount || item.longPosition,
        shortAccount: item.shortAccount || item.shortPosition,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useLongShortRatioHistory] Failed:', error);
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

// ============================================================================
// SETTLEMENT HISTORY HOOK
// ============================================================================

export interface SettlementInfo {
  symbol: string;
  price: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export function useSettlementHistory(symbol?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [settlements, setSettlements] = useState<SettlementInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchSettlements = useCallback(async () => {
    if (tradingMode === 'paper') {
      setSettlements([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setSettlements([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchSettlementHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchSettlementHistory(symbol, since, limit);

      const normalized: SettlementInfo[] = (raw || []).map((item: any) => ({
        symbol: item.symbol,
        price: item.price || item.settlementPrice || 0,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setSettlements(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useSettlementHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchSettlements();
  }, [fetchSettlements]);

  return { settlements, isLoading, error, isSupported, refresh: fetchSettlements };
}

// ============================================================================
// MY SETTLEMENT HISTORY HOOK
// ============================================================================

export interface MySettlementInfo extends SettlementInfo {
  amount: number;
  realizedPnl: number;
}

export function useMySettlementHistory(symbol?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [settlements, setSettlements] = useState<MySettlementInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchMySettlements = useCallback(async () => {
    if (tradingMode === 'paper') {
      setSettlements([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setSettlements([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchMySettlementHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchMySettlementHistory(symbol, since, limit);

      const normalized: MySettlementInfo[] = (raw || []).map((item: any) => ({
        symbol: item.symbol,
        price: item.price || item.settlementPrice || 0,
        amount: item.amount || item.quantity || 0,
        realizedPnl: item.realizedPnl || item.pnl || 0,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setSettlements(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useMySettlementHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchMySettlements();
  }, [fetchMySettlements]);

  return { settlements, isLoading, error, isSupported, refresh: fetchMySettlements };
}

// ============================================================================
// POSITION HISTORY HOOK
// ============================================================================

export interface PositionHistoryItem {
  symbol: string;
  side: 'long' | 'short';
  entryPrice: number;
  exitPrice?: number;
  amount: number;
  realizedPnl: number;
  openTimestamp: number;
  closeTimestamp?: number;
  info?: any;
}

export function usePositionHistory(symbol?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<PositionHistoryItem[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchHistory = useCallback(async () => {
    if (tradingMode === 'paper') {
      setHistory([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setHistory([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchPositionHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchPositionHistory(symbol, since, limit);

      const normalized: PositionHistoryItem[] = (raw || []).map((item: any) => ({
        symbol: item.symbol,
        side: item.side === 'sell' || item.side === 'short' ? 'short' : 'long',
        entryPrice: item.entryPrice || item.avgEntryPrice || 0,
        exitPrice: item.exitPrice || item.avgExitPrice,
        amount: item.amount || item.contracts || 0,
        realizedPnl: item.realizedPnl || item.pnl || 0,
        openTimestamp: item.openTimestamp || item.createTime || Date.now(),
        closeTimestamp: item.closeTimestamp || item.updateTime,
        info: item.info,
      }));

      normalized.sort((a, b) => (b.closeTimestamp || b.openTimestamp) - (a.closeTimestamp || a.openTimestamp));
      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[usePositionHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchHistory();
  }, [fetchHistory]);

  return { history, isLoading, error, isSupported, refresh: fetchHistory };
}
