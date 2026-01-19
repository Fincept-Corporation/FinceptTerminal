/**
 * Broker Configuration Panel - Fincept Style
 *
 * Handles broker API key configuration before authentication
 * Required for OAuth brokers like Zerodha and Fyers
 */

import React, { useState, useEffect } from 'react';
import { Settings, Save, Eye, EyeOff, AlertCircle, CheckCircle2, ExternalLink, HelpCircle, Loader2 } from 'lucide-react';
import { useStockBrokerSelection, useStockBrokerContext } from '@/contexts/StockBrokerContext';
import { createStockBrokerAdapter } from '@/brokers/stocks';
import type { BrokerCredentials } from '@/brokers/stocks/types';

// Fincept color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

interface BrokerConfigPanelProps {
  onConfigSaved?: () => void;
}

export function BrokerConfigPanel({ onConfigSaved }: BrokerConfigPanelProps) {
  const { metadata, broker, setBroker } = useStockBrokerSelection();
  const { adapter } = useStockBrokerContext();

  const [showSecret, setShowSecret] = useState(false);
  const [config, setConfig] = useState<BrokerCredentials>({
    apiKey: '',
    apiSecret: '',
  });
  const [isSaving, setIsSaving] = useState(false);
  const [saveSuccess, setSaveSuccess] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Debug: Log adapter availability
  useEffect(() => {
    console.log('[BrokerConfigPanel] Component mounted/updated', {
      hasMetadata: !!metadata,
      metadataId: metadata?.id,
      hasAdapter: !!adapter,
      adapterType: adapter?.constructor.name,
      hasSetCredentials: !!(adapter && adapter.setCredentials),
    });
  }, [adapter, metadata]);

  // Load existing config on mount
  useEffect(() => {
    if (adapter) {
      setConfig({
        apiKey: '',
        apiSecret: '',
      });
    }
  }, [adapter, metadata?.id]);

  const handleSave = async () => {
    setError(null);
    setSaveSuccess(false);

    // Validation
    if (!config.apiKey?.trim()) {
      setError('API Key is required');
      return;
    }

    if (metadata?.authType === 'oauth' && !config.apiSecret?.trim()) {
      setError('API Secret is required for OAuth brokers');
      return;
    }

    if (!metadata) {
      setError('No broker selected');
      return;
    }

    setIsSaving(true);

    try {
      console.log('[BrokerConfigPanel] === SAVE CONFIGURATION STARTED ===');

      // Get or create adapter
      let adapterToUse = adapter;

      if (!adapterToUse) {
        console.log('[BrokerConfigPanel] Creating temporary adapter for configuration');
        adapterToUse = createStockBrokerAdapter(metadata.id);
      }

      // Set credentials on the adapter
      if (!adapterToUse.setCredentials) {
        throw new Error(`${metadata.displayName} does not support credential configuration.`);
      }

      // Store to database FIRST
      await (adapterToUse as any).storeCredentials({
        apiKey: config.apiKey,
        apiSecret: config.apiSecret || '',
      });

      // THEN set on adapter instance (in-memory)
      adapterToUse.setCredentials(config.apiKey, config.apiSecret || '');

      // If we created a temporary adapter and broker is selected, re-select
      if (!adapter && broker) {
        await setBroker(broker);
      }

      setSaveSuccess(true);
      setTimeout(() => setSaveSuccess(false), 3000);

      if (onConfigSaved) {
        onConfigSaved();
      }
    } catch (err) {
      console.error('[BrokerConfigPanel] Save failed:', err);
      setError(err instanceof Error ? err.message : 'Failed to save configuration');
    } finally {
      setIsSaving(false);
    }
  };

  // Get API credentials URL for the broker
  const getApiKeysUrl = (brokerId: string) => {
    const urls: Record<string, string> = {
      zerodha: 'https://developers.kite.trade/',
      fyers: 'https://myapi.fyers.in/dashboard',
      angelone: 'https://smartapi.angelbroking.com/publisher-login',
      angel: 'https://smartapi.angelbroking.com/',
      dhan: 'https://dhanhq.co/api',
      upstox: 'https://upstox.com/developer/api/',
      kotak: 'https://napi.kotaksecurities.com/devportal/',
      saxobank: 'https://www.developer.saxo/openapi/appmanagement',
      alpaca: 'https://app.alpaca.markets/brokerage/new-account',
      ibkr: 'https://www.interactivebrokers.com/en/trading/ib-api.php',
    };
    return urls[brokerId] || metadata?.website || '#';
  };

  if (!metadata) {
    return (
      <div style={{
        padding: '24px',
        backgroundColor: COLORS.HEADER_BG,
        border: `1px solid ${COLORS.BORDER}`,
        textAlign: 'center'
      }}>
        <p style={{ color: COLORS.GRAY, fontSize: '11px' }}>Select a broker to configure</p>
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
        justifyContent: 'space-between',
        marginBottom: '20px',
        paddingBottom: '16px',
        borderBottom: `1px solid ${COLORS.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{
            width: '36px',
            height: '36px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: `${COLORS.ORANGE}20`,
            border: `1px solid ${COLORS.ORANGE}40`
          }}>
            <Settings size={18} color={COLORS.ORANGE} />
          </div>
          <div>
            <h3 style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 600, margin: 0 }}>
              CONFIGURE {metadata.displayName.toUpperCase()}
            </h3>
            <p style={{ color: COLORS.GRAY, fontSize: '10px', margin: '4px 0 0 0' }}>
              Set up your API credentials
            </p>
          </div>
        </div>

        <a
          href={getApiKeysUrl(metadata.id)}
          target="_blank"
          rel="noopener noreferrer"
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontSize: '10px',
            color: COLORS.ORANGE,
            textDecoration: 'none',
            padding: '6px 10px',
            border: `1px solid ${COLORS.ORANGE}40`,
            backgroundColor: `${COLORS.ORANGE}10`
          }}
        >
          <HelpCircle size={12} />
          GET API KEYS
          <ExternalLink size={10} />
        </a>
      </div>

      {/* Success Message */}
      {saveSuccess && (
        <div style={{
          marginBottom: '16px',
          padding: '12px',
          backgroundColor: `${COLORS.GREEN}15`,
          border: `1px solid ${COLORS.GREEN}40`,
          display: 'flex',
          alignItems: 'center',
          gap: '10px'
        }}>
          <CheckCircle2 size={16} color={COLORS.GREEN} />
          <p style={{ color: COLORS.GREEN, fontSize: '11px', margin: 0 }}>Configuration saved successfully!</p>
        </div>
      )}

      {/* Error Message */}
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

      {/* Info Box for OAuth Brokers */}
      {metadata.authType === 'oauth' && (
        <div style={{
          marginBottom: '16px',
          padding: '14px',
          backgroundColor: COLORS.PANEL_BG,
          border: `1px solid ${COLORS.BORDER}`
        }}>
          <h4 style={{
            color: COLORS.WHITE,
            fontSize: '11px',
            fontWeight: 600,
            margin: '0 0 10px 0',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}>
            <HelpCircle size={14} color={COLORS.ORANGE} />
            HOW TO GET YOUR API CREDENTIALS
          </h4>
          <ol style={{
            color: COLORS.GRAY,
            fontSize: '10px',
            margin: 0,
            paddingLeft: '16px',
            lineHeight: 1.8
          }}>
            <li>Visit <a href={getApiKeysUrl(metadata.id)} target="_blank" rel="noopener noreferrer" style={{ color: COLORS.ORANGE }}>{metadata.displayName} Developer Portal</a></li>
            <li>Create a new app or use an existing one</li>
            <li>Copy your App ID (API Key) and App Secret</li>
            <li>Set the redirect URL to: <code style={{ padding: '2px 6px', backgroundColor: COLORS.DARK_BG, color: COLORS.CYAN, fontSize: '9px' }}>http://127.0.0.1/</code></li>
            <li>Paste your credentials below and save</li>
          </ol>
        </div>
      )}

      {/* Form Fields */}
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
            API KEY / APP ID
            <span style={{ color: COLORS.RED, marginLeft: '4px' }}>*</span>
          </label>
          <input
            type="text"
            value={config.apiKey}
            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
            placeholder={`Enter your ${metadata.displayName} API Key`}
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
          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>
            {metadata.id === 'zerodha' && 'Your Kite Connect API key'}
            {metadata.id === 'fyers' && 'Your Fyers App ID'}
            {metadata.id === 'angelone' && 'Your Angel One Smart API key'}
            {metadata.id === 'dhan' && 'Your Dhan API App ID'}
            {metadata.id === 'upstox' && 'Your Upstox API key'}
            {metadata.id === 'kotak' && 'Your Kotak Neo Consumer Key (neo-fin-key)'}
            {metadata.id === 'saxobank' && 'Your Saxo Bank OpenAPI App Key'}
            {metadata.id === 'alpaca' && 'Your Alpaca API Key ID'}
            {metadata.id === 'ibkr' && 'Your Interactive Brokers Client ID'}
            {!['zerodha', 'fyers', 'angelone', 'dhan', 'upstox', 'kotak', 'saxobank', 'alpaca', 'ibkr'].includes(metadata.id) && 'Your broker API key'}
          </p>
        </div>

        {/* API Secret (for OAuth brokers) */}
        {metadata.authType === 'oauth' && (
          <div>
            <label style={{
              display: 'block',
              color: COLORS.GRAY,
              fontSize: '10px',
              fontWeight: 600,
              marginBottom: '6px',
              letterSpacing: '0.5px'
            }}>
              API SECRET / APP SECRET
              <span style={{ color: COLORS.RED, marginLeft: '4px' }}>*</span>
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
                  boxSizing: 'border-box'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = COLORS.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
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
                  padding: '4px'
                }}
              >
                {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
              </button>
            </div>
            <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>
              {metadata.id === 'zerodha' && 'Your Kite Connect API secret'}
              {metadata.id === 'fyers' && 'Your Fyers App Secret'}
              {metadata.id === 'dhan' && 'Your Dhan API App Secret'}
              {metadata.id === 'upstox' && 'Your Upstox API secret'}
              {metadata.id === 'saxobank' && 'Your Saxo Bank OpenAPI App Secret'}
              {metadata.id === 'alpaca' && 'Your Alpaca API Secret Key'}
              {!['zerodha', 'fyers', 'dhan', 'upstox', 'saxobank', 'alpaca'].includes(metadata.id) && 'Your broker API secret'}
            </p>
          </div>
        )}

        {/* Save Button */}
        <button
          onClick={handleSave}
          disabled={isSaving || !config.apiKey}
          style={{
            width: '100%',
            padding: '12px 16px',
            backgroundColor: isSaving || !config.apiKey ? COLORS.MUTED : COLORS.ORANGE,
            border: 'none',
            color: isSaving || !config.apiKey ? COLORS.GRAY : COLORS.DARK_BG,
            fontSize: '11px',
            fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace',
            letterSpacing: '0.5px',
            cursor: isSaving || !config.apiKey ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
            marginTop: '8px'
          }}
        >
          {isSaving ? (
            <>
              <Loader2 size={14} className="animate-spin" />
              SAVING...
            </>
          ) : (
            <>
              <Save size={14} />
              SAVE CONFIGURATION
            </>
          )}
        </button>
      </div>

      {/* Security Note */}
      <div style={{
        marginTop: '20px',
        padding: '12px',
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`
      }}>
        <p style={{ color: COLORS.MUTED, fontSize: '9px', margin: 0, lineHeight: 1.6 }}>
          <strong style={{ color: COLORS.GRAY }}>🔒 SECURITY:</strong> Your API credentials are stored securely on your local device using encrypted storage.
          They are never sent to our servers and only used to communicate directly with {metadata.displayName}'s API.
        </p>
      </div>

      {/* Next Steps */}
      {metadata.authType === 'oauth' && (
        <div style={{
          marginTop: '12px',
          padding: '12px',
          backgroundColor: `${COLORS.ORANGE}15`,
          border: `1px solid ${COLORS.ORANGE}40`
        }}>
          <p style={{ color: COLORS.ORANGE, fontSize: '10px', margin: 0 }}>
            <strong>NEXT STEP:</strong> After saving your configuration, switch to the <strong>AUTHENTICATION</strong> tab to complete the OAuth login flow.
          </p>
        </div>
      )}
    </div>
  );
}
