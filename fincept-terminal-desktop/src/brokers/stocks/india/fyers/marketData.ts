/**
 * Fyers Market Data Operations
 *
 * Module-level market data functions used by FyersAdapter
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  Quote,
  OHLCV,
  MarketDepth,
  TimeFrame,
  StockExchange,
} from '../../types';
import { toFyersSymbol, toFyersInterval, fromFyersQuote } from './mapper';

/**
 * Get quote for a single symbol
 */
export async function fyersGetQuote(
  apiKey: string,
  accessToken: string,
  symbol: string,
  exchange: StockExchange
): Promise<Quote> {
  const defaultQuote: Quote = {
    symbol,
    exchange,
    lastPrice: 0,
    open: 0,
    high: 0,
    low: 0,
    close: 0,
    previousClose: 0,
    change: 0,
    changePercent: 0,
    volume: 0,
    bid: 0,
    bidQty: 0,
    ask: 0,
    askQty: 0,
    timestamp: Date.now(),
  };

  try {
    const fyersSymbol = toFyersSymbol(symbol, exchange);

    const response = await invoke<{
      success: boolean;
      data?: unknown[];
      error?: string;
    }>('fyers_get_quotes', {
      apiKey,
      accessToken,
      symbols: [fyersSymbol],
    });

    if (response.success && response.data && response.data.length > 0) {
      return fromFyersQuote(response.data[0] as Record<string, unknown>);
    }

    return defaultQuote;
  } catch (error) {
    console.error('Failed to fetch quote:', error);
    return defaultQuote;
  }
}

/**
 * Get quotes for multiple symbols (batch processing)
 * Automatically batches into groups of 50 symbols
 * Fyers API rate limit: ~10 requests per second
 */
export async function fyersGetMultipleQuotes(
  apiKey: string,
  accessToken: string,
  symbols: Array<{ symbol: string; exchange: StockExchange }>
): Promise<Quote[]> {
  const BATCH_SIZE = 50; // Fyers supports up to 50 symbols per request
  const RATE_LIMIT_DELAY = 1000; // 1 second delay between batches
  const allResults: Quote[] = [];

  try {
    for (let i = 0; i < symbols.length; i += BATCH_SIZE) {
      const batch = symbols.slice(i, i + BATCH_SIZE);

      const fyersSymbols = batch.map(({ symbol, exchange }) =>
        toFyersSymbol(symbol, exchange)
      );

      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('fyers_get_quotes', {
        apiKey,
        accessToken,
        symbols: fyersSymbols,
      });

      if (response.success && response.data) {
        const quotes = response.data.map((item: unknown) =>
          fromFyersQuote(item as Record<string, unknown>)
        );
        allResults.push(...quotes);
      }

      if (i + BATCH_SIZE < symbols.length) {
        await new Promise(resolve => setTimeout(resolve, RATE_LIMIT_DELAY));
      }
    }

    console.log(`[Fyers] Fetched ${allResults.length} quotes in ${Math.ceil(symbols.length / BATCH_SIZE)} batches`);
    return allResults;
  } catch (error) {
    console.error('Failed to fetch multiple quotes:', error);
    return [];
  }
}

/**
 * Get OHLCV data - Historical data
 */
export async function fyersGetOHLCV(
  apiKey: string,
  accessToken: string,
  symbol: string,
  exchange: StockExchange,
  timeframe: TimeFrame,
  from: Date,
  to: Date
): Promise<OHLCV[]> {
  try {
    const fyersSymbol = toFyersSymbol(symbol, exchange);
    const fyersInterval = toFyersInterval(timeframe);

    const response = await invoke<{
      success: boolean;
      data?: Array<[number, number, number, number, number, number]>; // [timestamp, O, H, L, C, V]
      error?: string;
    }>('fyers_get_history', {
      apiKey,
      accessToken,
      symbol: fyersSymbol,
      resolution: fyersInterval,
      fromDate: from.toISOString().split('T')[0], // YYYY-MM-DD
      toDate: to.toISOString().split('T')[0],
    });

    console.log('[FyersAdapter] Raw response:', {
      success: response.success,
      dataLength: response.data?.length,
      error: response.error,
      firstCandle: response.data?.[0],
    });

    if (response.success && response.data) {
      const candles = response.data.map(candle => ({
        timestamp: candle[0],
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5],
      }));
      console.log(`[FyersAdapter] ✓ Fetched ${candles.length} candles for ${fyersSymbol}`);
      return candles;
    }

    const errorMsg = response.error || 'Unknown error from Fyers API';
    console.error(`[FyersAdapter] ❌ Fyers API error: ${errorMsg}`);
    throw new Error(`Fyers API error: ${errorMsg}`);
  } catch (error) {
    console.error('[FyersAdapter] Failed to fetch historical data:', error);
    throw error;
  }
}

/**
 * Get market depth - 5-level bid/ask
 */
export async function fyersGetMarketDepth(
  apiKey: string,
  accessToken: string,
  symbol: string,
  exchange: StockExchange
): Promise<MarketDepth> {
  try {
    const fyersSymbol = toFyersSymbol(symbol, exchange);

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('fyers_get_depth', {
      apiKey,
      accessToken,
      symbol: fyersSymbol,
    });

    if (response.success && response.data) {
      const depth = response.data;
      const bids = (depth.bids as Array<Record<string, unknown>>) || [];
      const asks = (depth.ask as Array<Record<string, unknown>>) || []; // Fyers uses 'ask' not 'asks'

      return {
        bids: bids.slice(0, 5).map(b => ({
          price: Number(b.price || 0),
          quantity: Number(b.volume || 0),
          orders: Number(b.ord || 0),
        })),
        asks: asks.slice(0, 5).map(a => ({
          price: Number(a.price || 0),
          quantity: Number(a.volume || 0),
          orders: Number(a.ord || 0),
        })),
      };
    }

    return { bids: [], asks: [] };
  } catch (error) {
    console.error('Failed to fetch market depth:', error);
    return { bids: [], asks: [] };
  }
}

/**
 * Fetch quotes for a list of Fyers-formatted symbols and return raw quote data
 */
export async function fyersFetchBatchQuotesRaw(
  apiKey: string,
  accessToken: string,
  fyersSymbols: string[]
): Promise<{ success: boolean; data?: unknown[]; error?: string }> {
  return invoke<{
    success: boolean;
    data?: unknown[];
    error?: string;
  }>('fyers_get_quotes', {
    apiKey,
    accessToken,
    symbols: fyersSymbols,
  });
}
