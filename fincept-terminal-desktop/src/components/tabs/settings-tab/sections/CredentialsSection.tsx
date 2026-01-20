import React from 'react';
import { Save } from 'lucide-react';
import { sqliteService, PREDEFINED_API_KEYS, type ApiKeys } from '@/services/core/sqliteService';
import { useTranslation } from 'react-i18next';
import type { SettingsColors } from '../types';

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
    <div>
      <div style={{ marginBottom: '24px' }}>
        <h2 style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
          {t('credentials.title')}
        </h2>
        <p style={{ color: colors.text, fontSize: '10px' }}>
          {t('credentials.description')}
        </p>
      </div>

      <div style={{ display: 'grid', gap: '16px' }}>
        {PREDEFINED_API_KEYS.map(({ key, label, description }) => (
          <div
            key={key}
            style={{
              background: colors.panel,
              border: '1px solid #1a1a1a',
              padding: '16px',
              borderRadius: '4px'
            }}
          >
            <h3 style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
              {label}
            </h3>
            <p style={{ color: colors.text, fontSize: '9px', marginBottom: '12px', opacity: 0.7 }}>
              {description}
            </p>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: '12px', alignItems: 'end' }}>
              <div>
                <label style={{ color: colors.text, fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                  {t('credentials.apiKey')}
                </label>
                <input
                  type="password"
                  value={apiKeys[key] || ''}
                  onChange={(e) => setApiKeys({ ...apiKeys, [key]: e.target.value })}
                  placeholder={t('credentials.enterValue')}
                  style={{
                    width: '100%',
                    background: colors.background,
                    border: '1px solid #2a2a2a',
                    color: colors.text,
                    padding: '8px',
                    fontSize: '10px',
                    borderRadius: '3px',
                    fontFamily: 'monospace'
                  }}
                />
              </div>
              <button
                onClick={() => handleSaveApiKeyField(key, apiKeys[key] || '')}
                disabled={loading || !apiKeys[key]}
                style={{
                  background: apiKeys[key] ? colors.primary : '#333',
                  color: colors.text,
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: (loading || !apiKeys[key]) ? 'not-allowed' : 'pointer',
                  borderRadius: '3px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  opacity: (loading || !apiKeys[key]) ? 0.5 : 1,
                  whiteSpace: 'nowrap'
                }}
              >
                <Save size={14} />
                {t('buttons.save')}
              </button>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
