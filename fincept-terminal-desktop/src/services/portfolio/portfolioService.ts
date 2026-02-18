// Portfolio Service - Comprehensive Portfolio Management for Fincept Terminal
// Handles portfolio creation, asset management, transactions, performance tracking, and analytics
// Using Rust SQLite backend via Tauri commands

import { invoke } from '@tauri-apps/api/core';
import { marketDataService, QuoteData } from '../markets/marketDataService';
import { portfolioLogger } from '../core/loggerService';

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
  first_purchase_date: string;
  last_updated: string;
}

export interface Transaction {
  id: string;
  portfolio_id: string;
  symbol: string;
  transaction_type: 'BUY' | 'SELL' | 'DIVIDEND' | 'SPLIT';
  quantity: number;
  price: number;
  total_value: number;
  transaction_date: string;
  notes?: string;
}

export interface PortfolioSnapshot {
  id: string;
  portfolio_id: string;
  total_value: number;
  total_cost_basis: number;
  total_pnl: number;
  total_pnl_percent: number;
  snapshot_data: string;
  snapshot_date: string;
}

export interface HoldingWithQuote extends PortfolioAsset {
  current_price: number;
  market_value: number;
  cost_basis: number;
  unrealized_pnl: number;
  unrealized_pnl_percent: number;
  day_change: number;
  day_change_percent: number;
  weight: number; // Portfolio weight percentage
}

// Alias for compatibility
export type PortfolioHolding = HoldingWithQuote;

export interface PortfolioSummary {
  portfolio: Portfolio;
  holdings: HoldingWithQuote[];
  total_market_value: number;
  total_cost_basis: number;
  total_unrealized_pnl: number;
  total_unrealized_pnl_percent: number;
  total_positions: number;
  total_day_change: number;
  total_day_change_percent: number;
  last_updated: string; // Timestamp of last update
}

// ==================== PORTFOLIO SERVICE CLASS ====================

class PortfolioService {
  private isInitialized = false;
  private symbolsMigrated = false;

  /**
   * Initialize database connection
   * Using Rust SQLite backend - no initialization needed
   */
  async initialize(): Promise<void> {
    if (this.isInitialized) return;
    this.isInitialized = true;
    portfolioLogger.info('Using Rust SQLite backend - no initialization needed');

    // Run one-time symbol migration (resolves bare symbols like "PIDILITIND" → "PIDILITIND.NS")
    if (!this.symbolsMigrated) {
      this.symbolsMigrated = true;
      this.migrateSymbols().catch(err =>
        portfolioLogger.warn(`Symbol migration skipped: ${err}`)
      );
    }
  }

  /**
   * One-time migration: resolve bare symbols to yfinance-compatible forms.
   * e.g. "PIDILITIND" → "PIDILITIND.NS", "AAPL" stays "AAPL"
   * Only touches symbols that have no '.' in them (not yet resolved).
   */
  private async migrateSymbols(): Promise<void> {
    try {
      const portfolios = await invoke<Portfolio[]>('portfolio_get_all');
      for (const portfolio of portfolios) {
        const assets = await invoke<PortfolioAsset[]>('portfolio_get_assets', {
          portfolioId: portfolio.id,
        });

        for (const asset of assets) {
          // Skip symbols that already have an exchange suffix
          if (asset.symbol.includes('.') || asset.symbol.includes('-') ||
              asset.symbol.includes('=') || asset.symbol.startsWith('^')) {
            continue;
          }

          try {
            const resolveOutput = await invoke<string>('execute_yfinance_command', {
              command: 'resolve_symbol',
              args: [asset.symbol],
            });
            const resolved = JSON.parse(resolveOutput);

            if (resolved.found && resolved.resolved_symbol &&
                resolved.resolved_symbol !== asset.symbol) {
              await invoke('portfolio_update_asset_symbol', {
                assetId: asset.id,
                newSymbol: resolved.resolved_symbol,
              });
              portfolioLogger.info(
                `Migrated symbol: ${asset.symbol} → ${resolved.resolved_symbol}`
              );
            }
          } catch (resolveErr) {
            portfolioLogger.warn(
              `Failed to resolve symbol ${asset.symbol}: ${resolveErr}`
            );
          }
        }
      }
    } catch (err) {
      portfolioLogger.warn(`Symbol migration error: ${err}`);
    }
  }

