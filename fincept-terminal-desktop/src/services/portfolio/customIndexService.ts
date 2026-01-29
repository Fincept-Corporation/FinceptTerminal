/**
 * Custom Index Service - Fincept Terminal
 * Handles aggregate index creation, calculation, and management
 */

import { invoke } from '@tauri-apps/api/core';
import { marketDataService, QuoteData } from '../markets/marketDataService';
import {
  CustomIndex,
  IndexConstituent,
  IndexConstituentConfig,
  IndexSnapshot,
  IndexSummary,
  IndexCalculationMethod,
  INDEX_METHOD_INFO,
} from '../../components/tabs/portfolio-tab/custom-index/types';

// ==================== INDEX SERVICE CLASS ====================

class CustomIndexService {
  private isInitialized = false;

  /**
   * Initialize the service
   */
  async initialize(): Promise<void> {
    if (this.isInitialized) return;
    this.isInitialized = true;
    console.log('[CustomIndexService] Initialized');
  }

  // ==================== INDEX CRUD ====================

  /**
   * Create a new custom index
   */
  async createIndex(
    name: string,
    description: string | undefined,
    calculationMethod: IndexCalculationMethod,
    baseValue: number = 100,
    capWeight: number | undefined,
    currency: string = 'USD',
    portfolioId?: string
  ): Promise<CustomIndex> {
    console.log(`[CustomIndexService] Creating index: ${name}`);

    const index = await invoke<CustomIndex>('custom_index_create', {
      name,
      description,
      calculationMethod,
      baseValue,
      capWeight: capWeight && capWeight > 0 ? capWeight : null,
      currency,
      portfolioId,
    });

    return index;
  }

  /**
   * Get all active custom indices
   */
  async getAllIndices(): Promise<CustomIndex[]> {
    console.log('[CustomIndexService] Fetching all indices');
    return invoke<CustomIndex[]>('custom_index_get_all');
  }

  /**
   * Get a specific index by ID
   */
  async getIndex(indexId: string): Promise<CustomIndex | null> {
    console.log(`[CustomIndexService] Fetching index: ${indexId}`);
    return invoke<CustomIndex | null>('custom_index_get_by_id', { indexId });
  }

  /**
   * Update index values (after recalculation)
   */
  async updateIndexValues(
    indexId: string,
    currentValue: number,
    previousClose: number,
    divisor: number
  ): Promise<void> {
    await invoke('custom_index_update', {
      indexId,
      currentValue,
      previousClose,
      divisor,
    });
  }

  /**
   * Delete an index (soft delete)
   */
  async deleteIndex(indexId: string): Promise<void> {
    console.log(`[CustomIndexService] Deleting index: ${indexId}`);
    await invoke('custom_index_delete', { indexId });
  }

  /**
   * Permanently delete an index
   */
  async hardDeleteIndex(indexId: string): Promise<void> {
    await invoke('custom_index_hard_delete', { indexId });
  }

  // ==================== CONSTITUENT MANAGEMENT ====================

  /**
   * Add a constituent to an index
   */
  async addConstituent(
    indexId: string,
    config: IndexConstituentConfig
  ): Promise<IndexConstituent> {
    console.log(`[CustomIndexService] Adding constituent ${config.symbol} to index ${indexId}`);

    return invoke<IndexConstituent>('index_constituent_add', {
      indexId,
      symbol: config.symbol.toUpperCase(),
      shares: config.shares,
      weight: config.weight,
      marketCap: config.marketCap,
      fundamentalScore: config.fundamentalScore,
      customPrice: config.customPrice,
      priceDate: config.priceDate,
    });
  }

  /**
   * Get all constituents for an index
   */
  async getConstituents(indexId: string): Promise<IndexConstituent[]> {
    return invoke<IndexConstituent[]>('index_constituent_get_all', { indexId });
  }

  /**
   * Update a constituent
   */
  async updateConstituent(
    constituentId: string,
    shares?: number,
    weight?: number,
    marketCap?: number,
    fundamentalScore?: number,
    customPrice?: number,
    priceDate?: string
  ): Promise<void> {
    await invoke('index_constituent_update', {
      constituentId,
      shares,
      weight,
      marketCap,
      fundamentalScore,
      customPrice,
      priceDate,
    });
  }

  /**
   * Get historical price for a symbol on a specific date
   */
  async getHistoricalPrice(symbol: string, date: string): Promise<number | null> {
    try {
      // Try to fetch via yfinance command
      const result = await invoke<{ close: number } | null>('get_historical_price', {
        symbol: symbol.toUpperCase(),
        date,
      });
      return result?.close || null;
    } catch {
      // Fallback: return null, user can enter manually
      return null;
    }
  }

