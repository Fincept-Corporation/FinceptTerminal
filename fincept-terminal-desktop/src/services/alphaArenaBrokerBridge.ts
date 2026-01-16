/**
 * Alpha Arena Broker Bridge
 * Connects Alpha Arena AI competition to BrokerContext paper/live trading
 *
 * This service replaces the isolated Python paper trading with integration
 * to the main trading infrastructure used by TradingTab.
 */

import { invoke } from '@tauri-apps/api/core';
import type { IExchangeAdapter } from '../brokers/crypto/types';
import type { PaperTradingAdapter } from '../paper-trading';

export interface AlphaArenaExecutionConfig {
  competitionId: string;
  modelName: string;
  portfolioId: string;
  adapter: IExchangeAdapter | PaperTradingAdapter;
  usePaperTrading: boolean;
}

export interface AlphaArenaTradeInstruction {
  symbol: string;
  side: 'buy' | 'sell';
  quantity: number;
  orderType: 'market' | 'limit';
  price?: number;
  confidence: number;
  reasoning: string;
}

export interface AlphaArenaTradeResult {
  success: boolean;
  orderId?: string;
  fillPrice?: number;
  fillQuantity?: number;
  fee?: number;
  error?: string;
  timestamp: string;
}

export interface AlphaArenaPortfolioSnapshot {
  cash: number;
  positions: Array<{
    symbol: string;
    side: 'long' | 'short';
    quantity: number;
    entryPrice: number;
    currentPrice: number;
    unrealizedPnl: number;
  }>;
  totalValue: number;
  totalPnl: number;
  returnPct: number;
}

/**
 * AlphaArenaBrokerBridge
 *
 * Replaces Python's PaperExecutionGateway with proper integration to:
 * - BrokerContext (paper/live mode switching)
 * - PaperTradingAdapter (realistic order execution)
 * - Real exchange adapters (live trading capability)
 * - Rust paper trading database
 */
export class AlphaArenaBrokerBridge {
  private configs: Map<string, AlphaArenaExecutionConfig> = new Map();

  /**
   * Initialize a model's trading adapter for Alpha Arena
   */
  async initializeModel(
    competitionId: string,
    modelName: string,
    adapter: IExchangeAdapter | PaperTradingAdapter,
    usePaperTrading: boolean = true,
    initialBalance: number = 10000
  ): Promise<string> {
    const portfolioId = `alpha-arena-${competitionId}-${modelName.toLowerCase().replace(/\s+/g, '-')}`;

    // If using paper trading, create portfolio in Rust database
    if (usePaperTrading) {
      try {
        await invoke('paper_trading_create_portfolio', {
          id: portfolioId,
          name: `Alpha Arena - ${modelName}`,
          provider: this.getProviderName(adapter),
          initialBalance,
          currency: 'USD',
          marginMode: 'cross',
          leverage: 1.0
        });
        console.log(`[AlphaArenaBridge] Created paper portfolio: ${portfolioId}`);
      } catch (error) {
        // Portfolio might already exist
        console.warn(`[AlphaArenaBridge] Portfolio creation warning:`, error);
      }
    }

    // Store configuration
    this.configs.set(modelName, {
      competitionId,
      modelName,
      portfolioId,
      adapter,
      usePaperTrading
    });

    return portfolioId;
  }

  /**
   * Execute a trade instruction for a model
   * Uses the same execution path as TradingTab
   */
  async executeTrade(
    modelName: string,
    instruction: AlphaArenaTradeInstruction
  ): Promise<AlphaArenaTradeResult> {
    const config = this.configs.get(modelName);
    if (!config) {
      return {
        success: false,
        error: `Model ${modelName} not initialized`,
        timestamp: new Date().toISOString()
      };
    }

    const startTime = Date.now();

    try {
      const { adapter, portfolioId, usePaperTrading } = config;

      // Get current price for the symbol
      let currentPrice: number;
      try {
        const ticker = await adapter.fetchTicker(instruction.symbol);
        currentPrice = ticker.last || ticker.close || 0;
      } catch (error) {
        return {
          success: false,
          error: `Failed to fetch ticker for ${instruction.symbol}: ${error}`,
          timestamp: new Date().toISOString()
        };
      }

      // Execute order through adapter
      const order = await adapter.createOrder(
        instruction.symbol,
        instruction.orderType,
        instruction.side,
        instruction.quantity,
        instruction.price
      );

      if (!order || !order.id) {
        return {
          success: false,
          error: 'Order creation failed - no order ID returned',
          timestamp: new Date().toISOString()
        };
      }

      // For paper trading, record additional metadata
      if (usePaperTrading) {
        // Wait a bit for order to fill (paper trading fills instantly)
        await new Promise(resolve => setTimeout(resolve, 100));

        // Fetch order status
        const filledOrder = await adapter.fetchOrder(order.id, instruction.symbol);

        // Calculate fill details
        const fillPrice = filledOrder.average || filledOrder.price || currentPrice;
        const fillQuantity = filledOrder.filled || instruction.quantity;
        const fee = (filledOrder.fee?.cost || 0);

        // Record decision in Alpha Arena database
        try {
          await invoke('alpha_arena_record_decision', {
            competitionId: config.competitionId,
            modelName,
            portfolioId,
            symbol: instruction.symbol,
            side: instruction.side,
            quantity: fillQuantity,
            price: fillPrice,
            confidence: instruction.confidence,
            reasoning: instruction.reasoning,
            orderId: order.id,
            fee,
            timestamp: new Date().toISOString()
          });
        } catch (error) {
          console.warn('[AlphaArenaBridge] Failed to record decision:', error);
        }

        return {
          success: true,
          orderId: order.id,
          fillPrice,
          fillQuantity,
          fee,
          timestamp: new Date().toISOString()
        };
      } else {
        // Live trading - order might be pending
        return {
          success: true,
          orderId: order.id,
          fillPrice: order.price,
          fillQuantity: order.filled || 0,
          fee: order.fee?.cost || 0,
          timestamp: new Date().toISOString()
        };
      }
    } catch (error) {
      console.error('[AlphaArenaBridge] Trade execution error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
        timestamp: new Date().toISOString()
      };
    }
  }

