/**
 * Funding Hooks
 * Handles deposits, withdrawals, and deposit addresses
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface UnifiedTransaction {
  id: string;
  txid?: string;
  type: 'deposit' | 'withdrawal';
  currency: string;
  amount: number;
  status: 'pending' | 'ok' | 'failed' | 'canceled';
  address?: string;
  tag?: string;
  fee?: {
    cost: number;
    currency: string;
  };
  timestamp: number;
  datetime: string;
  network?: string;
}

export interface DepositAddress {
  currency: string;
  address: string;
  tag?: string;
  network?: string;
  info?: any;
}

export interface WithdrawParams {
  code: string;
  amount: number;
  address: string;
  tag?: string;
  network?: string;
}

// ============================================================================
// DEPOSITS HOOK
// ============================================================================

export function useDeposits(currency?: string, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [deposits, setDeposits] = useState<UnifiedTransaction[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchDeposits = useCallback(async () => {
    if (tradingMode === 'paper') {
      setDeposits([]);
      setIsSupported(false);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setDeposits([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchDeposits !== 'function') {
      setIsSupported(false);
      setDeposits([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawDeposits = await (activeAdapter as any).fetchDeposits(currency, undefined, limit);

      const normalized: UnifiedTransaction[] = (rawDeposits || []).map((d: any) => ({
        id: d.id,
        txid: d.txid,
        type: 'deposit' as const,
        currency: d.currency,
        amount: d.amount || 0,
        status: normalizeStatus(d.status),
        address: d.address,
        tag: d.tag,
        fee: d.fee ? { cost: d.fee.cost || 0, currency: d.fee.currency || d.currency } : undefined,
        timestamp: d.timestamp || Date.now(),
        datetime: d.datetime || new Date().toISOString(),
        network: d.network,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setDeposits(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useDeposits] Failed to fetch deposits:', error);
      setError(error);

      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, currency, limit, tradingMode]);

  useEffect(() => {
    fetchDeposits();
  }, [fetchDeposits]);

  return {
    deposits,
    isLoading,
    error,
    isSupported,
    refresh: fetchDeposits,
  };
}

// ============================================================================
// WITHDRAWALS HOOK
// ============================================================================

export function useWithdrawals(currency?: string, limit: number = 50) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [withdrawals, setWithdrawals] = useState<UnifiedTransaction[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchWithdrawals = useCallback(async () => {
    if (tradingMode === 'paper') {
      setWithdrawals([]);
      setIsSupported(false);
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setWithdrawals([]);
      return;
    }

    if (typeof (activeAdapter as any).fetchWithdrawals !== 'function') {
      setIsSupported(false);
      setWithdrawals([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawWithdrawals = await (activeAdapter as any).fetchWithdrawals(currency, undefined, limit);

      const normalized: UnifiedTransaction[] = (rawWithdrawals || []).map((w: any) => ({
        id: w.id,
        txid: w.txid,
        type: 'withdrawal' as const,
        currency: w.currency,
        amount: w.amount || 0,
        status: normalizeStatus(w.status),
        address: w.address,
        tag: w.tag,
        fee: w.fee ? { cost: w.fee.cost || 0, currency: w.fee.currency || w.currency } : undefined,
        timestamp: w.timestamp || Date.now(),
        datetime: w.datetime || new Date().toISOString(),
        network: w.network,
      }));

      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setWithdrawals(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useWithdrawals] Failed to fetch withdrawals:', error);
      setError(error);

      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, currency, limit, tradingMode]);

  useEffect(() => {
    fetchWithdrawals();
  }, [fetchWithdrawals]);

  return {
    withdrawals,
    isLoading,
    error,
    isSupported,
    refresh: fetchWithdrawals,
  };
}

// ============================================================================
// DEPOSIT ADDRESS HOOK
// ============================================================================

export function useDepositAddress(currency?: string, network?: string) {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [address, setAddress] = useState<DepositAddress | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchAddress = useCallback(async () => {
    if (tradingMode === 'paper' || !currency) {
      setAddress(null);
      setIsSupported(tradingMode !== 'paper');
      return;
    }

    if (!activeAdapter || !activeAdapter.isConnected()) {
      setAddress(null);
      return;
    }

    if (typeof (activeAdapter as any).fetchDepositAddress !== 'function') {
      setIsSupported(false);
      setAddress(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const params = network ? { network } : undefined;
      const rawAddress = await (activeAdapter as any).fetchDepositAddress(currency, params);

      setAddress({
        currency: rawAddress.currency || currency,
        address: rawAddress.address,
        tag: rawAddress.tag,
        network: rawAddress.network || network,
        info: rawAddress.info,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useDepositAddress] Failed to fetch address:', error);
      setError(error);
      setAddress(null);

      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, currency, network, tradingMode]);

  useEffect(() => {
    if (currency) {
      fetchAddress();
    }
  }, [fetchAddress, currency]);

  return {
    address,
    isLoading,
    error,
    isSupported,
    refresh: fetchAddress,
  };
}

// ============================================================================
// WITHDRAW HOOK
// ============================================================================

export function useWithdraw() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastResult, setLastResult] = useState<any | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const withdraw = useCallback(
    async ({ code, amount, address, tag, network }: WithdrawParams) => {
      if (tradingMode === 'paper') {
        setError(new Error('Withdrawals not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).withdraw !== 'function') {
        setIsSupported(false);
        setError(new Error('Withdrawals not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const params = network ? { network } : undefined;
        const result = await (activeAdapter as any).withdraw(code, amount, address, tag, params);
        setLastResult(result);
        setIsSupported(true);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useWithdraw] Withdrawal failed:', error);
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

  return {
    withdraw,
    isLoading,
    error,
    lastResult,
    isSupported,
  };
}

// ============================================================================
// COMBINED TRANSACTIONS HOOK
// ============================================================================

export function useTransactions(currency?: string, limit: number = 50) {
  const { deposits, isLoading: depositsLoading, error: depositsError, isSupported: depositsSupported, refresh: refreshDeposits } = useDeposits(currency, limit);
  const { withdrawals, isLoading: withdrawalsLoading, error: withdrawalsError, isSupported: withdrawalsSupported, refresh: refreshWithdrawals } = useWithdrawals(currency, limit);

  const transactions = [...deposits, ...withdrawals].sort((a, b) => b.timestamp - a.timestamp);

  const refresh = useCallback(() => {
    refreshDeposits();
    refreshWithdrawals();
  }, [refreshDeposits, refreshWithdrawals]);

  return {
    transactions,
    deposits,
    withdrawals,
    isLoading: depositsLoading || withdrawalsLoading,
    error: depositsError || withdrawalsError,
    isSupported: depositsSupported || withdrawalsSupported,
    refresh,
  };
}

// ============================================================================
// HELPERS
// ============================================================================

function normalizeStatus(status: string): 'pending' | 'ok' | 'failed' | 'canceled' {
  const statusLower = String(status || '').toLowerCase();

  if (statusLower === 'ok' || statusLower === 'success' || statusLower === 'completed' || statusLower === 'confirmed') {
    return 'ok';
  }
  if (statusLower === 'pending' || statusLower === 'processing' || statusLower === 'confirming') {
    return 'pending';
  }
  if (statusLower === 'failed' || statusLower === 'rejected' || statusLower === 'error') {
    return 'failed';
  }
  if (statusLower === 'canceled' || statusLower === 'cancelled') {
    return 'canceled';
  }

  return 'pending'; // Default to pending for unknown statuses
}
