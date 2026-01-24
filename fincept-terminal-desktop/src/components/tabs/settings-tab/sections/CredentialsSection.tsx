import React from 'react';
import { Save, Key } from 'lucide-react';
import { sqliteService, PREDEFINED_API_KEYS, type ApiKeys } from '@/services/core/sqliteService';
import { useTranslation } from 'react-i18next';
import type { SettingsColors } from '../types';

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
};

interface CredentialsSectionProps {
  colors: SettingsColors;
  apiKeys: ApiKeys;
  setApiKeys: (keys: ApiKeys) => void;
  loading: boolean;
  setLoading: (loading: boolean) => void;
  showMessage: (type: 'success' | 'error', text: string) => void;
}

export function CredentialsSection({
  colors,
  apiKeys,
  setApiKeys,
  loading,
  setLoading,
  showMessage,
}: CredentialsSectionProps) {
  const { t } = useTranslation('settings');

  const handleSaveApiKeyField = async (keyName: string, value: string) => {
    setLoading(true);
    try {
      await sqliteService.setApiKey(keyName, value);
      setApiKeys({ ...apiKeys, [keyName]: value });
      showMessage('success', `${keyName} saved`);
    } catch (error) {
      showMessage('error', 'Failed to save');
    } finally {
      setLoading(false);
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
          <Key size={14} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            {t('credentials.title')}
          </span>
        </div>
        <p style={{
          fontSize: '10px',
          color: FINCEPT.GRAY,
          margin: 0,
          lineHeight: 1.5
        }}>
          {t('credentials.description')}
        </p>
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {PREDEFINED_API_KEYS.map(({ key, label, description }) => (
          <div
            key={key}
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              padding: '12px'
            }}
          >
            {/* Label */}
            <div style={{ marginBottom: '8px' }}>
              <h3 style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.WHITE,
                margin: 0,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                textTransform: 'uppercase'
              }}>
                {label}
              </h3>
              <p style={{
                fontSize: '9px',
                color: FINCEPT.GRAY,
                margin: 0,
                lineHeight: 1.4
              }}>
                {description}
              </p>
            </div>

            {/* Input and Button */}
            <div style={{ display: 'flex', gap: '8px', alignItems: 'end' }}>
              <div style={{ flex: 1 }}>
                <label style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  display: 'block',
                  marginBottom: '4px'
                }}>
                  {t('credentials.apiKey')}
                </label>
                <input
                  type="password"
                  value={apiKeys[key] || ''}
                  onChange={(e) => setApiKeys({ ...apiKeys, [key]: e.target.value })}
                  placeholder={t('credentials.enterValue')}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    outline: 'none',
                    transition: 'border-color 0.2s'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
                />
              </div>
              {/* Primary Button */}
              <button
                onClick={() => handleSaveApiKeyField(key, apiKeys[key] || '')}
                disabled={loading || !apiKeys[key]}
                style={{
                  padding: '8px 16px',
                  backgroundColor: (loading || !apiKeys[key]) ? FINCEPT.MUTED : FINCEPT.ORANGE,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: (loading || !apiKeys[key]) ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  whiteSpace: 'nowrap',
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  opacity: (loading || !apiKeys[key]) ? 0.5 : 1,
                  transition: 'all 0.2s'
                }}
              >
                <Save size={12} />
                {t('buttons.save')}
              </button>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
