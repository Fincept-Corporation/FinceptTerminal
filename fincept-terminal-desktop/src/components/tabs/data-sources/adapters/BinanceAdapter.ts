// Binance Adapter — routes through Rust WebSocket/REST backend
import { invoke } from '@tauri-apps/api/core';
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class BinanceAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      // Use the Rust WebSocket config check as a health test
      const result: any = await invoke('ws_get_config');
      return this.createSuccessResult('Binance backend available', { config: result });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(symbol: string, options?: { type?: string; interval?: string; limit?: number }): Promise<any> {
    const type = options?.type || 'quote';

    if (type === 'quote' || type === 'ticker') {
      // Rust WebSocket provides real-time ticker — fall back to yfinance for REST quote
      const result: any = await invoke('get_market_quote', { symbol });
      if (!result?.success) throw new Error(result?.error || `Binance quote failed for ${symbol}`);
      return result.data;
    }

    if (type === 'orderbook') {
      const result: any = await invoke('ws_get_orderbook', { symbol, exchange: 'binance' });
      return result;
    }

    if (type === 'historical') {
      const result: any = await invoke('get_historical_data', {
        symbol,
        startDate: new Date(Date.now() - 30 * 86400000).toISOString().slice(0, 10),
        endDate: new Date().toISOString().slice(0, 10),
      });
      if (!result?.success) throw new Error(result?.error || `Binance historical failed for ${symbol}`);
      return result.data;
    }

    throw new Error(`BinanceAdapter: unsupported query type "${type}"`);
  }

  async getMetadata(): Promise<Record<string, any>> {
    return { ...(await super.getMetadata()), provider: 'Binance', backend: 'rust/websocket' };
  }
}
