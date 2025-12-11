/**
 * Order Execution Hook
 * Unified order placement logic for all exchanges
 */

import { useState, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { UnifiedOrderRequest } from '../types';
import type { OrderParams } from '../../../../brokers/crypto/types';

export interface UseOrderExecutionResult {
  placeOrder: (order: UnifiedOrderRequest) => Promise<any>;
  cancelOrder: (orderId: string, symbol: string) => Promise<void>;
  editOrder: (orderId: string, symbol: string, updates: Partial<UnifiedOrderRequest>) => Promise<any>;
  isPlacing: boolean;
  isCanceling: boolean;
  isEditing: boolean;
  error: Error | null;
}

export function useOrderExecution(): UseOrderExecutionResult {
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [isPlacing, setIsPlacing] = useState(false);
  const [isCanceling, setIsCanceling] = useState(false);
  const [isEditing, setIsEditing] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  /**
   * Place order on active exchange (real or paper)
   */
  const placeOrder = useCallback(
    async (order: UnifiedOrderRequest) => {
      if (!activeAdapter || !activeAdapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      setIsPlacing(true);
      setError(null);

      try {
        // Build CCXT-compatible params
        const params: OrderParams = {};

        // Stop price
        if (order.stopPrice) {
          params.stopPrice = order.stopPrice;
        }

        // Take profit / stop loss
        if (order.takeProfitPrice) {
          params.takeProfitPrice = order.takeProfitPrice;
        }
        if (order.stopLossPrice) {
          params.stopLossPrice = order.stopLossPrice;
        }

        // Trailing stop
        if (order.trailingPercent) {
          params.trailingPercent = order.trailingPercent;
        }
        if (order.trailingAmount) {
          params.trailingAmount = order.trailingAmount;
        }

        // Leverage
        if (order.leverage) {
          params.leverage = order.leverage;
        }

        // Margin mode
        if (order.marginMode) {
          params.marginMode = order.marginMode;
        }

        // Order flags
        if (order.reduceOnly) {
          params.reduceOnly = order.reduceOnly;
        }
        if (order.postOnly) {
          params.postOnly = order.postOnly;
        }
        if (order.timeInForce) {
          params.timeInForce = order.timeInForce;
        }

        // Iceberg
        if (order.icebergQty) {
          params.icebergQty = order.icebergQty;
        }

        // Client order ID
        if (order.clientOrderId) {
          params.clientOrderId = order.clientOrderId;
        }

        // Execute order
        const result = await activeAdapter.createOrder(
          order.symbol,
          order.type,
          order.side,
          order.quantity,
          order.price,
          params
        );

        console.log('[useOrderExecution] Order placed:', result);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useOrderExecution] Order failed:', error);
        setError(error);
        throw error;
      } finally {
        setIsPlacing(false);
      }
    },
    [activeAdapter]
  );

  /**
   * Cancel order
   */
  const cancelOrder = useCallback(
    async (orderId: string, symbol: string) => {
      if (!activeAdapter || !activeAdapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      setIsCanceling(true);
      setError(null);

      try {
        await activeAdapter.cancelOrder(orderId, symbol);
        console.log('[useOrderExecution] Order canceled:', orderId);
      } catch (err) {
        const error = err as Error;
        console.error('[useOrderExecution] Cancel failed:', error);
        setError(error);
        throw error;
      } finally {
        setIsCanceling(false);
      }
    },
    [activeAdapter]
  );

  /**
   * Edit order (if supported)
   */
  const editOrder = useCallback(
    async (orderId: string, symbol: string, updates: Partial<UnifiedOrderRequest>) => {
      if (!activeAdapter || !activeAdapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      // Check if adapter supports editOrder
      if (typeof (activeAdapter as any).editOrder !== 'function') {
        throw new Error('This exchange does not support order editing');
      }

      setIsEditing(true);
      setError(null);

      try {
        const result = await (activeAdapter as any).editOrder(
          orderId,
          symbol,
          updates.type,
          updates.side,
          updates.quantity,
          updates.price,
          {
            stopPrice: updates.stopPrice,
            takeProfitPrice: updates.takeProfitPrice,
            stopLossPrice: updates.stopLossPrice,
          }
        );

        console.log('[useOrderExecution] Order edited:', result);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useOrderExecution] Edit failed:', error);
        setError(error);
        throw error;
      } finally {
        setIsEditing(false);
      }
    },
    [activeAdapter]
  );

  return {
    placeOrder,
    cancelOrder,
    editOrder,
    isPlacing,
    isCanceling,
    isEditing,
    error,
  };
}
