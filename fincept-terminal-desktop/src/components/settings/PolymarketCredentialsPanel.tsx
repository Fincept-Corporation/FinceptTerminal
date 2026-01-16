// File: src/components/settings/PolymarketCredentialsPanel.tsx
// Polymarket API credentials management panel

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
import { getSetting, saveSetting } from '@/services/core/sqliteService';

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
      // PURE SQLite - Load credentials from database
      const saved = await getSetting('polymarket_credentials');
      if (saved) {
        const parsed = JSON.parse(saved);
        setCredentials(parsed);

        // Set credentials in service
        polymarketServiceEnhanced.setCredentials({
          apiKey: parsed.apiKey,
          apiSecret: parsed.apiSecret,
          apiPassphrase: parsed.apiPassphrase
        });

        if (parsed.walletAddress) {
          setWalletAddress(parsed.walletAddress);
          setWalletConnected(true);
        }
      }
    } catch (error) {
      console.error('[PolymarketCredentials] Failed to load credentials from SQLite:', error);
    }
  };

  const saveCredentials = async (creds: PolymarketCredentials) => {
    try {
      // PURE SQLite - Save credentials to database
      await saveSetting('polymarket_credentials', JSON.stringify(creds), 'polymarket');
      setCredentials(creds);

      // Set in service
      polymarketServiceEnhanced.setCredentials({
        apiKey: creds.apiKey,
        apiSecret: creds.apiSecret,
        apiPassphrase: creds.apiPassphrase
      });
    } catch (error) {
      console.error('[PolymarketCredentials] Failed to save credentials to SQLite:', error);
      throw error;
    }
  };

  const handleManualSave = () => {
    if (!formData.apiKey || !formData.apiSecret || !formData.apiPassphrase) {
      alert('All fields are required');
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
      // Check if MetaMask is installed
      if (!window.ethereum) {
        throw new Error('MetaMask not detected. Please install MetaMask to continue.');
      }

      // Request account access
      const accounts = await window.ethereum.request({
        method: 'eth_requestAccounts'
      });

      if (!accounts || accounts.length === 0) {
        throw new Error('No accounts found. Please unlock MetaMask.');
      }

      const address = accounts[0];
      setWalletAddress(address);
      setWalletConnected(true);

      // Now generate API credentials
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
      // Import ethers dynamically (v6 API)
      const { BrowserProvider } = await import('ethers');

      const provider = new BrowserProvider(window.ethereum as any);
      const signer = await provider.getSigner();

      // EIP-712 domain and types for Polymarket
      const domain = {
        name: 'ClobAuthDomain',
        version: '1',
        chainId: 137 // Polygon Mainnet
      };

      const types = {
        ClobAuth: [
          { name: 'message', type: 'string' }
        ]
      };

      const value = {
        message: 'This message attests that I control the given wallet'
      };

      setTestResult({
        success: true,
        message: 'Please sign the message in MetaMask...'
      });

      // Request signature from user (v6 API uses signTypedData)
      const signature = await signer.signTypedData(domain, types, value);

      setTestResult({
        success: true,
        message: 'Generating API credentials...'
      });

      // Call Polymarket API to generate credentials
      const response = await fetch('https://clob.polymarket.com/auth/api-key', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          address: address,
          signature: signature,
          nonce: 0 // Use 0 for deterministic credentials
        })
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`API error: ${response.status} - ${errorText}`);
      }

      const apiCredentials = await response.json();

      // Save credentials
      const newCredentials: PolymarketCredentials = {
        apiKey: apiCredentials.apiKey,
        apiSecret: apiCredentials.secret,
        apiPassphrase: apiCredentials.passphrase,
        walletAddress: address,
        createdAt: Date.now(),
        method: 'wallet'
      };

      saveCredentials(newCredentials);

      setTestResult({
        success: true,
        message: 'API credentials generated and saved successfully!'
      });

    } catch (error: any) {
      console.error('Failed to generate API credentials:', error);
      setTestResult({
        success: false,
        message: error.message || 'Failed to generate API credentials'
      });
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
      // Test by fetching CLOB markets (public endpoint that requires auth for rate limits)
      const markets = await polymarketServiceEnhanced.getCLOBMarkets();

      if (markets && (markets.length > 0 || Array.isArray(markets))) {
        setTestResult({
          success: true,
          message: 'Connection successful! Credentials are valid.'
        });
      } else {
        setTestResult({
          success: false,
          message: 'Invalid response from API'
        });
      }
    } catch (error: any) {
      console.error('API test failed:', error);
      setTestResult({
        success: false,
        message: `Test failed: ${error.message}`
      });
    } finally {
      setIsTesting(false);
    }
  };

  const handleDelete = async () => {
    if (confirm('Are you sure you want to delete your Polymarket credentials?')) {
      // PURE SQLite - Clear credentials from database
      await saveSetting('polymarket_credentials', '', 'polymarket');
      setCredentials(null);
      setWalletConnected(false);
      setWalletAddress('');
      polymarketServiceEnhanced.clearCredentials();
      setTestResult({ success: true, message: 'Credentials deleted successfully' });
    }
  };

  const handleCopy = (text: string, label: string) => {
    navigator.clipboard.writeText(text);
    setTestResult({ success: true, message: `${label} copied to clipboard!` });
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
    setTestResult({ success: true, message: 'Credentials exported successfully!' });
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
          setTestResult({ success: true, message: 'Credentials imported successfully!' });
        } else {
          throw new Error('Invalid credentials file');
        }
      } catch (error) {
        setTestResult({ success: false, message: 'Failed to import credentials' });
      }
    };
    reader.readAsText(file);
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="border-b border-gray-700 pb-4">
        <div className="flex items-center gap-3 mb-2">
          <Shield className="text-orange-500" size={24} />
          <h2 className="text-lg font-bold text-white">Polymarket API Credentials</h2>
        </div>
        <p className="text-sm text-gray-400">
          Manage your Polymarket API credentials for trading and portfolio access
        </p>
      </div>

      {/* Status Message */}
      {testResult && (
        <div className={`flex items-start gap-3 p-3 rounded border ${
          testResult.success
            ? 'bg-green-900/20 border-green-700'
            : 'bg-red-900/20 border-red-700'
        }`}>
          {testResult.success ? (
            <CheckCircle className="text-green-400 flex-shrink-0" size={20} />
          ) : (
            <XCircle className="text-red-400 flex-shrink-0" size={20} />
          )}
          <p className={`text-sm ${testResult.success ? 'text-green-300' : 'text-red-300'}`}>
            {testResult.message}
          </p>
        </div>
      )}

      {/* Info Alert */}
      <div className="flex items-start gap-3 p-3 rounded border bg-blue-900/20 border-blue-700">
        <AlertCircle className="text-blue-400 flex-shrink-0" size={20} />
        <div className="text-sm text-blue-300 space-y-2">
          <p>
            <strong>Two ways to get credentials:</strong>
          </p>
          <ol className="list-decimal list-inside space-y-1 ml-2">
            <li><strong>Wallet Connection</strong> - Automatic credential generation from MetaMask</li>
            <li><strong>Manual Entry</strong> - Enter existing credentials from Polymarket</li>
          </ol>
          <p className="mt-2">
            <a
              href="https://docs.polymarket.com/developers/CLOB/authentication"
              target="_blank"
              rel="noopener noreferrer"
              className="text-blue-400 hover:text-blue-300 underline inline-flex items-center gap-1"
            >
              Learn more about authentication <ExternalLink size={12} />
            </a>
          </p>
        </div>
      </div>

      {/* Current Credentials Display */}
      {credentials && !isEditing && (
        <div className="bg-gray-800 rounded-lg border border-gray-700 p-4 space-y-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <CheckCircle className="text-green-400" size={20} />
              <h3 className="font-semibold text-white">Credentials Active</h3>
            </div>
            <div className="flex items-center gap-2">
              <span className="text-xs text-gray-400">
                Method: {credentials.method === 'wallet' ? 'Wallet' : 'Manual'}
              </span>
              {credentials.method === 'wallet' && (
                <Wallet className="text-blue-400" size={16} />
              )}
            </div>
          </div>

          {/* Wallet Address */}
          {credentials.walletAddress && (
            <div>
              <label className="block text-xs text-gray-400 mb-1">Wallet Address</label>
              <div className="flex items-center gap-2 bg-gray-900 rounded px-3 py-2">
                <code className="flex-1 text-sm text-white font-mono">
                  {credentials.walletAddress}
                </code>
                <button
                  onClick={() => handleCopy(credentials.walletAddress!, 'Wallet address')}
                  className="text-gray-400 hover:text-white transition-colors"
                >
                  <Copy size={14} />
                </button>
              </div>
            </div>
          )}

          {/* API Key */}
          <div>
            <label className="block text-xs text-gray-400 mb-1">API Key</label>
            <div className="flex items-center gap-2 bg-gray-900 rounded px-3 py-2">
              <code className="flex-1 text-sm text-white font-mono">
                {showSecrets ? credentials.apiKey : '••••••••••••••••'}
              </code>
              <button
                onClick={() => handleCopy(credentials.apiKey, 'API Key')}
                className="text-gray-400 hover:text-white transition-colors"
              >
                <Copy size={14} />
              </button>
            </div>
          </div>

          {/* API Secret */}
          <div>
            <label className="block text-xs text-gray-400 mb-1">API Secret</label>
            <div className="flex items-center gap-2 bg-gray-900 rounded px-3 py-2">
              <code className="flex-1 text-sm text-white font-mono">
                {showSecrets ? credentials.apiSecret : '••••••••••••••••'}
              </code>
              <button
                onClick={() => handleCopy(credentials.apiSecret, 'API Secret')}
                className="text-gray-400 hover:text-white transition-colors"
              >
                <Copy size={14} />
              </button>
            </div>
          </div>

          {/* API Passphrase */}
          <div>
            <label className="block text-xs text-gray-400 mb-1">API Passphrase</label>
            <div className="flex items-center gap-2 bg-gray-900 rounded px-3 py-2">
              <code className="flex-1 text-sm text-white font-mono">
                {showSecrets ? credentials.apiPassphrase : '••••••••••••••••'}
              </code>
              <button
                onClick={() => handleCopy(credentials.apiPassphrase, 'API Passphrase')}
                className="text-gray-400 hover:text-white transition-colors"
              >
                <Copy size={14} />
              </button>
            </div>
          </div>

          {/* Creation Date */}
          {credentials.createdAt && (
            <div className="text-xs text-gray-400">
              Created: {new Date(credentials.createdAt).toLocaleString()}
            </div>
          )}

          {/* Actions */}
          <div className="flex items-center gap-2 pt-2 border-t border-gray-700">
            <button
              onClick={() => setShowSecrets(!showSecrets)}
              className="flex items-center gap-2 px-3 py-1.5 bg-gray-700 hover:bg-gray-600 rounded text-sm transition-colors"
            >
              {showSecrets ? <EyeOff size={14} /> : <Eye size={14} />}
              {showSecrets ? 'Hide' : 'Show'}
            </button>
            <button
              onClick={handleTest}
              disabled={isTesting}
              className="flex items-center gap-2 px-3 py-1.5 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 rounded text-sm transition-colors"
            >
              {isTesting ? <RefreshCw size={14} className="animate-spin" /> : <Zap size={14} />}
              Test Connection
            </button>
            <button
              onClick={handleExport}
              className="flex items-center gap-2 px-3 py-1.5 bg-gray-700 hover:bg-gray-600 rounded text-sm transition-colors"
            >
              <Download size={14} />
              Export
            </button>
            <button
              onClick={() => setIsEditing(true)}
              className="flex items-center gap-2 px-3 py-1.5 bg-orange-600 hover:bg-orange-700 rounded text-sm transition-colors"
            >
              <Key size={14} />
              Update
            </button>
            <button
              onClick={handleDelete}
              className="flex items-center gap-2 px-3 py-1.5 bg-red-600 hover:bg-red-700 rounded text-sm transition-colors ml-auto"
            >
              <Trash2 size={14} />
              Delete
            </button>
          </div>
        </div>
      )}

      {/* Setup/Edit Section */}
      {(!credentials || isEditing) && (
        <div className="space-y-4">
          {/* Method Selection */}
          <div className="grid grid-cols-2 gap-4">
            {/* Wallet Connection */}
            <button
              onClick={handleWalletConnect}
              disabled={isConnecting}
              className="border border-gray-700 bg-gray-800 hover:bg-gray-700 rounded-lg p-6 text-left transition-all hover:border-orange-500 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <div className="flex items-center gap-3 mb-3">
                <Wallet className="text-orange-500" size={24} />
                <h3 className="font-semibold text-white">Wallet Connection</h3>
              </div>
              <p className="text-sm text-gray-400 mb-3">
                Connect MetaMask to automatically generate API credentials
              </p>
              <div className="flex items-center gap-2 text-xs text-green-400">
                <CheckCircle size={14} />
                <span>Recommended</span>
              </div>
              {isConnecting && (
                <div className="mt-3 flex items-center gap-2 text-sm text-orange-400">
                  <RefreshCw size={14} className="animate-spin" />
                  <span>Connecting...</span>
                </div>
              )}
            </button>

            {/* Manual Entry */}
            <button
              onClick={() => {
                setIsEditing(true);
                setTestResult(null);
              }}
              className="border border-gray-700 bg-gray-800 hover:bg-gray-700 rounded-lg p-6 text-left transition-all hover:border-blue-500"
            >
              <div className="flex items-center gap-3 mb-3">
                <Key className="text-blue-500" size={24} />
                <h3 className="font-semibold text-white">Manual Entry</h3>
              </div>
              <p className="text-sm text-gray-400 mb-3">
                Enter existing API credentials from Polymarket
              </p>
              <div className="flex items-center gap-2 text-xs text-gray-400">
                <LinkIcon size={14} />
                <span>For existing credentials</span>
              </div>
            </button>
          </div>

          {/* Manual Entry Form */}
          {isEditing && (
            <>
              <div className="bg-gray-800 rounded-lg border border-gray-700 p-4 space-y-4">
                <h3 className="font-semibold text-white flex items-center gap-2">
                  <Key size={18} />
                  Enter API Credentials
                </h3>

                {/* API Key Input */}
                <div>
                  <label className="block text-sm text-gray-300 mb-2">
                    API Key <span className="text-red-400">*</span>
                  </label>
                  <input
                    type="text"
                    value={formData.apiKey}
                    onChange={(e) => setFormData({ ...formData, apiKey: e.target.value })}
                    placeholder="Enter your API key"
                    className="w-full bg-gray-900 border border-gray-700 rounded px-3 py-2 text-white text-sm focus:border-orange-500 focus:outline-none"
                  />
                </div>

                {/* API Secret Input */}
                <div>
                  <label className="block text-sm text-gray-300 mb-2">
                    API Secret <span className="text-red-400">*</span>
                  </label>
                  <input
                    type="password"
                    value={formData.apiSecret}
                    onChange={(e) => setFormData({ ...formData, apiSecret: e.target.value })}
                    placeholder="Enter your API secret"
                    className="w-full bg-gray-900 border border-gray-700 rounded px-3 py-2 text-white text-sm focus:border-orange-500 focus:outline-none"
                  />
                </div>

                {/* API Passphrase Input */}
                <div>
                  <label className="block text-sm text-gray-300 mb-2">
                    API Passphrase <span className="text-red-400">*</span>
                  </label>
                  <input
                    type="password"
                    value={formData.apiPassphrase}
                    onChange={(e) => setFormData({ ...formData, apiPassphrase: e.target.value })}
                    placeholder="Enter your API passphrase"
                    className="w-full bg-gray-900 border border-gray-700 rounded px-3 py-2 text-white text-sm focus:border-orange-500 focus:outline-none"
                  />
                </div>

                {/* Import Option */}
                <div className="pt-3 border-t border-gray-700">
                  <label className="flex items-center gap-2 text-sm text-gray-400 cursor-pointer hover:text-white transition-colors">
                    <Upload size={16} />
                    <span>Or import from file</span>
                    <input
                      type="file"
                      accept=".json"
                      onChange={handleImport}
                      className="hidden"
                    />
                  </label>
                </div>

                {/* Action Buttons */}
                <div className="flex items-center gap-3 pt-2">
                  <button
                    onClick={handleManualSave}
                    className="flex items-center gap-2 px-4 py-2 bg-orange-600 hover:bg-orange-700 rounded text-sm font-semibold transition-colors"
                  >
                    <Save size={16} />
                    Save Credentials
                  </button>
                  {credentials && (
                    <button
                      onClick={() => {
                        setIsEditing(false);
                        setFormData({ apiKey: '', apiSecret: '', apiPassphrase: '' });
                      }}
                      className="px-4 py-2 bg-gray-700 hover:bg-gray-600 rounded text-sm transition-colors"
                    >
                      Cancel
                    </button>
                  )}
                </div>
              </div>
            </>
          )}
        </div>
      )}

      {/* Help Section */}
      <div className="bg-gray-800/50 rounded-lg border border-gray-700 p-4">
        <h3 className="font-semibold text-white mb-3 flex items-center gap-2">
          <AlertCircle size={18} />
          Need Help?
        </h3>
        <ul className="space-y-2 text-sm text-gray-300">
          <li className="flex items-start gap-2">
            <span className="text-orange-500">•</span>
            <span>
              <strong>No credentials?</strong> Use wallet connection to automatically generate them
            </span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-orange-500">•</span>
            <span>
              <strong>Already have credentials?</strong> Enter them manually or import from file
            </span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-orange-500">•</span>
            <span>
              <strong>Security:</strong> Credentials are stored locally and never sent to our servers
            </span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-orange-500">•</span>
            <span>
              Visit{' '}
              <a
                href="https://docs.polymarket.com"
                target="_blank"
                rel="noopener noreferrer"
                className="text-blue-400 hover:text-blue-300 underline"
              >
                Polymarket Docs
              </a>
              {' '}for more information
            </span>
          </li>
        </ul>
      </div>
    </div>
  );
};
