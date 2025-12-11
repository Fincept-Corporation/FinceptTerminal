// File: src/contexts/BrokerContext.tsx
// Unified broker management context for multi-broker trading
// Replaces ProviderContext with enhanced broker registry support

import React, { createContext, useContext, useState, useEffect, useCallback, useMemo, useRef, ReactNode } from 'react';
import { getWebSocketManager } from '../services/websocket';
import type { ProviderConfig } from '../services/websocket/types';
import { ConnectionStatus } from '../services/websocket/types';
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
  const [activeBroker, setActiveBrokerState] = useState<string>('kraken');
  const [tradingMode, setTradingModeState] = useState<'live' | 'paper'>('paper');
  const [paperPortfolioMode, setPaperPortfolioModeState] = useState<'separate' | 'unified'>(() => {
    return (localStorage.getItem('paper_portfolio_mode') as 'separate' | 'unified') || 'separate';
  });
  const [providerConfigs, setProviderConfigs] = useState<Map<string, ProviderConfig>>(new Map());
  const [isLoading, setIsLoading] = useState(true);
  const [isConnecting, setIsConnecting] = useState(false);

  // Adapter instances
  const [realAdapter, setRealAdapter] = useState<IExchangeAdapter | null>(null);
  const [paperAdapter, setPaperAdapter] = useState<PaperTradingAdapter | null>(null);

  // Get broker metadata
  const activeBrokerMetadata = useMemo(() => {
    return getBrokerMetadata(activeBroker) || null;
  }, [activeBroker]);

  // Get available brokers (only crypto for now)
  const availableBrokers = useMemo(() => {
    return getAllBrokers().filter(b => b.type === 'crypto');
  }, []);

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

  // Load configurations on mount
  useEffect(() => {
    const loadConfigurations = async () => {
      setIsLoading(true);

      try {
        const manager = getWebSocketManager();
        const configs = new Map<string, ProviderConfig>();

        // Initialize configs for all available brokers
        for (const broker of availableBrokers) {
          if (broker.websocket.enabled && broker.websocket.endpoint) {
            configs.set(broker.id, {
              provider_name: broker.id,
              enabled: true,
              endpoint: broker.websocket.endpoint,
            });
          }
        }

        setProviderConfigs(configs);

        // Load saved preferences
        const savedBroker = localStorage.getItem('active_trading_broker');
        const savedMode = localStorage.getItem('trading_mode') as 'live' | 'paper' | null;

        if (savedBroker && BROKER_REGISTRY[savedBroker]) {
          setActiveBrokerState(savedBroker);
        }

        if (savedMode) {
          setTradingModeState(savedMode);
        }

        // Apply configurations to WebSocket manager
        configs.forEach((config, provider) => {
          manager.setProviderConfig(provider, config);
        });
      } catch (error) {
        console.error('[BrokerContext] Failed to load configurations:', error);
      } finally {
        setIsLoading(false);
      }
    };

    loadConfigurations();
  }, [availableBrokers]);

  // Initialize adapters when broker changes
  const isInitializedRef = useRef(false);

  useEffect(() => {
    console.log(`[BrokerContext] useEffect triggered - activeBroker: ${activeBroker}, isLoading: ${isLoading}, isInitialized: ${isInitializedRef.current}`);

    // Don't initialize while loading configs
    if (isLoading) {
      console.log(`[BrokerContext] Skipping initialization - still loading configs`);
      return;
    }

    // Don't re-initialize if already initialized (keeps connection alive across tab changes)
    if (isInitializedRef.current) {
      console.log(`[BrokerContext] Skipping initialization - already initialized`);
      return;
    }

    const initializeAdapters = async () => {
      setIsConnecting(true);

      try {
        console.log(`[BrokerContext] ========================================`);
        console.log(`[BrokerContext] Initializing ${activeBroker} adapters...`);
        console.log(`[BrokerContext] ========================================`);

        // Clean up old adapters
        if (realAdapter) {
          console.log(`[BrokerContext] Disconnecting old real adapter...`);
          await realAdapter.disconnect();
        }
        if (paperAdapter) {
          console.log(`[BrokerContext] Disconnecting old paper adapter...`);
          await paperAdapter.disconnect();
        }

        // Create real adapter
        console.log(`[BrokerContext] Creating ${activeBroker} real adapter...`);
        const newRealAdapter = createBrokerAdapter(activeBroker);

        console.log(`[BrokerContext] Connecting ${activeBroker} real adapter...`);
        await newRealAdapter.connect();
        console.log(`[BrokerContext] ✓ ${activeBroker} real adapter connected (status: ${newRealAdapter.isConnected() ? 'CONNECTED' : 'NOT CONNECTED'})`);

        setRealAdapter(newRealAdapter);

        // Create paper adapter (wrapping real adapter)
        // Portfolio ID depends on mode: separate per broker or unified global
        console.log(`[BrokerContext] Creating ${activeBroker} paper adapter...`);
        const portfolioId = paperPortfolioMode === 'unified'
          ? 'paper_global'
          : `paper_${activeBroker}`;
        console.log(`[BrokerContext] Portfolio ID: ${portfolioId} (mode: ${paperPortfolioMode})`);

        let newPaperAdapter: PaperTradingAdapter;

        try {
          newPaperAdapter = createPaperTradingAdapter(
            {
              portfolioId,
              portfolioName: paperPortfolioMode === 'unified'
                ? 'Global Paper Trading Portfolio'
                : `${activeBroker.toUpperCase()} Paper Trading`,
              provider: activeBroker,
              assetClass: 'crypto',
              initialBalance: 100000,
              currency: 'USD',
              fees: {
                maker: activeBroker === 'kraken' ? 0.0002 : 0.0002,
                taker: activeBroker === 'kraken' ? 0.0005 : 0.0005,
              },
              slippage: {
                market: 0,
                limit: 0,
              },
              enableMarginTrading: true,
              defaultLeverage: 1,
              maxLeverage: activeBrokerMetadata?.advancedFeatures.maxLeverage || 1,
            },
            newRealAdapter
          );
          console.log(`[BrokerContext] ✓ Paper adapter instance created`);
        } catch (error) {
          console.error(`[BrokerContext] ✗ Failed to create paper adapter:`, error);
          throw error;
        }

        console.log(`[BrokerContext] Connecting ${activeBroker} paper adapter...`);
        await newPaperAdapter.connect();
        console.log(`[BrokerContext] ✓ ${activeBroker} paper adapter connected (status: ${newPaperAdapter.isConnected() ? 'CONNECTED' : 'NOT CONNECTED'})`);

        setPaperAdapter(newPaperAdapter);
        console.log(`[BrokerContext] ✓ setPaperAdapter() called`);

        // Connect WebSocket AFTER adapters are ready
        const manager = getWebSocketManager();
        const config = providerConfigs.get(activeBroker);

        console.log(`[BrokerContext] WebSocket config for ${activeBroker}:`, config ? 'EXISTS' : 'MISSING');

        if (config) {
          console.log(`[BrokerContext] Setting WebSocket config for ${activeBroker}...`);
          manager.setProviderConfig(activeBroker, config);

          console.log(`[BrokerContext] Connecting WebSocket for ${activeBroker}...`);
          await manager.connect(activeBroker);

          // Wait for WebSocket to actually OPEN (not just connect call to finish)
          let wsStatus = manager.getStatus(activeBroker);
          let waitCount = 0;
          const maxWait = 20; // 10 seconds max

          while (wsStatus !== ConnectionStatus.CONNECTED && waitCount < maxWait) {
            console.log(`[BrokerContext] Waiting for WebSocket... (${waitCount + 1}/${maxWait}) - Status: ${wsStatus}`);
            await new Promise(resolve => setTimeout(resolve, 500));
            wsStatus = manager.getStatus(activeBroker);
            waitCount++;
          }

          console.log(`[BrokerContext] ✓ ${activeBroker} WebSocket status: ${wsStatus}`);

          console.log(`[BrokerContext] ========================================`);
          console.log(`[BrokerContext] ${activeBroker} initialization complete!`);
          console.log(`[BrokerContext] Real Adapter: ${newRealAdapter.isConnected() ? '✓ CONNECTED' : '✗ NOT CONNECTED'}`);
          console.log(`[BrokerContext] Paper Adapter: ${newPaperAdapter.isConnected() ? '✓ CONNECTED' : '✗ NOT CONNECTED'}`);
          console.log(`[BrokerContext] WebSocket: ${wsStatus}`);
          console.log(`[BrokerContext] ========================================`);
        } else {
          console.warn(`[BrokerContext] No WebSocket config found for ${activeBroker}!`);
        }

        // Mark as initialized
        isInitializedRef.current = true;
      } catch (error) {
        console.error(`[BrokerContext] ✗ Failed to initialize ${activeBroker}:`, error);
        console.error(`[BrokerContext] Error details:`, error);
        console.error(`[BrokerContext] Stack trace:`, (error as Error)?.stack);

        // Set adapters to null on failure
        setRealAdapter(null);
        setPaperAdapter(null);
      } finally {
        setIsConnecting(false);
      }
    };

    initializeAdapters();

    // Cleanup only when broker actually changes (not on tab change)
    return () => {
      // Only cleanup if we're switching brokers (flag was reset)
      if (!isInitializedRef.current && realAdapter) {
        console.log(`[BrokerContext] Cleanup: Disconnecting adapters for broker switch`);
        realAdapter.disconnect();
        if (paperAdapter) {
          paperAdapter.disconnect();
        }
      }
    };
  }, [isLoading, activeBroker, paperPortfolioMode]); // Depend on activeBroker and portfolio mode

  // Set active broker
  const setActiveBroker = useCallback(async (brokerId: string) => {
    if (!BROKER_REGISTRY[brokerId]) {
      throw new Error(`Broker ${brokerId} is not available`);
    }

    console.log(`[BrokerContext] Switching to broker: ${brokerId}`);

    // Reset initialization flag to allow switching brokers
    isInitializedRef.current = false;

    setActiveBrokerState(brokerId);

    // Save preference
    localStorage.setItem('active_trading_broker', brokerId);
  }, []);

  // Set trading mode
  const setTradingMode = useCallback((mode: 'live' | 'paper') => {
    console.log(`[BrokerContext] Switching to ${mode} mode`);
    setTradingModeState(mode);
    localStorage.setItem('trading_mode', mode);
  }, []);

  // Set paper portfolio mode
  const setPaperPortfolioMode = useCallback((mode: 'separate' | 'unified') => {
    console.log(`[BrokerContext] Switching paper portfolio mode to: ${mode}`);
    setPaperPortfolioModeState(mode);
    localStorage.setItem('paper_portfolio_mode', mode);
    // Reset initialization to recreate adapters with new portfolio ID
    isInitializedRef.current = false;
  }, []);

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
        provider_name: provider,
      } as ProviderConfig;

      newConfigs.set(provider, updatedConfig);

      // Update WebSocket manager
      const manager = getWebSocketManager();
      manager.setProviderConfig(provider, updatedConfig);

      return newConfigs;
    });
  }, []);

  return (
    <BrokerContext.Provider
      value={{
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
      }}
    >
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
      const categoryFeatures = features[category] as any;
      return categoryFeatures?.[feature] === true;
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
