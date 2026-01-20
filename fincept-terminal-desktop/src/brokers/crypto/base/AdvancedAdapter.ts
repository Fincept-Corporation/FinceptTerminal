/**
 * Advanced Adapter
 *
 * Crypto convert, sentiment, and other advanced trading methods
 * Extends FundingAdapter - this is the final adapter in the chain
 *
 * Implements IExchangeAdapter interface (all required methods inherited from chain)
 */

import { FundingAdapter } from './FundingAdapter';
import { IExchangeAdapter } from '../types';

export abstract class AdvancedAdapter extends FundingAdapter implements IExchangeAdapter {
  // ============================================================================
  // CRYPTO CONVERT (SWAP)
  // ============================================================================

  async fetchConvertCurrencies(params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchConvertCurrencies']) {
        throw new Error('fetchConvertCurrencies not supported by this exchange');
      }
      return await (this.exchange as any).fetchConvertCurrencies(params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchConvertQuote(fromCurrency: string, toCurrency: string, amount: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchConvertQuote']) {
        throw new Error('fetchConvertQuote not supported by this exchange');
      }
      return await (this.exchange as any).fetchConvertQuote(fromCurrency, toCurrency, amount, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createConvertTrade(fromCurrency: string, toCurrency: string, amount: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createConvertTrade']) {
        throw new Error('createConvertTrade not supported by this exchange');
      }
      return await (this.exchange as any).createConvertTrade(fromCurrency, toCurrency, amount, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchConvertTradeHistory(code?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchConvertTradeHistory']) {
        throw new Error('fetchConvertTradeHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchConvertTradeHistory(code, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARKET SENTIMENT
  // ============================================================================

  async fetchLongShortRatioHistory(symbol: string, timeframe?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchLongShortRatioHistory']) {
        throw new Error('fetchLongShortRatioHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchLongShortRatioHistory(symbol, timeframe, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
