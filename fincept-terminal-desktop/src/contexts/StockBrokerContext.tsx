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
import * as symbolMaster from '@/services/symbolMasterService';

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
  connect: () => Promise<AuthResponse>; // Connect using stored credentials
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

  // Master contract state
  masterContractReady: boolean;

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

  // Master contract state
  const [masterContractReady, setMasterContractReady] = useState(false);

  // Cached trading data
  const [positions, setPositions] = useState<Position[]>([]);
  const [holdings, setHoldings] = useState<Holding[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [trades, setTrades] = useState<Trade[]>([]);
  const [funds, setFunds] = useState<Funds | null>(null);

  // Refs
  const abortControllerRef = useRef<AbortController | null>(null);
  const hasInitializedRef = useRef(false);

  // Counter to force adapter re-initialization
  const [adapterInitKey, setAdapterInitKey] = useState(0);

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

        // Load saved broker preference (but DON'T auto-connect)
        const savedBroker = await safeStorageGet(STORAGE_KEYS.ACTIVE_STOCK_BROKER);

        // If we have a saved broker, set it as active (adapter will be created, but NOT auto-authenticated)
        if (savedBroker && STOCK_BROKER_REGISTRY[savedBroker]) {
          console.log(`[StockBrokerContext] Found saved broker preference: ${savedBroker} (lazy init - no auto-connect)`);
          setActiveBrokerState(savedBroker);
          // Note: isLoading will be set to false by initializeAdapter useEffect
        } else {
          // No saved broker - default to yfinance (paper trading, no auth needed)
          console.log('[StockBrokerContext] No saved broker, defaulting to yfinance for paper trading');
          setActiveBrokerState('yfinance');
        }

        console.log('[StockBrokerContext] Preferences loaded', {
          broker: savedBroker,
          mode: savedMode,
        });
      } catch (err) {
        console.error('[StockBrokerContext] Failed to load preferences:', err);
        setIsLoading(false);
      }
    };

    loadSavedPreferences();
  }, []);

  // Auto-create adapter when broker changes (including on startup)
  // adapterInitKey forces re-init when incremented (for re-selecting same broker)
  useEffect(() => {
    const initializeAdapter = async () => {
      if (!activeBroker) {
        // If we're skipping init and isLoading is true, set it to false
        if (isLoading) {
          setIsLoading(false);
        }
        return;
      }

      console.log(`[StockBrokerContext] Initializing adapter for: ${activeBroker} (key: ${adapterInitKey})`);
      setIsLoading(true);

      try {
        const newAdapter = createStockBrokerAdapter(activeBroker);
        console.log(`[StockBrokerContext] Created adapter for: ${activeBroker}`);

        // Check if this is a paper-only broker that doesn't need credentials (like yfinance)
        // YFinance is a special case - it only supports paper trading and needs no authentication
        const isPaperOnlyBroker = activeBroker === 'yfinance';

        if (isPaperOnlyBroker) {
          console.log(`[StockBrokerContext] Paper-only broker detected: ${activeBroker}, auto-authenticating...`);

          // Auto-authenticate for paper-only brokers (no real credentials needed)
          const authResult = await newAdapter.authenticate({ apiKey: 'paper_trading' });

          if (authResult.success) {
            console.log(`[StockBrokerContext] ✓ Paper trading auto-authenticated for ${activeBroker}`);

            // IMPORTANT: Set adapter BEFORE isAuthenticated to avoid race condition
            // where EquityTradingTab checks isAuthenticated but adapter is still null
            setAdapter(newAdapter);
            setIsAuthenticated(true);
            setError(null);

            // Initialize unified trading session in paper mode
            try {
              await initTradingSession(activeBroker, 'paper');
              console.log(`[StockBrokerContext] ✓ Paper trading session initialized`);
            } catch (sessionErr) {
              console.error('[StockBrokerContext] Failed to init paper trading session:', sessionErr);
            }

            // Auto-download master contract for YFinance (CRITICAL for chart/symbol lookup)
            setMasterContractReady(false);
            symbolMaster.ensureMasterContract(activeBroker).then((result) => {
              if (result.success) {
                console.log(`[StockBrokerContext] ✓ Master contract ready (paper trading): ${result.total_symbols} symbols`);
                setMasterContractReady(true);
              } else {
                console.warn(`[StockBrokerContext] Master contract download issue: ${result.message}`);
                // YFinance doesn't have a master contract - set to true to allow charts to work
                // YFinance uses direct symbol lookup, not token-based lookup
                setMasterContractReady(true);
              }
            }).catch((err) => {
              console.warn(`[StockBrokerContext] Master contract download failed:`, err);
              // For YFinance, we can still work without master contract (uses direct symbol)
              setMasterContractReady(true);
            });
          } else {
            console.error(`[StockBrokerContext] Paper trading auto-auth failed:`, authResult.message);
            setError(authResult.message || 'Failed to initialize paper trading');
            setAdapter(newAdapter);
          }

          setIsLoading(false);
          return;
        }

        // For real brokers, just create the adapter and load credentials into memory
        // but DON'T auto-authenticate - user must explicitly connect
        try {
          console.log(`[StockBrokerContext] Loading credentials from database (no auto-connect)...`);
          const storedCreds = await (newAdapter as any).loadCredentials();

          if (storedCreds && storedCreds.apiKey) {
            console.log(`[StockBrokerContext] Found stored credentials, loading into adapter memory...`);

            // Set API credentials in adapter memory (for when user chooses to connect)
            if (newAdapter.setCredentials) {
              newAdapter.setCredentials(storedCreds.apiKey, storedCreds.apiSecret || '');
              console.log(`[StockBrokerContext] ✓ Credentials loaded into adapter memory`);
            }

            // Check if we have a potentially valid token (basic timestamp check only, no API call)
            // User can click "Connect" to actually validate and connect
            const hasToken = !!storedCreds.accessToken;
            const tokenTimestamp = storedCreds.tokenTimestamp ? new Date(storedCreds.tokenTimestamp) : null;
            const isTokenFromToday = tokenTimestamp
              ? BaseStockBrokerAdapter.isTokenValidForTodayIST(storedCreds.tokenTimestamp)
              : false;

            if (hasToken && isTokenFromToday) {
              console.log(`[StockBrokerContext] Token is valid (from today). Restoring session...`);

              // Restore access token to adapter so API calls work
              (newAdapter as any).accessToken = storedCreds.accessToken;
              (newAdapter as any)._isConnected = true;
              if (storedCreds.userId) {
                (newAdapter as any).userId = storedCreds.userId;
                (newAdapter as any).clientCode = storedCreds.userId;
              }

              // Restore feedToken, password, totpSecret from apiSecret JSON
              // These are needed for WebSocket connection (feedToken) and re-auth
              if (storedCreds.apiSecret) {
                try {
                  const secretData = JSON.parse(storedCreds.apiSecret);
                  if (secretData.feedToken) (newAdapter as any).feedToken = secretData.feedToken;
                  if (secretData.password) (newAdapter as any).password = secretData.password;
                  if (secretData.totpSecret) (newAdapter as any).totpSecret = secretData.totpSecret;
                  console.log(`[StockBrokerContext] Restored auth fields from apiSecret: feedToken=${!!secretData.feedToken}, password=${!!secretData.password}, totpSecret=${!!secretData.totpSecret}`);
                } catch {
                  // apiSecret is not JSON
                }
              }

              setAdapter(newAdapter);
              setIsAuthenticated(true);
              setError(null);

              // Initialize trading session and fetch data in background
              try {
                await initTradingSession(activeBroker, tradingMode);
              } catch (sessionErr) {
                console.error('[StockBrokerContext] Failed to init trading session:', sessionErr);
              }
              refreshAllData(newAdapter).catch(err => {
                console.warn('[StockBrokerContext] Background data refresh failed:', err);
              });

              // Auto-download master contract if needed (CRITICAL for chart/symbol lookup)
              setMasterContractReady(false);
              symbolMaster.ensureMasterContract(activeBroker).then((result) => {
                if (result.success) {
                  console.log(`[StockBrokerContext] ✓ Master contract ready (session restore): ${result.total_symbols} symbols`);
                  setMasterContractReady(true);
                } else {
                  console.warn(`[StockBrokerContext] Master contract download issue: ${result.message}`);
                  // Still set to true if we have cached data
                  setMasterContractReady(result.total_symbols > 0);
                }
              }).catch((err) => {
                console.warn(`[StockBrokerContext] Master contract download failed:`, err);
                setMasterContractReady(false);
              });

              console.log(`[StockBrokerContext] Session restored for ${activeBroker}`);
              setIsLoading(false);
              return;
            } else if (hasToken && !isTokenFromToday) {
              console.log(`[StockBrokerContext] Token expired (not from today). User needs to re-authenticate.`);
              setError(`Session expired for ${activeBroker}. Please re-authenticate.`);
            } else {
              console.log(`[StockBrokerContext] No token found. User needs to authenticate.`);
              setError(null);
            }

            setAdapter(newAdapter);
            setIsAuthenticated(false);
            setIsLoading(false);
            return;
          } else {
            console.log(`[StockBrokerContext] No stored credentials found - needs initial setup`);
            setAdapter(newAdapter);
            setIsAuthenticated(false);
            setError(null); // No error - just needs initial setup
            setIsLoading(false);
            return;
          }
        } catch (loadErr) {
          console.log('[StockBrokerContext] Failed to load credentials:', loadErr);
          setAdapter(newAdapter);
          setIsAuthenticated(false);
          setError(null);
          setIsLoading(false);
          return;
        }
      } catch (err) {
        console.error('[StockBrokerContext] Failed to create adapter:', err);
        setIsLoading(false);
      }
    };

    initializeAdapter();
  }, [activeBroker, tradingMode, adapterInitKey]);

  // ============================================================================
  // BROKER MANAGEMENT
  // ============================================================================

  const setActiveBroker = useCallback(async (brokerId: string) => {
    if (!STOCK_BROKER_REGISTRY[brokerId]) {
      throw new Error(`Stock broker "${brokerId}" is not registered`);
    }

    // Abort any ongoing connection
    abortControllerRef.current?.abort();

    // Check if re-selecting the same broker (needs re-init)
    const isReselecting = brokerId === activeBroker;

    // Clean up existing adapter (DON'T logout - preserves credentials in DB)
    if (adapter) {
      console.log(`[StockBrokerContext] ${isReselecting ? 'Re-initializing' : 'Switching from'} ${adapter.brokerId} ${isReselecting ? '' : `to ${brokerId}`}, preserving credentials`);
      // Cleanup WebSocket connections without deleting credentials
      try {
        if (adapter.disconnectWebSocket) {
          await adapter.disconnectWebSocket();
        }
      } catch (err) {
        console.error('[StockBrokerContext] Failed to disconnect WebSocket:', err);
      }
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

    // For paper-only brokers like yfinance, force paper mode
    if (brokerId === 'yfinance' && tradingMode !== 'paper') {
      console.log(`[StockBrokerContext] Paper-only broker ${brokerId} selected, forcing paper mode`);
      setTradingModeState('paper');
      await safeStorageSet(STORAGE_KEYS.STOCK_TRADING_MODE, 'paper');
    }

    // Set new broker (or same broker for re-init)
    setActiveBrokerState(brokerId);
    await safeStorageSet(STORAGE_KEYS.ACTIVE_STOCK_BROKER, brokerId);

    // If re-selecting same broker, increment key to force re-initialization
    if (isReselecting) {
      console.log(`[StockBrokerContext] Re-selecting same broker, forcing re-init...`);
      setAdapterInitKey(prev => prev + 1);
    }

    // Adapter will be created by useEffect watching activeBroker + adapterInitKey
    console.log(`[StockBrokerContext] Active broker set to: ${brokerId}`);
  }, [adapter, tradingMode, activeBroker]);

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

          // Auto-download master contract if needed
          setMasterContractReady(false);
          symbolMaster.ensureMasterContract(activeBroker).then((result) => {
            if (result.success) {
              console.log(`[StockBrokerContext] ✓ Master contract ready: ${result.total_symbols} symbols`);
              setMasterContractReady(true);
            } else {
              console.warn(`[StockBrokerContext] Master contract download issue: ${result.message}`);
              // Still set to true if we have cached data
              setMasterContractReady(result.total_symbols > 0);
            }
          }).catch((err) => {
            console.warn(`[StockBrokerContext] Master contract download failed:`, err);
            setMasterContractReady(false);
          });

          // Connect WebSocket for real-time data streaming
          try {
            await adapterToUse.connectWebSocket();
            console.log(`[StockBrokerContext] ✓ WebSocket connected for ${activeBroker}`);

            // If in paper trading mode, set up tick handler to update paper positions
            if (tradingMode === 'paper') {
              const { invoke } = await import('@tauri-apps/api/core');
              adapterToUse.onTick(async (tick) => {
                try {
                  // Update paper trading position prices
                  const symbol = `${tick.exchange}:${tick.symbol}`;
                  await invoke('process_tick_for_paper_trading', {
                    symbol,
                    price: tick.lastPrice
                  });
                } catch (err) {
                  console.error('[StockBrokerContext] Failed to process tick for paper trading:', err);
                }
              });
              console.log(`[StockBrokerContext] ✓ Paper trading tick handler registered`);
            }
          } catch (wsErr) {
            console.warn(`[StockBrokerContext] WebSocket connection failed for ${activeBroker}, will use REST polling:`, wsErr);
          }

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

  /**
   * Connect using stored credentials (calls initFromStorage on the adapter)
   * This is the explicit user-triggered connection - won't happen automatically
   */
  const connect = useCallback(async (): Promise<AuthResponse> => {
    if (!activeBroker) {
      return {
        success: false,
        message: 'No broker selected',
        errorCode: 'NO_BROKER',
      };
    }

    if (!adapter) {
      return {
        success: false,
        message: 'No adapter available. Please select a broker first.',
        errorCode: 'NO_ADAPTER',
      };
    }

    // Abort any existing connection
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    setIsConnecting(true);
    setError(null);

    try {
      console.log(`[StockBrokerContext] User-initiated connect for: ${activeBroker}`);

      // Try to restore session using stored credentials
      if (typeof (adapter as any).initFromStorage === 'function') {
        const restored = await (adapter as any).initFromStorage();

        if (restored) {
          console.log(`[StockBrokerContext] ✓ Session restored successfully via connect()`);

          setIsAuthenticated(true);
          setError(null);

          // Initialize unified trading session
          try {
            await initTradingSession(activeBroker, tradingMode);
            console.log(`[StockBrokerContext] ✓ Trading session initialized`);
          } catch (sessionErr) {
            console.error('[StockBrokerContext] Failed to init trading session:', sessionErr);
          }

          // Fetch initial data
          await refreshAllData(adapter);

          // Auto-download master contract if needed
          setMasterContractReady(false);
          symbolMaster.ensureMasterContract(activeBroker).then((result) => {
            if (result.success) {
              console.log(`[StockBrokerContext] ✓ Master contract ready: ${result.total_symbols} symbols`);
              setMasterContractReady(true);
            } else {
              console.warn(`[StockBrokerContext] Master contract download issue: ${result.message}`);
              setMasterContractReady(result.total_symbols > 0);
            }
          }).catch((err) => {
            console.warn(`[StockBrokerContext] Master contract download failed:`, err);
            setMasterContractReady(false);
          });

          // Connect WebSocket for real-time data streaming
          try {
            await adapter.connectWebSocket();
            console.log(`[StockBrokerContext] ✓ WebSocket connected for ${activeBroker}`);

            // If in paper trading mode, set up tick handler to update paper positions
            if (tradingMode === 'paper') {
              const { invoke } = await import('@tauri-apps/api/core');
              adapter.onTick(async (tick) => {
                try {
                  const symbol = `${tick.exchange}:${tick.symbol}`;
                  await invoke('process_tick_for_paper_trading', {
                    symbol,
                    price: tick.lastPrice
                  });
                } catch (err) {
                  console.error('[StockBrokerContext] Failed to process tick for paper trading:', err);
                }
              });
              console.log(`[StockBrokerContext] ✓ Paper trading tick handler registered`);
            }
          } catch (wsErr) {
            console.warn(`[StockBrokerContext] WebSocket connection failed for ${activeBroker}, will use REST polling:`, wsErr);
          }

          return {
            success: true,
            message: 'Connected successfully',
          };
        } else {
          // Session restore failed - need re-authentication
          const errorMsg = `Session expired for ${activeBroker}. Please re-authenticate in the BROKERS tab.`;
          setError(errorMsg);
          setIsAuthenticated(false);
          return {
            success: false,
            message: errorMsg,
            errorCode: 'SESSION_EXPIRED',
          };
        }
      } else {
        // No initFromStorage method - need manual auth
        const errorMsg = `${activeBroker} requires manual authentication. Please configure in BROKERS tab.`;
        setError(errorMsg);
        return {
          success: false,
          message: errorMsg,
          errorCode: 'AUTH_REQUIRED',
        };
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Connection failed';
      setError(errorMessage);
      return {
        success: false,
        message: errorMessage,
        errorCode: 'CONNECT_ERROR',
      };
    } finally {
      setIsConnecting(false);
    }
  }, [activeBroker, adapter, tradingMode]);

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
  // AUTO-REFRESH POSITIONS AFTER ORDER PLACEMENT (FROM MCP/CHAT)
  // ============================================================================

  useEffect(() => {
    const handleOrderPlaced = async (event: Event) => {
      const customEvent = event as CustomEvent;
      console.log('[StockBrokerContext] Order placed, refreshing data from DB...', customEvent.detail);
      // Simply refresh positions and orders from database
      // Call functions directly - they're stable enough or will use latest closure
      await refreshPositions();
      await refreshOrders();
    };

    window.addEventListener('stock-order-placed', handleOrderPlaced);
    return () => {
      window.removeEventListener('stock-order-placed', handleOrderPlaced);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // Empty deps - event listener setup only once

  // ============================================================================
  // PERIODIC REFRESH FOR PAPER TRADING (Real-time price updates from WebSocket)
  // ============================================================================

  useEffect(() => {
    // Only in paper mode with active positions
    if (tradingMode !== 'paper' || !isAuthenticated) {
      return;
    }

    // Refresh every 3 seconds to show updated prices
    // (WebSocket updates the DB, we just need to read it)
    const intervalId = setInterval(() => {
      refreshPositions();
    }, 3000);

    return () => clearInterval(intervalId);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tradingMode, isAuthenticated]); // Don't include refreshPositions to avoid recreating interval

  // ============================================================================
  // SUBSCRIBE TO POSITION SYMBOLS (Paper Trading Real-time Updates)
  // ============================================================================

  // Use a ref to track subscribed symbols and avoid re-subscription
  const subscribedSymbolsRef = useRef<Set<string>>(new Set());

  useEffect(() => {
    // Only subscribe in paper mode when we have positions
    if (tradingMode !== 'paper' || !adapter || positions.length === 0) {
      return;
    }

    // Subscribe to all position symbols once
    const subscribeToPositions = async () => {
      const currentSymbols = new Set(positions.map(p => `${p.symbol}:${p.exchange}`));
      const symbolsToSubscribe = positions.filter(pos => {
        const key = `${pos.symbol}:${pos.exchange}`;
        return !subscribedSymbolsRef.current.has(key);
      });

      if (symbolsToSubscribe.length === 0) {
        return; // All symbols already subscribed
      }

      console.log(`[StockBrokerContext] Subscribing to ${symbolsToSubscribe.length} new position symbols...`);
      for (const pos of symbolsToSubscribe) {
        try {
          await adapter.subscribe(pos.symbol, pos.exchange, 'quote');
          subscribedSymbolsRef.current.add(`${pos.symbol}:${pos.exchange}`);
        } catch (err) {
          console.warn(`[StockBrokerContext] Failed to subscribe to ${pos.symbol}:`, err);
        }
      }
    };

    subscribeToPositions();
  }, [positions, tradingMode, adapter]); // positions is now stable

  // ============================================================================
  // MCP BRIDGE INTEGRATION
  // ============================================================================

  useEffect(() => {
    // Connect stock broker adapter to MCP bridge for AI-controlled trading
    if (adapter && isAuthenticated && activeBroker) {
      import('@/services/mcp/internal').then(({ stockBrokerMCPBridge }) => {
        stockBrokerMCPBridge.connect({
          activeAdapter: adapter,
          tradingMode,
          activeBroker,
          isAuthenticated,
        });

        console.log(`[StockBrokerContext] Connected to MCP bridge: ${activeBroker} (${tradingMode} mode)`);
      });

      return () => {
        import('@/services/mcp/internal').then(({ stockBrokerMCPBridge }) => {
          stockBrokerMCPBridge.disconnect();
          console.log('[StockBrokerContext] Disconnected from MCP bridge');
        });
      };
    }
  }, [adapter, isAuthenticated, activeBroker, tradingMode]);

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
      connect,
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

      // Master contract
      masterContractReady,

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
      connect,
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
      masterContractReady,
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
 * Optional hook for accessing stock broker context
 * Returns null if provider is not available instead of throwing an error
 */
export function useStockBrokerContextOptional(): StockBrokerContextType | null {
  const context = useContext(StockBrokerContext);
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
    connect,
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
    connect, // Explicit connect using stored credentials
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