  async createPortfolio(name: string, owner: string, currency: string = 'USD', description?: string): Promise<Portfolio> {
    portfolioLogger.info(`Creating portfolio: ${name}`);
    const portfolio = await invoke<Portfolio>('portfolio_create', {
      name,
      owner,
      currency,
      description,
    });
    return portfolio;
  }

  async getPortfolios(): Promise<Portfolio[]> {
    portfolioLogger.info('Fetching all portfolios');
    const portfolios = await invoke<Portfolio[]>('portfolio_get_all');
    return portfolios;
  }

  async getPortfolio(portfolioId: string): Promise<Portfolio | null> {
    portfolioLogger.info(`Fetching portfolio: ${portfolioId}`);
    const portfolio = await invoke<Portfolio | null>('portfolio_get_by_id', { portfolioId });
    return portfolio;
  }

  async updatePortfolio(portfolioId: string, updates: Partial<Portfolio>): Promise<void> {
    // Not implemented yet - would need Rust command
    throw new Error('updatePortfolio not implemented yet');
  }

  async deletePortfolio(portfolioId: string): Promise<void> {
    portfolioLogger.info(`Deleting portfolio: ${portfolioId}`);
    await invoke('portfolio_delete', { portfolioId });
  }

  async addAsset(portfolioId: string, symbol: string, quantity: number, buyPrice: number, purchaseDate?: string): Promise<PortfolioAsset> {
    portfolioLogger.info(`Adding asset ${symbol} to portfolio ${portfolioId}`);
    await invoke('portfolio_add_asset', {
      portfolioId,
      symbol: symbol.toUpperCase(),
      quantity,
      price: buyPrice,
    });

    // Return a dummy asset (actual data fetched via getPortfolioAssets)
    return {
      id: '',
      portfolio_id: portfolioId,
      symbol: symbol.toUpperCase(),
      quantity,
      avg_buy_price: buyPrice,
      first_purchase_date: purchaseDate || new Date().toISOString(),
      last_updated: new Date().toISOString(),
    };
  }

  async removeAsset(portfolioId: string, symbol: string, sellPrice?: number): Promise<void> {
    // Sell all quantity
    const assets = await this.getPortfolioAssets(portfolioId);
    const asset = assets.find(a => a.symbol === symbol.toUpperCase());
    if (asset) {
      // Use provided price, or fetch current price, or fallback to avg buy price
      let price = sellPrice;
      if (price === undefined || price === null) {
        try {
          const quotes = await marketDataService.getQuotes([symbol]);
          price = quotes[0]?.price ?? asset.avg_buy_price;
        } catch {
          price = asset.avg_buy_price;
        }
      }
      await this.sellAsset(portfolioId, symbol, asset.quantity, price);
    }
  }

  async updateAsset(portfolioId: string, symbol: string, quantity: number, avgBuyPrice: number): Promise<void> {
    // Not directly supported - would need to sell all then re-buy
    throw new Error('updateAsset not implemented - use sell and add instead');
  }

  async sellAsset(portfolioId: string, symbol: string, quantity: number, sellPrice: number, transactionDate?: string): Promise<void> {
    portfolioLogger.info(`Selling ${quantity} of ${symbol} from portfolio ${portfolioId}`);
    await invoke('portfolio_sell_asset', {
      portfolioId,
      symbol: symbol.toUpperCase(),
      quantity,
      price: sellPrice,
    });
  }

  async getPortfolioAssets(portfolioId: string): Promise<PortfolioAsset[]> {
    portfolioLogger.info(`Fetching assets for portfolio: ${portfolioId}`);
    const assets = await invoke<PortfolioAsset[]>('portfolio_get_assets', { portfolioId });
    return assets;
  }

  async addTransaction(portfolioId: string, symbol: string, type: 'BUY' | 'SELL' | 'DIVIDEND' | 'SPLIT', quantity: number, price: number, transactionDate?: string, notes?: string): Promise<Transaction> {
    // Transactions are auto-created by add/sell asset commands
    throw new Error('addTransaction called directly - use addAsset or sellAsset instead');
  }

  async getPortfolioTransactions(portfolioId: string, limit: number = 100): Promise<Transaction[]> {
    portfolioLogger.info(`Fetching transactions for portfolio: ${portfolioId}`);
    const transactions = await invoke<Transaction[]>('portfolio_get_transactions', {
      portfolioId,
      limit,
    });
    return transactions;
  }

