/**
 * YFinance Paper Trading Settings
 *
 * Simple settings for YFinance paper trading - just cache TTL selection
 */

import React from 'react';
import { Settings, Clock, Info } from 'lucide-react';
import type { UpdateInterval } from '@/brokers/stocks/yfinance';

const CACHE_OPTIONS: Array<{ value: UpdateInterval; label: string }> = [
  { value: 15, label: '15 sec' },
  { value: 30, label: '30 sec' },
  { value: 60, label: '1 min' },
  { value: 300, label: '5 min' },
  { value: 600, label: '10 min' },
];

interface YFinanceSettingsProps {
  cacheTTL: UpdateInterval;
  onCacheTTLChange: (ttl: UpdateInterval) => void;
  isConnected: boolean;
}

export function YFinanceSettings({ cacheTTL, onCacheTTLChange, isConnected }: YFinanceSettingsProps) {
  return (
    <div className="rounded-lg p-4 bg-[#0F0F0F] border border-[#2A2A2A]">
      <div className="flex items-center gap-2 mb-3">
        <Settings size={16} className="text-[#FF8800]" />
        <span className="font-medium text-white text-sm">YFinance Settings</span>
        {isConnected && (
          <span className="ml-auto text-xs px-2 py-0.5 rounded bg-[#00D66F]/20 text-[#00D66F]">
            Connected
          </span>
        )}
      </div>

      <div className="flex items-center gap-2 mb-2">
        <Clock size={12} className="text-[#787878]" />
        <span className="text-xs text-[#787878]">Quote Cache Duration</span>
      </div>

      <div className="flex gap-1">
        {CACHE_OPTIONS.map((opt) => (
          <button
            key={opt.value}
            onClick={() => onCacheTTLChange(opt.value)}
            className={`px-2 py-1 text-xs rounded transition-colors ${
              cacheTTL === opt.value
                ? 'bg-[#FF8800] text-white'
                : 'bg-[#1A1A1A] text-[#787878] hover:bg-[#2A2A2A]'
            }`}
          >
            {opt.label}
          </button>
        ))}
      </div>

      <div className="mt-3 flex items-start gap-2 text-xs text-[#787878]">
        <Info size={12} className="mt-0.5 shrink-0" />
        <span>Quotes are cached and fetched on-demand. Lower values = fresher data but more API calls.</span>
      </div>
    </div>
  );
}

export default YFinanceSettings;
