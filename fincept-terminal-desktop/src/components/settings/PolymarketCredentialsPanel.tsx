// File: src/components/settings/PolymarketCredentialsPanel.tsx
// Polymarket API credentials management panel - Fincept Terminal Style

import React, { useState, useEffect } from 'react';
import {
  Key,
  Wallet,
  Save,
  Trash2,
  Eye,
  EyeOff,
  CheckCircle,
  XCircle,
  AlertCircle,
  RefreshCw,
  ExternalLink,
  Copy,
  Download,
  Upload,
  Zap,
  Shield,
  Link as LinkIcon
} from 'lucide-react';
import polymarketServiceEnhanced from '@/services/polymarket/polymarketServiceEnhanced';
import { saveCredential, getCredentialByService, deleteCredential } from '@/services/core/sqliteService';
import { showConfirm } from '@/utils/notifications';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#888888',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  INPUT_BG: '#141414',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#666666',
  FONT: '"IBM Plex Mono", "Consolas", monospace',
};

interface PolymarketCredentials {
  apiKey: string;
  apiSecret: string;
  apiPassphrase: string;
  walletAddress?: string;
  createdAt?: number;
  method: 'manual' | 'wallet';
}

export const PolymarketCredentialsPanel: React.FC = () => {
  // State
  const [credentials, setCredentials] = useState<PolymarketCredentials | null>(null);
  const [showSecrets, setShowSecrets] = useState(false);
  const [isEditing, setIsEditing] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [isTesting, setIsTesting] = useState(false);
  const [testResult, setTestResult] = useState<{ success: boolean; message: string } | null>(null);

  // Form state
  const [formData, setFormData] = useState({
    apiKey: '',
    apiSecret: '',
    apiPassphrase: ''
  });

  // Wallet state
  const [walletConnected, setWalletConnected] = useState(false);
  const [walletAddress, setWalletAddress] = useState('');

  // Load saved credentials on mount
  useEffect(() => {
    loadSavedCredentials();
  }, []);

  const loadSavedCredentials = async () => {
    try {
      const cred = await getCredentialByService('polymarket');
      if (cred && cred.api_key) {
        const additional = cred.additional_data ? JSON.parse(cred.additional_data) : {};
        const parsed: PolymarketCredentials = {
          apiKey: cred.api_key,
          apiSecret: cred.api_secret ?? '',
          apiPassphrase: cred.password ?? '',
          walletAddress: cred.username ?? undefined,
          createdAt: additional.createdAt,
          method: additional.method ?? 'manual',
        };
        setCredentials(parsed);
        polymarketServiceEnhanced.setCredentials({
          apiKey: parsed.apiKey,
          apiSecret: parsed.apiSecret,
          apiPassphrase: parsed.apiPassphrase,
          walletAddress: parsed.walletAddress,
        });
        if (parsed.walletAddress) {
          setWalletAddress(parsed.walletAddress);
          setWalletConnected(true);
        }
      }
    } catch (error) {
      console.error('[PolymarketCredentials] Failed to load credentials:', error);
    }
  };

  const saveCredentials = async (creds: PolymarketCredentials) => {
    try {
      await saveCredential({
        service_name: 'polymarket',
        username: creds.walletAddress,
        api_key: creds.apiKey,
        api_secret: creds.apiSecret,
        password: creds.apiPassphrase,
        additional_data: JSON.stringify({ method: creds.method, createdAt: creds.createdAt ?? Date.now() }),
      });
      setCredentials(creds);
      polymarketServiceEnhanced.setCredentials({
        apiKey: creds.apiKey,
        apiSecret: creds.apiSecret,
        apiPassphrase: creds.apiPassphrase,
        walletAddress: creds.walletAddress,
      });
    } catch (error) {
      console.error('[PolymarketCredentials] Failed to save credentials:', error);
      throw error;
    }
  };

  const handleManualSave = () => {
    if (!formData.apiKey || !formData.apiSecret || !formData.apiPassphrase) {
      setTestResult({ success: false, message: 'All fields are required' });
      return;
    }

    const newCredentials: PolymarketCredentials = {
      apiKey: formData.apiKey.trim(),
      apiSecret: formData.apiSecret.trim(),
      apiPassphrase: formData.apiPassphrase.trim(),
      createdAt: Date.now(),
      method: 'manual'
    };

    saveCredentials(newCredentials);
    setIsEditing(false);
    setFormData({ apiKey: '', apiSecret: '', apiPassphrase: '' });
    setTestResult({ success: true, message: 'Credentials saved successfully!' });
  };

  const handleWalletConnect = async () => {
    setIsConnecting(true);
    setTestResult(null);

    try {
      if (!window.ethereum) {
        throw new Error('MetaMask not detected. Please install MetaMask to continue.');
      }

      const accounts = await window.ethereum.request({
        method: 'eth_requestAccounts'
      });

      if (!accounts || accounts.length === 0) {
        throw new Error('No accounts found. Please unlock MetaMask.');
      }

      const address = accounts[0];
      setWalletAddress(address);
      setWalletConnected(true);
      await generateAPICredentials(address);

    } catch (error: any) {
      console.error('Wallet connection failed:', error);
      setTestResult({
        success: false,
        message: error.message || 'Failed to connect wallet'
      });
    } finally {
      setIsConnecting(false);
    }
  };

  const generateAPICredentials = async (address: string) => {
    try {
      const { BrowserProvider } = await import('ethers');
      const provider = new BrowserProvider(window.ethereum as any);
      const signer = await provider.getSigner();

      // L1 authentication: EIP-712 typed data signing as required by Polymarket CLOB API
      const domain = {
        name: 'ClobAuthDomain',
        version: '1',
        chainId: 137
      };

      const types = {
        ClobAuth: [{ name: 'message', type: 'string' }]
      };

      const value = {
        message: 'This message attests that I control the given wallet'
      };

      setTestResult({ success: true, message: 'Please sign the message in MetaMask...' });

      const signature = await signer.signTypedData(domain, types, value);
      const timestamp = Math.floor(Date.now() / 1000).toString();
      const nonce = '0';

      setTestResult({ success: true, message: 'Generating API credentials...' });

      // Polymarket /auth/api-key requires L1 headers, NOT a JSON body
      const response = await fetch('https://clob.polymarket.com/auth/api-key', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'POLY_ADDRESS': address,
          'POLY_SIGNATURE': signature,
          'POLY_TIMESTAMP': timestamp,
          'POLY_NONCE': nonce,
        },
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`API error: ${response.status} - ${errorText}`);
      }

      const apiCredentials = await response.json();

      const newCredentials: PolymarketCredentials = {
        apiKey: apiCredentials.apiKey ?? apiCredentials.key,
        apiSecret: apiCredentials.secret,
        apiPassphrase: apiCredentials.passphrase,
        walletAddress: address,
        createdAt: Date.now(),
        method: 'wallet'
      };

      await saveCredentials(newCredentials);
      setTestResult({ success: true, message: 'API credentials generated and saved!' });

    } catch (error: any) {
      console.error('Failed to generate API credentials:', error);
      setTestResult({ success: false, message: error.message || 'Failed to generate credentials' });
    }
  };

  const handleTest = async () => {
    if (!credentials) {
      setTestResult({ success: false, message: 'No credentials to test' });
      return;
    }

    setIsTesting(true);
    setTestResult(null);

    try {
      // Test against an authenticated endpoint — getOpenOrders requires valid L2 auth headers
      const orders = await polymarketServiceEnhanced.getOpenOrders();
      setTestResult({ success: true, message: `Connection successful! Credentials valid. (${orders.length} open orders)` });
    } catch (error: any) {
      // Distinguish auth failure from network error
      const msg = error.message ?? '';
      if (msg.includes('401') || msg.includes('403') || msg.includes('Unauthorized')) {
        setTestResult({ success: false, message: 'Authentication failed — check your API key, secret, and passphrase.' });
      } else {
        setTestResult({ success: false, message: `Test failed: ${msg}` });
      }
    } finally {
      setIsTesting(false);
    }
  };

  const handleDelete = async () => {
    const confirmed = await showConfirm(
      'This action cannot be undone.',
      {
        title: 'Delete Polymarket credentials?',
        type: 'danger'
      }
    );
    if (!confirmed) return;

    try {
      const cred = await getCredentialByService('polymarket');
      if (cred?.id) {
        await deleteCredential(cred.id);
      }
    } catch (error) {
      console.error('[PolymarketCredentials] Failed to delete credentials:', error);
    }
    setCredentials(null);
    setWalletConnected(false);
    setWalletAddress('');
    polymarketServiceEnhanced.clearCredentials();
    setTestResult({ success: true, message: 'Credentials deleted' });
  };

  const handleCopy = (text: string, label: string) => {
    navigator.clipboard.writeText(text);
    setTestResult({ success: true, message: `${label} copied!` });
    setTimeout(() => setTestResult(null), 2000);
  };

  const handleExport = () => {
    if (!credentials) return;
    const dataStr = JSON.stringify(credentials, null, 2);
    const dataBlob = new Blob([dataStr], { type: 'application/json' });
    const url = URL.createObjectURL(dataBlob);
    const link = document.createElement('a');
    link.href = url;
    link.download = 'polymarket-credentials.json';
    link.click();
    URL.revokeObjectURL(url);
    setTestResult({ success: true, message: 'Credentials exported!' });
  };

  const handleImport = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const imported = JSON.parse(e.target?.result as string);
        if (imported.apiKey && imported.apiSecret && imported.apiPassphrase) {
          saveCredentials(imported);
          setTestResult({ success: true, message: 'Credentials imported!' });
        } else {
          throw new Error('Invalid file');
        }
      } catch {
        setTestResult({ success: false, message: 'Failed to import credentials' });
      }
    };
    reader.readAsText(file);
  };

  // Button style helper
  const btnStyle = (color: string, isDisabled?: boolean) => ({
    padding: '6px 12px',
    backgroundColor: isDisabled ? FINCEPT.MUTED : color,
    border: 'none',
    color: color === FINCEPT.ORANGE ? '#000' : FINCEPT.WHITE,
    cursor: isDisabled ? 'not-allowed' : 'pointer',
    fontSize: '10px',
    fontWeight: 600,
    fontFamily: FINCEPT.FONT,
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
    opacity: isDisabled ? 0.5 : 1,
    letterSpacing: '0.5px',
  });

  return (
    <div style={{
      fontFamily: FINCEPT.FONT,
      color: FINCEPT.WHITE,
      backgroundColor: FINCEPT.DARK_BG,
    }}>
      {/* Header */}
      <div style={{
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '12px 0',
        marginBottom: '16px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
      }}>
        <Shield size={20} color={FINCEPT.ORANGE} />
        <div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '14px', fontWeight: 700, letterSpacing: '0.5px' }}>
            POLYMARKET API CREDENTIALS
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginTop: '2px' }}>
            Manage API credentials for trading and portfolio access
          </div>
        </div>
      </div>

      {/* Status Message */}
      {testResult && (
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '10px 12px',
          marginBottom: '16px',
          backgroundColor: testResult.success ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
          border: `1px solid ${testResult.success ? FINCEPT.GREEN : FINCEPT.RED}`,
        }}>
          {testResult.success ? (
            <CheckCircle size={16} color={FINCEPT.GREEN} />
          ) : (
            <XCircle size={16} color={FINCEPT.RED} />
          )}
          <span style={{ color: testResult.success ? FINCEPT.GREEN : FINCEPT.RED, fontSize: '11px' }}>
            {testResult.message}
          </span>
        </div>
      )}

      {/* Info Box */}
      <div style={{
        display: 'flex',
        alignItems: 'flex-start',
        gap: '10px',
        padding: '12px',
        marginBottom: '16px',
        backgroundColor: `${FINCEPT.BLUE}10`,
        border: `1px solid ${FINCEPT.BLUE}40`,
      }}>
        <AlertCircle size={16} color={FINCEPT.BLUE} style={{ flexShrink: 0, marginTop: '2px' }} />
        <div style={{ fontSize: '10px', color: FINCEPT.CYAN, lineHeight: 1.5 }}>
          <div style={{ fontWeight: 600, marginBottom: '6px' }}>TWO WAYS TO GET CREDENTIALS:</div>
          <div style={{ marginBottom: '4px' }}>1. <span style={{ color: FINCEPT.ORANGE }}>WALLET CONNECTION</span> - Auto-generate from MetaMask</div>
          <div style={{ marginBottom: '8px' }}>2. <span style={{ color: FINCEPT.BLUE }}>MANUAL ENTRY</span> - Enter existing credentials</div>
          <a
            href="https://docs.polymarket.com/developers/CLOB/authentication"
            target="_blank"
            rel="noopener noreferrer"
            style={{ color: FINCEPT.CYAN, textDecoration: 'underline', display: 'inline-flex', alignItems: 'center', gap: '4px' }}
          >
            Documentation <ExternalLink size={10} />
          </a>
        </div>
      </div>

      {/* Current Credentials Display */}
      {credentials && !isEditing && (
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          padding: '16px',
          marginBottom: '16px',
        }}>
          {/* Status Header */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '16px',
            paddingBottom: '12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <CheckCircle size={16} color={FINCEPT.GREEN} />
              <span style={{ color: FINCEPT.GREEN, fontSize: '12px', fontWeight: 600 }}>CREDENTIALS ACTIVE</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{ color: FINCEPT.MUTED, fontSize: '9px' }}>
                METHOD: {credentials.method === 'wallet' ? 'WALLET' : 'MANUAL'}
              </span>
              {credentials.method === 'wallet' && <Wallet size={12} color={FINCEPT.BLUE} />}
            </div>
          </div>

          {/* Credential Fields */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {/* Wallet Address */}
            {credentials.walletAddress && (
              <div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>WALLET ADDRESS</div>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  backgroundColor: FINCEPT.INPUT_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  padding: '8px 10px',
                }}>
                  <code style={{ flex: 1, color: FINCEPT.CYAN, fontSize: '11px' }}>{credentials.walletAddress}</code>
                  <button onClick={() => handleCopy(credentials.walletAddress!, 'Address')} style={{ background: 'none', border: 'none', cursor: 'pointer', padding: 0 }}>
                    <Copy size={12} color={FINCEPT.GRAY} />
                  </button>
                </div>
              </div>
            )}

            {/* API Key */}
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>API KEY</div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                backgroundColor: FINCEPT.INPUT_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                padding: '8px 10px',
              }}>
                <code style={{ flex: 1, color: FINCEPT.WHITE, fontSize: '11px' }}>
                  {showSecrets ? credentials.apiKey : '••••••••••••••••••••'}
                </code>
                <button onClick={() => handleCopy(credentials.apiKey, 'API Key')} style={{ background: 'none', border: 'none', cursor: 'pointer', padding: 0 }}>
                  <Copy size={12} color={FINCEPT.GRAY} />
                </button>
              </div>
            </div>

            {/* API Secret */}
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>API SECRET</div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                backgroundColor: FINCEPT.INPUT_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                padding: '8px 10px',
              }}>
                <code style={{ flex: 1, color: FINCEPT.WHITE, fontSize: '11px' }}>
                  {showSecrets ? credentials.apiSecret : '••••••••••••••••••••'}
                </code>
                <button onClick={() => handleCopy(credentials.apiSecret, 'API Secret')} style={{ background: 'none', border: 'none', cursor: 'pointer', padding: 0 }}>
                  <Copy size={12} color={FINCEPT.GRAY} />
                </button>
              </div>
            </div>

            {/* API Passphrase */}
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>API PASSPHRASE</div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                backgroundColor: FINCEPT.INPUT_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                padding: '8px 10px',
              }}>
                <code style={{ flex: 1, color: FINCEPT.WHITE, fontSize: '11px' }}>
                  {showSecrets ? credentials.apiPassphrase : '••••••••••••••••••••'}
                </code>
                <button onClick={() => handleCopy(credentials.apiPassphrase, 'Passphrase')} style={{ background: 'none', border: 'none', cursor: 'pointer', padding: 0 }}>
                  <Copy size={12} color={FINCEPT.GRAY} />
                </button>
              </div>
            </div>

            {/* Created Date */}
            {credentials.createdAt && (
              <div style={{ color: FINCEPT.MUTED, fontSize: '9px' }}>
                CREATED: {new Date(credentials.createdAt).toLocaleString()}
              </div>
            )}
          </div>

          {/* Actions */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            marginTop: '16px',
            paddingTop: '12px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            flexWrap: 'wrap',
          }}>
            <button onClick={() => setShowSecrets(!showSecrets)} style={btnStyle(FINCEPT.HEADER_BG)}>
              {showSecrets ? <EyeOff size={12} /> : <Eye size={12} />}
              {showSecrets ? 'HIDE' : 'SHOW'}
            </button>
            <button onClick={handleTest} disabled={isTesting} style={btnStyle(FINCEPT.BLUE, isTesting)}>
              {isTesting ? <RefreshCw size={12} className="animate-spin" /> : <Zap size={12} />}
              TEST
            </button>
            <button onClick={handleExport} style={btnStyle(FINCEPT.HEADER_BG)}>
              <Download size={12} />
              EXPORT
            </button>
            <button onClick={() => setIsEditing(true)} style={btnStyle(FINCEPT.ORANGE)}>
              <Key size={12} />
              UPDATE
            </button>
            <button onClick={handleDelete} style={{ ...btnStyle(FINCEPT.RED), marginLeft: 'auto' }}>
              <Trash2 size={12} />
              DELETE
            </button>
          </div>
        </div>
      )}

      {/* Setup/Edit Section */}
      {(!credentials || isEditing) && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {/* Method Selection Cards */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {/* Wallet Connection */}
            <button
              onClick={handleWalletConnect}
              disabled={isConnecting}
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                padding: '20px',
                textAlign: 'left',
                cursor: isConnecting ? 'not-allowed' : 'pointer',
                opacity: isConnecting ? 0.6 : 1,
                transition: 'border-color 0.2s',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '10px' }}>
                <Wallet size={20} color={FINCEPT.ORANGE} />
                <span style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600 }}>WALLET CONNECTION</span>
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '10px', lineHeight: 1.4 }}>
                Connect MetaMask to auto-generate API credentials
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: FINCEPT.GREEN, fontSize: '9px' }}>
                <CheckCircle size={12} />
                RECOMMENDED
              </div>
              {isConnecting && (
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: FINCEPT.ORANGE, fontSize: '10px', marginTop: '10px' }}>
                  <RefreshCw size={12} className="animate-spin" />
                  CONNECTING...
                </div>
              )}
            </button>

            {/* Manual Entry */}
            <button
              onClick={() => { setIsEditing(true); setTestResult(null); }}
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                padding: '20px',
                textAlign: 'left',
                cursor: 'pointer',
                transition: 'border-color 0.2s',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.borderColor = FINCEPT.BLUE; }}
              onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '10px' }}>
                <Key size={20} color={FINCEPT.BLUE} />
                <span style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600 }}>MANUAL ENTRY</span>
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '10px', lineHeight: 1.4 }}>
                Enter existing API credentials from Polymarket
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: FINCEPT.MUTED, fontSize: '9px' }}>
                <LinkIcon size={12} />
                FOR EXISTING CREDENTIALS
              </div>
            </button>
          </div>

          {/* Manual Entry Form */}
          {isEditing && (
            <div style={{
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              padding: '16px',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                <Key size={16} color={FINCEPT.ORANGE} />
                <span style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600 }}>ENTER API CREDENTIALS</span>
              </div>

              <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {/* API Key */}
                <div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>
                    API KEY <span style={{ color: FINCEPT.RED }}>*</span>
                  </div>
                  <input
                    type="text"
                    value={formData.apiKey}
                    onChange={(e) => setFormData({ ...formData, apiKey: e.target.value })}
                    placeholder="Enter your API key"
                    style={{
                      width: '100%',
                      backgroundColor: FINCEPT.INPUT_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      padding: '10px 12px',
                      color: FINCEPT.WHITE,
                      fontSize: '11px',
                      fontFamily: FINCEPT.FONT,
                      outline: 'none',
                    }}
                    onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                    onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                  />
                </div>

                {/* API Secret */}
                <div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>
                    API SECRET <span style={{ color: FINCEPT.RED }}>*</span>
                  </div>
                  <input
                    type="password"
                    value={formData.apiSecret}
                    onChange={(e) => setFormData({ ...formData, apiSecret: e.target.value })}
                    placeholder="Enter your API secret"
                    style={{
                      width: '100%',
                      backgroundColor: FINCEPT.INPUT_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      padding: '10px 12px',
                      color: FINCEPT.WHITE,
                      fontSize: '11px',
                      fontFamily: FINCEPT.FONT,
                      outline: 'none',
                    }}
                    onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                    onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                  />
                </div>

                {/* API Passphrase */}
                <div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', letterSpacing: '0.5px' }}>
                    API PASSPHRASE <span style={{ color: FINCEPT.RED }}>*</span>
                  </div>
                  <input
                    type="password"
                    value={formData.apiPassphrase}
                    onChange={(e) => setFormData({ ...formData, apiPassphrase: e.target.value })}
                    placeholder="Enter your API passphrase"
                    style={{
                      width: '100%',
                      backgroundColor: FINCEPT.INPUT_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      padding: '10px 12px',
                      color: FINCEPT.WHITE,
                      fontSize: '11px',
                      fontFamily: FINCEPT.FONT,
                      outline: 'none',
                    }}
                    onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                    onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                  />
                </div>

                {/* Import Option */}
                <div style={{ paddingTop: '8px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                  <label style={{
                    display: 'inline-flex',
                    alignItems: 'center',
                    gap: '6px',
                    color: FINCEPT.GRAY,
                    fontSize: '10px',
                    cursor: 'pointer',
                  }}>
                    <Upload size={14} />
                    Or import from file
                    <input type="file" accept=".json" onChange={handleImport} style={{ display: 'none' }} />
                  </label>
                </div>

                {/* Actions */}
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '8px' }}>
                  <button onClick={handleManualSave} style={btnStyle(FINCEPT.ORANGE)}>
                    <Save size={12} />
                    SAVE CREDENTIALS
                  </button>
                  {credentials && (
                    <button
                      onClick={() => { setIsEditing(false); setFormData({ apiKey: '', apiSecret: '', apiPassphrase: '' }); }}
                      style={btnStyle(FINCEPT.HEADER_BG)}
                    >
                      CANCEL
                    </button>
                  )}
                </div>
              </div>
            </div>
          )}
        </div>
      )}

      {/* Help Section */}
      <div style={{
        backgroundColor: `${FINCEPT.PANEL_BG}80`,
        border: `1px solid ${FINCEPT.BORDER}`,
        padding: '16px',
        marginTop: '16px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
          <AlertCircle size={14} color={FINCEPT.ORANGE} />
          <span style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 600 }}>NEED HELP?</span>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '10px', color: FINCEPT.GRAY, lineHeight: 1.5 }}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <span style={{ color: FINCEPT.ORANGE }}>&#8226;</span>
            <span><span style={{ color: FINCEPT.WHITE }}>No credentials?</span> Use wallet connection to auto-generate</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <span style={{ color: FINCEPT.ORANGE }}>&#8226;</span>
            <span><span style={{ color: FINCEPT.WHITE }}>Already have credentials?</span> Enter manually or import from file</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <span style={{ color: FINCEPT.ORANGE }}>&#8226;</span>
            <span><span style={{ color: FINCEPT.WHITE }}>Security:</span> Credentials stored locally, never sent to our servers</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <span style={{ color: FINCEPT.ORANGE }}>&#8226;</span>
            <span>
              Visit{' '}
              <a href="https://docs.polymarket.com" target="_blank" rel="noopener noreferrer" style={{ color: FINCEPT.CYAN, textDecoration: 'underline' }}>
                Polymarket Docs
              </a>
              {' '}for more info
            </span>
          </div>
        </div>
      </div>

      {/* CSS for spin animation */}
      <style>{`
        @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
        .animate-spin { animation: spin 1s linear infinite; }
      `}</style>
    </div>
  );
};
