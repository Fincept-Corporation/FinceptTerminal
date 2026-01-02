/**
 * Broker Configuration Tab - Bloomberg Style
 * Professional broker setup interface
 */

import React, { useState, useEffect } from 'react';
import { BrokerType, BrokerCredentials } from '../core/types';
import { authManager } from '../services/AuthManager';
import { useBrokerState } from '../hooks/useBrokerState';
import { Settings, CheckCircle2, XCircle, ChevronDown } from 'lucide-react';

// Add CSS for spinner animation
if (typeof document !== 'undefined') {
  const style = document.createElement('style');
  style.textContent = `
    @keyframes spin {
      from { transform: rotate(0deg); }
      to { transform: rotate(360deg); }
    }
  `;
  if (!document.head.querySelector('style[data-spinner]')) {
    style.setAttribute('data-spinner', 'true');
    document.head.appendChild(style);
  }
}

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface BrokerConfigTabProps {
  selectedBrokers: BrokerType[];
  onBrokersChange: (brokers: BrokerType[]) => void;
  brokerState: ReturnType<typeof useBrokerState>;
}

const BrokerConfigTab: React.FC<BrokerConfigTabProps> = ({
  selectedBrokers,
  onBrokersChange,
  brokerState,
}) => {
  const [expandedBroker, setExpandedBroker] = useState<BrokerType | null>(null);

  const toggleBroker = (brokerId: BrokerType) => {
    setExpandedBroker(expandedBroker === brokerId ? null : brokerId);
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'monospace',
      backgroundColor: BLOOMBERG.DARK_BG
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Settings size={16} color={BLOOMBERG.ORANGE} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: BLOOMBERG.WHITE,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            BROKER CONFIGURATION
          </span>
        </div>
        <div style={{
          fontSize: '9px',
          color: BLOOMBERG.GRAY,
          marginTop: '4px',
          letterSpacing: '0.3px'
        }}>
          ALL CREDENTIALS ENCRYPTED AND STORED SECURELY
        </div>
      </div>

      {/* Broker List */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Fyers */}
          <BrokerCard
            brokerId="fyers"
            title="FYERS"
            description="Indian stock broker with API support"
            isExpanded={expandedBroker === 'fyers'}
            onToggle={() => toggleBroker('fyers')}
            authStatus={brokerState.authStatus.get('fyers')}
          />

          {/* Kite (Zerodha) */}
          <BrokerCard
            brokerId="kite"
            title="KITE (ZERODHA)"
            description="India's largest stock broker"
            isExpanded={expandedBroker === 'kite'}
            onToggle={() => toggleBroker('kite')}
            authStatus={brokerState.authStatus.get('kite')}
          />

          {/* Alpaca Markets */}
          <BrokerCard
            brokerId="alpaca"
            title="ALPACA MARKETS"
            description="US stock broker with paper & live trading"
            isExpanded={expandedBroker === 'alpaca'}
            onToggle={() => toggleBroker('alpaca')}
            authStatus={brokerState.authStatus.get('alpaca')}
          />
        </div>
      </div>
    </div>
  );
};

