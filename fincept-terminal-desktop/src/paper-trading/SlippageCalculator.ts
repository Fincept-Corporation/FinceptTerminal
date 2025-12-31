/**
 * Slippage Calculator
 *
 * Provides realistic slippage calculation based on:
 * - Order size (larger orders = more slippage)
 * - Market volatility (high volatility = more slippage)
 * - Order book depth (simulated)
 * - Time of day (optional - trading hours vs off-hours)
 */

import type { PaperTradingConfig, PriceSnapshot } from './types';
import type { OrderSide } from '../brokers/crypto/types';

export class SlippageCalculator {
  private volatilityCache: Map<string, { volatility: number; timestamp: number }> = new Map();
  private priceHistory: Map<string, number[]> = new Map();
  private readonly VOLATILITY_WINDOW = 20; // 20 price samples for volatility calc
  private readonly VOLATILITY_CACHE_TTL = 60000; // 1 minute

  constructor(private config: PaperTradingConfig) {}

  /**
   * Calculate slippage for a market order
   */
  calculateSlippage(
    symbol: string,
    side: OrderSide,
    orderSize: number,
    basePrice: number,
    priceSnapshot?: PriceSnapshot
  ): number {
    const modelType = this.config.slippage.modelType || 'fixed';

    switch (modelType) {
      case 'fixed':
        return this.calculateFixedSlippage();

      case 'size-dependent':
        return this.calculateSizeDependentSlippage(orderSize, basePrice);

      case 'volatility-adjusted':
        return this.calculateVolatilityAdjustedSlippage(symbol, orderSize, basePrice);

      default:
        return this.calculateFixedSlippage();
    }
  }

  /**
   * Fixed slippage (original model)
   */
  private calculateFixedSlippage(): number {
    return this.config.slippage.market;
  }

  /**
   * Size-dependent slippage
   * Larger orders experience more slippage due to market impact
   */
  private calculateSizeDependentSlippage(orderSize: number, basePrice: number): number {
    const baseSlippage = this.config.slippage.market;
    const sizeImpactFactor = this.config.slippage.sizeImpactFactor || 0.0001;

    // Order value in USD (approximation)
    const orderValue = orderSize * basePrice;

    // Size impact: additional slippage proportional to order value
    // Example: $10,000 order with factor 0.0001 = 0.1% additional slippage
    const sizeImpact = (orderValue / 10000) * sizeImpactFactor;

    // Total slippage = base + size impact (capped at 10x base slippage)
    const totalSlippage = Math.min(baseSlippage + sizeImpact, baseSlippage * 10);

    return totalSlippage;
  }

  /**
   * Volatility-adjusted slippage
   * Higher volatility = higher slippage
   */
  private calculateVolatilityAdjustedSlippage(
    symbol: string,
    orderSize: number,
    basePrice: number
  ): number {
    // Start with size-dependent slippage
    const baseSizeSlippage = this.calculateSizeDependentSlippage(orderSize, basePrice);

    // Get current volatility
    const volatility = this.getVolatility(symbol);

    // Volatility multiplier (default 2.0 means 2x slippage at high volatility)
    const volatilityMultiplier = this.config.slippage.volatilityMultiplier || 2.0;

    // Calculate volatility adjustment
    // Low volatility (0-1%): 1.0x multiplier
    // Medium volatility (1-2%): 1.5x multiplier
    // High volatility (2%+): 2.0x+ multiplier
    let volatilityAdjustment = 1.0;
    if (volatility > 0.02) {
      // High volatility (>2%)
      volatilityAdjustment = volatilityMultiplier;
    } else if (volatility > 0.01) {
      // Medium volatility (1-2%)
      volatilityAdjustment = 1.0 + (volatilityMultiplier - 1.0) * 0.5;
    }

    return baseSizeSlippage * volatilityAdjustment;
  }

  /**
   * Update price history for volatility calculation
   */
  updatePriceHistory(symbol: string, price: number): void {
    let history = this.priceHistory.get(symbol);
    if (!history) {
      history = [];
      this.priceHistory.set(symbol, history);
    }

    history.push(price);

    // Keep only last N samples
    if (history.length > this.VOLATILITY_WINDOW) {
      history.shift();
    }
  }

  /**
   * Calculate volatility (standard deviation of returns)
   */
  private getVolatility(symbol: string): number {
    // Check cache first
    const cached = this.volatilityCache.get(symbol);
    if (cached && Date.now() - cached.timestamp < this.VOLATILITY_CACHE_TTL) {
      return cached.volatility;
    }

    const history = this.priceHistory.get(symbol);
    if (!history || history.length < 2) {
      // Not enough data, use default low volatility
      return 0.01; // 1%
    }

    // Calculate returns
    const returns: number[] = [];
    for (let i = 1; i < history.length; i++) {
      const returnValue = (history[i] - history[i - 1]) / history[i - 1];
      returns.push(returnValue);
    }

    // Calculate standard deviation
    const mean = returns.reduce((sum, r) => sum + r, 0) / returns.length;
    const squaredDiffs = returns.map(r => Math.pow(r - mean, 2));
    const variance = squaredDiffs.reduce((sum, sd) => sum + sd, 0) / squaredDiffs.length;
    const stdDev = Math.sqrt(variance);

    // Handle zero volatility (all prices identical)
    if (stdDev === 0 || !isFinite(stdDev)) {
      // Return default low volatility instead of zero to avoid division by zero
      const defaultVolatility = 0.01; // 1% default volatility
      this.volatilityCache.set(symbol, { volatility: defaultVolatility, timestamp: Date.now() });
      return defaultVolatility;
    }

    // Annualize (approximate - assumes 1 sample per minute, 525,600 minutes per year)
    // For simplicity, we'll use the raw stdDev as percentage volatility
    const volatility = Math.abs(stdDev);

    // Cache result
    this.volatilityCache.set(symbol, { volatility, timestamp: Date.now() });

    return volatility;
  }

  /**
   * Calculate execution price with slippage
   */
  calculateExecutionPrice(
    symbol: string,
    side: OrderSide,
    orderSize: number,
    basePrice: number,
    priceSnapshot?: PriceSnapshot
  ): number {
    const slippage = this.calculateSlippage(symbol, side, orderSize, basePrice, priceSnapshot);

    if (side === 'buy') {
      // Buy orders: price increases (worse execution)
      return basePrice * (1 + slippage);
    } else {
      // Sell orders: price decreases (worse execution)
      return basePrice * (1 - slippage);
    }
  }

  /**
   * Get slippage statistics
   */
  getSlippageStats(
    symbol: string,
    orderSize: number,
    basePrice: number
  ): {
    baseSlippage: number;
    sizeImpact: number;
    volatilityImpact: number;
    totalSlippage: number;
    executionPrice: number;
  } {
    const baseSlippage = this.config.slippage.market;
    const sizeDependent = this.calculateSizeDependentSlippage(orderSize, basePrice);
    const volatilityAdjusted = this.calculateVolatilityAdjustedSlippage(symbol, orderSize, basePrice);

    const sizeImpact = sizeDependent - baseSlippage;
    const volatilityImpact = volatilityAdjusted - sizeDependent;
    const totalSlippage = volatilityAdjusted;

    return {
      baseSlippage,
      sizeImpact,
      volatilityImpact,
      totalSlippage,
      executionPrice: basePrice * (1 + totalSlippage),
    };
  }

  /**
   * Clear cached data
   */
  clearCache(): void {
    this.volatilityCache.clear();
    this.priceHistory.clear();
  }
}
