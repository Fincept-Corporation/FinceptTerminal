// Master Contract Service - Symbol Search & Management
// Provides access to broker symbol data for fast search and offline access
// Updated to use unified symbol_master commands

import { invoke } from '@tauri-apps/api/core';
import * as symbolMaster from '@/services/symbolMasterService';

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
  status: string;
  symbol_count: number;
  last_updated: number;
  is_ready: boolean;
}

/**
 * Supported broker IDs for master contract
 */
export type SupportedBroker = 'angelone' | 'fyers' | 'zerodha' | 'upstox' | 'dhan' | 'shoonya';

// ============================================================================
// Master Contract Download
// ============================================================================

/**
 * Download master contract for a specific broker
 * Downloads symbol data from broker API and stores in local database
 */
export const downloadMasterContract = async (brokerId: SupportedBroker): Promise<DownloadResult> => {
  const result = await symbolMaster.downloadMasterContract(brokerId);
  return {
    success: result.success,
    message: result.message,
    symbol_count: Number(result.total_symbols)
  };
};

/**
 * Download Angel One master contract
 */
export const downloadAngelOneMasterContract = async (): Promise<DownloadResult> => {
  return await downloadMasterContract('angelone');
};

/**
 * Download Fyers master contract
 */
export const downloadFyersMasterContract = async (): Promise<DownloadResult> => {
  return await downloadMasterContract('fyers');
};

// ============================================================================
// Symbol Search
// ============================================================================

/**
 * Search symbols in master contract database
 */
export const searchSymbols = async (
  brokerId: SupportedBroker,
  query: string,
  exchange?: string,
  instrumentType?: string,
  limit: number = 50
): Promise<SymbolSearchResult[]> => {
  const response = await symbolMaster.searchSymbols(brokerId, query, {
    exchange,
    instrumentType,
    limit
  });

  if (!response.success) {
    return [];
  }

  return response.symbols.map(s => ({
    symbol: s.symbol,
    br_symbol: s.br_symbol,
    name: s.name,
    exchange: s.exchange,
    token: s.token,
    expiry: s.expiry,
    strike: s.strike,
    lot_size: s.lot_size,
    instrument_type: s.instrument_type,
    tick_size: s.tick_size
  }));
};

/**
 * Get symbol by exact match
 */
export const getSymbol = async (
  brokerId: SupportedBroker,
  symbol: string,
  exchange: string
): Promise<SymbolSearchResult | null> => {
  return await symbolMaster.getSymbol(brokerId, symbol, exchange);
};

/**
 * Get symbol by broker token
 */
export const getSymbolByToken = async (
  brokerId: SupportedBroker,
  token: string
): Promise<SymbolSearchResult | null> => {
  return await symbolMaster.getSymbolByToken(brokerId, token);
};

// ============================================================================
// Metadata Queries
// ============================================================================

/**
 * Get total symbol count for a broker
 */
export const getSymbolCount = async (brokerId: SupportedBroker): Promise<number> => {
  return await symbolMaster.getSymbolCount(brokerId);
};

/**
 * Get list of exchanges for a broker
 */
export const getExchanges = async (brokerId: SupportedBroker): Promise<string[]> => {
  return await symbolMaster.getExchanges(brokerId);
};

/**
 * Get list of expiries for derivatives
 */
export const getExpiries = async (
  brokerId: SupportedBroker,
  exchange?: string,
  underlying?: string
): Promise<string[]> => {
  return await symbolMaster.getExpiries(brokerId, { exchange, underlying });
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
  const status = await symbolMaster.getStatus(brokerId);
  if (!status) return null;

  return {
    broker_id: status.broker_id,
    status: status.status,
    symbol_count: Number(status.total_symbols),
    last_updated: status.last_updated,
    is_ready: status.is_ready
  };
};

/**
 * Check if cache is valid (not expired)
 */
export const isCacheValid = async (
  brokerId: SupportedBroker,
  _ttlSeconds: number = 86400
): Promise<boolean> => {
  const response = await symbolMaster.isReady(brokerId);
  return response.is_valid;
};

/**
 * Get cache age in seconds
 */
export const getCacheAge = async (brokerId: SupportedBroker): Promise<number | null> => {
  const response = await symbolMaster.isReady(brokerId);
  return response.age_seconds;
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
    zerodha: 'Zerodha',
    upstox: 'Upstox',
    dhan: 'Dhan',
    shoonya: 'Shoonya'
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
    MCX_FO: 'MCX F&O',
    NSE_INDEX: 'NSE Index',
    BSE_INDEX: 'BSE Index'
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
    FUT: 'Futures',
    INDEX: 'Index'
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
    const result = await symbolMaster.ensureMasterContract(brokerId);
    return {
      success: result.success,
      message: result.message,
      symbol_count: Number(result.total_symbols)
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
