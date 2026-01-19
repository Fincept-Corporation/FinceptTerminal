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
import * as paperTrading from '../paper-trading';
import { createPaperTradingAdapter, PaperTradingAdapter, type Portfolio } from '../paper-trading';
import type { IExchangeAdapter } from '../brokers/crypto/types';
import { websocketBridge, type ProviderConfig } from '../services/trading/websocketBridge';
import { saveSetting, getSetting } from '@/services/core/sqliteService';
import { initializeMarketDataService, getMarketDataService } from '../services/trading/UnifiedMarketDataService';

// Constants
const CONNECTION_TIMEOUT = 10000; // 10 seconds
const CONNECTION_RETRY_ATTEMPTS = 3;
const CONNECTION_RETRY_DELAY = 2000; // 2 seconds

// Helper to validate storage values
function validateTradingMode(value: string | null): 'live' | 'paper' | null {
  if (value === 'live' || value === 'paper') return value;
  return null;
}

function validatePaperPortfolioMode(value: string | null): 'separate' | 'unified' {
  if (value === 'separate' || value === 'unified') return value;
  return 'separate';
}

// Helper to safely access storage (SQLite)
async function safeStorageGet(key: string): Promise<string | null> {
  try {
    return await getSetting(key);
  } catch (error) {
    console.error(`[BrokerContext] Failed to get storage key "${key}":`, error);
    return null;
  }
}