  async getSymbolTransactions(portfolioId: string, symbol: string): Promise<Transaction[]> {
    const allTransactions = await this.getPortfolioTransactions(portfolioId, 1000);
    return allTransactions.filter(t => t.symbol === symbol.toUpperCase());
  }

  async updateTransaction(
    transactionId: string,
    quantity: number,
    price: number,
    transactionDate: string,
    notes?: string
  ): Promise<void> {
    portfolioLogger.info(`Updating transaction: ${transactionId}`);
    await invoke('portfolio_update_transaction', {
      transactionId,
      quantity,
      price,
      transactionDate,
      notes,
    });
  }

  async deleteTransaction(transactionId: string): Promise<void> {
    portfolioLogger.info(`Deleting transaction: ${transactionId}`);
    await invoke('portfolio_delete_transaction', { transactionId });
  }

  async getTransactionById(transactionId: string): Promise<Transaction | null> {
    portfolioLogger.info(`Fetching transaction: ${transactionId}`);
    const transaction = await invoke<Transaction | null>('portfolio_get_transaction_by_id', { transactionId });
    return transaction;
  }

  async getPortfolioSummary(portfolioId: string): Promise<PortfolioSummary> {
    portfolioLogger.info(`Calculating portfolio summary: ${portfolioId}`);

    // Get portfolio and assets
    const portfolio = await this.getPortfolio(portfolioId);
    if (!portfolio) {
      throw new Error('Portfolio not found');
    }

    const assets = await this.getPortfolioAssets(portfolioId);

    // Fetch current quotes for all symbols
    const symbols = assets.map(a => a.symbol);
    const quotes = await marketDataService.getQuotes(symbols);

    // Calculate holdings with quotes
    let totalMarketValue = 0;
    let totalCostBasis = 0;
    let totalDayChange = 0;

    const holdings: HoldingWithQuote[] = assets.map(asset => {
      const quote = quotes.find((q: QuoteData) => q.symbol === asset.symbol);
      const currentPrice = quote?.price || asset.avg_buy_price;
      const previousClose = quote?.previous_close || currentPrice;

      const marketValue = currentPrice * asset.quantity;
      const costBasis = asset.avg_buy_price * asset.quantity;
      const unrealizedPnl = marketValue - costBasis;
      const unrealizedPnlPercent = costBasis > 0 ? (unrealizedPnl / costBasis) * 100 : 0;
      const dayChange = (currentPrice - previousClose) * asset.quantity;
      const dayChangePercent = previousClose > 0 ? ((currentPrice - previousClose) / previousClose) * 100 : 0;

      totalMarketValue += marketValue;
      totalCostBasis += costBasis;
      totalDayChange += dayChange;

      return {
        ...asset,
        current_price: currentPrice,
        market_value: marketValue,
        cost_basis: costBasis,
        unrealized_pnl: unrealizedPnl,
        unrealized_pnl_percent: unrealizedPnlPercent,
        day_change: dayChange,
        day_change_percent: dayChangePercent,
        weight: 0, // Will be calculated below
      };
    });

    // Calculate weights
    holdings.forEach(h => {
      h.weight = totalMarketValue > 0 ? (h.market_value / totalMarketValue) * 100 : 0;
    });

    const totalUnrealizedPnl = totalMarketValue - totalCostBasis;
    const totalUnrealizedPnlPercent = totalCostBasis > 0 ? (totalUnrealizedPnl / totalCostBasis) * 100 : 0;
    const totalDayChangePercent = totalMarketValue > 0 ? (totalDayChange / (totalMarketValue - totalDayChange)) * 100 : 0;

    return {
      portfolio,
      holdings,
      total_market_value: totalMarketValue,
      total_cost_basis: totalCostBasis,
      total_unrealized_pnl: totalUnrealizedPnl,
      total_unrealized_pnl_percent: totalUnrealizedPnlPercent,
      total_positions: holdings.length,
      total_day_change: totalDayChange,
      total_day_change_percent: totalDayChangePercent,
      last_updated: new Date().toISOString(),
    };
  }

  async savePortfolioSnapshot(portfolioId: string): Promise<PortfolioSnapshot> {
    // Snapshots not implemented yet
    throw new Error('savePortfolioSnapshot not implemented yet');
  }

  async getPortfolioPerformanceHistory(portfolioId: string, limit: number = 30): Promise<PortfolioSnapshot[]> {
    // Snapshots not implemented yet
    return [];
  }

