/**
 * Derivatives Adapter
 *
 * Futures, options, leverage, and position management methods
 * Extends TradingAdapter with derivatives functionality
 */

import { TradingAdapter } from './TradingAdapter';

export abstract class DerivativesAdapter extends TradingAdapter {
  // ============================================================================
  // OPTIONS TRADING
  // ============================================================================

  async fetchGreeks(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchGreeks']) {
        throw new Error('fetchGreeks not supported by this exchange');
      }
      return await (this.exchange as any).fetchGreeks(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOption(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchOption']) {
        throw new Error('fetchOption not supported by this exchange');
      }
      return await (this.exchange as any).fetchOption(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOptionChain(underlyingSymbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchOptionChain']) {
        const markets = await this.fetchMarkets();
        return markets.filter((m: any) =>
          m.option && m.base === underlyingSymbol.split('/')[0]
        );
      }
      return await (this.exchange as any).fetchOptionChain(underlyingSymbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchVolatilityHistory(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchVolatilityHistory']) {
        throw new Error('fetchVolatilityHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchVolatilityHistory(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARGIN & BORROW RATES
  // ============================================================================

  async fetchBorrowRate(code: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchCrossBorrowRate']) {
        return await (this.exchange as any).fetchCrossBorrowRate(code, params);
      }
      if (this.exchange.has['fetchBorrowRate']) {
        return await (this.exchange as any).fetchBorrowRate(code, params);
      }
      throw new Error('fetchBorrowRate not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchBorrowRates(params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchCrossBorrowRates']) {
        return await (this.exchange as any).fetchCrossBorrowRates(params);
      }
      if (this.exchange.has['fetchBorrowRates']) {
        return await (this.exchange as any).fetchBorrowRates(params);
      }
      throw new Error('fetchBorrowRates not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchBorrowInterest(code?: string, symbol?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchBorrowInterest']) {
        throw new Error('fetchBorrowInterest not supported by this exchange');
      }
      return await (this.exchange as any).fetchBorrowInterest(code, symbol, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchCrossBorrowRate(code: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchCrossBorrowRate']) {
        return await this.fetchBorrowRate(code, params);
      }
      return await (this.exchange as any).fetchCrossBorrowRate(code, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchIsolatedBorrowRate(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchIsolatedBorrowRate']) {
        throw new Error('fetchIsolatedBorrowRate not supported by this exchange');
      }
      return await (this.exchange as any).fetchIsolatedBorrowRate(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchBorrowRateHistory(code: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchBorrowRateHistory']) {
        throw new Error('fetchBorrowRateHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchBorrowRateHistory(code, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // LEVERAGE INFO
  // ============================================================================

  async fetchLeverage(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchLeverage']) {
        return await (this.exchange as any).fetchLeverage(symbol, params);
      }
      const positions = await this.fetchPositions([symbol]);
      if (positions.length > 0) {
        return { symbol, leverage: positions[0].leverage };
      }
      throw new Error('fetchLeverage not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchLeverages(symbols?: string[], params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchLeverages']) {
        return await (this.exchange as any).fetchLeverages(symbols, params);
      }
      if (symbols) {
        const results: Record<string, any> = {};
        for (const symbol of symbols) {
          try {
            results[symbol] = await this.fetchLeverage(symbol, params);
          } catch {
            results[symbol] = null;
          }
        }
        return results;
      }
      throw new Error('fetchLeverages not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchLeverageTiers(symbols?: string[], params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchLeverageTiers']) {
        throw new Error('fetchLeverageTiers not supported by this exchange');
      }
      return await (this.exchange as any).fetchLeverageTiers(symbols, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMarketLeverageTiers(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (this.exchange.has['fetchMarketLeverageTiers']) {
        return await (this.exchange as any).fetchMarketLeverageTiers(symbol, params);
      }
      const tiers = await this.fetchLeverageTiers([symbol], params);
      return tiers[symbol] || null;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // POSITION MANAGEMENT
  // ============================================================================

  async fetchPosition(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchPosition']) {
        return await (this.exchange as any).fetchPosition(symbol, params);
      }
      const positions = await this.fetchPositions([symbol]);
      return positions.find((p: any) => p.symbol === symbol) || null;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchPositionMode(symbol?: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchPositionMode']) {
        throw new Error('fetchPositionMode not supported by this exchange');
      }
      return await (this.exchange as any).fetchPositionMode(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchPositionHistory(symbol?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchPositionHistory']) {
        return await (this.exchange as any).fetchPositionHistory(symbol, since, limit, params);
      }
      if (this.exchange.has['fetchPositionsHistory']) {
        return await (this.exchange as any).fetchPositionsHistory([symbol], since, limit, params);
      }
      throw new Error('fetchPositionHistory not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async closePosition(symbol: string, side?: 'long' | 'short', params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['closePosition']) {
        return await (this.exchange as any).closePosition(symbol, side, params);
      }
      const position = await this.fetchPosition(symbol);
      if (!position || position.contracts === 0) {
        return { info: 'No position to close' };
      }
      const closeSide = position.side === 'long' ? 'sell' : 'buy';
      const amount = Math.abs(position.contracts || position.contractSize || 0);
      return await this.createOrder(symbol, 'market', closeSide as any, amount, undefined, {
        reduceOnly: true,
        ...params
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async closeAllPositions(params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['closeAllPositions']) {
        return await (this.exchange as any).closeAllPositions(params);
      }
      const positions = await this.fetchPositions();
      const results = [];
      for (const position of positions) {
        if (position.contracts && position.contracts !== 0) {
          const result = await this.closePosition(position.symbol);
          results.push(result);
        }
      }
      return results;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING RATES
  // ============================================================================

  async fetchFundingRate(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchFundingRate']) {
        throw new Error('fetchFundingRate not supported by this exchange');
      }
      return await this.exchange.fetchFundingRate(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchFundingRates(symbols?: string[], params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (this.exchange.has['fetchFundingRates']) {
        return await (this.exchange as any).fetchFundingRates(symbols, params);
      }
      if (symbols) {
        const results: Record<string, any> = {};
        for (const symbol of symbols) {
          try {
            results[symbol] = await this.fetchFundingRate(symbol, params);
          } catch {
            results[symbol] = null;
          }
        }
        return results;
      }
      throw new Error('fetchFundingRates not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchFundingRateHistory(symbol: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchFundingRateHistory']) {
        throw new Error('fetchFundingRateHistory not supported by this exchange');
      }
      return await this.exchange.fetchFundingRateHistory(symbol, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // OPEN INTEREST
  // ============================================================================

  async fetchOpenInterest(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchOpenInterest']) {
        throw new Error('fetchOpenInterest not supported by this exchange');
      }
      return await (this.exchange as any).fetchOpenInterest(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOpenInterestHistory(symbol: string, timeframe?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchOpenInterestHistory']) {
        throw new Error('fetchOpenInterestHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchOpenInterestHistory(symbol, timeframe, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARK & INDEX PRICES
  // ============================================================================

  async fetchMarkPrice(symbol: string, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchMarkPrice']) {
        throw new Error('fetchMarkPrice not supported by this exchange');
      }
      return await (this.exchange as any).fetchMarkPrice(symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMarkPrices(symbols?: string[], params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (this.exchange.has['fetchMarkPrices']) {
        return await (this.exchange as any).fetchMarkPrices(symbols, params);
      }
      if (symbols) {
        const results: Record<string, any> = {};
        for (const symbol of symbols) {
          try {
            results[symbol] = await this.fetchMarkPrice(symbol, params);
          } catch {
            results[symbol] = null;
          }
        }
        return results;
      }
      throw new Error('fetchMarkPrices not supported by this exchange');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchIndexOHLCV(symbol: string, timeframe?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchIndexOHLCV']) {
        throw new Error('fetchIndexOHLCV not supported by this exchange');
      }
      return await (this.exchange as any).fetchIndexOHLCV(symbol, timeframe, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMarkOHLCV(symbol: string, timeframe?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchMarkOHLCV']) {
        throw new Error('fetchMarkOHLCV not supported by this exchange');
      }
      return await (this.exchange as any).fetchMarkOHLCV(symbol, timeframe, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchPremiumIndexOHLCV(symbol: string, timeframe?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchPremiumIndexOHLCV']) {
        throw new Error('fetchPremiumIndexOHLCV not supported by this exchange');
      }
      return await (this.exchange as any).fetchPremiumIndexOHLCV(symbol, timeframe, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // LIQUIDATIONS
  // ============================================================================

  async fetchLiquidations(symbol: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchLiquidations']) {
        throw new Error('fetchLiquidations not supported by this exchange');
      }
      return await (this.exchange as any).fetchLiquidations(symbol, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMyLiquidations(symbol?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchMyLiquidations']) {
        throw new Error('fetchMyLiquidations not supported by this exchange');
      }
      return await (this.exchange as any).fetchMyLiquidations(symbol, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // SETTLEMENTS
  // ============================================================================

  async fetchSettlementHistory(symbol?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchSettlementHistory']) {
        throw new Error('fetchSettlementHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchSettlementHistory(symbol, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMySettlementHistory(symbol?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchMySettlementHistory']) {
        throw new Error('fetchMySettlementHistory not supported by this exchange');
      }
      return await (this.exchange as any).fetchMySettlementHistory(symbol, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
