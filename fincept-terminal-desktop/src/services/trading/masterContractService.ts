// Master Contract Service - Symbol Search & Management
// Provides access to broker symbol data for fast search and offline access

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// Types
// ============================================================================

/**
 * Symbol search result from master contract database
 */
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
}

/**
 * Master contract download result
 */
export interface DownloadResult {
  success: boolean;
  message: string;
  symbol_count: number;
}

/**
 * Master contract cache metadata
 */
export interface MasterContractCache {
  broker_id: string;
  symbols_data: string;
  symbol_count: number;
  last_updated: number;
  created_at: number;
}

/**
 * Supported broker IDs for master contract
 */
export type SupportedBroker = 'angelone' | 'fyers' | 'zerodha';

// ============================================================================
// Master Contract Download
// ============================================================================

/**
 * Download master contract for a specific broker
 * Downloads symbol data from broker API and stores in local database
 */
export const downloadMasterContract = async (brokerId: SupportedBroker): Promise<DownloadResult> => {
  return await invoke<DownloadResult>('download_master_contract', { brokerId });
};

/**
 * Download Angel One master contract
 * Downloads from https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json
 */
export const downloadAngelOneMasterContract = async (): Promise<DownloadResult> => {
  return await invoke<DownloadResult>('download_angelone_master_contract');
};

/**
 * Download Fyers master contract
 * Downloads from https://public.fyers.in/sym_details/*.csv
 */
export const downloadFyersMasterContract = async (): Promise<DownloadResult> => {
  return await invoke<DownloadResult>('download_fyers_master_contract');
};

// ============================================================================
// Symbol Search
// ============================================================================

/**
 * Search symbols in master contract database
 * @param brokerId - Broker to search symbols for
 * @param query - Search query (symbol, name, or token)
 * @param exchange - Optional exchange filter (e.g., "NSE", "BSE", "NFO")
 * @param instrumentType - Optional instrument type filter (e.g., "EQ", "OPTIDX", "FUTSTK")
 * @param limit - Maximum number of results (default: 50)
 * @returns Array of matching symbols ordered by relevance
 */
export const searchSymbols = async (
  brokerId: SupportedBroker,
  query: string,
  exchange?: string,
  instrumentType?: string,
  limit: number = 50
): Promise<SymbolSearchResult[]> => {
  return await invoke<SymbolSearchResult[]>('search_symbols', {
    brokerId,
    query,
    exchange: exchange || null,
    instrumentType: instrumentType || null,
    limit
  });
};

/**
 * Get symbol by exact match
 * @param brokerId - Broker ID
 * @param symbol - Exact symbol name
 * @param exchange - Exchange name
 */
export const getSymbol = async (
  brokerId: SupportedBroker,
  symbol: string,
  exchange: string
): Promise<SymbolSearchResult | null> => {
  return await invoke<SymbolSearchResult | null>('get_symbol', {
    brokerId,
    symbol,
    exchange
  });
};

/**
 * Get symbol by broker token
 * @param brokerId - Broker ID
 * @param token - Broker-specific token
 */
export const getSymbolByToken = async (
  brokerId: SupportedBroker,
  token: string
): Promise<SymbolSearchResult | null> => {
  return await invoke<SymbolSearchResult | null>('get_symbol_by_token', {
    brokerId,
    token
  });
};

// ============================================================================
// Metadata Queries
// ============================================================================

/**
 * Get total symbol count for a broker
 */
export const getSymbolCount = async (brokerId: SupportedBroker): Promise<number> => {
  return await invoke<number>('get_symbol_count', { brokerId });
};

/**
 * Get list of exchanges for a broker
 */
export const getExchanges = async (brokerId: SupportedBroker): Promise<string[]> => {
  return await invoke<string[]>('get_exchanges', { brokerId });
};

/**
 * Get list of expiries for derivatives
 * @param brokerId - Broker ID
 * @param exchange - Optional exchange filter
 * @param underlying - Optional underlying symbol filter
 */
export const getExpiries = async (
  brokerId: SupportedBroker,
  exchange?: string,
  underlying?: string
): Promise<string[]> => {
  return await invoke<string[]>('get_expiries', {
    brokerId,
    exchange: exchange || null,
    underlying: underlying || null
  });
};

// ============================================================================
// Cache Management
// ============================================================================

/**
 * Check if master contract data is available for a broker
 */
export const hasSymbolData = async (brokerId: SupportedBroker): Promise<boolean> => {
  try {
    const count = await getSymbolCount(brokerId);
    return count > 0;
  } catch {
    return false;
  }
};

/**
 * Get master contract cache metadata
 */
export const getMasterContractCache = async (brokerId: SupportedBroker): Promise<MasterContractCache | null> => {
  return await invoke<MasterContractCache | null>('get_master_contract', { brokerId });
};

/**
 * Check if cache is valid (not expired)
 * @param brokerId - Broker ID
 * @param ttlSeconds - Time to live in seconds (default: 24 hours)
 */
