/**
 * Fyers Symbol Search & Master Contract
 *
 * Module-level symbol search functions used by FyersAdapter
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  Instrument,
  StockExchange,
} from '../../types';

/**
 * Search for symbols in master contract database
 */
export async function fyersSearchSymbols(
  query: string,
  exchange?: StockExchange
): Promise<Instrument[]> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: {
        results: Array<Record<string, unknown>>;
        count: number;
      };
      error?: string;
    }>('fyers_search_symbol', {
      keyword: query,
      exchange: exchange || null,
      limit: 20,
    });

    if (!response.success || !response.data) {
      console.warn('[Fyers] searchSymbols: No results or error:', response.error);
      return [];
    }

    return response.data.results.map(item => ({
      symbol: String(item.symbol || ''),
      exchange: String(item.exchange || 'NSE') as StockExchange,
      name: String(item.name || item.symbol || ''),
      token: String(item.fytoken || item.token || '0'),
      lotSize: Number(item.lot_size || 1),
      tickSize: Number(item.tick_size || 0.05),
      instrumentType: String(item.segment || item.instrument_type || 'EQ') as any,
      currency: 'INR',
      expiry: item.expiry ? String(item.expiry) : undefined,
      strike: item.strike ? Number(item.strike) : undefined,
    }));
  } catch (error) {
    console.error('[Fyers] searchSymbols error:', error);
    return [];
  }
}

/**
 * Get instrument details by symbol and exchange
 */
export async function fyersGetInstrument(
  symbol: string,
  exchange: StockExchange
): Promise<Instrument | null> {
  const fallback: Instrument = {
    symbol,
    exchange,
    name: symbol,
    token: '0',
    lotSize: 1,
    tickSize: 0.05,
    instrumentType: 'EQUITY',
    currency: 'INR',
  };

  try {
    const tokenResponse = await invoke<{
      success: boolean;
      data?: number;
      error?: string;
    }>('fyers_get_token_for_symbol', {
      symbol,
      exchange,
    });

    if (!tokenResponse.success || !tokenResponse.data) {
      return fallback;
    }

    const symbolResponse = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('fyers_get_symbol_by_token', {
      token: tokenResponse.data,
    });

    if (!symbolResponse.success || !symbolResponse.data) {
      return {
        ...fallback,
        token: String(tokenResponse.data),
      };
    }

    const data = symbolResponse.data;
    return {
      symbol: String(data.symbol || symbol),
      exchange: String(data.exchange || exchange) as StockExchange,
      name: String(data.name || data.symbol || symbol),
      token: String(data.fytoken || data.token || tokenResponse.data),
      lotSize: Number(data.lot_size || 1),
      tickSize: Number(data.tick_size || 0.05),
      instrumentType: String(data.segment || data.instrument_type || 'EQ') as any,
      currency: 'INR',
      expiry: data.expiry ? String(data.expiry) : undefined,
      strike: data.strike ? Number(data.strike) : undefined,
    };
  } catch (error) {
    console.error('[Fyers] getInstrument error:', error);
    return fallback;
  }
}

/**
 * Download master contract from Fyers
 * Downloads 6 CSV segments: NSE_CM, NSE_FO, NSE_CD, BSE_CM, BSE_FO, MCX_COM
 */
export async function fyersDownloadMasterContract(): Promise<void> {
  console.log('[Fyers] Downloading master contract...');
  try {
    const response = await invoke<{
      success: boolean;
      message?: string;
      segments?: Record<string, number>;
      total_symbols?: number;
      error?: string;
    }>('download_fyers_master_contract');

    if (!response.success) {
      throw new Error(response.error || 'Failed to download master contract');
    }

    console.log(`[Fyers] Master contract downloaded: ${response.total_symbols} symbols`);
    if (response.segments) {
      console.log('[Fyers] Segments:', response.segments);
    }
  } catch (error) {
    console.error('[Fyers] downloadMasterContract error:', error);
    throw error;
  }
}

/**
 * Get last updated timestamp of master contract
 */
export async function fyersGetMasterContractLastUpdated(): Promise<Date | null> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: {
        last_updated: number;
        symbol_count: number;
        age_seconds: number;
      };
      error?: string;
    }>('fyers_get_master_contract_metadata');

    if (!response.success || !response.data) {
      return null;
    }

    return new Date(response.data.last_updated * 1000);
  } catch (error) {
    console.error('[Fyers] getMasterContractLastUpdated error:', error);
    return null;
  }
}
