/**
 * Risk Manager
 * Trading safety and risk management system
 * Validates orders against configurable limits
 */

import { IDataObject } from '../types';
import { OrderRequest, Position, AccountBalance } from '../adapters/TradingBridge';

// ================================
// TYPES
// ================================

export interface TradingLimits {
  // Position limits
  maxPositionSize: number;           // Max quantity per position
  maxPositionValue: number;          // Max value per position ($)
  maxPortfolioExposure: number;      // Max % of portfolio in single asset
  maxTotalPositions: number;         // Max number of open positions

  // Order limits
  maxOrderValue: number;             // Max single order value ($)
  maxDailyTrades: number;            // Max trades per day
  maxDailyVolume: number;            // Max daily trading volume ($)

  // Loss limits
  dailyLossLimit: number;            // Max loss per day ($)
  weeklyLossLimit: number;           // Max loss per week ($)
  positionStopLoss: number;          // Auto stop-loss % per position

  // Asset restrictions
  allowedAssetClasses: string[];     // Allowed asset types
  blockedSymbols: string[];          // Blocked symbols
  allowShortSelling: boolean;        // Allow short positions
  allowMargin: boolean;              // Allow margin trading

  // Time restrictions
  tradingHoursOnly: boolean;         // Only trade during market hours
  noTradingDays: string[];           // Dates with no trading (YYYY-MM-DD)
}

export interface RiskCheckResult {
  valid: boolean;
  passed: RiskCheck[];
  failed: RiskCheck[];
  warnings: RiskCheck[];
  overallRisk: 'low' | 'medium' | 'high' | 'critical';
  message: string;
}

export interface RiskCheck {
  name: string;
  description: string;
  passed: boolean;
  severity: 'info' | 'warning' | 'error' | 'critical';
  value?: number | string;
  limit?: number | string;
  message: string;
}

export interface DailyTradingStats {
  date: string;
  tradesCount: number;
  totalVolume: number;
  realizedPnL: number;
  unrealizedPnL: number;
  peakBalance: number;
  currentBalance: number;
}

// ================================
// DEFAULT LIMITS
// ================================

const DEFAULT_LIMITS: TradingLimits = {
  // Position limits
  maxPositionSize: 10000,            // 10,000 units
  maxPositionValue: 100000,          // $100,000
  maxPortfolioExposure: 25,          // 25% of portfolio
  maxTotalPositions: 20,             // 20 positions

  // Order limits
  maxOrderValue: 50000,              // $50,000 per order
  maxDailyTrades: 50,                // 50 trades per day
  maxDailyVolume: 500000,            // $500,000 daily volume

  // Loss limits
  dailyLossLimit: 5000,              // $5,000 daily loss limit
  weeklyLossLimit: 15000,            // $15,000 weekly loss limit
  positionStopLoss: 10,              // 10% stop loss per position

  // Asset restrictions
  allowedAssetClasses: ['equity', 'crypto', 'etf', 'futures', 'options'],
  blockedSymbols: [],
  allowShortSelling: true,
  allowMargin: true,

  // Time restrictions
  tradingHoursOnly: false,
  noTradingDays: [],
};

// ================================
// RISK MANAGER
// ================================

class RiskManagerClass {
  private limits: TradingLimits;
  private dailyStats: DailyTradingStats;
  private weeklyPnL: number = 0;
  private enabled: boolean = true;

  constructor() {
    this.limits = { ...DEFAULT_LIMITS };
    this.dailyStats = this.createEmptyDailyStats();
  }

