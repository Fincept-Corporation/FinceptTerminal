/**
 * Grid Trading Engine
 *
 * Core grid trading logic that is broker-agnostic.
 * Calculates grid levels, manages state, and coordinates order placement.
 */

import type {
  GridConfig,
  GridState,
  GridLevel,
  GridType,
  GridCalculation,
  GridLevelStatus,
  GridStatus,
} from './types';

// ============================================================================
// GRID CALCULATION UTILITIES
// ============================================================================

/**
 * Calculate grid price levels based on configuration
 */
export function calculateGridLevels(config: GridConfig): number[] {
  const { upperPrice, lowerPrice, gridLevels, gridType } = config;

  if (upperPrice <= lowerPrice) {
    throw new Error('Upper price must be greater than lower price');
  }

  if (gridLevels < 2) {
    throw new Error('Grid levels must be at least 2');
  }

  const levels: number[] = [];

  if (gridType === 'arithmetic') {
    // Equal price gaps
    const step = (upperPrice - lowerPrice) / (gridLevels - 1);
    for (let i = 0; i < gridLevels; i++) {
      levels.push(lowerPrice + step * i);
    }
  } else {
    // Geometric - equal percentage gaps
    const ratio = Math.pow(upperPrice / lowerPrice, 1 / (gridLevels - 1));
    for (let i = 0; i < gridLevels; i++) {
      levels.push(lowerPrice * Math.pow(ratio, i));
    }
  }

  return levels.map(price => roundPrice(price, getDecimalPlaces(lowerPrice)));
}

/**
 * Calculate complete grid details
 */
export function calculateGridDetails(config: GridConfig): GridCalculation {
  const levels = calculateGridLevels(config);
  const { gridType, totalInvestment, gridLevels } = config;

  // Calculate grid spacing
  let gridSpacing: number;
  if (gridType === 'arithmetic') {
    gridSpacing = levels[1] - levels[0];
  } else {
    gridSpacing = ((levels[1] / levels[0]) - 1) * 100; // percentage
  }

  // Calculate quantity per level
  const avgPrice = (config.upperPrice + config.lowerPrice) / 2;
  const quantityPerLevel = config.quantityPerGrid || (totalInvestment / gridLevels / avgPrice);

  // Calculate totals
  const totalQuantity = quantityPerLevel * gridLevels;

  // Calculate potential profit (sum of all grid profits)
  let potentialProfit = 0;
  for (let i = 0; i < levels.length - 1; i++) {
    // Each completed grid cycle generates profit = (sell_price - buy_price) * quantity
    potentialProfit += (levels[i + 1] - levels[i]) * quantityPerLevel;
  }

  // Break-even is the weighted average of all buy levels
  const totalBuyValue = levels.slice(0, Math.ceil(levels.length / 2))
    .reduce((sum, price) => sum + price * quantityPerLevel, 0);
  const totalBuyQty = Math.ceil(levels.length / 2) * quantityPerLevel;
  const breakEvenPrice = totalBuyValue / totalBuyQty;

  return {
    levels,
    gridSpacing,
    quantityPerLevel: roundQuantity(quantityPerLevel, 8),
    totalQuantity: roundQuantity(totalQuantity, 8),
    totalInvestment,
    breakEvenPrice: roundPrice(breakEvenPrice, getDecimalPlaces(config.lowerPrice)),
    potentialProfit: roundPrice(potentialProfit, 2),
  };
}

/**
 * Initialize grid levels based on current price
 */
export function initializeGridLevels(
  config: GridConfig,
  currentPrice: number,
  calculation: GridCalculation
): GridLevel[] {
  const { levels, quantityPerLevel } = calculation;
  const { direction } = config;

  return levels.map((price, index) => {
    // Determine side based on direction and price relative to current
    let side: 'buy' | 'sell';

    if (direction === 'neutral') {
      // Neutral: buy below current price, sell above
      side = price < currentPrice ? 'buy' : 'sell';
    } else if (direction === 'long') {
      // Long bias: more buys, fewer sells
      side = price < currentPrice ? 'buy' : 'sell';
    } else {
      // Short bias: more sells, fewer buys
      side = price > currentPrice ? 'sell' : 'buy';
    }

    return {
      levelIndex: index,
      price,
      side,
      quantity: quantityPerLevel,
      status: 'pending' as GridLevelStatus,
    };
  });
}

/**
 * Create initial grid state
 */
