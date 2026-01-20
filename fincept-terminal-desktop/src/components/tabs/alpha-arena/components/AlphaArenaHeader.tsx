/**
 * Alpha Arena Header Component
 *
 * Displays the header with title and status indicators.
 */

import React from 'react';
import { Trophy, Settings, Info } from 'lucide-react';
import type { CompetitionStatus } from '../types';

interface AlphaArenaHeaderProps {
  competitionId?: string;
  status: CompetitionStatus | null;
  cycleCount: number;
  onSettingsClick?: () => void;
}

const AlphaArenaHeader: React.FC<AlphaArenaHeaderProps> = ({
  competitionId,
  status,
  cycleCount,
  onSettingsClick,
}) => {
  const getStatusBadge = (status: CompetitionStatus | null) => {
    if (!status) return null;

    const statusConfig = {
      created: { label: 'Ready', color: 'bg-blue-500/20 text-blue-400' },
      running: { label: 'Running', color: 'bg-green-500/20 text-green-400' },
      paused: { label: 'Paused', color: 'bg-yellow-500/20 text-yellow-400' },
      completed: { label: 'Completed', color: 'bg-gray-500/20 text-gray-400' },
      failed: { label: 'Failed', color: 'bg-red-500/20 text-red-400' },
    };

    const config = statusConfig[status] || statusConfig.created;

    return (
      <span className={`px-2 py-1 rounded text-xs font-medium ${config.color}`}>
        {config.label}
      </span>
    );
  };

  return (
    <div className="bg-[#1A1A1A] border-b border-[#2A2A2A] px-6 py-4">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <Trophy className="w-6 h-6 text-[#FF8800]" />
            <h1 className="text-xl font-bold text-white">Alpha Arena</h1>
          </div>

          {competitionId && (
            <>
              <div className="h-6 w-px bg-[#2A2A2A]" />
              <div className="flex items-center gap-2">
                <span className="text-sm text-[#787878]">
                  {competitionId.slice(0, 12)}...
                </span>
                {getStatusBadge(status)}
              </div>
            </>
          )}

          {cycleCount > 0 && (
            <div className="flex items-center gap-1 text-sm text-[#787878]">
              <span className="text-[#FF8800] font-mono">{cycleCount}</span>
              <span>cycles</span>
            </div>
          )}
        </div>

        <div className="flex items-center gap-2">
          <button
            onClick={onSettingsClick}
            className="p-2 rounded-lg hover:bg-[#2A2A2A] text-[#787878] hover:text-white transition-colors"
            title="Settings"
          >
            <Settings className="w-5 h-5" />
          </button>
          <button
            className="p-2 rounded-lg hover:bg-[#2A2A2A] text-[#787878] hover:text-white transition-colors"
            title="Info"
          >
            <Info className="w-5 h-5" />
          </button>
        </div>
      </div>

      {/* Subtitle */}
      <p className="text-sm text-[#787878] mt-1">
        AI Trading Competition - Multiple LLM models compete in real-time trading
      </p>
    </div>
  );
};

export default AlphaArenaHeader;
