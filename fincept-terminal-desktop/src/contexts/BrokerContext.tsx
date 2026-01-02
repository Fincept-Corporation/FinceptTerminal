// File: src/contexts/BrokerContext.tsx
// Unified broker management context for multi-broker trading
// Optimized and performance-oriented with proper cleanup and error handling

import React, { createContext, useContext, useState, useEffect, useCallback, useMemo, useRef, ReactNode } from 'react';
import {
  BROKER_REGISTRY,
  getAllBrokers,
  getBrokerMetadata,
  createBrokerAdapter,
  getDefaultSymbols,
  type BrokerMetadata,
  type BrokerFeatures,
  detectBrokerFeatures,
  createFeatureChecker,
} from '../brokers';
import { createPaperTradingAdapter } from '../paper-trading';
import type { IExchangeAdapter } from '../brokers/crypto/types';
import type { PaperTradingAdapter } from '../paper-trading/PaperTradingAdapter';
import { websocketBridge, type ProviderConfig, type ConnectionStatus } from '../services/websocketBridge';

// Constants
const CONNECTION_TIMEOUT = 10000; // 10 seconds
const CONNECTION_RETRY_ATTEMPTS = 3;
const CONNECTION_RETRY_DELAY = 2000; // 2 seconds

// Helper to validate localStorage values
function validateTradingMode(value: string | null): 'live' | 'paper' | null {
  if (value === 'live' || value === 'paper') return value;
  return null;
}

function validatePaperPortfolioMode(value: string | null): 'separate' | 'unified' {
  if (value === 'separate' || value === 'unified') return value;
  return 'separate';
}

// Helper to safely access localStorage
function safeLocalStorageGet(key: string): string | null {
  try {
    return localStorage.getItem(key);
  } catch (error) {
    return null;
  }
}

function safeLocalStorageSet(key: string, value: string): void {
  try {
    localStorage.setItem(key, value);
  } catch (error) {
  }
}

// Helper to create timeout promise
function createTimeout(ms: number): Promise<never> {
  return new Promise((_, reject) =>
    setTimeout(() => reject(new Error(`Connection timeout after ${ms}ms`)), ms)
  );
}

// Helper to retry connection with exponential backoff
async function retryConnect(
  connectFn: () => Promise<void>,
  attempts: number = CONNECTION_RETRY_ATTEMPTS,
  delay: number = CONNECTION_RETRY_DELAY
): Promise<void> {
  for (let i = 0; i < attempts; i++) {
    try {
      await Promise.race([connectFn(), createTimeout(CONNECTION_TIMEOUT)]);
      return;
    } catch (error) {
      if (i === attempts - 1) throw error;
      await new Promise(resolve => setTimeout(resolve, delay * (i + 1))); // Exponential backoff
    }
  }
}

interface BrokerContextType {
  // Active broker
  activeBroker: string;
  activeBrokerMetadata: BrokerMetadata | null;
  setActiveBroker: (brokerId: string) => Promise<void>;

  // Available brokers
  availableBrokers: BrokerMetadata[];

  // Trading mode
  tradingMode: 'live' | 'paper';
  setTradingMode: (mode: 'live' | 'paper') => void;

  // Paper portfolio mode
  paperPortfolioMode: 'separate' | 'unified';
  setPaperPortfolioMode: (mode: 'separate' | 'unified') => void;

  // Broker adapters
  realAdapter: IExchangeAdapter | null;
  paperAdapter: PaperTradingAdapter | null;
  activeAdapter: IExchangeAdapter | PaperTradingAdapter | null;

  // Broker capabilities
  features: BrokerFeatures | null;
  supports: ReturnType<typeof createFeatureChecker> | null;

  // Symbol management
  defaultSymbols: string[];

  // WebSocket
  providerConfigs: Map<string, ProviderConfig>;
  updateProviderConfig: (provider: string, config: Partial<ProviderConfig>) => Promise<void>;

  // Loading state
  isLoading: boolean;
  isConnecting: boolean;
}

const BrokerContext = createContext<BrokerContextType | null>(null);

interface BrokerProviderProps {
  children: ReactNode;
}

