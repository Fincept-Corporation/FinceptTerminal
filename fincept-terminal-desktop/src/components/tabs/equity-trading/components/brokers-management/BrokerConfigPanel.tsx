import React from 'react';
import {
  Settings,
  CheckCircle2,
  AlertTriangle,
  ExternalLink,
  Eye,
  EyeOff,
  Save,
  LogOut,
  RefreshCw,
  Loader2,
  Globe,
} from 'lucide-react';
import type { BrokerCredentials, StockBrokerMetadata } from '@/brokers/stocks/types';
import { COLORS, API_KEYS_URLS, BrokerStatus } from './types';
import { StatusBadge } from './StatusBadge';

interface BrokerConfigPanelProps {
  selectedBroker: StockBrokerMetadata | null;
  selectedBrokerId: string | null;
  activeBroker: string | null;
  isAuthenticated: boolean;
  tradingMode: string;
  error: string | null;
  configError: string | null;
  saveSuccess: boolean;
  isSaving: boolean;
  isConnecting: boolean;
  isCheckingToken: boolean;
  showSecret: boolean;
  setShowSecret: (v: boolean) => void;
  config: BrokerCredentials & Record<string, any>;
  setConfig: React.Dispatch<React.SetStateAction<any>>;
  mobileNumber: string;
  setMobileNumber: (v: string) => void;
  callbackUrl: string;
  setCallbackUrl: (v: string) => void;
  showCallbackInput: boolean;
  brokerStatuses: Map<string, BrokerStatus>;
  onSaveConfig: () => void;
  onOAuthLogin: () => void;
  onCallbackSubmit: () => void;
  onDisconnect: () => void;
  onCheckToken: () => void;
}

