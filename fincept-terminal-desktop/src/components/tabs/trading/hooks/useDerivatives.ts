/**
 * Derivatives Hooks
 * Funding rates, mark prices, open interest, liquidations for futures/perpetuals
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface FundingRateInfo {
  symbol: string;
  fundingRate: number;
  fundingTimestamp?: number;
  fundingDatetime?: string;
  nextFundingRate?: number;
  nextFundingTimestamp?: number;
  nextFundingDatetime?: string;
  previousFundingRate?: number;
  previousFundingTimestamp?: number;
  info?: any;
}

export interface MarkPriceInfo {
  symbol: string;
  markPrice: number;
  indexPrice?: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export interface OpenInterestInfo {
  symbol: string;
  openInterestAmount: number;
  openInterestValue: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export interface LiquidationInfo {
  id: string;
  symbol: string;
  side: 'buy' | 'sell';
  type: string;
  price: number;
  amount: number;
  cost: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export interface BorrowRateInfo {
  currency: string;
  rate: number;
  period?: number; // In hours or days
  timestamp: number;
  datetime: string;
  info?: any;
}

// ============================================================================
// FUNDING RATE HOOKS
// ============================================================================

export function useFundingRate(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [fundingRate, setFundingRate] = useState<FundingRateInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchFundingRate = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setFundingRate(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setFundingRate(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchFundingRate !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchFundingRate(symbol);

      setFundingRate({
        symbol: raw.symbol || symbol,
        fundingRate: raw.fundingRate || 0,
        fundingTimestamp: raw.fundingTimestamp,
        fundingDatetime: raw.fundingDatetime,
        nextFundingRate: raw.nextFundingRate,
        nextFundingTimestamp: raw.nextFundingTimestamp,
        nextFundingDatetime: raw.nextFundingDatetime,
        previousFundingRate: raw.previousFundingRate,
        previousFundingTimestamp: raw.previousFundingTimestamp,
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useFundingRate] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchFundingRate();
    const interval = setInterval(fetchFundingRate, 60000); // Update every minute
    return () => clearInterval(interval);
  }, [fetchFundingRate]);

  return { fundingRate, isLoading, error, isSupported, refresh: fetchFundingRate };
}

export function useFundingRates(symbols?: string[]) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [fundingRates, setFundingRates] = useState<FundingRateInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchFundingRates = useCallback(async () => {
    if (tradingMode === 'paper') {
      setFundingRates([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setFundingRates([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchFundingRates !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchFundingRates(symbols);

      const normalized: FundingRateInfo[] = Object.entries(raw || {}).map(([sym, data]: [string, any]) => ({
        symbol: sym,
        fundingRate: data.fundingRate || 0,
        fundingTimestamp: data.fundingTimestamp,
        fundingDatetime: data.fundingDatetime,
        nextFundingRate: data.nextFundingRate,
        nextFundingTimestamp: data.nextFundingTimestamp,
        info: data.info,
      }));

      // Sort by absolute funding rate (highest first)
      normalized.sort((a, b) => Math.abs(b.fundingRate) - Math.abs(a.fundingRate));
      setFundingRates(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useFundingRates] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbols, tradingMode]);

  useEffect(() => {
    fetchFundingRates();
    const interval = setInterval(fetchFundingRates, 60000);
    return () => clearInterval(interval);
  }, [fetchFundingRates]);

  return { fundingRates, isLoading, error, isSupported, refresh: fetchFundingRates };
}

export function useFundingRateHistory(symbol?: string, since?: number, limit: number = 100) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<FundingRateInfo[]>([]);
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

    if (typeof (activeAdapter as any).fetchFundingRateHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchFundingRateHistory(symbol, since, limit);

      const normalized: FundingRateInfo[] = (raw || []).map((item: any) => ({
        symbol: item.symbol || symbol,
        fundingRate: item.fundingRate || 0,
        fundingTimestamp: item.timestamp || item.fundingTimestamp,
        fundingDatetime: item.datetime || item.fundingDatetime,
        info: item.info,
      }));

      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useFundingRateHistory] Failed:', error);
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

// ============================================================================
// MARK PRICE HOOKS
// ============================================================================

export function useMarkPrice(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [markPrice, setMarkPrice] = useState<MarkPriceInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchMarkPrice = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setMarkPrice(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setMarkPrice(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchMarkPrice !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchMarkPrice(symbol);

      setMarkPrice({
        symbol: raw.symbol || symbol,
        markPrice: raw.markPrice || raw.mark || 0,
        indexPrice: raw.indexPrice || raw.index,
        timestamp: raw.timestamp || Date.now(),
        datetime: raw.datetime || new Date().toISOString(),
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useMarkPrice] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchMarkPrice();
    const interval = setInterval(fetchMarkPrice, 5000); // Update every 5s
    return () => clearInterval(interval);
  }, [fetchMarkPrice]);

  return { markPrice, isLoading, error, isSupported, refresh: fetchMarkPrice };
}

export function useMarkPrices(symbols?: string[]) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [markPrices, setMarkPrices] = useState<MarkPriceInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchMarkPrices = useCallback(async () => {
    if (tradingMode === 'paper') {
      setMarkPrices([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setMarkPrices([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchMarkPrices !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchMarkPrices(symbols);

      const normalized: MarkPriceInfo[] = Object.entries(raw || {}).map(([sym, data]: [string, any]) => ({
        symbol: sym,
        markPrice: data.markPrice || data.mark || 0,
        indexPrice: data.indexPrice || data.index,
        timestamp: data.timestamp || Date.now(),
        datetime: data.datetime || new Date().toISOString(),
        info: data.info,
      }));

      setMarkPrices(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useMarkPrices] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbols, tradingMode]);

  useEffect(() => {
    fetchMarkPrices();
    const interval = setInterval(fetchMarkPrices, 5000);
    return () => clearInterval(interval);
  }, [fetchMarkPrices]);

  return { markPrices, isLoading, error, isSupported, refresh: fetchMarkPrices };
}

// ============================================================================
// OPEN INTEREST HOOKS
// ============================================================================

export function useOpenInterest(symbol?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [openInterest, setOpenInterest] = useState<OpenInterestInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchOpenInterest = useCallback(async () => {
    if (tradingMode === 'paper' || !symbol) {
      setOpenInterest(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setOpenInterest(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchOpenInterest !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchOpenInterest(symbol);

      setOpenInterest({
        symbol: raw.symbol || symbol,
        openInterestAmount: raw.openInterestAmount || raw.openInterest || 0,
        openInterestValue: raw.openInterestValue || 0,
        timestamp: raw.timestamp || Date.now(),
        datetime: raw.datetime || new Date().toISOString(),
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useOpenInterest] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, tradingMode]);

  useEffect(() => {
    fetchOpenInterest();
    const interval = setInterval(fetchOpenInterest, 30000); // Update every 30s
    return () => clearInterval(interval);
  }, [fetchOpenInterest]);

  return { openInterest, isLoading, error, isSupported, refresh: fetchOpenInterest };
}

export function useOpenInterestHistory(symbol?: string, timeframe: string = '1h', since?: number, limit: number = 100) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<OpenInterestInfo[]>([]);
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

    if (typeof (activeAdapter as any).fetchOpenInterestHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchOpenInterestHistory(symbol, timeframe, since, limit);

      const normalized: OpenInterestInfo[] = (raw || []).map((item: any) => ({
        symbol: item.symbol || symbol,
        openInterestAmount: item.openInterestAmount || item.openInterest || 0,
        openInterestValue: item.openInterestValue || 0,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useOpenInterestHistory] Failed:', error);
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
// LIQUIDATIONS HOOKS
// ============================================================================

export function useLiquidations(symbol?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [liquidations, setLiquidations] = useState<LiquidationInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchLiquidations = useCallback(async () => {
    if (tradingMode === 'paper') {
      setLiquidations([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setLiquidations([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchLiquidations !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchLiquidations(symbol, since, limit);

      const normalized: LiquidationInfo[] = (raw || []).map((item: any) => ({
        id: item.id || `liq-${item.timestamp}`,
        symbol: item.symbol,
        side: item.side,
        type: item.type || 'liquidation',
        price: item.price || 0,
        amount: item.amount || 0,
        cost: item.cost || (item.price * item.amount) || 0,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      // Sort by timestamp descending
      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setLiquidations(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useLiquidations] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchLiquidations();
    const interval = setInterval(fetchLiquidations, 30000);
    return () => clearInterval(interval);
  }, [fetchLiquidations]);

  return { liquidations, isLoading, error, isSupported, refresh: fetchLiquidations };
}

export function useMyLiquidations(symbol?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [liquidations, setLiquidations] = useState<LiquidationInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchMyLiquidations = useCallback(async () => {
    if (tradingMode === 'paper') {
      setLiquidations([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setLiquidations([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchMyLiquidations !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchMyLiquidations(symbol, since, limit);

      const normalized: LiquidationInfo[] = (raw || []).map((item: any) => ({
        id: item.id || `liq-${item.timestamp}`,
        symbol: item.symbol,
        side: item.side,
        type: item.type || 'liquidation',
        price: item.price || 0,
        amount: item.amount || 0,
        cost: item.cost || (item.price * item.amount) || 0,
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setLiquidations(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useMyLiquidations] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchMyLiquidations();
  }, [fetchMyLiquidations]);

  return { liquidations, isLoading, error, isSupported, refresh: fetchMyLiquidations };
}

// ============================================================================
// BORROW RATE HOOKS
// ============================================================================

export function useBorrowRate(code?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [borrowRate, setBorrowRate] = useState<BorrowRateInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchBorrowRate = useCallback(async () => {
    if (tradingMode === 'paper' || !code) {
      setBorrowRate(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setBorrowRate(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchBorrowRate !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchBorrowRate(code);

      setBorrowRate({
        currency: raw.currency || code,
        rate: raw.rate || 0,
        period: raw.period,
        timestamp: raw.timestamp || Date.now(),
        datetime: raw.datetime || new Date().toISOString(),
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useBorrowRate] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, code, tradingMode]);

  useEffect(() => {
    fetchBorrowRate();
    const interval = setInterval(fetchBorrowRate, 60000);
    return () => clearInterval(interval);
  }, [fetchBorrowRate]);

  return { borrowRate, isLoading, error, isSupported, refresh: fetchBorrowRate };
}

export function useBorrowRates() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [borrowRates, setBorrowRates] = useState<BorrowRateInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchBorrowRates = useCallback(async () => {
    if (tradingMode === 'paper') {
      setBorrowRates([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setBorrowRates([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchBorrowRates !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchBorrowRates();

      const normalized: BorrowRateInfo[] = Object.entries(raw || {}).map(([currency, data]: [string, any]) => ({
        currency,
        rate: data.rate || 0,
        period: data.period,
        timestamp: data.timestamp || Date.now(),
        datetime: data.datetime || new Date().toISOString(),
        info: data.info,
      }));

      // Sort by rate descending
      normalized.sort((a, b) => b.rate - a.rate);
      setBorrowRates(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useBorrowRates] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode]);

  useEffect(() => {
    fetchBorrowRates();
    const interval = setInterval(fetchBorrowRates, 60000);
    return () => clearInterval(interval);
  }, [fetchBorrowRates]);

  return { borrowRates, isLoading, error, isSupported, refresh: fetchBorrowRates };
}

export function useBorrowInterest(code?: string, symbol?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [interest, setInterest] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchBorrowInterest = useCallback(async () => {
    if (tradingMode === 'paper') {
      setInterest([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setInterest([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchBorrowInterest !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchBorrowInterest(code, symbol, since, limit);
      setInterest(raw || []);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useBorrowInterest] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, code, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchBorrowInterest();
  }, [fetchBorrowInterest]);

  return { interest, isLoading, error, isSupported, refresh: fetchBorrowInterest };
}
