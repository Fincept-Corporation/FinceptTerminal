/**
 * Decisions Panel Component
 *
 * Displays recent trading decisions from AI models.
 */

import React from 'react';
import { Brain, TrendingUp, TrendingDown, Pause, ArrowUpRight, ArrowDownRight } from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import type { ModelDecision } from '../types';
import { formatCurrency, getModelColor } from '../types';

interface DecisionsPanelProps {
  decisions: ModelDecision[];
  maxItems?: number;
  isLoading?: boolean;
}

const DecisionsPanel: React.FC<DecisionsPanelProps> = ({
  decisions,
  maxItems = 10,
  isLoading = false,
}) => {
  // Defensive guard: ensure decisions is an array
  const safeDecisions = Array.isArray(decisions) ? decisions : [];
  const displayDecisions = safeDecisions.slice(0, maxItems);

  if (isLoading) {
    return (
      <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
        <div className="flex items-center gap-2 mb-4">
          <Brain className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Recent Decisions</h3>
        </div>
        <div className="space-y-2">
          {[1, 2, 3].map((i) => (
            <div key={i} className="h-16 bg-[#1A1A1A] rounded animate-pulse" />
          ))}
        </div>
      </div>
    );
  }

  if (displayDecisions.length === 0) {
    return (
      <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
        <div className="flex items-center gap-2 mb-4">
          <Brain className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Recent Decisions</h3>
        </div>
        <p className="text-[#787878] text-center py-8">
          No decisions yet. Run a cycle to see AI model decisions.
        </p>
      </div>
    );
  }

  return (
    <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <Brain className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Recent Decisions</h3>
        </div>
        <span className="text-sm text-[#787878]">{safeDecisions.length} total</span>
      </div>

      <div className="space-y-2 max-h-[400px] overflow-y-auto custom-scrollbar">
        {displayDecisions.map((decision, index) => (
          <DecisionCard key={`${decision.model_name}-${decision.cycle_number}-${index}`} decision={decision} />
        ))}
      </div>
    </div>
  );
};

interface DecisionCardProps {
  decision: ModelDecision;
}

const DecisionCard: React.FC<DecisionCardProps> = ({ decision }) => {
  const modelColor = getModelColor(decision.model_name);
  const confidence = Number.isFinite(decision.confidence) ? decision.confidence : 0;
  const quantity = Number.isFinite(decision.quantity) ? decision.quantity : 0;
  const priceAtDecision = decision.price_at_decision != null && Number.isFinite(decision.price_at_decision)
    ? decision.price_at_decision
    : undefined;

  const getActionIcon = (action: string) => {
    switch (action.toLowerCase()) {
      case 'buy':
        return <TrendingUp className="w-4 h-4 text-[#00D66F]" />;
      case 'sell':
        return <TrendingDown className="w-4 h-4 text-[#FF3B3B]" />;
      case 'short':
        return <ArrowDownRight className="w-4 h-4 text-[#FF3B3B]" />;
      default:
        return <Pause className="w-4 h-4 text-[#787878]" />;
    }
  };

  const getActionColor = (action: string) => {
    switch (action.toLowerCase()) {
      case 'buy':
        return 'text-[#00D66F] bg-[#00D66F]/10';
      case 'sell':
        return 'text-[#FF3B3B] bg-[#FF3B3B]/10';
      case 'short':
        return 'text-[#FF3B3B] bg-[#FF3B3B]/10';
      default:
        return 'text-[#787878] bg-[#787878]/10';
    }
  };

  const confidenceColor = confidence >= 0.7
    ? 'text-[#00D66F]'
    : confidence >= 0.5
      ? 'text-[#FF8800]'
      : 'text-[#FF3B3B]';

  return (
    <div className="p-3 bg-[#1A1A1A] rounded-lg border border-[#2A2A2A] hover:border-[#3A3A3A] transition-colors">
      <div className="flex items-start gap-3">
        {/* Model indicator */}
        <div
          className="w-1 h-full min-h-[50px] rounded-full"
          style={{ backgroundColor: modelColor }}
        />

        <div className="flex-1 min-w-0">
          {/* Header */}
          <div className="flex items-center justify-between mb-2">
            <div className="flex items-center gap-2">
              <span className="text-white font-medium text-sm">
                {decision.model_name}
              </span>
              <span className="text-xs text-[#787878]">
                Cycle {decision.cycle_number}
              </span>
            </div>

            {/* Action badge */}
            <span className={`px-2 py-0.5 rounded text-xs font-medium uppercase flex items-center gap-1 ${getActionColor(decision.action)}`}>
              {getActionIcon(decision.action)}
              {decision.action}
            </span>
          </div>

          {/* Details */}
          <div className="flex items-center gap-4 text-xs text-[#787878] mb-2">
            <span>{decision.symbol}</span>
            {quantity > 0 && (
              <span>Qty: {quantity.toFixed(4)}</span>
            )}
            {priceAtDecision !== undefined && (
              <span>@ {formatCurrency(priceAtDecision)}</span>
            )}
            <span className={confidenceColor}>
              {(confidence * 100).toFixed(0)}% confidence
            </span>
          </div>

          {/* Reasoning */}
          {decision.reasoning && (
            <p className="text-xs text-[#A0A0A0] line-clamp-2">
              {decision.reasoning}
            </p>
          )}

          {/* Trade result */}
          {decision.trade_executed && (
            <div className="mt-2 pt-2 border-t border-[#2A2A2A]">
              <span className="text-xs text-[#787878]">
                Trade: {decision.trade_executed.status}
                {decision.trade_executed.pnl !== undefined && (
                  <span className={decision.trade_executed.pnl >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}>
                    {' '}({decision.trade_executed.pnl >= 0 ? '+' : ''}{formatCurrency(decision.trade_executed.pnl)})
                  </span>
                )}
              </span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default withErrorBoundary(DecisionsPanel, { name: 'AlphaArena.DecisionsPanel' });