  /**
   * Remove a constituent from an index
   */
  async removeConstituent(indexId: string, symbol: string): Promise<void> {
    await invoke('index_constituent_remove', { indexId, symbol: symbol.toUpperCase() });
  }

  // ==================== SNAPSHOT MANAGEMENT ====================

  /**
   * Save an index snapshot
   */
  async saveSnapshot(
    indexId: string,
    indexValue: number,
    dayChange: number,
    dayChangePercent: number,
    totalMarketValue: number,
    divisor: number,
    constituentsData: object,
    snapshotDate: string
  ): Promise<IndexSnapshot> {
    return invoke<IndexSnapshot>('index_snapshot_save', {
      indexId,
      indexValue,
      dayChange,
      dayChangePercent,
      totalMarketValue,
      divisor,
      constituentsData: JSON.stringify(constituentsData),
      snapshotDate,
    });
  }

  /**
   * Get snapshots for an index
   */
  async getSnapshots(indexId: string, limit?: number): Promise<IndexSnapshot[]> {
    return invoke<IndexSnapshot[]>('index_snapshot_get_all', { indexId, limit });
  }

  /**
   * Get the latest snapshot
   */
  async getLatestSnapshot(indexId: string): Promise<IndexSnapshot | null> {
    return invoke<IndexSnapshot | null>('index_snapshot_get_latest', { indexId });
  }

  /**
   * Cleanup old snapshots
   */
  async cleanupSnapshots(indexId: string, daysToKeep: number): Promise<number> {
    return invoke<number>('index_snapshot_cleanup', { indexId, days: daysToKeep });
  }

  // ==================== INDEX CALCULATION ====================

  /**
   * Calculate the current index value and summary
   */
  async calculateIndexSummary(indexId: string): Promise<IndexSummary> {
    console.log(`[CustomIndexService] Calculating summary for index: ${indexId}`);

    const index = await this.getIndex(indexId);
    if (!index) {
      throw new Error('Index not found');
    }

    const constituentsRaw = await this.getConstituents(indexId);
    if (constituentsRaw.length === 0) {
      return {
        index,
        constituents: [],
        total_market_value: 0,
        index_value: index.base_value,
        day_change: 0,
        day_change_percent: 0,
        last_updated: new Date().toISOString(),
      };
    }

    // Fetch current prices for all symbols
    const symbols = constituentsRaw.map(c => c.symbol);
    const quotes = await marketDataService.getQuotes(symbols);

    // Calculate constituent values with current prices
    const constituents = this.enrichConstituents(constituentsRaw, quotes, index);

    // Calculate index value based on method
    const { indexValue, totalMarketValue, divisor } = this.calculateIndexValue(
      index,
      constituents
    );

    // Calculate changes
    const dayChange = indexValue - index.previous_close;
    const dayChangePercent = index.previous_close > 0
      ? (dayChange / index.previous_close) * 100
      : 0;

    // Update index in database
    await this.updateIndexValues(indexId, indexValue, index.previous_close, divisor);

    return {
      index: { ...index, current_value: indexValue, divisor },
      constituents,
      total_market_value: totalMarketValue,
      index_value: indexValue,
      day_change: dayChange,
      day_change_percent: dayChangePercent,
      last_updated: new Date().toISOString(),
    };
  }

  /**
   * Enrich constituents with current market data
   */
  private enrichConstituents(
    constituents: IndexConstituent[],
    quotes: QuoteData[],
    index: CustomIndex
  ): IndexConstituent[] {
    const quoteMap = new Map(quotes.map(q => [q.symbol, q]));
    let totalMarketValue = 0;

    // First pass: calculate market values
    const enriched = constituents.map(c => {
      const quote = quoteMap.get(c.symbol);
      const currentPrice = quote?.price || 0;
      const previousClose = quote?.previous_close || currentPrice;
      const shares = c.shares || 1;
      const marketValue = currentPrice * shares;

      totalMarketValue += marketValue;

      return {
        ...c,
        current_price: currentPrice,
        previous_close: previousClose,
        market_value: marketValue,
        effective_weight: 0, // Will be calculated
        day_change: currentPrice - previousClose,
        day_change_percent: previousClose > 0 ? ((currentPrice - previousClose) / previousClose) * 100 : 0,
        contribution: 0, // Will be calculated
      } as IndexConstituent;
    });

    // Second pass: calculate weights and contributions
    return enriched.map(c => {
      const effectiveWeight = this.calculateEffectiveWeight(
        c,
        totalMarketValue,
        index.calculation_method,
        index.cap_weight,
        enriched
      );

      return {
        ...c,
        effective_weight: effectiveWeight,
        contribution: effectiveWeight * (c.day_change_percent / 100),
      };
    });
  }

