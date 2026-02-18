// CoinGecko Adapter — routes through Rust/Python backend
import { invoke } from '@tauri-apps/api/core';
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CoinGeckoAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const result: any = await invoke('get_coingecko_trending');
      if (result?.success) {
        return this.createSuccessResult('CoinGecko connected via backend', result.data);
      }
      return this.createErrorResult(result?.error || 'CoinGecko test failed');
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(coinIdOrQuery: string, options?: { type?: string; vsCurrency?: string; days?: number }): Promise<any> {
    const type = options?.type || 'price';
    const vsCurrency = options?.vsCurrency || 'usd';

    if (type === 'price' || type === 'quote') {
      const result: any = await invoke('get_coingecko_price', { coinId: coinIdOrQuery, vsCurrency });
      if (!result?.success) throw new Error(result?.error || `CoinGecko price failed for ${coinIdOrQuery}`);
      return result.data;
    }

    if (type === 'market') {
      const result: any = await invoke('get_coingecko_market_data', { coinId: coinIdOrQuery });
      if (!result?.success) throw new Error(result?.error || `CoinGecko market data failed for ${coinIdOrQuery}`);
      return result.data;
    }

    if (type === 'historical') {
      const result: any = await invoke('get_coingecko_historical', {
        coinId: coinIdOrQuery,
        vsCurrency,
        days: options?.days ?? 7,
      });
      if (!result?.success) throw new Error(result?.error || `CoinGecko historical failed for ${coinIdOrQuery}`);
      return result.data;
    }

    if (type === 'search') {
      const result: any = await invoke('search_coingecko_coins', { query: coinIdOrQuery });
      if (!result?.success) throw new Error(result?.error || 'CoinGecko search failed');
      return result.data;
    }

    if (type === 'trending') {
      const result: any = await invoke('get_coingecko_trending');
      if (!result?.success) throw new Error(result?.error || 'CoinGecko trending failed');
      return result.data;
    }

    if (type === 'global') {
      const result: any = await invoke('get_coingecko_global_data');
      if (!result?.success) throw new Error(result?.error || 'CoinGecko global data failed');
      return result.data;
    }

    if (type === 'top') {
      const result: any = await invoke('get_coingecko_top_coins', { vsCurrency, limit: options?.days ?? 100 });
      if (!result?.success) throw new Error(result?.error || 'CoinGecko top coins failed');
      return result.data;
    }

    throw new Error(`CoinGeckoAdapter: unsupported query type "${type}"`);
  }

  async getMetadata(): Promise<Record<string, any>> {
    return { ...(await super.getMetadata()), provider: 'CoinGecko', backend: 'rust/python' };
  }
}
