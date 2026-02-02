import React, { useState } from 'react';
import { RefreshCw, Trash2, HardDrive, Database } from 'lucide-react';
import { cacheService, type CacheStats } from '@/services/cache/cacheService';
import { LLMModelsService } from '@/services/llmModelsService';
import type { SettingsColors } from '../types';
import { showConfirm } from '@/utils/notifications';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

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
    const confirmed = await showConfirm(
      'This action cannot be undone.\n\nNote: Settings and saved data will not be affected.',
      {
        title: 'Clear all cached data?',
        type: 'danger'
      }
    );
    if (!confirmed) return;

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
    <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        marginBottom: '16px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
          <HardDrive size={14} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            Storage & Cache Management
          </span>
        </div>
        <p style={{
          fontSize: '10px',
          color: FINCEPT.GRAY,
          margin: 0,
          lineHeight: 1.5
        }}>
          Manage application cache and storage. Clearing cache can help resolve issues and free up disk space.
        </p>
      </div>

      {/* Cache Statistics */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <Database size={12} color={FINCEPT.ORANGE} />
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.WHITE,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              Cache Statistics
            </span>
          </div>
          {/* Secondary Button */}
          <button
            onClick={refreshStats}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <RefreshCw size={10} />
            Refresh
          </button>
        </div>

        {cacheStats ? (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
            <div style={{
              backgroundColor: FINCEPT.HEADER_BG,
              padding: '10px',
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.BORDER}`
            }}>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '4px'
              }}>
                Total Entries
              </div>
              <div style={{ color: FINCEPT.CYAN, fontSize: '14px', fontWeight: 700 }}>
                {cacheStats.total_entries.toLocaleString()}
              </div>
            </div>
            <div style={{
              backgroundColor: FINCEPT.HEADER_BG,
              padding: '10px',
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.BORDER}`
            }}>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '4px'
              }}>
                Cache Size
              </div>
              <div style={{ color: FINCEPT.CYAN, fontSize: '14px', fontWeight: 700 }}>
                {(cacheStats.total_size_bytes / 1024 / 1024).toFixed(2)} MB
              </div>
            </div>
            <div style={{
              backgroundColor: FINCEPT.HEADER_BG,
              padding: '10px',
              borderRadius: '2px',
              border: `1px solid ${FINCEPT.BORDER}`
            }}>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '4px'
              }}>
                Expired Entries
              </div>
              <div style={{
                color: cacheStats.expired_entries > 0 ? FINCEPT.YELLOW : FINCEPT.CYAN,
                fontSize: '14px',
                fontWeight: 700
              }}>
                {cacheStats.expired_entries.toLocaleString()}
              </div>
            </div>
          </div>
        ) : (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            padding: '20px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <Database size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>Click "Refresh" to load cache statistics</span>
          </div>
        )}

        {cacheStats && cacheStats.categories && cacheStats.categories.length > 0 && (
          <div style={{ marginTop: '12px' }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '6px'
            }}>
              Categories
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {cacheStats.categories.map(cat => (
                <div
                  key={cat.category}
                  style={{
                    padding: '2px 6px',
                    backgroundColor: `${FINCEPT.ORANGE}20`,
                    color: FINCEPT.ORANGE,
                    fontSize: '8px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px'
                  }}
                >
                  <span style={{ textTransform: 'uppercase', letterSpacing: '0.5px' }}>{cat.category}</span>
                  <span style={{ color: FINCEPT.GRAY }}>
                    {cat.entry_count} · {(cat.total_size / 1024).toFixed(1)}KB
                  </span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Cache Actions */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GRAY,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          marginBottom: '12px'
        }}>
          Cache Actions
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          {/* Clear Expired */}
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            padding: '10px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderRadius: '2px',
            border: `1px solid ${FINCEPT.BORDER}`
          }}>
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.WHITE,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '2px'
              }}>
                Clear Expired Entries
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                Remove all expired cache entries to free up space
              </div>
            </div>
            {/* Secondary Button */}
            <button
              onClick={handleCleanup}
              disabled={clearingCache}
              style={{
                padding: '6px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: clearingCache ? 'not-allowed' : 'pointer',
                opacity: clearingCache ? 0.5 : 1,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                transition: 'all 0.2s',
                whiteSpace: 'nowrap'
              }}
              onMouseEnter={(e) => {
                if (!clearingCache) {
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }
              }}
              onMouseLeave={(e) => {
                if (!clearingCache) {
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              {clearingCache ? 'Cleaning...' : 'Clean Up'}
            </button>
          </div>

          {/* Clear All Cache */}
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            padding: '10px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderRadius: '2px',
            border: `1px solid ${FINCEPT.RED}40`
          }}>
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.WHITE,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '2px'
              }}>
                Clear All Cache
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                Remove all cached data. This will not affect your settings or saved data.
              </div>
            </div>
            {/* Danger Button */}
            <button
              onClick={handleClearAll}
              disabled={clearingCache}
              style={{
                padding: '8px 16px',
                backgroundColor: clearingCache ? FINCEPT.MUTED : FINCEPT.RED,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: clearingCache ? 'not-allowed' : 'pointer',
                opacity: clearingCache ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                whiteSpace: 'nowrap'
              }}
            >
              <Trash2 size={10} />
              {clearingCache ? 'Clearing...' : 'Clear All'}
            </button>
          </div>
        </div>
      </div>

      {/* Info */}
      <div style={{
        padding: '10px',
        backgroundColor: `${FINCEPT.CYAN}10`,
        borderRadius: '2px',
        border: `1px solid ${FINCEPT.CYAN}40`
      }}>
        <div style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.CYAN,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          marginBottom: '4px'
        }}>
          About Cache
        </div>
        <div style={{
          color: FINCEPT.GRAY,
          fontSize: '9px',
          lineHeight: 1.5
        }}>
          The cache stores temporary data like market quotes, news articles, and API responses to improve performance.
          Clearing the cache will not delete your settings, credentials, portfolios, or other saved data.
          The application will automatically rebuild the cache as you use it.
        </div>
      </div>
    </div>
  );
}
