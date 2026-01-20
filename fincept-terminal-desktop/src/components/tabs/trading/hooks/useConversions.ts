/**
 * Conversion & Transfer Hooks
 * Crypto swap/convert, internal transfers between accounts
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface ConvertQuote {
  id?: string;
  fromCurrency: string;
  toCurrency: string;
  fromAmount: number;
  toAmount: number;
  rate: number;
  inverseRate: number;
  fee?: number;
  feeCurrency?: string;
  expireTime?: number;
  info?: any;
}

export interface ConvertTrade {
  id: string;
  fromCurrency: string;
  toCurrency: string;
  fromAmount: number;
  toAmount: number;
  rate: number;
  fee?: number;
  feeCurrency?: string;
  status: 'pending' | 'success' | 'failed';
  timestamp: number;
  datetime: string;
  info?: any;
}

export interface TransferResult {
  id: string;
  currency: string;
  amount: number;
  fromAccount: string;
  toAccount: string;
  status: 'pending' | 'success' | 'failed';
  timestamp: number;
  datetime: string;
  info?: any;
}

// ============================================================================
// CONVERT CURRENCIES HOOK
// ============================================================================

export function useConvertCurrencies() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [currencies, setCurrencies] = useState<string[]>([]);
  const [pairs, setPairs] = useState<{ from: string; to: string }[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchCurrencies = useCallback(async () => {
    if (tradingMode === 'paper') {
      setCurrencies([]);
      setPairs([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setCurrencies([]);
      setPairs([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchConvertCurrencies !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchConvertCurrencies();

      // Extract unique currencies and pairs
      const currencySet = new Set<string>();
      const pairList: { from: string; to: string }[] = [];

      if (Array.isArray(raw)) {
        for (const item of raw) {
          if (item.from) currencySet.add(item.from);
          if (item.to) currencySet.add(item.to);
          if (item.from && item.to) {
            pairList.push({ from: item.from, to: item.to });
          }
        }
      } else if (raw && typeof raw === 'object') {
        // Could be { BTC: ['USDT', 'ETH'], ... }
        for (const [from, toList] of Object.entries(raw)) {
          currencySet.add(from);
          if (Array.isArray(toList)) {
            for (const to of toList) {
              currencySet.add(to as string);
              pairList.push({ from, to: to as string });
            }
          }
        }
      }

      setCurrencies(Array.from(currencySet).sort());
      setPairs(pairList);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useConvertCurrencies] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode]);

  useEffect(() => {
    fetchCurrencies();
  }, [fetchCurrencies]);

  return { currencies, pairs, isLoading, error, isSupported, refresh: fetchCurrencies };
}

// ============================================================================
// CONVERT QUOTE HOOK
// ============================================================================

export function useConvertQuote(fromCurrency?: string, toCurrency?: string, amount?: number) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [quote, setQuote] = useState<ConvertQuote | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchQuote = useCallback(async () => {
    if (tradingMode === 'paper' || !fromCurrency || !toCurrency || !amount || amount <= 0) {
      setQuote(null);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setQuote(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchConvertQuote !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchConvertQuote(fromCurrency, toCurrency, amount);

      setQuote({
        id: raw.id,
        fromCurrency: raw.fromCurrency || fromCurrency,
        toCurrency: raw.toCurrency || toCurrency,
        fromAmount: raw.fromAmount || amount,
        toAmount: raw.toAmount || 0,
        rate: raw.rate || raw.price || 0,
        inverseRate: raw.inverseRate || (raw.rate ? 1 / raw.rate : 0),
        fee: raw.fee,
        feeCurrency: raw.feeCurrency,
        expireTime: raw.expireTime || raw.expires,
        info: raw.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useConvertQuote] Failed:', error);
      setError(error);
      setQuote(null);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, fromCurrency, toCurrency, amount, tradingMode]);

  useEffect(() => {
    fetchQuote();
  }, [fetchQuote]);

  return { quote, isLoading, error, isSupported, refresh: fetchQuote };
}

// ============================================================================
// CREATE CONVERT TRADE HOOK
// ============================================================================

export function useCreateConvertTrade() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastTrade, setLastTrade] = useState<ConvertTrade | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const createTrade = useCallback(
    async (fromCurrency: string, toCurrency: string, amount: number, quoteId?: string) => {
      if (tradingMode === 'paper') {
        setError(new Error('Conversions not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).createConvertTrade !== 'function') {
        setIsSupported(false);
        setError(new Error('Currency conversion not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).createConvertTrade(
          fromCurrency,
          toCurrency,
          amount,
          quoteId ? { quoteId } : undefined
        );

        const trade: ConvertTrade = {
          id: raw.id,
          fromCurrency: raw.fromCurrency || fromCurrency,
          toCurrency: raw.toCurrency || toCurrency,
          fromAmount: raw.fromAmount || amount,
          toAmount: raw.toAmount || 0,
          rate: raw.rate || raw.price || 0,
          fee: raw.fee,
          feeCurrency: raw.feeCurrency,
          status: raw.status === 'success' || raw.status === 'filled' ? 'success' : 'pending',
          timestamp: raw.timestamp || Date.now(),
          datetime: raw.datetime || new Date().toISOString(),
          info: raw.info,
        };

        setLastTrade(trade);
        setIsSupported(true);
        return trade;
      } catch (err) {
        const error = err as Error;
        console.error('[useCreateConvertTrade] Failed:', error);
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

  return { createTrade, isLoading, error, lastTrade, isSupported };
}

// ============================================================================
// CONVERT TRADE HISTORY HOOK
// ============================================================================

export function useConvertTradeHistory(code?: string, since?: number, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<ConvertTrade[]>([]);
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

    if (typeof (activeAdapter as any).fetchConvertTradeHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchConvertTradeHistory(code, since, limit);

      const normalized: ConvertTrade[] = (raw || []).map((item: any) => ({
        id: item.id,
        fromCurrency: item.fromCurrency || item.from,
        toCurrency: item.toCurrency || item.to,
        fromAmount: item.fromAmount || 0,
        toAmount: item.toAmount || 0,
        rate: item.rate || item.price || 0,
        fee: item.fee,
        feeCurrency: item.feeCurrency,
        status: item.status === 'success' || item.status === 'filled' ? 'success' : item.status === 'failed' ? 'failed' : 'pending',
        timestamp: item.timestamp || Date.now(),
        datetime: item.datetime || new Date().toISOString(),
        info: item.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useConvertTradeHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, code, since, limit, tradingMode]);

  useEffect(() => {
    fetchHistory();
  }, [fetchHistory]);

  return { history, isLoading, error, isSupported, refresh: fetchHistory };
}

// ============================================================================
// TRANSFER HOOK
// ============================================================================

export function useTransfer() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastTransfer, setLastTransfer] = useState<TransferResult | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const transfer = useCallback(
    async (code: string, amount: number, fromAccount: string, toAccount: string) => {
      if (tradingMode === 'paper') {
        setError(new Error('Transfers not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).transfer !== 'function') {
        setIsSupported(false);
        setError(new Error('Internal transfers not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).transfer(code, amount, fromAccount, toAccount);

        const result: TransferResult = {
          id: raw.id,
          currency: raw.currency || code,
          amount: raw.amount || amount,
          fromAccount: raw.fromAccount || fromAccount,
          toAccount: raw.toAccount || toAccount,
          status: raw.status === 'ok' || raw.status === 'success' ? 'success' : 'pending',
          timestamp: raw.timestamp || Date.now(),
          datetime: raw.datetime || new Date().toISOString(),
          info: raw.info,
        };

        setLastTransfer(result);
        setIsSupported(true);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useTransfer] Failed:', error);
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

  return { transfer, isLoading, error, lastTransfer, isSupported };
}

// ============================================================================
// AVAILABLE ACCOUNTS HOOK (for transfer source/destination)
// ============================================================================

export function useAvailableAccounts() {
  const { activeBroker } = useBrokerContext();

  // Common account types across exchanges
  const commonAccounts = [
    { id: 'spot', name: 'Spot', description: 'Main trading account' },
    { id: 'margin', name: 'Margin', description: 'Cross margin account' },
    { id: 'futures', name: 'Futures', description: 'Futures/derivatives account' },
    { id: 'funding', name: 'Funding', description: 'Funding/wallet account' },
  ];

  // Exchange-specific accounts
  const exchangeAccounts: Record<string, typeof commonAccounts> = {
    binance: [
      ...commonAccounts,
      { id: 'isolated', name: 'Isolated Margin', description: 'Isolated margin account' },
      { id: 'coin_future', name: 'Coin-M Futures', description: 'Coin-margined futures' },
    ],
    bybit: [
      { id: 'SPOT', name: 'Spot', description: 'Spot trading account' },
      { id: 'CONTRACT', name: 'Derivatives', description: 'Derivatives account' },
      { id: 'FUND', name: 'Funding', description: 'Funding account' },
      { id: 'UNIFIED', name: 'Unified', description: 'Unified trading account' },
    ],
    okx: [
      { id: '18', name: 'Trading', description: 'Trading account' },
      { id: '6', name: 'Funding', description: 'Funding account' },
    ],
    kucoin: [
      { id: 'main', name: 'Main', description: 'Main account' },
      { id: 'trade', name: 'Trading', description: 'Trading account' },
      { id: 'margin', name: 'Margin', description: 'Margin account' },
      { id: 'contract', name: 'Futures', description: 'Futures account' },
    ],
  };

  return exchangeAccounts[activeBroker || ''] || commonAccounts;
}
