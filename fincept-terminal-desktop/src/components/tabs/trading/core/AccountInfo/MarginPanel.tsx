/**
 * MarginPanel - Displays margin information and warnings
 */

import React from 'react';
import { AlertTriangle, Shield, TrendingUp } from 'lucide-react';
import { useMarginInfo } from '../../hooks/useAccountInfo';

export function MarginPanel() {
  const { marginInfo, isLoading } = useMarginInfo();

  if (isLoading) {
    return (
      <div className="bg-gray-900 border border-gray-800 rounded p-4">
        <div className="flex items-center justify-center py-4">
          <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-blue-500"></div>
        </div>
      </div>
    );
  }

  if (!marginInfo) {
    return (
      <div className="bg-gray-900 border border-gray-800 rounded p-4">
        <div className="flex items-center gap-2 mb-2">
          <Shield className="w-4 h-4 text-gray-500" />
          <span className="text-sm font-medium text-gray-400">Margin Trading</span>
        </div>
        <div className="text-xs text-gray-500">
          Not available or not enabled on this account
        </div>
      </div>
    );
  }

  // Determine margin level status
  const getMarginStatus = () => {
    if (marginInfo.marginLevel >= 50) {
      return {
        color: 'text-green-500',
        bgColor: 'bg-green-500/10',
        borderColor: 'border-green-500/20',
        label: 'Healthy',
      };
    } else if (marginInfo.marginLevel >= 20) {
      return {
        color: 'text-yellow-500',
        bgColor: 'bg-yellow-500/10',
        borderColor: 'border-yellow-500/20',
        label: 'Warning',
      };
    } else {
      return {
        color: 'text-red-500',
        bgColor: 'bg-red-500/10',
        borderColor: 'border-red-500/20',
        label: 'Critical',
      };
    }
  };

  const status = getMarginStatus();

  return (
    <div className="bg-gray-900 border border-gray-800 rounded p-4">
      {/* Header */}
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center gap-2">
          <Shield className="w-4 h-4 text-blue-500" />
          <span className="text-sm font-medium text-gray-300">Margin Status</span>
        </div>
        <div className={`flex items-center gap-1 px-2 py-0.5 ${status.bgColor} ${status.borderColor} border rounded text-xs ${status.color}`}>
          <div className="w-1.5 h-1.5 rounded-full bg-current"></div>
          {status.label}
        </div>
      </div>

      {/* Margin Call Warning */}
      {marginInfo.isMarginCall && (
        <div className="mb-3 p-2 bg-red-500/10 border border-red-500/20 rounded flex items-start gap-2">
          <AlertTriangle className="w-4 h-4 text-red-500 flex-shrink-0 mt-0.5" />
          <div className="text-xs text-red-400">
            <div className="font-medium mb-1">Margin Call Warning!</div>
            <div>Your margin level is critically low. Add funds or close positions to avoid liquidation.</div>
          </div>
        </div>
      )}

      {/* Margin Level Progress Bar */}
      <div className="mb-4">
        <div className="flex items-center justify-between mb-1">
          <span className="text-xs text-gray-400">Margin Level</span>
          <span className={`text-sm font-mono font-bold ${status.color}`}>
            {marginInfo.marginLevel.toFixed(2)}%
          </span>
        </div>
        <div className="w-full h-2 bg-gray-800 rounded-full overflow-hidden">
          <div
            className={`h-full ${status.bgColor.replace('/10', '')} transition-all`}
            style={{ width: `${Math.min(marginInfo.marginLevel, 100)}%` }}
          ></div>
        </div>
      </div>

      {/* Margin Details */}
      <div className="space-y-2">
        <div className="flex items-center justify-between p-2 bg-black/30 rounded">
          <span className="text-xs text-gray-400">Total Margin</span>
          <span className="text-sm font-mono text-white">${marginInfo.totalMargin.toFixed(2)}</span>
        </div>

        <div className="flex items-center justify-between p-2 bg-black/30 rounded">
          <span className="text-xs text-gray-400">Used Margin</span>
          <span className="text-sm font-mono text-orange-500">${marginInfo.usedMargin.toFixed(2)}</span>
        </div>

        <div className="flex items-center justify-between p-2 bg-black/30 rounded">
          <span className="text-xs text-gray-400">Available Margin</span>
          <span className="text-sm font-mono text-green-500">${marginInfo.availableMargin.toFixed(2)}</span>
        </div>

        {marginInfo.maintenanceMargin > 0 && (
          <div className="flex items-center justify-between p-2 bg-black/30 rounded">
            <span className="text-xs text-gray-400">Maintenance Margin</span>
            <span className="text-sm font-mono text-blue-500">${marginInfo.maintenanceMargin.toFixed(2)}</span>
          </div>
        )}
      </div>

      {/* Info Tip */}
      <div className="mt-3 p-2 bg-blue-500/10 border border-blue-500/20 rounded">
        <div className="flex items-start gap-2">
          <TrendingUp className="w-3 h-3 text-blue-400 flex-shrink-0 mt-0.5" />
          <div className="text-xs text-blue-400">
            Maintain margin level above 50% for safe trading
          </div>
        </div>
      </div>
    </div>
  );
}
