// Cache Settings Component - Terminal UI/UX

import React, { useState, useEffect } from 'react';
import { Trash2, Shield, Clock, Database, CheckCircle2, AlertTriangle } from 'lucide-react';
import { mappingDatabase } from '../services/MappingDatabase';
import { encryptionService } from '../services/EncryptionService';
import { showConfirm, showSuccess, showError, showInfo } from '@/utils/notifications';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface CacheSettingsProps {
  cacheEnabled: boolean;
  cacheTTL: number;
  onCacheEnabledChange: (enabled: boolean) => void;
  onCacheTTLChange: (ttl: number) => void;
  mappingId?: string;
}

// Square terminal toggle (no rounded corners)
function TerminalToggle({
  checked,
  onChange,
  activeColor,
}: {
  checked: boolean;
  onChange: (v: boolean) => void;
  activeColor: string;
}) {
  const { colors } = useTerminalTheme();
  const borderColor = 'var(--ft-border-color, #2A2A2A)';
  return (
    <button
      role="switch"
      aria-checked={checked}
      onClick={() => onChange(!checked)}
      className="relative flex-shrink-0 transition-all"
      style={{
        width: '40px',
        height: '20px',
        backgroundColor: checked ? activeColor : colors.background,
        border: `1px solid ${checked ? activeColor : borderColor}`,
        cursor: 'pointer',
        outline: 'none',
      }}
    >
      {/* Knob */}
      <div
        className="absolute top-0 bottom-0 transition-all"
        style={{
          width: '18px',
          backgroundColor: checked ? colors.background : colors.textMuted,
          left: checked ? 'calc(100% - 19px)' : '1px',
          margin: '1px 0',
        }}
      />
    </button>
  );
}