export function BrokerConfigPanel({
  selectedBroker,
  selectedBrokerId,
  activeBroker,
  isAuthenticated,
  tradingMode,
  error,
  configError,
  saveSuccess,
  isSaving,
  isConnecting,
  isCheckingToken,
  showSecret,
  setShowSecret,
  config,
  setConfig,
  mobileNumber,
  setMobileNumber,
  callbackUrl,
  setCallbackUrl,
  showCallbackInput,
  brokerStatuses,
  onSaveConfig,
  onOAuthLogin,
  onCallbackSubmit,
  onDisconnect,
  onCheckToken,
}: BrokerConfigPanelProps) {
  if (!selectedBroker) {
    return (
      <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ textAlign: 'center' }}>
          <Settings size={48} color={COLORS.MUTED} style={{ marginBottom: '16px' }} />
          <p style={{ color: COLORS.GRAY, fontSize: '12px' }}>Select a broker from the list to configure</p>
        </div>
      </div>
    );
  }

  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Broker Header */}
      <div style={{ padding: '16px 20px', borderBottom: `1px solid ${COLORS.BORDER}`, backgroundColor: COLORS.HEADER_BG }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{
              width: '40px',
              height: '40px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              backgroundColor: `${COLORS.ORANGE}20`,
              border: `1px solid ${COLORS.ORANGE}40`,
            }}>
              <Globe size={20} color={COLORS.ORANGE} />
            </div>
            <div>
              <h2 style={{ color: COLORS.WHITE, fontSize: '14px', fontWeight: 600, margin: 0 }}>
                {selectedBroker.displayName}
              </h2>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
                <StatusBadge status={brokerStatuses.get(selectedBrokerId!)?.status || 'not_configured'} />
                <span style={{ color: COLORS.MUTED, fontSize: '10px' }}>
                  {selectedBroker.authType === 'oauth' ? 'OAuth 2.0' : selectedBroker.authType === 'totp' ? 'TOTP Auth' : 'API Key Auth'}
                </span>
              </div>
            </div>
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            {API_KEYS_URLS[selectedBrokerId!] && (
              <a
                href={API_KEYS_URLS[selectedBrokerId!]}
                target="_blank"
                rel="noopener noreferrer"
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  padding: '8px 12px',
                  backgroundColor: `${COLORS.ORANGE}10`,
                  border: `1px solid ${COLORS.ORANGE}40`,
                  color: COLORS.ORANGE,
                  fontSize: '10px',
                  textDecoration: 'none',
                  fontWeight: 600,
                }}
              >
                GET API KEYS
                <ExternalLink size={12} />
              </a>
            )}

            {selectedBrokerId === activeBroker && isAuthenticated && (
              <button
                onClick={onCheckToken}
                disabled={isCheckingToken}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  padding: '8px 12px',
                  backgroundColor: `${COLORS.BLUE}10`,
                  border: `1px solid ${COLORS.BLUE}40`,
                  color: COLORS.BLUE,
                  fontSize: '10px',
                  cursor: 'pointer',
                  fontWeight: 600,
                }}
              >
                {isCheckingToken ? <Loader2 size={12} className="animate-spin" /> : <RefreshCw size={12} />}
                CHECK TOKEN
              </button>
            )}
          </div>
        </div>
      </div>

      {/* Configuration Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '20px' }}>
        {/* Error Display */}
        {(error || configError) && (
          <div style={{
            marginBottom: '16px',
            padding: '12px',
            backgroundColor: `${COLORS.RED}15`,
            border: `1px solid ${COLORS.RED}40`,
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
          }}>
            <AlertTriangle size={16} color={COLORS.RED} />
            <p style={{ color: COLORS.RED, fontSize: '11px', margin: 0 }}>{error || configError}</p>
          </div>
        )}

        {/* Success Display */}
        {saveSuccess && (
          <div style={{
            marginBottom: '16px',
            padding: '12px',
            backgroundColor: `${COLORS.GREEN}15`,
            border: `1px solid ${COLORS.GREEN}40`,
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
          }}>
            <CheckCircle2 size={16} color={COLORS.GREEN} />
            <p style={{ color: COLORS.GREEN, fontSize: '11px', margin: 0 }}>Configuration saved successfully!</p>
          </div>
        )}

        {/* Connected State */}
        {selectedBrokerId === activeBroker && isAuthenticated ? (
          <div style={{ maxWidth: '500px' }}>
            <div style={{
              padding: '20px',
              backgroundColor: `${COLORS.GREEN}10`,
              border: `1px solid ${COLORS.GREEN}40`,
              marginBottom: '20px',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '12px' }}>
                <CheckCircle2 size={24} color={COLORS.GREEN} />
                <div>
                  <h3 style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 600, margin: 0 }}>
                    Connected to {selectedBroker.displayName}
                  </h3>
                  <p style={{ color: COLORS.GREEN, fontSize: '10px', margin: '4px 0 0 0' }}>
                    Trading mode: {tradingMode.toUpperCase()}
                  </p>
                </div>
              </div>
              <p style={{ color: COLORS.GRAY, fontSize: '10px', margin: 0 }}>
                Your broker account is linked and ready for trading. Indian broker tokens expire at midnight IST.
              </p>
            </div>

            <button
              onClick={onDisconnect}
              style={{
                width: '100%',
                padding: '12px 16px',
                backgroundColor: `${COLORS.RED}20`,
                border: `1px solid ${COLORS.RED}40`,
                color: COLORS.RED,
                fontSize: '11px',
                fontWeight: 600,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
              }}
            >
              <LogOut size={14} />
              DISCONNECT BROKER
            </button>
          </div>
        ) : (
          /* Configuration Form */
          <div style={{ maxWidth: '500px' }}>
            {/* Step 1: API Credentials */}
            <div style={{ marginBottom: '24px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                <div style={{
                  width: '24px',
                  height: '24px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  backgroundColor: COLORS.ORANGE,
                  color: COLORS.DARK_BG,
                  fontSize: '12px',
                  fontWeight: 700,
                }}>
                  1
                </div>
                <h3 style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, margin: 0 }}>
                  CONFIGURE API CREDENTIALS
                </h3>
              </div>

              {/* ============================================================ */}
              {/* BROKER-SPECIFIC AUTHENTICATION FORMS */}
              {/* Based on OpenAlgo authentication patterns */}
              {/* ============================================================ */}

              {/* ------------------------------------------------------------ */}
              {/* OAUTH BROKERS: Zerodha, Fyers, Upstox, Dhan, IIFL, IBKR, Saxo */}
              {/* Fields: API Key + API Secret */}
              {/* ------------------------------------------------------------ */}
              {['zerodha', 'fyers', 'upstox', 'dhan', 'iifl', 'ibkr', 'saxobank', 'flattrade'].includes(selectedBroker.id) && (
                <>
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      {selectedBroker.id === 'fyers' ? 'APP ID' :
                       selectedBroker.id === 'dhan' ? 'CLIENT ID' :
                       selectedBroker.id === 'flattrade' ? 'API KEY (userid:::api_key format)' :
                       'API KEY'} <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder={
                        selectedBroker.id === 'fyers' ? 'Enter your Fyers App ID (e.g., XYZ123-100)' :
                        selectedBroker.id === 'dhan' ? 'Enter your Dhan Client ID' :
                        selectedBroker.id === 'flattrade' ? 'Enter API Key (userid:::api_key)' :
                        `Enter your ${selectedBroker.displayName} API Key`
                      }
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
                      }}
                    />
                  </div>
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      {selectedBroker.id === 'fyers' ? 'SECRET KEY' : 'API SECRET'} <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={config.apiSecret || ''}
                        onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                        placeholder="Enter your API Secret"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '10px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    {selectedBroker.id === 'fyers' && (
                      <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                        Hash is generated as SHA256(api_key:api_secret) for token exchange
                      </p>
                    )}
                    {selectedBroker.id === 'flattrade' && (
                      <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                        Security hash: SHA256(api_key + request_code + api_secret)
                      </p>
                    )}
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* KOTAK NEO: UCC + Mobile + MPIN + TOTP (2-step auth) */}
              {/* Step 1: TOTP login → view_token */}
              {/* Step 2: MPIN validation → trading_token */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'kotak' && (
                <>
                  {/* UCC */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      UCC (USER CLIENT CODE) <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="e.g., KS12345"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your UCC (User Client Code) - Primary identifier
                    </p>
                  </div>

                  {/* Access Token / NEO-FIN-KEY */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      ACCESS TOKEN (NEO-FIN-KEY) <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={config.apiSecret || ''}
                        onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                        placeholder="Enter your Access Token"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '10px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                  </div>

                  {/* Mobile Number */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      MOBILE NUMBER <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <span style={{ color: COLORS.GRAY, fontSize: '12px', fontFamily: 'monospace' }}>+91</span>
                      <input
                        type="text"
                        value={mobileNumber}
                        onChange={(e) => setMobileNumber(e.target.value.replace(/\D/g, '').slice(0, 10))}
                        placeholder="9999955555"
                        maxLength={10}
                        style={{
                          flex: 1,
                          padding: '10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      10-digit mobile number (auto-prefixed with +91)
                    </p>
                  </div>

                  {/* MPIN */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      MPIN (6-DIGIT) <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type={showSecret ? 'text' : 'password'}
                      value={(config as any).mpin || ''}
                      onChange={(e) => setConfig({ ...config, mpin: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                      placeholder="******"
                      maxLength={6}
                      style={{
                        width: '100%',
                        padding: '10px 12px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: COLORS.WHITE,
                        fontSize: '16px',
                        fontFamily: 'monospace',
                        textAlign: 'center',
                        letterSpacing: '4px',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your 6-digit trading MPIN
                    </p>
                  </div>

                  {/* TOTP Code */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      TOTP CODE <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={(config as any).totpCode || ''}
                      onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                      placeholder="123456"
                      maxLength={6}
                      style={{
                        width: '100%',
                        padding: '10px 12px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: COLORS.WHITE,
                        fontSize: '16px',
                        fontFamily: 'monospace',
                        textAlign: 'center',
                        letterSpacing: '4px',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      6-digit code from Google/Microsoft Authenticator
                    </p>
                  </div>

                  {/* Kotak Auth Info */}
                  <div style={{ padding: '10px', backgroundColor: `${COLORS.YELLOW}10`, border: `1px solid ${COLORS.YELLOW}40`, marginBottom: '12px' }}>
                    <p style={{ color: COLORS.YELLOW, fontSize: '9px', margin: 0 }}>
                      <strong>TOTP Registration Required:</strong> You must register TOTP in the NEO app before using this feature.
                    </p>
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* GROWW: API Key + API Secret (Server-side TOTP) */}
              {/* TOTP is generated server-side from API Secret */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'groww' && (
                <>
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="Enter your Groww API Key"
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
                      }}
                    />
                  </div>
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API SECRET (TOTP SECRET) <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={config.apiSecret || ''}
                        onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                        placeholder="Enter your API Secret"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '10px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      TOTP is generated automatically from this secret - no manual entry required
                    </p>
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* US BROKERS: Alpaca, Tradier */}
              {/* Simple API Key authentication */}
              {/* ------------------------------------------------------------ */}
              {['alpaca', 'tradier'].includes(selectedBroker.id) && (
                <>
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      {selectedBroker.id === 'tradier' ? 'ACCESS TOKEN' : 'API KEY ID'} <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type={selectedBroker.id === 'tradier' ? 'password' : 'text'}
                      value={config.apiKey}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder={selectedBroker.id === 'tradier' ? 'Enter your Tradier Access Token' : 'Enter your Alpaca API Key ID'}
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
                      }}
                    />
                  </div>
                  {selectedBroker.id === 'alpaca' && (
                    <div style={{ marginBottom: '12px' }}>
                      <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                        SECRET KEY <span style={{ color: COLORS.RED }}>*</span>
                      </label>
                      <div style={{ position: 'relative' }}>
                        <input
                          type={showSecret ? 'text' : 'password'}
                          value={config.apiSecret || ''}
                          onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                          placeholder="Enter your Alpaca Secret Key"
                          style={{
                            width: '100%',
                            padding: '10px 40px 10px 12px',
                            backgroundColor: COLORS.PANEL_BG,
                            border: `1px solid ${COLORS.BORDER}`,
                            color: COLORS.WHITE,
                            fontSize: '12px',
                            fontFamily: 'monospace',
                            outline: 'none',
                            boxSizing: 'border-box',
                          }}
                        />
                        <button
                          type="button"
                          onClick={() => setShowSecret(!showSecret)}
                          style={{
                            position: 'absolute',
                            right: '10px',
                            top: '50%',
                            transform: 'translateY(-50%)',
                            background: 'none',
                            border: 'none',
                            color: COLORS.GRAY,
                            cursor: 'pointer',
                          }}
                        >
                          {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                        </button>
                      </div>
                    </div>
                  )}
                  <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                    {selectedBroker.id === 'tradier' ? 'Tradier uses a single bearer token for authentication' : 'Use paper trading keys for testing'}
                  </p>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* YFINANCE: No authentication required */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'yfinance' && (
                <div style={{ padding: '16px', backgroundColor: `${COLORS.GREEN}10`, border: `1px solid ${COLORS.GREEN}40`, marginBottom: '12px' }}>
                  <p style={{ color: COLORS.GREEN, fontSize: '11px', margin: 0, fontWeight: 600 }}>
                    No API keys required
                  </p>
                  <p style={{ color: COLORS.GRAY, fontSize: '10px', margin: '8px 0 0 0' }}>
                    YFinance provides free market data for paper trading. Click "Save Configuration" to activate paper trading mode.
                  </p>
                </div>
              )}

              {/* ------------------------------------------------------------ */}
              {/* SHOONYA (FINVASIA): SHA256 Hash-based Authentication */}
              {/* Fields: Vendor Code, API Secret, User ID, Password, TOTP */}
              {/* Hash: SHA256(password), AppKey: SHA256(userid|api_secret) */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'shoonya' && (
                <>
                  {/* Vendor Code */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      VENDOR CODE <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey || ''}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="Enter your Vendor Code"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Vendor Code from Shoonya API settings
                    </p>
                  </div>

                  {/* API Secret Key */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API SECRET KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={config.apiSecret || ''}
                        onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                        placeholder="Enter your API Secret Key"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '10px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Used to generate AppKey hash: SHA256(userid|api_secret)
                    </p>
                  </div>

                  {/* User ID */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      USER ID <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.userId || ''}
                      onChange={(e) => setConfig({ ...config, userId: e.target.value })}
                      placeholder="Enter your Shoonya User ID"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Shoonya trading account User ID
                    </p>
                  </div>

                  {/* Password */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      PASSWORD <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={(config as any).password || ''}
                        onChange={(e) => setConfig({ ...config, password: e.target.value } as any)}
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
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '12px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Shoonya trading password (will be SHA256 hashed)
                    </p>
                  </div>

                  {/* TOTP */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      TOTP CODE <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={(config as any).totpCode || ''}
                      onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                      placeholder="123456"
                      maxLength={6}
                      style={{
                        width: '100%',
                        padding: '10px 12px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: COLORS.WHITE,
                        fontSize: '16px',
                        fontFamily: 'monospace',
                        textAlign: 'center',
                        letterSpacing: '4px',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      6-digit TOTP from your authenticator app
                    </p>
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* ANGEL ONE: TOTP-based Authentication */}
              {/* Fields: API Key, Client ID, PIN, TOTP Secret */}
              {/* Headers: X-PrivateKey (api_key), Content-Type */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'angelone' && (
                <>
                  {/* API Key */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey || ''}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="Enter your SmartAPI Key"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Angel One SmartAPI Key from publisher dashboard
                    </p>
                  </div>

                  {/* Client ID (Trading Account) */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      CLIENT ID (TRADING ACCOUNT) <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.userId || ''}
                      onChange={(e) => setConfig({ ...config, userId: e.target.value.toUpperCase() })}
                      placeholder="e.g. T55289"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Angel One trading account login ID (letters and numbers only)
                    </p>
                  </div>

                  {/* PIN */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      PIN <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={(config as any).pin || ''}
                        onChange={(e) => setConfig({ ...config, pin: e.target.value } as any)}
                        placeholder="Enter your trading PIN"
                        minLength={4}
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '12px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Angel One account PIN (minimum 4 digits)
                    </p>
                  </div>

                  {/* TOTP Secret */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      TOTP SECRET <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={(config as any).totpSecret || ''}
                        onChange={(e) => setConfig({ ...config, totpSecret: e.target.value.trim() } as any)}
                        placeholder="Enter your TOTP secret key"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '12px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Base32 secret key from Angel One TOTP setup (enables automated login)
                    </p>
                  </div>

                  {/* Angel One Auth Info */}
                  <div style={{ padding: '10px', backgroundColor: `${COLORS.CYAN}10`, border: `1px solid ${COLORS.CYAN}40`, marginBottom: '12px' }}>
                    <p style={{ color: COLORS.CYAN, fontSize: '9px', margin: 0 }}>
                      <strong>Note:</strong> 6-digit TOTP is generated automatically from the TOTP secret. Authentication happens immediately on save.
                    </p>
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* ALICE BLUE: Encryption Key-based Authentication */}
              {/* Fields: User ID, API Key, API Secret */}
              {/* Step 1: Get encKey from API using User ID */}
              {/* Step 2: Generate checksum: SHA256(userid + api_secret + encKey) */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'aliceblue' && (
                <>
                  {/* User ID */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      USER ID <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.userId || ''}
                      onChange={(e) => setConfig({ ...config, userId: e.target.value.toUpperCase() })}
                      placeholder="Enter your AliceBlue User ID"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your AliceBlue trading account User ID
                    </p>
                  </div>

                  {/* API Key */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey || ''}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="Enter your API Key"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your AliceBlue API Key
                    </p>
                  </div>

                  {/* API Secret */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API SECRET <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={config.apiSecret || ''}
                        onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                        placeholder="Enter your API Secret"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '12px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your AliceBlue API Secret
                    </p>
                  </div>

                  {/* AliceBlue Auth Info */}
                  <div style={{ padding: '10px', backgroundColor: `${COLORS.CYAN}10`, border: `1px solid ${COLORS.CYAN}40`, marginBottom: '12px' }}>
                    <p style={{ color: COLORS.CYAN, fontSize: '9px', margin: 0 }}>
                      <strong>Auth Flow:</strong> Encryption key is fetched automatically. Checksum generated as SHA256(userid + api_secret + encKey)
                    </p>
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* 5PAISA: TOTP-based Authentication */}
              {/* Fields: App Name/User Key, App Secret, Client Code/Mobile, PIN, TOTP */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'fivepaisa' && (
                <>
                  {/* App Name / User Key */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      APP NAME / USER KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey || ''}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="Enter your App Name or User Key"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your 5Paisa App Name or User Key
                    </p>
                  </div>

                  {/* App Secret / Encryption Key */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      APP SECRET / ENCRYPTION KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={config.apiSecret || ''}
                        onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                        placeholder="Enter your App Secret"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '10px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                  </div>

                  {/* Client Code / Mobile Number */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      CLIENT CODE / MOBILE NO <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.userId || ''}
                      onChange={(e) => setConfig({ ...config, userId: e.target.value })}
                      placeholder="Enter Client Code or Mobile Number"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your 5Paisa Client Code or registered Mobile Number
                    </p>
                  </div>

                  {/* PIN */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      PIN <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={(config as any).pin || ''}
                        onChange={(e) => setConfig({ ...config, pin: e.target.value } as any)}
                        placeholder="Enter your PIN"
                        style={{
                          width: '100%',
                          padding: '10px 40px 10px 12px',
                          backgroundColor: COLORS.PANEL_BG,
                          border: `1px solid ${COLORS.BORDER}`,
                          color: COLORS.WHITE,
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          outline: 'none',
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '12px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your 5Paisa trading PIN
                    </p>
                  </div>

                  {/* TOTP */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      TOTP CODE <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={(config as any).totpCode || ''}
                      onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                      placeholder="123456"
                      maxLength={6}
                      style={{
                        width: '100%',
                        padding: '10px 12px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: COLORS.WHITE,
                        fontSize: '16px',
                        fontFamily: 'monospace',
                        textAlign: 'center',
                        letterSpacing: '4px',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      6-digit TOTP from your authenticator app
                    </p>
                  </div>
                </>
              )}

              {/* ------------------------------------------------------------ */}
              {/* MOTILAL OSWAL: Multi-factor Authentication (DOB + optional TOTP) */}
              {/* Fields: API Key, User ID, Password, DOB (2FA), TOTP (optional) */}
              {/* Password hash: SHA256(password + api_key) */}
              {/* ------------------------------------------------------------ */}
              {selectedBroker.id === 'motilal' && (
                <>
                  {/* API Key */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      API KEY <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.apiKey || ''}
                      onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                      placeholder="Enter your Motilal Oswal API Key"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Motilal Oswal API Key
                    </p>
                  </div>

                  {/* User ID */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      USER ID (CLIENT ID) <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={config.userId || ''}
                      onChange={(e) => setConfig({ ...config, userId: e.target.value })}
                      placeholder="Enter your User ID"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Your Motilal Oswal Client ID
                    </p>
                  </div>

                  {/* Password */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      PASSWORD <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <div style={{ position: 'relative' }}>
                      <input
                        type={showSecret ? 'text' : 'password'}
                        value={(config as any).password || ''}
                        onChange={(e) => setConfig({ ...config, password: e.target.value } as any)}
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
                          boxSizing: 'border-box',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => setShowSecret(!showSecret)}
                        style={{
                          position: 'absolute',
                          right: '12px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          color: COLORS.GRAY,
                          cursor: 'pointer',
                        }}
                      >
                        {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                      </button>
                    </div>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Password hash is generated as SHA256(password + api_key)
                    </p>
                  </div>

                  {/* 2FA - Date of Birth */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      2FA - DATE OF BIRTH <span style={{ color: COLORS.RED }}>*</span>
                    </label>
                    <input
                      type="text"
                      value={(config as any).dob || ''}
                      onChange={(e) => setConfig({ ...config, dob: e.target.value } as any)}
                      placeholder="DD/MM/YYYY (e.g., 18/10/1988)"
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
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Format: DD/MM/YYYY (required as 2FA factor)
                    </p>
                  </div>

                  {/* TOTP (Optional) */}
                  <div style={{ marginBottom: '12px' }}>
                    <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                      TOTP CODE (OPTIONAL)
                    </label>
                    <input
                      type="text"
                      value={(config as any).totpCode || ''}
                      onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                      placeholder="Leave blank if using OTP via SMS/Email"
                      maxLength={6}
                      style={{
                        width: '100%',
                        padding: '10px 12px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        color: COLORS.WHITE,
                        fontSize: '16px',
                        fontFamily: 'monospace',
                        textAlign: 'center',
                        letterSpacing: '4px',
                        outline: 'none',
                        boxSizing: 'border-box',
                      }}
                    />
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                      Optional: 6-digit TOTP from authenticator app (leave blank for SMS/Email OTP)
                    </p>
                  </div>
                </>
              )}

              <button
                onClick={onSaveConfig}
                disabled={isSaving || !config.apiKey}
                style={{
                  width: '100%',
                  padding: '10px 16px',
                  backgroundColor: isSaving || !config.apiKey ? COLORS.MUTED : COLORS.ORANGE,
                  border: 'none',
                  color: isSaving || !config.apiKey ? COLORS.GRAY : COLORS.DARK_BG,
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: isSaving || !config.apiKey ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                }}
              >
                {isSaving ? <Loader2 size={14} className="animate-spin" /> : <Save size={14} />}
                {['shoonya', 'angelone', 'aliceblue', 'fivepaisa', 'motilal'].includes(selectedBroker.id) ? 'SAVE & AUTHENTICATE' : 'SAVE CONFIGURATION'}
              </button>
            </div>

            {/* Step 2: Authentication */}
            {selectedBroker.authType === 'oauth' && (
              <div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                  <div style={{
                    width: '24px',
                    height: '24px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    backgroundColor: brokerStatuses.get(selectedBrokerId!)?.hasCredentials ? COLORS.ORANGE : COLORS.MUTED,
                    color: COLORS.DARK_BG,
                    fontSize: '12px',
                    fontWeight: 700,
                  }}>
                    2
                  </div>
                  <h3 style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, margin: 0 }}>
                    AUTHENTICATE WITH {selectedBroker.displayName.toUpperCase()}
                  </h3>
                </div>

                {!brokerStatuses.get(selectedBrokerId!)?.hasCredentials ? (
                  <p style={{ color: COLORS.MUTED, fontSize: '10px', marginBottom: '12px' }}>
                    Save your API credentials first to enable authentication.
                  </p>
                ) : (
                  <>
                    <button
                      onClick={onOAuthLogin}
                      disabled={isConnecting}
                      style={{
                        width: '100%',
                        padding: '12px 16px',
                        backgroundColor: isConnecting ? COLORS.MUTED : COLORS.ORANGE,
                        border: 'none',
                        color: isConnecting ? COLORS.GRAY : COLORS.DARK_BG,
                        fontSize: '11px',
                        fontWeight: 700,
                        cursor: isConnecting ? 'not-allowed' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '8px',
                        marginBottom: '12px',
                      }}
                    >
                      {isConnecting ? <Loader2 size={14} className="animate-spin" /> : <ExternalLink size={14} />}
                      LOGIN WITH {selectedBroker.displayName.toUpperCase()}
                    </button>

                    {/* Callback URL Input */}
                    {showCallbackInput && (
                      <div style={{
                        padding: '16px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                      }}>
                        <p style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 600, marginBottom: '8px' }}>
                          PASTE CALLBACK URL
                        </p>
                        <p style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '12px' }}>
                          After authorizing, copy the URL from your browser and paste it below.
                        </p>
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
                            marginBottom: '12px',
                          }}
                        />
                        <button
                          onClick={onCallbackSubmit}
                          disabled={!callbackUrl || isConnecting}
                          style={{
                            width: '100%',
                            padding: '10px 16px',
                            backgroundColor: !callbackUrl || isConnecting ? COLORS.MUTED : COLORS.GREEN,
                            border: 'none',
                            color: !callbackUrl || isConnecting ? COLORS.GRAY : COLORS.DARK_BG,
                            fontSize: '11px',
                            fontWeight: 700,
                            cursor: !callbackUrl || isConnecting ? 'not-allowed' : 'pointer',
                          }}
                        >
                          COMPLETE AUTHENTICATION
                        </button>
                      </div>
                    )}
                  </>
                )}
              </div>
            )}

            {/* Security Note */}
            <div style={{
              marginTop: '24px',
              padding: '12px',
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
            }}>
              <p style={{ color: COLORS.MUTED, fontSize: '9px', margin: 0, lineHeight: 1.6 }}>
                <strong style={{ color: COLORS.GRAY }}>SECURITY:</strong> Your credentials are stored securely on your local device.
                They are never sent to our servers and only communicate directly with {selectedBroker.displayName}'s API.
                Indian broker tokens expire at midnight IST daily.
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
