// Alpha Vantage Adapter — routes through Rust/Python backend
import { invoke } from '@tauri-apps/api/core';
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class AlphaVantageAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const result: any = await invoke('get_alphavantage_quote', { symbol: 'IBM' });
      if (result?.success) {
        return this.createSuccessResult('Alpha Vantage connected via backend', result.data);
      }
      return this.createErrorResult(result?.error || 'Alpha Vantage test failed');
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(symbol: string, options?: { type?: string; interval?: string; outputsize?: string }): Promise<any> {
    const type = options?.type || 'quote';

    if (type === 'quote') {
      const result: any = await invoke('get_alphavantage_quote', { symbol });
      if (!result?.success) throw new Error(result?.error || `Alpha Vantage quote failed for ${symbol}`);
      return result.data;
    }

    if (type === 'historical') {
      const interval = options?.interval || 'daily';
      const result: any = interval === 'daily'
        ? await invoke('get_alphavantage_daily', { symbol, outputsize: options?.outputsize || 'compact' })
        : await invoke('get_alphavantage_intraday', { symbol, interval });
      if (!result?.success) throw new Error(result?.error || `Alpha Vantage historical failed for ${symbol}`);
      return result.data;
    }

    if (type === 'fundamentals' || type === 'overview') {
      const result: any = await invoke('get_alphavantage_overview', { symbol });
      if (!result?.success) throw new Error(result?.error || `Alpha Vantage overview failed for ${symbol}`);
      return result.data;
    }

    if (type === 'search') {
      const result: any = await invoke('search_alphavantage_symbols', { keywords: symbol });
      if (!result?.success) throw new Error(result?.error || 'Alpha Vantage search failed');
      return result.data;
    }

    if (type === 'movers') {
      const result: any = await invoke('get_alphavantage_market_movers');
      if (!result?.success) throw new Error(result?.error || 'Alpha Vantage movers failed');
      return result.data;
    }

    throw new Error(`AlphaVantageAdapter: unsupported query type "${type}"`);
  }

  async getMetadata(): Promise<Record<string, any>> {
    return { ...(await super.getMetadata()), provider: 'Alpha Vantage', backend: 'rust/python' };
  }
}
