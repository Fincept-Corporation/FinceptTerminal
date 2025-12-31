/**
 * Paper Trading Balance & Position Management
 *
 * Handles balance tracking, position management, margin calculations, and liquidation checks
 */

import type { Balance } from 'ccxt';
import type {
  PaperTradingConfig,
  PaperTradingPortfolio,
  PaperTradingPosition,
  MarginRequirement,
  LiquidationCheck,
} from './types';
import type { OrderSide } from '../brokers/crypto/types';
import { InsufficientFunds } from '../brokers/crypto/types';
import { paperTradingDatabase } from './PaperTradingDatabase';

export class PaperTradingBalance {
  constructor(private config: PaperTradingConfig) {}

  // ============================================================================
  // BALANCE OPERATIONS
  // ============================================================================

  /**
   * Get current balance in CCXT format
   */
  async getBalance(portfolioId: string): Promise<Balance> {
    const portfolio = await paperTradingDatabase.getPortfolio(portfolioId);
    if (!portfolio) {
      throw new Error(`Portfolio ${portfolioId} not found`);
    }

    const positions = await paperTradingDatabase.getPortfolioPositions(portfolioId, 'open');
    const marginUsed = this.calculateTotalMarginUsed(positions);

    const currency = portfolio.currency || 'USD';
    const free = Math.max(0, portfolio.currentBalance - marginUsed);
    const used = marginUsed;
    const total = portfolio.currentBalance;

    const balanceObj: any = {
      free: { [currency]: free },
      used: { [currency]: used },
      total: { [currency]: total },
      info: {
        portfolio_id: portfolioId,
        margin_mode: portfolio.marginMode,
        leverage: portfolio.leverage,
      },
    };

    return balanceObj as Balance;
  }

  /**
   * Check if sufficient funds for an order
   */
  async checkSufficientFunds(
    portfolioId: string,
    symbol: string,
    side: OrderSide,
    quantity: number,
    price: number,
    leverage?: number
  ): Promise<{ sufficient: boolean; required: number; available: number; error?: string }> {
    const portfolio = await paperTradingDatabase.getPortfolio(portfolioId);
    if (!portfolio) {
      return { sufficient: false, required: 0, available: 0, error: 'Portfolio not found' };
    }

    const effectiveLeverage = leverage || portfolio.leverage || 1;
    const orderValue = quantity * price;
    const marginRequired = orderValue / effectiveLeverage;

    // Add fees to required amount
    const feeRate = side === 'buy' ? this.config.fees.taker : this.config.fees.maker;
    const fee = orderValue * feeRate;
    const totalRequired = marginRequired + fee;

    const balance = await this.getBalance(portfolioId);
    const currency = portfolio.currency || 'USD';
    const available = (balance.free as any)?.[currency] || 0;

    const sufficient = available >= totalRequired;

    return {
      sufficient,
      required: totalRequired,
      available,
      error: sufficient ? undefined : `Insufficient funds: ${available.toFixed(2)} ${currency} available, ${totalRequired.toFixed(2)} ${currency} required`,
    };
  }

  /**
   * Lock funds when placing an order
   */
  async lockFunds(portfolioId: string, amount: number): Promise<void> {
    // Funds are implicitly locked by creating positions with margin requirements
    // No explicit locking needed as we check available funds on each order
  }

  /**
   * Release funds when canceling an order
   */
  async releaseFunds(portfolioId: string, amount: number): Promise<void> {
    // No explicit release needed, funds become available when positions close
  }

  // ============================================================================
  // POSITION MANAGEMENT
  // ============================================================================

