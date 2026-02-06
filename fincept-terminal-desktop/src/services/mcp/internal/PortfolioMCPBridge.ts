// Portfolio MCP Bridge - Connects portfolioService to MCP Internal Tools
// Exposes complete portfolio functionality to AI via chat

import { terminalMCPProvider } from './TerminalMCPProvider';
import { portfolioService } from '@/services/portfolio/portfolioService';

/**
 * Bridge that connects portfolioService to MCP tools
 * Allows AI to manage portfolios, assets, transactions, and analytics via chat
 */
export class PortfolioMCPBridge {
  private connected: boolean = false;

  /**
   * Connect portfolio service to MCP contexts
   */
  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({
      // ==================== PORTFOLIO CRUD ====================

      // Get all portfolios
      getPortfolios: async () => {
        return await portfolioService.getPortfolios();
      },

      // Get single portfolio
      getPortfolio: async (portfolioId: string) => {
        return await portfolioService.getPortfolio(portfolioId);
      },

      // Create portfolio
      createPortfolio: async (params: { name: string; owner?: string; currency?: string; description?: string }) => {
        return await portfolioService.createPortfolio(
          params.name,
          params.owner || 'User',
          params.currency || 'USD',
          params.description
        );
      },

      // Delete portfolio
      deletePortfolio: async (portfolioId: string) => {
        await portfolioService.deletePortfolio(portfolioId);
      },

      // ==================== ASSET MANAGEMENT ====================

      // Add asset to portfolio
      addAsset: async (portfolioId: string, asset: { symbol: string; quantity: number; avg_buy_price: number }) => {
        return await portfolioService.addAsset(
          portfolioId,
          asset.symbol,
          asset.quantity,
          asset.avg_buy_price
        );
      },

      // Remove asset from portfolio (sells all shares)
      removeAsset: async (portfolioId: string, symbol: string, sellPrice?: number) => {
        await portfolioService.removeAsset(portfolioId, symbol, sellPrice);
      },

      // Sell asset (partial or full)
      sellAsset: async (portfolioId: string, symbol: string, quantity: number, price: number) => {
        await portfolioService.sellAsset(portfolioId, symbol, quantity, price);
      },

      // Get portfolio assets
      getPortfolioAssets: async (portfolioId: string) => {
        return await portfolioService.getPortfolioAssets(portfolioId);
      },

      // ==================== TRANSACTIONS ====================

      // Add transaction (note: portfolioService auto-creates transactions via addAsset/sellAsset)
      addTransaction: async (portfolioId: string, tx: {
        type: string;
        symbol: string;
        quantity: number;
        price: number;
        date?: string;
        notes?: string;
      }) => {
        // Convert to appropriate action
        const txType = tx.type.toUpperCase();
        if (txType === 'BUY') {
          return await portfolioService.addAsset(portfolioId, tx.symbol, tx.quantity, tx.price, tx.date);
        } else if (txType === 'SELL') {
          await portfolioService.sellAsset(portfolioId, tx.symbol, tx.quantity, tx.price, tx.date);
          return { success: true };
        } else {
          // Dividend and split are not directly supported - return info
          return {
            success: false,
            message: `${txType} transactions are tracked automatically by the system`,
          };
        }
      },

      // Get portfolio transactions
      getPortfolioTransactions: async (portfolioId: string, limit?: number) => {
        return await portfolioService.getPortfolioTransactions(portfolioId, limit || 100);
      },

      // Get transactions for specific symbol
      getSymbolTransactions: async (portfolioId: string, symbol: string) => {
        return await portfolioService.getSymbolTransactions(portfolioId, symbol);
      },

      // ==================== SUMMARY & ANALYTICS ====================

      // Get portfolio summary with holdings and P&L
      getPortfolioSummary: async (portfolioId: string) => {
        return await portfolioService.getPortfolioSummary(portfolioId);
      },

      // Calculate advanced metrics (Sharpe, volatility, VaR, etc.)
      calculateAdvancedMetrics: async (portfolioId: string) => {
        return await portfolioService.calculateAdvancedMetrics(portfolioId);
      },

      // Calculate risk metrics
      calculateRiskMetrics: async (portfolioId: string) => {
        return await portfolioService.calculateRiskMetrics(portfolioId);
      },

      // ==================== OPTIMIZATION ====================

      // Optimize portfolio weights
      optimizePortfolioWeights: async (portfolioId: string, method?: string) => {
        return await portfolioService.optimizePortfolioWeights(portfolioId, method || 'max_sharpe');
      },

      // Get rebalancing recommendations
      getRebalancingRecommendations: async (portfolioId: string, targetAllocations: Record<string, number>) => {
        return await portfolioService.getRebalancingRecommendations(portfolioId, targetAllocations);
      },

      // ==================== ANALYSIS ====================

      // Analyze diversification
      analyzeDiversification: async (portfolioId: string) => {
        return await portfolioService.analyzeDiversification(portfolioId);
      },

      // Get performance history
      getPortfolioPerformanceHistory: async (portfolioId: string, limit?: number) => {
        return await portfolioService.getPortfolioPerformanceHistory(portfolioId, limit || 30);
      },

      // ==================== EXPORT ====================

      // Export portfolio to CSV
      exportPortfolioCSV: async (portfolioId: string) => {
        return await portfolioService.exportPortfolioCSV(portfolioId);
      },
    });

    this.connected = true;
    console.log('[PortfolioMCPBridge] Connected portfolio service to MCP');
  }

  /**
   * Disconnect and clear contexts
   */
  disconnect(): void {
    if (!this.connected) return;

    terminalMCPProvider.setContexts({
      // Portfolio CRUD
      getPortfolios: undefined,
      getPortfolio: undefined,
      createPortfolio: undefined,
      deletePortfolio: undefined,
      // Asset management
      addAsset: undefined,
      removeAsset: undefined,
      sellAsset: undefined,
      getPortfolioAssets: undefined,
      // Transactions
      addTransaction: undefined,
      getPortfolioTransactions: undefined,
      getSymbolTransactions: undefined,
      // Summary & analytics
      getPortfolioSummary: undefined,
      calculateAdvancedMetrics: undefined,
      calculateRiskMetrics: undefined,
      // Optimization
      optimizePortfolioWeights: undefined,
      getRebalancingRecommendations: undefined,
      // Analysis
      analyzeDiversification: undefined,
      getPortfolioPerformanceHistory: undefined,
      // Export
      exportPortfolioCSV: undefined,
    });

    this.connected = false;
    console.log('[PortfolioMCPBridge] Disconnected portfolio service from MCP');
  }

  /**
   * Check if bridge is connected
   */
  isConnected(): boolean {
    return this.connected;
  }
}

// Singleton instance
export const portfolioMCPBridge = new PortfolioMCPBridge();
