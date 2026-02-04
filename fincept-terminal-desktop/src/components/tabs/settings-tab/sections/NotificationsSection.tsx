import React, { useState, useEffect } from 'react';
import { Bell, Save, Send, Search } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { notificationService, getAllProviders } from '@/services/notifications';
import type { NotificationEventType, NotificationProviderConfig } from '@/services/notifications';
import type { SettingsColors } from '../types';

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

const EVENT_TYPES: { type: NotificationEventType; label: string; color: string }[] = [
  { type: 'success', label: 'Success', color: FINCEPT.GREEN },
  { type: 'error', label: 'Error', color: FINCEPT.RED },
  { type: 'warning', label: 'Warning', color: FINCEPT.YELLOW },
  { type: 'info', label: 'Info', color: FINCEPT.CYAN },
];

interface NotificationsSectionProps {
  colors: SettingsColors;
  loading: boolean;
  setLoading: (loading: boolean) => void;
  showMessage: (type: 'success' | 'error', text: string) => void;
}

export function NotificationsSection({
  colors,
  loading,
  setLoading,
  showMessage,
}: NotificationsSectionProps) {
  const { t } = useTranslation('settings');
  const [eventFilters, setEventFilters] = useState<Record<NotificationEventType, boolean>>({
    success: true,
    error: true,
    warning: true,
    info: true,
  });
  const [providerConfigs, setProviderConfigs] = useState<NotificationProviderConfig[]>([]);
  const [localConfigs, setLocalConfigs] = useState<Record<string, Record<string, string>>>({});
  const [testingProvider, setTestingProvider] = useState<string | null>(null);
  const [detectingField, setDetectingField] = useState<string | null>(null);

  useEffect(() => {
    loadNotificationSettings();
  }, []);

  const loadNotificationSettings = async () => {
    try {
      if (!notificationService.isInitialized()) {
        await notificationService.initialize();
      }
      const settings = await notificationService.loadSettings();
      setEventFilters({ ...settings.eventFilters });
      setProviderConfigs([...settings.providers]);

      // Initialize local config state for editing
      const local: Record<string, Record<string, string>> = {};
      for (const p of settings.providers) {
        local[p.id] = { ...p.config };
      }
      setLocalConfigs(local);
    } catch (err) {
      console.error('Failed to load notification settings:', err);
    }
  };

  const handleToggleEventFilter = async (type: NotificationEventType) => {
    const newVal = !eventFilters[type];
    setEventFilters(prev => ({ ...prev, [type]: newVal }));
    try {
      await notificationService.saveEventFilter(type, newVal);
    } catch {
      showMessage('error', 'Failed to save event filter');
    }
  };

  const handleToggleProvider = async (providerId: string) => {
    const prov = providerConfigs.find(p => p.id === providerId);
    if (!prov) return;
    const newEnabled = !prov.enabled;
    setProviderConfigs(prev =>
      prev.map(p => (p.id === providerId ? { ...p, enabled: newEnabled } : p))
    );
    try {
      await notificationService.saveProviderEnabled(providerId, newEnabled);
      showMessage('success', `${prov.name} ${newEnabled ? 'enabled' : 'disabled'}`);
    } catch {
      showMessage('error', 'Failed to update provider');
    }
  };

  const handleSaveProviderConfig = async (providerId: string) => {
    setLoading(true);
    try {
      const config = localConfigs[providerId] || {};
      for (const [key, value] of Object.entries(config)) {
        await notificationService.saveProviderConfig(providerId, key, value);
      }
      setProviderConfigs(prev =>
        prev.map(p => (p.id === providerId ? { ...p, config: { ...config } } : p))
      );
      showMessage('success', t('notifications.saved'));
    } catch {
      showMessage('error', 'Failed to save configuration');
    } finally {
      setLoading(false);
    }
  };

  const handleTestProvider = async (providerId: string) => {
    setTestingProvider(providerId);
    try {
      // Use local (unsaved) config so user can test before saving
      const config = localConfigs[providerId] || {};
      const ok = await notificationService.testProvider(providerId, config);
      if (ok) {
        showMessage('success', t('notifications.testSuccess'));
      } else {
        showMessage('error', t('notifications.testFailed'));
      }
    } catch {
      showMessage('error', t('notifications.testFailed'));
    } finally {
      setTestingProvider(null);
    }
  };

  const handleAutoDetect = async (providerId: string, fieldKey: string, autoDetect: (config: Record<string, string>) => Promise<{ value: string; label: string } | null>) => {
    const detectId = `${providerId}_${fieldKey}`;
    setDetectingField(detectId);
    try {
      const currentConfig = localConfigs[providerId] || {};
      const result = await autoDetect(currentConfig);
      if (result) {
        updateLocalConfig(providerId, fieldKey, result.value);
        showMessage('success', `Detected: ${result.label} (${result.value})`);
      } else {
        showMessage('error', 'No chat found. Send a message to your bot first, then try again.');
      }
    } catch {
      showMessage('error', 'Detection failed. Check your bot token.');
    } finally {
      setDetectingField(null);
    }
  };

  const updateLocalConfig = (providerId: string, key: string, value: string) => {
    setLocalConfigs(prev => ({
      ...prev,
      [providerId]: { ...(prev[providerId] || {}), [key]: value },
    }));
  };

  const registeredProviders = getAllProviders();

  return (
    <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        marginBottom: '16px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
          <Bell size={14} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
          }}>
            {t('notifications.title')}
          </span>
        </div>
        <p style={{ fontSize: '10px', color: FINCEPT.GRAY, margin: 0, lineHeight: 1.5 }}>
          {t('notifications.description')}
        </p>
      </div>

      {/* Event Filters */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
        marginBottom: '16px',
      }}>
        <h3 style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          margin: 0,
          marginBottom: '4px',
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
        }}>
          {t('notifications.eventFilters')}
        </h3>
        <p style={{ fontSize: '9px', color: FINCEPT.GRAY, margin: 0, marginBottom: '12px', lineHeight: 1.4 }}>
          {t('notifications.eventFilterDescription')}
        </p>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '10px' }}>
          {EVENT_TYPES.map(({ type, label, color }) => (
            <div
              key={type}
              onClick={() => handleToggleEventFilter(type)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                padding: '6px 12px',
                backgroundColor: eventFilters[type] ? `${color}15` : FINCEPT.DARK_BG,
                border: `1px solid ${eventFilters[type] ? color : FINCEPT.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                transition: 'all 0.2s',
              }}
            >
              <div style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                backgroundColor: eventFilters[type] ? color : FINCEPT.MUTED,
                transition: 'background-color 0.2s',
              }} />
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: eventFilters[type] ? color : FINCEPT.GRAY,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
              }}>
                {label}
              </span>
            </div>
          ))}
        </div>
      </div>

      {/* Providers */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <h3 style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GRAY,
          margin: 0,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
        }}>
          {t('notifications.providers')}
        </h3>

        {registeredProviders.map(provider => {
          const provConfig = providerConfigs.find(p => p.id === provider.id);
          const isEnabled = provConfig?.enabled ?? false;
          const isTesting = testingProvider === provider.id;
          const local = localConfigs[provider.id] || {};

          return (
            <div
              key={provider.id}
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${isEnabled ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '12px',
                transition: 'border-color 0.2s',
              }}
            >
              {/* Provider Header */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: '12px',
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: FINCEPT.WHITE,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                  }}>
                    {provider.name}
                  </span>
                  <span style={{
                    fontSize: '8px',
                    fontWeight: 700,
                    padding: '2px 6px',
                    borderRadius: '2px',
                    backgroundColor: isEnabled ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                    color: isEnabled ? FINCEPT.GREEN : FINCEPT.RED,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                  }}>
                    {isEnabled ? t('notifications.enabled') : t('notifications.disabled')}
                  </span>
                </div>

                {/* Toggle */}
                <div
                  onClick={() => handleToggleProvider(provider.id)}
                  style={{
                    width: '36px',
                    height: '18px',
                    borderRadius: '9px',
                    backgroundColor: isEnabled ? FINCEPT.ORANGE : FINCEPT.MUTED,
                    cursor: 'pointer',
                    position: 'relative',
                    transition: 'background-color 0.2s',
                  }}
                >
                  <div style={{
                    width: '14px',
                    height: '14px',
                    borderRadius: '50%',
                    backgroundColor: FINCEPT.WHITE,
                    position: 'absolute',
                    top: '2px',
                    left: isEnabled ? '20px' : '2px',
                    transition: 'left 0.2s',
                  }} />
                </div>
              </div>

              {/* Config Fields */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                {provider.configFields.map(field => {
                  const detectId = `${provider.id}_${field.key}`;
                  const isDetecting = detectingField === detectId;

                  return (
                    <div key={field.key}>
                      <label style={{
                        fontSize: '9px',
                        fontWeight: 700,
                        color: FINCEPT.GRAY,
                        letterSpacing: '0.5px',
                        textTransform: 'uppercase',
                        display: 'block',
                        marginBottom: '4px',
                      }}>
                        {field.label}
                        {field.required && <span style={{ color: FINCEPT.RED, marginLeft: '4px' }}>*</span>}
                      </label>

                      <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                        <input
                          type={field.type}
                          value={local[field.key] || ''}
                          onChange={(e) => updateLocalConfig(provider.id, field.key, e.target.value)}
                          placeholder={field.placeholder}
                          style={{
                            flex: 1,
                            padding: '8px 10px',
                            backgroundColor: FINCEPT.DARK_BG,
                            color: FINCEPT.WHITE,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            borderRadius: '2px',
                            fontSize: '10px',
                            fontFamily: '"IBM Plex Mono", monospace',
                            outline: 'none',
                            transition: 'border-color 0.2s',
                          }}
                          onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                          onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                        />

                        {field.autoDetect && (
                          <button
                            onClick={() => handleAutoDetect(provider.id, field.key, field.autoDetect!)}
                            disabled={isDetecting}
                            style={{
                              padding: '8px 12px',
                              backgroundColor: 'transparent',
                              color: isDetecting ? FINCEPT.MUTED : FINCEPT.GREEN,
                              border: `1px solid ${isDetecting ? FINCEPT.MUTED : FINCEPT.GREEN}`,
                              borderRadius: '2px',
                              fontSize: '9px',
                              fontWeight: 700,
                              cursor: isDetecting ? 'not-allowed' : 'pointer',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '6px',
                              whiteSpace: 'nowrap',
                              letterSpacing: '0.5px',
                              textTransform: 'uppercase',
                              opacity: isDetecting ? 0.5 : 1,
                              transition: 'all 0.2s',
                            }}
                          >
                            {isDetecting ? (
                              <>
                                <div style={{
                                  width: '12px',
                                  height: '12px',
                                  border: `2px solid ${FINCEPT.GREEN}`,
                                  borderTopColor: 'transparent',
                                  borderRadius: '50%',
                                  animation: 'spin 0.8s linear infinite',
                                }} />
                                DETECTING...
                              </>
                            ) : (
                              <>
                                <Search size={12} />
                                {field.autoDetectLabel || 'DETECT'}
                              </>
                            )}
                          </button>
                        )}
                      </div>

                      {field.autoDetectHint && (
                        <p style={{
                          fontSize: '8px',
                          color: FINCEPT.MUTED,
                          margin: '4px 0 0 0',
                          lineHeight: 1.4,
                          fontStyle: 'italic',
                        }}>
                          {field.autoDetectHint}
                        </p>
                      )}
                    </div>
                  );
                })}
              </div>

              {/* Action Buttons */}
              <div style={{ display: 'flex', gap: '8px', marginTop: '12px' }}>
                <button
                  onClick={() => handleSaveProviderConfig(provider.id)}
                  disabled={loading}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: loading ? FINCEPT.MUTED : FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: loading ? 'not-allowed' : 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    whiteSpace: 'nowrap',
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    opacity: loading ? 0.5 : 1,
                    transition: 'all 0.2s',
                  }}
                >
                  <Save size={12} />
                  {t('buttons.save')}
                </button>

                <button
                  onClick={() => handleTestProvider(provider.id)}
                  disabled={isTesting}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: 'transparent',
                    color: isTesting ? FINCEPT.MUTED : FINCEPT.CYAN,
                    border: `1px solid ${isTesting ? FINCEPT.MUTED : FINCEPT.CYAN}`,
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: isTesting ? 'not-allowed' : 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    whiteSpace: 'nowrap',
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    opacity: isTesting ? 0.5 : 1,
                    transition: 'all 0.2s',
                  }}
                >
                  {isTesting ? (
                    <>
                      <div style={{
                        width: '12px',
                        height: '12px',
                        border: `2px solid ${FINCEPT.CYAN}`,
                        borderTopColor: 'transparent',
                        borderRadius: '50%',
                        animation: 'spin 0.8s linear infinite',
                      }} />
                      TESTING...
                    </>
                  ) : (
                    <>
                      <Send size={12} />
                      {t('notifications.testButton')}
                    </>
                  )}
                </button>
              </div>
            </div>
          );
        })}
      </div>

      <style>{`
        @keyframes spin {
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}