  /**
   * Create or update position after order fill
   *
   * Position Logic:
   * - BUY order creates/adds to LONG position OR closes SHORT position
   * - SELL order creates/adds to SHORT position OR closes LONG position
   *
   * When you already have a position:
   * - If you have a LONG position and SELL, it CLOSES the long (not creates short)
   * - If you have a SHORT position and BUY, it CLOSES the short (not creates long)
   */
  async updatePositionAfterFill(
    portfolioId: string,
    symbol: string,
    side: OrderSide,
    fillPrice: number,
    fillQuantity: number,
    leverage: number,
    marginMode: 'cross' | 'isolated',
    reduceOnly?: boolean
  ): Promise<PaperTradingPosition> {
    console.log(`[PaperTradingBalance] Processing ${side.toUpperCase()} order for ${fillQuantity} ${symbol} @ ${fillPrice}`);

    // CRITICAL FIX: Check for OPPOSITE positions to close
    // BUY closes SHORT, SELL closes LONG
    const oppositePositionSide = side === 'buy' ? 'short' : 'long';
    const oppositePosition = await paperTradingDatabase.getPositionBySymbolAndSide(portfolioId, symbol, oppositePositionSide, 'open');

    // If reduceOnly and no opposite position, reject
    if (reduceOnly && !oppositePosition) {
      throw new Error(`Cannot reduce position - no ${oppositePositionSide} position exists for ${symbol}`);
    }

    // If reduceOnly OR opposite position exists, REDUCE/CLOSE that position
    if (oppositePosition) {
      console.log(`[PaperTradingBalance] Reducing/closing ${oppositePositionSide} position for ${symbol}`);
      console.log(`  Existing: ${oppositePosition.quantity} @ ${oppositePosition.entryPrice}`);
      console.log(`  Reducing by: ${fillQuantity} @ ${fillPrice}`);

      // Validate reduceOnly doesn't exceed position
      if (reduceOnly && fillQuantity > oppositePosition.quantity) {
        throw new Error(`Reduce-only order quantity ${fillQuantity} exceeds position size ${oppositePosition.quantity}`);
      }

      const pnl = oppositePositionSide === 'long'
        ? (fillPrice - oppositePosition.entryPrice) * fillQuantity
        : (oppositePosition.entryPrice - fillPrice) * fillQuantity;

      console.log(`  Realized PnL: ${pnl.toFixed(2)}`);

      // Update portfolio balance with realized PnL (done after fee calculation in executeTrade)
      const portfolio = await paperTradingDatabase.getPortfolio(portfolioId);
      if (portfolio) {
        await paperTradingDatabase.updatePortfolioBalance(portfolioId, portfolio.currentBalance + pnl);
      }

      if (oppositePosition.quantity === fillQuantity) {
        // FULLY CLOSE position
        console.log(`  Fully closing position`);
        await paperTradingDatabase.updatePosition(oppositePosition.id, {
          status: 'closed',
          closedAt: new Date().toISOString(),
          realizedPnl: pnl,
          quantity: 0
        });
      } else if (oppositePosition.quantity > fillQuantity) {
        // PARTIALLY CLOSE position
        const remainingQty = oppositePosition.quantity - fillQuantity;
        console.log(`  Partially closing, remaining: ${remainingQty}`);
        await paperTradingDatabase.updatePosition(oppositePosition.id, {
          quantity: remainingQty,
          realizedPnl: (oppositePosition.realizedPnl || 0) + pnl
        });
      } else {
        // Fill exceeds position - close position and open opposite
        console.log(`  Fill exceeds position, closing and opening opposite`);
        await paperTradingDatabase.updatePosition(oppositePosition.id, {
          status: 'closed',
          closedAt: new Date().toISOString(),
          realizedPnl: pnl,
          quantity: 0
        });

        // The excess creates a NEW position in the ORIGINAL order direction
        const excessQuantity = fillQuantity - oppositePosition.quantity;
        const newPositionSide = side === 'buy' ? 'long' : 'short';
        console.log(`[PaperTradingBalance] Creating ${newPositionSide} position with excess quantity ${excessQuantity}`);
        return await this.createNewPosition(portfolioId, symbol, newPositionSide, fillPrice, excessQuantity, leverage, marginMode);
      }

      const updatedPosition = await paperTradingDatabase.getPosition(oppositePosition.id);
      if (!updatedPosition) throw new Error('Failed to retrieve updated position');
      return updatedPosition;
    }

    // NO opposite position found - CREATE OR ADD to position with matching side
    // BUY order creates/adds to LONG position
    // SELL order creates/adds to SHORT position
    const newPositionSide = side === 'buy' ? 'long' : 'short';

    console.log(`[PaperTradingBalance] No opposite position found, checking for same-side position...`);

    // Check for existing position WITH THE SAME SIDE (for adding to existing position)
    const existingPosition = await paperTradingDatabase.getPositionBySymbolAndSide(portfolioId, symbol, newPositionSide, 'open');

    if (!existingPosition) {
      // Create new position
      console.log(`[PaperTradingBalance] Creating new ${newPositionSide.toUpperCase()} position`);
      return await this.createNewPosition(portfolioId, symbol, newPositionSide, fillPrice, fillQuantity, leverage, marginMode);
    } else {
      // Position exists with same side - ADD to position (VWAP averaging)
      console.log(`[PaperTradingBalance] Adding to existing ${newPositionSide} position for ${symbol}`);
      console.log(`  Existing: ${existingPosition.quantity} @ ${existingPosition.entryPrice}`);
      console.log(`  Adding: ${fillQuantity} @ ${fillPrice}`);

      // Calculate new average entry price (VWAP - Volume Weighted Average Price)
      // IMPORTANT: Fees are already deducted from balance when order executes
      // DO NOT include fees in entry price calculation as that would double-count them
      const existingCost = existingPosition.entryPrice * existingPosition.quantity;
      const newFillCost = fillPrice * fillQuantity; // Pure cost without fees
      const totalCost = existingCost + newFillCost;
      const totalQuantity = existingPosition.quantity + fillQuantity;
      const newEntryPrice = totalCost / totalQuantity;

      console.log(`  New position: ${totalQuantity} @ ${newEntryPrice}`);

      const entryFeeRate = this.config.fees?.taker || 0.0005;
      const exitFeeRate = this.config.fees?.taker || 0.0005;
      const liquidationPrice = this.calculateLiquidationPrice(
        newPositionSide,
        newEntryPrice,
        leverage,
        totalQuantity,
        entryFeeRate,
        exitFeeRate
      );

      await paperTradingDatabase.updatePosition(existingPosition.id, {
        quantity: totalQuantity,
        entryPrice: newEntryPrice,
        liquidationPrice,
      });

      const updatedPosition = await paperTradingDatabase.getPosition(existingPosition.id);
      if (!updatedPosition) throw new Error('Failed to retrieve updated position');

      return updatedPosition;
    }
  }

