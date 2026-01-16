// MarketDataProvider - React component to feed prices into UnifiedMarketDataService
// Wraps trading components and keeps market data flowing from WebSocket hooks

import React, { useEffect, useRef, createContext, useContext, ReactNode } from 'react';
import {
  UnifiedMarketDataService,
  getMarketDataService,
  initializeMarketDataService,
} from '../../services/trading/UnifiedMarketDataService';
import { useRustTicker } from '../../hooks/useRustWebSocket';

// Context for accessing the market data service
interface MarketDataContextValue {
  service: UnifiedMarketDataService;
  isInitialized: boolean;
}

const MarketDataContext = createContext<MarketDataContextValue | null>(null);

/**
 * Hook to access the market data service
 */
export function useMarketDataService(): UnifiedMarketDataService {
  const context = useContext(MarketDataContext);
  if (!context) {
    // If not in provider, return singleton
    return getMarketDataService();
  }
  return context.service;
}

/**
 * Hook to check if market data service is initialized
 */
export function useMarketDataInitialized(): boolean {
  const context = useContext(MarketDataContext);
  return context?.isInitialized ?? false;
}

interface MarketDataProviderProps {
  children: ReactNode;
}

/**
 * MarketDataProvider - Initializes and provides the market data service
 *
 * This component should wrap your trading UI at a high level.
 * It initializes the UnifiedMarketDataService and starts listening
 * to WebSocket events for price updates.
 */
export function MarketDataProvider({ children }: MarketDataProviderProps) {
  const serviceRef = useRef<UnifiedMarketDataService | null>(null);
  const [isInitialized, setIsInitialized] = React.useState(false);

  useEffect(() => {
    let mounted = true;

    async function init() {
      try {
        const service = await initializeMarketDataService();
        if (mounted) {
          serviceRef.current = service;
          setIsInitialized(true);
          console.log('[MarketDataProvider] Service initialized');
        }
      } catch (error) {
        console.error('[MarketDataProvider] Failed to initialize:', error);
      }
    }

    init();

    return () => {
      mounted = false;
      // Don't destroy service on unmount - it's a singleton
    };
  }, []);

  const value: MarketDataContextValue = {
    service: serviceRef.current || getMarketDataService(),
    isInitialized,
  };

  return (
    <MarketDataContext.Provider value={value}>
      {children}
    </MarketDataContext.Provider>
  );
}

interface SymbolPriceFeederProps {
  provider: string;
  symbol: string;
  children?: ReactNode;
}

/**
 * SymbolPriceFeeder - Feeds a single symbol's price into the market data service
 *
 * Use this component when you want to ensure a specific symbol's price
 * is being streamed and available in the market data service.
 *
 * Example:
 * <SymbolPriceFeeder provider="kraken" symbol="BTC/USD">
 *   <TradingPanel />
 * </SymbolPriceFeeder>
 */
export function SymbolPriceFeeder({ provider, symbol, children }: SymbolPriceFeederProps) {
  const service = useMarketDataService();
  const { data: ticker } = useRustTicker(provider, symbol);

  // Feed ticker data into service
  useEffect(() => {
    if (ticker && ticker.price > 0) {
      service.updatePrice(provider, symbol, {
        price: ticker.price,
        bid: ticker.bid,
        ask: ticker.ask,
        volume: ticker.volume,
        high: ticker.high,
        low: ticker.low,
        change: ticker.change,
        change_percent: ticker.change_percent,
        timestamp: ticker.timestamp,
      });
    }
  }, [ticker, provider, symbol, service]);

  return <>{children}</>;
}

interface MultiSymbolPriceFeederProps {
  provider: string;
  symbols: string[];
  children?: ReactNode;
}

/**
 * MultiSymbolPriceFeeder - Feeds multiple symbols' prices into the market data service
 *
 * More efficient than multiple SymbolPriceFeeder components as it handles
 * all subscriptions in one place.
 */
export function MultiSymbolPriceFeeder({ provider, symbols, children }: MultiSymbolPriceFeederProps) {
  return (
    <>
      {symbols.map((symbol) => (
        <SymbolPriceFeederInternal key={`${provider}:${symbol}`} provider={provider} symbol={symbol} />
      ))}
      {children}
    </>
  );
}

// Internal component for each symbol (no children)
function SymbolPriceFeederInternal({ provider, symbol }: { provider: string; symbol: string }) {
  const service = useMarketDataService();
  const { data: ticker } = useRustTicker(provider, symbol);

  useEffect(() => {
    if (ticker && ticker.price > 0) {
      service.updatePrice(provider, symbol, {
        price: ticker.price,
        bid: ticker.bid,
        ask: ticker.ask,
        volume: ticker.volume,
        high: ticker.high,
        low: ticker.low,
        change: ticker.change,
        change_percent: ticker.change_percent,
        timestamp: ticker.timestamp,
      });
    }
  }, [ticker, provider, symbol, service]);

  return null;
}

export default MarketDataProvider;
