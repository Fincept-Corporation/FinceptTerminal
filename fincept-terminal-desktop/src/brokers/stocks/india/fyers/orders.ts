/**
 * Fyers Order Operations
 *
 * Module-level order functions used by FyersAdapter
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  OrderParams,
  OrderResponse,
  ModifyOrderParams,
  Order,
  Trade,
  BulkOperationResult,
  SmartOrderParams,
  MarginRequired,
  Position,
  StockExchange,
} from '../../types';
import { toFyersOrderParams, toFyersModifyParams, fromFyersOrder, fromFyersTrade } from './mapper';

/**
 * Place a new order
 */
export async function fyersPlaceOrder(
  apiKey: string,
  accessToken: string,
  params: OrderParams
): Promise<OrderResponse> {
  try {
    const fyersParams = toFyersOrderParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('fyers_place_order', {
      apiKey,
      accessToken,
      ...fyersParams,
    });

    if (response.success && response.order_id) {
      return {
        success: true,
        orderId: response.order_id,
        message: 'Order placed successfully',
      };
    }

    return {
      success: false,
      message: response.error || 'Order placement failed',
      errorCode: 'ORDER_PLACEMENT_FAILED',
    };
  } catch (error) {
    return {
      success: false,
      message: error instanceof Error ? error.message : 'Failed to place order',
      errorCode: 'ORDER_PLACEMENT_ERROR',
    };
  }
}

/**
 * Modify an existing order
 */
export async function fyersModifyOrder(
  apiKey: string,
  accessToken: string,
  params: ModifyOrderParams
): Promise<OrderResponse> {
  if (!params.orderId) {
    return {
      success: false,
      message: 'Order ID is required',
      errorCode: 'MISSING_ORDER_ID',
    };
  }

  try {
    const fyersParams = toFyersModifyParams(params.orderId, params);

    const response = await invoke<{
      success: boolean;
      data?: unknown;
      error?: string;
    }>('fyers_modify_order', {
      apiKey,
      accessToken,
      ...fyersParams,
    });

    if (response.success) {
      return {
        success: true,
        orderId: params.orderId,
        message: 'Order modified successfully',
      };
    }

    return {
      success: false,
      message: response.error || 'Order modification failed',
      errorCode: 'ORDER_MODIFICATION_FAILED',
    };
  } catch (error) {
    return {
      success: false,
      message: error instanceof Error ? error.message : 'Failed to modify order',
      errorCode: 'ORDER_MODIFICATION_ERROR',
    };
  }
}

/**
 * Cancel an order
 */
export async function fyersCancelOrder(
  apiKey: string,
  accessToken: string,
  orderId: string
): Promise<OrderResponse> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: unknown;
      error?: string;
    }>('fyers_cancel_order', {
      apiKey,
      accessToken,
      orderId,
    });

    if (response.success) {
      return {
        success: true,
        orderId,
        message: 'Order cancelled successfully',
      };
    }

    return {
      success: false,
      message: response.error || 'Order cancellation failed',
      errorCode: 'ORDER_CANCELLATION_FAILED',
    };
  } catch (error) {
    return {
      success: false,
      message: error instanceof Error ? error.message : 'Failed to cancel order',
      errorCode: 'ORDER_CANCELLATION_ERROR',
    };
  }
}

/**
 * Get all orders
 */
export async function fyersGetOrders(
  apiKey: string,
  accessToken: string
): Promise<Order[]> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: unknown[];
      error?: string;
    }>('fyers_get_orders', {
      apiKey,
      accessToken,
    });

    if (response.success && response.data) {
      return response.data.map((order: unknown) =>
        fromFyersOrder(order as Record<string, unknown>)
      );
    }

    return [];
  } catch (error) {
    console.error('Failed to fetch orders:', error);
    return [];
  }
}

/**
 * Get all trades
 */
export async function fyersGetTradeBook(
  apiKey: string,
  accessToken: string
): Promise<Trade[]> {
  try {
    const response = await invoke<{
      success: boolean;
      data?: unknown[];
      error?: string;
    }>('fyers_get_trade_book', {
      apiKey,
      accessToken,
    });

    if (response.success && response.data) {
      return response.data.map((trade: unknown) =>
        fromFyersTrade(trade as Record<string, unknown>)
      );
    }

    return [];
  } catch (error) {
    console.error('Failed to fetch trades:', error);
    return [];
  }
}

/**
 * Cancel all open orders
 */