  /**
   * Helper to create new position
   */
  private async createNewPosition(
    portfolioId: string,
    symbol: string,
    side: 'long' | 'short',
    entryPrice: number,
    quantity: number,
    leverage: number,
    marginMode: 'cross' | 'isolated'
  ): Promise<PaperTradingPosition> {
    console.log(`[PaperTradingBalance] Creating new ${side} position for ${symbol}`);
    const positionId = `pos_${Date.now()}_${Math.random().toString(36).substring(7)}`;

    // Use config fee rates for accurate liquidation price
    const entryFeeRate = this.config.fees?.taker || 0.0005;
    const exitFeeRate = this.config.fees?.taker || 0.0005;
    const liquidationPrice = this.calculateLiquidationPrice(
      side,
      entryPrice,
      leverage,
      quantity,
      entryFeeRate,
      exitFeeRate
    );

    await paperTradingDatabase.createPosition({
      id: positionId,
      portfolioId,
      symbol,
      side,
      entryPrice,
      quantity,
      leverage,
      marginMode,
    });

    const position = await paperTradingDatabase.getPosition(positionId);
    if (!position) throw new Error('Failed to create position');

    await paperTradingDatabase.updatePosition(positionId, { liquidationPrice });
    return position;
  }

  /**
   * Update position prices and unrealized P&L
   */
  async updatePositionPrices(portfolioId: string, symbol: string, currentPrice: number): Promise<void> {
    const position = await paperTradingDatabase.getPositionBySymbol(portfolioId, symbol, 'open');
    if (!position) return;

    const unrealizedPnl = this.calculateUnrealizedPnL(
      position.side,
      position.entryPrice,
      currentPrice,
      position.quantity
    );

    await paperTradingDatabase.updatePosition(position.id, {
      currentPrice,
      unrealizedPnl,
    });
  }