  /**
   * Enable/disable risk management
   */
  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
    console.log(`[RiskManager] Risk management ${enabled ? 'ENABLED' : 'DISABLED'}`);
  }

  /**
   * Check if risk management is enabled
   */
  isEnabled(): boolean {
    return this.enabled;
  }

  /**
   * Update trading limits
   */
  setLimits(limits: Partial<TradingLimits>): void {
    this.limits = { ...this.limits, ...limits };
    console.log('[RiskManager] Limits updated:', this.limits);
  }

  /**
   * Get current limits
   */
  getLimits(): TradingLimits {
    return { ...this.limits };
  }

  /**
   * Validate an order against risk limits
   */
  async validateOrder(
    order: OrderRequest,
    currentPositions: Position[],
    balance: AccountBalance,
    currentPrice?: number
  ): Promise<RiskCheckResult> {
    if (!this.enabled) {
      return {
        valid: true,
        passed: [],
        failed: [],
        warnings: [],
        overallRisk: 'low',
        message: 'Risk management disabled',
      };
    }

    const checks: RiskCheck[] = [];
    const price = currentPrice || order.price || 100; // Fallback price
    const orderValue = order.quantity * price;

    // 1. Check blocked symbols
    checks.push(this.checkBlockedSymbol(order.symbol));

    // 2. Check order value
    checks.push(this.checkOrderValue(orderValue));

    // 3. Check position size
    checks.push(this.checkPositionSize(order.quantity));

    // 4. Check portfolio exposure
    checks.push(this.checkPortfolioExposure(orderValue, balance.total));

    // 5. Check max positions
    checks.push(this.checkMaxPositions(currentPositions));

    // 6. Check daily trades
    checks.push(this.checkDailyTrades());

    // 7. Check daily volume
    checks.push(this.checkDailyVolume(orderValue));

    // 8. Check daily loss limit
    checks.push(this.checkDailyLossLimit());

    // 9. Check weekly loss limit
    checks.push(this.checkWeeklyLossLimit());

    // 10. Check sufficient balance
    checks.push(this.checkSufficientBalance(orderValue, balance.available));

    // 11. Check short selling
    if (order.side === 'SELL') {
      checks.push(this.checkShortSelling(order, currentPositions));
    }

    // 12. Check margin
    if (order.product === 'MARGIN') {
      checks.push(this.checkMarginAllowed());
    }

    // 13. Check trading hours
    checks.push(this.checkTradingHours());

    // 14. Check no-trading days
    checks.push(this.checkNoTradingDays());

    // Compile results
    const passed = checks.filter((c) => c.passed);
    const failed = checks.filter((c) => !c.passed && c.severity === 'error' || c.severity === 'critical');
    const warnings = checks.filter((c) => !c.passed && c.severity === 'warning');

    const overallRisk = this.calculateOverallRisk(checks);
    const valid = failed.length === 0;

    return {
      valid,
      passed,
      failed,
      warnings,
      overallRisk,
      message: valid
        ? `Order validated with ${warnings.length} warnings`
        : `Order rejected: ${failed.map((f) => f.message).join('; ')}`,
    };
  }

  /**
   * Record a completed trade
   */
  recordTrade(orderValue: number, pnl: number = 0): void {
    this.ensureDailyStatsValid();
    this.dailyStats.tradesCount++;
    this.dailyStats.totalVolume += orderValue;
    this.dailyStats.realizedPnL += pnl;
    this.weeklyPnL += pnl;
  }

  /**
   * Update unrealized P&L
   */
  updateUnrealizedPnL(pnl: number): void {
    this.ensureDailyStatsValid();
    this.dailyStats.unrealizedPnL = pnl;
  }

  /**
   * Get daily trading stats
   */
  getDailyStats(): DailyTradingStats {
    this.ensureDailyStatsValid();
    return { ...this.dailyStats };
  }

  /**
   * Reset daily stats (call at start of new day)
   */
  resetDailyStats(): void {
    this.dailyStats = this.createEmptyDailyStats();
  }

  /**
   * Reset weekly P&L (call at start of new week)
   */
  resetWeeklyPnL(): void {
    this.weeklyPnL = 0;
  }

  // ================================
  // INDIVIDUAL CHECKS
  // ================================

  private checkBlockedSymbol(symbol: string): RiskCheck {
    const blocked = this.limits.blockedSymbols.includes(symbol.toUpperCase());
    return {
      name: 'Blocked Symbol',
      description: 'Check if symbol is blocked from trading',
      passed: !blocked,
      severity: 'critical',
      value: symbol,
      message: blocked ? `Symbol ${symbol} is blocked from trading` : 'Symbol allowed',
    };
  }

  private checkOrderValue(orderValue: number): RiskCheck {
    const passed = orderValue <= this.limits.maxOrderValue;
    return {
      name: 'Order Value',
      description: 'Check if order value within limit',
      passed,
      severity: 'error',
      value: orderValue,
      limit: this.limits.maxOrderValue,
      message: passed
        ? 'Order value within limit'
        : `Order value $${orderValue.toFixed(2)} exceeds limit $${this.limits.maxOrderValue}`,
    };
  }

  private checkPositionSize(quantity: number): RiskCheck {
    const passed = quantity <= this.limits.maxPositionSize;
    return {
      name: 'Position Size',
      description: 'Check if position size within limit',
      passed,
      severity: 'error',
      value: quantity,
      limit: this.limits.maxPositionSize,
      message: passed
        ? 'Position size within limit'
        : `Quantity ${quantity} exceeds max position size ${this.limits.maxPositionSize}`,
    };
  }

  private checkPortfolioExposure(orderValue: number, portfolioValue: number): RiskCheck {
    if (portfolioValue === 0) {
      return {
        name: 'Portfolio Exposure',
        description: 'Check portfolio exposure percentage',
        passed: true,
        severity: 'warning',
        message: 'Cannot calculate exposure (no portfolio value)',
      };
    }

    const exposure = (orderValue / portfolioValue) * 100;
    const passed = exposure <= this.limits.maxPortfolioExposure;

    return {
      name: 'Portfolio Exposure',
      description: 'Check portfolio exposure percentage',
      passed,
      severity: 'warning',
      value: exposure.toFixed(2) + '%',
      limit: this.limits.maxPortfolioExposure + '%',
      message: passed
        ? `Exposure ${exposure.toFixed(2)}% within limit`
        : `Exposure ${exposure.toFixed(2)}% exceeds ${this.limits.maxPortfolioExposure}% limit`,
    };
  }

  private checkMaxPositions(positions: Position[]): RiskCheck {
    const passed = positions.length < this.limits.maxTotalPositions;
    return {
      name: 'Max Positions',
      description: 'Check number of open positions',
      passed,
      severity: 'warning',
      value: positions.length,
      limit: this.limits.maxTotalPositions,
      message: passed
        ? `${positions.length} positions (max ${this.limits.maxTotalPositions})`
        : `Already at maximum ${this.limits.maxTotalPositions} positions`,
    };
  }

  private checkDailyTrades(): RiskCheck {
    this.ensureDailyStatsValid();
    const passed = this.dailyStats.tradesCount < this.limits.maxDailyTrades;
    return {
      name: 'Daily Trades',
      description: 'Check daily trade count',
      passed,
      severity: 'warning',
      value: this.dailyStats.tradesCount,
      limit: this.limits.maxDailyTrades,
      message: passed
        ? `${this.dailyStats.tradesCount} trades today (max ${this.limits.maxDailyTrades})`
        : `Daily trade limit of ${this.limits.maxDailyTrades} reached`,
    };
  }

  private checkDailyVolume(orderValue: number): RiskCheck {
    this.ensureDailyStatsValid();
    const newVolume = this.dailyStats.totalVolume + orderValue;
    const passed = newVolume <= this.limits.maxDailyVolume;
    return {
      name: 'Daily Volume',
      description: 'Check daily trading volume',
      passed,
      severity: 'warning',
      value: newVolume,
      limit: this.limits.maxDailyVolume,
      message: passed
        ? `Volume $${newVolume.toFixed(2)} within limit`
        : `Would exceed daily volume limit of $${this.limits.maxDailyVolume}`,
    };
  }

  private checkDailyLossLimit(): RiskCheck {
    this.ensureDailyStatsValid();
    const totalLoss = this.dailyStats.realizedPnL + this.dailyStats.unrealizedPnL;
    const passed = totalLoss >= -this.limits.dailyLossLimit;
    return {
      name: 'Daily Loss Limit',
      description: 'Check if daily loss limit reached',
      passed,
      severity: 'critical',
      value: totalLoss,
      limit: -this.limits.dailyLossLimit,
      message: passed
        ? `Daily P&L: $${totalLoss.toFixed(2)}`
        : `Daily loss limit of $${this.limits.dailyLossLimit} reached - trading disabled`,
    };
  }

  private checkWeeklyLossLimit(): RiskCheck {
    const passed = this.weeklyPnL >= -this.limits.weeklyLossLimit;
    return {
      name: 'Weekly Loss Limit',
      description: 'Check if weekly loss limit reached',
      passed,
      severity: 'critical',
      value: this.weeklyPnL,
      limit: -this.limits.weeklyLossLimit,
      message: passed
        ? `Weekly P&L: $${this.weeklyPnL.toFixed(2)}`
        : `Weekly loss limit of $${this.limits.weeklyLossLimit} reached - trading disabled`,
    };
  }

  private checkSufficientBalance(orderValue: number, available: number): RiskCheck {
    const passed = orderValue <= available;
    return {
      name: 'Sufficient Balance',
      description: 'Check if sufficient funds available',
      passed,
      severity: 'error',
      value: available,
      limit: orderValue,
      message: passed
        ? `Sufficient balance: $${available.toFixed(2)}`
        : `Insufficient funds: need $${orderValue.toFixed(2)}, have $${available.toFixed(2)}`,
    };
  }

  private checkShortSelling(order: OrderRequest, positions: Position[]): RiskCheck {
    // Check if this is opening a new short or closing an existing long
    const existingPosition = positions.find((p) => p.symbol === order.symbol);
    const isClosingLong = existingPosition && existingPosition.side === 'BUY';

    if (isClosingLong) {
      return {
        name: 'Short Selling',
        description: 'Check if short selling is allowed',
        passed: true,
        severity: 'info',
        message: 'Closing existing long position',
      };
    }

    const passed = this.limits.allowShortSelling;
    return {
      name: 'Short Selling',
      description: 'Check if short selling is allowed',
      passed,
      severity: 'error',
      message: passed ? 'Short selling allowed' : 'Short selling is disabled',
    };
  }

  private checkMarginAllowed(): RiskCheck {
    const passed = this.limits.allowMargin;
    return {
      name: 'Margin Trading',
      description: 'Check if margin trading is allowed',
      passed,
      severity: 'error',
      message: passed ? 'Margin trading allowed' : 'Margin trading is disabled',
    };
  }

  private checkTradingHours(): RiskCheck {
    if (!this.limits.tradingHoursOnly) {
      return {
        name: 'Trading Hours',
        description: 'Check if within trading hours',
        passed: true,
        severity: 'info',
        message: 'Trading hours restriction disabled',
      };
    }

    const now = new Date();
    const hour = now.getUTCHours();
    const day = now.getUTCDay();

    // Simple check: weekdays 9:30 AM - 4:00 PM ET (14:30 - 21:00 UTC)
    const isWeekday = day >= 1 && day <= 5;
    const isMarketHours = hour >= 14 && hour < 21;
    const passed = isWeekday && isMarketHours;

    return {
      name: 'Trading Hours',
      description: 'Check if within trading hours',
      passed,
      severity: 'warning',
      message: passed ? 'Within trading hours' : 'Outside trading hours',
    };
  }

  private checkNoTradingDays(): RiskCheck {
    const today = new Date().toISOString().split('T')[0];
    const isNoTradingDay = this.limits.noTradingDays.includes(today);

    return {
      name: 'No Trading Day',
      description: 'Check if today is a no-trading day',
      passed: !isNoTradingDay,
      severity: 'error',
      message: isNoTradingDay ? `${today} is marked as no-trading day` : 'Trading allowed today',
    };
  }

  // ================================
  // HELPER METHODS
  // ================================

  private createEmptyDailyStats(): DailyTradingStats {
    return {
      date: new Date().toISOString().split('T')[0],
      tradesCount: 0,
      totalVolume: 0,
      realizedPnL: 0,
      unrealizedPnL: 0,
      peakBalance: 0,
      currentBalance: 0,
    };
  }

  private ensureDailyStatsValid(): void {
    const today = new Date().toISOString().split('T')[0];
    if (this.dailyStats.date !== today) {
      this.resetDailyStats();
    }
  }

  private calculateOverallRisk(checks: RiskCheck[]): 'low' | 'medium' | 'high' | 'critical' {
    const hasCritical = checks.some((c) => !c.passed && c.severity === 'critical');
    const hasError = checks.some((c) => !c.passed && c.severity === 'error');
    const hasWarning = checks.some((c) => !c.passed && c.severity === 'warning');

    if (hasCritical) return 'critical';
    if (hasError) return 'high';
    if (hasWarning) return 'medium';
    return 'low';
  }
}

// Export singleton
export const RiskManager = new RiskManagerClass();
export { RiskManagerClass, DEFAULT_LIMITS };
