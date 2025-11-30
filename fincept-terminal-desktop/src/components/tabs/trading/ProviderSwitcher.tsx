// File: src/components/tabs/trading/ProviderSwitcher.tsx
// Provider selection component for multi-provider trading

import React from 'react';
import { useProviderContext } from '../../../contexts/ProviderContext';

export function ProviderSwitcher() {
  const { activeProvider, availableProviders, setActiveProvider, isLoading } = useProviderContext();

  if (isLoading) {
    return (
      <div className="flex items-center gap-2 bg-[#0d0d0d] px-4 py-2 border-b border-gray-800">
        <span className="text-xs text-gray-500">PROVIDER:</span>
        <div className="text-xs text-gray-400">Loading...</div>
      </div>
    );
  }

  return (
    <div className="flex items-center gap-2 bg-[#0d0d0d] px-4 py-2 border-b border-gray-800">
      <span className="text-xs text-gray-500 uppercase font-semibold">Provider:</span>
      <div className="flex gap-2">
        {availableProviders.map(provider => (
          <button
            key={provider}
            onClick={() => setActiveProvider(provider)}
            disabled={isLoading}
            className={`
              px-3 py-1 rounded text-xs font-bold uppercase transition-colors
              ${activeProvider === provider
                ? 'bg-orange-600 text-black'
                : 'bg-gray-800 text-gray-400 hover:bg-gray-700 hover:text-gray-300'
              }
              ${isLoading ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}
            `}
          >
            {provider}
          </button>
        ))}
      </div>

      {activeProvider && (
        <div className="ml-4 flex items-center gap-2">
          <div className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></div>
          <span className="text-xs text-gray-500">Connected to {activeProvider}</span>
        </div>
      )}
    </div>
  );
}