  /**
   * Check if position should be liquidated
   */
  async checkLiquidation(position: PaperTradingPosition, currentPrice: number): Promise<LiquidationCheck> {
    if (!position.liquidationPrice) {
      return { shouldLiquidate: false, position, currentPrice, liquidationPrice: 0, marginRatio: 0 };
    }

    const shouldLiquidate =
      (position.side === 'long' && currentPrice <= position.liquidationPrice) ||
      (position.side === 'short' && currentPrice >= position.liquidationPrice);

    if (!shouldLiquidate) {
      const marginValue = (position.quantity * position.entryPrice) / position.leverage;
      const positionValue = position.quantity * currentPrice;
      const pnl = position.side === 'long'
        ? positionValue - position.quantity * position.entryPrice
        : position.quantity * position.entryPrice - positionValue;
      const equity = marginValue + pnl;
      const marginRatio = (equity / positionValue) * 100;

      return {
        shouldLiquidate: false,
        position,
        currentPrice,
        liquidationPrice: position.liquidationPrice,
        marginRatio,
      };
    }

    return {
      shouldLiquidate: true,
      position,
      currentPrice,
      liquidationPrice: position.liquidationPrice,
      marginRatio: 0,
      reason: `Price ${currentPrice} ${position.side === 'long' ? '<=' : '>='} liquidation price ${position.liquidationPrice}`,
    };
  }

  /**
   * Execute liquidation
   */
  async executeLiquidation(position: PaperTradingPosition, liquidationPrice: number): Promise<void> {
    // Calculate final realized P&L at liquidation
    const realizedPnl = this.calculateRealizedPnL(
      position.side,
      position.entryPrice,
      liquidationPrice,
      position.quantity
    );

    await paperTradingDatabase.updatePosition(position.id, {
      quantity: 0,
      currentPrice: liquidationPrice,
      unrealizedPnl: 0,
      realizedPnl: position.realizedPnl + realizedPnl,
      status: 'closed',
      closedAt: new Date().toISOString(),
    });

    // Update portfolio balance
    const portfolio = await paperTradingDatabase.getPortfolio(position.portfolioId);
    if (portfolio) {
      const newBalance = portfolio.currentBalance + realizedPnl;
      await paperTradingDatabase.updatePortfolioBalance(position.portfolioId, newBalance);
    }
  }

  // ============================================================================
  // P&L CALCULATIONS
  // ============================================================================

  /**
   * Calculate unrealized P&L for a position
   */
  calculateUnrealizedPnL(
    side: 'long' | 'short',
    entryPrice: number,
    currentPrice: number,
    quantity: number
  ): number {
    if (side === 'long') {
      return (currentPrice - entryPrice) * quantity;
    } else {
      return (entryPrice - currentPrice) * quantity;
    }
  }

  /**
   * Calculate realized P&L when closing a position
   */
  calculateRealizedPnL(
    side: 'long' | 'short',
    entryPrice: number,
    exitPrice: number,
    quantity: number
  ): number {
    if (side === 'long') {
      return (exitPrice - entryPrice) * quantity;
    } else {
      return (entryPrice - exitPrice) * quantity;
    }
  }

  /**
   * Calculate total margin used across all positions
   */
  private calculateTotalMarginUsed(positions: PaperTradingPosition[]): number {
    return positions.reduce((total, position) => {
      const positionValue = position.quantity * position.entryPrice;
      const marginUsed = positionValue / position.leverage;
      return total + marginUsed;
    }, 0);
  }

  // ============================================================================
  // MARGIN & LIQUIDATION CALCULATIONS
  // ============================================================================

