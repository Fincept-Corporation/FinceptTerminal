// Zerodha Kite Configuration Form
// Collects Kite Connect API credentials for trading and market data access

import React, { useState } from 'react';
import { ExternalLink, AlertCircle } from 'lucide-react';

interface KiteFormProps {
  onSubmit: (config: { args: string[]; env: Record<string, string> }) => void;
  onCancel: () => void;
}

const KiteForm: React.FC<KiteFormProps> = ({ onSubmit, onCancel }) => {
  const [apiKey, setApiKey] = useState('');
  const [apiSecret, setApiSecret] = useState('');

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_RED = '#ef4444';

  const handleSubmit = () => {
    if (!apiKey.trim() || !apiSecret.trim()) {
      alert('Please fill in all required fields');
      return;
    }

    onSubmit({
      args: [],
      env: {
        KITE_API_KEY: apiKey.trim(),
        KITE_API_SECRET: apiSecret.trim()
      }
    });
  };

  const openKiteConsole = () => {
    window.open('https://developers.kite.trade/', '_blank');
  };

  return (
    <div>
      {/* Warning Banner */}
      <div style={{
        backgroundColor: `${BLOOMBERG_YELLOW}20`,
        border: `1px solid ${BLOOMBERG_YELLOW}`,
        borderRadius: '4px',
        padding: '10px',
        marginBottom: '16px',
        display: 'flex',
        gap: '8px',
        alignItems: 'flex-start'
      }}>
        <AlertCircle size={16} color={BLOOMBERG_YELLOW} style={{ flexShrink: 0, marginTop: '2px' }} />
        <div style={{ fontSize: '10px', color: BLOOMBERG_WHITE, lineHeight: '1.5' }}>
          <strong>Trading API Integration</strong>
          <div style={{ marginTop: '4px', color: BLOOMBERG_GRAY }}>
            This MCP server connects to Zerodha Kite for live trading. Ensure you have a Kite Connect app registered.
            You'll need to authenticate through the browser on first use.
          </div>
        </div>
      </div>

      {/* Description */}
      <div style={{
        color: BLOOMBERG_WHITE,
        fontSize: '10px',
        marginBottom: '12px',
        lineHeight: '1.5'
      }}>
        Configure your Kite Connect API credentials. Get your API key and secret from the Kite Connect Developer Console.
      </div>

      {/* Get Credentials Link */}
      <button
        onClick={openKiteConsole}
        style={{
          backgroundColor: 'transparent',
          border: `1px solid ${BLOOMBERG_ORANGE}`,
          color: BLOOMBERG_ORANGE,
          padding: '8px 12px',
          fontSize: '10px',
          cursor: 'pointer',
          borderRadius: '3px',
          marginBottom: '16px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          fontWeight: 'bold'
        }}
      >
        <ExternalLink size={12} />
        OPEN KITE CONNECT CONSOLE
      </button>

      {/* API Key */}
      <div style={{ marginBottom: '12px' }}>
        <label style={{
          display: 'block',
          color: BLOOMBERG_ORANGE,
          fontSize: '10px',
          marginBottom: '4px',
          fontWeight: 'bold'
        }}>
          API KEY *
        </label>
        <input
          type="text"
          value={apiKey}
          onChange={(e) => setApiKey(e.target.value)}
          placeholder="Enter your Kite Connect API Key"
          style={{
            width: '100%',
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${apiKey ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '8px',
            fontSize: '10px',
            fontFamily: 'Consolas, monospace',
            borderRadius: '3px'
          }}
        />
      </div>

      {/* API Secret */}
      <div style={{ marginBottom: '12px' }}>
        <label style={{
          display: 'block',
          color: BLOOMBERG_ORANGE,
          fontSize: '10px',
          marginBottom: '4px',
          fontWeight: 'bold'
        }}>
          API SECRET *
        </label>
        <input
          type="password"
          value={apiSecret}
          onChange={(e) => setApiSecret(e.target.value)}
          placeholder="Enter your Kite Connect API Secret"
          style={{
            width: '100%',
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${apiSecret ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '8px',
            fontSize: '10px',
            fontFamily: 'Consolas, monospace',
            borderRadius: '3px'
          }}
        />
      </div>

      {/* Info Box */}
      <div style={{
        marginTop: '16px',
        padding: '10px',
        backgroundColor: BLOOMBERG_DARK_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        borderRadius: '4px'
      }}>
        <div style={{
          color: BLOOMBERG_ORANGE,
          fontSize: '9px',
          marginBottom: '6px',
          fontWeight: 'bold'
        }}>
          HOW TO GET CREDENTIALS:
        </div>
        <ol style={{
          color: BLOOMBERG_GRAY,
          fontSize: '9px',
          lineHeight: '1.6',
          margin: '0',
          paddingLeft: '16px'
        }}>
          <li>Visit <span style={{ color: BLOOMBERG_YELLOW }}>developers.kite.trade</span></li>
          <li>Create or login to your Kite Connect account</li>
          <li>Create a new app or select existing app</li>
          <li>Copy your <strong>API Key</strong> and <strong>API Secret</strong></li>
          <li>Set redirect URL to <span style={{ color: BLOOMBERG_YELLOW }}>http://localhost:8080/callback</span></li>
        </ol>
      </div>

      {/* Security Notice */}
      <div style={{
        marginTop: '12px',
        padding: '8px',
        backgroundColor: `${BLOOMBERG_RED}15`,
        border: `1px solid ${BLOOMBERG_RED}`,
        borderRadius: '4px',
        fontSize: '9px',
        color: BLOOMBERG_GRAY,
        lineHeight: '1.5'
      }}>
        <strong style={{ color: BLOOMBERG_RED }}>🔒 Security:</strong> Your API credentials are stored locally and encrypted.
        Never share your API Secret. You'll need to authenticate via browser on first use.
      </div>

      {/* Actions */}
      <div style={{
        display: 'flex',
        gap: '8px',
        justifyContent: 'flex-end',
        marginTop: '16px',
        paddingTop: '12px',
        borderTop: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <button
          onClick={onCancel}
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '8px 16px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '3px',
            fontWeight: 'bold'
          }}
        >
          CANCEL
        </button>
        <button
          onClick={handleSubmit}
          disabled={!apiKey.trim() || !apiSecret.trim()}
          style={{
            backgroundColor: (apiKey.trim() && apiSecret.trim()) ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
            border: 'none',
            color: 'black',
            padding: '8px 16px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: (apiKey.trim() && apiSecret.trim()) ? 'pointer' : 'not-allowed',
            borderRadius: '3px',
            opacity: (apiKey.trim() && apiSecret.trim()) ? 1 : 0.5
          }}
        >
          INSTALL & CONNECT
        </button>
      </div>
    </div>
  );
};

export default KiteForm;
