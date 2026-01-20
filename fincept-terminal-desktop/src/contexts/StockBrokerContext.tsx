/**
 * Stock Broker Context
 *
 * Unified state management for stock brokers globally
 * Separate from crypto BrokerContext to maintain clean architecture
 */

import React, {
  createContext,
  useContext,
  useState,
  useEffect,
  useCallback,
  useMemo,
  useRef,
  ReactNode,
} from 'react';

import {
  STOCK_BROKER_REGISTRY,
  getAllStockBrokers,
  getStockBrokerMetadata,
  createStockBrokerAdapter,
  getStockBrokersByRegion,
  stockBrokerSupportsFeature,
  stockBrokerSupportsTradingFeature,
  getStockBrokerDefaultSymbols,
} from '../brokers/stocks';
import { BaseStockBrokerAdapter } from '../brokers/stocks/BaseStockBrokerAdapter';

import type {
  IStockBrokerAdapter,
  StockBrokerMetadata,
  Region,
  BrokerCredentials,
  AuthResponse,
  Position,
  Holding,
  Funds,
  Order,
  Trade,
} from '../brokers/stocks/types';

import { saveSetting, getSetting } from '@/services/core/sqliteService';
import {
  initTradingSession,
  switchTradingMode as switchUnifiedTradingMode,
  getPositions as getUnifiedPositions,
  getOrders as getUnifiedOrders,
  getPaperFunds,
  getPaperTrades,
  type TradingMode,
  type UnifiedPosition,
  type PaperOrder,
} from '@/services/unifiedTradingService';

// ============================================================================
// CONSTANTS
// ============================================================================

const CONNECTION_TIMEOUT = 15000; // 15 seconds for stock brokers (may need OAuth)
const CONNECTION_RETRY_ATTEMPTS = 2;
const CONNECTION_RETRY_DELAY = 3000;

// Storage keys
const STORAGE_KEYS = {
  ACTIVE_STOCK_BROKER: 'active_stock_broker',
  STOCK_TRADING_MODE: 'stock_trading_mode',
  LAST_CONNECTED_BROKERS: 'last_connected_stock_brokers',
} as const;

// ============================================================================
// HELPERS
// ============================================================================

function validateTradingMode(value: string | null): 'live' | 'paper' | null {
  if (value === 'live' || value === 'paper') return value;
  return null;
}

async function safeStorageGet(key: string): Promise<string | null> {
  try {
    return await getSetting(key);
  } catch (error) {
    console.error(`[StockBrokerContext] Failed to get storage key "${key}":`, error);
    return null;
  }
}

async function safeStorageSet(key: string, value: string): Promise<void> {
  try {
    await saveSetting(key, value, 'stock_broker');
  } catch (error) {
    console.error(`[StockBrokerContext] Failed to set storage key "${key}":`, error);
  }
}

function createTimeout(ms: number): Promise<never> {
  return new Promise((_, reject) =>
    setTimeout(() => reject(new Error(`Connection timeout after ${ms}ms`)), ms)
  );
}

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
      await new Promise((resolve) => setTimeout(resolve, delay * (i + 1)));
    }
  }
}

// ============================================================================
// CONTEXT TYPES
// ============================================================================

interface StockBrokerContextType {
  // Active broker
  activeBroker: string | null;
  activeBrokerMetadata: StockBrokerMetadata | null;
  setActiveBroker: (brokerId: string) => Promise<void>;

  // Available brokers
  availableBrokers: StockBrokerMetadata[];
  getBrokersByRegion: (region: Region) => StockBrokerMetadata[];

  // Trading mode
  tradingMode: 'live' | 'paper';
  setTradingMode: (mode: 'live' | 'paper') => void;

  // Adapter instance
  adapter: IStockBrokerAdapter | null;

  // Authentication
  isAuthenticated: boolean;
  authenticate: (credentials: BrokerCredentials) => Promise<AuthResponse>;
  logout: () => Promise<void>;
  getAuthUrl: () => Promise<string | null>;

  // Broker capabilities (shortcuts)
  supportsFeature: (feature: keyof StockBrokerMetadata['features']) => boolean;
  supportsTradingFeature: (feature: keyof StockBrokerMetadata['tradingFeatures']) => boolean;

  // Default symbols for selected broker
  defaultSymbols: string[];

  // Cached data
  positions: Position[];
  holdings: Holding[];
  orders: Order[];
  trades: Trade[];
  funds: Funds | null;

  // Data refresh
  refreshPositions: () => Promise<void>;
  refreshHoldings: () => Promise<void>;
  refreshOrders: () => Promise<void>;
  refreshTrades: () => Promise<void>;
  refreshFunds: () => Promise<void>;
  refreshAll: () => Promise<void>;