  /**
   * Calculate margin requirement for a trade
   */
  calculateMarginRequirement(
    symbol: string,
    side: 'long' | 'short',
    price: number,
    quantity: number,
    leverage: number,
    marginMode: 'cross' | 'isolated'
  ): MarginRequirement {
    const positionValue = price * quantity;
    const initialMargin = positionValue / leverage;

    // Maintenance margin is typically 50% of initial margin for simplicity
    const maintenanceMargin = initialMargin * 0.5;

    const liquidationPrice = this.calculateLiquidationPrice(side, price, leverage, quantity);

    return {
      symbol,
      side,
      quantity,
      price,
      leverage,
      marginMode,
      initialMargin,
      maintenanceMargin,
      liquidationPrice,
    };
  }

  /**
   * Calculate liquidation price (improved with fee consideration)
   */
  private calculateLiquidationPrice(
    side: 'long' | 'short',
    entryPrice: number,
    leverage: number,
    quantity: number,
    entryFeeRate: number = 0.0005, // Default taker fee 0.05%
    exitFeeRate: number = 0.0005   // Default exit fee 0.05%
  ): number | null {
    if (leverage <= 1) return null; // No liquidation for unleveraged positions
    if (quantity <= 0) return null; // Cannot calculate liquidation for zero/negative quantity
    if (entryPrice <= 0) return null; // Invalid entry price

    // Maintenance margin rate (typically 50% of initial margin)
    const maintenanceMarginRate = 0.5 / leverage;

    // Position value
    const positionValue = entryPrice * quantity;

    // Initial margin (capital used to open position)
    const initialMargin = positionValue / leverage;

    // Entry fees already paid (reduces available margin)
    const entryFees = positionValue * entryFeeRate;

    // Exit fees that will be charged on liquidation
    const liquidationPenalty = positionValue * exitFeeRate;

    // Effective margin after fees
    const effectiveMargin = initialMargin - entryFees;

    // Maintenance margin requirement
    const maintenanceMargin = positionValue * maintenanceMarginRate;

    // Maximum loss before liquidation = effective margin - maintenance margin - exit fees
    const maxLoss = effectiveMargin - maintenanceMargin - liquidationPenalty;

    // Calculate liquidation price
    if (side === 'long') {
      // Long liquidation: price drops until loss equals maxLoss
      // Loss = (entryPrice - liquidationPrice) * quantity
      // liquidationPrice = entryPrice - (maxLoss / quantity)
      return entryPrice - (maxLoss / quantity);
    } else {
      // Short liquidation: price rises until loss equals maxLoss
      // Loss = (liquidationPrice - entryPrice) * quantity
      // liquidationPrice = entryPrice + (maxLoss / quantity)
      return entryPrice + (maxLoss / quantity);
    }
  }

  // ============================================================================
  // PORTFOLIO STATISTICS
  // ============================================================================

  /**
   * Calculate portfolio total P&L
   */
  async calculatePortfolioPnL(portfolioId: string): Promise<{
    unrealizedPnL: number;
    realizedPnL: number;
    totalPnL: number;
  }> {
    const positions = await paperTradingDatabase.getPortfolioPositions(portfolioId);

    let unrealizedPnL = 0;
    let realizedPnL = 0;

    for (const position of positions) {
      realizedPnL += position.realizedPnl;
      if (position.status === 'open' && position.unrealizedPnl !== null) {
        unrealizedPnL += position.unrealizedPnl;
      }
    }

    return {
      unrealizedPnL,
      realizedPnL,
      totalPnL: unrealizedPnL + realizedPnL,
    };
  }

  /**
   * Calculate total portfolio value (balance + unrealized P&L)
   */
  async calculateTotalValue(portfolioId: string): Promise<number> {
    const portfolio = await paperTradingDatabase.getPortfolio(portfolioId);
    if (!portfolio) return 0;

    const { unrealizedPnL } = await this.calculatePortfolioPnL(portfolioId);
    return portfolio.currentBalance + unrealizedPnL;
  }
}