  /**
   * Calculate effective weight based on method
   */
  private calculateEffectiveWeight(
    constituent: IndexConstituent,
    totalMarketValue: number,
    method: string,
    capWeight: number | undefined,
    allConstituents: IndexConstituent[]
  ): number {
    let weight = 0;
    const n = allConstituents.length;

    switch (method) {
      case 'price_weighted':
        const totalPrices = allConstituents.reduce((sum, c) => sum + c.current_price, 0);
        weight = totalPrices > 0 ? (constituent.current_price / totalPrices) * 100 : 0;
        break;

      case 'market_cap_weighted':
      case 'float_adjusted':
        weight = totalMarketValue > 0 ? (constituent.market_value / totalMarketValue) * 100 : 0;
        break;

      case 'equal_weighted':
        weight = n > 0 ? 100 / n : 0;
        break;

      case 'fundamental_weighted':
        const totalScore = allConstituents.reduce((sum, c) => sum + (c.fundamentalScore || 0), 0);
        weight = totalScore > 0 ? ((constituent.fundamentalScore || 0) / totalScore) * 100 : 100 / n;
        break;

      case 'capped_weighted':
        // First calculate market cap weight
        weight = totalMarketValue > 0 ? (constituent.market_value / totalMarketValue) * 100 : 0;
        // Then apply cap
        if (capWeight && capWeight > 0 && weight > capWeight) {
          weight = capWeight;
        }
        break;

      case 'risk_parity':
        // Using day change volatility as proxy for volatility
        const volatilities = allConstituents.map(c => Math.abs(c.day_change_percent) || 1);
        const inverseVols = volatilities.map(v => 1 / v);
        const totalInverseVol = inverseVols.reduce((sum, iv) => sum + iv, 0);
        const myInverseVol = 1 / (Math.abs(constituent.day_change_percent) || 1);
        weight = totalInverseVol > 0 ? (myInverseVol / totalInverseVol) * 100 : 100 / n;
        break;

      default:
        // Custom weight if provided, else equal weight
        weight = constituent.weight || (100 / n);
    }

    return weight;
  }

  /**
   * Calculate the index value based on the calculation method
   */
  private calculateIndexValue(
    index: CustomIndex,
    constituents: IndexConstituent[]
  ): { indexValue: number; totalMarketValue: number; divisor: number } {
    const method = index.calculation_method;
    const baseValue = index.base_value;
    let divisor = index.divisor;
    let indexValue = baseValue;
    let totalMarketValue = 0;

    if (constituents.length === 0) {
      return { indexValue: baseValue, totalMarketValue: 0, divisor };
    }

    switch (method) {
      case 'price_weighted': {
        // Dow Jones style: Sum of prices / divisor
        const sumPrices = constituents.reduce((sum, c) => sum + c.current_price, 0);

        // If this is the first calculation, set divisor to make index = base value
        if (divisor === 1 && sumPrices > 0) {
          divisor = sumPrices / baseValue;
        }

        indexValue = sumPrices / divisor;
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);
        break;
      }

      case 'market_cap_weighted':
      case 'float_adjusted':
      case 'capped_weighted':
      case 'modified_market_cap': {
        // S&P 500 style: Total market cap relative to base
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);

        // If this is the first calculation, set divisor to make index = base value
        if (divisor === 1 && totalMarketValue > 0) {
          divisor = totalMarketValue / baseValue;
        }

        indexValue = totalMarketValue / divisor;
        break;
      }