  async getRebalancingRecommendations(portfolioId: string, targetAllocations: Record<string, number>): Promise<any[]> {
    // Advanced features not implemented yet
    return [];
  }

  async analyzeDiversification(portfolioId: string): Promise<any> {
    // Advanced features not implemented yet
    return {};
  }

  async calculateRiskMetrics(portfolioId: string): Promise<any> {
    // Advanced features not implemented yet
    return {};
  }

  // ==================== IMPORT / EXPORT HELPERS ====================

  parsePortfolioCSV(csv: string): import('../../components/tabs/portfolio-tab/types').PortfolioExportJSON {
    const lines = csv.replace(/\r\n/g, '\n').replace(/\r/g, '\n').split('\n');

    let portfolioName = '';
    let owner = '';
    let currency = '';
    let exportDate = new Date().toISOString();

    for (const line of lines) {
      if (line.startsWith('Portfolio Name:,')) portfolioName = line.slice('Portfolio Name:,'.length).trim();
      else if (line.startsWith('Owner:,')) owner = line.slice('Owner:,'.length).trim();
      else if (line.startsWith('Currency:,')) currency = line.slice('Currency:,'.length).trim();
      else if (line.startsWith('Export Date:,')) exportDate = line.slice('Export Date:,'.length).trim();
    }

    if (!portfolioName) throw new Error('Could not find "Portfolio Name:" in CSV');

    // Find Transactions section
    const txHeader = 'Date,Symbol,Type,Quantity,Price,Total Value,Notes';
    const txHeaderIdx = lines.findIndex(l => l.trim() === txHeader);
    if (txHeaderIdx === -1) throw new Error('Could not find Transactions header in CSV');

    const transactions: import('../../components/tabs/portfolio-tab/types').PortfolioExportTransaction[] = [];
    for (let i = txHeaderIdx + 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;
      // Split into max 7 parts (notes may contain commas)
      const parts = line.split(',');
      if (parts.length < 6) continue;
      const [date, symbol, type, quantityStr, priceStr, totalValueStr, ...noteParts] = parts;
      const txType = type.trim().toUpperCase() as 'BUY' | 'SELL' | 'DIVIDEND' | 'SPLIT';
      if (!['BUY', 'SELL', 'DIVIDEND', 'SPLIT'].includes(txType)) continue;
      const quantity = parseFloat(quantityStr);
      const price = parseFloat(priceStr);
      const total_value = parseFloat(totalValueStr);
      if (isNaN(quantity) || isNaN(price)) continue;
      transactions.push({
        date: date.trim(),
        symbol: symbol.trim().toUpperCase(),
        type: txType,
        quantity,
        price,
        total_value: isNaN(total_value) ? quantity * price : total_value,
        notes: noteParts.join(',').trim(),
      });
    }

    return { format_version: '1.0', portfolio_name: portfolioName, owner, currency, export_date: exportDate, transactions };
  }

  parsePortfolioJSON(json: string): import('../../components/tabs/portfolio-tab/types').PortfolioExportJSON {
    let data: any;
    try { data = JSON.parse(json); } catch { throw new Error('Invalid JSON file'); }
    if (!data.portfolio_name) throw new Error('Missing portfolio_name in JSON');
    if (!Array.isArray(data.transactions)) throw new Error('Missing transactions array in JSON');
    const validTypes = ['BUY', 'SELL', 'DIVIDEND', 'SPLIT'];
    const transactions = data.transactions.map((t: any, i: number) => {
      if (!t.symbol) throw new Error(`Transaction ${i}: missing symbol`);
      if (!validTypes.includes(String(t.type).toUpperCase())) throw new Error(`Transaction ${i}: invalid type "${t.type}"`);
      return {
        date: t.date || new Date().toISOString(),
        symbol: String(t.symbol).toUpperCase(),
        type: String(t.type).toUpperCase() as 'BUY' | 'SELL' | 'DIVIDEND' | 'SPLIT',
        quantity: parseFloat(t.quantity) || 0,
        price: parseFloat(t.price) || 0,
        total_value: parseFloat(t.total_value) || 0,
        notes: t.notes || '',
      };
    });
    return {
      format_version: '1.0',
      portfolio_name: data.portfolio_name,
      owner: data.owner || '',
      currency: data.currency || 'USD',
      export_date: data.export_date || new Date().toISOString(),
      transactions,
    };
  }