  // Loading states
  isLoading: boolean;
  isConnecting: boolean;
  isRefreshing: boolean;

  // Error state
  error: string | null;
  clearError: () => void;
}

const StockBrokerContext = createContext<StockBrokerContextType | null>(null);

// ============================================================================
// PROVIDER COMPONENT
// ============================================================================

interface StockBrokerProviderProps {
  children: ReactNode;
}

export function StockBrokerProvider({ children }: StockBrokerProviderProps) {
  // Core state
  const [activeBroker, setActiveBrokerState] = useState<string | null>(null);
  const [tradingMode, setTradingModeState] = useState<'live' | 'paper'>('paper');
  const [adapter, setAdapter] = useState<IStockBrokerAdapter | null>(null);
  const [isAuthenticated, setIsAuthenticated] = useState(false);

  // Loading states
  const [isLoading, setIsLoading] = useState(true);
  const [isConnecting, setIsConnecting] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);

  // Error state
  const [error, setError] = useState<string | null>(null);

  // Cached trading data
  const [positions, setPositions] = useState<Position[]>([]);
  const [holdings, setHoldings] = useState<Holding[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [trades, setTrades] = useState<Trade[]>([]);
  const [funds, setFunds] = useState<Funds | null>(null);

  // Refs
  const abortControllerRef = useRef<AbortController | null>(null);
  const hasInitializedRef = useRef(false);

  // ============================================================================
  // COMPUTED VALUES (Memoized)
  // ============================================================================

  const availableBrokers = useMemo(() => {
    return getAllStockBrokers();
  }, []);

  const activeBrokerMetadata = useMemo(() => {
    if (!activeBroker) return null;
    return getStockBrokerMetadata(activeBroker) || null;
  }, [activeBroker]);

  const defaultSymbols = useMemo(() => {
    if (!activeBroker) return [];
    return getStockBrokerDefaultSymbols(activeBroker);
  }, [activeBroker]);

  // ============================================================================
  // CAPABILITY CHECKERS
  // ============================================================================

  const supportsFeature = useCallback(
    (feature: keyof StockBrokerMetadata['features']): boolean => {
      if (!activeBroker) return false;
      return stockBrokerSupportsFeature(activeBroker, feature);
    },
    [activeBroker]
  );

  const supportsTradingFeature = useCallback(
    (feature: keyof StockBrokerMetadata['tradingFeatures']): boolean => {
      if (!activeBroker) return false;
      return stockBrokerSupportsTradingFeature(activeBroker, feature);
    },
    [activeBroker]
  );

  const getBrokersByRegion = useCallback((region: Region): StockBrokerMetadata[] => {
    return getStockBrokersByRegion(region);
  }, []);

  // ============================================================================
  // INITIALIZATION
  // ============================================================================

  useEffect(() => {
    const loadSavedPreferences = async () => {
      if (hasInitializedRef.current) return;
      hasInitializedRef.current = true;

      setIsLoading(true);

      try {
        // Load saved trading mode first
        const savedModeStr = await safeStorageGet(STORAGE_KEYS.STOCK_TRADING_MODE);
        const savedMode = validateTradingMode(savedModeStr);
        if (savedMode) {
          setTradingModeState(savedMode);
        }

        // Load saved broker preference
        const savedBroker = await safeStorageGet(STORAGE_KEYS.ACTIVE_STOCK_BROKER);

        // If we have a saved broker, try to use it
        if (savedBroker && STOCK_BROKER_REGISTRY[savedBroker]) {
          console.log(`[StockBrokerContext] Found saved broker: ${savedBroker}`);
          setActiveBrokerState(savedBroker);
        } else {
          // No saved broker - scan all brokers to find one with valid session
          console.log('[StockBrokerContext] No saved broker, scanning for valid sessions...');
          const allBrokers = getAllStockBrokers();

          for (const broker of allBrokers) {
            try {
              const tempAdapter = createStockBrokerAdapter(broker.id);
              const creds = await (tempAdapter as any).loadCredentials?.();

              if (creds?.accessToken && creds?.tokenTimestamp) {
                // Check if token is valid for today (IST check for Indian brokers)
                const isValid = BaseStockBrokerAdapter.isTokenValidForTodayIST(creds.tokenTimestamp);
                if (isValid) {
                  console.log(`[StockBrokerContext] Found valid session for: ${broker.id}`);
                  setActiveBrokerState(broker.id);
                  break;
                }
              }
            } catch (err) {
              // Skip broker if check fails
            }
          }
        }

        console.log('[StockBrokerContext] Preferences loaded', {
          broker: savedBroker,
          mode: savedMode,
        });
      } catch (err) {
        console.error('[StockBrokerContext] Failed to load preferences:', err);
      } finally {
        setIsLoading(false);
      }
    };

    loadSavedPreferences();
  }, []);

  // Auto-create adapter when broker changes (including on startup)
  useEffect(() => {
    const initializeAdapter = async () => {
      if (!activeBroker || adapter?.brokerId === activeBroker) return;

      console.log(`[StockBrokerContext] Initializing adapter for: ${activeBroker}`);

      try {
        const newAdapter = createStockBrokerAdapter(activeBroker);
        console.log(`[StockBrokerContext] Created adapter for: ${activeBroker}`);

        // Load credentials from database
        try {
          console.log(`[StockBrokerContext] Loading credentials from database...`);
          const storedCreds = await (newAdapter as any).loadCredentials();

          if (storedCreds && storedCreds.apiKey) {
            console.log(`[StockBrokerContext] Found stored credentials, loading into adapter memory...`);

            // Set API credentials in adapter memory
            if (newAdapter.setCredentials) {
              newAdapter.setCredentials(storedCreds.apiKey, storedCreds.apiSecret || '');
              console.log(`[StockBrokerContext] ✓ Credentials loaded into adapter memory`);
            }

            // If access_token exists, attempt auto-restore session
            if (storedCreds.accessToken && typeof (newAdapter as any).initFromStorage === 'function') {
              console.log(`[StockBrokerContext] Access token found, attempting auto-restore...`);
              const restored = await (newAdapter as any).initFromStorage();

              if (restored) {
                console.log(`[StockBrokerContext] ✓ Session restored successfully`);
                setIsAuthenticated(true);

                // Initialize unified trading session
                try {
                  await initTradingSession(activeBroker, tradingMode);
                  console.log(`[StockBrokerContext] ✓ Trading session initialized`);
                } catch (sessionErr) {
                  console.error('[StockBrokerContext] Failed to init trading session:', sessionErr);
                }

                // Note: Data will be auto-loaded by the useEffect that watches isAuthenticated
              } else {
                console.log(`[StockBrokerContext] Session restore failed (token may be expired)`);
                // Indian broker tokens expire at midnight IST - notify user
                setError(`Session expired. Please re-authenticate with ${activeBroker}.`);
                setIsAuthenticated(false);
              }
            } else {
              console.log(`[StockBrokerContext] No access token found, manual OAuth required`);
            }
          } else {
            console.log(`[StockBrokerContext] No stored credentials found`);
          }
        } catch (loadErr) {
          console.log('[StockBrokerContext] Failed to load credentials:', loadErr);
        }

        setAdapter(newAdapter);
      } catch (err) {
        console.error('[StockBrokerContext] Failed to create adapter:', err);
      }
    };

    initializeAdapter();
  }, [activeBroker, tradingMode]);

  // ============================================================================
  // BROKER MANAGEMENT
  // ============================================================================

  const setActiveBroker = useCallback(async (brokerId: string) => {
    if (!STOCK_BROKER_REGISTRY[brokerId]) {
      throw new Error(`Stock broker "${brokerId}" is not registered`);
    }

    // Abort any ongoing connection
    abortControllerRef.current?.abort();

    // Clean up existing adapter (DON'T logout - preserves credentials in DB)
    if (adapter && adapter.brokerId !== brokerId) {
      console.log(`[StockBrokerContext] Switching from ${adapter.brokerId} to ${brokerId}, preserving credentials`);
      // Don't call logout() - it deletes credentials from database
      // Just reset the adapter instance
    }

    // Reset state
    setAdapter(null);
    setIsAuthenticated(false);
    setPositions([]);
    setHoldings([]);
    setOrders([]);
    setTrades([]);
    setFunds(null);
    setError(null);

    // Set new broker
    setActiveBrokerState(brokerId);
    await safeStorageSet(STORAGE_KEYS.ACTIVE_STOCK_BROKER, brokerId);

    // Adapter will be created by useEffect watching activeBroker
    console.log(`[StockBrokerContext] Active broker set to: ${brokerId}`);
  }, [adapter]);

  const setTradingMode = useCallback(async (mode: 'live' | 'paper') => {
    setTradingModeState(mode);
    await safeStorageSet(STORAGE_KEYS.STOCK_TRADING_MODE, mode);

    // Switch mode in unified trading service
    try {
      await switchUnifiedTradingMode(mode);
      console.log(`[StockBrokerContext] Trading mode switched to: ${mode}`);
    } catch (err) {
      console.error('[StockBrokerContext] Failed to switch unified trading mode:', err);
    }
  }, []);

  // ============================================================================
  // AUTHENTICATION
  // ============================================================================

  const getAuthUrl = useCallback(async (): Promise<string | null> => {
    if (!activeBroker) return null;

    try {
      // Use existing adapter if available, otherwise create temporary one
      const brokerAdapter = adapter || createStockBrokerAdapter(activeBroker);

      // Load credentials before generating auth URL
      try {
        console.log('[StockBrokerContext] Attempting to load credentials...');
        const storedCreds = await (brokerAdapter as any).loadCredentials();
        console.log('[StockBrokerContext] loadCredentials result:', {
          hasCredentials: !!storedCreds,
          hasApiKey: !!(storedCreds && storedCreds.apiKey),
          hasApiSecret: !!(storedCreds && storedCreds.apiSecret),
          apiKeyLength: storedCreds?.apiKey?.length || 0,
        });

        if (storedCreds && storedCreds.apiKey && brokerAdapter.setCredentials) {
          console.log('[StockBrokerContext] Setting credentials on adapter');
          brokerAdapter.setCredentials(storedCreds.apiKey, storedCreds.apiSecret || '');
          console.log('[StockBrokerContext] ✓ Credentials loaded and set for auth URL generation');
        } else {
          console.warn('[StockBrokerContext] No valid credentials found', storedCreds);
        }
      } catch (loadErr) {
        console.error('[StockBrokerContext] Failed to load credentials:', loadErr);
      }

      if (brokerAdapter.getAuthUrl) {
        return brokerAdapter.getAuthUrl();
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to get auth URL:', err);
    }

    return null;
  }, [activeBroker, adapter]);

  const authenticate = useCallback(
    async (credentials: BrokerCredentials): Promise<AuthResponse> => {
      if (!activeBroker) {
        return {
          success: false,
          message: 'No broker selected',
          errorCode: 'NO_BROKER',
        };
      }

      // Abort any existing connection
      abortControllerRef.current?.abort();
      abortControllerRef.current = new AbortController();

      setIsConnecting(true);
      setError(null);

      try {
        // Use existing adapter or create new one
        const adapterToUse = adapter || createStockBrokerAdapter(activeBroker);

        // Authenticate
        const response = await adapterToUse.authenticate(credentials);

        if (response.success) {
          // Update adapter if we created a new one
          if (!adapter) {
            setAdapter(adapterToUse);
          }
          setIsAuthenticated(true);
          setError(null); // Clear any previous session expired error

          // Initialize unified trading session
          try {
            await initTradingSession(activeBroker, tradingMode);
            console.log(`[StockBrokerContext] Initialized trading session: broker=${activeBroker}, mode=${tradingMode}`);
          } catch (sessionErr) {
            console.error('[StockBrokerContext] Failed to init trading session:', sessionErr);
          }

          // Fetch initial data
          await refreshAllData(adapterToUse);

          console.log(`[StockBrokerContext] Successfully authenticated with ${activeBroker}`);
        } else {
          setError(response.message || 'Authentication failed');
        }

        return response;
      } catch (err) {
        const errorMessage = err instanceof Error ? err.message : 'Authentication failed';
        setError(errorMessage);

        return {
          success: false,
          message: errorMessage,
          errorCode: 'AUTH_ERROR',
        };
      } finally {
        setIsConnecting(false);
      }
    },
    [activeBroker, tradingMode, adapter]
  );

  const logout = useCallback(async () => {
    if (!adapter) return;

    try {
      await adapter.logout();
    } catch (err) {
      console.error('[StockBrokerContext] Logout error:', err);
    }

    // Reset state
    setAdapter(null);
    setIsAuthenticated(false);
    setPositions([]);
    setHoldings([]);
    setOrders([]);
    setTrades([]);
    setFunds(null);

    console.log('[StockBrokerContext] Logged out');
  }, [adapter]);

  // ============================================================================
  // DATA REFRESH FUNCTIONS
  // ============================================================================

  const refreshAllData = async (adapterInstance: IStockBrokerAdapter, mode: 'live' | 'paper' = tradingMode) => {
    setIsRefreshing(true);

    try {
      if (mode === 'paper') {
        // Paper mode: fetch from unified trading service
        console.log('[StockBrokerContext] refreshAllData: Paper mode - fetching paper trading data');

        const [positionsData, ordersData, tradesData, fundsData] = await Promise.allSettled([
          getUnifiedPositions(),
          getUnifiedOrders(),
          getPaperTrades(),
          getPaperFunds(),
        ]);

        // Convert and set positions
        if (positionsData.status === 'fulfilled') {
          const convertedPositions: Position[] = positionsData.value.map((p) => ({
            symbol: p.symbol,
            exchange: p.exchange as Position['exchange'],
            productType: 'CNC' as Position['productType'],
            quantity: p.quantity,
            buyQuantity: p.side === 'long' ? p.quantity : 0,
            sellQuantity: p.side === 'short' ? p.quantity : 0,
            buyValue: p.side === 'long' ? p.entry_price * p.quantity : 0,
            sellValue: p.side === 'short' ? p.entry_price * p.quantity : 0,
            averagePrice: p.entry_price,
            lastPrice: p.current_price,
            pnl: p.unrealized_pnl,
            pnlPercent: p.entry_price > 0 ? ((p.current_price - p.entry_price) / p.entry_price) * 100 : 0,
            dayPnl: 0,
          }));
          setPositions(convertedPositions);
        }

        // Paper trading has no holdings (demat positions)
        setHoldings([]);

        // Convert and set orders
        if (ordersData.status === 'fulfilled') {
          const mapStatus = (status: string): Order['status'] => {
            switch (status) {
              case 'pending': return 'PENDING';
              case 'filled': return 'FILLED';
              case 'partial': return 'PARTIALLY_FILLED';
              case 'cancelled': return 'CANCELLED';
              default: return 'PENDING';
            }
          };
          const convertedOrders: Order[] = ordersData.value.map((o) => ({
            orderId: o.id,
            symbol: o.symbol,
            exchange: 'NSE' as Order['exchange'],
            side: o.side as Order['side'],
            quantity: o.quantity,
            filledQuantity: o.filled_qty,
            pendingQuantity: o.quantity - o.filled_qty,
            price: o.price || 0,
            averagePrice: o.avg_price || 0,
            triggerPrice: o.stop_price || undefined,
            orderType: o.order_type as Order['orderType'],
            productType: 'CNC' as Order['productType'],
            validity: 'DAY' as Order['validity'],
            status: mapStatus(o.status),
            statusMessage: '',
            placedAt: new Date(o.created_at),
            updatedAt: o.filled_at ? new Date(o.filled_at) : undefined,
          }));
          setOrders(convertedOrders);
        }

        // Convert and set trades
        if (tradesData.status === 'fulfilled') {
          const convertedTrades: Trade[] = tradesData.value.map((t: PaperOrder) => ({
            tradeId: t.id,
            orderId: t.id,
            symbol: t.symbol,
            exchange: 'NSE' as Trade['exchange'],
            side: t.side as Trade['side'],
            quantity: t.filled_qty,
            price: t.avg_price || t.price || 0,
            productType: 'CNC' as Trade['productType'],
            tradedAt: t.filled_at ? new Date(t.filled_at) : new Date(t.created_at),
          }));
          setTrades(convertedTrades);
        }

        // Convert and set funds
        if (fundsData.status === 'fulfilled' && fundsData.value) {
          const paperFundsData = fundsData.value;
          // Map currency string to Currency type (default to INR if unknown)
          const currencyMap: Record<string, Funds['currency']> = {
            INR: 'INR', USD: 'USD', EUR: 'EUR', GBP: 'GBP', JPY: 'JPY', AUD: 'AUD', CAD: 'CAD', SGD: 'SGD', HKD: 'HKD'
          };
          const mappedCurrency = currencyMap[paperFundsData.currency] || 'INR';
          const convertedFunds: Funds = {
            totalBalance: paperFundsData.totalBalance,
            availableCash: paperFundsData.availableCash,
            usedMargin: paperFundsData.usedMargin,
            availableMargin: paperFundsData.availableMargin,
            collateral: 0,
            unrealizedPnl: 0,
            realizedPnl: 0,
            currency: mappedCurrency,
          };
          setFunds(convertedFunds);
        }
      } else {
        // Live mode: fetch from broker adapter
        console.log('[StockBrokerContext] refreshAllData: Live mode - fetching broker data');

        const [positionsData, holdingsData, ordersData, tradesData, fundsData] = await Promise.allSettled([
          adapterInstance.getPositions(),
          adapterInstance.getHoldings(),
          adapterInstance.getOrders(),
          adapterInstance.getTradeBook(),
          adapterInstance.getFunds(),
        ]);

        if (positionsData.status === 'fulfilled') {
          setPositions(positionsData.value);
        }
        if (holdingsData.status === 'fulfilled') {
          setHoldings(holdingsData.value);
        }
        if (ordersData.status === 'fulfilled') {
          setOrders(ordersData.value);
        }
        if (tradesData.status === 'fulfilled') {
          setTrades(tradesData.value);
        }
        if (fundsData.status === 'fulfilled') {
          setFunds(fundsData.value);
        }
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh data:', err);
    } finally {
      setIsRefreshing(false);
    }
  };

  const refreshPositions = useCallback(async () => {
    if (!isAuthenticated) return;

    try {
      if (tradingMode === 'paper') {
        // Fetch from unified trading service (paper positions)
        const unifiedPositions = await getUnifiedPositions();
        // Convert UnifiedPosition to Position format
        const convertedPositions: Position[] = unifiedPositions.map((p) => ({
          symbol: p.symbol,
          exchange: p.exchange as Position['exchange'],
          productType: 'CNC' as Position['productType'],
          quantity: p.quantity,
          buyQuantity: p.side === 'long' ? p.quantity : 0,
          sellQuantity: p.side === 'short' ? p.quantity : 0,
          buyValue: p.side === 'long' ? p.entry_price * p.quantity : 0,
          sellValue: p.side === 'short' ? p.entry_price * p.quantity : 0,
          averagePrice: p.entry_price,
          lastPrice: p.current_price,
          pnl: p.unrealized_pnl,
          pnlPercent: p.entry_price > 0 ? ((p.current_price - p.entry_price) / p.entry_price) * 100 : 0,
          dayPnl: 0,
        }));
        setPositions(convertedPositions);
      } else if (adapter) {
        // Fetch from live broker
        const data = await adapter.getPositions();
        setPositions(data);
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh positions:', err);
    }
  }, [adapter, isAuthenticated, tradingMode]);

  const refreshHoldings = useCallback(async () => {
    if (!isAuthenticated) return;

    try {
      if (tradingMode === 'paper') {
        // Paper trading doesn't have long-term holdings (delivery positions)
        // Holdings are actual shares held in demat account, which paper trading doesn't have
        setHoldings([]);
        console.log('[StockBrokerContext] Paper mode: Holdings cleared (paper trading has no demat holdings)');
      } else if (adapter) {
        // Fetch from live broker
        const data = await adapter.getHoldings();
        setHoldings(data);
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh holdings:', err);
    }
  }, [adapter, isAuthenticated, tradingMode]);

  const refreshOrders = useCallback(async () => {
    if (!isAuthenticated) return;

    try {
      if (tradingMode === 'paper') {
        // Fetch from unified trading service (paper orders)
        const paperOrders = await getUnifiedOrders();
        // Map paper order status to Order status
        const mapStatus = (status: string): Order['status'] => {
          switch (status) {
            case 'pending': return 'PENDING';
            case 'filled': return 'FILLED';
            case 'partial': return 'PARTIALLY_FILLED';
            case 'cancelled': return 'CANCELLED';
            default: return 'PENDING';
          }
        };
        // Convert PaperOrder to Order format
        const convertedOrders: Order[] = paperOrders.map((o) => ({
          orderId: o.id,
          symbol: o.symbol,
          exchange: 'NSE' as Order['exchange'],
          side: o.side as Order['side'],
          quantity: o.quantity,
          filledQuantity: o.filled_qty,
          pendingQuantity: o.quantity - o.filled_qty,
          price: o.price || 0,
          averagePrice: o.avg_price || 0,
          triggerPrice: o.stop_price || undefined,
          orderType: o.order_type as Order['orderType'],
          productType: 'CNC' as Order['productType'],
          validity: 'DAY' as Order['validity'],
          status: mapStatus(o.status),
          statusMessage: '',
          placedAt: new Date(o.created_at),
          updatedAt: o.filled_at ? new Date(o.filled_at) : undefined,
        }));
        setOrders(convertedOrders);
      } else if (adapter) {
        // Fetch from live broker
        const data = await adapter.getOrders();
        setOrders(data);
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh orders:', err);
    }
  }, [adapter, isAuthenticated, tradingMode]);

  const refreshTrades = useCallback(async () => {
    if (!isAuthenticated) return;

    try {
      if (tradingMode === 'paper') {
        // Fetch filled orders from paper trading as "trades"
        const paperTrades = await getPaperTrades();
        // Convert PaperOrder to Trade format
        const convertedTrades: Trade[] = paperTrades.map((t: PaperOrder) => ({
          tradeId: t.id,
          orderId: t.id,
          symbol: t.symbol,
          exchange: 'NSE' as Trade['exchange'],
          side: t.side as Trade['side'],
          quantity: t.filled_qty,
          price: t.avg_price || t.price || 0,
          productType: 'CNC' as Trade['productType'],
          tradedAt: t.filled_at ? new Date(t.filled_at) : new Date(t.created_at),
        }));
        setTrades(convertedTrades);
        console.log(`[StockBrokerContext] Paper mode: Loaded ${convertedTrades.length} paper trades`);
      } else if (adapter) {
        // Fetch from live broker
        const data = await adapter.getTradeBook();
        setTrades(data);
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh trades:', err);
    }
  }, [adapter, isAuthenticated, tradingMode]);

  const refreshFunds = useCallback(async () => {
    if (!isAuthenticated) return;

    try {
      if (tradingMode === 'paper') {
        // Fetch from paper trading portfolio
        const paperFundsData = await getPaperFunds();
        if (paperFundsData) {
          // Map currency string to Currency type (default to INR if unknown)
          const currencyMap: Record<string, Funds['currency']> = {
            INR: 'INR', USD: 'USD', EUR: 'EUR', GBP: 'GBP', JPY: 'JPY', AUD: 'AUD', CAD: 'CAD', SGD: 'SGD', HKD: 'HKD'
          };
          const mappedCurrency = currencyMap[paperFundsData.currency] || 'INR';
          // Convert paper funds to Funds format
          const convertedFunds: Funds = {
            totalBalance: paperFundsData.totalBalance,
            availableCash: paperFundsData.availableCash,
            usedMargin: paperFundsData.usedMargin,
            availableMargin: paperFundsData.availableMargin,
            collateral: 0,
            unrealizedPnl: 0,
            realizedPnl: 0,
            currency: mappedCurrency,
          };
          setFunds(convertedFunds);
          console.log(`[StockBrokerContext] Paper mode: Loaded paper funds - Balance: ${convertedFunds.availableCash} ${convertedFunds.currency}`);
        } else {
          setFunds(null);
        }
      } else if (adapter) {
        // Fetch from live broker
        const data = await adapter.getFunds();
        setFunds(data);
      }
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh funds:', err);
    }
  }, [adapter, isAuthenticated, tradingMode]);

  const refreshAll = useCallback(async () => {
    if (!isAuthenticated) return;

    setIsRefreshing(true);
    try {
      // Use individual refresh functions that already handle paper/live mode
      await Promise.allSettled([
        refreshPositions(),
        refreshHoldings(),
        refreshOrders(),
        refreshTrades(),
        refreshFunds(),
      ]);
    } catch (err) {
      console.error('[StockBrokerContext] Failed to refresh all data:', err);
    } finally {
      setIsRefreshing(false);
    }
  }, [isAuthenticated, refreshPositions, refreshHoldings, refreshOrders, refreshTrades, refreshFunds]);

  // ============================================================================
  // AUTO-LOAD DATA ON AUTHENTICATION
  // ============================================================================

  useEffect(() => {
    // Auto-load data when adapter becomes authenticated
    if (adapter && isAuthenticated && !isRefreshing) {
      // Check if we have any data already loaded
      const hasData = positions.length > 0 || holdings.length > 0 || orders.length > 0 || funds !== null;

      if (!hasData) {
        console.log('[StockBrokerContext] Authenticated adapter detected, loading initial data...');
        refreshAllData(adapter).catch((err) => {
          console.error('[StockBrokerContext] Failed to auto-load data:', err);
        });
      }
    }
  }, [adapter, isAuthenticated]);

  // ============================================================================
  // REFRESH DATA ON TRADING MODE CHANGE
  // ============================================================================

  const previousTradingModeRef = useRef(tradingMode);

  useEffect(() => {
    // Refresh all data when trading mode changes (and we're authenticated)
    if (isAuthenticated && previousTradingModeRef.current !== tradingMode) {
      console.log(`[StockBrokerContext] Trading mode changed from ${previousTradingModeRef.current} to ${tradingMode}, refreshing data...`);
      previousTradingModeRef.current = tradingMode;

      // Clear current data first
      setPositions([]);
      setHoldings([]);
      setOrders([]);
      setTrades([]);
      setFunds(null);

      // Refresh with new mode
      if (adapter) {
        refreshAllData(adapter, tradingMode).catch((err) => {
          console.error('[StockBrokerContext] Failed to refresh data after mode switch:', err);
        });
      }
    }
  }, [tradingMode, isAuthenticated, adapter]);

  // ============================================================================
  // ERROR HANDLING
  // ============================================================================

  const clearError = useCallback(() => {
    setError(null);
  }, []);

  // ============================================================================
  // CONTEXT VALUE
  // ============================================================================

  const contextValue = useMemo(
    () => ({
      // Active broker
      activeBroker,
      activeBrokerMetadata,
      setActiveBroker,

      // Available brokers
      availableBrokers,
      getBrokersByRegion,

      // Trading mode
      tradingMode,
      setTradingMode,

      // Adapter
      adapter,

      // Authentication
      isAuthenticated,
      authenticate,
      logout,
      getAuthUrl,

      // Capabilities
      supportsFeature,
      supportsTradingFeature,

      // Default symbols
      defaultSymbols,

      // Cached data
      positions,
      holdings,
      orders,
      trades,
      funds,

      // Refresh functions
      refreshPositions,
      refreshHoldings,
      refreshOrders,
      refreshTrades,
      refreshFunds,
      refreshAll,

      // Loading states
      isLoading,
      isConnecting,
      isRefreshing,

      // Error
      error,
      clearError,
    }),
    [
      activeBroker,
      activeBrokerMetadata,
      setActiveBroker,
      availableBrokers,
      getBrokersByRegion,
      tradingMode,
      setTradingMode,
      adapter,
      isAuthenticated,
      authenticate,
      logout,
      getAuthUrl,
      supportsFeature,
      supportsTradingFeature,
      defaultSymbols,
      positions,
      holdings,
      orders,
      trades,
      funds,
      refreshPositions,
      refreshHoldings,
      refreshOrders,
      refreshTrades,
      refreshFunds,
      refreshAll,
      isLoading,
      isConnecting,
      isRefreshing,
      error,
      clearError,
    ]
  );

  return (
    <StockBrokerContext.Provider value={contextValue}>
      {children}
    </StockBrokerContext.Provider>
  );
}

// ============================================================================
// HOOKS
// ============================================================================

/**
 * Main hook for accessing stock broker context
 */
export function useStockBrokerContext(): StockBrokerContextType {
  const context = useContext(StockBrokerContext);

  if (!context) {
    throw new Error('useStockBrokerContext must be used within a StockBrokerProvider');
  }

  return context;
}

/**
 * Hook for broker selection
 */
export function useStockBrokerSelection() {
  const {
    activeBroker,
    activeBrokerMetadata,
    availableBrokers,
    setActiveBroker,
    getBrokersByRegion,
    isLoading,
  } = useStockBrokerContext();

  return {
    broker: activeBroker,
    metadata: activeBrokerMetadata,
    brokers: availableBrokers,
    setBroker: setActiveBroker,
    getBrokersByRegion,
    isLoading,
  };
}

/**
 * Hook for authentication
 */
export function useStockBrokerAuth() {
  const {
    isAuthenticated,
    authenticate,
    logout,
    getAuthUrl,
    isConnecting,
    error,
    clearError,
    adapter,
  } = useStockBrokerContext();

  return {
    isAuthenticated,
    authenticate,
    logout,
    getAuthUrl,
    isConnecting,
    error,
    clearError,
    adapter,
  };
}

/**
 * Hook for trading data
 */
export function useStockTradingData() {
  const {
    adapter,
    positions,
    holdings,
    orders,
    trades,
    funds,
    refreshPositions,
    refreshHoldings,
    refreshOrders,
    refreshTrades,
    refreshFunds,
    refreshAll,
    isRefreshing,
    isAuthenticated,
  } = useStockBrokerContext();

  return {
    adapter,
    positions,
    holdings,
    orders,
    trades,
    funds,
    refreshPositions,
    refreshHoldings,
    refreshOrders,
    refreshTrades,
    refreshFunds,
    refreshAll,
    isRefreshing,
    isReady: isAuthenticated && adapter !== null,
  };
}

/**
 * Hook for broker capabilities
 */
export function useStockBrokerCapabilities() {
  const {
    activeBrokerMetadata,
    supportsFeature,
    supportsTradingFeature,
    defaultSymbols,
  } = useStockBrokerContext();

  return {
    metadata: activeBrokerMetadata,
    supportsFeature,
    supportsTradingFeature,
    defaultSymbols,
    // Quick access to common features
    hasWebSocket: supportsFeature('webSocket'),
    hasAMO: supportsFeature('amo'),
    hasGTT: supportsFeature('gtt'),
    hasBracketOrder: supportsFeature('bracketOrder'),
    hasCoverOrder: supportsFeature('coverOrder'),
    hasOptionsChain: supportsFeature('optionsChain'),
  };
}

/**
 * Hook for trading mode
 */
export function useStockTradingMode() {
  const { tradingMode, setTradingMode } = useStockBrokerContext();

  return {
    mode: tradingMode,
    setMode: setTradingMode,
    isLive: tradingMode === 'live',
    isPaper: tradingMode === 'paper',
  };
}