async function safeStorageSet(key: string, value: string): Promise<void> {
  try {
    await saveSetting(key, value, 'broker');
  } catch (error) {
    console.error(`[BrokerContext] Failed to set storage key "${key}":`, error);
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
  paperPortfolio: Portfolio | null;
  activeAdapter: IExchangeAdapter | PaperTradingAdapter | null;

  // Paper trading functions (direct Rust calls)
  paperTradingApi: typeof paperTrading;

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

  const [activeBroker, setActiveBrokerState] = useState<string>('kraken');
  const [tradingMode, setTradingModeState] = useState<'live' | 'paper'>('paper');
  const [paperPortfolioMode, setPaperPortfolioModeState] = useState<'separate' | 'unified'>('separate');
  const [providerConfigs, setProviderConfigs] = useState<Map<string, ProviderConfig>>(new Map());
  const [isLoading, setIsLoading] = useState(true);
  const [isConnecting, setIsConnecting] = useState(false);

  // Adapter instances
  const [realAdapter, setRealAdapter] = useState<IExchangeAdapter | null>(null);
  const [paperAdapter, setPaperAdapter] = useState<PaperTradingAdapter | null>(null);
  const [paperPortfolio, setPaperPortfolio] = useState<Portfolio | null>(null);

  // Refs for cleanup and abort control
  const abortControllerRef = useRef<AbortController | null>(null);
  const hasInitializedRef = useRef(false); // Prevent duplicate initialization
  const marketDataInitializedRef = useRef(false); // Track market data service init

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
    return realAdapter ? detectBrokerFeatures(realAdapter) : null;
  }, [realAdapter]);

  // Create feature checker
  const supports = useMemo(() => {
    return features ? createFeatureChecker(features) : null;
  }, [features]);

  // Get active adapter based on trading mode
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
        const savedBroker = await safeStorageGet('active_trading_broker');
        const savedModeStr = await safeStorageGet('trading_mode');
        const savedMode = validateTradingMode(savedModeStr);
        const savedPaperMode = await safeStorageGet('paper_portfolio_mode');

        if (savedBroker && BROKER_REGISTRY[savedBroker]) {
          setActiveBrokerState(savedBroker);
        }

        if (savedMode) {
          setTradingModeState(savedMode);
        }

        if (savedPaperMode) {
          setPaperPortfolioModeState(validatePaperPortfolioMode(savedPaperMode));
        }

      } catch (error) {
        console.error('[BrokerContext] Failed to load configurations:', error);
      } finally {
        setIsLoading(false);

        // Initialize market data service (only once)
        if (!marketDataInitializedRef.current) {
          marketDataInitializedRef.current = true;
          initializeMarketDataService()
            .then(() => console.log('[BrokerContext] Market data service initialized'))
            .catch((err) => console.error('[BrokerContext] Market data service init failed:', err));
        }

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
    real: IExchangeAdapter | null
  ) => {
    try {
      if (real) {
        try {
          await Promise.race([real.disconnect(), createTimeout(5000)]);
        } catch (err) {
          console.error('[BrokerContext] Real adapter disconnect error:', err);
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

    if (signal.aborted) return;

    setIsConnecting(true);

    try {
      // Clean up old adapters first
      await cleanupAdapters(realAdapter);

      if (signal.aborted) return;

      // Create real adapter
      const newRealAdapter = createBrokerAdapter(activeBroker);

      try {
        await retryConnect(() => newRealAdapter.connect());
      } catch (connectError) {
        console.error(`[BrokerContext] [WARN] Failed to connect ${activeBroker} real adapter after retries:`, connectError);
      }

      if (signal.aborted) {
        await cleanupAdapters(newRealAdapter);
        return;
      }

      // Update real adapter state
      setRealAdapter(newRealAdapter);

      // Create or get paper portfolio from Rust backend
      const portfolioName = paperPortfolioMode === 'unified'
        ? 'Global Paper Trading Portfolio'
        : `${activeBroker.toUpperCase()} Paper Trading`;

      const fees = getBrokerFees(activeBrokerMetadata);

      try {
        // Create paper trading adapter with market data service (NO realAdapter dependency)
        // This allows paper trading to work independently of real broker connection
        const marketDataService = getMarketDataService();
        const newPaperAdapter = createPaperTradingAdapter(
          {
            portfolioId: '',
            portfolioName,
            provider: activeBroker,
            assetClass: 'crypto',
            initialBalance: 100000,
            currency: 'USD',
            fees,
            slippage: { market: 0.001, limit: 0 },
            enableMarginTrading: true,
            defaultLeverage: 1,
            maxLeverage: activeBrokerMetadata?.advancedFeatures.maxLeverage || 1,
          },
          marketDataService
        );

        await newPaperAdapter.connect();

        if (signal.aborted) {
          await newPaperAdapter.disconnect();
          return;
        }

        setPaperAdapter(newPaperAdapter);

        // Also get the portfolio for direct Rust API access
        const portfolio = await paperTrading.getPortfolio(newPaperAdapter.portfolioId);
        setPaperPortfolio(portfolio);
      } catch (error) {
        console.error('[BrokerContext] Failed to create paper adapter:', error);
        setPaperAdapter(null);
        setPaperPortfolio(null);
      }

    } catch (error) {
      if (signal.aborted) return;

      console.error(`[BrokerContext] [FAIL] Failed to initialize ${activeBroker}:`, error);

      setRealAdapter(null);
      setPaperAdapter(null);
      setPaperPortfolio(null);
    } finally {
      setIsConnecting(false);
    }
  }, [activeBroker, paperPortfolioMode, activeBrokerMetadata, cleanupAdapters, realAdapter, getBrokerFees]);

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
    await safeStorageSet('active_broker', brokerId);

    // Re-initialize adapters
    await initializeAdapters();
  }, [initializeAdapters]);

  // Set trading mode
  const setTradingMode = useCallback(async (mode: 'live' | 'paper') => {
    setTradingModeState(mode);
    await safeStorageSet('trading_mode', mode);
  }, []);

  // Set paper portfolio mode
  const setPaperPortfolioMode = useCallback(async (mode: 'separate' | 'unified') => {

    // Abort any ongoing initialization
    abortControllerRef.current?.abort();

    // Update state
    setPaperPortfolioModeState(mode);
    await safeStorageSet('paper_portfolio_mode', mode);

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
      websocketBridge.setConfig(updatedConfig).catch((error: unknown) => {
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
    paperPortfolio,
    activeAdapter,
    paperTradingApi: paperTrading,
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
    paperPortfolio,
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
  const { activeAdapter, realAdapter, paperAdapter, paperPortfolio, paperTradingApi, tradingMode } = useBrokerContext();

  return {
    adapter: activeAdapter,
    realAdapter,
    paperAdapter,
    paperPortfolio,
    paperTradingApi,
    tradingMode,
  };
}
