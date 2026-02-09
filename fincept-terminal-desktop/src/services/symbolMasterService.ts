/**
 * Symbol Master Service
 *
 * Unified service for managing broker symbol databases.
 * Provides search, status tracking, and auto-download functionality.
 */

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// TYPES
// ============================================================================

export interface SymbolSearchResult {
  symbol: string;
  br_symbol: string;
  name: string | null;
  exchange: string;
  token: string;
  expiry: string | null;
  strike: number | null;
  lot_size: number | null;
  instrument_type: string | null;
  tick_size: number | null;
  relevance_score: number;
}

export interface MasterContractStatus {
  broker_id: string;
  status: 'pending' | 'downloading' | 'success' | 'error' | 'unknown';
  message: string | null;
  last_updated: number;
  total_symbols: number;
  is_ready: boolean;
}

export interface SymbolSearchResponse {
  success: boolean;
  symbols: SymbolSearchResult[];
  total: number;
  error?: string;
}

export interface StatusResponse {
  success: boolean;
  status: MasterContractStatus | null;
  error?: string;
}

export interface CacheValidityResponse {
  success: boolean;
  is_valid: boolean;
  age_seconds: number | null;
  ttl_seconds: number;
  error?: string;
}

export interface DownloadResponse {
  success: boolean;
  message: string;
  total_symbols: number;
}

// ============================================================================
// SEARCH FUNCTIONS
// ============================================================================

/**
 * Search symbols for a broker
 */
export async function searchSymbols(
  brokerId: string,
  query: string,
  options?: {
    exchange?: string;
    instrumentType?: string;
    limit?: number;
  }
): Promise<SymbolSearchResponse> {
  try {
    return await invoke<SymbolSearchResponse>('symbol_master_search', {
      brokerId,
      query,
      exchange: options?.exchange,
      instrumentType: options?.instrumentType,
      limit: options?.limit || 50,
    });
  } catch (error) {
    console.error('[symbolMaster] Search error:', error);
    return {
      success: false,
      symbols: [],
      total: 0,
      error: String(error),
    };
  }
}

/**
 * Get symbol by exact match
 */
export async function getSymbol(
  brokerId: string,
  symbol: string,
  exchange: string
): Promise<SymbolSearchResult | null> {
  try {
    const result = await invoke<{ success: boolean; symbol: SymbolSearchResult | null }>(
      'symbol_master_get_symbol',
      { brokerId, symbol, exchange }
    );
    return result.symbol;
  } catch (error) {
    console.error('[symbolMaster] Get symbol error:', error);
    return null;
  }
}

/**
 * Get symbol by token
 */
export async function getSymbolByToken(
  brokerId: string,
  token: string
): Promise<SymbolSearchResult | null> {
  try {
    const result = await invoke<{ success: boolean; symbol: SymbolSearchResult | null }>(
      'symbol_master_get_by_token',
      { brokerId, token }
    );
    return result.symbol;
  } catch (error) {
    console.error('[symbolMaster] Get by token error:', error);
    return null;
  }
}

/**
 * Get token for a symbol (useful for WebSocket subscriptions)
 */
export async function getToken(
  brokerId: string,
  symbol: string,
  exchange: string
): Promise<string | null> {
  try {
    const result = await invoke<{ success: boolean; token: string | null }>(
      'symbol_master_get_token',
      { brokerId, symbol, exchange }
    );
    return result.token;
  } catch (error) {
    console.error('[symbolMaster] Get token error:', error);
    return null;
  }
}

// ============================================================================
// STATUS FUNCTIONS
// ============================================================================

/**
 * Get master contract status for a broker
 */
export async function getStatus(brokerId: string): Promise<MasterContractStatus | null> {
  try {
    const result = await invoke<StatusResponse>('symbol_master_get_status', { brokerId });
    return result.status;
  } catch (error) {
    console.error('[symbolMaster] Get status error:', error);
    return null;
  }
}