export function createInitialGridState(
  config: GridConfig,
  currentPrice: number
): GridState {
  const calculation = calculateGridDetails(config);
  const levels = initializeGridLevels(config, currentPrice, calculation);

  return {
    config,
    levels,
    status: 'draft',
    currentPrice,

    // Statistics
    totalBuys: 0,
    totalSells: 0,
    realizedPnl: 0,
    unrealizedPnl: 0,
    totalPnl: 0,
    gridProfit: 0,

    // Position
    currentPosition: 0,
    averageEntryPrice: 0,

    // Timestamps
    createdAt: Date.now(),
    lastUpdatedAt: Date.now(),

    // Error tracking
    errorCount: 0,
  };
}

// ============================================================================
// GRID STATE UPDATES
// ============================================================================

/**
 * Update grid state when price changes
 */
export function updateGridOnPriceChange(
  grid: GridState,
  newPrice: number
): GridState {
  // Calculate unrealized P&L
  let unrealizedPnl = 0;
  if (grid.currentPosition !== 0 && grid.averageEntryPrice > 0) {
    unrealizedPnl = (newPrice - grid.averageEntryPrice) * grid.currentPosition;
  }

  return {
    ...grid,
    currentPrice: newPrice,
    unrealizedPnl,
    totalPnl: grid.realizedPnl + unrealizedPnl,
    lastUpdatedAt: Date.now(),
  };
}

/**
 * Update grid level status
 */
export function updateGridLevelStatus(
  grid: GridState,
  levelIndex: number,
  status: GridLevelStatus,
  orderId?: string
): GridState {
  const levels = [...grid.levels];
  levels[levelIndex] = {
    ...levels[levelIndex],
    status,
    orderId,
    createdAt: status === 'open' ? Date.now() : levels[levelIndex].createdAt,
  };

  return {
    ...grid,
    levels,
    lastUpdatedAt: Date.now(),
  };
}

/**
 * Update grid when an order is filled
 */
export function updateGridOnFill(
  grid: GridState,
  levelIndex: number,
  filledQuantity: number,
  filledPrice: number
): GridState {
  const levels = [...grid.levels];
  const level = levels[levelIndex];

  // Update level
  levels[levelIndex] = {
    ...level,
    status: 'filled',
    filledQuantity,
    filledPrice,
    filledAt: Date.now(),
  };

  // Update position tracking
  let newPosition = grid.currentPosition;
  let newTotalBuys = grid.totalBuys;
  let newTotalSells = grid.totalSells;
  let newRealizedPnl = grid.realizedPnl;
  let newGridProfit = grid.gridProfit;
  let newAvgEntry = grid.averageEntryPrice;

  if (level.side === 'buy') {
    // Buy order filled
    const oldPosition = newPosition;
    newPosition += filledQuantity;
    newTotalBuys += 1;

    // Update average entry price
    if (newPosition > 0) {
      const oldValue = oldPosition > 0 ? oldPosition * newAvgEntry : 0;
      const newValue = filledQuantity * filledPrice;
      newAvgEntry = (oldValue + newValue) / newPosition;
    }
  } else {
    // Sell order filled
    newPosition -= filledQuantity;
    newTotalSells += 1;

    // Calculate realized P&L if closing position
    if (grid.currentPosition > 0 && grid.averageEntryPrice > 0) {
      const pnl = (filledPrice - grid.averageEntryPrice) * filledQuantity;
      newRealizedPnl += pnl;
      newGridProfit += pnl;
    }
  }

  // Calculate unrealized P&L
  let unrealizedPnl = 0;
  if (newPosition !== 0 && newAvgEntry > 0) {
    unrealizedPnl = (grid.currentPrice - newAvgEntry) * newPosition;
  }

  // Flip the grid level to opposite side for next trade
  const oppositePrice = findNextOppositeLevel(levels, levelIndex, grid.config);
  if (oppositePrice) {
    levels[levelIndex] = {
      ...levels[levelIndex],
      side: level.side === 'buy' ? 'sell' : 'buy',
      price: oppositePrice,
      status: 'pending',
      orderId: undefined,
      filledQuantity: undefined,
      filledPrice: undefined,
      filledAt: undefined,
    };
  }

  return {
    ...grid,
    levels,
    currentPosition: newPosition,
    averageEntryPrice: newAvgEntry,
    totalBuys: newTotalBuys,
    totalSells: newTotalSells,
    realizedPnl: newRealizedPnl,
    unrealizedPnl,
    totalPnl: newRealizedPnl + unrealizedPnl,
    gridProfit: newGridProfit,
    lastUpdatedAt: Date.now(),
  };
}

/**
 * Find the next opposite level price for a filled order
 */
