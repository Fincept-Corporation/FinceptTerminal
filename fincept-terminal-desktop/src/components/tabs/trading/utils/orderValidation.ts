/**
 * Order Validation Utilities
 * Validates orders before submission
 */

import type { UnifiedOrderRequest, ExchangeOrderCapabilities } from '../types';

export interface ValidationResult {
  valid: boolean;
  errors: string[];
}

/**
 * Validate order against exchange capabilities
 */
export function validateOrder(
  order: UnifiedOrderRequest,
  capabilities: ExchangeOrderCapabilities,
  balance?: number,
  currentPrice?: number
): ValidationResult {
  const errors: string[] = [];

  // Check basic fields
  if (!order.symbol) {
    errors.push('Symbol is required');
  }

  if (!order.quantity || order.quantity <= 0) {
    errors.push('Quantity must be greater than 0');
  }

  // Validate order type
  if (!capabilities.supportedOrderTypes.includes(order.type)) {
    errors.push(`Order type "${order.type}" is not supported by this exchange`);
  }

  // Validate limit orders
  if (order.type === 'limit' && (!order.price || order.price <= 0)) {
    errors.push('Limit orders require a valid price');
  }

  // Validate stop orders
  if ((order.type === 'stop' || order.type === 'stop_limit') && !order.stopPrice) {
    errors.push('Stop orders require a stop price');
  }

  if (order.type === 'stop_limit' && !order.price) {
    errors.push('Stop-limit orders require both stop price and limit price');
  }

  // Validate trailing stop
  if (order.type === 'trailing_stop') {
    if (!capabilities.supportsTrailingStop) {
      errors.push('This exchange does not support trailing stop orders');
    }
    if (!order.trailingPercent && !order.trailingAmount) {
      errors.push('Trailing stop orders require trailing percent or trailing amount');
    }
  }

  // Validate stop loss / take profit
  if (order.stopLossPrice && !capabilities.supportsStopLoss) {
    errors.push('This exchange does not support stop loss orders');
  }

  if (order.takeProfitPrice && !capabilities.supportsTakeProfit) {
    errors.push('This exchange does not support take profit orders');
  }

  // Validate leverage
  if (order.leverage) {
    if (!capabilities.supportsLeverage) {
      errors.push('This exchange does not support leverage');
    } else if (capabilities.maxLeverage && order.leverage > capabilities.maxLeverage) {
      errors.push(`Maximum leverage for this exchange is ${capabilities.maxLeverage}x`);
    }
  }

  // Validate iceberg
  if (order.icebergQty && !capabilities.supportsIceberg) {
    errors.push('This exchange does not support iceberg orders');
  }

  if (order.icebergQty && order.icebergQty >= order.quantity) {
    errors.push('Iceberg visible quantity must be less than total quantity');
  }

  // Validate post-only
  if (order.postOnly && order.type === 'market') {
    errors.push('Market orders cannot be post-only');
  }

  // Validate reduce-only
  if (order.reduceOnly && !capabilities.supportsReduceOnly) {
    errors.push('This exchange does not support reduce-only orders');
  }

  // Validate balance (if provided)
  if (balance !== undefined && currentPrice) {
    const orderCost = calculateOrderCost(order, currentPrice);
    if (order.side === 'buy' && orderCost > balance) {
      errors.push(`Insufficient balance. Required: $${orderCost.toFixed(2)}, Available: $${balance.toFixed(2)}`);
    }
  }

  return {
    valid: errors.length === 0,
    errors,
  };
}

/**
 * Calculate order cost
 */
export function calculateOrderCost(order: UnifiedOrderRequest, currentPrice: number): number {
  const price = order.price || currentPrice;
  const quantity = order.quantity;

  if (order.leverage && order.leverage > 1) {
    return (price * quantity) / order.leverage;
  }

  return price * quantity;
}

/**
 * Validate order quantity against min/max constraints
 */
export function validateQuantity(
  quantity: number,
  minQty?: number,
  maxQty?: number,
  stepSize?: number
): ValidationResult {
  const errors: string[] = [];

  if (minQty && quantity < minQty) {
    errors.push(`Quantity must be at least ${minQty}`);
  }

  if (maxQty && quantity > maxQty) {
    errors.push(`Quantity cannot exceed ${maxQty}`);
  }

  if (stepSize && quantity % stepSize !== 0) {
    errors.push(`Quantity must be a multiple of ${stepSize}`);
  }

  return {
    valid: errors.length === 0,
    errors,
  };
}

/**
 * Validate price against min/max constraints
 */
export function validatePrice(
  price: number,
  minPrice?: number,
  maxPrice?: number,
  tickSize?: number
): ValidationResult {
  const errors: string[] = [];

  if (minPrice && price < minPrice) {
    errors.push(`Price must be at least ${minPrice}`);
  }

  if (maxPrice && price > maxPrice) {
    errors.push(`Price cannot exceed ${maxPrice}`);
  }

  if (tickSize && !isValidPriceStep(price, tickSize)) {
    errors.push(`Price must be a multiple of ${tickSize}`);
  }

  return {
    valid: errors.length === 0,
    errors,
  };
}

/**
 * Check if price is valid for given tick size
 */
function isValidPriceStep(price: number, tickSize: number): boolean {
  const remainder = price % tickSize;
  return Math.abs(remainder) < 0.0000001 || Math.abs(remainder - tickSize) < 0.0000001;
}

/**
 * Format validation errors for display
 */
export function formatValidationErrors(errors: string[]): string {
  if (errors.length === 0) {
    return '';
  }

  if (errors.length === 1) {
    return errors[0];
  }

  return errors.map((error, index) => `${index + 1}. ${error}`).join('\n');
}