export function CacheSettings({
  cacheEnabled,
  cacheTTL,
  onCacheEnabledChange,
  onCacheTTLChange,
  mappingId,
}: CacheSettingsProps) {
  const { colors } = useTerminalTheme();
  const [encryptionEnabled, setEncryptionEnabled] = useState(true);
  const [isClearing, setIsClearing] = useState(false);
  const [ttlFocused, setTtlFocused] = useState(false);

  const borderColor = 'var(--ft-border-color, #2A2A2A)';

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
      if (enabled) {
        showInfo('Encryption enabled. New mappings will encrypt credentials.');
      } else {
        showInfo('Encryption disabled. Credentials will be stored in plain text.');
      }
    } catch (error) {
      console.error('Failed to toggle encryption:', error);
      showError('Failed to update encryption setting');
    }
  };

  const handleClearCache = async () => {
    const confirmed = await showConfirm('This action cannot be undone.', {
      title: 'Clear cache for this mapping?',
      type: 'warning',
    });
    if (!confirmed) return;

    setIsClearing(true);
    try {
      await mappingDatabase.clearCache(mappingId);
      showSuccess('Cache cleared successfully');
    } catch (error) {
      console.error('Failed to clear cache:', error);
      showError('Failed to clear cache');
    } finally {
      setIsClearing(false);
    }
  };

  const handleClearAllCache = async () => {
    const confirmed = await showConfirm(
      'This will clear ALL cached responses for all mappings. This action cannot be undone.',
      { title: 'Clear all cache?', type: 'danger' }
    );
    if (!confirmed) return;

    setIsClearing(true);
    try {
      await mappingDatabase.clearCache();
      showSuccess('All cache cleared successfully');
    } catch (error) {
      console.error('Failed to clear all cache:', error);
      showError('Failed to clear all cache');
    } finally {
      setIsClearing(false);
    }
  };

  return (
    <div className="space-y-4">

      {/* ── Response Caching Section ── */}
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
        {/* Section Header */}
        <div
          className="flex items-center gap-2 px-3 py-2"
          style={{ borderBottom: `1px solid ${colors.primary}40`, backgroundColor: `${colors.primary}08` }}
        >
          <Clock size={12} color={colors.primary} />
          <span className="text-[10px] font-bold tracking-wider" style={{ color: colors.primary }}>
            RESPONSE CACHING
          </span>
        </div>

        <div className="p-3 space-y-2">
          {/* Enable Cache Row */}
          <div
            className="flex items-center justify-between px-3 py-2.5"
            style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}` }}
          >
            <div>
              <div className="text-xs font-bold" style={{ color: colors.text }}>Enable Caching</div>
              <div className="text-[11px] font-mono mt-0.5" style={{ color: colors.textMuted }}>
                Cache API responses to reduce requests
              </div>
            </div>
            <TerminalToggle
              checked={cacheEnabled}
              onChange={onCacheEnabledChange}
              activeColor={colors.primary}
            />
          </div>

          {/* Cache TTL */}
          {cacheEnabled && (
            <div
              className="px-3 py-2.5"
              style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}` }}
            >
              <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: colors.text }}>
                CACHE DURATION (SECONDS)
              </div>
              <input
                type="text"
                inputMode="numeric"
                value={String(cacheTTL)}
                onChange={(e) => {
                  const v = e.target.value;
                  if (v === '' || /^\d+$/.test(v)) {
                    onCacheTTLChange(v === '' ? 0 : Number(v));
                  }
                }}
                onFocus={() => setTtlFocused(true)}
                onBlur={() => setTtlFocused(false)}
                className="w-full px-2 py-1.5 text-xs font-mono outline-none transition-all"
                style={{
                  backgroundColor: colors.panel,
                  border: `1px solid ${ttlFocused ? colors.primary : borderColor}`,
                  color: colors.text,
                }}
              />
              <div className="text-[10px] font-mono mt-1.5" style={{ color: colors.textMuted }}>
                Common: <span style={{ color: colors.primary }}>60</span> (1 min) ·{' '}
                <span style={{ color: colors.primary }}>300</span> (5 min) ·{' '}
                <span style={{ color: colors.primary }}>3600</span> (1 hr)
              </div>
            </div>
          )}

          {/* Clear Cache Buttons */}
          <div className="flex gap-2 pt-1">
            {mappingId && (
              <button
                onClick={handleClearCache}
                disabled={isClearing}
                className="flex-1 flex items-center justify-center gap-1.5 py-2 text-[11px] font-bold tracking-wide transition-all"
                style={{
                  backgroundColor: isClearing ? colors.background : 'transparent',
                  border: `1px solid ${borderColor}`,
                  color: isClearing ? colors.textMuted : colors.text,
                  opacity: isClearing ? 0.6 : 1,
                  cursor: isClearing ? 'not-allowed' : 'pointer',
                }}
                onMouseEnter={(e) => {
                  if (!isClearing) {
                    e.currentTarget.style.borderColor = colors.primary;
                    e.currentTarget.style.color = colors.primary;
                    e.currentTarget.style.backgroundColor = `${colors.primary}08`;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isClearing) {
                    e.currentTarget.style.borderColor = borderColor;
                    e.currentTarget.style.color = colors.text;
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                <Trash2 size={11} />
                {isClearing ? 'CLEARING...' : 'CLEAR THIS CACHE'}
              </button>
            )}
            <button
              onClick={handleClearAllCache}
              disabled={isClearing}
              className="flex-1 flex items-center justify-center gap-1.5 py-2 text-[11px] font-bold tracking-wide transition-all"
              style={{
                backgroundColor: isClearing ? colors.background : `${colors.alert}10`,
                border: `1px solid ${colors.alert}`,
                color: isClearing ? colors.textMuted : colors.alert,
                opacity: isClearing ? 0.6 : 1,
                cursor: isClearing ? 'not-allowed' : 'pointer',
              }}
              onMouseEnter={(e) => {
                if (!isClearing) {
                  e.currentTarget.style.backgroundColor = `${colors.alert}20`;
                }
              }}
              onMouseLeave={(e) => {
                if (!isClearing) {
                  e.currentTarget.style.backgroundColor = `${colors.alert}10`;
                }
              }}
            >
              <Database size={11} />
              {isClearing ? 'CLEARING...' : 'CLEAR ALL CACHE'}
            </button>
          </div>
        </div>
      </div>

      {/* ── Security Section ── */}
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${borderColor}` }}>
        {/* Section Header */}
        <div
          className="flex items-center gap-2 px-3 py-2"
          style={{ borderBottom: `1px solid ${(colors.info || '#0088FF')}40`, backgroundColor: `${colors.info || '#0088FF'}08` }}
        >
          <Shield size={12} color={colors.info || '#0088FF'} />
          <span className="text-[10px] font-bold tracking-wider" style={{ color: colors.info || '#0088FF' }}>
            SECURITY
          </span>
        </div>

        <div className="p-3 space-y-2">
          {/* Encryption Toggle Row */}
          <div
            className="flex items-center justify-between px-3 py-2.5"
            style={{ backgroundColor: colors.background, border: `1px solid ${borderColor}` }}
          >
            <div>
              <div className="text-xs font-bold" style={{ color: colors.text }}>Encrypt API Credentials</div>
              <div className="text-[11px] font-mono mt-0.5" style={{ color: colors.textMuted }}>
                Store API keys encrypted in local database
              </div>
            </div>
            <TerminalToggle
              checked={encryptionEnabled}
              onChange={handleEncryptionToggle}
              activeColor={colors.success}
            />
          </div>

          {/* Encryption Info Panel */}
          <div
            className="flex items-start gap-2 px-3 py-2.5"
            style={{
              backgroundColor: encryptionEnabled
                ? `${colors.success}08`
                : `${colors.alert}08`,
              border: `1px solid ${encryptionEnabled
                ? `${colors.success}40`
                : `${colors.alert}40`}`,
            }}
          >
            {encryptionEnabled ? (
              <CheckCircle2 size={13} color={colors.success} className="flex-shrink-0 mt-0.5" />
            ) : (
              <AlertTriangle size={13} color={colors.alert} className="flex-shrink-0 mt-0.5" />
            )}
            <div className="text-[11px] font-mono" style={{ color: encryptionEnabled ? colors.success : colors.alert }}>
              {encryptionEnabled ? (
                <>
                  <span className="font-bold">ENCRYPTION ENABLED.</span>{' '}
                  <span style={{ color: colors.textMuted }}>
                    API keys and tokens are encrypted using AES-256-GCM before being stored locally.
                  </span>
                </>
              ) : (
                <>
                  <span className="font-bold">ENCRYPTION DISABLED.</span>{' '}
                  <span style={{ color: colors.textMuted }}>
                    API keys and tokens stored in plain text. Enable encryption for production use.
                  </span>
                </>
              )}
            </div>
          </div>

          {/* AES badge row */}
          <div className="flex items-center gap-2 px-1">
            <div
              className="flex items-center gap-1 px-2 py-1 text-[10px] font-mono font-bold"
              style={{ backgroundColor: `${colors.info || '#0088FF'}10`, border: `1px solid ${colors.info || '#0088FF'}30`, color: colors.info || '#0088FF' }}
            >
              <Shield size={10} />
              AES-256-GCM
            </div>
            <div className="text-[10px] font-mono" style={{ color: colors.textMuted }}>
              Industry-standard symmetric encryption
            </div>
          </div>
        </div>
      </div>

    </div>
  );
}