export async function fyersCancelAllOrders(
  apiKey: string,
  accessToken: string
): Promise<BulkOperationResult> {
  const orders = await fyersGetOrders(apiKey, accessToken);
  const openOrders = orders.filter(o => o.status === 'OPEN' || o.status === 'PENDING');

  const results = await Promise.all(
    openOrders.map(async (order) => {
      try {
        const result = await fyersCancelOrder(apiKey, accessToken, order.orderId);
        return { success: result.success, orderId: order.orderId };
      } catch (error) {
        return { success: false, orderId: order.orderId, error: String(error) };
      }
    })
  );

  const successful = results.filter(r => r.success).length;
  const failed = results.filter(r => !r.success).length;

  return {
    success: failed === 0,
    totalCount: results.length,
    successCount: successful,
    failedCount: failed,
    results,
  };
}

/**
 * Place a smart order (position-aware)
 */
export async function fyersPlaceSmartOrder(
  apiKey: string,
  accessToken: string,
  params: SmartOrderParams,
  getOpenPositionQty: (symbol: string, exchange: StockExchange, productType: string) => Promise<number>
): Promise<OrderResponse> {
  try {
    const currentPosition = await getOpenPositionQty(
      params.symbol,
      params.exchange,
      params.productType
    );

    const targetPosition = params.positionSize || 0;

    let action: 'BUY' | 'SELL';
    let quantity: number;

    if (targetPosition > currentPosition) {
      action = 'BUY';
      quantity = targetPosition - currentPosition;
    } else if (targetPosition < currentPosition) {
      action = 'SELL';
      quantity = currentPosition - targetPosition;
    } else {
      return {
        success: true,
        message: 'Already at target position',
      };
    }

    const orderParams: OrderParams = {
      symbol: params.symbol,
      exchange: params.exchange,
      side: action,
      quantity,
      orderType: params.orderType || 'MARKET',
      productType: params.productType,
      price: params.price,
      triggerPrice: params.triggerPrice,
      validity: params.validity || 'DAY',
    };

    console.log(`[Fyers Smart Order] Current: ${currentPosition}, Target: ${targetPosition}, Action: ${action} ${quantity}`);

    return fyersPlaceOrder(apiKey, accessToken, orderParams);
  } catch (error) {
    return {
      success: false,
      message: error instanceof Error ? error.message : 'Smart order failed',
      errorCode: 'SMART_ORDER_FAILED',
    };
  }
}

/**
 * Calculate margin for a list of orders
 */
export async function fyersCalculateMargin(
  apiKey: string,
  accessToken: string,
  orders: OrderParams[]
): Promise<MarginRequired> {
  try {
    const fyersOrders = orders.map(order => toFyersOrderParams(order));

    const response = await invoke<{
      success: boolean;
      data?: {
        margin_total?: number;
        margin_new_order?: number;
        margin_avail?: number;
      };
      error?: string;
    }>('fyers_calculate_margin', {
      apiKey,
      accessToken,
      orders: fyersOrders,
    });

    if (response.success && response.data) {
      const marginData = response.data;
      return {
        totalMargin: marginData.margin_new_order || marginData.margin_total || 0,
        initialMargin: marginData.margin_total || 0,
      };
    }

    return { totalMargin: 0, initialMargin: 0 };
  } catch (error) {
    console.error('Failed to calculate margin:', error);
    return { totalMargin: 0, initialMargin: 0 };
  }
}

/**
 * Close all active positions
 */
export async function fyersCloseAllPositions(
  apiKey: string,
  accessToken: string,
  positions: Position[]
): Promise<BulkOperationResult> {
  const activePositions = positions.filter(p => p.quantity !== 0);

  if (activePositions.length === 0) {
    return {
      success: true,
      totalCount: 0,
      successCount: 0,
      failedCount: 0,
      results: [],
    };
  }

  const results = await Promise.all(
    activePositions.map(async (position) => {
      try {
        const exitSide: 'BUY' | 'SELL' = position.quantity > 0 ? 'SELL' : 'BUY';
        const exitQuantity = Math.abs(position.quantity);

        const orderParams: OrderParams = {
          symbol: position.symbol,
          exchange: position.exchange,
          side: exitSide,
          quantity: exitQuantity,
          orderType: 'MARKET',
          productType: position.productType,
          validity: 'DAY',
        };

        const result = await fyersPlaceOrder(apiKey, accessToken, orderParams);
        return {
          success: result.success,
          symbol: position.symbol,
          orderId: result.orderId,
          error: result.message
        };
      } catch (error) {
        return {
          success: false,
          symbol: position.symbol,
          error: String(error)
        };
      }
    })
  );

  const successful = results.filter(r => r.success).length;
  const failed = results.filter(r => !r.success).length;

  console.log(`[Fyers] Close All Positions: ${successful} success, ${failed} failed`);

  return {
    success: failed === 0,
    totalCount: results.length,
    successCount: successful,
    failedCount: failed,
    results,
  };
}
