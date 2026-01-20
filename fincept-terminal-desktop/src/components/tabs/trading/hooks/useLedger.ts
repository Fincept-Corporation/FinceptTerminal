/**
 * Ledger & Transfer History Hooks
 * Ledger entries, transfer history, deposit/withdraw fees
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface LedgerEntry {
  id: string;
  currency: string;
  account?: string;
  referenceAccount?: string;
  referenceId?: string;
  type: string;
  amount: number;
  before?: number;
  after?: number;
  fee?: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export interface TransferEntry {
  id: string;
  currency: string;
  amount: number;
  fromAccount: string;
  toAccount: string;
  status: string;
  timestamp: number;
  datetime: string;
  info?: any;
}

export interface DepositWithdrawFee {
  currency: string;
  network?: string;
  deposit?: {
    fee?: number;
    percentage?: boolean;
  };
  withdraw?: {
    fee?: number;
    percentage?: boolean;
    min?: number;
    max?: number;
  };
  info?: any;
}

// ============================================================================
// FETCH LEDGER HOOK
// ============================================================================

export function useLedger(
  code?: string,
  since?: number,
  limit: number = 100,
  params?: { type?: string }
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [entries, setEntries] = useState<LedgerEntry[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchLedger = useCallback(async () => {
    if (tradingMode === 'paper') {
      setEntries([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setEntries([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchLedger !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchLedger(code, since, limit, params);

      const normalized: LedgerEntry[] = (raw || []).map((entry: any) => ({
        id: entry.id,
        currency: entry.currency,
        account: entry.account,
        referenceAccount: entry.referenceAccount,
        referenceId: entry.referenceId,
        type: entry.type,
        amount: entry.amount,
        before: entry.before,
        after: entry.after,
        fee: entry.fee?.cost || entry.fee,
        timestamp: entry.timestamp,
        datetime: entry.datetime || new Date(entry.timestamp).toISOString(),
        info: entry.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setEntries(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useLedger] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, code, since, limit, params, tradingMode]);

  useEffect(() => {
    fetchLedger();
  }, [fetchLedger]);

  return { entries, isLoading, error, isSupported, refresh: fetchLedger };
}

// ============================================================================
// FETCH TRANSFERS HOOK
// ============================================================================

export function useTransferHistory(
  code?: string,
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [transfers, setTransfers] = useState<TransferEntry[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchTransfers = useCallback(async () => {
    if (tradingMode === 'paper') {
      setTransfers([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setTransfers([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchTransfers !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchTransfers(code, since, limit);

      const normalized: TransferEntry[] = (raw || []).map((transfer: any) => ({
        id: transfer.id,
        currency: transfer.currency,
        amount: transfer.amount,
        fromAccount: transfer.fromAccount || transfer.from,
        toAccount: transfer.toAccount || transfer.to,
        status: transfer.status,
        timestamp: transfer.timestamp,
        datetime: transfer.datetime || new Date(transfer.timestamp).toISOString(),
        info: transfer.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setTransfers(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useTransferHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, code, since, limit, tradingMode]);

  useEffect(() => {
    fetchTransfers();
  }, [fetchTransfers]);

  return { transfers, isLoading, error, isSupported, refresh: fetchTransfers };
}

// ============================================================================
// FETCH DEPOSIT/WITHDRAW FEES HOOK
// ============================================================================

export function useDepositWithdrawFees(codes?: string[]) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [fees, setFees] = useState<Record<string, DepositWithdrawFee>>({});
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchFees = useCallback(async () => {
    if (tradingMode === 'paper') {
      setFees({});
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setFees({});
      return;
    }

    if (typeof (activeAdapter as any).fetchDepositWithdrawFees !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchDepositWithdrawFees(codes);

      const normalized: Record<string, DepositWithdrawFee> = {};

      for (const [currency, feeInfo] of Object.entries(raw || {})) {
        const info = feeInfo as any;
        normalized[currency] = {
          currency,
          network: info.network,
          deposit: info.deposit
            ? {
                fee: info.deposit.fee,
                percentage: info.deposit.percentage,
              }
            : undefined,
          withdraw: info.withdraw
            ? {
                fee: info.withdraw.fee,
                percentage: info.withdraw.percentage,
                min: info.withdraw.min,
                max: info.withdraw.max,
              }
            : undefined,
          info: info.info,
        };
      }

      setFees(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useDepositWithdrawFees] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, codes, tradingMode]);

  useEffect(() => {
    fetchFees();
  }, [fetchFees]);

  return { fees, isLoading, error, isSupported, refresh: fetchFees };
}

// ============================================================================
// FETCH BORROW RATE HISTORY HOOK
// ============================================================================

export interface BorrowRateHistoryEntry {
  currency: string;
  rate: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export function useBorrowRateHistory(
  code: string,
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<BorrowRateHistoryEntry[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchHistory = useCallback(async () => {
    if (tradingMode === 'paper' || !code) {
      setHistory([]);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setHistory([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchBorrowRateHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchBorrowRateHistory(code, since, limit);

      const normalized: BorrowRateHistoryEntry[] = (raw || []).map((entry: any) => ({
        currency: entry.currency || code,
        rate: entry.rate || entry.borrowRate || 0,
        timestamp: entry.timestamp,
        datetime: entry.datetime || new Date(entry.timestamp).toISOString(),
        info: entry.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useBorrowRateHistory] Failed:', error);
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
// FETCH FUNDING HISTORY HOOK
// ============================================================================

export interface FundingHistoryEntry {
  id: string;
  symbol: string;
  currency: string;
  amount: number;
  timestamp: number;
  datetime: string;
  info?: any;
}

export function useFundingHistory(
  symbol?: string,
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<FundingHistoryEntry[]>([]);
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

    if (typeof (activeAdapter as any).fetchFundingHistory !== 'function') {
      setIsSupported(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const raw = await (activeAdapter as any).fetchFundingHistory(symbol, since, limit);

      const normalized: FundingHistoryEntry[] = (raw || []).map((entry: any) => ({
        id: entry.id,
        symbol: entry.symbol,
        currency: entry.currency,
        amount: entry.amount,
        timestamp: entry.timestamp,
        datetime: entry.datetime || new Date(entry.timestamp).toISOString(),
        info: entry.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useFundingHistory] Failed:', error);
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
// FETCH INCOME HISTORY HOOK (Futures)
// ============================================================================

export interface IncomeEntry {
  id: string;
  symbol: string;
  incomeType: string;
  income: number;
  asset: string;
  timestamp: number;
  datetime: string;
  info?: any;
}

export function useIncomeHistory(
  incomeType?: string,
  symbol?: string,
  since?: number,
  limit: number = 100
) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [history, setHistory] = useState<IncomeEntry[]>([]);
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

    if (typeof (activeAdapter as any).fetchIncomeHistory !== 'function') {
      // Try alternative method name
      if (typeof (activeAdapter as any).fetchIncome !== 'function') {
        setIsSupported(false);
        return;
      }
    }

    setIsLoading(true);
    setError(null);

    try {
      const method = (activeAdapter as any).fetchIncomeHistory || (activeAdapter as any).fetchIncome;
      const raw = await method(incomeType, symbol, since, limit);

      const normalized: IncomeEntry[] = (raw || []).map((entry: any) => ({
        id: entry.id,
        symbol: entry.symbol,
        incomeType: entry.incomeType || entry.type,
        income: entry.income || entry.amount,
        asset: entry.asset || entry.currency,
        timestamp: entry.timestamp || entry.time,
        datetime: entry.datetime || new Date(entry.timestamp || entry.time).toISOString(),
        info: entry.info,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setHistory(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useIncomeHistory] Failed:', error);
      setError(error);
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, incomeType, symbol, since, limit, tradingMode]);

  useEffect(() => {
    fetchHistory();
  }, [fetchHistory]);

  return { history, isLoading, error, isSupported, refresh: fetchHistory };
}
