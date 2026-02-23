/**
 * useExchangeMarkets
 *
 * Derives the watchlist symbols and wsId<->unified symbol maps directly from
 * the CCXT realAdapter that is already connected in BrokerContext.
 *
 * - No hardcoded symbol lists
 * - Works for any broker (Kraken, Binance, HyperLiquid, OKX, …)
 * - Automatically updates when the broker changes
 */

import { useState, useEffect } from 'react';
import type { CoreExchangeAdapter } from '@/brokers/crypto/base/CoreAdapter';
import { DEFAULT_CRYPTO_WATCHLIST } from '@/components/tabs/crypto-trading/constants';

export interface ExchangeMarkets {
  /** wsId → unified symbol  e.g. "XBT/USD" → "BTC/USD" */
  wsIdToUnified: Map<string, string>;
  /** unified symbol → wsId  e.g. "BTC/USD" → "XBT/USD" */
  unifiedToWsId: Map<string, string>;
  /** Default watchlist: preferred symbols available on this exchange (USD/USDT spot) */
  watchlistSymbols: string[];
  /** True once maps are populated from the exchange */
  ready: boolean;
}

const PREFERRED_QUOTE = ['USD', 'USDT'];

// Symbols we want to show by preference order (unified format)
const PREFERRED_SYMBOLS = [
  'BTC/USD', 'ETH/USD', 'SOL/USD', 'XRP/USD', 'ADA/USD',
  'DOGE/USD', 'AVAX/USD', 'DOT/USD', 'LTC/USD', 'LINK/USD',
  'UNI/USD', 'ATOM/USD', 'XLM/USD', 'ETC/USD', 'BCH/USD',
  'NEAR/USD', 'FIL/USD', 'ICP/USD', 'INJ/USD', 'MKR/USD',
  'AAVE/USD', 'PEPE/USD', 'WIF/USD', 'BONK/USD', 'SUI/USD',
  'BNB/USD', 'BNB/USDT', 'ARB/USD', 'OP/USD', 'TRX/USDT',
];

function buildWatchlist(activeSymbols: Set<string>): string[] {
  const result: string[] = [];

  // First: include preferred symbols that exist on this exchange
  for (const sym of PREFERRED_SYMBOLS) {
    if (activeSymbols.has(sym)) result.push(sym);
  }

  // Fill up to 30 with other USD spot pairs alphabetically if needed
  if (result.length < 30) {
    for (const sym of Array.from(activeSymbols).sort()) {
      if (result.length >= 30) break;
      if (!result.includes(sym)) result.push(sym);
    }
  }

  return result.slice(0, 30);
}

export function useExchangeMarkets(realAdapter: any | null): ExchangeMarkets {
  const [markets, setMarkets] = useState<ExchangeMarkets>({
    wsIdToUnified: new Map(),
    unifiedToWsId: new Map(),
    watchlistSymbols: DEFAULT_CRYPTO_WATCHLIST,
    ready: false,
  });

  useEffect(() => {
    if (!realAdapter || typeof realAdapter.getMarketSymbolMap !== 'function') {
      // Adapter not ready or doesn't have the method — keep defaults
      setMarkets(prev => ({ ...prev, ready: false }));
      return;
    }

    const { wsIdToUnified, unifiedToWsId, activeUsdSymbols } =
      (realAdapter as CoreExchangeAdapter).getMarketSymbolMap();

    if (wsIdToUnified.size === 0) {
      // Markets not loaded yet (connect() hasn't finished)
      setMarkets(prev => ({ ...prev, ready: false }));
      return;
    }

    const activeSet = new Set(activeUsdSymbols);
    const watchlistSymbols = buildWatchlist(activeSet);

    setMarkets({ wsIdToUnified, unifiedToWsId, watchlistSymbols, ready: true });
  }, [realAdapter]);

  return markets;
}