export function BrokerProvider({ children }: BrokerProviderProps) {

  const [activeBroker, setActiveBrokerState] = useState<string>(() => {
    return safeLocalStorageGet('active_broker') || 'kraken';
  });
  const [tradingMode, setTradingModeState] = useState<'live' | 'paper'>('paper');
  const [paperPortfolioMode, setPaperPortfolioModeState] = useState<'separate' | 'unified'>(() => {
    return validatePaperPortfolioMode(safeLocalStorageGet('paper_portfolio_mode'));
  });
  const [providerConfigs, setProviderConfigs] = useState<Map<string, ProviderConfig>>(new Map());
  const [isLoading, setIsLoading] = useState(true);
  const [isConnecting, setIsConnecting] = useState(false);

  // Adapter instances
  const [realAdapter, setRealAdapter] = useState<IExchangeAdapter | null>(null);
  const [paperAdapter, setPaperAdapter] = useState<PaperTradingAdapter | null>(null);

  // Refs for cleanup and abort control
  const abortControllerRef = useRef<AbortController | null>(null);
  const hasInitializedRef = useRef(false); // Prevent duplicate initialization

  // Get available brokers (memoized once, doesn't change)
  const availableBrokers = useMemo(() => {
    return getAllBrokers().filter(b => b.type === 'crypto');
  }, []);

  // Get broker metadata
  const activeBrokerMetadata = useMemo(() => {
    return getBrokerMetadata(activeBroker) || null;
  }, [activeBroker]);

  // Get default symbols
  const defaultSymbols = useMemo(() => {
    return getDefaultSymbols(activeBroker);
  }, [activeBroker]);

  // Detect features
  const features = useMemo(() => {
    const adapter = tradingMode === 'paper' ? paperAdapter : realAdapter;
    return adapter ? detectBrokerFeatures(adapter) : null;
  }, [realAdapter, paperAdapter, tradingMode]);

  // Create feature checker
  const supports = useMemo(() => {
    return features ? createFeatureChecker(features) : null;
  }, [features]);

  // Get active adapter
  const activeAdapter = useMemo(() => {
    return tradingMode === 'paper' ? paperAdapter : realAdapter;
  }, [tradingMode, realAdapter, paperAdapter]);

  // Get broker fees from metadata
  const getBrokerFees = useCallback((metadata: BrokerMetadata | null) => {
    if (!metadata?.fees) {
      return { maker: 0.001, taker: 0.001 }; // Default 0.1%
    }
    return {
      maker: metadata.fees.maker || 0.001,
      taker: metadata.fees.taker || 0.001,
    };
  }, []);

  // Load configurations on mount
  useEffect(() => {
    const loadConfigurations = async () => {

      setIsLoading(true);

      try {
        const configs = new Map<string, ProviderConfig>();

        // Initialize configs for all available brokers
        for (const broker of availableBrokers) {
          if (broker.websocket.enabled && broker.websocket.endpoint) {
            configs.set(broker.id, {
              name: broker.id,
              url: broker.websocket.endpoint,
              enabled: true,
              reconnect_delay_ms: 5000,
              max_reconnect_attempts: 10,
              heartbeat_interval_ms: 30000,
            });
          }
        }

        setProviderConfigs(configs);

        // Initialize provider configs in Rust WebSocket backend
        for (const [providerId, config] of configs.entries()) {
          try {
            await websocketBridge.setConfig(config);
          } catch (error) {
            console.error(`[BrokerContext] ✗ Failed to configure provider ${providerId}:`, error);
          }
        }

        // Load saved preferences with validation
        const savedBroker = safeLocalStorageGet('active_trading_broker');
        const savedMode = validateTradingMode(safeLocalStorageGet('trading_mode'));

        if (savedBroker && BROKER_REGISTRY[savedBroker]) {
          setActiveBrokerState(savedBroker);
        }

        if (savedMode) {
          setTradingModeState(savedMode);
        }

      } catch (error) {
        console.error('[BrokerContext] Failed to load configurations:', error);
      } finally {
        setIsLoading(false);

        // Trigger initialization only once
        if (!hasInitializedRef.current) {
          hasInitializedRef.current = true;
          // Use queueMicrotask to avoid timing issues with React.Suspense boundaries
          queueMicrotask(() => initializeAdapters());
        }
      }
    };

    loadConfigurations();
  }, []); // Only run once on mount

  // Cleanup adapters helper
  const cleanupAdapters = useCallback(async (
    real: IExchangeAdapter | null,
    paper: PaperTradingAdapter | null
  ) => {
    try {
      if (real) {
        try {
          await Promise.race([real.disconnect(), createTimeout(5000)]);
        } catch (err) {
          console.error('[BrokerContext] Real adapter disconnect error:', err);
        }
      }
      if (paper) {
        try {
          await Promise.race([paper.disconnect(), createTimeout(5000)]);
        } catch (err) {
          console.error('[BrokerContext] Paper adapter disconnect error:', err);
        }
      }
    } catch (err) {
      console.error('[BrokerContext] Cleanup error:', err);
    }
  }, []);

  // Initialize adapters function
  const initializeAdapters = useCallback(async () => {

    // Create new abort controller for this initialization
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();
    const signal = abortControllerRef.current.signal;

    // Don't check isMountedRef here - it causes issues with lazy loading and Suspense
    // The abort signal is sufficient to prevent stale operations
    if (signal.aborted) {
      return;
    }

    setIsConnecting(true);

    try {

      // Clean up old adapters first
      await cleanupAdapters(realAdapter, paperAdapter);

      if (signal.aborted) return;

      // Create real adapter
      const newRealAdapter = createBrokerAdapter(activeBroker);

      try {
        await retryConnect(() => newRealAdapter.connect());
      } catch (connectError) {
        console.error(`[BrokerContext] [WARN] Failed to connect ${activeBroker} real adapter after retries:`, connectError);
      }

      if (signal.aborted) {
        await cleanupAdapters(newRealAdapter, null);
        return;
      }

      // Update real adapter state
      setRealAdapter(newRealAdapter);

      // Create paper adapter (wrapping real adapter)
      const portfolioId = paperPortfolioMode === 'unified'
        ? 'paper_global'
        : `paper_${activeBroker}`;

      const fees = getBrokerFees(activeBrokerMetadata);

      const newPaperAdapter = createPaperTradingAdapter(
        {
          portfolioId,
          portfolioName: paperPortfolioMode === 'unified'
            ? 'Global Paper Trading Portfolio'
            : `${activeBroker.toUpperCase()} Paper Trading`,
          provider: activeBroker,
          assetClass: 'crypto',
          initialBalance: 100000,
          currency: 'USD',
          fees,
          slippage: {
            market: 0.001, // 0.1% realistic slippage
            limit: 0,
          },
          enableMarginTrading: true,
          defaultLeverage: 1,
          maxLeverage: activeBrokerMetadata?.advancedFeatures.maxLeverage || 1,
        },
        newRealAdapter
      );

      if (signal.aborted) {
        await cleanupAdapters(newRealAdapter, newPaperAdapter);
        return;
      }

      try {
        await retryConnect(() => newPaperAdapter.connect());
      } catch (connectError) {
        console.error(`[BrokerContext] [WARN] Failed to connect paper adapter:`, connectError);
        throw connectError; // Paper adapter is critical
      }

      if (signal.aborted) {
        await cleanupAdapters(newRealAdapter, newPaperAdapter);
        return;
      }

      // Update paper adapter state
      setPaperAdapter(newPaperAdapter);

    } catch (error) {
      if (signal.aborted) return;

      console.error(`[BrokerContext] [FAIL] Failed to initialize ${activeBroker}:`, error);
      console.error(`[BrokerContext] Error details:`, error);
      console.error(`[BrokerContext] Stack trace:`, (error as Error)?.stack);

      // Set adapters to null on failure
      setRealAdapter(null);
      setPaperAdapter(null);
    } finally {
      setIsConnecting(false);
    }
  }, [activeBroker, paperPortfolioMode, activeBrokerMetadata, cleanupAdapters, realAdapter, paperAdapter, getBrokerFees]);

  // Set active broker
  const setActiveBroker = useCallback(async (brokerId: string) => {
    if (!BROKER_REGISTRY[brokerId]) {
      throw new Error(`Broker ${brokerId} is not available`);
    }


    // Abort any ongoing initialization
    abortControllerRef.current?.abort();

    // Update state
    setActiveBrokerState(brokerId);

    // Save preference
    safeLocalStorageSet('active_broker', brokerId);

    // Re-initialize adapters
    await initializeAdapters();
  }, [initializeAdapters]);

  // Set trading mode
  const setTradingMode = useCallback((mode: 'live' | 'paper') => {
    setTradingModeState(mode);
    safeLocalStorageSet('trading_mode', mode);
  }, []);

  // Set paper portfolio mode
  const setPaperPortfolioMode = useCallback(async (mode: 'separate' | 'unified') => {

    // Abort any ongoing initialization
    abortControllerRef.current?.abort();

    // Update state
    setPaperPortfolioModeState(mode);
    safeLocalStorageSet('paper_portfolio_mode', mode);

    // Re-initialize adapters
    await initializeAdapters();
  }, [initializeAdapters]);

  // Update provider config
  const updateProviderConfig = useCallback(async (
    provider: string,
    config: Partial<ProviderConfig>
  ) => {
    setProviderConfigs(prev => {
      const newConfigs = new Map(prev);
      const existingConfig = newConfigs.get(provider);

      const updatedConfig: ProviderConfig = {
        ...existingConfig,
        ...config,
        name: provider, // Use 'name' instead of 'provider_name'
      } as ProviderConfig;

      newConfigs.set(provider, updatedConfig);

      // Update config in Rust WebSocket backend
      websocketBridge.setConfig(updatedConfig).catch(error => {
        console.error(`[BrokerContext] Failed to update provider config for ${provider}:`, error);
      });

      return newConfigs;
    });
  }, []);

  // Memoize context value to prevent unnecessary re-renders
  const contextValue = useMemo(() => ({
    activeBroker,
    activeBrokerMetadata,
    setActiveBroker,
    availableBrokers,
    tradingMode,
    setTradingMode,
    paperPortfolioMode,
    setPaperPortfolioMode,
    realAdapter,
    paperAdapter,
    activeAdapter,
    features,
    supports,
    defaultSymbols,
    providerConfigs,
    updateProviderConfig,
    isLoading,
    isConnecting,
  }), [
    activeBroker,
    activeBrokerMetadata,
    setActiveBroker,
    availableBrokers,
    tradingMode,
    setTradingMode,
    paperPortfolioMode,
    setPaperPortfolioMode,
    realAdapter,
    paperAdapter,
    activeAdapter,
    features,
    supports,
    defaultSymbols,
    providerConfigs,
    updateProviderConfig,
    isLoading,
    isConnecting,
  ]);

  return (
    <BrokerContext.Provider value={contextValue}>
      {children}
    </BrokerContext.Provider>
  );
}

export function useBrokerContext(): BrokerContextType {
  const context = useContext(BrokerContext);

  if (!context) {
    throw new Error('useBrokerContext must be used within a BrokerProvider');
  }

  return context;
}

/**
 * Hook for quick broker selection
 */
export function useBrokerSelection() {
  const { activeBroker, availableBrokers, setActiveBroker, isLoading } = useBrokerContext();

  return {
    broker: activeBroker,
    brokers: availableBrokers,
    setBroker: setActiveBroker,
    isLoading,
  };
}

/**
 * Hook for feature checks
 */
export function useBrokerFeatures() {
  const { features, supports } = useBrokerContext();

  return {
    features,
    supports,
    hasFeature: (category: keyof BrokerFeatures, feature: string) => {
      if (!features) return false;
      const categoryFeatures = features[category];
      if (!categoryFeatures || typeof categoryFeatures !== 'object') return false;
      return (categoryFeatures as Record<string, boolean>)[feature] === true;
    },
  };
}

/**
 * Hook for adapter access
 */
export function useBrokerAdapter() {
  const { activeAdapter, realAdapter, paperAdapter, tradingMode } = useBrokerContext();

  return {
    adapter: activeAdapter,
    realAdapter,
    paperAdapter,
    tradingMode,
  };
}