  async exportPortfolioJSON(portfolioId: string): Promise<string> {
    portfolioLogger.info(`Exporting portfolio ${portfolioId} to JSON`);
    const summary = await this.getPortfolioSummary(portfolioId);
    const transactions = await this.getPortfolioTransactions(portfolioId, 1000);
    const data: import('../../components/tabs/portfolio-tab/types').PortfolioExportJSON = {
      format_version: '1.0',
      portfolio_name: summary.portfolio.name,
      owner: summary.portfolio.owner,
      currency: summary.portfolio.currency,
      export_date: new Date().toISOString(),
      transactions: transactions.map(t => ({
        date: t.transaction_date,
        symbol: t.symbol,
        type: t.transaction_type,
        quantity: t.quantity,
        price: t.price,
        total_value: t.total_value,
        notes: t.notes || '',
      })),
    };
    return JSON.stringify(data, null, 2);
  }

  async importPortfolio(
    data: import('../../components/tabs/portfolio-tab/types').PortfolioExportJSON,
    mode: import('../../components/tabs/portfolio-tab/types').ImportMode,
    mergeTargetId?: string,
    onProgress?: (current: number, total: number) => void
  ): Promise<import('../../components/tabs/portfolio-tab/types').ImportResult> {
    portfolioLogger.info(`Importing portfolio "${data.portfolio_name}" mode=${mode}`);

    let targetPortfolioId: string;
    let targetPortfolioName: string;

    if (mode === 'new') {
      const created = await this.createPortfolio(data.portfolio_name, data.owner || 'Imported', data.currency || 'USD');
      targetPortfolioId = created.id;
      targetPortfolioName = created.name;
    } else {
      if (!mergeTargetId) throw new Error('mergeTargetId required for merge mode');
      const existing = await this.getPortfolio(mergeTargetId);
      if (!existing) throw new Error('Target portfolio not found');
      targetPortfolioId = mergeTargetId;
      targetPortfolioName = existing.name;
    }

    // Sort transactions chronologically
    const sorted = [...data.transactions].sort((a, b) => new Date(a.date).getTime() - new Date(b.date).getTime());

    const errors: string[] = [];
    let replayed = 0;

    for (let i = 0; i < sorted.length; i++) {
      const txn = sorted[i];
      onProgress?.(i, sorted.length);
      try {
        if (txn.type === 'BUY') {
          await this.addAsset(targetPortfolioId, txn.symbol, txn.quantity, txn.price);
          replayed++;
        } else if (txn.type === 'SELL') {
          await this.sellAsset(targetPortfolioId, txn.symbol, txn.quantity, txn.price);
          replayed++;
        } else {
          errors.push(`Skipped ${txn.type} for ${txn.symbol} on ${txn.date} (unsupported type)`);
        }
      } catch (err: any) {
        errors.push(`${txn.date} ${txn.symbol} ${txn.type}: ${err?.message || String(err)}`);
      }
    }

    onProgress?.(sorted.length, sorted.length);
    return { portfolioId: targetPortfolioId, portfolioName: targetPortfolioName, transactionsReplayed: replayed, errors };
  }

  async exportPortfolioCSV(portfolioId: string): Promise<string> {
    portfolioLogger.info(`Exporting portfolio ${portfolioId} to CSV`);

    const summary = await this.getPortfolioSummary(portfolioId);
    const transactions = await this.getPortfolioTransactions(portfolioId, 1000);

    // Build CSV
    let csv = 'Portfolio Export\n\n';
    csv += `Portfolio Name:,${summary.portfolio.name}\n`;
    csv += `Owner:,${summary.portfolio.owner}\n`;
    csv += `Currency:,${summary.portfolio.currency}\n`;
    csv += `Export Date:,${new Date().toISOString()}\n\n`;

    csv += 'Holdings\n';
    csv += 'Symbol,Quantity,Avg Buy Price,Current Price,Market Value,Cost Basis,Unrealized P&L,P&L %,Weight %\n';
    summary.holdings.forEach(h => {
      csv += `${h.symbol},${h.quantity},${h.avg_buy_price},${h.current_price},${h.market_value},${h.cost_basis},${h.unrealized_pnl},${h.unrealized_pnl_percent},${h.weight}\n`;
    });

    csv += '\nTransactions\n';
    csv += 'Date,Symbol,Type,Quantity,Price,Total Value,Notes\n';
    transactions.forEach(t => {
      csv += `${t.transaction_date},${t.symbol},${t.transaction_type},${t.quantity},${t.price},${t.total_value},${t.notes || ''}\n`;
    });

    return csv;
  }

