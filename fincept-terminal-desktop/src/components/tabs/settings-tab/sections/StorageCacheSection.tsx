import React, { useState } from 'react';
import { RefreshCw, Trash2 } from 'lucide-react';
import { cacheService, type CacheStats } from '@/services/cache/cacheService';
import { LLMModelsService } from '@/services/llmModelsService';
import type { SettingsColors } from '../types';

interface StorageCacheSectionProps {
  colors: SettingsColors;
  showMessage: (type: 'success' | 'error', text: string) => void;
}

export function StorageCacheSection({
  colors,
  showMessage,
}: StorageCacheSectionProps) {
  const [cacheStats, setCacheStats] = useState<CacheStats | null>(null);
  const [clearingCache, setClearingCache] = useState(false);

  const refreshStats = async () => {
    const stats = await cacheService.getStats();
    setCacheStats(stats);
  };

  const handleCleanup = async () => {
    setClearingCache(true);
    try {
      const removed = await cacheService.cleanup();
      showMessage('success', `Cleaned up ${removed} expired cache entries`);
      await refreshStats();
    } catch (error) {
      showMessage('error', 'Failed to cleanup cache');
    } finally {
      setClearingCache(false);
    }
  };

  const handleClearAll = async () => {
    if (!confirm('Are you sure you want to clear all cached data? This action cannot be undone.')) {
      return;
    }
    setClearingCache(true);
    try {
      const removed = await cacheService.clearAll();
      await LLMModelsService.clearCache();
      showMessage('success', `Cleared ${removed} cache entries successfully`);
      setCacheStats(null);
      await refreshStats();
    } catch (error) {
      showMessage('error', 'Failed to clear cache');
    } finally {
      setClearingCache(false);
    }
  };

  return (
    <div style={{ padding: '20px' }}>
      <div style={{ marginBottom: '24px' }}>
        <h3 style={{ fontSize: '16px', fontWeight: 600, color: colors.text, marginBottom: '8px' }}>
          Storage & Cache Management
        </h3>
        <p style={{ color: colors.textMuted, fontSize: '13px' }}>
          Manage application cache and storage. Clearing cache can help resolve issues and free up disk space.
        </p>
      </div>

      {/* Cache Statistics */}
      <div style={{
        background: '#1a1a1a',
        borderRadius: '8px',
        padding: '16px',
        marginBottom: '20px',
        border: '1px solid #2a2a2a'
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
          <h4 style={{ fontSize: '14px', fontWeight: 500, color: colors.text }}>Cache Statistics</h4>
          <button
            onClick={refreshStats}
            style={{
              padding: '6px 12px',
              background: '#2a2a2a',
              border: '1px solid #3a3a3a',
              borderRadius: '4px',
              color: colors.text,
              fontSize: '12px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px'
            }}
          >
            <RefreshCw size={12} />
            Refresh
          </button>
        </div>

        {cacheStats ? (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px' }}>
            <div style={{ background: '#252525', padding: '12px', borderRadius: '6px' }}>
              <div style={{ color: colors.textMuted, fontSize: '11px', marginBottom: '4px' }}>Total Entries</div>
              <div style={{ color: colors.text, fontSize: '18px', fontWeight: 600 }}>{cacheStats.total_entries.toLocaleString()}</div>
            </div>
            <div style={{ background: '#252525', padding: '12px', borderRadius: '6px' }}>
              <div style={{ color: colors.textMuted, fontSize: '11px', marginBottom: '4px' }}>Cache Size</div>
              <div style={{ color: colors.text, fontSize: '18px', fontWeight: 600 }}>
                {(cacheStats.total_size_bytes / 1024 / 1024).toFixed(2)} MB
              </div>
            </div>
            <div style={{ background: '#252525', padding: '12px', borderRadius: '6px' }}>
              <div style={{ color: colors.textMuted, fontSize: '11px', marginBottom: '4px' }}>Expired Entries</div>
              <div style={{ color: cacheStats.expired_entries > 0 ? '#f59e0b' : colors.text, fontSize: '18px', fontWeight: 600 }}>
                {cacheStats.expired_entries.toLocaleString()}
              </div>
            </div>
          </div>
        ) : (
          <div style={{ color: colors.textMuted, fontSize: '13px', textAlign: 'center', padding: '20px' }}>
            Click "Refresh" to load cache statistics
          </div>
        )}

        {cacheStats && cacheStats.categories && cacheStats.categories.length > 0 && (
          <div style={{ marginTop: '16px' }}>
            <div style={{ color: colors.textMuted, fontSize: '12px', marginBottom: '8px' }}>Categories</div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
              {cacheStats.categories.map(cat => (
                <div
                  key={cat.category}
                  style={{
                    background: '#252525',
                    padding: '6px 10px',
                    borderRadius: '4px',
                    fontSize: '11px',
                    color: colors.text
                  }}
                >
                  <span style={{ color: colors.primary }}>{cat.category}</span>
                  <span style={{ color: colors.textMuted, marginLeft: '8px' }}>
                    {cat.entry_count} entries · {(cat.total_size / 1024).toFixed(1)} KB
                  </span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Cache Actions */}
      <div style={{
        background: '#1a1a1a',
        borderRadius: '8px',
        padding: '16px',
        border: '1px solid #2a2a2a'
      }}>
        <h4 style={{ fontSize: '14px', fontWeight: 500, color: colors.text, marginBottom: '16px' }}>Cache Actions</h4>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Clear Expired */}
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '12px', background: '#252525', borderRadius: '6px' }}>
            <div>
              <div style={{ color: colors.text, fontSize: '13px', fontWeight: 500 }}>Clear Expired Entries</div>
              <div style={{ color: colors.textMuted, fontSize: '11px' }}>Remove all expired cache entries to free up space</div>
            </div>
            <button
              onClick={handleCleanup}
              disabled={clearingCache}
              style={{
                padding: '8px 16px',
                background: '#3a3a3a',
                border: 'none',
                borderRadius: '4px',
                color: colors.text,
                fontSize: '12px',
                cursor: clearingCache ? 'not-allowed' : 'pointer',
                opacity: clearingCache ? 0.6 : 1
              }}
            >
              {clearingCache ? 'Cleaning...' : 'Clean Up'}
            </button>
          </div>

          {/* Clear All Cache */}
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '12px', background: '#252525', borderRadius: '6px', border: '1px solid #dc262640' }}>
            <div>
              <div style={{ color: colors.text, fontSize: '13px', fontWeight: 500 }}>Clear All Cache</div>
              <div style={{ color: colors.textMuted, fontSize: '11px' }}>Remove all cached data. This will not affect your settings or saved data.</div>
            </div>
            <button
              onClick={handleClearAll}
              disabled={clearingCache}
              style={{
                padding: '8px 16px',
                background: '#dc2626',
                border: 'none',
                borderRadius: '4px',
                color: 'white',
                fontSize: '12px',
                fontWeight: 500,
                cursor: clearingCache ? 'not-allowed' : 'pointer',
                opacity: clearingCache ? 0.6 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '6px'
              }}
            >
              <Trash2 size={14} />
              {clearingCache ? 'Clearing...' : 'Clear All'}
            </button>
          </div>
        </div>
      </div>

      {/* Info */}
      <div style={{ marginTop: '20px', padding: '12px', background: '#1a1a2e', borderRadius: '6px', border: '1px solid #2a2a4e' }}>
        <div style={{ color: '#8b8bff', fontSize: '12px', fontWeight: 500, marginBottom: '6px' }}>About Cache</div>
        <div style={{ color: colors.textMuted, fontSize: '11px', lineHeight: '1.5' }}>
          The cache stores temporary data like market quotes, news articles, and API responses to improve performance.
          Clearing the cache will not delete your settings, credentials, portfolios, or other saved data.
          The application will automatically rebuild the cache as you use it.
        </div>
      </div>
    </div>
  );
}
