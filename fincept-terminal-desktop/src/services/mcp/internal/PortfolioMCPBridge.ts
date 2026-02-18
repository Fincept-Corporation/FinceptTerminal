// Portfolio MCP Bridge - Connects portfolioService to MCP Internal Tools
// Exposes complete portfolio functionality to AI via chat

import { terminalMCPProvider } from './TerminalMCPProvider';
import { portfolioService } from '@/services/portfolio/portfolioService';

// All context keys used by this bridge - single source of truth for connect/disconnect
const CONTEXT_KEYS = [
  'getPortfolios', 'getPortfolio', 'createPortfolio', 'deletePortfolio',
  'addAsset', 'removeAsset', 'sellAsset', 'getPortfolioAssets',
  'addTransaction', 'getPortfolioTransactions', 'getSymbolTransactions',
  'updateTransaction', 'deleteTransaction', 'getTransactionById',
  'getPortfolioSummary', 'calculateAdvancedMetrics', 'calculateRiskMetrics',
  'optimizePortfolioWeights', 'getRebalancingRecommendations',
  'analyzeDiversification', 'getPortfolioPerformanceHistory',
  'exportPortfolioCSV', 'exportPortfolioJSON', 'importPortfolio',
  'savePortfolioSnapshot', 'getPortfolioSnapshot',
] as const;

/**
 * Bridge that connects portfolioService to MCP tools.
 * Allows AI agents to manage portfolios, assets, transactions, and analytics via chat.
 */
export class PortfolioMCPBridge {
  private connected: boolean = false;

  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({
      // ── Portfolio CRUD ──────────────────────────────────────────────────────
      getPortfolios: () => portfolioService.getPortfolios(),

      getPortfolio: (portfolioId: string) =>
        portfolioService.getPortfolio(portfolioId),

      createPortfolio: (params: { name: string; owner?: string; currency?: string; description?: string }) =>
        portfolioService.createPortfolio(
          params.name,
          params.owner || 'User',
          params.currency || 'USD',
          params.description,
        ),

      deletePortfolio: (portfolioId: string) =>
        portfolioService.deletePortfolio(portfolioId),

      // ── Asset Management ────────────────────────────────────────────────────
      addAsset: (portfolioId: string, asset: { symbol: string; quantity: number; avg_buy_price: number }) =>
        portfolioService.addAsset(portfolioId, asset.symbol, asset.quantity, asset.avg_buy_price),

      removeAsset: (portfolioId: string, symbol: string, sellPrice?: number) =>
        portfolioService.removeAsset(portfolioId, symbol, sellPrice),

      sellAsset: (portfolioId: string, symbol: string, quantity: number, price: number) =>
        portfolioService.sellAsset(portfolioId, symbol, quantity, price),

      getPortfolioAssets: (portfolioId: string) =>
        portfolioService.getPortfolioAssets(portfolioId),

      // ── Transactions ────────────────────────────────────────────────────────
      addTransaction: async (portfolioId: string, tx: {
        type: string; symbol: string; quantity: number; price: number; date?: string; notes?: string;
      }) => {
        const type = tx.type.toUpperCase();
        if (type === 'BUY') return portfolioService.addAsset(portfolioId, tx.symbol, tx.quantity, tx.price, tx.date);
        if (type === 'SELL') {
          await portfolioService.sellAsset(portfolioId, tx.symbol, tx.quantity, tx.price, tx.date);
          return { success: true };
        }
        return { success: false, message: `${type} transactions are tracked automatically` };
      },

      getPortfolioTransactions: (portfolioId: string, limit?: number) =>
        portfolioService.getPortfolioTransactions(portfolioId, limit || 100),

      getSymbolTransactions: (portfolioId: string, symbol: string) =>
        portfolioService.getSymbolTransactions(portfolioId, symbol),

      // ── Summary & Analytics ─────────────────────────────────────────────────
      getPortfolioSummary: (portfolioId: string) =>
        portfolioService.getPortfolioSummary(portfolioId),

      calculateAdvancedMetrics: (portfolioId: string) =>
        portfolioService.calculateAdvancedMetrics(portfolioId),

      calculateRiskMetrics: (portfolioId: string) =>
        portfolioService.calculateRiskMetrics(portfolioId),

      // ── Optimization ────────────────────────────────────────────────────────
      optimizePortfolioWeights: (portfolioId: string, method?: string) =>
        portfolioService.optimizePortfolioWeights(portfolioId, method || 'max_sharpe'),

      getRebalancingRecommendations: (portfolioId: string, targetAllocations: Record<string, number>) =>
        portfolioService.getRebalancingRecommendations(portfolioId, targetAllocations),

      // ── Analysis ────────────────────────────────────────────────────────────
      analyzeDiversification: (portfolioId: string) =>
        portfolioService.analyzeDiversification(portfolioId),

      getPortfolioPerformanceHistory: (portfolioId: string, limit?: number) =>
        portfolioService.getPortfolioPerformanceHistory(portfolioId, limit || 30),

      // ── Transaction management ──────────────────────────────────────────────
      updateTransaction: (transactionId: string, quantity: number, price: number, transactionDate: string, notes?: string) =>
        portfolioService.updateTransaction(transactionId, quantity, price, transactionDate, notes),

      deleteTransaction: (transactionId: string) =>
        portfolioService.deleteTransaction(transactionId),

      getTransactionById: (transactionId: string) =>
        portfolioService.getTransactionById(transactionId),

      // ── Export ──────────────────────────────────────────────────────────────
      exportPortfolioCSV: (portfolioId: string) =>
        portfolioService.exportPortfolioCSV(portfolioId),

      exportPortfolioJSON: (portfolioId: string) =>
        portfolioService.exportPortfolioJSON(portfolioId),

      // ── Import ──────────────────────────────────────────────────────────────
      importPortfolio: (data: any, mode: 'new' | 'merge', mergeTargetId?: string) =>
        portfolioService.importPortfolio(data, mode, mergeTargetId),

      // ── Snapshot ────────────────────────────────────────────────────────────
      savePortfolioSnapshot: (portfolioId: string) =>
        portfolioService.savePortfolioSnapshot(portfolioId),

      // ── Snapshot (rich formatted context for agents) ─────────────────────
      getPortfolioSnapshot: async (portfolioId: string) => {
        const [summary, metrics, diversification] = await Promise.allSettled([
          portfolioService.getPortfolioSummary(portfolioId),
          portfolioService.calculateAdvancedMetrics(portfolioId),
          portfolioService.analyzeDiversification(portfolioId),
        ]);

        return {
          summary: summary.status === 'fulfilled' ? summary.value : null,
          metrics: metrics.status === 'fulfilled' ? metrics.value : null,
          diversification: diversification.status === 'fulfilled' ? diversification.value : null,
          fetched_at: new Date().toISOString(),
        };
      },
    });

    this.connected = true;
    console.log('[PortfolioMCPBridge] Connected');
  }

  disconnect(): void {
    if (!this.connected) return;
    const cleared = Object.fromEntries(CONTEXT_KEYS.map(k => [k, undefined]));
    terminalMCPProvider.setContexts(cleared);
    this.connected = false;
    console.log('[PortfolioMCPBridge] Disconnected');
  }

  isConnected(): boolean {
    return this.connected;
  }
}

// Singleton instance
export const portfolioMCPBridge = new PortfolioMCPBridge();
