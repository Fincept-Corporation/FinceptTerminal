// ApiKeyPanel Component - Configure API keys for data sources

import React from 'react';
import { Key, RefreshCw, Settings } from 'lucide-react';
import { FINCEPT } from '../constants';

interface ApiKeyPanelProps {
  apiKey: string;
  setApiKey: (key: string) => void;
  onSave: () => void;
  onClose: () => void;
  saving: boolean;
  sourceName: string;
  sourceColor: string;
  portalUrl: string;
}

export function ApiKeyPanel({
  apiKey,
  setApiKey,
  onSave,
  onClose,
  saving,
  sourceName,
  sourceColor,
  portalUrl,
}: ApiKeyPanelProps) {
  return (
    <div
      style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
      }}
    >
      <Key size={16} color={sourceColor} />
      <div style={{ flex: 1 }}>
        <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '4px' }}>
          {sourceName} API KEY
        </div>
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
          Get your API key from <span style={{ color: sourceColor }}>{portalUrl}</span>
        </div>
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <input
            type="password"
            value={apiKey}
            onChange={(e) => setApiKey(e.target.value)}
            placeholder={`Enter your ${sourceName} API Key...`}
            style={{
              flex: 1,
              padding: '8px 12px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontFamily: '"IBM Plex Mono", monospace',
              outline: 'none',
            }}
            onFocus={(e) => e.currentTarget.style.borderColor = sourceColor}
            onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
          />
          <button
            onClick={onSave}
            disabled={saving || !apiKey.trim()}
            style={{
              padding: '8px 16px',
              backgroundColor: sourceColor,
              color: FINCEPT.WHITE,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: saving || !apiKey.trim() ? 'not-allowed' : 'pointer',
              opacity: saving || !apiKey.trim() ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            {saving ? <RefreshCw size={12} className="animate-spin" /> : <Settings size={12} />}
            SAVE KEY
          </button>
          <button
            onClick={onClose}
            style={{
              padding: '8px 12px',
              backgroundColor: 'transparent',
              color: FINCEPT.GRAY,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
            }}
          >
            CLOSE
          </button>
        </div>
      </div>
    </div>
  );
}

export default ApiKeyPanel;