  /**
   * Get portfolio snapshot for a model
   * Reads from Rust paper trading database
   */
  async getPortfolioSnapshot(modelName: string): Promise<AlphaArenaPortfolioSnapshot | null> {
    const config = this.configs.get(modelName);
    if (!config) {
      console.error(`[AlphaArenaBridge] Model ${modelName} not initialized`);
      return null;
    }

    try {
      const { portfolioId, adapter } = config;

      // Get portfolio from Rust database
      const portfolio = await invoke<any>('paper_trading_get_portfolio', {
        id: portfolioId
      });

      // Get positions from Rust database
      const positions = await invoke<any[]>('paper_trading_get_positions', {
        portfolioId,
        status: 'open'
      });

      // Fetch current prices for open positions
      const positionsWithPrices = await Promise.all(
        positions.map(async (pos) => {
          let currentPrice = pos.current_price || pos.entry_price;
          try {
            const ticker = await adapter.fetchTicker(pos.symbol);
            currentPrice = ticker.last || ticker.close || pos.entry_price;
          } catch (error) {
            console.warn(`[AlphaArenaBridge] Failed to fetch price for ${pos.symbol}`);
          }

          const unrealizedPnl = pos.side === 'long'
            ? (currentPrice - pos.entry_price) * pos.quantity
            : (pos.entry_price - currentPrice) * pos.quantity;

          return {
            symbol: pos.symbol,
            side: pos.side,
            quantity: pos.quantity,
            entryPrice: pos.entry_price,
            currentPrice,
            unrealizedPnl
          };
        })
      );

      const totalUnrealizedPnl = positionsWithPrices.reduce((sum, p) => sum + p.unrealizedPnl, 0);
      const totalValue = portfolio.current_balance + totalUnrealizedPnl;
      const totalPnl = totalValue - portfolio.initial_balance;
      const returnPct = (totalPnl / portfolio.initial_balance) * 100;

      return {
        cash: portfolio.current_balance,
        positions: positionsWithPrices,
        totalValue,
        totalPnl,
        returnPct
      };
    } catch (error) {
      console.error('[AlphaArenaBridge] Failed to get portfolio snapshot:', error);
      return null;
    }
  }

  /**
   * Get balance for a model (cash only)
   */
  async getBalance(modelName: string): Promise<number> {
    const snapshot = await this.getPortfolioSnapshot(modelName);
    return snapshot?.cash || 0;
  }

  /**
   * Get all open positions for a model
   */
  async getOpenPositions(modelName: string) {
    const snapshot = await this.getPortfolioSnapshot(modelName);
    return snapshot?.positions || [];
  }

  /**
   * Close all positions for a model
   */
  async closeAllPositions(modelName: string): Promise<void> {
    const config = this.configs.get(modelName);
    if (!config) return;

    const positions = await this.getOpenPositions(modelName);

    for (const position of positions) {
      const closeSide = position.side === 'long' ? 'sell' : 'buy';
      await this.executeTrade(modelName, {
        symbol: position.symbol,
        side: closeSide,
        quantity: position.quantity,
        orderType: 'market',
        confidence: 1.0,
        reasoning: 'Closing position - competition end'
      });
    }
  }

  /**
   * Reset model portfolio (for testing)
   */
  async resetPortfolio(modelName: string, initialBalance: number = 10000): Promise<void> {
    const config = this.configs.get(modelName);
    if (!config) return;

    const { portfolioId } = config;

    // Close all positions first
    await this.closeAllPositions(modelName);

    // Reset balance
    await invoke('paper_trading_update_balance', {
      id: portfolioId,
      newBalance: initialBalance
    });
  }

  /**
   * Cleanup - disconnect adapters
   */
  async cleanup(): Promise<void> {
    for (const [modelName, config] of this.configs.entries()) {
      try {
        if (config.adapter.isConnected()) {
          await config.adapter.disconnect();
        }
      } catch (error) {
        console.error(`[AlphaArenaBridge] Cleanup error for ${modelName}:`, error);
      }
    }
    this.configs.clear();
  }

  /**
   * Helper: Get provider name from adapter
   */
  private getProviderName(adapter: IExchangeAdapter | PaperTradingAdapter): string {
    return adapter.name.toLowerCase().replace(/\s+/g, '_').replace('_(paper_trading)', '');
  }
}

// Singleton instance
export const alphaArenaBrokerBridge = new AlphaArenaBrokerBridge();
