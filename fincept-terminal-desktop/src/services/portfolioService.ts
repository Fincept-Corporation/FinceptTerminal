// Portfolio Service - Comprehensive Portfolio Management for Fincept Terminal
// Handles portfolio creation, asset management, transactions, performance tracking, and analytics

import Database from '@tauri-apps/plugin-sql';
import { marketDataService, QuoteData } from './marketDataService';

// ==================== TYPES ====================

export interface Portfolio {
  id: string;
  name: string;
  owner: string;
  currency: string;
  description?: string;
  created_at: string;
  updated_at: string;
}

export interface PortfolioAsset {
  id: string;
  portfolio_id: string;
  symbol: string;
  quantity: number;
  avg_buy_price: number;
  first_purchase_date?: string;
  last_updated: string;
  notes?: string;
}

export interface Transaction {
  id: string;
  portfolio_id: string;
  symbol: string;
  transaction_type: 'BUY' | 'SELL';
  quantity: number;
  price: number;
  total_value: number;
  transaction_date: string;
  notes?: string;
  created_at: string;
}

export interface PortfolioSnapshot {
  id: string;
  portfolio_id: string;
  total_value: number;
  total_cost_basis: number;
  total_pnl: number;
  total_pnl_percent: number;
  snapshot_date: string;
  snapshot_data: string; // JSON of full portfolio state
}

export interface PortfolioHolding extends PortfolioAsset {
  current_price: number;
  market_value: number;
  cost_basis: number;
  unrealized_pnl: number;
  unrealized_pnl_percent: number;
  day_change: number;
  day_change_percent: number;
  weight: number;
}

export interface PortfolioSummary {
  portfolio: Portfolio;
  total_positions: number;
  total_market_value: number;
  total_cost_basis: number;
  total_unrealized_pnl: number;
  total_unrealized_pnl_percent: number;
  total_day_change: number;
  total_day_change_percent: number;
  holdings: PortfolioHolding[];
  last_updated: string;
}

export interface PortfolioPerformance {
  portfolio_id: string;
  ytd_return: number;
  one_month_return: number;
  three_month_return: number;
  one_year_return: number;
  sharpe_ratio: number;
  beta: number;
  max_drawdown: number;
  volatility: number;
  total_return: number;
}

// ==================== PORTFOLIO SERVICE CLASS ====================

class PortfolioService {
  private db: Database | null = null;
  private dbPath = 'sqlite:fincept_terminal.db';
  private isInitialized = false;

  /**
   * Initialize database connection
   */
  async initialize(): Promise<void> {
    if (this.isInitialized && this.db) {
      return;
    }

    try {
      this.db = await Database.load(this.dbPath);
      this.isInitialized = true;
      console.log('[PortfolioService] Initialized successfully');
    } catch (error) {
      console.error('[PortfolioService] Initialization failed:', error);
      throw error;
    }
  }

  /**
   * Ensure database is initialized
   */
  private ensureInitialized(): void {
    if (!this.db || !this.isInitialized) {
      throw new Error('PortfolioService not initialized. Call initialize() first.');
    }
  }

  // ==================== PORTFOLIO CRUD ====================

  /**
   * Create a new portfolio
   */
  async createPortfolio(name: string, owner: string, currency: string = 'USD', description?: string): Promise<Portfolio> {
    this.ensureInitialized();

    const id = crypto.randomUUID();
    await this.db!.execute(
      `INSERT INTO portfolios (id, name, owner, currency, description)
       VALUES ($1, $2, $3, $4, $5)`,
      [id, name, owner, currency, description || null]
    );

    const result = await this.db!.select<Portfolio[]>(
      'SELECT * FROM portfolios WHERE id = $1',
      [id]
    );

    console.log(`[PortfolioService] Created portfolio: ${name} (ID: ${id})`);
    return result[0];
  }

  /**
   * Get all portfolios
   */
  async getPortfolios(): Promise<Portfolio[]> {
    this.ensureInitialized();

    const portfolios = await this.db!.select<Portfolio[]>(
      'SELECT * FROM portfolios ORDER BY created_at DESC'
    );

    return portfolios;
  }

  /**
   * Get portfolio by ID
   */
  async getPortfolio(portfolioId: string): Promise<Portfolio | null> {
    this.ensureInitialized();

    const result = await this.db!.select<Portfolio[]>(
      'SELECT * FROM portfolios WHERE id = $1',
      [portfolioId]
    );

    return result.length > 0 ? result[0] : null;
  }