function findNextOppositeLevel(
  levels: GridLevel[],
  filledIndex: number,
  config: GridConfig
): number | null {
  const calculation = calculateGridDetails(config);
  const allPrices = calculation.levels;

  // If it was a buy, look for the next sell level above
  // If it was a sell, look for the next buy level below
  const filledLevel = levels[filledIndex];

  if (filledLevel.side === 'buy') {
    // Find the next price level above
    const nextIndex = filledIndex + 1;
    if (nextIndex < allPrices.length) {
      return allPrices[nextIndex];
    }
  } else {
    // Find the next price level below
    const nextIndex = filledIndex - 1;
    if (nextIndex >= 0) {
      return allPrices[nextIndex];
    }
  }

  return null;
}

/**
 * Get pending levels that need orders placed
 */
export function getPendingLevels(grid: GridState): GridLevel[] {
  return grid.levels.filter(level => level.status === 'pending');
}

/**
 * Get open levels (orders placed, waiting to fill)
 */
export function getOpenLevels(grid: GridState): GridLevel[] {
  return grid.levels.filter(level => level.status === 'open');
}

/**
 * Check if grid should be stopped (stop loss or take profit hit)
 */
export function checkGridStopConditions(grid: GridState): {
  shouldStop: boolean;
  reason?: string;
} {
  const { config, currentPrice } = grid;

  if (config.stopLoss && currentPrice <= config.stopLoss) {
    return { shouldStop: true, reason: 'Stop loss triggered' };
  }

  if (config.takeProfit && currentPrice >= config.takeProfit) {
    return { shouldStop: true, reason: 'Take profit triggered' };
  }

  return { shouldStop: false };
}

/**
 * Validate grid configuration
 */
export function validateGridConfig(config: Partial<GridConfig>): string[] {
  const errors: string[] = [];

  if (!config.symbol) {
    errors.push('Symbol is required');
  }

  if (!config.upperPrice || config.upperPrice <= 0) {
    errors.push('Upper price must be greater than 0');
  }

  if (!config.lowerPrice || config.lowerPrice <= 0) {
    errors.push('Lower price must be greater than 0');
  }

  if (config.upperPrice && config.lowerPrice && config.upperPrice <= config.lowerPrice) {
    errors.push('Upper price must be greater than lower price');
  }

  if (!config.gridLevels || config.gridLevels < 2) {
    errors.push('Grid levels must be at least 2');
  }

  if (config.gridLevels && config.gridLevels > 200) {
    errors.push('Grid levels cannot exceed 200');
  }

  if (!config.totalInvestment || config.totalInvestment <= 0) {
    errors.push('Total investment must be greater than 0');
  }

  if (!config.brokerId) {
    errors.push('Broker ID is required');
  }

  if (config.stopLoss && config.lowerPrice && config.stopLoss >= config.lowerPrice) {
    errors.push('Stop loss must be below lower price');
  }

  if (config.takeProfit && config.upperPrice && config.takeProfit <= config.upperPrice) {
    errors.push('Take profit must be above upper price');
  }

  return errors;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Get decimal places from a price
 */
function getDecimalPlaces(price: number): number {
  const priceStr = price.toString();
  const decimalIndex = priceStr.indexOf('.');
  if (decimalIndex === -1) return 0;
  return priceStr.length - decimalIndex - 1;
}

/**
 * Round price to specified decimal places
 */
function roundPrice(price: number, decimals: number): number {
  const factor = Math.pow(10, decimals);
  return Math.round(price * factor) / factor;
}

/**
 * Round quantity to specified decimal places
 */
function roundQuantity(quantity: number, decimals: number): number {
  const factor = Math.pow(10, decimals);
  return Math.round(quantity * factor) / factor;
}

/**
 * Generate a unique grid ID
 */
export function generateGridId(): string {
  return `grid_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
}

/**
 * Calculate profit per grid (expected profit when one buy-sell cycle completes)
 */
export function calculateProfitPerGrid(config: GridConfig): number {
  const { upperPrice, lowerPrice, gridLevels, gridType, quantityPerGrid } = config;

  let avgSpread: number;
  if (gridType === 'arithmetic') {
    avgSpread = (upperPrice - lowerPrice) / (gridLevels - 1);
  } else {
    const ratio = Math.pow(upperPrice / lowerPrice, 1 / (gridLevels - 1));
    const avgPrice = (upperPrice + lowerPrice) / 2;
    avgSpread = avgPrice * (ratio - 1);
  }

  return avgSpread * quantityPerGrid;
}

/**
 * Estimate annual return based on grid parameters and trade frequency
 */
export function estimateAnnualReturn(
  config: GridConfig,
  tradesPerDay: number
): number {
  const profitPerTrade = calculateProfitPerGrid(config);
  const dailyProfit = profitPerTrade * tradesPerDay;
  const annualProfit = dailyProfit * 365;
  return (annualProfit / config.totalInvestment) * 100;
}