  async calculateAdvancedMetrics(portfolioId: string): Promise<any> {
    portfolioLogger.info(`Calculating advanced metrics for portfolio: ${portfolioId}`);

    const summary = await this.getPortfolioSummary(portfolioId);
    const holdings = summary.holdings;

    if (holdings.length === 0) {
      return {
        sharpe_ratio: 0,
        portfolio_volatility: 0,
        value_at_risk_95: 0,
        max_drawdown: 0,
        portfolio_return: 0,
      };
    }

    // Calculate basic metrics from holdings data
    const weights = holdings.map(h => h.weight / 100);
    const returns = holdings.map(h => (h.unrealized_pnl_percent || 0) / 100);

    // Portfolio return (weighted average)
    const portfolioReturn = weights.reduce((sum, w, i) => sum + w * returns[i], 0);

    // Portfolio volatility estimate (simplified - using day change variance)
    const dayChanges = holdings.map(h => (h.day_change_percent || 0) / 100);
    const avgDayChange = dayChanges.reduce((sum, d) => sum + d, 0) / dayChanges.length;
    const variance = dayChanges.reduce((sum, d) => sum + Math.pow(d - avgDayChange, 2), 0) / dayChanges.length;
    const dailyVolatility = Math.sqrt(variance);
    const annualizedVolatility = dailyVolatility * Math.sqrt(252);

    // Sharpe ratio (assuming 4% risk-free rate)
    const riskFreeRate = 0.04;
    const excessReturn = portfolioReturn - riskFreeRate;
    const sharpeRatio = annualizedVolatility > 0 ? excessReturn / annualizedVolatility : 0;

    // VaR 95% (parametric)
    const var95 = avgDayChange - 1.645 * dailyVolatility;

    // Max drawdown estimate from unrealized losses
    const maxLoss = Math.min(...returns, 0);

    return {
      sharpe_ratio: sharpeRatio,
      portfolio_volatility: annualizedVolatility,
      value_at_risk_95: Math.abs(var95),
      max_drawdown: Math.abs(maxLoss),
      portfolio_return: portfolioReturn,
    };
  }

  async optimizePortfolioWeights(portfolioId: string, method: string = 'max_sharpe'): Promise<any> {
    portfolioLogger.info(`Optimizing portfolio weights: ${portfolioId}, method: ${method}`);

    const summary = await this.getPortfolioSummary(portfolioId);
    const holdings = summary.holdings;

    if (holdings.length === 0) {
      return {
        optimal_weights: [],
        sharpe_ratio: 0,
        expected_return: 0,
        volatility: 0,
      };
    }

    // Simple equal-weight optimization as fallback
    // In production, this would call the Python optimizer
    const n = holdings.length;
    const equalWeight = 1 / n;

    // Calculate metrics for equal-weight portfolio
    const returns = holdings.map(h => (h.unrealized_pnl_percent || 0) / 100);
    const expectedReturn = returns.reduce((sum, r) => sum + r * equalWeight, 0);

    const dayChanges = holdings.map(h => (h.day_change_percent || 0) / 100);
    const avgChange = dayChanges.reduce((sum, d) => sum + d, 0) / n;
    const variance = dayChanges.reduce((sum, d) => sum + Math.pow(d - avgChange, 2), 0) / n;
    const volatility = Math.sqrt(variance) * Math.sqrt(252);

    const sharpeRatio = volatility > 0 ? (expectedReturn - 0.04) / volatility : 0;

    return {
      optimal_weights: holdings.map(() => equalWeight),
      sharpe_ratio: sharpeRatio,
      expected_return: expectedReturn,
      volatility: volatility,
    };
  }

  async generateAssetAllocation(portfolioId: string, strategy: string): Promise<any> {
    // Advanced features not implemented yet
    return {};
  }

  async calculateRetirementPlan(portfolioId: string, params: any): Promise<any> {
    // Advanced features not implemented yet
    return {};
  }

  async analyzeBehavioralBiases(portfolioId: string): Promise<any> {
    // Advanced features not implemented yet
    return {};
  }
}

export const portfolioService = new PortfolioService();
