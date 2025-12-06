// File: src/components/tabs/trading/OrdersTable.tsx
// Display all orders (open, filled, cancelled)

import React from 'react';
import type { Order } from 'ccxt';

interface OrdersTableProps {
  orders: Order[];
  onCancelOrder?: (orderId: string) => void;
  isLoading?: boolean;
}

export function OrdersTable({ orders, onCancelOrder, isLoading }: OrdersTableProps) {
  const openOrders = orders.filter((o) => o.status === 'open');
  const filledOrders = orders.filter((o) => o.status === 'closed' || o.status === 'filled');
  const cancelledOrders = orders.filter((o) => o.status === 'canceled' || o.status === 'cancelled');

  return (
    <div className="bg-[#0d0d0d] border-t border-gray-800">
      <div className="px-4 py-2 border-b border-gray-800">
        <h3 className="text-sm font-bold text-gray-400">ORDERS</h3>
      </div>

      {/* Tabs */}
      <div className="flex border-b border-gray-800 text-xs">
        <button className="px-4 py-2 border-b-2 border-orange-600 text-orange-600 font-bold">
          OPEN ({openOrders.length})
        </button>
        <button className="px-4 py-2 text-gray-500 hover:text-gray-300">
          FILLED ({filledOrders.length})
        </button>
        <button className="px-4 py-2 text-gray-500 hover:text-gray-300">
          CANCELLED ({cancelledOrders.length})
        </button>
      </div>

      {/* Open Orders */}
      <div className="max-h-48 overflow-y-auto">
        {openOrders.length === 0 ? (
          <div className="p-4 text-center text-gray-500 text-xs">No open orders</div>
        ) : (
          <table className="w-full text-xs font-mono">
            <thead className="bg-[#1a1a1a] text-gray-500 sticky top-0">
              <tr>
                <th className="px-3 py-2 text-left">TIME</th>
                <th className="px-3 py-2 text-left">SYMBOL</th>
                <th className="px-3 py-2 text-left">TYPE</th>
                <th className="px-3 py-2 text-left">SIDE</th>
                <th className="px-3 py-2 text-right">QTY</th>
                <th className="px-3 py-2 text-right">PRICE</th>
                <th className="px-3 py-2 text-right">FILLED</th>
                <th className="px-3 py-2 text-center">ACTION</th>
              </tr>
            </thead>
            <tbody>
              {openOrders.map((order) => (
                <OrderRow
                  key={order.id}
                  order={order}
                  onCancel={onCancelOrder}
                  isLoading={isLoading}
                />
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

interface OrderRowProps {
  order: Order;
  onCancel?: (orderId: string) => void;
  isLoading?: boolean;
}

function OrderRow({ order, onCancel, isLoading }: OrderRowProps) {
  const time = order.datetime ? new Date(order.datetime).toLocaleTimeString() : '-';
  const fillPercent = order.amount > 0 ? ((order.filled / order.amount) * 100).toFixed(0) : '0';

  return (
    <tr className="border-b border-gray-800 hover:bg-gray-800/50">
      <td className="px-3 py-2 text-gray-400">{time}</td>
      <td className="px-3 py-2 text-white font-semibold">{order.symbol}</td>
      <td className="px-3 py-2">
        <span className="px-2 py-0.5 rounded text-[10px] bg-blue-900/30 text-blue-400 font-semibold uppercase">
          {order.type}
        </span>
      </td>
      <td className="px-3 py-2">
        <span
          className={`px-2 py-0.5 rounded text-[10px] font-semibold ${
            order.side === 'buy'
              ? 'bg-green-900/30 text-green-400'
              : 'bg-red-900/30 text-red-400'
          }`}
        >
          {(order.side || 'unknown').toUpperCase()}
        </span>
      </td>
      <td className="px-3 py-2 text-right text-gray-300">{order.amount.toFixed(4)}</td>
      <td className="px-3 py-2 text-right text-gray-300">
        {order.price ? `$${order.price.toFixed(2)}` : 'MARKET'}
      </td>
      <td className="px-3 py-2 text-right">
        <span className={fillPercent === '100' ? 'text-green-400' : 'text-yellow-400'}>
          {fillPercent}%
        </span>
      </td>
      <td className="px-3 py-2 text-center">
        {onCancel && (
          <button
            onClick={() => onCancel(order.id || '')}
            disabled={isLoading}
            className="px-2 py-1 bg-gray-700 hover:bg-gray-600 text-white rounded text-[10px] font-semibold disabled:opacity-50 disabled:cursor-not-allowed"
          >
            CANCEL
          </button>
        )}
      </td>
    </tr>
  );
}