/**
 * Check if master contracts are ready (cached and valid)
 */
export async function isReady(brokerId: string): Promise<CacheValidityResponse> {
  try {
    return await invoke<CacheValidityResponse>('symbol_master_is_ready', { brokerId });
  } catch (error) {
    console.error('[symbolMaster] Is ready error:', error);
    return {
      success: false,
      is_valid: false,
      age_seconds: null,
      ttl_seconds: 86400,
      error: String(error),
    };
  }
}

/**
 * Initialize broker status (should be called on login)
 */
export async function initBroker(brokerId: string): Promise<StatusResponse> {
  try {
    return await invoke<StatusResponse>('symbol_master_init_broker', { brokerId });
  } catch (error) {
    console.error('[symbolMaster] Init broker error:', error);
    return {
      success: false,
      status: null,
      error: String(error),
    };
  }
}

// ============================================================================
// METADATA FUNCTIONS
// ============================================================================

/**
 * Get symbol count for a broker
 */
export async function getSymbolCount(brokerId: string): Promise<number> {
  try {
    const result = await invoke<{ success: boolean; count: number }>('symbol_master_get_count', { brokerId });
    return result.count;
  } catch (error) {
    console.error('[symbolMaster] Get count error:', error);
    return 0;
  }
}

/**
 * Get available exchanges for a broker
 */
export async function getExchanges(brokerId: string): Promise<string[]> {
  try {
    const result = await invoke<{ success: boolean; exchanges: string[] }>('symbol_master_get_exchanges', { brokerId });
    return result.exchanges;
  } catch (error) {
    console.error('[symbolMaster] Get exchanges error:', error);
    return [];
  }
}

/**
 * Get available expiries for FNO symbols
 */
export async function getExpiries(
  brokerId: string,
  options?: { exchange?: string; underlying?: string }
): Promise<string[]> {
  try {
    const result = await invoke<{ success: boolean; expiries: string[] }>('symbol_master_get_expiries', {
      brokerId,
      exchange: options?.exchange,
      underlying: options?.underlying,
    });
    return result.expiries;
  } catch (error) {
    console.error('[symbolMaster] Get expiries error:', error);
    return [];
  }
}

/**
 * Get available underlyings (for option chain)
 */
export async function getUnderlyings(
  brokerId: string,
  exchange?: string
): Promise<string[]> {
  try {
    const result = await invoke<{ success: boolean; underlyings: string[] }>('symbol_master_get_underlyings', {
      brokerId,
      exchange,
    });
    return result.underlyings;
  } catch (error) {
    console.error('[symbolMaster] Get underlyings error:', error);
    return [];
  }
}

// ============================================================================
// DOWNLOAD FUNCTIONS
// ============================================================================

/**
 * Download master contract for a broker
 * Automatically selects the correct download function based on broker ID
 */
export async function downloadMasterContract(
  brokerId: string,
  credentials?: { apiKey?: string; accessToken?: string }
): Promise<DownloadResponse> {
  try {
    switch (brokerId.toLowerCase()) {
      case 'fyers':
        return await invoke<DownloadResponse>('fyers_download_symbols');

      case 'zerodha':
        if (!credentials?.apiKey || !credentials?.accessToken) {
          return {
            success: false,
            message: 'Zerodha requires API key and access token',
            total_symbols: 0,
          };
        }
        return await invoke<DownloadResponse>('zerodha_download_symbols', {
          apiKey: credentials.apiKey,
          accessToken: credentials.accessToken,
        });

      case 'angelone':
      case 'angel':
        return await invoke<DownloadResponse>('angelone_download_symbols');

      case 'upstox':
        return await invoke<DownloadResponse>('upstox_download_symbols');

      case 'dhan':
        return await invoke<DownloadResponse>('dhan_download_symbols');

      case 'shoonya':
      case 'finvasia':
        return await invoke<DownloadResponse>('shoonya_download_symbols');

      default:
        return {
          success: false,
          message: `No download implementation for broker: ${brokerId}`,
          total_symbols: 0,
        };
    }
  } catch (error) {
    console.error(`[symbolMaster] Download error for ${brokerId}:`, error);
    return {
      success: false,
      message: String(error),
      total_symbols: 0,
    };
  }
}

