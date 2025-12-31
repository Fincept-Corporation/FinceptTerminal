/**
 * useBrokerState Hook
 * Multi-broker state management for equity trading
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import {
  BrokerType,
  AuthStatus,
  UnifiedOrder,
  UnifiedPosition,
  UnifiedHolding,
  UnifiedFunds,
  BrokerStatus,
} from '../core/types';
import { authManager } from '../services/AuthManager';
import { brokerOrchestrator } from '../core/BrokerOrchestrator';

interface BrokerState {
  authStatus: Map<BrokerType, AuthStatus>;
  orders: Map<BrokerType, UnifiedOrder[]>;
  positions: Map<BrokerType, UnifiedPosition[]>;
  holdings: Map<BrokerType, UnifiedHolding[]>;
  funds: Map<BrokerType, UnifiedFunds>;
  loading: {
    auth: boolean;
    orders: boolean;
    positions: boolean;
    holdings: boolean;
    funds: boolean;
  };
  errors: {
    auth?: string;
    orders?: string;
    positions?: string;
    holdings?: string;
    funds?: string;
  };
}

export function useBrokerState(brokers: BrokerType[] = ['fyers', 'kite']) {
  const [state, setState] = useState<BrokerState>({
    authStatus: new Map(),
    orders: new Map(),
    positions: new Map(),
    holdings: new Map(),
    funds: new Map(),
    loading: { auth: false, orders: false, positions: false, holdings: false, funds: false },
    errors: {},
  });

  const refreshIntervalRef = useRef<ReturnType<typeof setInterval> | undefined>(undefined);

  // Initialize authentication
  const initializeBrokers = useCallback(async () => {
    setState(prev => ({ ...prev, loading: { ...prev.loading, auth: true }, errors: { ...prev.errors, auth: undefined } }));

    try {
      const statuses = new Map<BrokerType, AuthStatus>();

      for (const brokerId of brokers) {
        // Check if already authenticated to avoid redundant initialization
        const currentStatus = authManager.getAuthStatus(brokerId);

        if (currentStatus?.authenticated) {
          // Already authenticated, use existing status
          statuses.set(brokerId, currentStatus);
          brokerOrchestrator.enableBroker(brokerId);
        } else {
          // Not authenticated, try to initialize
          const status = await authManager.initializeBroker(brokerId);
          statuses.set(brokerId, status);

          if (status.authenticated) {
            brokerOrchestrator.enableBroker(brokerId);
          }
        }
      }

      setState(prev => ({
        ...prev,
        authStatus: statuses,
        loading: { ...prev.loading, auth: false },
      }));
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        loading: { ...prev.loading, auth: false },
        errors: { ...prev.errors, auth: error.message },
      }));
    }
  }, [brokers]);

  // Fetch orders from all brokers
  const fetchOrders = useCallback(async () => {
    setState(prev => ({ ...prev, loading: { ...prev.loading, orders: true }, errors: { ...prev.errors, orders: undefined } }));

    try {
      const ordersMap = await brokerOrchestrator.getAllOrders();
      setState(prev => ({
        ...prev,
        orders: ordersMap,
        loading: { ...prev.loading, orders: false },
      }));
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        loading: { ...prev.loading, orders: false },
        errors: { ...prev.errors, orders: error.message },
      }));
    }
  }, []);

  // Fetch positions from all brokers
  const fetchPositions = useCallback(async () => {
    setState(prev => ({ ...prev, loading: { ...prev.loading, positions: true }, errors: { ...prev.errors, positions: undefined } }));

    try {
      const positionsMap = await brokerOrchestrator.getAllPositions();
      setState(prev => ({
        ...prev,
        positions: positionsMap,
        loading: { ...prev.loading, positions: false },
      }));
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        loading: { ...prev.loading, positions: false },
        errors: { ...prev.errors, positions: error.message },
      }));
    }
  }, []);

  // Fetch holdings from all brokers
  const fetchHoldings = useCallback(async () => {
    setState(prev => ({ ...prev, loading: { ...prev.loading, holdings: true }, errors: { ...prev.errors, holdings: undefined } }));

    try {
      const holdingsMap = await brokerOrchestrator.getAllHoldings();
      setState(prev => ({
        ...prev,
        holdings: holdingsMap,
        loading: { ...prev.loading, holdings: false },
      }));
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        loading: { ...prev.loading, holdings: false },
        errors: { ...prev.errors, holdings: error.message },
      }));
    }
  }, []);

  // Fetch funds from all brokers
  const fetchFunds = useCallback(async () => {
    setState(prev => ({ ...prev, loading: { ...prev.loading, funds: true }, errors: { ...prev.errors, funds: undefined } }));

    try {
      const fundsMap = await brokerOrchestrator.getAllFunds();
      setState(prev => ({
        ...prev,
        funds: fundsMap,
        loading: { ...prev.loading, funds: false },
      }));
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        loading: { ...prev.loading, funds: false },
        errors: { ...prev.errors, funds: error.message },
      }));
    }
  }, []);

  // Refresh all data
  const refreshAll = useCallback(async () => {
    await Promise.all([
      fetchOrders(),
      fetchPositions(),
      fetchHoldings(),
      fetchFunds(),
    ]);
  }, [fetchOrders, fetchPositions, fetchHoldings, fetchFunds]);

  // Get active brokers
  const getActiveBrokers = useCallback(() => {
    return Array.from(state.authStatus.entries())
      .filter(([_, status]) => status.authenticated)
      .map(([brokerId]) => brokerId);
  }, [state.authStatus]);

  // Check if broker is authenticated
  const isAuthenticated = useCallback((brokerId: BrokerType) => {
    return state.authStatus.get(brokerId)?.authenticated || false;
  }, [state.authStatus]);

  // Get aggregated orders
  const getAllOrders = useCallback(() => {
    const allOrders: UnifiedOrder[] = [];
    state.orders.forEach(orders => allOrders.push(...orders));
    return allOrders.sort((a, b) => b.orderTime.getTime() - a.orderTime.getTime());
  }, [state.orders]);

  // Get aggregated positions
  const getAllPositions = useCallback(() => {
    const allPositions: UnifiedPosition[] = [];
    state.positions.forEach(positions => allPositions.push(...positions));
    return allPositions;
  }, [state.positions]);

  // Get aggregated holdings
  const getAllHoldings = useCallback(() => {
    const allHoldings: UnifiedHolding[] = [];
    state.holdings.forEach(holdings => allHoldings.push(...holdings));
    return allHoldings;
  }, [state.holdings]);

  // Get total P&L across all brokers
  const getTotalPnL = useCallback(() => {
    let total = 0;
    state.positions.forEach(positions => {
      positions.forEach(pos => total += pos.pnl);
    });
    state.holdings.forEach(holdings => {
      holdings.forEach(hold => total += hold.pnl);
    });
    return total;
  }, [state.positions, state.holdings]);

  // Get total funds across all brokers
  const getTotalFunds = useCallback(() => {
    let totalCash = 0;
    let totalMargin = 0;

    state.funds.forEach(fund => {
      totalCash += fund.availableCash;
      totalMargin += fund.availableMargin;
    });

    return { totalCash, totalMargin };
  }, [state.funds]);

  // Initialize on mount
  useEffect(() => {
    initializeBrokers();

    // Listen to auth status changes
    const unsubscribe = authManager.onAuthStatusChange((brokerId, status) => {
      setState(prev => {
        const newAuthStatus = new Map(prev.authStatus);
        newAuthStatus.set(brokerId, status);
        return { ...prev, authStatus: newAuthStatus };
      });

      // Enable/disable broker in orchestrator
      if (status.authenticated) {
        brokerOrchestrator.enableBroker(brokerId);
      } else {
        brokerOrchestrator.disableBroker(brokerId);
      }
    });

    return () => {
      unsubscribe();
      if (refreshIntervalRef.current) {
        clearInterval(refreshIntervalRef.current);
      }
    };
  }, [initializeBrokers]);

  // Auto-refresh data
  const startAutoRefresh = useCallback((intervalMs: number = 5000) => {
    if (refreshIntervalRef.current) {
      clearInterval(refreshIntervalRef.current);
    }

    refreshIntervalRef.current = setInterval(() => {
      refreshAll();
    }, intervalMs);
  }, [refreshAll]);

  const stopAutoRefresh = useCallback(() => {
    if (refreshIntervalRef.current !== undefined) {
      clearInterval(refreshIntervalRef.current);
      refreshIntervalRef.current = undefined;
    }
  }, []);

  return {
    // State
    authStatus: state.authStatus,
    orders: state.orders,
    positions: state.positions,
    holdings: state.holdings,
    funds: state.funds,
    loading: state.loading,
    errors: state.errors,

    // Actions
    initializeBrokers,
    fetchOrders,
    fetchPositions,
    fetchHoldings,
    fetchFunds,
    refreshAll,
    startAutoRefresh,
    stopAutoRefresh,

    // Getters
    getActiveBrokers,
    isAuthenticated,
    getAllOrders,
    getAllPositions,
    getAllHoldings,
    getTotalPnL,
    getTotalFunds,
  };
}
