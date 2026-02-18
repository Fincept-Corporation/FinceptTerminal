// Tradier Adapter — routes through Rust backend
import { invoke } from '@tauri-apps/api/core';
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class TradierAdapter extends BaseAdapter {
  private get token(): string { return this.getConfig<string>('accessToken') || ''; }
  private get isPaper(): boolean { return this.getConfig<boolean>('sandbox', false); }

  async testConnection(): Promise<TestConnectionResult> {
    try {
      const result: any = await invoke('tradier_get_profile', { token: this.token, isPaper: this.isPaper });
      if (result?.profile) {
        return this.createSuccessResult('Tradier connected via backend', result.profile);
      }
      return this.createErrorResult('Tradier profile fetch failed');
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(symbol: string, options?: { type?: string; interval?: string; start?: string; end?: string }): Promise<any> {
    const type = options?.type || 'quote';

    if (type === 'quote') {
      const result: any = await invoke('tradier_get_quotes', {
        token: this.token,
        symbols: symbol,
        isPaper: this.isPaper,
      });
      return result;
    }

    if (type === 'historical') {
      const result: any = await invoke('tradier_get_historical', {
        token: this.token,
        symbol,
        interval: options?.interval || 'daily',
        start: options?.start || new Date(Date.now() - 30 * 86400000).toISOString().slice(0, 10),
        end: options?.end || new Date().toISOString().slice(0, 10),
        isPaper: this.isPaper,
      });
      return result;
    }

    if (type === 'options') {
      const result: any = await invoke('tradier_get_option_expirations', {
        token: this.token,
        symbol,
        isPaper: this.isPaper,
      });
      return result;
    }

    if (type === 'search') {
      const result: any = await invoke('tradier_search_symbols', {
        token: this.token,
        query: symbol,
        isPaper: this.isPaper,
      });
      return result;
    }

    throw new Error(`TradierAdapter: unsupported query type "${type}"`);
  }

  async getMetadata(): Promise<Record<string, any>> {
    return { ...(await super.getMetadata()), provider: 'Tradier', backend: 'rust', isPaper: this.isPaper };
  }
}
