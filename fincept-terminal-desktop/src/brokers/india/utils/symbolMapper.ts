/**
 * Symbol mapping utilities for Indian brokers
 * Handles conversion between standardized symbols and broker-specific formats
 */

import type { IndianExchange, InstrumentType } from '../types';
import { INDEX_SYMBOL_MAP } from '../constants';

/**
 * Parse expiry date from various formats to standardized DD-MMM-YY format
 */
export function parseExpiry(expiry: string | Date | null | undefined): string {
  if (!expiry) return '';

  try {
    let date: Date;

    if (expiry instanceof Date) {
      date = expiry;
    } else if (typeof expiry === 'string') {
      // Handle formats like "2024-01-25", "25-JAN-24", "25JAN24"
      if (expiry.includes('-') && expiry.length === 10) {
        // ISO format: 2024-01-25
        date = new Date(expiry);
      } else if (expiry.includes('-')) {
        // DD-MMM-YY format
        return expiry.toUpperCase();
      } else {
        // Try to parse as date string
        date = new Date(expiry);
      }
    } else {
      return '';
    }

    if (isNaN(date.getTime())) return '';

    const months = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN',
                    'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC'];
    const day = String(date.getDate()).padStart(2, '0');
    const month = months[date.getMonth()];
    const year = String(date.getFullYear()).slice(-2);

    return `${day}-${month}-${year}`;
  } catch {
    return '';
  }
}

/**
 * Format expiry for symbol construction (removes dashes)
 */
export function formatExpiryForSymbol(expiry: string): string {
  return expiry.replace(/-/g, '');
}

/**
 * Format strike price (remove decimals if whole number)
 */
export function formatStrike(strike: number | string): string {
  const num = typeof strike === 'string' ? parseFloat(strike) : strike;
  return Number.isInteger(num) ? String(Math.floor(num)) : String(num);
}

/**
 * Standardize index symbol names
 */
export function standardizeIndexSymbol(symbol: string): string {
  return INDEX_SYMBOL_MAP[symbol] || symbol;
}

/**
 * Build standardized options symbol
 * Format: {UNDERLYING}{EXPIRY}{STRIKE}{OPTIONTYPE}
 * Example: NIFTY25JAN2450000CE
 */
export function buildOptionsSymbol(
  underlying: string,
  expiry: string,
  strike: number,
  optionType: 'CE' | 'PE'
): string {
  const formattedExpiry = formatExpiryForSymbol(parseExpiry(expiry));
  const formattedStrike = formatStrike(strike);
  return `${underlying}${formattedExpiry}${formattedStrike}${optionType}`;
}

/**
 * Build standardized futures symbol
 * Format: {UNDERLYING}{EXPIRY}FUT
 * Example: NIFTY25JAN25FUT
 */
export function buildFuturesSymbol(underlying: string, expiry: string): string {
  const formattedExpiry = formatExpiryForSymbol(parseExpiry(expiry));
  return `${underlying}${formattedExpiry}FUT`;
}

/**
 * Parse symbol to extract components
 */
export interface ParsedSymbol {
  underlying: string;
  expiry?: string;
  strike?: number;
  optionType?: 'CE' | 'PE';
  instrumentType: InstrumentType;
}

export function parseSymbol(symbol: string): ParsedSymbol {
  // Check for options (ends with CE or PE)
  const optionsMatch = symbol.match(/^(.+?)(\d{2}[A-Z]{3}\d{2})(\d+)(CE|PE)$/);
  if (optionsMatch) {
    return {
      underlying: optionsMatch[1],
      expiry: optionsMatch[2],
      strike: parseInt(optionsMatch[3], 10),
      optionType: optionsMatch[4] as 'CE' | 'PE',
      instrumentType: optionsMatch[4] as 'CE' | 'PE',
    };
  }

  // Check for futures (ends with FUT)
  const futuresMatch = symbol.match(/^(.+?)(\d{2}[A-Z]{3}\d{2})FUT$/);
  if (futuresMatch) {
    return {
      underlying: futuresMatch[1],
      expiry: futuresMatch[2],
      instrumentType: 'FUT',
    };
  }

  // Equity symbol
  return {
    underlying: symbol,
    instrumentType: 'EQ',
  };
}

/**
 * Get the exchange for derivatives based on underlying
 */
export function getDerivativesExchange(underlying: string): IndianExchange {
  // Index derivatives are on NFO
  const indexUnderlyings = ['NIFTY', 'BANKNIFTY', 'FINNIFTY', 'MIDCPNIFTY'];
  if (indexUnderlyings.includes(underlying)) {
    return 'NFO';
  }

  // BSE indices
  if (['SENSEX', 'BANKEX', 'SENSEX50'].includes(underlying)) {
    return 'BFO';
  }

  // Commodities
  if (['GOLD', 'SILVER', 'CRUDEOIL', 'NATURALGAS', 'COPPER'].includes(underlying)) {
    return 'MCX';
  }

  // Currency
  if (['USDINR', 'EURINR', 'GBPINR', 'JPYINR'].includes(underlying)) {
    return 'CDS';
  }

  // Default to NFO for stock derivatives
  return 'NFO';
}

/**
 * Check if exchange is an index exchange
 */
export function isIndexExchange(exchange: IndianExchange): boolean {
  return ['NSE_INDEX', 'BSE_INDEX', 'MCX_INDEX', 'CDS_INDEX'].includes(exchange);
}

/**
 * Get the base exchange for an index exchange
 */
export function getBaseExchange(exchange: IndianExchange): IndianExchange {
  const mapping: Partial<Record<IndianExchange, IndianExchange>> = {
    NSE_INDEX: 'NSE',
    BSE_INDEX: 'BSE',
    MCX_INDEX: 'MCX',
    CDS_INDEX: 'CDS',
  };
  return mapping[exchange] || exchange;
}

/**
 * Convert exchange code for different broker formats
 */
export function mapExchangeCode(
  exchange: IndianExchange,
  brokerFormat: 'standard' | 'zerodha' | 'angel' | 'dhan'
): string {
  // Most brokers use the same codes
  if (brokerFormat === 'standard' || brokerFormat === 'zerodha') {
    return exchange;
  }

  // Angel uses different codes for some exchanges
  if (brokerFormat === 'angel') {
    const angelMapping: Partial<Record<IndianExchange, string>> = {
      NSE: 'NSE',
      BSE: 'BSE',
      NFO: 'NFO',
      MCX: 'MCX',
      CDS: 'CDS',
      NSE_INDEX: 'NSE',
      BSE_INDEX: 'BSE',
    };
    return angelMapping[exchange] || exchange;
  }

  // Dhan uses segment codes
  if (brokerFormat === 'dhan') {
    const dhanMapping: Partial<Record<IndianExchange, string>> = {
      NSE: 'NSE_EQ',
      BSE: 'BSE_EQ',
      NFO: 'NSE_FNO',
      BFO: 'BSE_FNO',
      MCX: 'MCX_COMM',
      CDS: 'NSE_CURRENCY',
    };
    return dhanMapping[exchange] || exchange;
  }

  return exchange;
}

/**
 * Validate symbol format
 */
export function isValidSymbol(symbol: string): boolean {
  if (!symbol || typeof symbol !== 'string') return false;

  // Basic validation: alphanumeric, can contain & and -
  return /^[A-Z0-9&\-]+$/i.test(symbol);
}

/**
 * Normalize symbol (uppercase, trim)
 */
export function normalizeSymbol(symbol: string): string {
  return symbol.toUpperCase().trim();
}
