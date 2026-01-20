/**
 * Competition Controls Component
 *
 * Controls for running competition cycles.
 */

import React from 'react';
import { Play, Square, RefreshCw, RotateCcw, Loader2 } from 'lucide-react';

interface CompetitionControlsProps {
  isRunning: boolean;
  isAutoRunning: boolean;
  isLoading: boolean;
  hasCompetition: boolean;
  onRunCycle: () => void;
  onToggleAutoRun: () => void;
  onReset?: () => void;
  cycleInterval?: number;
}

const CompetitionControls: React.FC<CompetitionControlsProps> = ({
  isRunning,
  isAutoRunning,
  isLoading,
  hasCompetition,
  onRunCycle,
  onToggleAutoRun,
  onReset,
  cycleInterval = 150,
}) => {
  return (
    <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
      <div className="flex items-center justify-between">
        <h3 className="text-white font-semibold">Controls</h3>

        {isAutoRunning && (
          <span className="text-xs text-[#787878]">
            Auto-run every {cycleInterval}s
          </span>
        )}
      </div>

      <div className="flex items-center gap-3 mt-4">
        {/* Run Single Cycle */}
        <button
          onClick={onRunCycle}
          disabled={!hasCompetition || isLoading || isAutoRunning}
          className="flex-1 flex items-center justify-center gap-2 px-4 py-3 rounded-lg bg-[#FF8800] text-black font-medium hover:bg-[#FF9900] disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
        >
          {isLoading ? (
            <>
              <Loader2 className="w-5 h-5 animate-spin" />
              Running...
            </>
          ) : (
            <>
              <Play className="w-5 h-5" />
              Run Cycle
            </>
          )}
        </button>

        {/* Auto Run Toggle */}
        <button
          onClick={onToggleAutoRun}
          disabled={!hasCompetition}
          className={`flex items-center justify-center gap-2 px-4 py-3 rounded-lg border font-medium transition-colors ${
            isAutoRunning
              ? 'bg-red-500/10 border-red-500 text-red-400 hover:bg-red-500/20'
              : 'bg-[#1A1A1A] border-[#2A2A2A] text-white hover:bg-[#2A2A2A]'
          }`}
        >
          {isAutoRunning ? (
            <>
              <Square className="w-5 h-5" />
              Stop Auto
            </>
          ) : (
            <>
              <RefreshCw className="w-5 h-5" />
              Auto Run
            </>
          )}
        </button>

        {/* Reset */}
        {onReset && (
          <button
            onClick={onReset}
            disabled={isLoading || isAutoRunning}
            className="p-3 rounded-lg border border-[#2A2A2A] text-[#787878] hover:bg-[#1A1A1A] hover:text-white transition-colors disabled:opacity-50"
            title="Reset Competition"
          >
            <RotateCcw className="w-5 h-5" />
          </button>
        )}
      </div>

      {/* Status indicators */}
      <div className="flex items-center gap-4 mt-4 text-sm">
        <div className="flex items-center gap-2">
          <div
            className={`w-2 h-2 rounded-full ${
              isRunning ? 'bg-green-500' : 'bg-[#787878]'
            }`}
          />
          <span className="text-[#787878]">
            {isRunning ? 'Competition Active' : 'Competition Idle'}
          </span>
        </div>

        {isAutoRunning && (
          <div className="flex items-center gap-2">
            <div className="w-2 h-2 rounded-full bg-blue-500 animate-pulse" />
            <span className="text-blue-400">Auto-running</span>
          </div>
        )}
      </div>
    </div>
  );
};

export default CompetitionControls;