  /**
   * Update portfolio
   */
  async updatePortfolio(portfolioId: string, updates: Partial<Omit<Portfolio, 'id' | 'created_at' | 'updated_at'>>): Promise<void> {
    this.ensureInitialized();

    const sets: string[] = [];
    const values: any[] = [];

    if (updates.name !== undefined) {
      sets.push('name = $' + (values.length + 1));
      values.push(updates.name);
    }
    if (updates.owner !== undefined) {
      sets.push('owner = $' + (values.length + 1));
      values.push(updates.owner);
    }
    if (updates.currency !== undefined) {
      sets.push('currency = $' + (values.length + 1));
      values.push(updates.currency);
    }
    if (updates.description !== undefined) {
      sets.push('description = $' + (values.length + 1));
      values.push(updates.description);
    }

    if (sets.length > 0) {
      sets.push("updated_at = datetime('now')");
      values.push(portfolioId);

      await this.db!.execute(
        `UPDATE portfolios SET ${sets.join(', ')} WHERE id = $${values.length}`,
        values
      );
    }
  }

  /**
   * Delete portfolio (cascades to assets, transactions, and snapshots)
   */
  async deletePortfolio(portfolioId: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute('DELETE FROM portfolios WHERE id = $1', [portfolioId]);
    console.log(`[PortfolioService] Deleted portfolio: ${portfolioId}`);
  }

  // ==================== ASSET MANAGEMENT ====================

  /**
   * Add asset to portfolio
   */
  async addAsset(portfolioId: string, symbol: string, quantity: number, buyPrice: number, purchaseDate?: string): Promise<PortfolioAsset> {
    this.ensureInitialized();

    // Check if asset already exists
    const existing = await this.db!.select<PortfolioAsset[]>(
      'SELECT * FROM portfolio_assets WHERE portfolio_id = $1 AND symbol = $2',
      [portfolioId, symbol]
    );

    if (existing.length > 0) {
      // Update existing position (average down/up)
      const existingAsset = existing[0];
      const newQuantity = existingAsset.quantity + quantity;
      const newAvgPrice = ((existingAsset.avg_buy_price * existingAsset.quantity) + (buyPrice * quantity)) / newQuantity;

      await this.db!.execute(
        `UPDATE portfolio_assets
         SET quantity = $1, avg_buy_price = $2, last_updated = datetime('now')
         WHERE id = $3`,
        [newQuantity, newAvgPrice, existingAsset.id]
      );

      // Record transaction
      await this.addTransaction(portfolioId, symbol, 'BUY', quantity, buyPrice, purchaseDate);

      const result = await this.db!.select<PortfolioAsset[]>(
        'SELECT * FROM portfolio_assets WHERE id = $1',
        [existingAsset.id]
      );

      return result[0];
    } else {
      // Create new position
      const id = crypto.randomUUID();
      await this.db!.execute(
        `INSERT INTO portfolio_assets (id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date)
         VALUES ($1, $2, $3, $4, $5, $6)`,
        [id, portfolioId, symbol, quantity, buyPrice, purchaseDate || new Date().toISOString()]
      );

      // Record transaction
      await this.addTransaction(portfolioId, symbol, 'BUY', quantity, buyPrice, purchaseDate);

      const result = await this.db!.select<PortfolioAsset[]>(
        'SELECT * FROM portfolio_assets WHERE id = $1',
        [id]
      );

      console.log(`[PortfolioService] Added asset: ${symbol} to portfolio ${portfolioId}`);
      return result[0];
    }
  }

  /**
   * Remove asset from portfolio
   */
  async removeAsset(portfolioId: string, symbol: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute(
      'DELETE FROM portfolio_assets WHERE portfolio_id = $1 AND symbol = $2',
      [portfolioId, symbol]
    );

    console.log(`[PortfolioService] Removed asset: ${symbol} from portfolio ${portfolioId}`);
  }

  /**
   * Update asset (modify quantity and average price)
   */
  async updateAsset(portfolioId: string, symbol: string, quantity: number, avgBuyPrice: number): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute(
      `UPDATE portfolio_assets
       SET quantity = $1, avg_buy_price = $2, last_updated = datetime('now')
       WHERE portfolio_id = $3 AND symbol = $4`,
      [quantity, avgBuyPrice, portfolioId, symbol]
    );