export const isCacheValid = async (
  brokerId: SupportedBroker,
  ttlSeconds: number = 86400
): Promise<boolean> => {
  return await invoke<boolean>('is_cache_valid', { brokerId, ttlSeconds });
};

/**
 * Get cache age in seconds
 */
export const getCacheAge = async (brokerId: SupportedBroker): Promise<number | null> => {
  return await invoke<number | null>('get_cache_age', { brokerId });
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Format symbol for display
 */
export const formatSymbolDisplay = (result: SymbolSearchResult): string => {
  const parts = [result.symbol];
  if (result.exchange) {
    parts.push(`(${result.exchange})`);
  }
  if (result.expiry) {
    parts.push(result.expiry);
  }
  if (result.strike) {
    parts.push(`${result.strike}`);
  }
  return parts.join(' ');
};

/**
 * Get broker-specific display name
 */
export const getBrokerDisplayName = (brokerId: SupportedBroker): string => {
  const names: Record<SupportedBroker, string> = {
    angelone: 'Angel One',
    fyers: 'Fyers',
    zerodha: 'Zerodha'
  };
  return names[brokerId] || brokerId;
};

/**
 * Get exchange display name
 */
export const getExchangeDisplayName = (exchange: string): string => {
  const names: Record<string, string> = {
    NSE: 'NSE (Equity)',
    BSE: 'BSE (Equity)',
    NFO: 'NSE F&O',
    BFO: 'BSE F&O',
    MCX: 'MCX (Commodity)',
    CDS: 'Currency Derivatives',
    NSE_CM: 'NSE Cash',
    NSE_FO: 'NSE F&O',
    BSE_CM: 'BSE Cash',
    BSE_FO: 'BSE F&O',
    MCX_FO: 'MCX F&O'
  };
  return names[exchange] || exchange;
};

/**
 * Map instrument type to display name
 */
export const getInstrumentTypeDisplayName = (instrumentType: string | null): string => {
  if (!instrumentType) return 'Unknown';

  const names: Record<string, string> = {
    EQ: 'Equity',
    EQUITY: 'Equity',
    FUTSTK: 'Stock Futures',
    FUTIDX: 'Index Futures',
    OPTSTK: 'Stock Options',
    OPTIDX: 'Index Options',
    FUTCOM: 'Commodity Futures',
    OPTCOM: 'Commodity Options',
    FUTCUR: 'Currency Futures',
    OPTCUR: 'Currency Options',
    CE: 'Call Option',
    PE: 'Put Option',
    FUT: 'Futures'
  };
  return names[instrumentType.toUpperCase()] || instrumentType;
};

// ============================================================================
// Service Class (for compatibility with existing patterns)
// ============================================================================

class MasterContractService {
  // Download
  downloadMasterContract = downloadMasterContract;
  downloadAngelOneMasterContract = downloadAngelOneMasterContract;
  downloadFyersMasterContract = downloadFyersMasterContract;

  // Search
  searchSymbols = searchSymbols;
  getSymbol = getSymbol;
  getSymbolByToken = getSymbolByToken;

  // Metadata
  getSymbolCount = getSymbolCount;
  getExchanges = getExchanges;
  getExpiries = getExpiries;

  // Cache
  hasSymbolData = hasSymbolData;
  getMasterContractCache = getMasterContractCache;
  isCacheValid = isCacheValid;
  getCacheAge = getCacheAge;

  // Utils
  formatSymbolDisplay = formatSymbolDisplay;
  getBrokerDisplayName = getBrokerDisplayName;
  getExchangeDisplayName = getExchangeDisplayName;
  getInstrumentTypeDisplayName = getInstrumentTypeDisplayName;

  /**
   * Initialize master contract for a broker
   * Downloads if not available or cache is expired
   */
  async initializeBroker(brokerId: SupportedBroker): Promise<DownloadResult> {
    const hasData = await this.hasSymbolData(brokerId);
    const cacheValid = await this.isCacheValid(brokerId);

    if (!hasData || !cacheValid) {
      console.log(`[MasterContractService] Downloading master contract for ${brokerId}...`);
      return await this.downloadMasterContract(brokerId);
    }

    const count = await this.getSymbolCount(brokerId);
    return {
      success: true,
      message: 'Cache is valid',
      symbol_count: count
    };
  }

  /**
   * Search with debounce-friendly interface
   * Returns empty array on error instead of throwing
   */
  async safeSearch(
    brokerId: SupportedBroker,
    query: string,
    options?: {
      exchange?: string;
      instrumentType?: string;
      limit?: number;
    }
  ): Promise<SymbolSearchResult[]> {
    if (!query || query.length < 1) {
      return [];
    }

    try {
      return await this.searchSymbols(
        brokerId,
        query,
        options?.exchange,
        options?.instrumentType,
        options?.limit || 50
      );
    } catch (error) {
      console.error('[MasterContractService] Search error:', error);
      return [];
    }
  }
}

export const masterContractService = new MasterContractService();
export default masterContractService;