      case 'equal_weighted': {
        // Average of individual returns, then apply to base
        const avgReturn = constituents.reduce((sum, c) => sum + (c.day_change_percent / 100), 0) / constituents.length;

        // For first calculation, use price sum approach
        const sumPrices = constituents.reduce((sum, c) => sum + c.current_price, 0);
        if (divisor === 1 && sumPrices > 0) {
          divisor = sumPrices / baseValue;
        }

        indexValue = sumPrices / divisor;
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);
        break;
      }

      case 'geometric_mean': {
        // Geometric average of price relatives
        const priceRelatives = constituents.map(c =>
          c.previous_close > 0 ? c.current_price / c.previous_close : 1
        );
        const geometricMean = Math.pow(
          priceRelatives.reduce((prod, pr) => prod * pr, 1),
          1 / priceRelatives.length
        );

        indexValue = index.previous_close * geometricMean;
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);
        break;
      }

      case 'fundamental_weighted':
      case 'factor_weighted': {
        // Weighted by scores
        const totalScore = constituents.reduce((sum, c) => sum + (c.fundamentalScore || 1), 0);
        let weightedSum = 0;

        constituents.forEach(c => {
          const weight = (c.fundamentalScore || 1) / totalScore;
          weightedSum += weight * c.current_price;
        });

        if (divisor === 1 && weightedSum > 0) {
          divisor = weightedSum / baseValue;
        }

        indexValue = weightedSum / divisor;
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);
        break;
      }

      case 'risk_parity': {
        // Inverse volatility weighted
        const volatilities = constituents.map(c => Math.abs(c.day_change_percent) || 1);
        const inverseVols = volatilities.map(v => 1 / v);
        const totalInverseVol = inverseVols.reduce((sum, iv) => sum + iv, 0);

        let weightedSum = 0;
        constituents.forEach((c, i) => {
          const weight = inverseVols[i] / totalInverseVol;
          weightedSum += weight * c.current_price;
        });

        if (divisor === 1 && weightedSum > 0) {
          divisor = weightedSum / baseValue;
        }

        indexValue = weightedSum / divisor;
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);
        break;
      }

      default:
        // Default to price-weighted
        const sumPrices = constituents.reduce((sum, c) => sum + c.current_price, 0);
        if (divisor === 1 && sumPrices > 0) {
          divisor = sumPrices / baseValue;
        }
        indexValue = sumPrices / divisor;
        totalMarketValue = constituents.reduce((sum, c) => sum + c.market_value, 0);
    }

    return { indexValue, totalMarketValue, divisor };
  }

  // ==================== UTILITIES ====================

  /**
   * Get information about a calculation method
   */
  getMethodInfo(method: IndexCalculationMethod) {
    return INDEX_METHOD_INFO[method];
  }

  /**
   * Get all available calculation methods
   */
  getAllMethods(): { method: IndexCalculationMethod; info: typeof INDEX_METHOD_INFO[IndexCalculationMethod] }[] {
    return (Object.keys(INDEX_METHOD_INFO) as IndexCalculationMethod[]).map(method => ({
      method,
      info: INDEX_METHOD_INFO[method],
    }));
  }

  /**
   * Create index from portfolio holdings
   */
  async createIndexFromPortfolio(
    portfolioId: string,
    name: string,
    calculationMethod: IndexCalculationMethod,
    baseValue: number = 100,
    selectedSymbols?: string[]
  ): Promise<CustomIndex> {
    // Get portfolio assets
    const assets = await invoke<any[]>('portfolio_get_assets', { portfolioId });

    // Filter if specific symbols selected
    const filteredAssets = selectedSymbols
      ? assets.filter(a => selectedSymbols.includes(a.symbol))
      : assets;

    // Create the index
    const index = await this.createIndex(
      name,
      `Index created from portfolio`,
      calculationMethod,
      baseValue,
      undefined,
      'USD',
      portfolioId
    );

    // Add constituents
    for (const asset of filteredAssets) {
      await this.addConstituent(index.id, {
        symbol: asset.symbol,
        shares: asset.quantity,
        weight: undefined,
      });
    }

    return index;
  }

  /**
   * Save daily snapshot (call this at end of trading day)
   */
  async saveDailySnapshot(indexId: string): Promise<IndexSnapshot> {
    const summary = await this.calculateIndexSummary(indexId);

    const today = new Date().toISOString().split('T')[0];

    return this.saveSnapshot(
      indexId,
      summary.index_value,
      summary.day_change,
      summary.day_change_percent,
      summary.total_market_value,
      summary.index.divisor,
      summary.constituents,
      today
    );
  }

  /**
   * Rebase index (reset to new base value)
   */
  async rebaseIndex(indexId: string, newBaseValue: number): Promise<void> {
    const summary = await this.calculateIndexSummary(indexId);

    // Calculate new divisor to make current index = new base value
    const { totalMarketValue } = this.calculateIndexValue(
      summary.index,
      summary.constituents
    );

    const newDivisor = totalMarketValue / newBaseValue;

    await this.updateIndexValues(indexId, newBaseValue, newBaseValue, newDivisor);
  }
}

export const customIndexService = new CustomIndexService();