    console.log(`[PortfolioService] Updated asset: ${symbol} in portfolio ${portfolioId}`);
  }

  /**
   * Sell asset (reduce quantity)
   */
  async sellAsset(portfolioId: string, symbol: string, quantity: number, sellPrice: number, transactionDate?: string): Promise<void> {
    this.ensureInitialized();

    const asset = await this.db!.select<PortfolioAsset[]>(
      'SELECT * FROM portfolio_assets WHERE portfolio_id = $1 AND symbol = $2',
      [portfolioId, symbol]
    );

    if (asset.length === 0) {
      throw new Error(`Asset ${symbol} not found in portfolio`);
    }

    const currentAsset = asset[0];

    if (quantity > currentAsset.quantity) {
      throw new Error(`Cannot sell ${quantity} shares of ${symbol}. Only ${currentAsset.quantity} available.`);
    }

    const newQuantity = currentAsset.quantity - quantity;

    if (newQuantity === 0) {
      // Remove asset completely
      await this.removeAsset(portfolioId, symbol);
    } else {
      // Update quantity
      await this.db!.execute(
        `UPDATE portfolio_assets
         SET quantity = $1, last_updated = datetime('now')
         WHERE portfolio_id = $2 AND symbol = $3`,
        [newQuantity, portfolioId, symbol]
      );
    }

    // Record sell transaction
    await this.addTransaction(portfolioId, symbol, 'SELL', quantity, sellPrice, transactionDate);

    console.log(`[PortfolioService] Sold ${quantity} shares of ${symbol} from portfolio ${portfolioId}`);
  }

  /**
   * Get all assets in a portfolio
   */
  async getPortfolioAssets(portfolioId: string): Promise<PortfolioAsset[]> {
    this.ensureInitialized();

    return await this.db!.select<PortfolioAsset[]>(
      'SELECT * FROM portfolio_assets WHERE portfolio_id = $1 ORDER BY symbol',
      [portfolioId]
    );
  }

  // ==================== TRANSACTION MANAGEMENT ====================

  /**
   * Add transaction
   */
  async addTransaction(
    portfolioId: string,
    symbol: string,
    type: 'BUY' | 'SELL',
    quantity: number,
    price: number,
    transactionDate?: string,
    notes?: string
  ): Promise<Transaction> {
    this.ensureInitialized();

    const id = crypto.randomUUID();
    const totalValue = quantity * price;
    const date = transactionDate || new Date().toISOString();

    await this.db!.execute(
      `INSERT INTO portfolio_transactions
       (id, portfolio_id, symbol, transaction_type, quantity, price, total_value, transaction_date, notes)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)`,
      [id, portfolioId, symbol, type, quantity, price, totalValue, date, notes || null]
    );

    const result = await this.db!.select<Transaction[]>(
      'SELECT * FROM portfolio_transactions WHERE id = $1',
      [id]
    );

    return result[0];
  }

  /**
   * Get all transactions for a portfolio
   */
  async getPortfolioTransactions(portfolioId: string, limit: number = 100): Promise<Transaction[]> {
    this.ensureInitialized();

    return await this.db!.select<Transaction[]>(
      'SELECT * FROM portfolio_transactions WHERE portfolio_id = $1 ORDER BY transaction_date DESC LIMIT $2',
      [portfolioId, limit]
    );
  }

  /**
   * Get transactions for a specific symbol
   */
  async getSymbolTransactions(portfolioId: string, symbol: string): Promise<Transaction[]> {
    this.ensureInitialized();

    return await this.db!.select<Transaction[]>(
      'SELECT * FROM portfolio_transactions WHERE portfolio_id = $1 AND symbol = $2 ORDER BY transaction_date DESC',
      [portfolioId, symbol]
    );
  }

  // ==================== PORTFOLIO VALUATION & HOLDINGS ====================

  /**
   * Fetch market prices for symbols
   */
  async fetchMarketPrices(symbols: string[]): Promise<Map<string, QuoteData>> {
    const quotes = await marketDataService.getQuotes(symbols);
    const priceMap = new Map<string, QuoteData>();

    quotes.forEach(quote => {
      priceMap.set(quote.symbol, quote);
    });

    return priceMap;
  }

  /**
   * Calculate portfolio value with current market prices
   */
  async calculatePortfolioValue(portfolioId: string): Promise<number> {
    this.ensureInitialized();

    const assets = await this.getPortfolioAssets(portfolioId);

    if (assets.length === 0) {
      return 0;
    }

    const symbols = assets.map(a => a.symbol);
    const prices = await this.fetchMarketPrices(symbols);

    let totalValue = 0;

    assets.forEach(asset => {
      const quote = prices.get(asset.symbol);
      if (quote) {
        totalValue += quote.price * asset.quantity;
      }
    });

    return totalValue;
  }

  /**
   * Get portfolio holdings with current market data
   */
  async getPortfolioHoldings(portfolioId: string): Promise<PortfolioHolding[]> {
    this.ensureInitialized();

    const assets = await this.getPortfolioAssets(portfolioId);

    if (assets.length === 0) {
      return [];
    }

    const symbols = assets.map(a => a.symbol);
    const prices = await this.fetchMarketPrices(symbols);

    const holdings: PortfolioHolding[] = [];
    let totalMarketValue = 0;

    // First pass: calculate total market value
    assets.forEach(asset => {
      const quote = prices.get(asset.symbol);
      if (quote) {
        totalMarketValue += quote.price * asset.quantity;
      }
    });

    // Second pass: create holdings with weights
    assets.forEach(asset => {
      const quote = prices.get(asset.symbol);

      if (quote) {
        const marketValue = quote.price * asset.quantity;
        const costBasis = asset.avg_buy_price * asset.quantity;
        const unrealizedPnL = marketValue - costBasis;
        const unrealizedPnLPercent = (unrealizedPnL / costBasis) * 100;
        const dayChange = quote.change * asset.quantity;
        const dayChangePercent = quote.change_percent;
        const weight = (marketValue / totalMarketValue) * 100;

        holdings.push({
          ...asset,
          current_price: quote.price,
          market_value: marketValue,
          cost_basis: costBasis,
          unrealized_pnl: unrealizedPnL,
          unrealized_pnl_percent: unrealizedPnLPercent,
          day_change: dayChange,
          day_change_percent: dayChangePercent,
          weight: weight
        });
      }
    });

    return holdings;
  }

  /**
   * Get complete portfolio summary
   */
  async getPortfolioSummary(portfolioId: string): Promise<PortfolioSummary> {
    this.ensureInitialized();

    const portfolio = await this.getPortfolio(portfolioId);
    if (!portfolio) {
      throw new Error(`Portfolio ${portfolioId} not found`);
    }

    const holdings = await this.getPortfolioHoldings(portfolioId);

    let totalMarketValue = 0;
    let totalCostBasis = 0;
    let totalUnrealizedPnL = 0;
    let totalDayChange = 0;

    holdings.forEach(holding => {
      totalMarketValue += holding.market_value;
      totalCostBasis += holding.cost_basis;
      totalUnrealizedPnL += holding.unrealized_pnl;
      totalDayChange += holding.day_change;
    });

    const totalUnrealizedPnLPercent = totalCostBasis > 0 ? (totalUnrealizedPnL / totalCostBasis) * 100 : 0;
    const totalDayChangePercent = (totalMarketValue - totalDayChange) > 0
      ? (totalDayChange / (totalMarketValue - totalDayChange)) * 100
      : 0;

    return {
      portfolio,
      total_positions: holdings.length,
      total_market_value: totalMarketValue,
      total_cost_basis: totalCostBasis,
      total_unrealized_pnl: totalUnrealizedPnL,
      total_unrealized_pnl_percent: totalUnrealizedPnLPercent,
      total_day_change: totalDayChange,
      total_day_change_percent: totalDayChangePercent,
      holdings,
      last_updated: new Date().toISOString()
    };
  }

  // ==================== RETURNS & PERFORMANCE ANALYTICS ====================

  /**
   * Calculate realized and unrealized P&L
   */
  async calculateReturns(portfolioId: string): Promise<{ realized_pnl: number; unrealized_pnl: number; total_pnl: number }> {
    this.ensureInitialized();

    const summary = await this.getPortfolioSummary(portfolioId);
    const unrealizedPnL = summary.total_unrealized_pnl;

    // Calculate realized P&L from transactions
    const transactions = await this.getPortfolioTransactions(portfolioId);
    let realizedPnL = 0;

    // Group transactions by symbol
    const symbolGroups = new Map<string, Transaction[]>();
    transactions.forEach(txn => {
      if (!symbolGroups.has(txn.symbol)) {
        symbolGroups.set(txn.symbol, []);
      }
      symbolGroups.get(txn.symbol)!.push(txn);
    });

    // Calculate realized gains/losses using FIFO
    symbolGroups.forEach((txns, symbol) => {
      const buys = txns.filter(t => t.transaction_type === 'BUY').reverse();
      const sells = txns.filter(t => t.transaction_type === 'SELL').reverse();

      sells.forEach(sell => {
        let remainingQuantity = sell.quantity;

        for (const buy of buys) {
          if (remainingQuantity <= 0) break;

          const quantityToMatch = Math.min(remainingQuantity, buy.quantity);
          const costBasis = buy.price * quantityToMatch;
          const proceeds = sell.price * quantityToMatch;
          realizedPnL += (proceeds - costBasis);

          remainingQuantity -= quantityToMatch;
        }
      });
    });

    return {
      realized_pnl: realizedPnL,
      unrealized_pnl: unrealizedPnL,
      total_pnl: realizedPnL + unrealizedPnL
    };
  }

  // ==================== SNAPSHOTS & HISTORY ====================

  /**
   * Save portfolio snapshot
   */
  async saveSnapshot(portfolioId: string): Promise<PortfolioSnapshot> {
    this.ensureInitialized();

    const summary = await this.getPortfolioSummary(portfolioId);

    const id = crypto.randomUUID();
    const snapshotData = JSON.stringify({
      holdings: summary.holdings,
      total_positions: summary.total_positions
    });

    await this.db!.execute(
      `INSERT INTO portfolio_snapshots
       (id, portfolio_id, total_value, total_cost_basis, total_pnl, total_pnl_percent, snapshot_data)
       VALUES ($1, $2, $3, $4, $5, $6, $7)`,
      [
        id,
        portfolioId,
        summary.total_market_value,
        summary.total_cost_basis,
        summary.total_unrealized_pnl,
        summary.total_unrealized_pnl_percent,
        snapshotData
      ]
    );

    const result = await this.db!.select<PortfolioSnapshot[]>(
      'SELECT * FROM portfolio_snapshots WHERE id = $1',
      [id]
    );

    console.log(`[PortfolioService] Saved snapshot for portfolio ${portfolioId}`);
    return result[0];
  }

  /**
   * Get portfolio performance history
   */
  async getPortfolioPerformanceHistory(portfolioId: string, limit: number = 30): Promise<PortfolioSnapshot[]> {
    this.ensureInitialized();

    return await this.db!.select<PortfolioSnapshot[]>(
      'SELECT * FROM portfolio_snapshots WHERE portfolio_id = $1 ORDER BY snapshot_date DESC LIMIT $2',
      [portfolioId, limit]
    );
  }

  // ==================== REBALANCING ====================

  /**
   * Rebalance portfolio to target weights
   * Strategy: 'equal' - equal weight all positions, or custom weight map
   */
  async rebalancePortfolio(portfolioId: string, strategy: 'equal' | Map<string, number>): Promise<Map<string, { action: 'BUY' | 'SELL'; quantity: number; reason: string }>> {
    this.ensureInitialized();

    const holdings = await this.getPortfolioHoldings(portfolioId);
    const totalValue = holdings.reduce((sum, h) => sum + h.market_value, 0);

    const rebalanceActions = new Map<string, { action: 'BUY' | 'SELL'; quantity: number; reason: string }>();

    if (strategy === 'equal') {
      const targetWeight = 100 / holdings.length;

      holdings.forEach(holding => {
        const currentWeight = holding.weight;
        const diff = targetWeight - currentWeight;

        if (Math.abs(diff) > 0.5) { // 0.5% threshold
          const targetValue = (targetWeight / 100) * totalValue;
          const currentValue = holding.market_value;
          const valueDiff = targetValue - currentValue;
          const quantityDiff = valueDiff / holding.current_price;

          rebalanceActions.set(holding.symbol, {
            action: quantityDiff > 0 ? 'BUY' : 'SELL',
            quantity: Math.abs(quantityDiff),
            reason: `Rebalance from ${currentWeight.toFixed(2)}% to ${targetWeight.toFixed(2)}%`
          });
        }
      });
    } else {
      // Custom weight map
      holdings.forEach(holding => {
        const targetWeight = strategy.get(holding.symbol) || 0;
        const currentWeight = holding.weight;
        const diff = targetWeight - currentWeight;

        if (Math.abs(diff) > 0.5) {
          const targetValue = (targetWeight / 100) * totalValue;
          const currentValue = holding.market_value;
          const valueDiff = targetValue - currentValue;
          const quantityDiff = valueDiff / holding.current_price;

          rebalanceActions.set(holding.symbol, {
            action: quantityDiff > 0 ? 'BUY' : 'SELL',
            quantity: Math.abs(quantityDiff),
            reason: `Rebalance from ${currentWeight.toFixed(2)}% to ${targetWeight.toFixed(2)}%`
          });
        }
      });
    }

    return rebalanceActions;
  }

  // ==================== EXPORT ====================

  /**
   * Export portfolio data as CSV
   */
  async exportPortfolioCSV(portfolioId: string): Promise<string> {
    const summary = await this.getPortfolioSummary(portfolioId);

    let csv = 'Symbol,Quantity,Avg Buy Price,Current Price,Market Value,Cost Basis,Unrealized P&L,Unrealized P&L %,Day Change,Day Change %,Weight %\n';

    summary.holdings.forEach(holding => {
      csv += `${holding.symbol},${holding.quantity},${holding.avg_buy_price.toFixed(2)},${holding.current_price.toFixed(2)},${holding.market_value.toFixed(2)},${holding.cost_basis.toFixed(2)},${holding.unrealized_pnl.toFixed(2)},${holding.unrealized_pnl_percent.toFixed(2)},${holding.day_change.toFixed(2)},${holding.day_change_percent.toFixed(2)},${holding.weight.toFixed(2)}\n`;
    });

    csv += `\nTOTAL,,,,${summary.total_market_value.toFixed(2)},${summary.total_cost_basis.toFixed(2)},${summary.total_unrealized_pnl.toFixed(2)},${summary.total_unrealized_pnl_percent.toFixed(2)},${summary.total_day_change.toFixed(2)},${summary.total_day_change_percent.toFixed(2)},100.00\n`;

    return csv;
  }

  /**
   * Export portfolio transactions as CSV
   */
  async exportTransactionsCSV(portfolioId: string): Promise<string> {
    const transactions = await this.getPortfolioTransactions(portfolioId, 1000);

    let csv = 'Date,Symbol,Type,Quantity,Price,Total Value,Notes\n';

    transactions.forEach(txn => {
      csv += `${txn.transaction_date},${txn.symbol},${txn.transaction_type},${txn.quantity},${txn.price.toFixed(2)},${txn.total_value.toFixed(2)},"${txn.notes || ''}"\n`;
    });

    return csv;
  }

  // ==================== UTILITY ====================

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return this.isInitialized && this.db !== null;
  }

  /**
   * Get service status
   */
  getStatus(): { initialized: boolean; ready: boolean } {
    return {
      initialized: this.isInitialized,
      ready: this.isReady()
    };
  }

  // ==================== PORTFOLIO ANALYTICS (Python-based) ====================

  /**
   * Calculate advanced portfolio metrics using Python analytics
   */
  async calculateAdvancedMetrics(portfolioId: string, riskFreeRate: number = 0.03): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const holdings = await this.getPortfolioHoldings(portfolioId);

    if (holdings.length === 0) {
      throw new Error('Portfolio has no holdings to analyze');
    }

    // Get historical returns for each asset (simplified - would need real data)
    const returnsData: {[key: string]: number[]} = {};

    for (const holding of holdings) {
      // TODO: Fetch actual historical returns
      // For now, generate sample data
      returnsData[holding.symbol] = Array.from({length: 252}, () => (Math.random() - 0.5) * 0.02);
    }

    const weights = holdings.map(h => h.weight / 100);

    const result = await invoke<string>('calculate_portfolio_metrics', {
      returnsData: JSON.stringify(returnsData),
      weights: JSON.stringify(weights),
      riskFreeRate
    });

    return JSON.parse(result);
  }

  /**
   * Optimize portfolio weights using Python optimization
   */
  async optimizePortfolioWeights(portfolioId: string, method: 'max_sharpe' | 'min_volatility' = 'max_sharpe', riskFreeRate: number = 0.03): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const holdings = await this.getPortfolioHoldings(portfolioId);

    if (holdings.length < 2) {
      throw new Error('Need at least 2 assets to optimize portfolio');
    }

    const returnsData: {[key: string]: number[]} = {};

    for (const holding of holdings) {
      // TODO: Fetch actual historical returns
      returnsData[holding.symbol] = Array.from({length: 252}, () => (Math.random() - 0.5) * 0.02);
    }

    const result = await invoke<string>('optimize_portfolio', {
      returnsData: JSON.stringify(returnsData),
      method,
      riskFreeRate
    });

    return JSON.parse(result);
  }

  /**
   * Generate efficient frontier for portfolio
   */
  async generateEfficientFrontier(portfolioId: string, numPoints: number = 50, riskFreeRate: number = 0.03): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const holdings = await this.getPortfolioHoldings(portfolioId);

    const returnsData: {[key: string]: number[]} = {};

    for (const holding of holdings) {
      // TODO: Fetch actual historical returns
      returnsData[holding.symbol] = Array.from({length: 252}, () => (Math.random() - 0.5) * 0.02);
    }

    const result = await invoke<string>('generate_efficient_frontier', {
      returnsData: JSON.stringify(returnsData),
      numPoints,
      riskFreeRate
    });

    return JSON.parse(result);
  }

  /**
   * Get comprehensive portfolio analytics overview
   */
  async getAnalyticsOverview(portfolioId: string): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const holdings = await this.getPortfolioHoldings(portfolioId);

    const returnsData: {[key: string]: number[]} = {};

    for (const holding of holdings) {
      // TODO: Fetch actual historical returns
      returnsData[holding.symbol] = Array.from({length: 252}, () => (Math.random() - 0.5) * 0.02);
    }

    const weights = holdings.map(h => h.weight / 100);

    const result = await invoke<string>('get_portfolio_overview', {
      returnsData: JSON.stringify(returnsData),
      weights: JSON.stringify(weights)
    });

    return JSON.parse(result);
  }

  /**
   * Calculate comprehensive risk metrics (VaR, CVaR, volatility, etc.)
   */
  async calculateRiskMetrics(portfolioId: string): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const holdings = await this.getPortfolioHoldings(portfolioId);

    if (holdings.length === 0) {
      throw new Error('Portfolio has no holdings to analyze');
    }

    const returnsData: {[key: string]: number[]} = {};

    for (const holding of holdings) {
      returnsData[holding.symbol] = Array.from({length: 252}, () => (Math.random() - 0.5) * 0.02);
    }

    const weights = holdings.map(h => h.weight / 100);

    const result = await invoke<string>('calculate_risk_metrics', {
      returnsData: JSON.stringify(returnsData),
      weights: JSON.stringify(weights)
    });

    return JSON.parse(result);
  }

  /**
   * Generate strategic asset allocation plan based on age and risk tolerance
   */
  async generateAssetAllocation(age: number, riskTolerance: 'conservative' | 'moderate' | 'aggressive', yearsToRetirement: number): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const result = await invoke<string>('generate_asset_allocation', {
      age,
      riskTolerance,
      yearsToRetirement
    });

    return JSON.parse(result);
  }

  /**
   * Calculate retirement planning projections
   */
  async calculateRetirementPlan(currentAge: number, retirementAge: number, currentSavings: number, annualContribution: number): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const result = await invoke<string>('calculate_retirement_plan', {
      currentAge,
      retirementAge,
      currentSavings,
      annualContribution
    });

    return JSON.parse(result);
  }

  /**
   * Analyze behavioral finance biases in portfolio
   */
  async analyzeBehavioralBiases(portfolioId: string): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const holdings = await this.getPortfolioHoldings(portfolioId);

    const portfolioData: any = {
      weights: holdings.map(h => h.weight / 100),
      symbols: holdings.map(h => h.symbol),
      returns: holdings.map(h => h.unrealized_pnl_percent / 100)
    };

    const result = await invoke<string>('analyze_behavioral_biases', {
      portfolioData: JSON.stringify(portfolioData)
    });

    return JSON.parse(result);
  }

  /**
   * Analyze ETF costs and efficiency
   */
  async analyzeEtfCosts(symbols: string[], expenseRatios: number[]): Promise<any> {
    const { invoke } = await import('@tauri-apps/api/core');

    const result = await invoke<string>('analyze_etf_costs', {
      symbols: JSON.stringify(symbols),
      expenseRatios: JSON.stringify(expenseRatios)
    });

    return JSON.parse(result);
  }
}

// Export singleton instance
export const portfolioService = new PortfolioService();
export default portfolioService;
