// File: src/components/tabs/trading/PaperTradingPanel.tsx
// Paper trading control panel with toggle and portfolio display

import React, { useState } from 'react';
import { useProviderContext } from '../../../contexts/ProviderContext';
import { paperTradingDatabase } from '../../../paper-trading';

interface PaperTradingPanelProps {
  portfolioId: string | null;
  balance?: number;
  totalPnL?: number;
  onPortfolioChange: (portfolioId: string | null) => void;
  onReset?: () => void;
  isResetting?: boolean;
}

export function PaperTradingPanel({
  portfolioId,
  balance = 0,
  totalPnL = 0,
  onPortfolioChange,
  onReset,
  isResetting
}: PaperTradingPanelProps) {
  const [enabled, setEnabled] = useState(!!portfolioId);
  const [isCreating, setIsCreating] = useState(false);
  const { activeProvider } = useProviderContext();

  const handleToggle = async () => {
    if (!enabled) {
      // Enable paper trading - create new portfolio
      setIsCreating(true);
      try {
        const newPortfolioId = crypto.randomUUID();

        await paperTradingDatabase.createPortfolio({
          id: newPortfolioId,
          name: `${activeProvider.toUpperCase()} Paper Trading`,
          provider: activeProvider,
          initialBalance: 100000,
          currency: 'USD',
        });

        setEnabled(true);
        onPortfolioChange(newPortfolioId);
      } catch (error) {
        console.error('[PaperTradingPanel] Failed to create portfolio:', error);
      } finally {
        setIsCreating(false);
      }
    } else {
      // Disable paper trading
      setEnabled(false);
      onPortfolioChange(null);
    }
  };

  const pnlColor = totalPnL >= 0 ? 'text-green-400' : 'text-red-400';
  const pnlPercentage = balance > 0 ? ((totalPnL / balance) * 100).toFixed(2) : '0.00';

  return (
    <div className="bg-[#0d0d0d] border-b border-gray-800 px-4 py-2 flex items-center justify-between">
      <div className="flex items-center gap-4">
        {/* Toggle */}
        <div className="flex items-center gap-2">
          <button
            onClick={handleToggle}
            disabled={isCreating}
            className={`relative inline-flex h-6 w-11 items-center rounded-full transition-colors ${
              enabled ? 'bg-orange-600' : 'bg-gray-700'
            } ${isCreating ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}`}
          >
            <span
              className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform ${
                enabled ? 'translate-x-6' : 'translate-x-1'
              }`}
            />
          </button>
          <span className="text-xs font-semibold text-gray-400 uppercase">
            Paper Trading {isCreating ? '(Creating...)' : ''}
          </span>
        </div>

        {/* Portfolio Stats */}
        {enabled && portfolioId && (
          <>
            <div className="h-4 w-px bg-gray-700" />
            <div className="flex items-center gap-4">
              <div className="text-xs">
                <span className="text-gray-500">Balance:</span>
                <span className="text-green-400 ml-2 font-mono font-semibold">
                  ${balance.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </span>
              </div>

              <div className="text-xs">
                <span className="text-gray-500">Total P&L:</span>
                <span className={`${pnlColor} ml-2 font-mono font-semibold`}>
                  {totalPnL >= 0 ? '+' : ''}${totalPnL.toFixed(2)} ({pnlPercentage}%)
                </span>
              </div>
            </div>
          </>
        )}
      </div>

      {/* Reset Button */}
      {enabled && portfolioId && onReset && (
        <button
          onClick={onReset}
          disabled={isResetting}
          className="text-xs text-orange-500 hover:text-orange-400 font-semibold disabled:opacity-50 disabled:cursor-not-allowed"
        >
          {isResetting ? 'RESETTING...' : 'RESET PORTFOLIO'}
        </button>
      )}
    </div>
  );
}
