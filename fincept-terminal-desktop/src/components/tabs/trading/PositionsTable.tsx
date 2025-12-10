// File: src/components/tabs/trading/PositionsTable.tsx
// Display open and closed positions

import React from 'react';
import type { Position } from 'ccxt';

interface PositionsTableProps {
  positions: Position[];
  onClosePosition?: (positionId: string) => void;
  isLoading?: boolean;
}

export function PositionsTable({ positions, onClosePosition, isLoading }: PositionsTableProps) {
  const openPositions = positions.filter((p: any) => p.side);  // CCXT positions don't have status
  const closedPositions: Position[] = [];  // Closed positions not tracked in CCXT format

  return (
    <div className="bg-[#0d0d0d] border-t border-gray-800">
      <div className="px-4 py-2 border-b border-gray-800">
        <h3 className="text-sm font-bold text-gray-400">POSITIONS</h3>
      </div>

      {/* Open Positions */}
      <div className="max-h-64 overflow-y-auto">
        {openPositions.length === 0 ? (
          <div className="p-4 text-center text-gray-500 text-xs">
            No open positions
          </div>
        ) : (
          <table className="w-full text-xs font-mono">
            <thead className="bg-[#1a1a1a] text-gray-500 sticky top-0">
              <tr>
                <th className="px-3 py-2 text-left">SYMBOL</th>
                <th className="px-3 py-2 text-left">SIDE</th>
                <th className="px-3 py-2 text-right">QTY</th>
                <th className="px-3 py-2 text-right">ENTRY</th>
                <th className="px-3 py-2 text-right">CURRENT</th>
                <th className="px-3 py-2 text-right">P&L</th>
                <th className="px-3 py-2 text-center">ACTION</th>
              </tr>
            </thead>
            <tbody>
              {openPositions.map(position => (
                <PositionRow
                  key={position.id}
                  position={position}
                  onClose={onClosePosition}
                  isLoading={isLoading}
                />
              ))}
            </tbody>
          </table>
        )}
      </div>

      {/* Closed Positions (Collapsible) */}
      {closedPositions.length > 0 && (
        <details className="border-t border-gray-800">
          <summary className="px-4 py-2 cursor-pointer text-xs text-gray-500 hover:bg-gray-800/50">
            Closed Positions ({closedPositions.length})
          </summary>
          <div className="max-h-48 overflow-y-auto">
            <table className="w-full text-xs font-mono">
              <thead className="bg-[#1a1a1a] text-gray-500">
                <tr>
                  <th className="px-3 py-2 text-left">SYMBOL</th>
                  <th className="px-3 py-2 text-left">SIDE</th>
                  <th className="px-3 py-2 text-right">QTY</th>
                  <th className="px-3 py-2 text-right">ENTRY</th>
                  <th className="px-3 py-2 text-right">REALIZED P&L</th>
                  <th className="px-3 py-2 text-right">CLOSED</th>
                </tr>
              </thead>
              <tbody>
                {closedPositions.map(position => (
                  <ClosedPositionRow key={position.id} position={position} />
                ))}
              </tbody>
            </table>
          </div>
        </details>
      )}
    </div>
  );
}

interface PositionRowProps {
  position: Position;
  onClose?: (positionId: string) => void;
  isLoading?: boolean;
}

function PositionRow({ position, onClose, isLoading }: PositionRowProps) {
  const pnl = position.unrealizedPnl || 0;
  const pnlColor = pnl >= 0 ? 'text-green-400' : 'text-red-400';

  // Calculate P&L percentage based on initial margin
  const entryPrice = position.entryPrice || 0;
  const currentPrice = position.markPrice || position.entryPrice || 0;
  const quantity = position.contracts || 0;
  const initialValue = entryPrice * quantity;
  const pnlPercent = initialValue > 0
    ? (pnl / initialValue) * 100
    : 0;

  // Calculate price movement
  const priceChange = currentPrice - entryPrice;
  const priceChangePercent = entryPrice > 0 ? (priceChange / entryPrice) * 100 : 0;

  return (
    <tr className="border-b border-gray-800 hover:bg-gray-800/50">
      <td className="px-3 py-2 text-white font-semibold">{position.symbol}</td>
      <td className="px-3 py-2">
        <span className={`px-2 py-0.5 rounded text-[10px] font-semibold ${
          position.side === 'long' ? 'bg-green-900/30 text-green-400' : 'bg-red-900/30 text-red-400'
        }`}>
          {(position.side || 'unknown').toUpperCase()}
        </span>
      </td>
      <td className="px-3 py-2 text-right text-gray-300">{quantity.toFixed(4)}</td>
      <td className="px-3 py-2 text-right text-gray-300">${entryPrice.toFixed(2)}</td>
      <td className="px-3 py-2 text-right">
        <div className="flex flex-col items-end">
          <span className="text-white">${currentPrice.toFixed(2)}</span>
          <span className={`text-[9px] ${priceChange >= 0 ? 'text-green-400' : 'text-red-400'}`}>
            {priceChange >= 0 ? '+' : ''}{priceChange.toFixed(2)} ({priceChangePercent >= 0 ? '+' : ''}{priceChangePercent.toFixed(2)}%)
          </span>
        </div>
      </td>
      <td className={`px-3 py-2 text-right font-semibold ${pnlColor}`}>
        <div className="flex flex-col items-end">
          <span className="text-sm">{pnl >= 0 ? '+' : ''}${Math.abs(pnl).toFixed(2)}</span>
          <span className="text-[9px] opacity-75">
            {pnlPercent >= 0 ? '+' : ''}{pnlPercent.toFixed(2)}%
          </span>
        </div>
      </td>
      <td className="px-3 py-2 text-center">
        {onClose && (
          <button
            onClick={() => onClose(position.id || '')}
            disabled={isLoading}
            className="px-2 py-1 bg-red-600 hover:bg-red-700 text-white rounded text-[10px] font-semibold disabled:opacity-50 disabled:cursor-not-allowed"
          >
            CLOSE
          </button>
        )}
      </td>
    </tr>
  );
}

function ClosedPositionRow({ position }: { position: Position }) {
  const pnl = position.realizedPnl || 0;
  const pnlColor = pnl >= 0 ? 'text-green-400' : 'text-red-400';
  const closedDate = position.datetime
    ? new Date(position.datetime).toLocaleDateString()
    : '-';

  return (
    <tr className="border-b border-gray-800 hover:bg-gray-800/50 text-gray-500">
      <td className="px-3 py-2">{position.symbol}</td>
      <td className="px-3 py-2">
        <span className={`px-2 py-0.5 rounded text-[10px] ${
          position.side === 'long' ? 'bg-green-900/20 text-green-600' : 'bg-red-900/20 text-red-600'
        }`}>
          {(position.side || 'unknown').toUpperCase()}
        </span>
      </td>
      <td className="px-3 py-2 text-right">{(position.contracts || 0).toFixed(4)}</td>
      <td className="px-3 py-2 text-right">${(position.entryPrice || 0).toFixed(2)}</td>
      <td className={`px-3 py-2 text-right font-semibold ${pnlColor}`}>
        {pnl >= 0 ? '+' : ''}{pnl.toFixed(2)}
      </td>
      <td className="px-3 py-2 text-right">{closedDate}</td>
    </tr>
  );
}
