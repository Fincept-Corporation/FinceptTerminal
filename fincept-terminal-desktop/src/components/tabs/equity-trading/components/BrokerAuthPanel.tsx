/**
 * Broker Authentication Panel - Bloomberg Style
 *
 * Handles OAuth and credential-based authentication for stock brokers
 */

import React, { useState } from 'react';
import { KeyRound, ExternalLink, Loader2, AlertCircle, CheckCircle2, Eye, EyeOff, Lock, LogOut, Zap } from 'lucide-react';
import { open } from '@tauri-apps/plugin-shell';
import { useStockBrokerAuth, useStockBrokerSelection } from '@/contexts/StockBrokerContext';
import type { BrokerCredentials } from '@/brokers/stocks/types';

// Bloomberg color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

interface BrokerAuthPanelProps {
  onAuthSuccess?: () => void;
}

export function BrokerAuthPanel({ onAuthSuccess }: BrokerAuthPanelProps) {
  const { metadata } = useStockBrokerSelection();
  const { isAuthenticated, authenticate, logout, getAuthUrl, isConnecting, error, clearError, adapter } = useStockBrokerAuth();

  const [showCredentials, setShowCredentials] = useState(false);
  const [credentials, setCredentials] = useState<BrokerCredentials>({
    apiKey: '',
    apiSecret: '',
    accessToken: '',
    userId: '',
    totpSecret: '',
    password: '',
    pin: '',
  });
  const [callbackUrl, setCallbackUrl] = useState('');
  const [showCallbackInput, setShowCallbackInput] = useState(false);

  // Handle OAuth flow
  const handleOAuthLogin = async () => {
    try {
      const authUrl = await getAuthUrl();
      if (authUrl) {
        console.log('[BrokerAuthPanel] Opening OAuth URL:', authUrl.substring(0, 50) + '...');
        await open(authUrl);
        console.log('[BrokerAuthPanel] ✓ Browser opened successfully');
        setShowCallbackInput(true);
      } else {
        console.error('[BrokerAuthPanel] No auth URL returned');
      }
    } catch (err) {
      console.error('[BrokerAuthPanel] Failed to get auth URL:', err);
    }
  };

  // Handle callback URL submission
  const handleCallbackSubmit = async () => {
    try {
      const url = new URL(callbackUrl);
      const requestToken = url.searchParams.get('request_token');
      const authCode = url.searchParams.get('auth_code');
      const code = url.searchParams.get('code');

      const token = requestToken || authCode || code;

      if (!token) {
        console.error('[BrokerAuthPanel] No auth code/token found in callback URL');
        return;
      }

      console.log('[BrokerAuthPanel] Found auth token, loading stored credentials...');

      const storedCreds = await (adapter as any)?.loadCredentials();

      if (!storedCreds || !storedCreds.apiKey) {
        console.error('[BrokerAuthPanel] No stored credentials found. Please save API credentials first.');
        return;
      }

      const authCredentials: BrokerCredentials = {
        apiKey: storedCreds.apiKey,
        apiSecret: storedCreds.apiSecret,
      };

      if (requestToken) {
        (authCredentials as any).requestToken = requestToken;
      } else if (authCode) {
        (authCredentials as any).authCode = authCode;
      } else {
        authCredentials.accessToken = token;
      }

      const response = await authenticate(authCredentials);

      if (response.success) {
        console.log('[BrokerAuthPanel] ✓ Authentication successful');
        setShowCallbackInput(false);
        setCallbackUrl('');
        if (onAuthSuccess) {
          onAuthSuccess();
        }
      } else {
        console.error('[BrokerAuthPanel] Authentication failed:', response.message);
      }
    } catch (err) {
      console.error('[BrokerAuthPanel] Failed to process callback:', err);
    }
  };

  // Handle credential-based login
  const handleCredentialLogin = async () => {
    clearError();

    const response = await authenticate(credentials);
    if (response.success && onAuthSuccess) {
      onAuthSuccess();
    }
  };

  // Handle logout
  const handleLogout = async () => {
    await logout();
    setCredentials({
      apiKey: '',
      apiSecret: '',
      accessToken: '',
      userId: '',
      totpSecret: '',
      password: '',
      pin: '',
    });
  };

  // Test API endpoint
  const testApiEndpoint = async () => {
    try {
      console.log('[BrokerAuthPanel] Testing API endpoint...');

      if (adapter && metadata?.id === 'zerodha') {
        const quote = await adapter.getQuote('INFY', 'NSE');
        console.log('[BrokerAuthPanel] ✓ Quote received:', quote);
        alert(`Success! INFY LTP: ₹${quote.lastPrice}\nCheck console for full response`);
      } else if (adapter && metadata?.id === 'fyers') {
        const quote = await adapter.getQuote('RELIANCE', 'NSE');
        console.log('[BrokerAuthPanel] ✓ Quote received:', quote);
        alert(`Success! RELIANCE LTP: ₹${quote.lastPrice}\nCheck console for full response`);
      } else if (adapter && metadata?.id === 'angelone') {
        const quote = await adapter.getQuote('SBIN', 'NSE');
        console.log('[BrokerAuthPanel] ✓ Quote received:', quote);
        alert(`Success! SBIN LTP: ₹${quote.lastPrice}\nCheck console for full response`);
      } else {
        alert('Test only available for Zerodha, Fyers, and Angel One');
      }
    } catch (err: any) {
      console.error('[BrokerAuthPanel] API test failed:', err);
      alert(`API test failed: ${err.message}\nCheck console for details`);
    }
  };

  if (!metadata) {
    return (
      <div style={{
        padding: '24px',
        backgroundColor: COLORS.HEADER_BG,
        border: `1px solid ${COLORS.BORDER}`,
        textAlign: 'center'
      }}>
        <p style={{ color: COLORS.GRAY, fontSize: '11px' }}>Select a broker to continue</p>
      </div>
    );
  }

  // Already authenticated
  if (isAuthenticated) {
    return (
      <div style={{
        padding: '20px',
        backgroundColor: COLORS.HEADER_BG,
        border: `1px solid ${COLORS.BORDER}`,
        fontFamily: '"IBM Plex Mono", monospace'
      }}>
        {/* Connected Header */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          marginBottom: '20px',
          paddingBottom: '16px',
          borderBottom: `1px solid ${COLORS.BORDER}`
        }}>
          <div style={{
            width: '36px',
            height: '36px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: `${COLORS.GREEN}20`,
            border: `1px solid ${COLORS.GREEN}40`
          }}>
            <CheckCircle2 size={18} color={COLORS.GREEN} />
          </div>
          <div>
            <h3 style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 600, margin: 0 }}>
              CONNECTED TO {metadata.displayName.toUpperCase()}
            </h3>
            <p style={{ color: COLORS.GREEN, fontSize: '10px', margin: '4px 0 0 0' }}>
              Your broker account is linked
            </p>
          </div>
        </div>

        {/* Action Buttons */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <button
            onClick={testApiEndpoint}
            style={{
              width: '100%',
              padding: '10px 16px',
              backgroundColor: `${COLORS.BLUE}20`,
              border: `1px solid ${COLORS.BLUE}40`,
              color: COLORS.BLUE,
              fontSize: '11px',
              fontWeight: 600,
              fontFamily: '"IBM Plex Mono", monospace',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px'
            }}
          >
            <Zap size={14} />
            TEST API ENDPOINT
          </button>

          <button
            onClick={handleLogout}
            style={{
              width: '100%',
              padding: '10px 16px',
              backgroundColor: `${COLORS.RED}20`,
              border: `1px solid ${COLORS.RED}40`,
              color: COLORS.RED,
              fontSize: '11px',
              fontWeight: 600,
              fontFamily: '"IBM Plex Mono", monospace',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px'
            }}
          >
            <LogOut size={14} />
            DISCONNECT BROKER
          </button>
        </div>
      </div>
    );
  }

  return (
    <div style={{
      padding: '20px',
      backgroundColor: COLORS.HEADER_BG,
      border: `1px solid ${COLORS.BORDER}`,
      fontFamily: '"IBM Plex Mono", monospace'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        marginBottom: '20px',
        paddingBottom: '16px',
        borderBottom: `1px solid ${COLORS.BORDER}`
      }}>
        <div style={{
          width: '36px',
          height: '36px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: `${COLORS.ORANGE}20`,
          border: `1px solid ${COLORS.ORANGE}40`
        }}>
          <KeyRound size={18} color={COLORS.ORANGE} />
        </div>
        <div>
          <h3 style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 600, margin: 0 }}>
            CONNECT TO {metadata.displayName.toUpperCase()}
          </h3>
          <p style={{ color: COLORS.GRAY, fontSize: '10px', margin: '4px 0 0 0' }}>
            {metadata.authType === 'oauth' ? 'OAuth 2.0 Authentication' : 'API Credentials'}
          </p>
        </div>
      </div>

      {/* Error display */}
      {error && (
        <div style={{
          marginBottom: '16px',
          padding: '12px',
          backgroundColor: `${COLORS.RED}15`,
          border: `1px solid ${COLORS.RED}40`,
          display: 'flex',
          alignItems: 'center',
          gap: '10px'
        }}>
          <AlertCircle size={16} color={COLORS.RED} />
          <p style={{ color: COLORS.RED, fontSize: '11px', margin: 0 }}>{error}</p>
        </div>
      )}

      {/* OAuth Flow */}
      {metadata.authType === 'oauth' && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          <p style={{ color: COLORS.GRAY, fontSize: '11px', margin: 0, lineHeight: 1.6 }}>
            Click the button below to authenticate with {metadata.displayName}. You will be redirected to their login page.
          </p>

          <button
            onClick={handleOAuthLogin}
            disabled={isConnecting}
            style={{
              width: '100%',
              padding: '12px 16px',
              backgroundColor: isConnecting ? COLORS.MUTED : COLORS.ORANGE,
              border: 'none',
              color: isConnecting ? COLORS.GRAY : COLORS.DARK_BG,
              fontSize: '11px',
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
              letterSpacing: '0.5px',
              cursor: isConnecting ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px'
            }}
          >
            {isConnecting ? (
              <Loader2 size={14} className="animate-spin" />
            ) : (
              <ExternalLink size={14} />
            )}
            LOGIN WITH {metadata.displayName.toUpperCase()}
          </button>

          {/* Callback URL Input */}
          {showCallbackInput && (
            <div style={{
              marginTop: '8px',
              padding: '16px',
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`
            }}>
              <div style={{ display: 'flex', alignItems: 'flex-start', gap: '10px', marginBottom: '12px' }}>
                <AlertCircle size={16} color={COLORS.ORANGE} style={{ flexShrink: 0, marginTop: '2px' }} />
                <div>
                  <p style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 600, margin: '0 0 4px 0' }}>
                    PASTE CALLBACK URL
                  </p>
                  <p style={{ color: COLORS.GRAY, fontSize: '9px', margin: 0 }}>
                    After authorizing on {metadata.displayName}, copy the URL from your browser's address bar and paste it below.
                  </p>
                </div>
              </div>

              <input
                type="text"
                value={callbackUrl}
                onChange={(e) => setCallbackUrl(e.target.value)}
                placeholder="https://127.0.0.1/?auth_code=..."
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none',
                  boxSizing: 'border-box',
                  marginBottom: '12px'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
              />

              <button
                onClick={handleCallbackSubmit}
                disabled={!callbackUrl || isConnecting}
                style={{
                  width: '100%',
                  padding: '10px 16px',
                  backgroundColor: !callbackUrl || isConnecting ? COLORS.MUTED : COLORS.GREEN,
                  border: 'none',
                  color: !callbackUrl || isConnecting ? COLORS.GRAY : COLORS.DARK_BG,
                  fontSize: '11px',
                  fontWeight: 700,
                  fontFamily: '"IBM Plex Mono", monospace',
                  cursor: !callbackUrl || isConnecting ? 'not-allowed' : 'pointer'
                }}
              >
                COMPLETE AUTHENTICATION
              </button>
            </div>
          )}

          <p style={{ color: COLORS.MUTED, fontSize: '9px', textAlign: 'center', margin: 0 }}>
            You'll be redirected to {metadata.website} to authorize access
          </p>
        </div>
      )}

      {/* Credential-based Flow */}
      {metadata.authType !== 'oauth' && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {/* API Key */}
          <div>
            <label style={{
              display: 'block',
              color: COLORS.GRAY,
              fontSize: '10px',
              fontWeight: 600,
              marginBottom: '6px',
              letterSpacing: '0.5px'
            }}>
              API KEY
            </label>
            <input
              type="text"
              value={credentials.apiKey}
              onChange={(e) => setCredentials({ ...credentials, apiKey: e.target.value })}
              placeholder="Enter your API key"
              style={{
                width: '100%',
                padding: '10px 12px',
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '12px',
                fontFamily: 'monospace',
                outline: 'none',
                boxSizing: 'border-box'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
            />
          </div>

          {/* API Secret */}
          {metadata.authType === 'api_key' && (
            <div>
              <label style={{
                display: 'block',
                color: COLORS.GRAY,
                fontSize: '10px',
                fontWeight: 600,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                API SECRET
              </label>
              <div style={{ position: 'relative' }}>
                <input
                  type={showCredentials ? 'text' : 'password'}
                  value={credentials.apiSecret || ''}
                  onChange={(e) => setCredentials({ ...credentials, apiSecret: e.target.value })}
                  placeholder="Enter your API secret"
                  style={{
                    width: '100%',
                    padding: '10px 40px 10px 12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    fontSize: '12px',
                    fontFamily: 'monospace',
                    outline: 'none',
                    boxSizing: 'border-box'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                />
                <button
                  type="button"
                  onClick={() => setShowCredentials(!showCredentials)}
                  style={{
                    position: 'absolute',
                    right: '10px',
                    top: '50%',
                    transform: 'translateY(-50%)',
                    background: 'none',
                    border: 'none',
                    color: COLORS.GRAY,
                    cursor: 'pointer',
                    padding: '4px'
                  }}
                >
                  {showCredentials ? <EyeOff size={16} /> : <Eye size={16} />}
                </button>
              </div>
            </div>
          )}

          {/* TOTP auth fields (Angel One style) */}
          {metadata.authType === 'totp' && (
            <>
              {/* Client Code / User ID */}
              <div>
                <label style={{
                  display: 'block',
                  color: COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 600,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  CLIENT CODE / USER ID
                </label>
                <input
                  type="text"
                  value={credentials.userId || ''}
                  onChange={(e) => setCredentials({ ...credentials, userId: e.target.value })}
                  placeholder="Enter your client code"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    fontSize: '12px',
                    fontFamily: 'monospace',
                    outline: 'none',
                    boxSizing: 'border-box'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                />
              </div>

              {/* Password */}
              <div>
                <label style={{
                  display: 'block',
                  color: COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 600,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  PASSWORD
                </label>
                <div style={{ position: 'relative' }}>
                  <input
                    type={showCredentials ? 'text' : 'password'}
                    value={credentials.password || ''}
                    onChange={(e) => setCredentials({ ...credentials, password: e.target.value })}
                    placeholder="Enter your password"
                    style={{
                      width: '100%',
                      padding: '10px 40px 10px 12px',
                      backgroundColor: COLORS.PANEL_BG,
                      border: `1px solid ${COLORS.BORDER}`,
                      color: COLORS.WHITE,
                      fontSize: '12px',
                      fontFamily: 'monospace',
                      outline: 'none',
                      boxSizing: 'border-box'
                    }}
                    onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                    onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                  />
                  <button
                    type="button"
                    onClick={() => setShowCredentials(!showCredentials)}
                    style={{
                      position: 'absolute',
                      right: '10px',
                      top: '50%',
                      transform: 'translateY(-50%)',
                      background: 'none',
                      border: 'none',
                      color: COLORS.GRAY,
                      cursor: 'pointer',
                      padding: '4px'
                    }}
                  >
                    {showCredentials ? <EyeOff size={16} /> : <Eye size={16} />}
                  </button>
                </div>
              </div>

              {/* TOTP Code */}
              <div>
                <label style={{
                  display: 'block',
                  color: COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 600,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  TOTP CODE
                </label>
                <input
                  type="text"
                  value={credentials.pin || ''}
                  onChange={(e) => setCredentials({ ...credentials, pin: e.target.value })}
                  placeholder="Enter 6-digit TOTP code"
                  maxLength={6}
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    fontSize: '12px',
                    fontFamily: 'monospace',
                    outline: 'none',
                    boxSizing: 'border-box',
                    letterSpacing: '4px',
                    textAlign: 'center'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                />
                <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>
                  Enter the 6-digit code from your authenticator app
                </p>
              </div>
            </>
          )}

          {/* Password auth fields */}
          {metadata.authType === 'password' && (
            <>
              <div>
                <label style={{
                  display: 'block',
                  color: COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 600,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  USER ID
                </label>
                <input
                  type="text"
                  value={credentials.userId || ''}
                  onChange={(e) => setCredentials({ ...credentials, userId: e.target.value })}
                  placeholder="Enter your user ID"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    fontSize: '12px',
                    fontFamily: 'monospace',
                    outline: 'none',
                    boxSizing: 'border-box'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                />
              </div>
              <div>
                <label style={{
                  display: 'block',
                  color: COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 600,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  PASSWORD
                </label>
                <input
                  type="password"
                  value={credentials.password || ''}
                  onChange={(e) => setCredentials({ ...credentials, password: e.target.value })}
                  placeholder="Enter your password"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    fontSize: '12px',
                    fontFamily: 'monospace',
                    outline: 'none',
                    boxSizing: 'border-box'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                />
              </div>
              <div>
                <label style={{
                  display: 'block',
                  color: COLORS.GRAY,
                  fontSize: '10px',
                  fontWeight: 600,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  TRADING PIN
                </label>
                <input
                  type="password"
                  value={credentials.pin || ''}
                  onChange={(e) => setCredentials({ ...credentials, pin: e.target.value })}
                  placeholder="Enter your trading PIN"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                    color: COLORS.WHITE,
                    fontSize: '12px',
                    fontFamily: 'monospace',
                    outline: 'none',
                    boxSizing: 'border-box'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
                />
              </div>
            </>
          )}

          <button
            onClick={handleCredentialLogin}
            disabled={isConnecting || !credentials.apiKey}
            style={{
              width: '100%',
              padding: '12px 16px',
              backgroundColor: isConnecting || !credentials.apiKey ? COLORS.MUTED : COLORS.ORANGE,
              border: 'none',
              color: isConnecting || !credentials.apiKey ? COLORS.GRAY : COLORS.DARK_BG,
              fontSize: '11px',
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
              letterSpacing: '0.5px',
              cursor: isConnecting || !credentials.apiKey ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              marginTop: '8px'
            }}
          >
            {isConnecting ? (
              <Loader2 size={14} className="animate-spin" />
            ) : (
              <Lock size={14} />
            )}
            CONNECT BROKER
          </button>
        </div>
      )}

      {/* Security Note */}
      <div style={{
        marginTop: '20px',
        padding: '12px',
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`
      }}>
        <p style={{ color: COLORS.MUTED, fontSize: '9px', margin: 0, lineHeight: 1.6 }}>
          Your credentials are stored securely on your local device and are never sent to our servers.
          We only communicate directly with {metadata.displayName}'s API.
        </p>
      </div>
    </div>
  );
}