// Broker Configuration Card
const BrokerCard: React.FC<{
  brokerId: BrokerType;
  title: string;
  description: string;
  isExpanded: boolean;
  onToggle: () => void;
  authStatus?: any;
}> = ({ brokerId, title, description, isExpanded, onToggle, authStatus }) => {
  const [formData, setFormData] = useState<any>({});
  const [saving, setSaving] = useState(false);
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const isAuthenticated = authStatus?.authenticated || false;

  // Load saved credentials when card expands
  useEffect(() => {
    if (isExpanded && !formData.apiKey) {
      loadSavedCredentials();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [isExpanded, brokerId]);

  const loadSavedCredentials = async () => {
    setLoading(true);
    try {
      const credentials = await authManager.loadCredentials(brokerId);
      if (credentials) {
        const data: any = {
          apiKey: credentials.apiKey,
          apiSecret: credentials.apiSecret,
          accessToken: credentials.accessToken,
        };

        if (brokerId === 'fyers' && credentials.additionalData) {
          data.clientId = credentials.additionalData.clientId;
          data.pin = credentials.additionalData.pin;
          data.totpKey = credentials.additionalData.totpKey;
          data.appType = credentials.additionalData.appType || '100';
        }

        if (brokerId === 'alpaca' && credentials.additionalData) {
          data.isPaper = credentials.additionalData.isPaper !== false;
        }

        setFormData(data);
      }
    } catch (error) {
      console.error('Failed to load credentials:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleSave = async () => {
    setSaving(true);
    setMessage(null);

    try {
      // Validate required fields
      if (!formData.apiKey || !formData.apiSecret) {
        setMessage({ type: 'error', text: 'API Key and API Secret are required' });
        setSaving(false);
        return;
      }

      // Kite requires access token
      if (brokerId === 'kite' && !formData.accessToken) {
        setMessage({ type: 'error', text: 'Access Token is required for Kite. Please generate it using the login URL.' });
        setSaving(false);
        return;
      }

      const credentials: BrokerCredentials = {
        brokerId,
        apiKey: formData.apiKey || '',
        apiSecret: formData.apiSecret || '',
        accessToken: formData.accessToken,
        additionalData: brokerId === 'fyers' ? {
          clientId: formData.clientId,
          pin: formData.pin,
          totpKey: formData.totpKey,
          appType: formData.appType || '100',
        } : brokerId === 'alpaca' ? {
          isPaper: formData.isPaper !== false,
        } : {},
      };

      await authManager.saveCredentials(credentials);
      await authManager.initializeBroker(brokerId);

      setMessage({ type: 'success', text: `${title} connected successfully` });

      // Keep the form data in state after successful save
      // This ensures the form remains populated even after re-renders
    } catch (error: any) {
      setMessage({ type: 'error', text: error.message || 'Connection failed' });
    } finally {
      setSaving(false);
    }
  };

  const handleDisconnect = async () => {
    try {
      await authManager.disconnectBroker(brokerId);
      // Note: We don't clear formData here, so credentials remain visible in the form
      // This allows users to see what was saved and easily reconnect
      setMessage({ type: 'success', text: `${title} disconnected` });
    } catch (error: any) {
      setMessage({ type: 'error', text: error.message || 'Disconnect failed' });
    }
  };

  return (
    <div style={{
      backgroundColor: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '12px 16px',
          cursor: 'pointer',
          backgroundColor: BLOOMBERG.PANEL_BG,
          transition: 'background-color 0.15s ease',
          borderBottom: isExpanded ? `1px solid ${BLOOMBERG.BORDER}` : 'none'
        }}
        onClick={onToggle}
        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER}
        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.PANEL_BG}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {isAuthenticated ? (
            <CheckCircle2 size={14} color={BLOOMBERG.GREEN} />
          ) : (
            <XCircle size={14} color={BLOOMBERG.MUTED} />
          )}
          <div>
            <div style={{
              fontSize: '11px',
              fontWeight: 700,
              color: BLOOMBERG.WHITE,
              letterSpacing: '0.5px'
            }}>
              {title}
            </div>
            <div style={{
              fontSize: '9px',
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.3px',
              marginTop: '2px'
            }}>
              {description}
            </div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {isAuthenticated && (
            <span style={{
              fontSize: '9px',
              color: BLOOMBERG.GREEN,
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              CONNECTED
            </span>
          )}
          <ChevronDown
            size={14}
            color={BLOOMBERG.GRAY}
            style={{
              transform: isExpanded ? 'rotate(180deg)' : 'rotate(0deg)',
              transition: 'transform 0.2s ease'
            }}
          />
        </div>
      </div>

      {/* Configuration Form */}
      {isExpanded && (
        <div style={{
          padding: '16px',
          backgroundColor: BLOOMBERG.INPUT_BG
        }}>
          {loading && (
            <div style={{
              marginBottom: '12px',
              padding: '10px 12px',
              border: `1px solid ${BLOOMBERG.BLUE}`,
              backgroundColor: `${BLOOMBERG.BLUE}10`,
              fontSize: '10px',
              fontWeight: 600,
              color: BLOOMBERG.BLUE,
              letterSpacing: '0.3px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <div style={{
                width: '12px',
                height: '12px',
                border: `2px solid ${BLOOMBERG.BLUE}`,
                borderTopColor: 'transparent',
                borderRadius: '50%',
                animation: 'spin 0.8s linear infinite'
              }} />
              Loading saved credentials...
            </div>
          )}

          {message && (
            <div style={{
              marginBottom: '12px',
              padding: '10px 12px',
              border: `1px solid ${message.type === 'success' ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
              backgroundColor: message.type === 'success'
                ? `${BLOOMBERG.GREEN}10`
                : `${BLOOMBERG.RED}10`,
              fontSize: '10px',
              fontWeight: 600,
              color: message.type === 'success' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
              letterSpacing: '0.3px'
            }}>
              {message.text}
            </div>
          )}

          {brokerId === 'fyers' && (
            <FyersConfigForm
              formData={formData}
              onChange={setFormData}
              onSave={handleSave}
              onDisconnect={handleDisconnect}
              saving={saving}
              isAuthenticated={isAuthenticated}
            />
          )}

          {brokerId === 'kite' && (
            <KiteConfigForm
              formData={formData}
              onChange={setFormData}
              onSave={handleSave}
              onDisconnect={handleDisconnect}
              saving={saving}
              isAuthenticated={isAuthenticated}
            />
          )}

          {brokerId === 'alpaca' && (
            <AlpacaConfigForm
              formData={formData}
              onChange={setFormData}
              onSave={handleSave}
              onDisconnect={handleDisconnect}
              saving={saving}
              isAuthenticated={isAuthenticated}
            />
          )}
        </div>
      )}
    </div>
  );
};

// Fyers Configuration Form
const FyersConfigForm: React.FC<{
  formData: any;
  onChange: (data: any) => void;
  onSave: () => void;
  onDisconnect: () => void;
  saving: boolean;
  isAuthenticated: boolean;
}> = ({ formData, onChange, onSave, onDisconnect, saving, isAuthenticated }) => {
  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    marginBottom: '6px',
    letterSpacing: '0.5px',
    textTransform: 'uppercase'
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: BLOOMBERG.DARK_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '0',
    color: BLOOMBERG.WHITE,
    fontSize: '11px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
    transition: 'border-color 0.15s ease'
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <div>
        <label style={labelStyle}>APP ID</label>
        <input
          type="text"
          value={formData.apiKey || ''}
          onChange={(e) => onChange({ ...formData, apiKey: e.target.value })}
          style={inputStyle}
          placeholder="Your Fyers App ID"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>APP SECRET</label>
        <input
          type="password"
          value={formData.apiSecret || ''}
          onChange={(e) => onChange({ ...formData, apiSecret: e.target.value })}
          style={inputStyle}
          placeholder="Your Fyers App Secret"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>CLIENT ID</label>
        <input
          type="text"
          value={formData.clientId || ''}
          onChange={(e) => onChange({ ...formData, clientId: e.target.value })}
          style={inputStyle}
          placeholder="Your Fyers Client ID"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>PIN</label>
        <input
          type="password"
          value={formData.pin || ''}
          onChange={(e) => onChange({ ...formData, pin: e.target.value })}
          style={inputStyle}
          placeholder="Your Fyers PIN"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>TOTP KEY (OPTIONAL)</label>
        <input
          type="text"
          value={formData.totpKey || ''}
          onChange={(e) => onChange({ ...formData, totpKey: e.target.value })}
          style={inputStyle}
          placeholder="TOTP Secret for 2FA"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
        <div style={{
          fontSize: '9px',
          color: BLOOMBERG.MUTED,
          marginTop: '4px',
          letterSpacing: '0.3px'
        }}>
          For automated login without manual 2FA
        </div>
      </div>

      <div style={{ display: 'flex', gap: '8px', marginTop: '8px' }}>
        <button
          onClick={onSave}
          disabled={saving}
          style={{
            flex: 1,
            padding: '10px 16px',
            backgroundColor: saving ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE,
            border: `1px solid ${saving ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE}`,
            borderRadius: '0',
            color: BLOOMBERG.DARK_BG,
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: saving ? 'not-allowed' : 'pointer',
            transition: 'all 0.15s ease',
            fontFamily: 'monospace',
            outline: 'none',
            textTransform: 'uppercase'
          }}
          onMouseEnter={(e) => {
            if (!saving) e.currentTarget.style.backgroundColor = BLOOMBERG.YELLOW;
          }}
          onMouseLeave={(e) => {
            if (!saving) e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE;
          }}
        >
          {saving ? 'CONNECTING...' : isAuthenticated ? 'UPDATE' : 'CONNECT'}
        </button>

        {isAuthenticated && (
          <button
            onClick={onDisconnect}
            style={{
              padding: '10px 16px',
              backgroundColor: 'transparent',
              border: `1px solid ${BLOOMBERG.RED}`,
              borderRadius: '0',
              color: BLOOMBERG.RED,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              fontFamily: 'monospace',
              outline: 'none',
              textTransform: 'uppercase'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.RED;
              e.currentTarget.style.color = BLOOMBERG.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = BLOOMBERG.RED;
            }}
          >
            DISCONNECT
          </button>
        )}
      </div>
    </div>
  );
};

// Kite Configuration Form
const KiteConfigForm: React.FC<{
  formData: any;
  onChange: (data: any) => void;
  onSave: () => void;
  onDisconnect: () => void;
  saving: boolean;
  isAuthenticated: boolean;
}> = ({ formData, onChange, onSave, onDisconnect, saving, isAuthenticated }) => {
  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    marginBottom: '6px',
    letterSpacing: '0.5px',
    textTransform: 'uppercase'
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: BLOOMBERG.DARK_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '0',
    color: BLOOMBERG.WHITE,
    fontSize: '11px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
    transition: 'border-color 0.15s ease'
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <div>
        <label style={labelStyle}>API KEY</label>
        <input
          type="text"
          value={formData.apiKey || ''}
          onChange={(e) => onChange({ ...formData, apiKey: e.target.value })}
          style={inputStyle}
          placeholder="Your Kite API Key"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>API SECRET</label>
        <input
          type="password"
          value={formData.apiSecret || ''}
          onChange={(e) => onChange({ ...formData, apiSecret: e.target.value })}
          style={inputStyle}
          placeholder="Your Kite API Secret"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>ACCESS TOKEN (REQUIRED)</label>
        <input
          type="text"
          value={formData.accessToken || ''}
          onChange={(e) => onChange({ ...formData, accessToken: e.target.value })}
          style={inputStyle}
          placeholder="Paste your access token here"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
        <div style={{
          fontSize: '9px',
          color: BLOOMBERG.CYAN,
          marginTop: '6px',
          letterSpacing: '0.3px',
          padding: '8px',
          border: `1px solid ${BLOOMBERG.BORDER}`,
          backgroundColor: BLOOMBERG.PANEL_BG
        }}>
          <div style={{ marginBottom: '4px', fontWeight: 700, color: BLOOMBERG.ORANGE }}>
            HOW TO GET ACCESS TOKEN:
          </div>
          <div>1. Click "Generate Login URL" button below</div>
          <div>2. Complete login on Kite Connect website</div>
          <div>3. Copy the access token from the redirect URL</div>
          <div>4. Paste it in the field above and click CONNECT</div>
        </div>

        {formData.apiKey && !isAuthenticated && (
          <button
            onClick={() => {
              const loginUrl = `https://kite.zerodha.com/connect/login?api_key=${formData.apiKey}&v=3`;
              window.open(loginUrl, '_blank');
            }}
            style={{
              marginTop: '8px',
              width: '100%',
              padding: '8px 12px',
              backgroundColor: 'transparent',
              border: `1px solid ${BLOOMBERG.CYAN}`,
              borderRadius: '0',
              color: BLOOMBERG.CYAN,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              fontFamily: 'monospace',
              outline: 'none',
              textTransform: 'uppercase'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.CYAN;
              e.currentTarget.style.color = BLOOMBERG.DARK_BG;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = BLOOMBERG.CYAN;
            }}
          >
            🔗 GENERATE LOGIN URL
          </button>
        )}
      </div>

      <div style={{ display: 'flex', gap: '8px', marginTop: '8px' }}>
        <button
          onClick={onSave}
          disabled={saving}
          style={{
            flex: 1,
            padding: '10px 16px',
            backgroundColor: saving ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE,
            border: `1px solid ${saving ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE}`,
            borderRadius: '0',
            color: BLOOMBERG.DARK_BG,
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: saving ? 'not-allowed' : 'pointer',
            transition: 'all 0.15s ease',
            fontFamily: 'monospace',
            outline: 'none',
            textTransform: 'uppercase'
          }}
          onMouseEnter={(e) => {
            if (!saving) e.currentTarget.style.backgroundColor = BLOOMBERG.YELLOW;
          }}
          onMouseLeave={(e) => {
            if (!saving) e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE;
          }}
        >
          {saving ? 'CONNECTING...' : isAuthenticated ? 'UPDATE' : 'CONNECT'}
        </button>

        {isAuthenticated && (
          <button
            onClick={onDisconnect}
            style={{
              padding: '10px 16px',
              backgroundColor: 'transparent',
              border: `1px solid ${BLOOMBERG.RED}`,
              borderRadius: '0',
              color: BLOOMBERG.RED,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              fontFamily: 'monospace',
              outline: 'none',
              textTransform: 'uppercase'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.RED;
              e.currentTarget.style.color = BLOOMBERG.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = BLOOMBERG.RED;
            }}
          >
            DISCONNECT
          </button>
        )}
      </div>
    </div>
  );
};

// Alpaca Configuration Form
const AlpacaConfigForm: React.FC<{
  formData: any;
  onChange: (data: any) => void;
  onSave: () => void;
  onDisconnect: () => void;
  saving: boolean;
  isAuthenticated: boolean;
}> = ({ formData, onChange, onSave, onDisconnect, saving, isAuthenticated }) => {
  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    marginBottom: '6px',
    letterSpacing: '0.5px',
    textTransform: 'uppercase'
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: BLOOMBERG.DARK_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '0',
    color: BLOOMBERG.WHITE,
    fontSize: '11px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
    transition: 'border-color 0.15s ease'
  };

  const isPaper = formData.isPaper !== false; // default to paper

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Trading Mode Selection */}
      <div>
        <label style={labelStyle}>TRADING MODE</label>
        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={() => onChange({ ...formData, isPaper: true })}
            style={{
              flex: 1,
              padding: '10px',
              background: isPaper ? BLOOMBERG.GREEN : 'transparent',
              border: `1px solid ${isPaper ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}`,
              color: isPaper ? BLOOMBERG.DARK_BG : BLOOMBERG.GREEN,
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s',
              letterSpacing: '0.5px',
              fontFamily: 'monospace',
              textTransform: 'uppercase'
            }}
          >
            📊 PAPER TRADING
          </button>
          <button
            onClick={() => onChange({ ...formData, isPaper: false })}
            style={{
              flex: 1,
              padding: '10px',
              background: !isPaper ? BLOOMBERG.RED : 'transparent',
              border: `1px solid ${!isPaper ? BLOOMBERG.RED : BLOOMBERG.BORDER}`,
              color: !isPaper ? BLOOMBERG.WHITE : BLOOMBERG.RED,
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s',
              letterSpacing: '0.5px',
              fontFamily: 'monospace',
              textTransform: 'uppercase'
            }}
          >
            🔴 LIVE TRADING
          </button>
        </div>
        <div style={{
          fontSize: '9px',
          color: BLOOMBERG.MUTED,
          marginTop: '6px',
          padding: '8px',
          border: `1px solid ${BLOOMBERG.BORDER}`,
          backgroundColor: BLOOMBERG.PANEL_BG,
          letterSpacing: '0.3px'
        }}>
          {isPaper
            ? '✓ Safe simulation mode. Uses paper-api.alpaca.markets'
            : '⚠️ Real money trading. Uses api.alpaca.markets - USE WITH CAUTION'}
        </div>
      </div>

      <div>
        <label style={labelStyle}>API KEY</label>
        <input
          type="text"
          value={formData.apiKey || ''}
          onChange={(e) => onChange({ ...formData, apiKey: e.target.value })}
          style={inputStyle}
          placeholder="Your Alpaca API Key"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
      </div>

      <div>
        <label style={labelStyle}>SECRET KEY</label>
        <input
          type="password"
          value={formData.apiSecret || ''}
          onChange={(e) => onChange({ ...formData, apiSecret: e.target.value })}
          style={inputStyle}
          placeholder="Your Alpaca Secret Key"
          onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
          onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
        />
        <div style={{
          fontSize: '9px',
          color: BLOOMBERG.CYAN,
          marginTop: '6px',
          letterSpacing: '0.3px',
          padding: '8px',
          border: `1px solid ${BLOOMBERG.BORDER}`,
          backgroundColor: BLOOMBERG.PANEL_BG
        }}>
          <div style={{ marginBottom: '4px', fontWeight: 700, color: BLOOMBERG.ORANGE }}>
            HOW TO GET API CREDENTIALS:
          </div>
          <div>1. Sign up at <a href="https://alpaca.markets" target="_blank" rel="noopener noreferrer" style={{ color: BLOOMBERG.ORANGE, textDecoration: 'underline' }}>alpaca.markets</a></div>
          <div>2. Navigate to Paper Trading or Live Trading section</div>
          <div>3. Click "View API Keys" or "Generate New Keys"</div>
          <div>4. Copy API Key and Secret Key from dashboard</div>
          <div>5. Paste them above and click CONNECT</div>
        </div>
      </div>

      <div style={{ display: 'flex', gap: '8px', marginTop: '8px' }}>
        <button
          onClick={onSave}
          disabled={saving}
          style={{
            flex: 1,
            padding: '10px 16px',
            backgroundColor: saving ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE,
            border: `1px solid ${saving ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE}`,
            borderRadius: '0',
            color: BLOOMBERG.DARK_BG,
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: saving ? 'not-allowed' : 'pointer',
            transition: 'all 0.15s ease',
            fontFamily: 'monospace',
            outline: 'none',
            textTransform: 'uppercase'
          }}
          onMouseEnter={(e) => {
            if (!saving) e.currentTarget.style.backgroundColor = BLOOMBERG.YELLOW;
          }}
          onMouseLeave={(e) => {
            if (!saving) e.currentTarget.style.backgroundColor = BLOOMBERG.ORANGE;
          }}
        >
          {saving ? 'CONNECTING...' : isAuthenticated ? 'UPDATE' : 'CONNECT'}
        </button>

        {isAuthenticated && (
          <button
            onClick={onDisconnect}
            style={{
              padding: '10px 16px',
              backgroundColor: 'transparent',
              border: `1px solid ${BLOOMBERG.RED}`,
              borderRadius: '0',
              color: BLOOMBERG.RED,
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              fontFamily: 'monospace',
              outline: 'none',
              textTransform: 'uppercase'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.RED;
              e.currentTarget.style.color = BLOOMBERG.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = BLOOMBERG.RED;
            }}
          >
            DISCONNECT
          </button>
        )}
      </div>
    </div>
  );
};

export default BrokerConfigTab;
