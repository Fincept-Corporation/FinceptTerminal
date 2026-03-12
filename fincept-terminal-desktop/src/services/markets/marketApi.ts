// Market Data API Service - Direct HTTP calls to Fincept API
// Endpoints: /market/*
// Complements marketDataService.ts (which uses Tauri invoke/Rust bridge)

import { fetch as tauriFetch } from '@tauri-apps/plugin-http';

const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://api.fincept.in',
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;
const safeFetch = import.meta.env.DEV ? window.fetch.bind(window) : tauriFetch;

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  status_code: number;
}

async function makeApiRequest<T = any>(
  endpoint: string,
  apiKey?: string
): Promise<ApiResponse<T>> {
  try {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
    };
    if (apiKey) headers['X-API-Key'] = apiKey;

    const response = await safeFetch(getApiEndpoint(endpoint), { method: 'GET', headers });
    const data = await response.json();

    return {
      success: response.ok,
      data,
      error: response.ok ? undefined : data.detail || data.error || 'Request failed',
      status_code: response.status,
    };
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
      status_code: 500,
    };
  }
}

export class MarketApiService {
  // Search tickers
  static async searchTickers(
    q: string,
    apiKey?: string,
    options?: { limit?: number; type?: string; exchange?: string; country?: string }
  ): Promise<ApiResponse> {
    const params = new URLSearchParams({ q });
    if (options?.limit) params.append('limit', String(options.limit));
    if (options?.type) params.append('type', options.type);
    if (options?.exchange) params.append('exchange', options.exchange);
    if (options?.country) params.append('country', options.country);
    return makeApiRequest(`/market/search?${params}`, apiKey);
  }

  // Get ticker price
  static async getTickerPrice(symbol: string, apiKey?: string, exchange?: string): Promise<ApiResponse> {
    const params = exchange ? `?exchange=${encodeURIComponent(exchange)}` : '';
    return makeApiRequest(`/market/price/${encodeURIComponent(symbol)}${params}`, apiKey);
  }

  // Get ticker info
  static async getTickerInfo(symbol: string, apiKey?: string, exchange?: string): Promise<ApiResponse> {
    const params = exchange ? `?exchange=${encodeURIComponent(exchange)}` : '';
    return makeApiRequest(`/market/info/${encodeURIComponent(symbol)}${params}`, apiKey);
  }

  // Get ticker snapshot
  static async getTickerSnapshot(symbol: string, apiKey?: string, exchange?: string): Promise<ApiResponse> {
    const params = exchange ? `?exchange=${encodeURIComponent(exchange)}` : '';
    return makeApiRequest(`/market/snapshot/${encodeURIComponent(symbol)}${params}`, apiKey);
  }

  // Get ticker history
  static async getTickerHistory(
    symbol: string,
    apiKey?: string,
    options?: { interval?: string; period?: string; start_date?: string; end_date?: string; date?: string; exchange?: string }
  ): Promise<ApiResponse> {
    const params = new URLSearchParams();
    if (options?.interval) params.append('interval', options.interval);
    if (options?.period) params.append('period', options.period);
    if (options?.start_date) params.append('start_date', options.start_date);
    if (options?.end_date) params.append('end_date', options.end_date);
    if (options?.date) params.append('date', options.date);
    if (options?.exchange) params.append('exchange', options.exchange);
    const query = params.toString();
    return makeApiRequest(`/market/history/${encodeURIComponent(symbol)}${query ? `?${query}` : ''}`, apiKey);
  }

  // Get ticker financials
  static async getTickerFinancials(
    symbol: string,
    apiKey?: string,
    options?: { quarterly?: boolean; exchange?: string }
  ): Promise<ApiResponse> {
    const params = new URLSearchParams();
    if (options?.quarterly !== undefined) params.append('quarterly', String(options.quarterly));
    if (options?.exchange) params.append('exchange', options.exchange);
    const query = params.toString();
    return makeApiRequest(`/market/financials/${encodeURIComponent(symbol)}${query ? `?${query}` : ''}`, apiKey);
  }

  // Get ticker news
  static async getTickerNews(symbol: string, apiKey?: string, maxResults?: number): Promise<ApiResponse> {
    const params = maxResults ? `?max_results=${maxResults}` : '';
    return makeApiRequest(`/market/news/${encodeURIComponent(symbol)}${params}`, apiKey);
  }

  // Batch prices for multiple symbols
  static async getBatchPrices(symbols: string[], apiKey?: string, exchange?: string): Promise<ApiResponse> {
    const params = new URLSearchParams({ symbols: symbols.join(',') });
    if (exchange) params.append('exchange', exchange);
    return makeApiRequest(`/market/batch/prices?${params}`, apiKey);
  }

  // Batch history for multiple symbols
  static async getBatchHistory(
    symbols: string[],
    apiKey?: string,
    options?: { period?: string; interval?: string; exchange?: string }
  ): Promise<ApiResponse> {
    const params = new URLSearchParams({ symbols: symbols.join(',') });
    if (options?.period) params.append('period', options.period);
    if (options?.interval) params.append('interval', options.interval);
    if (options?.exchange) params.append('exchange', options.exchange);
    return makeApiRequest(`/market/batch/history?${params}`, apiKey);
  }
}
