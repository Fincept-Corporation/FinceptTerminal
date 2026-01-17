/**
 * Funds Panel Component
 *
 * Displays account funds and margin information
 */

import React from 'react';
import { RefreshCw, Wallet, TrendingUp, TrendingDown, DollarSign, Shield, PieChart } from 'lucide-react';
import { useStockTradingData, useStockBrokerSelection } from '@/contexts/StockBrokerContext';

interface FundsPanelProps {
  compact?: boolean;
}

export function FundsPanel({ compact = false }: FundsPanelProps) {
  const { funds, refreshFunds, isRefreshing, isReady } = useStockTradingData();
  const { metadata } = useStockBrokerSelection();

  if (!isReady) {
    return (
      <div className="p-4 bg-[#1A1A1A] rounded-lg border border-[#2A2A2A]">
        <p className="text-sm text-gray-500 text-center">Connect to view funds</p>
      </div>
    );
  }

  if (!funds) {
    return (
      <div className="p-4 bg-[#1A1A1A] rounded-lg border border-[#2A2A2A]">
        <div className="flex items-center justify-between mb-3">
          <h3 className="text-sm font-medium text-white">Funds</h3>
          <button
            onClick={() => refreshFunds()}
            disabled={isRefreshing}
            className="p-1 text-gray-400 hover:text-white"
          >
            <RefreshCw className={`w-4 h-4 ${isRefreshing ? 'animate-spin' : ''}`} />
          </button>
        </div>
        <p className="text-sm text-gray-500 text-center">Loading funds...</p>
      </div>
    );
  }

  // Calculate utilization percentage
  const utilizationPercent = funds.totalBalance > 0
    ? (funds.usedMargin / funds.totalBalance) * 100
    : 0;

  // Currency symbol
  const currency = metadata?.currency || 'INR';
  const formatCurrency = (value: number) =>
    value.toLocaleString('en-IN', { style: 'currency', currency, minimumFractionDigits: 2 });

  if (compact) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        gap: '8px',
        width: '100%',
        overflow: 'hidden'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', minWidth: 0, flex: 1 }}>
          <Wallet size={12} color="#FF8800" style={{ flexShrink: 0 }} />
          <span style={{ fontSize: '9px', color: '#787878', whiteSpace: 'nowrap' }}>Available:</span>
          <span style={{ fontSize: '10px', fontFamily: 'monospace', color: '#00D66F', fontWeight: 600, whiteSpace: 'nowrap' }}>
            ₹{funds.availableCash.toLocaleString('en-IN', { minimumFractionDigits: 0, maximumFractionDigits: 0 })}
          </span>
        </div>
        <div style={{ width: '1px', height: '12px', backgroundColor: '#2A2A2A', flexShrink: 0 }} />
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', minWidth: 0 }}>
          <span style={{ fontSize: '9px', color: '#787878', whiteSpace: 'nowrap' }}>Margin:</span>
          <span style={{ fontSize: '10px', fontFamily: 'monospace', color: '#FF8800', fontWeight: 600, whiteSpace: 'nowrap' }}>
            ₹{funds.usedMargin.toLocaleString('en-IN', { minimumFractionDigits: 0, maximumFractionDigits: 0 })}
          </span>
        </div>
        <button
          onClick={() => refreshFunds()}
          disabled={isRefreshing}
          style={{
            padding: '2px',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            color: '#787878',
            flexShrink: 0
          }}
        >
          <RefreshCw size={10} className={isRefreshing ? 'animate-spin' : ''} />
        </button>
      </div>
    );
  }

  return (
    <div className="bg-[#1A1A1A] rounded-lg border border-[#2A2A2A] overflow-hidden">
      {/* Header */}
      <div className="px-4 py-3 border-b border-[#2A2A2A] flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Wallet className="w-4 h-4 text-[#FF8800]" />
          <h3 className="text-sm font-medium text-white">Funds & Margin</h3>
        </div>
        <button
          onClick={() => refreshFunds()}
          disabled={isRefreshing}
          className="p-1.5 text-gray-400 hover:text-white hover:bg-[#2A2A2A] rounded-md transition-colors"
        >
          <RefreshCw className={`w-4 h-4 ${isRefreshing ? 'animate-spin' : ''}`} />
        </button>
      </div>

      <div className="p-4 space-y-4">
        {/* Main Balance */}
        <div className="grid grid-cols-2 gap-4">
          <div className="p-3 bg-[#0F0F0F] rounded-lg">
            <div className="flex items-center gap-2 mb-1">
              <DollarSign className="w-3.5 h-3.5 text-gray-500" />
              <span className="text-xs text-gray-400">Total Balance</span>
            </div>
            <p className="text-lg font-mono text-white">{formatCurrency(funds.totalBalance)}</p>
          </div>

          <div className="p-3 bg-[#0F0F0F] rounded-lg">
            <div className="flex items-center gap-2 mb-1">
              <Wallet className="w-3.5 h-3.5 text-[#00D66F]" />
              <span className="text-xs text-gray-400">Available Cash</span>
            </div>
            <p className="text-lg font-mono text-[#00D66F]">{formatCurrency(funds.availableCash)}</p>
          </div>
        </div>

        {/* Margin Details */}
        <div className="space-y-3">
          <div className="flex items-center justify-between text-sm">
            <span className="text-gray-400">Used Margin</span>
            <span className="font-mono text-[#FF8800]">{formatCurrency(funds.usedMargin)}</span>
          </div>

          <div className="flex items-center justify-between text-sm">
            <span className="text-gray-400">Available Margin</span>
            <span className="font-mono text-white">{formatCurrency(funds.availableMargin)}</span>
          </div>

          {/* Margin Utilization Bar */}
          <div>
            <div className="flex items-center justify-between text-xs mb-1">
              <span className="text-gray-500">Margin Utilization</span>
              <span className={`${
                utilizationPercent > 80 ? 'text-[#FF3B3B]' :
                utilizationPercent > 50 ? 'text-[#FF8800]' :
                'text-[#00D66F]'
              }`}>
                {utilizationPercent.toFixed(1)}%
              </span>
            </div>
            <div className="h-2 bg-[#0F0F0F] rounded-full overflow-hidden">
              <div
                className={`h-full transition-all duration-300 ${
                  utilizationPercent > 80 ? 'bg-[#FF3B3B]' :
                  utilizationPercent > 50 ? 'bg-[#FF8800]' :
                  'bg-[#00D66F]'
                }`}
                style={{ width: `${Math.min(utilizationPercent, 100)}%` }}
              />
            </div>
          </div>
        </div>

        {/* Segment-wise (if available) */}
        {(funds.equityAvailable !== undefined || funds.commodityAvailable !== undefined) && (
          <div className="pt-3 border-t border-[#2A2A2A] space-y-2">
            <p className="text-xs text-gray-500 font-medium mb-2">Segment Breakdown</p>
            {funds.equityAvailable !== undefined && (
              <div className="flex items-center justify-between text-sm">
                <span className="text-gray-400">Equity</span>
                <span className="font-mono text-white">{formatCurrency(funds.equityAvailable)}</span>
              </div>
            )}
            {funds.commodityAvailable !== undefined && (
              <div className="flex items-center justify-between text-sm">
                <span className="text-gray-400">Commodity</span>
                <span className="font-mono text-white">{formatCurrency(funds.commodityAvailable)}</span>
              </div>
            )}
          </div>
        )}

        {/* Collateral (if available) */}
        {funds.collateral !== undefined && funds.collateral > 0 && (
          <div className="flex items-center justify-between text-sm pt-3 border-t border-[#2A2A2A]">
            <div className="flex items-center gap-2">
              <Shield className="w-3.5 h-3.5 text-blue-400" />
              <span className="text-gray-400">Collateral</span>
            </div>
            <span className="font-mono text-blue-400">{formatCurrency(funds.collateral)}</span>
          </div>
        )}

        {/* P&L (if available) */}
        {(funds.unrealizedPnl !== undefined || funds.realizedPnl !== undefined) && (
          <div className="pt-3 border-t border-[#2A2A2A] space-y-2">
            {funds.unrealizedPnl !== undefined && (
              <div className="flex items-center justify-between text-sm">
                <div className="flex items-center gap-2">
                  <PieChart className="w-3.5 h-3.5 text-gray-500" />
                  <span className="text-gray-400">Unrealized P&L</span>
                </div>
                <span className={`font-mono ${funds.unrealizedPnl >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}`}>
                  {funds.unrealizedPnl >= 0 ? '+' : ''}{formatCurrency(funds.unrealizedPnl)}
                </span>
              </div>
            )}
            {funds.realizedPnl !== undefined && (
              <div className="flex items-center justify-between text-sm">
                <div className="flex items-center gap-2">
                  {funds.realizedPnl >= 0 ? (
                    <TrendingUp className="w-3.5 h-3.5 text-[#00D66F]" />
                  ) : (
                    <TrendingDown className="w-3.5 h-3.5 text-[#FF3B3B]" />
                  )}
                  <span className="text-gray-400">Realized P&L</span>
                </div>
                <span className={`font-mono ${funds.realizedPnl >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}`}>
                  {funds.realizedPnl >= 0 ? '+' : ''}{formatCurrency(funds.realizedPnl)}
                </span>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
