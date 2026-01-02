/**
 * Trading Nodes Index
 * All trading execution and management nodes
 */

export * from './PlaceOrderNode';
export * from './ModifyOrderNode';
export * from './CancelOrderNode';
export * from './GetPositionsNode';
export * from './GetHoldingsNode';
export * from './GetOrdersNode';
export * from './GetBalanceNode';
export * from './ClosePositionNode';

// Re-export for convenience
import { PlaceOrderNode } from './PlaceOrderNode';
import { ModifyOrderNode } from './ModifyOrderNode';
import { CancelOrderNode } from './CancelOrderNode';
import { GetPositionsNode } from './GetPositionsNode';
import { GetHoldingsNode } from './GetHoldingsNode';
import { GetOrdersNode } from './GetOrdersNode';
import { GetBalanceNode } from './GetBalanceNode';
import { ClosePositionNode } from './ClosePositionNode';

export const TradingNodes = {
  PlaceOrderNode,
  ModifyOrderNode,
  CancelOrderNode,
  GetPositionsNode,
  GetHoldingsNode,
  GetOrdersNode,
  GetBalanceNode,
  ClosePositionNode,
};

export const TradingNodeTypes = [
  'placeOrder',
  'modifyOrder',
  'cancelOrder',
  'getPositions',
  'getHoldings',
  'getOrders',
  'getBalance',
  'closePosition',
] as const;

export type TradingNodeType = typeof TradingNodeTypes[number];
