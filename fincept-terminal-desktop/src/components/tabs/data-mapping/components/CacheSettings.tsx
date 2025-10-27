// Cache Settings Component - Manage caching and encryption preferences

import React, { useState, useEffect } from 'react';
import { Trash2, Shield, Clock, Database } from 'lucide-react';
import { mappingDatabase } from '../services/MappingDatabase';
import { encryptionService } from '../services/EncryptionService';

interface CacheSettingsProps {
  cacheEnabled: boolean;
  cacheTTL: number;
  onCacheEnabledChange: (enabled: boolean) => void;
  onCacheTTLChange: (ttl: number) => void;
  mappingId?: string;
}

export function CacheSettings({
  cacheEnabled,
  cacheTTL,
  onCacheEnabledChange,
  onCacheTTLChange,
  mappingId,
}: CacheSettingsProps) {
  const [encryptionEnabled, setEncryptionEnabled] = useState(true);
  const [isClearing, setIsClearing] = useState(false);

  useEffect(() => {
    loadEncryptionPreference();
  }, []);

  const loadEncryptionPreference = async () => {
    const enabled = await encryptionService.isEncryptionEnabled();
    setEncryptionEnabled(enabled);
  };

  const handleEncryptionToggle = async (enabled: boolean) => {
    try {
      await encryptionService.setEncryptionEnabled(enabled);
      setEncryptionEnabled(enabled);

      // Show user notification
      alert(
        enabled
          ? 'Encryption enabled. New mappings will encrypt credentials.'
          : 'Encryption disabled. Credentials will be stored in plain text.'
      );
    } catch (error) {
      console.error('Failed to toggle encryption:', error);
      alert('Failed to update encryption setting');
    }
  };

  const handleClearCache = async () => {
    if (!confirm('Clear cache for this mapping? This cannot be undone.')) {
      return;
    }

    setIsClearing(true);
    try {
      await mappingDatabase.clearCache(mappingId);
      alert('Cache cleared successfully');
    } catch (error) {
      console.error('Failed to clear cache:', error);
      alert('Failed to clear cache');
    } finally {
      setIsClearing(false);
    }
  };

  const handleClearAllCache = async () => {
    if (
      !confirm(
        'Clear ALL cached responses for all mappings? This cannot be undone.'
      )
    ) {
      return;
    }

    setIsClearing(true);
    try {
      await mappingDatabase.clearCache();
      alert('All cache cleared successfully');
    } catch (error) {
      console.error('Failed to clear all cache:', error);
      alert('Failed to clear all cache');
    } finally {
      setIsClearing(false);
    }
  };

  return (
    <div className="space-y-6">
      {/* Cache Settings */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3 flex items-center gap-2">
          <Clock size={14} />
          Response Caching
        </h3>

        <div className="space-y-3">
          {/* Enable Cache Toggle */}
          <div className="flex items-center justify-between p-3 bg-zinc-900 rounded border border-zinc-700">
            <div>
              <div className="text-sm font-bold text-white">Enable Caching</div>
              <div className="text-xs text-gray-400">
                Cache API responses to reduce requests
              </div>
            </div>
            <label className="relative inline-block w-12 h-6">
              <input
                type="checkbox"
                checked={cacheEnabled}
                onChange={(e) => onCacheEnabledChange(e.target.checked)}
                className="sr-only peer"
              />
              <div className="w-full h-full bg-zinc-700 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-6 peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-orange-600"></div>
            </label>
          </div>

          {/* Cache TTL */}
          {cacheEnabled && (
            <div className="p-3 bg-zinc-900 rounded border border-zinc-700">
              <label className="text-sm font-bold text-white block mb-2">
                Cache Duration (seconds)
              </label>
              <input
                type="number"
                value={cacheTTL}
                onChange={(e) => onCacheTTLChange(Number(e.target.value))}
                min="0"
                step="60"
                className="w-full bg-zinc-800 border border-zinc-700 text-gray-300 px-3 py-2 text-sm rounded focus:outline-none focus:border-orange-500"
              />
              <div className="text-xs text-gray-500 mt-2">
                Common values: 60 (1 min), 300 (5 min), 3600 (1 hour)
              </div>
            </div>
          )}

          {/* Clear Cache Buttons */}
          <div className="flex gap-2">
            {mappingId && (
              <button
                onClick={handleClearCache}
                disabled={isClearing}
                className="flex-1 bg-zinc-800 hover:bg-zinc-700 disabled:bg-zinc-900 text-gray-300 py-2 px-3 rounded text-xs font-bold flex items-center justify-center gap-2 transition-colors"
              >
                <Trash2 size={12} />
                {isClearing ? 'Clearing...' : 'Clear This Cache'}
              </button>
            )}
            <button
              onClick={handleClearAllCache}
              disabled={isClearing}
              className="flex-1 bg-red-900/30 hover:bg-red-900/50 disabled:bg-zinc-900 text-red-400 hover:text-red-300 py-2 px-3 rounded text-xs font-bold flex items-center justify-center gap-2 border border-red-700 transition-colors"
            >
              <Database size={12} />
              {isClearing ? 'Clearing...' : 'Clear All Cache'}
            </button>
          </div>
        </div>
      </div>

      {/* Encryption Settings */}
      <div>
        <h3 className="text-xs font-bold text-orange-500 uppercase mb-3 flex items-center gap-2">
          <Shield size={14} />
          Security
        </h3>

        <div className="space-y-3">
          {/* Encryption Toggle */}
          <div className="flex items-center justify-between p-3 bg-zinc-900 rounded border border-zinc-700">
            <div>
              <div className="text-sm font-bold text-white">
                Encrypt API Credentials
              </div>
              <div className="text-xs text-gray-400">
                Store API keys and tokens encrypted in local database
              </div>
            </div>
            <label className="relative inline-block w-12 h-6">
              <input
                type="checkbox"
                checked={encryptionEnabled}
                onChange={(e) => handleEncryptionToggle(e.target.checked)}
                className="sr-only peer"
              />
              <div className="w-full h-full bg-zinc-700 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-6 peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-green-600"></div>
            </label>
          </div>

          {/* Encryption Info */}
          <div className="p-3 bg-blue-900/20 border border-blue-700 rounded">
            <div className="flex items-start gap-2">
              <Shield size={14} className="text-blue-400 mt-0.5" />
              <div className="text-xs text-blue-300">
                {encryptionEnabled ? (
                  <>
                    <strong>Encryption is enabled.</strong> Your API keys and
                    tokens are encrypted using AES-256-GCM before being stored.
                    You can toggle this setting anytime.
                  </>
                ) : (
                  <>
                    <strong>Encryption is disabled.</strong> Your API keys and
                    tokens will be stored in plain text. This is less secure but
                    may be useful for debugging.
                  </>
                )}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