/**
 * Check and download master contract if needed
 * Auto-downloads if cache is expired or empty
 */
export async function ensureMasterContract(
  brokerId: string,
  credentials?: { apiKey?: string; accessToken?: string }
): Promise<DownloadResponse> {
  try {
    // Check if cache is valid
    const cacheStatus = await isReady(brokerId);

    if (cacheStatus.is_valid) {
      console.log(`[symbolMaster] Cache valid for ${brokerId}, skipping download`);
      const count = await getSymbolCount(brokerId);
      return {
        success: true,
        message: `Using cached symbols (${formatAge(cacheStatus.age_seconds)} old)`,
        total_symbols: count,
      };
    }

    console.log(`[symbolMaster] Cache expired/empty for ${brokerId}, downloading...`);
    return await downloadMasterContract(brokerId, credentials);
  } catch (error) {
    console.error(`[symbolMaster] Ensure master contract error for ${brokerId}:`, error);
    return {
      success: false,
      message: String(error),
      total_symbols: 0,
    };
  }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Normalize index symbol to unified format
 */
export async function normalizeIndexSymbol(symbol: string): Promise<string> {
  try {
    return await invoke<string>('symbol_master_normalize_index', { symbol });
  } catch (error) {
    console.error('[symbolMaster] Normalize index error:', error);
    return symbol;
  }
}

/**
 * Format expiry date to DD-MMM-YY format
 */
export async function formatExpiry(expiry: string): Promise<string> {
  try {
    return await invoke<string>('symbol_master_format_expiry', { expiry });
  } catch (error) {
    console.error('[symbolMaster] Format expiry error:', error);
    return expiry;
  }
}

/**
 * Build unified futures symbol
 */
export async function buildFuturesSymbol(underlying: string, expiry: string): Promise<string> {
  try {
    return await invoke<string>('symbol_master_build_futures_symbol', { underlying, expiry });
  } catch (error) {
    console.error('[symbolMaster] Build futures symbol error:', error);
    return underlying;
  }
}

/**
 * Build unified options symbol
 */
export async function buildOptionsSymbol(
  underlying: string,
  expiry: string,
  strike: number,
  optionType: 'CE' | 'PE'
): Promise<string> {
  try {
    return await invoke<string>('symbol_master_build_options_symbol', {
      underlying,
      expiry,
      strike,
      optionType,
    });
  } catch (error) {
    console.error('[symbolMaster] Build options symbol error:', error);
    return underlying;
  }
}

/**
 * Format age in human-readable format
 */
function formatAge(seconds: number | null): string {
  if (seconds === null) return 'unknown';
  if (seconds < 60) return `${seconds}s`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h`;
  return `${Math.floor(seconds / 86400)}d`;
}

// ============================================================================
// BROKER-SPECIFIC HELPERS
// ============================================================================

/**
 * List of brokers that require authentication for master contract download
 */
export const BROKERS_REQUIRING_AUTH = ['zerodha'] as const;

/**
 * List of brokers with public master contract endpoints (no auth needed)
 */
export const BROKERS_WITH_PUBLIC_MASTER = [
  'fyers',
  'angelone',
  'angel',
  'upstox',
  'dhan',
  'shoonya',
  'finvasia',
] as const;

/**
 * Check if broker requires authentication for master contract download
 */
export function requiresAuthForDownload(brokerId: string): boolean {
  return BROKERS_REQUIRING_AUTH.includes(brokerId.toLowerCase() as any);
}

/**
 * Get list of supported brokers for master contract
 */
export function getSupportedBrokers(): string[] {
  return [
    'fyers',
    'zerodha',
    'angelone',
    'upstox',
    'dhan',
    'shoonya',
  ];
}
